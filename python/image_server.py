import os
import socket
import base64
import struct

import cv2

def image_to_msg_bytes(image):
    ret, image_array = cv2.imencode('.png', image)
    image_bytes = image_array.tobytes()
    base64_image_bytes = base64.b64encode(image_bytes)
    len_bytes = struct.pack('<Q', len(base64_image_bytes))
    return len_bytes + base64_image_bytes

def start_server(image_dir_path, port):
    imgs = []
    for name in os.listdir(image_dir_path):
        if name.endswith(('.jpg', '.jpeg', '.png')):
            path = os.path.join(image_dir_path, name)
            img = cv2.imread(path)
            imgs.append(img)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('', port))
    s.listen(1)
    conn, addr = s.accept()
    index = 0
    while True:
        msg_bytes = image_to_msg_bytes(imgs[index])
        conn.sendall(msg_bytes)
        index = (index + 1) % len(imgs)

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('image_dir_path', help='image dir path')
    parser.add_argument('port', type=int, help='server port')
    args = parser.parse_args()

    start_server(args.image_dir_path, args.port)
