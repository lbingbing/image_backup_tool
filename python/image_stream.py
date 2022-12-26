import sys
import re
import struct
import base64
import threading

import cv2

class ImageStream:
    def get_frame(self):
        raise NotImplementedError()

class CameraImageStream(ImageStream):
    def __init__(self, url, scale):
        if not url.startswith('http'):
            url = int(url)
        self.cap = cv2.VideoCapture(url)
        if self.cap.isOpened():
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.cap.get(cv2.CAP_PROP_FRAME_WIDTH) * scale)
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT) * scale)

    def close(self):
        if self.cap.isOpened():
            self.cap.release()

    def get_frame(self):
        success, frame = self.cap.read()
        return frame

class ThreadedImageStream(ImageStream):
    def __init__(self, buffer_size):
        self.ring_buffer = [None for i in range(buffer_size)]
        self.size = 0
        self.wptr = 0
        self.rptr = 0
        self.worker_thread = None
        self.lock = threading.Lock()
        self.cv = threading.Condition(self.lock)
        self.stop = False

    def close():
        with self.lock:
            stop = self.stop
        if not stop:
            self.join()

    def get_frame(self):
        with self.cv:
            self.cv.wait_for(lambda: self.stop or self.size)
            frame = None
            if self.size:
                frame = self.ring_buffer[self.rptr]
                self.rptr = (self.rptr + 1) % len(self.ring_buffer)
                self.size -= 1
        return frame

    def start(self):
        self.worker_thread = threading.Thread(target=self.worker)
        self.worker_thread.start()

    def stop(self):
        with self.lock:
            self.stop = True

    def join(self):
        self.stop()
        self.worker_thread.join()
        self.worker_thread = None

    def fetch_data(self):
        raise NotImplementedError()

    def worker(self):
        while True:
            with self.lock:
                if self.stop:
                    break
            data = self.fetch_data()
            frame = None
            if data:
                frame = cv2.imdecode(data, cv2.IMREAD_COLOR)
            with self.cv:
                self.ring_buffer[self.wptr] = frame
                self.wptr = (self.wptr + 1) % len(self.ring_buffer)
                if self.size == len(self.ring_buffer):
                    self.rptr = (self.rptr + 1) % len(self.ring_buffer)
                else:
                    self.size += 1
                self.cv.notify(1)

class PipeImageStream(ThreadedImageStream):
    def __init__(self, buffer_size):
        super().__init__(buffer_size)

        self.start()

    def fetch_data(self):
        try:
            length_bytes = bytearray()
            while len(length_bytes) < 8:
                length_bytes += sys.stdin.read(8 - len(length_bytes))
            length = struct.unpack('<Q', length_bytes)[0]
            data = bytearray()
            while len(data) < length:
                data += sys.stdin.read(length - len(data))
            return base64.b64decode(data)
        except Exception:
            return b''

class SocketImageStream(ThreadedImageStream):
    def __init__(self, addr, port, buffer_size):
        super().__init__(buffer_size)

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.sock.connect((addr, port))
        except Exception:
            self.stop()
        if self.stop:
            self.start()

    def fetch_data(self):
        try:
            length_bytes = bytearray()
            while len(length_bytes) < 8:
                length_bytes += self.sock.recv(8 - len(length_bytes))
            data = bytearray()
            while len(data) < length:
                data += self.sock.recv(length - len(data))
            return base64.b64decode(data)
        except Exception:
            return b''

def create_image_stream():
    stream_type = 'camera'
    camera_url = 0
    scale = 1
    buffer_size = 128
    ip = '127.0.0.1'
    port = 8123
    type_pattern = r'^type=(\S+)$'
    camera_pattern = r'^camera=(\S+)$'
    scale_pattern = r'^scale=(\S+)$'
    buffer_size_pattern =r'^buffer_size=(\d+)$'
    ip_pattern = r'^ip=(\S+)$'
    port_pattern = r'^port=(\d+)$'
    blank_pattern = r'^\s*(#.*)?$'
    with open('image_stream.ini', encoding='utf-8') as f:
        for line in f:
            type_m = re.match(type_pattern, line)
            camera_m = re.match(camera_pattern, line)
            scale_m = re.match(scale_pattern, line)
            buffer_size_m = re.match(buffer_size_pattern, line)
            ip_m = re.match(ip_pattern, line)
            port_m = re.match(port_pattern, line)
            blank_m = re.match(blank_pattern, line)
            if type_m:
                stream_type = type_m.group(1)
            elif camera_m:
                camera_url = camera_m.group(1)
            elif scale_m:
                scale = int(scale_m.group(1))
            elif buffer_size_m:
                buffer_size = int(buffer_size_m.group(1))
            elif ip_m:
                ip = ip_m.group(1)
            elif port_m:
                port = int(port_m.group(1))
            elif blank_m:
                pass
            else:
                assert 0, 'unsupported config \'{}\''.format(line)
    if stream_type == 'camera':
        image_stream = CameraImageStream(camera_url, scale)
    elif stream_type == 'pipe':
        image_stream = PipeImageStream(buffer_size)
    elif stream_type == 'socket':
        image_stream = SocketImageStream(ip, port, buffer_size)
    else:
        assert 0, 'unsupported type \'{}\''.format(stream_type)
    return image_stream
