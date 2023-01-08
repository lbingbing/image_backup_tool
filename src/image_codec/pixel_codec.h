#pragma once

#include <iostream>
#include <string>
#include <exception>

#include "image_codec_api.h"
#include "image_codec_types.h"

using DecodeResult = std::tuple<bool, uint32_t, Bytes>;

class PixelCodec {
public:
    static constexpr int CRC_BYTE_NUM = 4;
    static constexpr int PART_ID_BYTE_NUM = 4;
    static constexpr int META_BYTE_NUM = CRC_BYTE_NUM + PART_ID_BYTE_NUM;

    virtual ~PixelCodec() {}
    virtual PixelType GetPixelType() = 0;
    virtual int PixelValueNum() = 0;
    virtual int BitNumPerPixel() = 0;
    IMAGE_CODEC_API Pixels Encode(uint32_t part_id, const Bytes& part_bytes, int frame_size);
    IMAGE_CODEC_API DecodeResult Decode(const Pixels& pixels);

protected:
    virtual Pixels BytesToPixels(const Bytes& bytes) = 0;
    virtual Bytes PixelsToBytes(const Pixels& pixels) = 0;
};

class Pixel2Codec : public PixelCodec {
protected:
    PixelType GetPixelType() override { return PixelType::PIXEL2; }
    int PixelValueNum() override { return 2; }
    int BitNumPerPixel() override { return 1; }
    IMAGE_CODEC_API Pixels BytesToPixels(const Bytes& bytes) override;
    IMAGE_CODEC_API Bytes PixelsToBytes(const Pixels& pixels) override;
};

class Pixel4Codec : public PixelCodec {
protected:
    PixelType GetPixelType() override { return PixelType::PIXEL4; }
    int PixelValueNum() override { return 4; }
    int BitNumPerPixel() override { return 2; }
    IMAGE_CODEC_API Pixels BytesToPixels(const Bytes& bytes) override;
    IMAGE_CODEC_API Bytes PixelsToBytes(const Pixels& pixels) override;
};

class Pixel8Codec : public PixelCodec {
protected:
    PixelType GetPixelType() override { return PixelType::PIXEL8; }
    int PixelValueNum() override { return 8; }
    int BitNumPerPixel() override { return 3; }
    IMAGE_CODEC_API Pixels BytesToPixels(const Bytes& bytes) override;
    IMAGE_CODEC_API Bytes PixelsToBytes(const Pixels& pixels) override;
};

IMAGE_CODEC_API std::unique_ptr<PixelCodec> create_pixel_codec(PixelType pixel_type);
