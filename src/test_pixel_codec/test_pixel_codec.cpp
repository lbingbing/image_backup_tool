#include <iostream>
#include <string>

#include "image_codec.h"

bool test_pixel_codec(const std::string& pixel_type_str) {
    auto pixel_type = parse_pixel_type(pixel_type_str);
    auto pixel_codec = create_pixel_codec(pixel_type);
    uint32_t part_id1 = 0;
    for (int frame_size = 0; frame_size < 2048; ++frame_size) {
        if (frame_size * pixel_codec->BitNumPerPixel() / 8 >= PixelCodec::META_BYTE_NUM + Task::MIN_PART_BYTE_NUM) {
            Dim dim{1, 1, frame_size, 1};
            uint32_t part_byte_num = get_part_byte_num(dim, pixel_type);
            for (int padding_byte_num = 0; padding_byte_num < 32; ++padding_byte_num) {
                if (padding_byte_num < part_byte_num) {
                    Bytes part_bytes1;
                    for (int i = 0; i < part_byte_num - padding_byte_num; ++i) {
                        part_bytes1.push_back(i % 256);
                    }
                    Pixels pixels = pixel_codec->Encode(part_id1, part_bytes1, frame_size);
                    auto [success, part_id2, part_bytes2] = pixel_codec->Decode(pixels);
                    part_bytes2.resize(part_byte_num - padding_byte_num);
                    if (!success || part_id2 != part_id1 || part_bytes2 != part_bytes1) {
                        std::cout << pixel_type_str << " encode " << frame_size << " " << padding_byte_num << " fail\n";
                        return false;
                    }
                    part_id1 += 1;
                }
            }
        }
    }
    std::cout << pixel_type_str << " " << part_id1 << " tests pass\n";
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
