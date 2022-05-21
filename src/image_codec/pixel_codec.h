#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <exception>

#include "image_codec_api.h"

using Byte = uint8_t;
using Bytes = std::vector<Byte>;
using Pixel = uint8_t;
using Pixels = std::vector<Pixel>;
using DecodeResult = std::tuple<bool, uint32_t, Bytes>;

IMAGE_CODEC_API std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& v);

enum class PixelType {
    PIXEL2 = 0,
    PIXEL4 = 1,
    PIXEL8 = 2,
};

constexpr uint8_t PIXEL_WHITE   = 0;
constexpr uint8_t PIXEL_BLACK   = 1;
constexpr uint8_t PIXEL_RED     = 2;
constexpr uint8_t PIXEL_BLUE    = 3;
constexpr uint8_t PIXEL_GREEN   = 4;
constexpr uint8_t PIXEL_CYAN    = 5;
constexpr uint8_t PIXEL_MAGENTA = 6;
constexpr uint8_t PIXEL_YELLOW  = 7;
constexpr uint8_t PIXEL_UNKNOWN = 8;
constexpr uint8_t PIXEL_NUM     = 9;

IMAGE_CODEC_API std::string get_pixel_name(Pixel pixel);

IMAGE_CODEC_API PixelType parse_pixel_type(const std::string& pixel_type_str);
IMAGE_CODEC_API std::string get_pixel_type_str(PixelType pixel_type);

IMAGE_CODEC_API void encrypt_part_bytes(Bytes& bytes);

class PixelCodec {
public:
	static constexpr int CRC_BYTE_NUM = 4;
	static constexpr int PART_ID_BYTE_NUM = 4;
	static constexpr int META_BYTE_NUM = CRC_BYTE_NUM + PART_ID_BYTE_NUM;

	virtual PixelType GetPixelType() = 0;
	virtual int PixelValueNum() = 0;
	virtual int BitNumPerPixel() = 0;
	IMAGE_CODEC_API Pixels Encode(uint32_t part_id, const Bytes& part_bytes, int pixel_num);
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
