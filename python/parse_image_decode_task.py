import argparse

import image_decode_task

parser = argparse.ArgumentParser()
parser.add_argument('file_path', help='file path')
parser.add_argument('show_undone_part_num', type=int, help='show undone part num')
args = parser.parse_args()

task = image_decode_task.Task(args.file_path)
task.load()
task.print(args.show_undone_part_num)
