import enum
import struct
import zlib

import image_codec_types

def encrypt_bytes(bytes1):
    bytes1 = bytearray(reversed(bytes1))
    for i in range(len(bytes1)):
        bytes1[i] ^= 170
    return bytes1

def scramble_part_id_bytes(part_id_bytes, crc_bytes):
    return bytes(b1 ^ b2 for b1, b2 in zip(part_id_bytes, crc_bytes))

class PixelCodec:
    crc_byte_num = 4
    part_id_byte_num = 4
    meta_byte_num = crc_byte_num + part_id_byte_num

    pixel_value_num = None
    bit_num_per_pixel = None

    def get_pixel_type(self):
        raise NotImplementedError()

    def bytes_to_pixels(self, b):
        raise NotImplementedError()

    def pixels_to_bytes(self, pixels):
        raise NotImplementedError()

    def encode(self, part_id, part_bytes, pixel_num):
        assert pixel_num >= ((self.meta_byte_num + len(part_bytes)) * 8 + self.bit_num_per_pixel - 1) // self.bit_num_per_pixel
        part_id_bytes = struct.pack('<I', part_id)
        padded_part_bytes = part_bytes + bytes([0] * (pixel_num * self.bit_num_per_pixel // 8 - self.meta_byte_num - len(part_bytes)))
        bytes_for_crc = part_id_bytes + padded_part_bytes
        crc = zlib.crc32(bytes_for_crc)
        crc_bytes = struct.pack('<I', crc)
        scrambled_part_id_bytes = scramble_part_id_bytes(part_id_bytes, crc_bytes)
        pixels = self.bytes_to_pixels(encrypt_bytes(crc_bytes + scrambled_part_id_bytes + padded_part_bytes))
        pixels += [0] * (pixel_num - len(pixels))
        return pixels

    def decode(self, pixels):
        assert len(pixels) >= ((self.meta_byte_num + 1) * 8 + self.bit_num_per_pixel - 1) // self.bit_num_per_pixel
        truncated_pixels = pixels[:(len(pixels) * self.bit_num_per_pixel // 8 * 8 + self.bit_num_per_pixel - 1) // self.bit_num_per_pixel]
        bytes1 = encrypt_bytes(self.pixels_to_bytes(truncated_pixels))
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

class Pixel2Codec(PixelCodec):
    pixel_value_num = 2
    bit_num_per_pixel = 1

    def get_pixel_type(self):
        return image_codec_types.PixelType.PIXEL2

    def bytes_to_pixels(self, b):
        s = ''.join('{:0>8b}'.format(e) for e in b)
        return [int(e) for e in s]

    def pixels_to_bytes(self, pixels):
        return bytes([int(''.join(str(e) for e in pixels[i:i+8]), 2) for i in range(0, len(pixels), 8)])

class Pixel4Codec(PixelCodec):
    pixel_value_num = 4
    bit_num_per_pixel = 2

    def get_pixel_type(self):
        return image_codec_types.PixelType.PIXEL4

    def bytes_to_pixels(self, b):
        s = ''.join('{:0>8b}'.format(e) for e in b)
        return [int(s[i:i+2], 2) for i in range(0, len(s), 2)]

    def pixels_to_bytes(self, pixels):
        return bytes([int(''.join('{:0>2b}'.format(e) for e in pixels[i:i+4]), 2) for i in range(0, len(pixels), 4)])

class Pixel8Codec(PixelCodec):
    pixel_value_num = 8
    bit_num_per_pixel = 3

    def get_pixel_type(self):
        return image_codec_types.PixelType.PIXEL8

    def bytes_to_pixels(self, b):
        s = ''.join('{:0>8b}'.format(e) for e in b)
        s += '0' * ((len(s) + 2) // 3 * 3 - len(s))
        return [int(s[i:i+3], 2) for i in range(0, len(s), 3)]

    def pixels_to_bytes(self, pixels):
        s = ''.join('{:0>3b}'.format(e) for e in pixels)
        s = s[:len(s) // 8 * 8]
        return bytes([int(s[i:i+8], 2) for i in range(0, len(s), 8)])

def create_pixel_codec(pixel_type):
    if pixel_type == image_codec_types.PixelType.PIXEL2:
        return Pixel2Codec()
    elif pixel_type == image_codec_types.PixelType.PIXEL4:
        return Pixel4Codec()
    elif pixel_type == image_codec_types.PixelType.PIXEL8:
        return Pixel8Codec()
    else:
        assert 0, "invalid pixel type '{}'".format(pixel_type)
