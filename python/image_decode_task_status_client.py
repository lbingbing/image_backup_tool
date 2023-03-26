import struct
import socket
import http.client

import server_utils

class TaskStatusClient:
    def __init__(self, ip, port):
        self.ip = ip
        self.port = port

    def get_task_status(self):
        raise NotImplementedError()

class TaskStatusTcpClient(TaskStatusClient):
    def __init__(self, ip, port):
        super().__init__(ip, port)

    def get_task_status(self):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(5)
                s.connect((self.ip, self.port))
                len_bytes = bytearray()
                while len(len_bytes) < 8:
                    len_bytes += s.recv(8 - len(len_bytes))
                task_byte_len = struct.unpack('<Q', len_bytes)[0]
                task_bytes = bytearray()
                if task_byte_len:
                    while len(task_bytes) < task_byte_len:
                        task_bytes += s.recv(task_byte_len - len(task_bytes))
                return task_bytes
        except Exception as e:
            return None

class TaskStatusHttpClient(TaskStatusClient):
    def __init__(self, ip, port):
        super().__init__(ip, port)

    def get_task_status(self):
        try:
            conn = http.client.HTTPConnection(self.ip, self.port)
            conn.request('GET', '/')
            resp = conn.getresponse()
            task_bytes = resp.read(int(resp.getheader('Content-Length')))
            return task_bytes
        except Exception as e:
            return None

server_type_to_client_mapping = {
    server_utils.ServerType.TCP: TaskStatusTcpClient,
    server_utils.ServerType.HTTP: TaskStatusHttpClient,
    }

def create_task_status_client(server_type, ip, port):
    return server_type_to_client_mapping[server_type](ip, port)
