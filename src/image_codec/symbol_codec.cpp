#include <iomanip>
#include <map>

#include <boost/crc.hpp>

#include "symbol_codec.h"

namespace {

std::map<std::string, SymbolType> symbol_type_str_to_symbol_type_mapping({
    {"symbol1", SymbolType::SYMBOL1},
    {"symbol2", SymbolType::SYMBOL2},
    {"symbol3", SymbolType::SYMBOL3},
});

std::map<SymbolType, std::string> symbol_type_to_symbol_type_str_mapping({
    {SymbolType::SYMBOL1, "symbol1"},
    {SymbolType::SYMBOL2, "symbol2"},
    {SymbolType::SYMBOL3, "symbol3"},
});

}

SymbolType parse_symbol_type(const std::string& symbol_type_str) {
    if (symbol_type_str_to_symbol_type_mapping.find(symbol_type_str) == symbol_type_str_to_symbol_type_mapping.end()) {
        throw std::invalid_argument("invalid symbol type '" + symbol_type_str + "'");
    }
    return symbol_type_str_to_symbol_type_mapping[symbol_type_str];
}

std::string get_symbol_type_str(SymbolType symbol_type) {
    if (symbol_type_to_symbol_type_str_mapping.find(symbol_type) == symbol_type_to_symbol_type_str_mapping.end()) {
        throw std::invalid_argument("invalid symbol type " + std::to_string(static_cast<int>(symbol_type)));
    }
    return symbol_type_to_symbol_type_str_mapping[symbol_type];
}

namespace {

Bytes encrypt_bytes(const Bytes& bytes) {
    Bytes encrypted_bytes(bytes.rbegin(), bytes.rend());
    for (auto& e : encrypted_bytes) {
        e ^= Byte(170);
    }
    return encrypted_bytes;
}

Bytes uint32_to_bytes(uint32_t v) {
    return {
        static_cast<Byte>((v >> 0) & 0xff),
        static_cast<Byte>((v >> 8) & 0xff),
        static_cast<Byte>((v >> 16) & 0xff),
        static_cast<Byte>((v >> 24) & 0xff),
    };
}

uint32_t bytes_to_uint32(const Bytes& bytes) {
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

uint32_t crc32(const Bytes& bytes) {
    boost::crc_32_type crc_gen;
    crc_gen.process_bytes(bytes.data(), bytes.size());
    return crc_gen.checksum();
}

Bytes scramble_part_id_bytes(const Bytes& part_id_bytes, const Bytes& crc_bytes) {
    Bytes scrambled_part_id_bytes = part_id_bytes;
    for (size_t i = 0; i < scrambled_part_id_bytes.size(); ++i) {
        scrambled_part_id_bytes[i] ^= crc_bytes[i];
    }
    return scrambled_part_id_bytes;
}

}

Symbols SymbolCodec::Encode(uint32_t part_id, const Bytes& part_bytes, int frame_size) {
    if (frame_size < ((META_BYTE_NUM + part_bytes.size()) * 8 + BitNumPerSymbol() - 1) / BitNumPerSymbol()) throw std::invalid_argument("invalid encode arguments");
    Bytes part_id_bytes = uint32_to_bytes(part_id);
    Bytes padded_part_bytes = part_bytes;
    padded_part_bytes.resize(static_cast<size_t>(frame_size * BitNumPerSymbol() / 8 - META_BYTE_NUM), 0);
    Bytes bytes_for_crc;
    bytes_for_crc.insert(bytes_for_crc.end(), part_id_bytes.begin(), part_id_bytes.end());
    bytes_for_crc.insert(bytes_for_crc.end(), padded_part_bytes.begin(), padded_part_bytes.end());
    uint32_t crc = crc32(bytes_for_crc);
    Bytes crc_bytes = uint32_to_bytes(crc);
    Bytes scrambled_part_id_bytes = scramble_part_id_bytes(part_id_bytes, crc_bytes);
    Bytes bytes;
    bytes.insert(bytes.end(), crc_bytes.begin(), crc_bytes.end());
    bytes.insert(bytes.end(), scrambled_part_id_bytes.begin(), scrambled_part_id_bytes.end());
    bytes.insert(bytes.end(), padded_part_bytes.begin(), padded_part_bytes.end());
    Symbols symbols = BytesToSymbols(encrypt_bytes(bytes));
    symbols.resize(frame_size, 0);
    if (0) {
        std::cout << "encode\n";
        std::cout << "frame_size: " << std::dec << frame_size << "\n";
        std::cout << "part_id: " << std::hex << part_id << "\n";
        std::cout << "part_id_bytes: " << part_id_bytes << "\n";
        std::cout << "part_bytes: " << part_bytes << "\n";
        std::cout << "padded_part_bytes: " << padded_part_bytes << "\n";
        std::cout << "bytes_for_crc: " << bytes_for_crc << "\n";
        std::cout << "crc: " << std::hex << crc << "\n";
        std::cout << "crc_bytes: " << crc_bytes << "\n";
        std::cout << "bytes: " << bytes << "\n";
        std::cout << "symbols: " << symbols << "\n";
    }
    return symbols;
}

DecodeResult SymbolCodec::Decode(const Symbols& symbols) {
    if (static_cast<int>(symbols.size()) < ((META_BYTE_NUM + 1) * 8 + BitNumPerSymbol() - 1) / BitNumPerSymbol()) throw std::invalid_argument("invalid encode arguments");
    Symbols truncated_symbols(symbols.begin(), symbols.begin() + (symbols.size() * BitNumPerSymbol() / 8 * 8 + BitNumPerSymbol() - 1) / BitNumPerSymbol());
    Bytes bytes = encrypt_bytes(SymbolsToBytes(truncated_symbols));
    Bytes crc_bytes(bytes.begin(), bytes.begin() + CRC_BYTE_NUM);
    uint32_t crc = bytes_to_uint32(crc_bytes);
    Bytes scrambled_part_id_bytes(bytes.begin() + CRC_BYTE_NUM, bytes.begin() + META_BYTE_NUM);
    Bytes part_id_bytes = scramble_part_id_bytes(scrambled_part_id_bytes, crc_bytes);
    uint32_t part_id = bytes_to_uint32(part_id_bytes);
    Bytes padded_part_bytes(bytes.begin() + META_BYTE_NUM, bytes.end());
    Bytes bytes_for_crc;
    bytes_for_crc.insert(bytes_for_crc.end(), part_id_bytes.begin(), part_id_bytes.end());
    bytes_for_crc.insert(bytes_for_crc.end(), padded_part_bytes.begin(), padded_part_bytes.end());
    uint32_t crc_computed = crc32(bytes_for_crc);
    bool success = crc_computed == crc;
    if (0) {
        std::cout << "decode\n";
        std::cout << "symbols: " << symbols << "\n";
        std::cout << "truncated_symbols: " << truncated_symbols << "\n";
        std::cout << "bytes: " << bytes << "\n";
        std::cout << "crc_bytes: " << crc_bytes << "\n";
        std::cout << "crc: " << std::hex << crc << "\n";
        std::cout << "part_id_bytes: " << part_id_bytes << "\n";
        std::cout << "part_id: " << std::hex << part_id << "\n";
        std::cout << "padded_part_bytes: " << padded_part_bytes << "\n";
        std::cout << "bytes_for_crc: " << bytes_for_crc << "\n";
        std::cout << "crc_computed: " << crc_computed << "\n";
    }
    return {success, part_id, padded_part_bytes};
}

Symbols Symbol1Codec::BytesToSymbols(const Bytes& bytes) {
    Symbols symbols;
    for (auto byte : bytes) {
        symbols.push_back((byte >> 7) & 0x1);
        symbols.push_back((byte >> 6) & 0x1);
        symbols.push_back((byte >> 5) & 0x1);
        symbols.push_back((byte >> 4) & 0x1);
        symbols.push_back((byte >> 3) & 0x1);
        symbols.push_back((byte >> 2) & 0x1);
        symbols.push_back((byte >> 1) & 0x1);
        symbols.push_back((byte >> 0) & 0x1);
    }
    return symbols;
}

Bytes Symbol1Codec::SymbolsToBytes(const Symbols& symbols) {
    Bytes bytes;
    for (size_t i = 0; i < symbols.size(); i += 8) {
        Byte byte = 0;
        byte |= (symbols[i+0] & 0x1) << 7;
        byte |= (symbols[i+1] & 0x1) << 6;
        byte |= (symbols[i+2] & 0x1) << 5;
        byte |= (symbols[i+3] & 0x1) << 4;
        byte |= (symbols[i+4] & 0x1) << 3;
        byte |= (symbols[i+5] & 0x1) << 2;
        byte |= (symbols[i+6] & 0x1) << 1;
        byte |= (symbols[i+7] & 0x1) << 0;
        bytes.push_back(byte);
    }
    return bytes;
}

Symbols Symbol2Codec::BytesToSymbols(const Bytes& bytes) {
    Symbols symbols;
    for (auto byte : bytes) {
        symbols.push_back((byte >> 6) & 0x3);
        symbols.push_back((byte >> 4) & 0x3);
        symbols.push_back((byte >> 2) & 0x3);
        symbols.push_back((byte >> 0) & 0x3);
    }
    return symbols;
}

Bytes Symbol2Codec::SymbolsToBytes(const Symbols& symbols) {
    Bytes bytes;
    for (size_t i = 0; i < symbols.size(); i += 4) {
        Byte byte = 0;
        byte |= (symbols[i+0] & 0x3) << 6;
        byte |= (symbols[i+1] & 0x3) << 4;
        byte |= (symbols[i+2] & 0x3) << 2;
        byte |= (symbols[i+3] & 0x3) << 0;
        bytes.push_back(byte);
    }
    return bytes;
}

Symbols Symbol3Codec::BytesToSymbols(const Bytes& bytes) {
    Symbols symbols;
    Byte left = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        Byte byte = bytes[i];
        if (i % 3 == 0) {
            symbols.push_back((byte >> 5) & 0x7);
            symbols.push_back((byte >> 2) & 0x7);
            left = byte & 0x3;
        } else if (i % 3 == 1) {
            symbols.push_back((left << 1) | ((byte >> 7) & 0x1));
            symbols.push_back((byte >> 4) & 0x7);
            symbols.push_back((byte >> 1) & 0x7);
            left = byte & 0x1;
        } else if (i % 3 == 2) {
            symbols.push_back((left << 2)  | ((byte >> 6) & 0x3));
            symbols.push_back((byte >> 3) & 0x7);
            symbols.push_back(byte & 0x7);
            left = 0;
        }
    }
    if (bytes.size() % 3 != 0) {
        symbols.push_back(left << (bytes.size() % 3));
    }
    return symbols;
}

Bytes Symbol3Codec::SymbolsToBytes(const Symbols& symbols) {
    Bytes bytes;
    Byte byte = 0;
    for (size_t i = 0; i < symbols.size(); ++i) {
        Symbol symbol = symbols[i];
        if (i % 8 == 0) {
            byte = symbol << 5;
        } else if (i % 8 == 1) {
            byte = byte | symbol << 2;
        } else if (i % 8 == 2) {
            byte = byte | symbol >> 1;
            bytes.push_back(byte);
            byte = (symbol & 0x1) << 7;
        } else if (i % 8 == 3) {
            byte = byte | symbol << 4;
        } else if (i % 8 == 4) {
            byte = byte | symbol << 1;
        } else if (i % 8 == 5) {
            byte = byte | symbol >> 2;
            bytes.push_back(byte);
            byte = (symbol & 0x3) << 6;
        } else if (i % 8 == 6) {
            byte = byte | symbol << 3;
        } else if (i % 8 == 7) {
            byte = byte | symbol;
            bytes.push_back(byte);
        }
    }
    return bytes;
}

std::unique_ptr<SymbolCodec> create_symbol_codec(SymbolType symbol_type) {
    std::unique_ptr<SymbolCodec> symbol_codec;
    if (symbol_type == SymbolType::SYMBOL1) {
        symbol_codec = std::make_unique<Symbol1Codec>();
    } else if (symbol_type == SymbolType::SYMBOL2) {
        symbol_codec = std::make_unique<Symbol2Codec>();
    } else if (symbol_type == SymbolType::SYMBOL3) {
        symbol_codec = std::make_unique<Symbol3Codec>();
    } else {
        throw std::invalid_argument("invalid symbol type '" + std::to_string(static_cast<int>(symbol_type)) + "'");
    }
    return symbol_codec;
}
