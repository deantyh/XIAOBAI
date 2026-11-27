#include "sdkconfig.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <img_converters.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "board.h"
#include "display.h"
#include "esp32_camera.h"
#include "esp_timer.h"
#include "jpg/image_to_jpeg.h"
#include "lvgl_display.h"
#include "mcp_server.h"
#include "system_info.h"

#define TAG "Esp32Camera"

// V0.0.5: Adaptive white balance for the OV5640 on this kit.
// V0.0.4 used fixed U/V offsets (-36/+41) derived from a phone photo of the
// LCD. That photo passed through the panel and the phone's own ISP, so the
// inferred offsets were far too small: real white-sheet previews were still
// heavily cyan (R≈15, G≈130, B≈254), implying raw U≈200 / V≈69 versus the
// neutral U=V=128. Fixed guesses keep missing the right value.
//
// Instead, apply a gray-world auto white balance on every frame: measure the
// average U and V, then shift them toward 128. This adapts to whatever the
// sensor actually outputs (and to changing light) without any hand-tuned
// constant. The offset is clamped so a strongly colored scene cannot be
// over-corrected, and the raw/derived values are logged once per capture so
// the sensor behaviour can be diagnosed from serial output.
static constexpr int kAwbNeutral = 128;
static constexpr int kAwbMaxOffset = 80;  // limit per-channel correction

static inline uint8_t ClampByte(int v) {
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static inline int ClampOffset(int v) {
    return v < -kAwbMaxOffset ? -kAwbMaxOffset : (v > kAwbMaxOffset ? kAwbMaxOffset : v);
}

static void CorrectYuyvWhiteBalance(const uint8_t* src, uint8_t* dst, size_t pixel_count) {
    // One YUYV group is 4 bytes [Y0, U, Y1, V] covering two pixels.
    const size_t groups = pixel_count / 2;

    // First pass: sample U/V across the frame to estimate the average.
    // Sampling (rather than every pixel) keeps the cost low on ESP32.
    uint64_t sum_u = 0;
    uint64_t sum_v = 0;
    size_t samples = 0;
    const size_t step = (groups / 512) ? (groups / 512) : 1;
    for (size_t g = 0; g < groups; g += step) {
        const uint8_t* s = src + g * 4;
        sum_u += s[1];  // U (Cb)
        sum_v += s[3];  // V (Cr)
        ++samples;
    }
    if (samples == 0) {
        memcpy(dst, src, pixel_count * 2);
        return;
    }

    const int avg_u = (int)(sum_u / samples);
    const int avg_v = (int)(sum_v / samples);
    const int offset_u = ClampOffset(kAwbNeutral - avg_u);
    const int offset_v = ClampOffset(kAwbNeutral - avg_v);
    ESP_LOGI(TAG, "AWB: raw U=%d V=%d -> offset U=%+d V=%+d", avg_u, avg_v, offset_u, offset_v);

    // Second pass: apply the correction to the whole frame.
    for (size_t g = 0; g < groups; ++g) {
        const uint8_t* s = src + g * 4;
        uint8_t* d = dst + g * 4;
        d[0] = s[0];                              // Y0
        d[1] = ClampByte((int)s[1] + offset_u);   // U (Cb)
        d[2] = s[2];                              // Y1
        d[3] = ClampByte((int)s[3] + offset_v);   // V (Cr)
    }
}

// Convert one YUYV (YUV422) sample pair to an RGB565 pixel using BT.601.
// The OV5640 on the breadboard S3CAM kit outputs YUYV; converting explicitly
// avoids all RGB565 byte-order / R-B-channel guesswork.
static inline uint16_t YuyvSampleToRgb565(uint8_t y, uint8_t u, uint8_t v) {
    int c = (int)y - 16;
    int d = (int)u - 128;
    int e = (int)v - 128;
    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void YuyvToRgb565(const uint8_t* src, uint16_t* dst, size_t pixel_count) {
    size_t i = 0;
    for (; i + 1 < pixel_count; i += 2) {
        const uint8_t* p = src + (i / 2) * 4;
        dst[i] = YuyvSampleToRgb565(p[0], p[1], p[3]);  // Y0, U, V
        dst[i + 1] = YuyvSampleToRgb565(p[2], p[1], p[3]);  // Y1, U, V
    }
    if (i < pixel_count) {
        const uint8_t* p = src + (i / 2) * 4;
        dst[i] = YuyvSampleToRgb565(p[0], p[1], p[3]);
    }
}

#if CONFIG_XIAOZHI_CAMERA_MIRROR_CONFIGURED
#if CONFIG_XIAOZHI_CAMERA_HMIRROR
static constexpr bool kConfiguredHMirror = true;
#else
static constexpr bool kConfiguredHMirror = false;
#endif
#if CONFIG_XIAOZHI_CAMERA_VFLIP
static constexpr bool kConfiguredVFlip = true;
#else
static constexpr bool kConfiguredVFlip = false;
#endif
#endif

Esp32Camera::Esp32Camera(const camera_config_t& config) {
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed with error 0x%x", err);
        return;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        if (s->id.PID == GC0308_PID) {
            s->set_hmirror(s, 0);  // Control camera mirror: 1 for mirror, 0 for normal
        }
#if CONFIG_XIAOZHI_CAMERA_MIRROR_CONFIGURED
        s->set_hmirror(s, kConfiguredHMirror ? 1 : 0);
        s->set_vflip(s, kConfiguredVFlip ? 1 : 0);
#endif
        ESP_LOGI(TAG, "Camera initialized: format=%d", config.pixel_format);
    }

    streaming_on_ = true;
}

Esp32Camera::~Esp32Camera() {
    if (streaming_on_) {
        if (current_fb_) {
            esp_camera_fb_return(current_fb_);
            current_fb_ = nullptr;
        }
        if (encode_buf_) {
            heap_caps_free(encode_buf_);
            encode_buf_ = nullptr;
            encode_buf_size_ = 0;
        }
        if (yuv_corrected_buf_) {
            heap_caps_free(yuv_corrected_buf_);
            yuv_corrected_buf_ = nullptr;
            yuv_corrected_buf_size_ = 0;
        }
        esp_camera_deinit();
        streaming_on_ = false;
    }
}

void Esp32Camera::SetExplainUrl(const std::string& url, const std::string& token) {
    explain_url_ = url;
    explain_token_ = token;
}

bool Esp32Camera::Capture() {
    if (encoder_thread_.joinable()) {
        encoder_thread_.join();
    }

    if (!streaming_on_) {
        return false;
    }

    // Get the latest frame, discard old frames for real-time performance
    for (int i = 0; i < 2; i++) {
        if (current_fb_) {
            esp_camera_fb_return(current_fb_);
        }
        current_fb_ = esp_camera_fb_get();
        if (!current_fb_) {
            ESP_LOGE(TAG, "Camera capture failed");
            return false;
        }
    }

    // Build an RGB565 buffer for the LVGL preview. RGB565 frames are reused
    // with optional byte/R-B corrections; YUYV frames are converted here.
    // JPEG upload keeps the native frame format (YUYV is encoded directly),
    // so no color information is lost on the AI path.
    if (current_fb_->format == PIXFORMAT_RGB565 || current_fb_->format == PIXFORMAT_YUV422) {
        size_t pixel_count = current_fb_->width * current_fb_->height;
        size_t data_size = pixel_count * 2;

        // Allocate or reallocate encode buffer if needed
        if (encode_buf_size_ < data_size) {
            if (encode_buf_) {
                heap_caps_free(encode_buf_);
            }
            encode_buf_ =
                (uint8_t*)heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (encode_buf_ == nullptr) {
                ESP_LOGE(TAG, "Failed to allocate memory for encode buffer");
                encode_buf_size_ = 0;
                return false;
            }
            encode_buf_size_ = data_size;
        }

        if (current_fb_->format == PIXFORMAT_RGB565) {
            // Legacy RGB565 boards may still need byte-order/R-B fixes.
            uint16_t* src = (uint16_t*)current_fb_->buf;
            uint16_t* dst = (uint16_t*)encode_buf_;
            if (swap_rb_channels_enabled_) {
                for (size_t i = 0; i < pixel_count; i++) {
                    uint16_t v = swap_bytes_enabled_ ? __builtin_bswap16(src[i]) : src[i];
                    dst[i] = (uint16_t)(((v & 0x001F) << 11) | (v & 0x07E0) | ((v >> 11) & 0x001F));
                }
            } else if (swap_bytes_enabled_) {
                for (size_t i = 0; i < pixel_count; i++) {
                    dst[i] = __builtin_bswap16(src[i]);
                }
            } else {
                memcpy(encode_buf_, current_fb_->buf, data_size);
            }
        } else {
            // OV5640 on the breadboard S3CAM kit: apply gray-world white
            // balance in the YUV domain, then convert YUYV -> RGB565 for the
            // preview. The corrected YUYV buffer is reused by Explain() for
            // JPEG so the AI sees the same colors as the screen.
            if (yuv_corrected_buf_size_ < data_size) {
                if (yuv_corrected_buf_) {
                    heap_caps_free(yuv_corrected_buf_);
                }
                yuv_corrected_buf_ =
                    (uint8_t*)heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (yuv_corrected_buf_ == nullptr) {
                    ESP_LOGE(TAG, "Failed to allocate corrected YUYV buffer");
                    yuv_corrected_buf_size_ = 0;
                    return false;
                }
                yuv_corrected_buf_size_ = data_size;
            }
            CorrectYuyvWhiteBalance(current_fb_->buf, yuv_corrected_buf_, pixel_count);
            YuyvToRgb565(yuv_corrected_buf_, (uint16_t*)encode_buf_, pixel_count);
        }

        // Allocate separate buffer for preview display
        uint8_t* preview_data =
            (uint8_t*)heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (preview_data != nullptr) {
            memcpy(preview_data, encode_buf_, data_size);
            auto display = Board::GetInstance().GetDisplay();
            if (display != nullptr) {
                display->SetPreviewImage(std::make_unique<LvglAllocatedImage>(
                    preview_data, data_size, current_fb_->width, current_fb_->height,
                    current_fb_->width * 2, LV_COLOR_FORMAT_RGB565));
            } else {
                heap_caps_free(preview_data);
            }
        }
    } else if (current_fb_->format == PIXFORMAT_JPEG) {
        // JPEG format preview usually requires decoding, skip preview display for now, just log
        ESP_LOGW(TAG, "JPEG capture success, len=%zu, but not supported for preview",
                 current_fb_->len);
    }

    ESP_LOGI(TAG, "Captured frame: %dx%d, len=%zu, format=%d", current_fb_->width,
             current_fb_->height, current_fb_->len, current_fb_->format);

    return true;
}

bool Esp32Camera::SetHMirror(bool enabled) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) {
        return false;
    }
    s->set_hmirror(s, enabled ? 1 : 0);
    return true;
}

bool Esp32Camera::SetVFlip(bool enabled) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) {
        return false;
    }
    s->set_vflip(s, enabled ? 1 : 0);
    return true;
}

bool Esp32Camera::SetSwapBytes(bool enabled) {
    swap_bytes_enabled_ = enabled;
    return true;
}

bool Esp32Camera::SetSwapRBChannels(bool enabled) {
    swap_rb_channels_enabled_ = enabled;
    return true;
}

std::expected<std::string, std::string> Esp32Camera::Explain(const std::string& question) {
    return Explain(question, explain_url_, explain_token_);
}

std::expected<std::string, std::string> Esp32Camera::Explain(const std::string& question,
                                                             const std::string& url,
                                                             const std::string& token) {
    if (url.empty()) {
        return std::unexpected("Image explain URL or token is not set");
    }

    if (current_fb_ == nullptr) {
        return std::unexpected("No camera frame captured");
    }

    // Encode synchronously and take ownership of the whole JPEG buffer. The
    // multipart body is streamed with an explicit Content-Length instead of
    // being assembled into one std::string: the server (ALB) rejects chunked
    // uploads, and a std::string that fails to grow aborts the firmware because
    // C++ exceptions are disabled.
    const uint16_t w = current_fb_->width;
    const uint16_t h = current_fb_->height;
    v4l2_pix_fmt_t enc_fmt;
    switch (current_fb_->format) {
        case PIXFORMAT_RGB565:
            enc_fmt = V4L2_PIX_FMT_RGB565;
            break;
        case PIXFORMAT_YUV422:
            enc_fmt = V4L2_PIX_FMT_YUYV;  // YUV422 is actually YUYV format
            break;
        case PIXFORMAT_YUV420:
            enc_fmt = V4L2_PIX_FMT_YUV420;
            break;
        case PIXFORMAT_GRAYSCALE:
            enc_fmt = V4L2_PIX_FMT_GREY;
            break;
        case PIXFORMAT_JPEG:
            enc_fmt = V4L2_PIX_FMT_JPEG;
            break;
        case PIXFORMAT_RGB888:
            enc_fmt = V4L2_PIX_FMT_RGB24;
            break;
        default:
            ESP_LOGE(TAG, "Unsupported pixel format: %d", current_fb_->format);
            return std::unexpected("Unsupported pixel format");
    }

    // Use the corrected buffer for RGB565/YUYV, otherwise the original frame.
    // YUYV frames are gray-world white-balanced in Capture(); upload the
    // corrected data so the AI sees the same colors as the on-screen preview.
    uint8_t* jpeg_src_buf = current_fb_->buf;
    size_t jpeg_src_len = current_fb_->len;
    if (current_fb_->format == PIXFORMAT_RGB565 && encode_buf_ != nullptr) {
        jpeg_src_buf = encode_buf_;
        jpeg_src_len = encode_buf_size_;
    } else if (current_fb_->format == PIXFORMAT_YUV422 && yuv_corrected_buf_ != nullptr) {
        jpeg_src_buf = yuv_corrected_buf_;
        jpeg_src_len = current_fb_->width * current_fb_->height * 2;
    }

    int64_t start_time = esp_timer_get_time();
    uint8_t* jpeg_data = nullptr;
    size_t jpeg_size = 0;
    bool ok = image_to_jpeg(jpeg_src_buf, jpeg_src_len, w, h, enc_fmt, 80, &jpeg_data, &jpeg_size);
    ESP_LOGI(TAG, "JPEG encoding time: %ld ms", int((esp_timer_get_time() - start_time) / 1000));
    if (!ok || jpeg_data == nullptr || jpeg_size == 0) {
        free(jpeg_data);
        ESP_LOGE(TAG, "JPEG encoder failed or produced empty output");
        return std::unexpected("Failed to encode image to JPEG");
    }

    const std::string boundary = "----ESP32_CAMERA_BOUNDARY";
    std::string head;
    head += "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"question\"\r\n\r\n";
    head += question + "\r\n";
    head += "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"file\"; filename=\"camera.jpg\"\r\n";
    head += "Content-Type: image/jpeg\r\n\r\n";
    const std::string tail = "\r\n--" + boundary + "--\r\n";

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(3);
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
    if (!token.empty()) {
        http->SetHeader("Authorization", "Bearer " + token);
    }
    http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    // An explicit Content-Length makes Http::Write() send raw bytes instead of
    // chunk framing, so only the JPEG is buffered.
    http->SetHeader("Content-Length",
                    std::to_string(head.size() + jpeg_size + tail.size()));
    if (auto opened = http->Open("POST", url); !opened) {
        free(jpeg_data);
        ESP_LOGE(TAG, "Failed to upload photo: %s", opened.error().ToString().c_str());
        http->Close();
        return std::unexpected("Failed to upload photo");
    }

    auto write_all = [&http](const void* data, size_t size) -> bool {
        const char* ptr = static_cast<const char*>(data);
        while (size > 0) {
            auto written = http->Write(ptr, size);
            if (!written || *written <= 0 || static_cast<size_t>(*written) > size) {
                return false;
            }
            ptr += *written;
            size -= static_cast<size_t>(*written);
        }
        return true;
    };

    bool uploaded = write_all(head.data(), head.size()) &&
                    write_all(jpeg_data, jpeg_size) &&
                    write_all(tail.data(), tail.size());
    free(jpeg_data);
    if (!uploaded) {
        ESP_LOGE(TAG, "Failed to upload photo body");
        http->Close();
        return std::unexpected("Failed to upload photo");
    }
    ESP_LOGI(TAG, "Explain upload done: jpeg=%u bytes, content-length=%u",
             (unsigned)jpeg_size, (unsigned)(head.size() + jpeg_size + tail.size()));

    auto status_code = http->GetStatusCode();
    if (!status_code) {
        ESP_LOGE(TAG, "Failed to read HTTP status: %s", status_code.error().ToString().c_str());
        http->Close();
        return std::unexpected("Failed to upload photo");
    }
    if (*status_code != 200) {
        ESP_LOGE(TAG, "Failed to upload photo, status code: %d", *status_code);
        http->Close();
        return std::unexpected("Failed to upload photo");
    }

    std::string result = http->ReadAll();
    http->Close();

    size_t remain_stack_size = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG,
             "Explain image size=%dx%d, compressed size=%d, remain stack size=%d, question=%s\n%s",
             current_fb_->width, current_fb_->height, (int)jpeg_size, (int)remain_stack_size,
             question.c_str(), result.c_str());
    return result;
}
