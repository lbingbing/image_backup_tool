import os
import shutil
import time
import socket
import base64
import struct
import subprocess
import threading
import queue

import cv2

TMP_DIR_PATH = 'window_snapshot_server.tmp_dir'

def image_to_msg_bytes(image):
    ret, image_array = cv2.imencode('.png', image)
    image_bytes = image_array.tobytes()
    base64_image_bytes = base64.b64encode(image_bytes)
    len_bytes = struct.pack('<Q', len(base64_image_bytes))
    return len_bytes + base64_image_bytes

def img_worker(running, running_lock, q, window_number, worker_id):
    img_path = os.path.join(TMP_DIR_PATH, '{}.jpg'.format(worker_id))
    while True:
        with running_lock:
            if not running[0]:
                break
        res = subprocess.run(['screencapture', '-l', window_number, img_path])
        if res.returncode == 0:
            img = cv2.imread(img_path)
            msg_bytes = image_to_msg_bytes(img)
            q.put(msg_bytes)
        else:
            break

def server_worker(q, window_number, port):
    running = True
    while running:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('', port))
        s.listen(1)
        conn, addr = s.accept()
        while True:
            msg_bytes = q.get()
            q.task_done()
            if msg_bytes is None:
                running = False
                break
            try:
                conn.sendall(msg_bytes)
            except Exception:
                pass

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('window_number', help='window number')
    parser.add_argument('port', type=int, help='server port')
    parser.add_argument('--mp', type=int, default=1, help='multiprocessing')
    args = parser.parse_args()

    if not os.path.isdir(TMP_DIR_PATH):
        os.mkdir(TMP_DIR_PATH)

    running = [True]
    running_lock = threading.Lock()
    q = queue.Queue(maxsize=args.mp*2)
    img_threads = [threading.Thread(target=img_worker, args=(running, running_lock, q, args.window_number, worker_id)) for worker_id in range(args.mp)]
    server_thread = threading.Thread(target=server_worker, args=(q, args.window_number, args.port))
    server_thread.start()
    for t in img_threads:
        t.start()

    try:
        while True:
            print('[{}] serving'.format(time.ctime()))
            time.sleep(2)
    except KeyboardInterrupt:
        with running_lock:
            running[0] = False
        while True:
            try:
                q.get(block=False)
                q.task_done()
            except queue.Empty:
                break
        for t in img_threads:
            t.join()
        q.put(None)
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect(('127.0.0.1', args.port))
                while True:
                    data = s.recv(4096)
                    if not data:
                        break
        except Exception:
            pass
        server_thread.join()
        q.join()

    print('cleaning')
    if os.path.isdir(TMP_DIR_PATH):
        shutil.rmtree(TMP_DIR_PATH)
