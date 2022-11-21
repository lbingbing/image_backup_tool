#pragma once

#include <condition_variable>
#include <functional>
#include <opencv2/opencv.hpp>

#include "image_codec_api.h"
#include "thread_safe_queue.h"
#include "decode_image.h"
#include "decode_image_task.h"
#include "decode_image_task_status_server.h"

class PixelImageCodecWorker {
public:
    struct CalibrationProgress {
        uint64_t frame_num = 0;
        float fps = 0;
    };

    struct SavePartProgress {
        uint64_t frame_num = 0;
        uint32_t done_part_num = 0;
        uint32_t part_num = 0;
        float fps = 0;
        float done_fps = 0;
        float bps = 0;
        int left_days = 0;
        int left_hours = 0;
        int left_minutes = 0;
        int left_seconds = 0;
    };

    using GetTransformCb = std::function<Transform()>;
    using CalibrateCb = std::function<void(const Calibration&)>;
    using SendCalibrationImageResultCb = std::function<void(const cv::Mat&, bool, const std::vector<std::vector<cv::Mat>>&)>;
    using CalibrationProgressCb = std::function<void(const CalibrationProgress&)>;
    using SendDecodeImageResultCb = std::function<void(const cv::Mat&, bool, const std::vector<std::vector<cv::Mat>>&)>;
    using SendAutoTransformCb = std::function<void(const Transform&)>;
    using SavePartProgressCb = std::function<void(const SavePartProgress&)>;
    using SavePartFinishCb = std::function<void()>;
    using SavePartCompleteCb = std::function<void()>;
    using SavePartErrorCb = std::function<void(const std::string&)>;

    IMAGE_CODEC_API PixelImageCodecWorker(PixelType pixel_type);
    IMAGE_CODEC_API void FetchImageWorker(std::atomic<bool>& running, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, int interval);
    IMAGE_CODEC_API void CalibrateWorker(ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, CalibrateCb calibrate_cb, SendCalibrationImageResultCb send_calibration_image_result_cb, CalibrationProgressCb calibration_progress_cb);
    IMAGE_CODEC_API void DecodeImageWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration);
    IMAGE_CODEC_API void DecodeResultWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration, SendDecodeImageResultCb send_decode_image_result_cb, int interval);
    IMAGE_CODEC_API void AutoTransformWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration, SendAutoTransformCb send_auto_trasform_cb, int interval);
    IMAGE_CODEC_API void SavePartWorker(std::atomic<bool>& running, ThreadSafeQueue<DecodeResult>& part_q, std::string output_file, const Dim& dim, int pixel_size, int space_size, uint32_t part_num, SavePartProgressCb save_part_progress_cb, SavePartFinishCb save_part_finish_cb, SavePartCompleteCb save_part_complete_cb, SavePartErrorCb error_cb, Task::FinalizationStartCb finalization_start_cb, Task::FinalizationProgressCb finalization_progress_cb, Task::FinalizationCompleteCb finalization_complete_cb, TaskStatusServer* task_status_server);

private:
    PixelImageCodec m_pixel_image_codec;
};
