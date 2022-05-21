import os
import base64
import struct
import subprocess
import threading
import socket
import ssl
import wsgiref.simple_server
import wsgiref.util
import mimetypes

import numpy as np
import cv2

def base64_image_bytes_to_image(base64_data):
    image_data = base64.b64decode(base64_data)
    image_array = np.frombuffer(image_data, dtype=np.uint8)
    image = cv2.imdecode(image_array, cv2.IMREAD_COLOR)
    image = cv2.rotate(image, cv2.ROTATE_90_COUNTERCLOCKWISE)
    return image

def image_to_msg_bytes(image):
    ret, image_array = cv2.imencode('.png', image)
    image_bytes = image_array.tobytes()
    base64_image_bytes = base64.b64encode(image_bytes)
    len_bytes = struct.pack('<I', len(base64_image_bytes))
    return len_bytes + base64_image_bytes

class RingBuffer:
    def __init__(self, size):
        self.buf = [None] * size
        self.size = 0
        self.wptr = 0
        self.rptr = 0
        self.cv = threading.Condition()
        self.stop = False

    def inc_wptr(self,):
        self.wptr = (self.wptr + 1) % len(self.buf)

    def inc_rptr(self,):
        self.rptr = (self.rptr + 1) % len(self.buf)

    def put(self, item):
        with self.cv:
            self.buf[self.wptr] = item
            self.inc_wptr()
            if self.size == len(self.buf):
                self.inc_rptr()
            else:
                self.size += 1
            self.cv.notify()

    def get(self):
        with self.cv:
            self.cv.wait_for(lambda: self.stop or self.size)
            if self.size:
                data = self.buf[self.rptr]
                self.inc_rptr()
                self.size -= 1
                return data
            else:
                return None

class ImageServer:
    def __init__(self, port, rb):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s.bind(('', port))
        self.s.listen(2)

        self.rb = rb

        self.server_thread = threading.Thread(target=self.server_worker)
        self.server_thread.start()

    def join(self):
        self.server_thread.join()

    def server_worker(self):
        while True:
            conn, addr = self.s.accept()
            print('client connected')
            conn.settimeout(10)
            threading.Thread(target=self.client_worker, args=(conn,)).start()

    def client_worker(self, conn):
        while True:
            try:
                data = self.rb.get()
                if not data:
                    break
                conn.sendall(data)
            except Exception as e:
                print(e)
                break
        print('client disconnected')
        conn.close()

class App:
    def __init__(self, send_cb):
        self.send_cb = send_cb

    def __call__(self, environ, start_response):
        path = environ['PATH_INFO'][1:]
        if path == 'data':
            try:
                request_body_size = int(environ.get('CONTENT_LENGTH', 0))
            except (ValueError):
                request_body_size = 0
            base64_image_bytes = environ['wsgi.input'].read(request_body_size)
            image = base64_image_bytes_to_image(base64_image_bytes)
            msg_bytes = image_to_msg_bytes(image)
            print('bytes length', len(msg_bytes))
            self.send_cb(msg_bytes)
            start_response('200 OK', [])
            return []
        else:
            f = open('camera_server.html', 'rb')
            start_response('200 OK', [('Content-type', 'text/html'), ('Content-Length', str(os.fstat(f.fileno()).st_size))])
            return wsgiref.util.FileWrapper(f)

def start_server(port, process):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.connect(("1.1.1.1", 80))
        ip = s.getsockname()[0]
    print('Serving Camera on {}:{}'.format(ip, port))
    app = App(process)
    httpd = wsgiref.simple_server.make_server('', port, app)
    httpd.socket = ssl.wrap_socket(httpd.socket, keyfile='lbca.key', certfile='lbca.pem', server_side=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nKeyboard interrupt received, exiting.")
        httpd.server_close()

class PipeCb:
    def __init__(self, process):
        self.process = process

    def __call__(self, msg_bytes):
        try:
            self.process.stdin.write(msg_bytes)
            self.process.stdin.flush()
        except Exception as e:
            print(e)

class SocketCb:
    def __init__(self, rb):
        self.rb = rb

    def __call__(self, msg_bytes):
        self.rb.put(msg_bytes)

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('camera_port', type=int, help='camera server port')
    parser.add_argument('--image_port', type=int, help='image server port')
    args = parser.parse_args()

    if args.image_port is None:
        cmd = './test_image_fetcher.exe'
        process = subprocess.Popen(cmd.split(), stdin=subprocess.PIPE)
        start_server(args.camera_port, PipeCb(process))
        try:
            process.stdin.close()
        except Exception as e:
           print(e)
        process.wait()
    else:
        rb = RingBuffer(16)
        server = ImageServer(args.image_port, rb)
        start_server(args.camera_port, SocketCb(rb))
        server.join()

