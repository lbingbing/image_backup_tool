import struct
import threading

def parse_task_status_server_port(task_status_server_port_str):
    task_status_server_port = 0
    fail = False
    try:
        task_status_server_port = int(task_status_server_port_str)
    except Exception:
        fail = True
    if fail or task_status_server_port < 8100 | task_status_server_port > 8200:
        raise InvalidImageCodecArgument('invalid port \'{}\''.format(task_status_server_port_str))
    return task_status_server_port

class TaskStatusServer:
    def __init__(self):
        self.port = 0
        self.worker_thread = None
        self.running = False
        self.need_update_task_status = False
        self.task_status_bytes = b''
        self.lock = threading.Lock()

    def close(self):
        self.stop()

    def stop(self):
        with self.lock:
            if self.running:
                was_running = True
                self.running = False
        if was_running:
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.connect(('127.0.0.1', self.port))
                    task_status_byte_len_bytes = 0
                    while len(task_status_byte_len_bytes) < 8:
                        task_status_byte_len_bytes += s.recv(8 - len(task_status_byte_len_bytes))
                    task_status_byte_len = struct.unpack('<Q', task_status_byte_len_bytes)[0]
                    if task_status_byte_len:
                        task_status_bytes = bytearray()
                        while len(task_status_bytes) < task_status_byte_len:
                            task_status_bytes += s.recv(task_status_byte_len - len(task_status_bytes))
            except Exception:
                pass
            self.worker_thread.join()
            self.worker_thread = None

    def start(self, port):
        self.port = port
        self.running = True
        self.need_update_task_status = True
        self.worker_thread = threading.Thread(target=self.worker)
        self.worker_thread.start()

    def worker(self):
        while True:
            with self.lock:
                if not self.running:
                    break
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                    s.bind(('', port))
                    s.listen(1)
                    conn, addr = s.accept()
                    task_status_bytes = b''
                    with self.lock:
                        task_status_bytes = bytes(self.task_status_bytes)
                    task_status_byte_len = len(task_status_bytes)
                    task_status_byte_len_bytes = struct.pack('<Q', task_status_byte_len)
                    conn.sendall(task_status_byte_len_bytes)
                    if task_status_byte_len:
                        conn.sendall(task_status_bytes)
            except Exception:
                pass

    def update_task_status(self, task_status_bytes):
        with self.lock:
            self.task_status_bytes = bytes(task_status_bytes)
            self.need_update_task_status = False
