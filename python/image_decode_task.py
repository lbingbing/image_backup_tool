import os
import struct

import pixel_codec

class FinalizationProgress:
    def __init__(self):
        self.done_block_num = 0
        self.block_num = 0

def get_part_byte_num(tile_x_num, tile_y_num, tile_x_size, tile_y_size, pixel_type):
    codec = pixel_codec.create_pixel_codec(pixel_type)
    return tile_x_num * tile_y_num * tile_x_size * tile_y_size * codec.bit_num_per_pixel // 8 - codec.meta_byte_num

class Task:
    def __init__(self, path):
        self.path = path
        self.task_path = path + '.task'
        self.blob_path = path + '.blob'
        self.blob_buf = []
        self.finalization_start_cb = None
        self.finalization_progress_cb = None
        self.finalization_complete_cb = None

    def init(self, dim, pixel_type, pixel_size, space_size, part_num):
        self.dim = dim
        self.pixel_type = pixel_type
        self.pixel_size = pixel_size
        self.space_size = space_size
        self.part_num = part_num
        self.done_part_num = 0
        self.task_status_bytes = bytearray([0] * ((part_num + 7) // 8))

    def load(self):
        with open(self.task_path, 'rb') as task_file:
            task_bytes = task_file.read()
        *dim, pixel_type, self.pixel_size, self.space_size, self.part_num, self.done_part_num = struct.unpack('<IIIIIIIII', task_bytes[:36])
        self.dim = tuple(dim)
        self.pixel_type = pixel_codec.PixelType(pixel_type)
        self.task_status_bytes = bytearray(task_bytes[36:])

    def set_finalization_cb(self, finalization_start_cb, finalization_progress_cb, finalization_complete_cb):
        self.finalization_start_cb = finalization_start_cb
        self.finalization_progress_cb = finalization_progress_cb
        self.finalization_complete_cb = finalization_complete_cb

    def allocate_blob(self):
        if os.path.isfile(self.blob_path):
            return True
        with open(self.blob_path, 'wb') as blob_file:
            part_byte_num = get_part_byte_num(*self.dim, self.pixel_type)
            blob_file.truncate(part_byte_num * self.part_num)
        return True

    def flush(self):
        with open(self.blob_path, 'r+b') as blob_file:
            for part_id, part_bytes in self.blob_buf:
                blob_file.seek(part_id * len(part_bytes))
                blob_file.write(part_bytes)
        self.blob_buf = []

        with open(self.task_path, 'wb') as task_file:
            task_info_bytes = struct.pack('<IIIIIIIII', *self.dim, self.pixel_type.value, self.pixel_size, self.space_size, self.part_num, self.done_part_num)
            task_file.write(task_info_bytes)
            task_file.write(self.task_status_bytes)

    def is_part_done(self, part_id):
        byte_index = part_id // 8
        mask = 1 << (part_id % 8)
        return bool(self.task_status_bytes[byte_index] & mask)

    def update_part(self, part_id, part_bytes):
        if self.is_part_done(part_id):
            return

        byte_index = part_id // 8
        mask = 1 << (part_id % 8)
        self.task_status_bytes[byte_index] |= mask
        self.done_part_num += 1

        self.blob_buf.append((part_id, part_bytes))

        if (self.done_part_num & 0xff) == 0:
            self.flush()

    def is_done(self):
        return self.done_part_num == self.part_num

    def finalize(self):
        self.flush()
        with open(self.blob_path, 'r+b') as blob_file:
            part_byte_num = get_part_byte_num(*self.dim, self.pixel_type)
            blob_file.seek(part_byte_num * (self.part_num - 1), io.SEEK_SET);
            file_size_bytes = blob_file.read(8)
            file_size = struct.unpack('<Q', file_size_bytes)[0]
            blob_file.truncate(file_size)
        os.rename(self.blob_path, self.path)
        os.remove(self.task_path)
        if self.finalization_complete_cb:
            self.finalization_complete_cb()

    def print(self, show_undone_part_num):
        print('dim={}'.format(self.dim))
        print('pixel_type={}'.format(self.pixel_type.name))
        print('pixel_size={}'.format(self.pixel_size))
        print('space_size={}'.format(self.space_size))
        print('part_num={}'.format(self.part_num))
        print('done_part_num={}'.format(self.done_part_num))
        print('undone parts:')
        if show_undone_part_num > 0:
            for part_id in range(self.part_num):
                if not self.is_part_done(part_id):
                    print(part_id)
                    show_undone_part_num -= 1
                    if show_undone_part_num == 0:
                        break

def get_task_bytes(file_path, part_byte_num):
    with open(file_path, 'rb') as f:
        file_bytes = bytearray(f.read())
    file_size = len(file_bytes)
    left_bytes_num1 = file_size % part_byte_num
    padding_bytes1 = bytes([0] * (part_byte_num - left_bytes_num1)) if left_bytes_num1 else b''
    size_bytes = bytearray(struct.pack('<Q', file_size))
    padding_bytes2 = bytes([0] * (part_byte_num - len(size_bytes)))
    raw_bytes = file_bytes + padding_bytes1 + size_bytes + padding_bytes2
    part_num = len(raw_bytes) // part_byte_num
    return raw_bytes, part_num

def is_part_done(task_status_bytes, part_id):
    byte_index = part_id // 8
    mask = 1 << (part_id % 8)
    return bool(task_status_bytes[byte_index] & mask)
