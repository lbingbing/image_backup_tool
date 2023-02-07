import curses
import os
import enum
import time
import threading

import image_codec_types
import symbol_codec
import image_decode_task

PIXEL_WIDTH = 4
PIXEL_HEIGHT = 2
INTERVAL = 50

COLOR_STRS = [
    '    ',
    '    ',
    ]

CALIBRATION_STRS = [
    ' \u2584\u2584 ',
    ' \u2580\u2580 ',
    ]

COLOR_WHITE = 10
COLOR_BLACK = 11
COLOR_RED = 12
COLOR_BLUE = 13
COLOR_GREEN = 14
COLOR_CYAN = 15
COLOR_MAGENTA = 16
COLOR_YELLOW = 17
TEXT_COLOR_PAIR = 10
CALIBRATION_COLOR_PAIR = 11
WHITE_COLOR_PAIR = 12
BLACK_COLOR_PAIR = 13
RED_COLOR_PAIR = 14
BLUE_COLOR_PAIR = 15
GREEN_COLOR_PAIR = 16
CYAN_COLOR_PAIR = 17
MAGENTA_COLOR_PAIR = 18
YELLOW_COLOR_PAIR = 19

class Context:
    def __init__(self):
        pass

@enum.unique
class State(enum.Enum):
    INFO = 0
    CALIBRATE = 1
    DISPLAY_MANUAL = 2
    DISPLAY_AUTO = 3

class App:
    def __init__(self, stdscr, target_file_path, symbol_type, dim):
        self.stdscr = stdscr
        self.target_file_path = target_file_path
        self.symbol_type = symbol_codec.parse_symbol_type(symbol_type)
        self.dim = dim
        self.tile_x_num, self.tile_y_num, self.tile_x_size, self.tile_y_size = self.dim
        self.state = State.INFO
        self.task_status_auto_update = False
        self.task_status_auto_update_threshold = 200
        self.undone_part_ids = None
        self.cur_undone_part_id_index = None
        self.cur_part_id = None
        self.auto_display_thread = None
        self.lock = threading.Lock()

        self.auto_navigate_update_fps_interval = int(2000 / INTERVAL)
        self.auto_navigate_fps = 0

        self.symbol_codec = symbol_codec.create_symbol_codec(self.symbol_type)
        self.part_byte_num = image_decode_task.get_part_byte_num(self.symbol_type, self.dim)
        assert self.part_byte_num >= image_decode_task.Task.min_part_byte_num
        self.raw_bytes, self.part_num = image_decode_task.get_task_bytes(self.target_file_path, self.part_byte_num)
        self.cur_part_id = 0

        self.show_info()

    def main_loop(self):
        while True:
            key = self.stdscr.getkey()
            if self.state == State.INFO:
                if key == 'q':
                    break
                elif key == 'c':
                    self.state = State.CALIBRATE
                    self.draw_calibration()
                elif key == 'd':
                    self.state = State.DISPLAY_MANUAL
                    self.draw_part(self.cur_part_id)
                elif key == 't':
                    self.update_task_status()
                    self.show_info()
            elif self.state == State.CALIBRATE:
                if key == 'q':
                    break
                elif key == 'i':
                    self.state = State.INFO
                    self.show_info()
                elif key == 'd':
                    self.state = State.DISPLAY_MANUAL
                    self.draw_part(self.cur_part_id)
            elif self.state == State.DISPLAY_MANUAL:
                if key == 'q':
                    break
                elif key == 'i':
                    self.state = State.INFO
                    self.show_info()
                elif key == 'c':
                    self.state = State.CALIBRATE
                    self.draw_calibration()
                elif key == 'z':
                    self.navigate_prev_part()
                elif key == 'x':
                    self.navigate_next_part()
                elif key == ' ':
                    self.start_auto_display()
            elif self.state == State.DISPLAY_AUTO:
                if key == ' ':
                    self.stop_auto_display()
            else:
                assert 0

    def show_info(self):
        self.stdscr.clear()
        self.draw_background()
        info_s = 'window size {}x{}'.format(curses.COLS, curses.LINES)
        info_s += '\npart_id {}/{}, undone_part_id_num {}'.format(self.cur_part_id, self.part_num - 1, len(self.undone_part_ids) if self.undone_part_ids else self.part_num)
        info_s += '\nfps {:.2f}'.format(self.auto_navigate_fps)
        lines = info_s.split('\n')
        char_lines = [line[i:i+curses.COLS] for line in lines for i in range(0, len(line), curses.COLS)]
        for line_id, line in enumerate(char_lines):
            self.stdscr.addstr(line_id, 0, line, curses.color_pair(TEXT_COLOR_PAIR))
        self.stdscr.refresh()

    def draw_calibration(self):
        self.draw(is_calibration=True)

    def draw_part(self, part_id):
        part_bytes = bytes(self.raw_bytes[part_id*self.part_byte_num:(part_id+1)*self.part_byte_num])
        symbols = self.symbol_codec.encode(part_id, part_bytes, self.tile_x_num * self.tile_y_num * self.tile_x_size * self.tile_y_size)
        data = [[[[symbols[((tile_y_id * self.tile_x_num + tile_x_id) * self.tile_y_size + y) * self.tile_x_size + x]
            for x in range(self.tile_x_size)]
            for y in range(self.tile_y_size)]
            for tile_x_id in range(self.tile_x_num)]
            for tile_y_id in range(self.tile_y_num)]
        self.draw(is_calibration=False, data=data)

    def is_tile_border(self, x, y):
        return y == 0 or y == self.tile_y_size + 1 or x == 0 or x == self.tile_x_size + 1

    def draw(self, is_calibration, data=None):
        self.stdscr.clear()
        self.draw_background()
        x_offset = (curses.COLS - (self.tile_x_num * (self.tile_x_size + 2) + self.tile_x_num - 1) * PIXEL_WIDTH) // 2 // PIXEL_WIDTH
        y_offset = (curses.LINES - (self.tile_y_num * (self.tile_y_size + 2) + self.tile_y_num - 1) * PIXEL_HEIGHT) // 2 // PIXEL_HEIGHT
        for tile_y_id in range(self.tile_y_num):
            tile_y = tile_y_id * (self.tile_y_size + 2 + 1) + y_offset
            for tile_x_id in range(self.tile_x_num):
                tile_x = tile_x_id * (self.tile_x_size + 2 + 1) + x_offset
                for y in range(self.tile_y_size + 2):
                    for x in range(self.tile_x_size + 2):
                        if is_calibration:
                            if self.is_tile_border(x, y):
                                self.draw_color_pixel(tile_x+x, tile_y+y, WHITE_COLOR_PAIR)
                            else:
                                self.draw_calibration_pixel(tile_x+x, tile_y+y)
                        else:
                            if self.is_tile_border(x, y):
                                self.draw_color_pixel(tile_x+x, tile_y+y, BLACK_COLOR_PAIR)
                            else:
                                symbol = data[tile_y_id][tile_x_id][y-1][x-1]
                                if symbol == image_codec_types.PixelColor.WHITE.value:
                                    color_pair = WHITE_COLOR_PAIR
                                elif symbol == image_codec_types.PixelColor.BLACK.value:
                                    color_pair = BLACK_COLOR_PAIR
                                elif symbol == image_codec_types.PixelColor.RED.value:
                                    color_pair = RED_COLOR_PAIR
                                elif symbol == image_codec_types.PixelColor.BLUE.value:
                                    color_pair = BLUE_COLOR_PAIR
                                elif symbol == image_codec_types.PixelColor.GREEN.value:
                                    color_pair = GREEN_COLOR_PAIR
                                elif symbol == image_codec_types.PixelColor.CYAN.value:
                                    color_pair = CYAN_COLOR_PAIR
                                elif symbol == image_codec_types.PixelColor.MAGENTA.value:
                                    color_pair = MAGENTA_COLOR_PAIR
                                elif symbol == image_codec_types.PixelColor.YELLOW.value:
                                    color_pair = YELLOW_COLOR_PAIR
                                else:
                                    assert 0, 'invalid symbol \'{}\''.format(symbol)
                                self.draw_color_pixel(tile_x+x, tile_y+y, color_pair)
        self.stdscr.refresh()

    def draw_background(self):
        for y in range(curses.LINES-1):
            self.stdscr.addstr(y, 0, ' '*curses.COLS, curses.color_pair(WHITE_COLOR_PAIR))

    def draw_calibration_pixel(self, x, y):
        for i in range(PIXEL_HEIGHT):
            self.stdscr.addstr(y*PIXEL_HEIGHT+i, x*PIXEL_WIDTH, CALIBRATION_STRS[i], curses.color_pair(CALIBRATION_COLOR_PAIR))

    def draw_color_pixel(self, x, y, color_pair):
        for i in range(PIXEL_HEIGHT):
            self.stdscr.addstr(y*PIXEL_HEIGHT+i, x*PIXEL_WIDTH, COLOR_STRS[i], curses.color_pair(color_pair))

    def navigate_next_part(self):
        if self.undone_part_ids:
            need_normal_navigate = True
            if self.task_status_auto_update and len(self.undone_part_ids) > self.task_status_auto_update_threshold and self.cur_undone_part_id_index == len(self.undone_part_ids) - 1:
                need_normal_navigate = not self.update_task_status()
            if need_normal_navigate:
                self.cur_undone_part_id_index = self.cur_undone_part_id_index + 1 if self.cur_undone_part_id_index < len(self.undone_part_ids) - 1 else 0
                self.cur_part_id = self.undone_part_ids[self.cur_undone_part_id_index]
        else:
            need_normal_navigate = True
            if self.task_status_auto_update and self.part_num > self.task_status_auto_update_threshold and self.cur_part_id == self.part_num - 1:
                need_normal_navigate = not self.update_task_status()
            if need_normal_navigate:
                self.cur_part_id = self.cur_part_id + 1 if self.cur_part_id < self.part_num - 1 else 0
        self.draw_part(self.cur_part_id)

    def navigate_prev_part(self):
        if self.undone_part_ids:
            self.cur_undone_part_id_index = self.cur_undone_part_id_index - 1 if self.cur_undone_part_id_index > 0 else len(self.undone_part_ids) - 1
            self.cur_part_id = self.undone_part_ids[self.cur_undone_part_id_index]
        else:
            self.cur_part_id = self.cur_part_id - 1 if self.cur_part_id > 0 else self.part_num - 1
        self.draw_part(self.cur_part_id)

    def start_auto_display(self):
        self.state = State.DISPLAY_AUTO
        self.auto_display_thread = threading.Thread(target=self.auto_display_worker)
        self.auto_display_thread.setDaemon(True)
        self.auto_display_thread.start()

    def stop_auto_display(self):
        if self.state == State.DISPLAY_AUTO:
            with self.lock:
                self.state = State.DISPLAY_MANUAL
            self.auto_display_thread.join()

    def auto_display_worker(self):
        auto_navigate_frame_num = 0
        auto_navigate_time = time.time()
        while True:
            with self.lock:
                if self.state != State.DISPLAY_AUTO:
                    break
            self.navigate_next_part()
            auto_navigate_frame_num += 1
            if auto_navigate_frame_num == self.auto_navigate_update_fps_interval:
                t = time.time()
                delta_t = t - auto_navigate_time
                auto_navigate_time = t
                fps = auto_navigate_frame_num / delta_t
                self.auto_navigate_fps = self.auto_navigate_fps * 0.5 + fps * 0.5
                auto_navigate_frame_num = 0
            time.sleep(INTERVAL / 1000)

    def update_task_status(self):
        task_status_file_path = self.target_file_path + '.task'
        if os.path.isfile(task_status_file_path):
            with open(task_status_file_path, 'rb') as f:
                task_bytes = f.read()
            if task_bytes:
                symbol_type, dim, part_num, done_part_num, task_status_bytes = image_decode_task.from_task_bytes(task_bytes)
                assert symbol_type == self.symbol_type
                assert dim == (self.tile_x_num, self.tile_y_num, self.tile_x_size, self.tile_y_size)
                assert part_num == self.part_num
                assert len(task_status_bytes) == (self.part_num + 7) // 8
                self.undone_part_ids = [part_id for part_id in range(self.part_num) if not image_decode_task.is_part_done(task_status_bytes, part_id)]
                assert self.undone_part_ids
                self.cur_undone_part_id_index = 0
                self.cur_part_id = self.undone_part_ids[self.cur_undone_part_id_index]
                return True
            else:
                return False
        else:
            return False

def main(stdscr, target_file_path, symbol_type, dim):
    tile_x_num, tile_y_num, tile_x_size, tile_y_size = dim
    assert curses.can_change_color(), 'color not supported'
    required_cols = PIXEL_WIDTH * (tile_x_num * (tile_x_size + 2) + (tile_x_num - 1))
    required_lines = PIXEL_HEIGHT * (tile_y_num * (tile_y_size + 2) + (tile_y_num - 1))
    assert curses.COLS >= required_cols, '{} cols required instead of {}'.format(required_cols, curses.COLS)
    assert curses.LINES >= required_lines, '{} lines required instead of {}'.format(required_lines, curses.LINES)

    curses.curs_set(0)
    curses.init_color(COLOR_BLACK, 0, 0, 0)
    curses.init_color(COLOR_WHITE, 1000, 1000, 1000)
    curses.init_color(COLOR_RED, 1000, 0, 0)
    curses.init_color(COLOR_BLUE, 0, 0, 1000)
    curses.init_color(COLOR_GREEN, 0, 1000, 0)
    curses.init_color(COLOR_CYAN, 0, 1000, 1000)
    curses.init_color(COLOR_MAGENTA, 1000, 0, 1000)
    curses.init_color(COLOR_YELLOW, 1000, 1000, 0)
    curses.init_pair(TEXT_COLOR_PAIR, COLOR_BLACK, COLOR_WHITE)
    curses.init_pair(CALIBRATION_COLOR_PAIR, COLOR_BLACK, COLOR_WHITE)
    curses.init_pair(WHITE_COLOR_PAIR, COLOR_WHITE, COLOR_WHITE)
    curses.init_pair(BLACK_COLOR_PAIR, COLOR_WHITE, COLOR_BLACK)
    curses.init_pair(RED_COLOR_PAIR, COLOR_WHITE, COLOR_RED)
    curses.init_pair(BLUE_COLOR_PAIR, COLOR_WHITE, COLOR_BLUE)
    curses.init_pair(GREEN_COLOR_PAIR, COLOR_WHITE, COLOR_GREEN)
    curses.init_pair(CYAN_COLOR_PAIR, COLOR_WHITE, COLOR_CYAN)
    curses.init_pair(MAGENTA_COLOR_PAIR, COLOR_WHITE, COLOR_MAGENTA)
    curses.init_pair(YELLOW_COLOR_PAIR, COLOR_WHITE, COLOR_YELLOW)

    app = App(stdscr, target_file_path, symbol_type, dim)
    app.main_loop()

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('target_file_path', help='target file_path')
    parser.add_argument('symbol_type', help='symbol type')
    parser.add_argument('dim', help='dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size')
    args = parser.parse_args()

    dim = image_codec_types.parse_dim(args.dim)
    curses.wrapper(main, args.target_file_path, args.symbol_type, dim)
