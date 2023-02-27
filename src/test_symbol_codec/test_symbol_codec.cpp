#include <iostream>
#include <string>

#include "image_codec.h"

bool test_symbol_codec(const std::string& symbol_type_str) {
    auto symbol_type = parse_symbol_type(symbol_type_str);
    auto symbol_codec = create_symbol_codec(symbol_type);
    uint32_t part_id1 = 0;
    for (int frame_size = 0; frame_size < 4096; frame_size += 7) {
        if (frame_size * symbol_codec->BitNumPerSymbol() / 8 >= SymbolCodec::META_BYTE_NUM + Task::MIN_PART_BYTE_NUM) {
            Dim dim{1, 1, frame_size, 1};
            int part_byte_num = get_part_byte_num(symbol_type, dim);
            for (int padding_byte_num = 0; padding_byte_num < 128; padding_byte_num += 3) {
                if (padding_byte_num < part_byte_num) {
                    Bytes part_bytes1;
                    for (int i = 0; i < part_byte_num - padding_byte_num; ++i) {
                        part_bytes1.push_back(i % 256);
                    }
                    Symbols symbols = symbol_codec->Encode(part_id1, part_bytes1, frame_size);
                    auto [success, part_id2, part_bytes2] = symbol_codec->Decode(symbols);
                    part_bytes2.resize(part_byte_num - padding_byte_num);
                    if (!success || part_id1 != part_id2 || part_bytes1 != part_bytes2) {
                        std::cout << symbol_type_str << " codec " << frame_size << " " << padding_byte_num << " fail\n";
                        std::cout << "success=" << success << "\n";
                        std::cout << "part_id1=" << part_id1 << "\n";
                        std::cout << "part_id2=" << part_id2 << "\n";
                        std::cout << "part_bytes1=" << part_bytes1 << "\n";
                        std::cout << "part_bytes2=" << part_bytes2 << "\n";
                        std::cout << "symbols=" << symbols << "\n";
                        return false;
                    }
                    part_id1 += 1;
                }
            }
        }
    }
    std::cout << symbol_type_str << " codec " << part_id1 << " tests pass\n";
    return true;
}

int main() {
    bool pass = true;
    pass = pass && test_symbol_codec("symbol1");
    pass = pass && test_symbol_codec("symbol2");
    pass = pass && test_symbol_codec("symbol3");
    if (pass) {
        std::cout << "pass\n";
        return 0;
    } else {
        std::cout << "fail\n";
        return 1;
    }
}
