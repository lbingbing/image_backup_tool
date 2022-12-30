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
                task_byte_len = struct.unpack('<Q', len_bytes)[0]
                task_bytes = bytearray()
                if task_byte_len:
                    while len(task_bytes) < task_byte_len:
                        task_bytes += s.recv(task_byte_len - len(task_bytes))
                return task_bytes
            except Exception as e:
                return None

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('target_file_path', help='target file path')
    parser.add_argument('ip', help='ip')
    parser.add_argument('port', type=int, help='port')
    args = parser.parse_args()

    task_status_client = TaskStatusClient(args.ip, args.port)
    task_bytes = task_status_client.get_task_status()
    with open(args.target_file_path+'.task', 'wb') as f:
        f.write(task_bytes)
