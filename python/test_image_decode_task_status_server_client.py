import argparse
import sys
import time

import server_utils
import image_decode_task_status_server
import image_decode_task_status_client

parser = argparse.ArgumentParser()
parser.add_argument('server_type', help='server type')
parser.add_argument('port', type=int, help='port')
parser.add_argument('task_byte_num', type=int, help='task byte num')
args = parser.parse_args()

server_type = server_utils.parse_server_type(args.server_type)

task_bytes1 = bytes([i % 256 for i in range(args.task_byte_num)])

task_status_server = image_decode_task_status_server.create_task_status_server(server_type)
task_status_server.update_task_status(task_bytes1)
task_status_server.start(args.port)

task_status_client = image_decode_task_status_client.create_task_status_client(server_type, '127.0.0.1', args.port)
success = False
for i in range(3):
    task_bytes2 = task_status_client.get_task_status()
    if task_bytes1 == task_bytes2:
        success = True
        break
    else:
        time.sleep(1)

task_status_server.stop()

if success:
    print('pass')
    sys.exit(0)
else:
    print('fail')
    sys.exit(1)
