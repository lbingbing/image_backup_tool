import os
import sys
import io
import multiprocessing
import queue
import cv2
import PIL.Image

import transform_utils
import bit_codec
import decode_image
import merge_parts

def capture_image_worker(frame_q, stop_q):
    url = 1
    #url = 'http://192.168.10.108:8080/video'
    capture = cv2.VideoCapture(url)
    #width, height = capture.get(3), capture.get(4)
    #capture.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    #capture.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    #print(width, height)

    frame_id = 0
    while True:
        ret, frame = capture.read()
        cv2.imshow('frame', frame)
        frame_q.put((frame_id, frame))
        if frame_id % 10 == 0:
            try:
                stop_q.get(False)
                break
            except queue.Empty:
                pass
        frame_id += 1
        key = cv2.waitKey(50)
        if key == ord('q'):
            break

def decode_image_worker(part_q, frame_q, image_dir_path, row_num, col_num, transform):
    while True:
        data = frame_q.get()
        if data is None:
            break
        frame_id, frame = data
        with PIL.Image.fromarray(frame) as img:
            if image_dir_path:
                img.save(os.path.join(image_dir_path, '{}.jpg'.format(frame_id)))
            success, part_id, part_bytes, result_img = decode_image.decode_image(img, row_num, col_num, transform)
        part_q.put((success, part_id, part_bytes))

def save_part_worker(stop_q, part_q, output_file, row_num, col_num, part_num):
    task = decode_image.Task()
    task_file = output_file + '.task'
    if os.path.isfile(task_file):
        task.load(task_file)
        assert row_num == task.row_num, 'inconsistent task'
        assert col_num == task.col_num, 'inconsistent task'
        assert part_num == task.part_num, 'inconsistent task'
    else:
        task.init(row_num, col_num, part_num)

    mode = 'r+b' if os.path.isfile(output_file) else 'w+b'
    with open(output_file, mode) as f:
        frame_num = 0
        while True:
            part = part_q.get()
            if part is None:
                break
            success, part_id, part_bytes = part
            if success and not task.test_part_done(part_id):
                part_bytes = bit_codec.encrypt_part_bytes(part_bytes)
                f.seek(part_id * len(part_bytes), io.SEEK_SET)
                f.write(part_bytes)
                task.set_part_done(part_id)
            frame_num += 1
            print('\r{}/{} parts transferred, {} frames processed'.format(task.done_part_num, task.part_num, frame_num), end='', file=sys.stderr, flush=True)
            if task.done_part_num == task.part_num:
                stop_q.put(None)
        print(file=sys.stderr)

        task.save(task_file)

        if task.done_part_num == task.part_num:
            merge_parts.truncate_file(f)
            os.remove(task_file)
            print('transfer done', file=sys.stderr)

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('output_file', help='output file')
    parser.add_argument('dim', help='dim as row_num,col_num')
    parser.add_argument('part_num', type=int, help='part num')
    parser.add_argument('--image_dir_path', help='image dir path')
    transform_utils.add_transform_arguments(parser)
    parser.add_argument('--mp', type=int, default=1, help='multiprocessing')
    args = parser.parse_args()

    row_num, col_num = decode_image.parse_dim(args.dim)
    transform = transform_utils.get_transform(args)

    frame_q = multiprocessing.Queue(20)
    part_q = multiprocessing.Queue(20)
    stop_q = multiprocessing.Queue(1)

    capture_image_p = multiprocessing.Process(target=capture_image_worker, args=(frame_q, stop_q))
    capture_image_p.start()

    decode_image_ps = []
    for i in range(args.mp):
        decode_image_ps.append(multiprocessing.Process(target=decode_image_worker, args=(part_q, frame_q, args.image_dir_path, row_num, col_num, transform)))
    for e in decode_image_ps:
        e.start()

    save_part_p = multiprocessing.Process(target=save_part_worker, args=(stop_q, part_q, args.output_file, row_num, col_num, args.part_num))
    save_part_p.start()

    capture_image_p.join()
    for i in range(args.mp):
        frame_q.put(None)
    for e in decode_image_ps:
        e.join()
    part_q.put(None)
    save_part_p.join()
