import argparse
import os
import subprocess

import image_codec_types
import symbol_codec
import image_decode_task

parser = argparse.ArgumentParser()
parser.add_argument('mode', choices=['p2p', 'c2c', 'p2c', 'c2p'], help='mode')
parser.add_argument('target_file', help='target file')
parser.add_argument('target_file_size', type=int, help='target file size')
parser.add_argument('symbol_type', help='symbol type')
parser.add_argument('dim', help='dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size')
parser.add_argument('pixel_size', help='pixel size')
parser.add_argument('space_size', help='space size')
parser.add_argument('port', help='server port')
args = parser.parse_args()

target_file1 = args.target_file + '1'
target_file2 = args.target_file + '2'
symbol_type = symbol_codec.parse_symbol_type(args.symbol_type)
dim = image_codec_types.parse_dim(args.dim)

target_file_bytes1 = bytes(i & 0xff for i in range(args.target_file_size))

with open(target_file1, 'wb') as f:
    f.write(target_file_bytes1)

part_byte_num = image_decode_task.get_part_byte_num(symbol_type, dim)
assert part_byte_num >= image_decode_task.Task.min_part_byte_num
raw_bytes, part_num = image_decode_task.get_task_bytes(target_file1, part_byte_num)
part_num_str = str(part_num)

p_cmd1 = []
p_cmd2 = []
src, dst = args.mode.split('2')
if src == 'p':
    p_cmd1 += ['python', 'part_image_stream_server.py']
elif src == 'c':
    p_cmd1 += ['part_image_stream_server']
if dst == 'p':
    p_cmd2 += ['python', 'decode_image_stream.py']
elif dst == 'c':
    p_cmd2 += ['decode_image_stream']
p_cmd1 += [target_file1, args.symbol_type, args.dim, args.pixel_size, args.space_size, args.port]
p_cmd2 += [target_file2, args.symbol_type, args.dim, part_num_str]
print(' '.join(p_cmd1))
print(' '.join(p_cmd2))
p1 = subprocess.Popen(p_cmd1)
p2 = subprocess.Popen(p_cmd2)

p2.wait()
p1.wait()

assert p1.returncode == 0
assert p2.returncode == 0

with open(target_file2, 'rb') as f:
    target_file_bytes2 = f.read()

assert target_file_bytes1 == target_file_bytes2

os.remove(target_file1)
os.remove(target_file2)

print('pass')
