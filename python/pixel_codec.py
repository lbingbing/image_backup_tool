import enum
import struct
import zlib

class PixelType(enum.Enum):
    PIXEL2 = 0
    PIXEL4 = 1
    PIXEL8 = 2

class PixelValue(enum.Enum):
    WHITE   = 0
    BLACK   = 1
    RED     = 2
    BLUE    = 3
    GREEN   = 4
    CYAN    = 5
    MAGENTA = 6
    YELLOW  = 7

def parse_pixel_type(pixel_type_str):
    if pixel_type_str == 'pixel2':
        return PixelType.PIXEL2
    elif pixel_type_str == 'pixel4':
        return PixelType.PIXEL4
    elif pixel_type_str == 'pixel8':
        return PixelType.PIXEL8
    else:
        assert 0, "invalid pixel type '{}'".format(pixel_type_str)

def encrypt_part_bytes(part_bytes):
    part_bytes = bytearray(reversed(part_bytes))
    for i in range(len(part_bytes)):
        part_bytes[i] ^= 170
    return part_bytes

class PixelCodec:
    crc_byte_num = 4
    part_id_byte_num = 4
    meta_byte_num = crc_byte_num + part_id_byte_num

    pixel_value_num = None
    bit_num_per_pixel = None

    def bytes_to_pixels(self, b):
        raise NotImplementedError()

    def pixels_to_bytes(self, pixels):
        raise NotImplementedError()

    def encode(self, part_id, part_bytes, pixel_num):
        assert pixel_num >= ((self.meta_byte_num + len(part_bytes)) * 8 + self.bit_num_per_pixel - 1) // self.bit_num_per_pixel
        part_id_bytes = struct.pack('<I', part_id)
        padded_part_bytes = part_bytes +  bytes([0] * (pixel_num * self.bit_num_per_pixel // 8 - self.meta_byte_num - len(part_bytes)))
        bytes_for_crc = part_id_bytes + padded_part_bytes
        crc = zlib.crc32(bytes_for_crc)
        crc_bytes = struct.pack('<I', crc)
        bytes1 = crc_bytes + part_id_bytes + padded_part_bytes
        pixels = self.bytes_to_pixels(bytes1)
        pixels += [0] * (pixel_num - len(pixels))
        return pixels

    def decode(self, pixels):
        assert len(pixels) >= ((self.meta_byte_num + 1) * 8 + self.bit_num_per_pixel - 1) // self.bit_num_per_pixel
        truncated_pixels = pixels[:(len(pixels) * self.bit_num_per_pixel // 8 * 8 + self.bit_num_per_pixel - 1) // self.bit_num_per_pixel]
        bytes1 = self.pixels_to_bytes(truncated_pixels)
        crc_bytes = bytes1[:self.crc_byte_num]
        crc = struct.unpack('<I', crc_bytes)[0]
        part_id_bytes = bytes1[self.crc_byte_num:self.meta_byte_num]
        part_id = struct.unpack('<I', part_id_bytes)[0]
        part_bytes = bytes1[self.meta_byte_num:]
        bytes_for_crc = part_id_bytes + part_bytes
        crc_computed = zlib.crc32(bytes_for_crc)
        success = crc_computed == crc
        return success, part_id, part_bytes

class Pixel2Codec(PixelCodec):
    pixel_value_num = 2
    bit_num_per_pixel = 1

    def bytes_to_pixels(self, b):
        s = ''.join('{:0>8b}'.format(e) for e in b)
        return [int(e) for e in s]

    def pixels_to_bytes(self, pixels):
        return bytes([int(''.join(str(e) for e in pixels[i:i+8]), 2) for i in range(0, len(pixels), 8)])

class Pixel4Codec(PixelCodec):
    pixel_value_num = 4
    bit_num_per_pixel = 2

    def bytes_to_pixels(self, b):
        s = ''.join('{:0>8b}'.format(e) for e in b)
        return [int(s[i:i+2], 2) for i in range(0, len(s), 2)]

    def pixels_to_bytes(self, pixels):
        return bytes([int(''.join('{:0>2b}'.format(e) for e in pixels[i:i+4]), 2) for i in range(0, len(pixels), 4)])

class Pixel8Codec(PixelCodec):
    pixel_value_num = 8
    bit_num_per_pixel = 3

    def bytes_to_pixels(self, b):
        s = ''.join('{:0>8b}'.format(e) for e in b)
        s += '0' * ((len(s) + 2) // 3 * 3 - len(s))
        return [int(s[i:i+3], 2) for i in range(0, len(s), 3)]

    def pixels_to_bytes(self, pixels):
        s = ''.join('{:0>3b}'.format(e) for e in pixels)
        s = s[:len(s) // 8 * 8]
        return bytes([int(s[i:i+8], 2) for i in range(0, len(s), 8)])

def create_pixel_codec(pixel_type):
    if pixel_type == PixelType.PIXEL2:
        return Pixel2Codec()
    elif pixel_type == PixelType.PIXEL4:
        return Pixel4Codec()
    elif pixel_type == PixelType.PIXEL8:
        return Pixel8Codec()
    else:
        assert 0, "invalid pixel type '{}'".format(pixel_type)
