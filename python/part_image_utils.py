import os
import time
import base64
import struct
import itertools
import socket

import numpy as np
import cv2 as cv

import image_codec_types
import symbol_codec
import image_decode_task

def prepare_part_images(target_file, symbol_type, dim):
    codec = symbol_codec.create_symbol_codec(symbol_type)
    part_byte_num = image_decode_task.get_part_byte_num(symbol_type, dim)
    assert part_byte_num >= image_decode_task.Task.min_part_byte_num, 'invalid part_byte_num \'{}\''.format(part_byte_num)
    raw_bytes, part_num = image_decode_task.get_task_bytes(target_file, part_byte_num)
    print('{} parts'.format(part_num))
    return codec, part_byte_num, raw_bytes, part_num

def is_tile_border(tile_x_size, tile_y_size, x, y):
    return y == 0 or y == tile_y_size + 1 or x == 0 or x == tile_x_size + 1

def generate_part_image(dim, pixel_size, space_size, codec, part_id, part_bytes):
    tile_x_num, tile_y_num, tile_x_size, tile_y_size = dim
    symbols = codec.encode(part_id, part_bytes, tile_x_num * tile_y_num * tile_x_size * tile_y_size)
    img_h = (tile_y_num * (tile_y_size + 2) + (tile_y_num - 1) * space_size + 2) * pixel_size
    img_w = (tile_x_num * (tile_x_size + 2) + (tile_x_num - 1) * space_size + 2) * pixel_size
    img = np.empty((img_h, img_w, 3), np.uint8)
    cv.rectangle(img, (0, 0, img_w, pixel_size), (255, 255, 255), cv.FILLED)
    cv.rectangle(img, (0, img_h - pixel_size, img_w, pixel_size), (255, 255, 255), cv.FILLED)
    cv.rectangle(img, (0, 0, pixel_size, img_h), (255, 255, 255), cv.FILLED)
    cv.rectangle(img, (img_w - pixel_size, 0, pixel_size, img_h), (255, 255, 255), cv.FILLED)
    for tile_y_id in range(tile_y_num):
        tile_y = (tile_y_id * (tile_y_size + 2 + space_size) + 1) * pixel_size
        for tile_x_id in range(tile_x_num):
            tile_x = (tile_x_id * (tile_x_size + 2 + space_size) + 1) * pixel_size
            for y in range(tile_y_size + 2):
                for x in range(tile_x_size + 2):
                    if is_tile_border(tile_x_size, tile_y_size, x, y):
                        color = (0, 0, 0)
                    else:
                        symbol = symbols[((tile_y_id * tile_x_num + tile_x_id) * tile_y_size + (y - 1)) * tile_x_size + (x - 1)]
                        if symbol == image_codec_types.PixelColor.WHITE.value:
                            color = (255, 255, 255)
                        elif symbol == image_codec_types.PixelColor.BLACK.value:
                            color = (0, 0, 0)
                        elif symbol == image_codec_types.PixelColor.RED.value:
                            color = (0, 0, 255)
                        elif symbol == image_codec_types.PixelColor.BLUE.value:
                            color = (255, 0, 0)
                        elif symbol == image_codec_types.PixelColor.GREEN.value:
                            color = (0, 255, 0)
                        elif symbol == image_codec_types.PixelColor.CYAN.value:
                            color = (255, 255, 0)
                        elif symbol == image_codec_types.PixelColor.MAGENTA.value:
                            color = (255, 0, 255)
                        elif symbol == image_codec_types.PixelColor.YELLOW.value:
                            color = (0, 255, 255)
                        else:
                            assert 0, 'invalid symbol \'{}\''.format(symbol)
                    cv.rectangle(img, (tile_x + x * pixel_size, tile_y + y * pixel_size, pixel_size, pixel_size), color, cv.FILLED)
    return img

def generate_part_images(dim, pixel_size, space_size, codec, part_byte_num, raw_bytes, part_num):
    for part_id in range(part_num):
        part_bytes = bytes(raw_bytes[part_id*part_byte_num:(part_id+1)*part_byte_num])
        img = generate_part_image(dim, pixel_size, space_size, codec, part_id, part_bytes)
        yield part_id, img

def get_part_image_file_name(part_num, part_id):
    img_file_name_fmt = 'part{{:0>{}d}}.png'.format(len(str(part_num - 1)))
    return img_file_name_fmt.format(part_id)

def img_to_msg_bytes(img):
    ret, img_array = cv.imencode('.png', img)
    img_bytes = img_array.tobytes()
    base64_img_bytes = base64.b64encode(img_bytes)
    len_bytes = struct.pack('<Q', len(base64_img_bytes))
    return len_bytes + base64_img_bytes

def start_image_stream_server(gen_images, port):
    print('start server')
    while True:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.bind(('', port))
            break
        except Exception as e:
            time.sleep(1)
            print('retry')
    s.listen(1)
    conn, addr = s.accept()
    for img_file_name, img in itertools.cycle(gen_images):
        msg_bytes = img_to_msg_bytes(img)
        try:
            print(img_file_name)
            conn.sendall(msg_bytes)
        except Exception as e:
            print('client disconnected')
            break
