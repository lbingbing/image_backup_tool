import os
import re
import struct
import json
import socket
from contextlib import contextmanager
import wsgiref.simple_server
import wsgiref.util
import mimetypes

import pixel_codec
import image_decode_task

class App:
    def __init__(self, cfg):
        self.pixel_size = cfg['pixel_size']
        self.row_num = cfg['row_num']
        self.col_num = cfg['col_num']
        self.part_byte_num = cfg['part_byte_num']
        self.interval = cfg['interval']
        self.auto_display = cfg['auto_display']

        self.raw_bytes, self.part_num = image_decode_task.get_task_bytes(cfg['file_path'], self.part_byte_num)

    def is_init_request(self, path):
        return path.startswith('init')

    def is_data_request(self, path):
        return path.startswith('data?')

    def get_data_request_query_str(self, path):
        return path[len('data?'):]

    def get_data(self, query_str):
        m = re.match(r'^part_id=(\d+)$', query_str)
        part_id = int(m.group(1))
        part_bytes = self.raw_bytes[part_id*self.part_byte_num:(part_id+1)*self.part_byte_num]
        bits = pixel_codec.encode(part_id, part_bytes, self.row_num * self.col_num)
        rows = [bits[i*self.col_num:(i+1)*self.col_num] for i in range(self.row_num)]
        return json.dumps(rows).encode(encoding='utf-8')

    def __call__(self, environ, start_response):
        path = environ['PATH_INFO'][1:]
        if environ['QUERY_STRING']:
            path += '?' + environ['QUERY_STRING']
        if self.is_init_request(path):
            data = {
                'row_num': row_num,
                'col_num': col_num,
                'pixel_size': self.pixel_size,
                'part_num': self.part_num,
                'interval': self.interval,
                'auto_display': self.auto_display,
                }
            data = json.dumps(data).encode(encoding='utf-8')
            headers = [
                ('Content-type', 'application/json'),
                ('Content-Length', str(len(data))),
                ]
            start_response('200 OK', headers)
            return [data]
        elif self.is_data_request(path):
            data = self.get_data(self.get_data_request_query_str(path))
            headers = [
                ('Content-type', 'application/json'),
                ('Content-Length', str(len(data))),
                ]
            start_response('200 OK', headers)
            return [data]
        else:
            f = open('display_server.html', 'rb')
            mime_type = mimetypes.guess_type(path)[0]
            start_response('200 OK', [('Content-type', mime_type), ('Content-Length', str(os.fstat(f.fileno()).st_size))])
            return wsgiref.util.FileWrapper(f)
        else:
            start_response('404 Not Found', [])
            return []

@contextmanager
def suppress_stdout_stderr():
    null_fds = [os.open(os.devnull, os.O_RDWR) for x in range(2)]
    save_fds = [os.dup(1), os.dup(2)]
    os.dup2(null_fds[0], 1)
    os.dup2(null_fds[1], 2)
    try:
        yield
    finally:
        os.dup2(save_fds[0], 1)
        os.dup2(save_fds[1], 2)
        for fd in null_fds + save_fds:
            os.close(fd)

def start_server(port, app_cfg, verbose=False):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.connect(('1.1.1.1', 80))
        ip = s.getsockname()[0]
    print('Serving Display on {}:{}'.format(ip, port))
    app = App(app_cfg)
    httpd = wsgiref.simple_server.make_server('', port, app)
    try:
        if verbose:
            httpd.serve_forever()
        else:
            with suppress_stdout_stderr():
                httpd.serve_forever()
    except KeyboardInterrupt:
        print('\nKeyboard interrupt received, exiting.')
        httpd.server_close()

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('file_path', help='target file')
    parser.add_argument('dim', help='dim as row_num,col_num')
    parser.add_argument('port', type=int, help='http server port')
    parser.add_argument('--pixel_size', type=int, default=16, help='element size')
    parser.add_argument('--interval', type=int, default=250, help='display interval')
    parser.add_argument('--auto_display', action='store_true', help='auto display')
    parser.add_argument('--verbose', action='store_true', help='verbose output')
    args = parser.parse_args()

    row_num, col_num = map(int, args.dim.split(','))
    part_byte_num = row_num * col_num * pixel_codec.bit_num_per_pixel // 8 - pixel_codec.meta_byte_num
    assert part_byte_num > 0

    app_cfg = {
        'file_path': args.file_path,
        'row_num': row_num,
        'col_num': col_num,
        'pixel_size': args.pixel_size,
        'part_byte_num': part_byte_num,
        'interval': args.interval,
        'auto_display': args.auto_display,
        }
    start_server(args.port, app_cfg, args.verbose)

