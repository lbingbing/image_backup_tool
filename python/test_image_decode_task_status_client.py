import argparse

import image_decode_task_status_client

parser = argparse.ArgumentParser()
parser.add_argument('ip', help='ip')
parser.add_argument('port', type=int, help='port')
args = parser.parse_args()

task_status_client = image_decode_task_status_client.TaskStatusClient(args.ip, args.port)
task_status_bytes = task_status_client.get_task_status()
if task_status_bytes is None:
    print('client fail to receive')
else:
    print('client receive {} bytes'.format(len(task_status_bytes)))
