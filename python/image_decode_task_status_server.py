import struct
import threading

import image_codec_types

def parse_task_status_server_port(task_status_server_port_str):
    task_status_server_port = 0
    fail = False
    try:
        task_status_server_port = int(task_status_server_port_str)
    except Exception:
        fail = True
    if fail or task_status_server_port < 8100 | task_status_server_port > 8200:
        raise image_codec_types.InvalidImageCodecArgument('invalid port \'{}\''.format(task_status_server_port_str))
    return task_status_server_port

class TaskStatusServer:
    def __init__(self):
        self.port = 0
        self.worker_thread = None
        self.running = False
        self.need_update_task_status = False
        self.task_bytes = b''
        self.server_start_cb = None
        self.lock = threading.Lock()

    def close(self):
        self.stop()

    def start(self, port, server_start_cb=None):
        self.port = port
        self.running = True
        self.need_update_task_status = True
        self.server_start_cb = server_start_cb
        self.worker_thread = threading.Thread(target=self.worker)
        self.worker_thread.start()

    def stop(self):
        with self.lock:
            if self.running:
                was_running = True
                self.running = False
        if was_running:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                try:
                    s.connect(('127.0.0.1', self.port))
                    server_alive = True
                except Exception as e:
                    server_alive = False
                if server_alive:
                    len_bytes = bytearray()
                    while len(len_bytes) < 8:
                        len_bytes += s.recv(8 - len(len_bytes))
                    task_byte_len = struct.unpack('<Q', len_bytes)[0]
                    task_bytes = bytearray()
                    if task_byte_len:
                        while len(task_bytes) < task_byte_len:
                            task_bytes += s.recv(task_byte_len - len(task_bytes))
            self.worker_thd.join()

    def worker(self):
        try_num = 10
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            for try_id in range(try_num):
                try:
                    s.bind(('', self.port))
                    server_alive = True
                    self.server_start_cb(True)
                    break
                except OSError as e:
                    if try_id == try_num - 1:
                        server_alive = False
                        self.server_start_cb(False)
                    time.sleep(1)
            if server_alive:
                s.listen(1)
                while self.is_running():
                    with self.lock:
                        task_bytes = bytes(self.task_bytes)
                    conn, addr = s.accept()
                    with self.lock:
                        task_bytes = bytes(self.task_bytes)
                    try:
                        with conn:
                            conn.sendall(struct.pack('<Q', len(task_bytes)))
                            conn.sendall(task_bytes)
                    except Exception as e:
                        pass

    def update_task_status(self, task_bytes):
        with self.lock:
            self.task_bytes = bytes(task_bytes)
            self.need_update_task_status = False

    def is_running(self):
        with self.lock:
            return self.running

    def is_need_update_task_status(self):
        with self.lock:
            return self.need_update_task_status
