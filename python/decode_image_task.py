import os
import struct

import pixel_codec

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
        *self.dim, pixel_type, self.pixel_size, self.space_size, self.part_num, self.done_part_num = struct.unpack('<IIIIIIIII', task_bytes[:36])
        self.pixel_type = pixel_codec.PixelType(pixel_type)
        self.task_status_bytes = bytearray(task_bytes[36:])

    def set_finalization_cb(self, finalization_start_cb, finalization_progress_cb, finalization_complete_cb):
        self.finalization_start_cb = finalization_start_cb
        self.finalization_progress_cb = finalization_progress_cb
        self.finalization_complete_cb = finalization_complete_cb

    def flush(self):
        mode = 'r+b' if os.path.isfile(self.blob_path) else 'w+b'
        with open(self.blob_path, mode) as blob_file:
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
            blob_file.seek(0, io.SEEK_SET)
            file_size_bytes = blob_file.read(8)
            file_size = struct.unpack('<Q', file_size_bytes)[0]
            block_size = 1024 * 1024
            block_num = (file_size + block_size - 1) // block_size
            block_index = 0
            if self.finalization_start_cb:
                self.finalization_start_cb(0, block_num)
            while True:
                offset = block_size * block_index
                blob_file.seek(8 + offset, io.SEEK_SET)
                block = blob_file.read(block_size)
                if not block:
                    break
                blob_file.seek(offset, io.SEEK_SET)
                blob_file.write(block)
                block_index += 1
                if self.finalization_progress_cb:
                    self.finalization_progress_cb(block_index, block_num)
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

def is_part_done(task_status_bytes, part_id):
    byte_index = part_id // 8
    mask = 1 << (part_id % 8)
    return bool(task_status_bytes[byte_index] & mask)
