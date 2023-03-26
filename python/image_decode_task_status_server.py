import struct
import time
import threading
import socket
import wsgiref.simple_server
import http.client

import image_codec_types
import server_utils

class TaskStatusServer:
    def __init__(self):
        self.port = None
        self.worker_thread = None
        self.running = False
        self.task_bytes = b''
        self.lock = threading.Lock()

    def start(self, port):
        self.port = port
        self.running = True
        self.worker_thread = threading.Thread(target=self.worker)
        self.worker_thread.start()

    def stop(self):
        with self.lock:
            if self.running:
                was_running = True
                self.running = False
        if was_running:
            self.request_self()
            self.worker_thread.join()

    def worker(self):
        while True:
            with self.lock:
                if not self.running:
                    break
            self.handle_request()

    def get_task_bytes(self):
        with self.lock:
            return bytes(self.task_bytes)

    def handle_request(self):
        raise NotImplementedError()

    def request_self(self):
        raise NotImplementedError()

    def update_task_status(self, task_bytes):
        with self.lock:
            self.task_bytes = bytes(task_bytes)

class TaskStatusTcpServer(TaskStatusServer):
    def handle_request(self):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.bind(('', self.port))
                s.listen(1)
                conn, addr = s.accept()
                task_bytes = self.get_task_bytes()
                with conn:
                    conn.sendall(struct.pack('<Q', len(task_bytes)))
                    conn.sendall(task_bytes)
        except Exception as e:
            pass

    def request_self(self):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect(('127.0.0.1', self.port))
                len_bytes = bytearray()
                while len(len_bytes) < 8:
                    len_bytes += s.recv(8 - len(len_bytes))
                task_byte_len = struct.unpack('<Q', len_bytes)[0]
                task_bytes = bytearray()
                if task_byte_len:
                    while len(task_bytes) < task_byte_len:
                        task_bytes += s.recv(task_byte_len - len(task_bytes))
        except Exception as e:
            pass

class WsgiApp:
    def __init__(self, task_bytes):
        self.task_bytes = task_bytes

    def __call__(self, environ, start_response):
        headers = [
                ('Content-type', 'text/plain'),
                ('Content-Length', str(len(self.task_bytes))),
                ]
        start_response('200 OK', headers)
        return [self.task_bytes]

class TaskStatusHttpServer(TaskStatusServer):
    def handle_request(self):
        try:
            httpd = wsgiref.simple_server.make_server('', self.port, WsgiApp(self.get_task_bytes()))
            httpd.handle_request()
            httpd.server_close()
        except Exception as e:
            pass

    def request_self(self):
        try:
            conn = http.client.HTTPConnection('127.0.0.1', self.port)
            conn.request('GET', '/')
            resp = conn.getresponse()
            resp.read(int(resp.getheader('Content-Length')))
        except Exception as e:
            pass

server_type_to_server_mapping = {
    server_utils.ServerType.TCP: TaskStatusTcpServer,
    server_utils.ServerType.HTTP: TaskStatusHttpServer,
    }

def create_task_status_server(server_type):
    return server_type_to_server_mapping[server_type]()
