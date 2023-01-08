import sys

import image_codec_types
import pixel_codec
import image_decode_task

def test_pixel_codec(pixel_type_str):
    pixel_type = image_codec_types.parse_pixel_type(pixel_type_str)
    codec = pixel_codec.create_pixel_codec(pixel_type)
    part_id1 = 0
    for pixel_num in range(256):
        part_byte_num = image_decode_task.get_part_byte_num(1, 1, pixel_num, 1, pixel_type)
        if part_byte_num >= image_decode_task.Task.min_part_byte_num:
            for padding_byte_num in range(16):
                if padding_byte_num < part_byte_num:
                    part_bytes1 = bytes([i % 256 for i in range(part_byte_num - padding_byte_num)])
                    symbols = codec.encode(part_id1, part_bytes1, pixel_num)
                    success, part_id2, part_bytes2 = codec.decode(symbols)
                    if not success or part_id2 != part_id1 or part_bytes2[:part_byte_num-padding_byte_num] != part_bytes1:
                        print('{} encode {} {} fail'.format(pixel_type_str, pixel_num, padding_byte_num))
                        return False
                    part_id1 += 1
    print('{} {} tests pass'.format(pixel_type_str, part_id1))
    return True

is_pass = True
is_pass = is_pass and test_pixel_codec('pixel2')
is_pass = is_pass and test_pixel_codec('pixel4')
is_pass = is_pass and test_pixel_codec('pixel8')
if is_pass:
    print('pass')
    sys.exit(0)
else:
    print('fail')
    sys.exit(1)
