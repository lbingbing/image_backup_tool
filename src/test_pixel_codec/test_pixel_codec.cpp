#include <iostream>
#include <string>

#include "image_codec.h"

bool test_pixel_codec(const std::string& pixel_type_str) {
    auto pixel_codec = create_pixel_codec(parse_pixel_type(pixel_type_str));
    for (int i = 0; i < 2048; ++i) {
        Bytes bytes;
        for (int j = 0; j < i + 1; ++j) {
            bytes.push_back(j % 256);
        }
        for (int j = 0; j < 32; ++j) {
            Pixels pixels = pixel_codec->Encode(i, bytes, ((PixelCodec::META_BYTE_NUM + (i + 1)) * 8 + pixel_codec->BitNumPerPixel() - 1) / pixel_codec->BitNumPerPixel() + j);
            auto [success, part_id, part_bytes] = pixel_codec->Decode(pixels);
            if (!success) {
                std::cout << pixel_type_str << " encode " << i << " " << j << " fail\n";
                return false;
            }
        }
    }
    std::cout << pixel_type_str << " pass\n";
    return true;
}

int main() {
    bool pass = true;
    pass = pass && test_pixel_codec("pixel2");
    pass = pass && test_pixel_codec("pixel4");
    pass = pass && test_pixel_codec("pixel8");
    if (pass) {
        std::cout << "pass\n";
        return 0;
    } else {
        std::cout << "fail\n";
        return 1;
    }
}
