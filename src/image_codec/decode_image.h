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
    virtual ~PixelImageCodec() {}
    virtual PixelCodec& GetPixelCodec() = 0;
    IMAGE_CODEC_API CalibrateResult Calibrate(const cv::Mat& img, const Dim& dim, const Transform& transform, bool result_image = false);
    IMAGE_CODEC_API ImageDecodeResult Decode(const cv::Mat& img, const Dim& dim, const Transform& transform, const Calibration& calibration, bool result_image = false);
};

class Pixel2ImageCodec : public PixelImageCodec {
public:
    PixelCodec& GetPixelCodec() override { return m_pixel2_codec; }

private:
    Pixel2Codec m_pixel2_codec;
};

class Pixel4ImageCodec : public PixelImageCodec {
public:
    PixelCodec& GetPixelCodec() override { return m_pixel4_codec; }

private:
    Pixel4Codec m_pixel4_codec;
};

class Pixel8ImageCodec : public PixelImageCodec {
public:
    PixelCodec& GetPixelCodec() override { return m_pixel8_codec; }

private:
    Pixel8Codec m_pixel8_codec;
};

IMAGE_CODEC_API std::unique_ptr<PixelImageCodec> create_pixel_image_codec(PixelType pixel_type);
