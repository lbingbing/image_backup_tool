import time
import threading
import queue

import image_codec_types
import symbol_codec
import transform_utils
import image_decoder
import image_decode_worker

class App:
    def __init__(self, output_file, symbol_type, dim, part_num, mp, transform):
        self.output_file = output_file
        self.image_decode_worker = image_decode_worker.ImageDecodeWorker(symbol_type, dim)
        self.part_num = part_num
        self.transform = transform
        self.mp = mp

        self.calibration = image_decoder.Calibration()

        self.frame_q = queue.Queue(maxsize=4)
        self.part_q = queue.Queue(maxsize=4)
        self.fetch_image_thread = None
        self.decode_image_threads = []
        self.save_part_thread = None
        self.transform_lock = threading.Lock()
        self.running = [False]
        self.running_lock = threading.Lock()

    def IsRunning(self):
        with self.running_lock:
            return self.running[0]

    def start(self):
        with self.running_lock:
            self.running[0] = True

        def get_transform_fn():
            with self.transform_lock:
                return self.transform.clone()

        self.fetch_image_thread = threading.Thread(target=self.image_decode_worker.fetch_image_worker, args=(self.running, self.running_lock, self.frame_q, 25))
        self.fetch_image_thread.start()
        for i in range(self.mp):
            t = threading.Thread(target=self.image_decode_worker.decode_image_worker, args=(self.part_q, self.frame_q, get_transform_fn, self.calibration))
            t.start()
            self.decode_image_threads.append(t)

        def save_part_progress_cb(save_part_progress):
            s = '{} frames processed, {}/{} parts transferred, fps={:.2f}, done_fps={:.2f}, bps={:.0f}, left_time={:0>2d}d{:0>2d}h{:0>2d}m{:0>2d}s'.format(save_part_progress.frame_num, save_part_progress.done_part_num, save_part_progress.part_num, save_part_progress.fps, save_part_progress.done_fps, save_part_progress.bps, save_part_progress.left_days, save_part_progress.left_hours, save_part_progress.left_minutes, save_part_progress.left_seconds)
            print(s)

        def save_part_complete_cb():
            print('transfer done')

        def save_part_error_cb(msg):
            print('error')
            print(msg)

        self.save_part_thread = threading.Thread(target=self.image_decode_worker.save_part_worker, args=(self.running, self.running_lock, self.part_q, self.output_file, self.part_num, save_part_progress_cb, None, save_part_complete_cb, save_part_error_cb, None, None, None, None))
        self.save_part_thread.start()

    def stop(self):
        with self.running_lock:
            self.running[0] = False
        self.fetch_image_thread.join()
        self.fetch_image_thread = None
        for i in range(self.mp):
            self.frame_q.put(None)
        self.frame_q.join()
        for t in self.decode_image_threads:
            t.join()
        self.decode_image_threads.clear()
        self.part_q.put(None)
        self.part_q.join()
        self.save_part_thread.join()
        self.save_part_thread = None

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('output_file', help='output file')
    parser.add_argument('symbol_type', help='symbol type')
    parser.add_argument('dim', help='dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size')
    parser.add_argument('part_num', type=int, help='part num')
    parser.add_argument('--mp', type=int, default=1, help='multiprocessing')
    transform_utils.add_transform_arguments(parser)
    args = parser.parse_args()

    symbol_type = symbol_codec.parse_symbol_type(args.symbol_type)
    dim = image_codec_types.parse_dim(args.dim)
    transform = transform_utils.get_transform(args)

    app = App(args.output_file, symbol_type, dim, args.part_num, args.mp, transform)
    print('start')
    app.start()
    try:
        while app.IsRunning():
            time.sleep(1)
    except KeyboardInterrupt:
        print('stop')
    app.stop()
