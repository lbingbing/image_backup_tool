import socket
import struct

class TaskStatusClient:
    def __init__(self, ip, port):
        self.ip = ip
        self.port = port

    def get_task_status(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(5)
            try:
                s.connect((self.ip, self.port))
                len_bytes = bytearray()
                while len(len_bytes) < 8:
                    len_bytes += s.recv(8 - len(len_bytes))
                task_status_byte_len = struct.unpack('<Q', len_bytes)[0]
                task_status_bytes = bytearray()
                if task_status_byte_len:
                    while len(task_status_bytes) < task_status_byte_len:
                        task_status_bytes += s.recv(task_status_byte_len - len(task_status_bytes))
                return task_status_bytes
            except Exception as e:
                return None
