import argparse
import os

parser = argparse.ArgumentParser()
parser.add_argument('image_dir_path', help='image dir path')
parser.add_argument('video_file_path', help='video file path')
args = parser.parse_args()

cmd = 'ffmpeg -i ' + args.video_file_path + ' -r 6 -f image2 ' + args.image_dir_path + '%03d.jpeg'
os.system(cmd)
