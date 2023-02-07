import enum
import struct
import zlib

@enum.unique
class SymbolType(enum.Enum):
    SYMBOL1 = 0
    SYMBOL2 = 1
    SYMBOL3 = 2

def parse_symbol_type(symbol_type_str):
    return SymbolType[symbol_type_str.upper()]

def encrypt_bytes(bytes1):
    bytes1 = bytearray(reversed(bytes1))
    for i in range(len(bytes1)):
        bytes1[i] ^= 170
    return bytes1

def scramble_part_id_bytes(part_id_bytes, crc_bytes):
    return bytes(b1 ^ b2 for b1, b2 in zip(part_id_bytes, crc_bytes))

class SymbolCodec:
    crc_byte_num = 4
    part_id_byte_num = 4
    meta_byte_num = crc_byte_num + part_id_byte_num

    symbol_type = None
    bit_num_per_symbol = None

    def get_symbol_value_num(self):
        return 2 ** self.bit_num_per_symbol

    def bytes_to_symbols(self, b):
        raise NotImplementedError()

    def symbols_to_bytes(self, symbols):
        raise NotImplementedError()

    def encode(self, part_id, part_bytes, frame_size):
        assert frame_size >= ((self.meta_byte_num + len(part_bytes)) * 8 + self.bit_num_per_symbol - 1) // self.bit_num_per_symbol
        part_id_bytes = struct.pack('<I', part_id)
        padded_part_bytes = part_bytes + bytes([0] * (frame_size * self.bit_num_per_symbol // 8 - self.meta_byte_num - len(part_bytes)))
        bytes_for_crc = part_id_bytes + padded_part_bytes
        crc = zlib.crc32(bytes_for_crc)
        crc_bytes = struct.pack('<I', crc)
        scrambled_part_id_bytes = scramble_part_id_bytes(part_id_bytes, crc_bytes)
        symbols = self.bytes_to_symbols(encrypt_bytes(crc_bytes + scrambled_part_id_bytes + padded_part_bytes))
        symbols += [0] * (frame_size - len(symbols))
        return symbols

    def decode(self, symbols):
        assert len(symbols) >= ((self.meta_byte_num + 1) * 8 + self.bit_num_per_symbol - 1) // self.bit_num_per_symbol
        truncated_symbols = symbols[:(len(symbols) * self.bit_num_per_symbol // 8 * 8 + self.bit_num_per_symbol - 1) // self.bit_num_per_symbol]
        bytes1 = encrypt_bytes(self.symbols_to_bytes(truncated_symbols))
        crc_bytes = bytes1[:self.crc_byte_num]
        crc = struct.unpack('<I', crc_bytes)[0]
        scrambled_part_id_bytes = bytes1[self.crc_byte_num:self.meta_byte_num]
        part_id_bytes = scramble_part_id_bytes(scrambled_part_id_bytes, crc_bytes)
        part_id = struct.unpack('<I', part_id_bytes)[0]
        padded_part_bytes = bytes1[self.meta_byte_num:]
        bytes_for_crc = part_id_bytes + padded_part_bytes
        crc_computed = zlib.crc32(bytes_for_crc)
        success = crc_computed == crc
        return success, part_id, padded_part_bytes

class Symbol1Codec(SymbolCodec):
    symbol_type = SymbolType.SYMBOL1
    bit_num_per_symbol = 1

    def bytes_to_symbols(self, b):
        s = ''.join('{:0>8b}'.format(e) for e in b)
        return [int(e) for e in s]

    def symbols_to_bytes(self, symbols):
        return bytes([int(''.join(str(e) for e in symbols[i:i+8]), 2) for i in range(0, len(symbols), 8)])

class Symbol2Codec(SymbolCodec):
    symbol_type = SymbolType.SYMBOL2
    bit_num_per_symbol = 2

    def bytes_to_symbols(self, b):
        s = ''.join('{:0>8b}'.format(e) for e in b)
        return [int(s[i:i+2], 2) for i in range(0, len(s), 2)]

    def symbols_to_bytes(self, symbols):
        return bytes([int(''.join('{:0>2b}'.format(e) for e in symbols[i:i+4]), 2) for i in range(0, len(symbols), 4)])

class Symbol3Codec(SymbolCodec):
    symbol_type = SymbolType.SYMBOL3
    bit_num_per_symbol = 3

    def bytes_to_symbols(self, b):
        s = ''.join('{:0>8b}'.format(e) for e in b)
        s += '0' * ((len(s) + 2) // 3 * 3 - len(s))
        return [int(s[i:i+3], 2) for i in range(0, len(s), 3)]

    def symbols_to_bytes(self, symbols):
        s = ''.join('{:0>3b}'.format(e) for e in symbols)
        s = s[:len(s) // 8 * 8]
        return bytes([int(s[i:i+8], 2) for i in range(0, len(s), 8)])

symbol_type_to_symbol_codec_mapping = {
    SymbolType.SYMBOL1: Symbol1Codec,
    SymbolType.SYMBOL2: Symbol2Codec,
    SymbolType.SYMBOL3: Symbol3Codec,
    }

def create_symbol_codec(symbol_type):
    return symbol_type_to_symbol_codec_mapping[symbol_type]()
