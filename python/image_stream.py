import sys
import re
import struct
import base64
import threading
import socket
import configparser

import numpy as np
import cv2 as cv

import server_utils

class ImageStream:
    def get_frame(self):
        raise NotImplementedError()

class CameraImageStream(ImageStream):
    def __init__(self, url, scale):
        if not url.startswith('http'):
            url = int(url)
        self.cap = cv.VideoCapture(url)
        if self.cap.isOpened():
            self.cap.set(cv.CAP_PROP_FRAME_WIDTH, self.cap.get(cv.CAP_PROP_FRAME_WIDTH) * scale)
            self.cap.set(cv.CAP_PROP_FRAME_HEIGHT, self.cap.get(cv.CAP_PROP_FRAME_HEIGHT) * scale)

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
        self.running = False

    def close(self):
        with self.lock:
            was_running = self.running
            self.running = False
        if was_running:
            self.worker_thread.join()
            self.worker_thread = None

    def get_frame(self):
        with self.cv:
            self.cv.wait_for(lambda: not self.running or self.size)
            frame = None
            if self.size:
                frame = self.ring_buffer[self.rptr]
                self.rptr = (self.rptr + 1) % len(self.ring_buffer)
                self.size -= 1
        return frame

    def start(self):
        self.running = True
        self.worker_thread = threading.Thread(target=self.worker)
        self.worker_thread.start()

    def fetch_data(self):
        raise NotImplementedError()

    def worker(self):
        while True:
            with self.lock:
                if not self.running:
                    break
            data = self.fetch_data()
            frame = None
            if data:
                frame = cv.imdecode(np.frombuffer(data, dtype=np.uint8), cv.IMREAD_COLOR)
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
            self.start()
        except Exception:
            pass

    def fetch_data(self):
        try:
            length_bytes = bytearray()
            while len(length_bytes) < 8:
                length_bytes += self.sock.recv(8 - len(length_bytes))
            length = struct.unpack('<Q', length_bytes)[0]
            data = bytearray()
            while len(data) < length:
                data += self.sock.recv(length - len(data))
            return base64.b64decode(data)
        except Exception:
            return b''

def create_image_stream():
    config = configparser.ConfigParser()
    config.read('image_stream.ini')
    stream_type = config.get('DEFAULT', 'stream_type', fallback='camera')
    camera_url = config.get('DEFAULT', 'camera_url', fallback=0)
    scale = config.getint('DEFAULT', 'scale', fallback=1)
    buffer_size = config.getint('DEFAULT', 'buffer_size', fallback=64)
    server = config.get('DEFAULT', 'server', fallback='127.0.0.1:80')
    ip, port = server_utils.parse_server_addr(server)
    if stream_type == 'camera':
        image_stream = CameraImageStream(camera_url, scale)
    elif stream_type == 'pipe':
        image_stream = PipeImageStream(buffer_size)
    elif stream_type == 'socket':
        image_stream = SocketImageStream(ip, port, buffer_size)
    return image_stream
