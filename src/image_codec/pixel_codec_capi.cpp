#include <algorithm>

#include "image_codec_api.h"
#include "image_codec_types.h"
#include "pixel_codec.h"

extern "C" {

IMAGE_CODEC_API void* create_pixel_codec_c(char* pixel_type_str) {
    PixelType pixel_type = parse_pixel_type(pixel_type_str);
    if (pixel_type == PixelType::PIXEL2) {
        return new Pixel2Codec;
    } else if (pixel_type == PixelType::PIXEL4) {
        return new Pixel4Codec;
    } else if (pixel_type == PixelType::PIXEL8) {
        return new Pixel8Codec;
    } else {
        return nullptr;
    }
}

IMAGE_CODEC_API void pixel_codec_encode_c(void* pixel_codec, Pixel* pixels, uint32_t part_id, Byte* part_bytes, size_t part_byte_num, int pixel_num) {
    Bytes part_bytes1(part_bytes, part_bytes+part_byte_num);
    Pixels pixels1 = reinterpret_cast<PixelCodec*>(pixel_codec)->Encode(part_id, part_bytes1, pixel_num);
    std::copy_n(pixels1.data(), pixel_num, pixels);
}

IMAGE_CODEC_API void pixel_codec_decode_c(void* pixel_codec, bool* success, uint32_t* part_id, Byte* part_bytes, Pixel* pixels, int pixel_num) {
    Pixels pixels1(pixels, pixels+pixel_num);
    auto [success1, part_id1, part_bytes1] = reinterpret_cast<PixelCodec*>(pixel_codec)->Decode(pixels1);
    *success = success1;
    *part_id = part_id1;
    std::copy(part_bytes1.begin(), part_bytes1.end(), part_bytes);
}

IMAGE_CODEC_API void destory_pixel_codec_c(void* pixel_codec) {
    delete reinterpret_cast<PixelCodec*>(pixel_codec);
}

}
