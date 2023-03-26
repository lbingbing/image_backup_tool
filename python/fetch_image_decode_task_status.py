import argparse

import server_utils
import image_decode_task_status_client

parser = argparse.ArgumentParser()
parser.add_argument('task_file', help='task file')
parser.add_argument('server_type', help='server type')
parser.add_argument('ip', help='ip')
parser.add_argument('port', type=int, help='port')
args = parser.parse_args()

server_type = server_utils.parse_server_type(args.server_type)
task_status_client = image_decode_task_status_client.create_task_status_client(server_type, args.ip, args.port)
task_bytes = task_status_client.get_task_status()
if task_status_bytes is None:
    print('fail to get task status')
else:
    with open(args.task_file, 'wb') as f:
        f.write(task_bytes)
    print('success')
