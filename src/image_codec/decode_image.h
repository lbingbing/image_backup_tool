#pragma once

#include "image_codec_api.h"
#include "image_codec_types.h"
#include "transform_utils.h"
#include "pixel_codec.h"

struct TileCalibration {
    std::vector<std::vector<std::pair<float, float>>> centers;
};

struct Calibration {
    bool valid = false;
    Dim dim;
    std::vector<std::vector<TileCalibration>> tiles;

    IMAGE_CODEC_API void Load(const std::string& path);
    IMAGE_CODEC_API void Save(const std::string& path);
};

using CalibrateResult = std::tuple<cv::Mat, Calibration, std::vector<std::vector<cv::Mat>>>;
using ImageDecodeResult = std::tuple<bool, uint32_t, Bytes, Pixels, cv::Mat, std::vector<std::vector<cv::Mat>>>;

class PixelImageCodec {
public:
    IMAGE_CODEC_API PixelImageCodec(PixelType pixel_type);
    IMAGE_CODEC_API PixelCodec& GetPixelCodec() { return *m_pixel_codec; }
    IMAGE_CODEC_API CalibrateResult Calibrate(const cv::Mat& img, const Dim& dim, const Transform& transform, bool result_image = false);
    IMAGE_CODEC_API ImageDecodeResult Decode(const cv::Mat& img, const Dim& dim, const Transform& transform, const Calibration& calibration, bool result_image = false);

private:
    std::unique_ptr<PixelCodec> m_pixel_codec;
};
