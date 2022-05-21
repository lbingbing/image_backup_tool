#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <opencv2/opencv.hpp>

#include "image_codec_api.h"
#include "thread_safe_queue.h"
#include "decode_image.h"
#include "decode_image_task.h"

using GetTransformCb = std::function<Transform()>;
using CalibrateCb = std::function<void(const Calibration&)>;
using SendResultCb = std::function<void(const cv::Mat&, bool, const std::vector<std::vector<cv::Mat>>&)>;
using CalibrateProgressCb = std::function<void(uint64_t, float)>;
using SavePartProgressCb = std::function<void(uint32_t, uint32_t, uint64_t, float, float, float, int64_t)>;
using SavePartFinishCb = std::function<void()>;
using SavePartCompleteCb = std::function<void()>;
using SavePartErrorCb = std::function<void(std::string)>;

class PixelImageCodecWorker {
public:
	virtual PixelImageCodec& GetPixelImageCodec() = 0;
	IMAGE_CODEC_API void FetchImageWorker(std::atomic<bool>& running, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q);
	IMAGE_CODEC_API void CalibrateWorker(ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, CalibrateCb calibrate_cb, SendResultCb send_result_cb, CalibrateProgressCb calibrate_progress_cb);
	IMAGE_CODEC_API void DecodeResultWorker(ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration, SendResultCb send_result_cb, int delay);
	IMAGE_CODEC_API void DecodeImageWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, std::string image_dir_path, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration);
	IMAGE_CODEC_API void SavePartWorker(std::atomic<bool>& running, ThreadSafeQueue<DecodeResult>& part_q, std::string output_file, const Dim& dim, int pixel_size, int space_size, uint32_t part_num, SavePartProgressCb progress_cb, SavePartFinishCb finish_cb, SavePartCompleteCb complete_cb, SavePartErrorCb error_cb);
};

class Pixel2ImageCodecWorker : public PixelImageCodecWorker {
public:
	Pixel2ImageCodec& GetPixelImageCodec() override { return m_pixel2_image_codec; }

private:
	Pixel2ImageCodec m_pixel2_image_codec;
};

class Pixel4ImageCodecWorker : public PixelImageCodecWorker {
public:
	Pixel4ImageCodec& GetPixelImageCodec() override { return m_pixel4_image_codec; }

private:
	Pixel4ImageCodec m_pixel4_image_codec;
};

class Pixel8ImageCodecWorker : public PixelImageCodecWorker {
public:
	Pixel8ImageCodec& GetPixelImageCodec() override { return m_pixel8_image_codec; }

private:
	Pixel8ImageCodec m_pixel8_image_codec;
};

IMAGE_CODEC_API std::unique_ptr<PixelImageCodecWorker> create_pixel_image_codec_worker(PixelType pixel_type);
