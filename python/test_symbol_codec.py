import sys

import symbol_codec
import image_decode_task

def test_symbol_codec(symbol_type_str):
    symbol_type = symbol_codec.parse_symbol_type(symbol_type_str)
    codec = symbol_codec.create_symbol_codec(symbol_type)
    part_id1 = 0
    for frame_size in range(0, 2048, 7):
        part_byte_num = image_decode_task.get_part_byte_num(symbol_type, (1, 1, frame_size, 1))
        if part_byte_num >= image_decode_task.Task.min_part_byte_num:
            for padding_byte_num in range(0, 64, 3):
                if padding_byte_num < part_byte_num:
                    part_bytes1 = bytes([i % 256 for i in range(part_byte_num - padding_byte_num)])
                    symbols = codec.encode(part_id1, part_bytes1, frame_size)
                    success, part_id2, part_bytes2 = codec.decode(symbols)
                    if not success or part_id1 != part_id2 or part_bytes1 != part_bytes2[:part_byte_num-padding_byte_num]:
                        print('{} codec {} {} fail'.format(symbol_type_str, frame_size, padding_byte_num))
                        print('success={}'.format(success))
                        print('part_id1={}'.format(part_id1))
                        print('part_id2={}'.format(part_id2))
                        print('part_bytes1={}'.format(part_bytes1))
                        print('part_bytes2={}'.format(part_bytes2))
                        print('symbols={}'.format(symbols))
                        return False
                    part_id1 += 1
    print('{} codec {} tests pass'.format(symbol_type_str, part_id1))
    return True

is_pass = True
is_pass = is_pass and test_symbol_codec('symbol1')
is_pass = is_pass and test_symbol_codec('symbol2')
is_pass = is_pass and test_symbol_codec('symbol3')
if is_pass:
    print('pass')
    sys.exit(0)
else:
    print('fail')
    sys.exit(1)
