import os
import time
import itertools

import image_decoder
import image_stream
import image_decode_task
import server_utils
import image_decode_task_status_server

class CalibrationProgress:
    def __init__(self):
        self.frame_num = 0
        self.fps = 0

class SavePartProgress:
    def __init__(self):
        self.frame_num = 0
        self.done_part_num = 0
        self.part_num = 0
        self.fps = 0
        self.done_fps = 0
        self.bps = 0
        self.left_days = 0
        self.left_hours = 0
        self.left_minutes = 0
        self.left_seconds = 0

class ImageDecodeWorker:
    def __init__(self, symbol_type, dim):
        self.image_decoder = image_decoder.ImageDecoder(symbol_type, dim)

    def fetch_image_worker(self, running, running_lock, frame_q, interval):
        frame_id = 0
        while True:
            with running_lock:
                if not running[0]:
                    break
            stream = image_stream.create_image_stream()
            while True:
                with running_lock:
                    if not running[0]:
                        break
                frame = stream.get_frame()
                if frame is None:
                    break
                frame_q.put((frame_id, frame))
                frame_id += 1
                time.sleep(interval / 1000)
            stream.close()

    def calibrate_worker(self, frame_q, get_transform_cb, calibrate_cb, send_calibration_image_result_cb, calibration_progress_cb):
        t0 = time.time()
        frame_num = 0
        fps = 0
        frame_num0 = 0
        while True:
            data = frame_q.get()
            frame_q.task_done()
            if data is None:
                break
            frame_id, frame = data
            frame1, calibration, result_imgs = self.image_decoder.calibrate(frame, get_transform_cb(), True)
            frame_num += 1
            if frame_num & 0x1f == 0:
                t1 = time.time()
                delta_t = t1 - t0
                t0 = t1
                delta_frame_num = frame_num - frame_num0
                frame_num0 = frame_num
                fps1 = delta_frame_num / delta_t
                fps = fps * 0.5 + fps1 * 0.5
            calibrate_cb(calibration)
            send_calibration_image_result_cb(frame1, calibration.valid, result_imgs)
            calibration_progress = CalibrationProgress()
            calibration_progress.frame_num = frame_num
            calibration_progress.fps = fps
            calibration_progress_cb(calibration_progress)

    def decode_image_worker(self, part_q, frame_q, get_transform_cb, calibration):
        frame_num = 0
        transform = get_transform_cb()
        while True:
            data = frame_q.get()
            frame_q.task_done()
            if data is None:
                break
            frame_id, frame = data
            success, part_id, part_bytes, part_symbols, frame1, result_imgs = self.image_decoder.decode(frame, transform, calibration, False)
            part_q.put((success, part_id, part_bytes))
            frame_num += 1
            if frame_num & 0x7 == 0:
                transform = get_transform_cb()

    def decode_result_worker(self, part_q, frame_q, get_transform_cb, calibration, send_decode_image_result_cb):
        while True:
            data = frame_q.get()
            frame_q.task_done()
            if data is None:
                break
            frame_id, frame = data
            success, part_id, part_bytes, part_symbols, frame1, result_imgs = self.image_decoder.decode(frame, get_transform_cb(), calibration, True)
            part_q.put((success, part_id, part_bytes))
            send_decode_image_result_cb(frame1, success, result_imgs)
            time.sleep(1)

    def auto_transform_worker(self, part_q, frame_q, get_transform_cb, calibration, send_auto_trasform_cb):
        PIXELIZATION_CHANNEL_RANGE = (150, 180)
        PIXELIZATION_CHANNEL_DIFF = 3
        LOOP_NUM = 4
        frame_num = 0
        pixelization_thresholds = [(t, t, t) for t in range(PIXELIZATION_CHANNEL_RANGE[0], PIXELIZATION_CHANNEL_RANGE[1])]
        for t1 in range(PIXELIZATION_CHANNEL_RANGE[0], PIXELIZATION_CHANNEL_RANGE[1]):
            for t2 in range(PIXELIZATION_CHANNEL_RANGE[0], PIXELIZATION_CHANNEL_RANGE[1]):
                for t3 in range(PIXELIZATION_CHANNEL_RANGE[0], PIXELIZATION_CHANNEL_RANGE[1]):
                    if t1 != t2 and t2 != t3 and t3 != t1 and \
                       abs(t1 - t2) <= PIXELIZATION_CHANNEL_DIFF and \
                       abs(t2 - t3) <= PIXELIZATION_CHANNEL_DIFF and \
                       abs(t3 - t1) <= PIXELIZATION_CHANNEL_DIFF:
                        pixelization_thresholds.append((t1, t2, t3))
        auto_transforms = list(itertools.product(pixelization_thresholds))
        cur_auto_transform_index = 0
        auto_transform_scores = {}
        while True:
            data = frame_q.get()
            frame_q.task_done()
            if data is None:
                break
            frame_id, frame = data
            transform = get_transform_cb()
            has_succeeded = False
            for loop_id in range(LOOP_NUM):
                transform.pixelization_threshold, = auto_transforms[cur_auto_transform_index]
                success, part_id, part_bytes, part_symbols, frame1, result_imgs = self.image_decoder.decode(frame, transform, calibration, False)
                if not has_succeeded and (success or loop_id == LOOP_NUM - 1):
                    part_q.put((success, part_id, part_bytes))
                    has_succeeded = True
                auto_transform = auto_transforms[cur_auto_transform_index]
                if auto_transform not in auto_transform_scores:
                    auto_transform_scores[auto_transform] = 0
                auto_transform_scores[auto_transform] = auto_transform_scores[auto_transform] * 0.75 + float(success) * 0.25
                cur_auto_transform_index = (cur_auto_transform_index + 1) % len(auto_transforms)
            frame_num += 1
            if frame_num & 0x1f == 0:
                total_score = 0
                pixelization_threshold_weighted_sum = [0, 0, 0]
                for auto_transform, score in auto_transform_scores.items():
                    pixelization_threshold, = auto_transform
                    total_score += score
                    pixelization_threshold_weighted_sum[0] += pixelization_threshold[0] * score
                    pixelization_threshold_weighted_sum[1] += pixelization_threshold[1] * score
                    pixelization_threshold_weighted_sum[2] += pixelization_threshold[2] * score
                if total_score > 0:
                    transform.pixelization_threshold[0] = round(pixelization_threshold_weighted_sum[0] / total_score)
                    transform.pixelization_threshold[1] = round(pixelization_threshold_weighted_sum[1] / total_score)
                    transform.pixelization_threshold[2] = round(pixelization_threshold_weighted_sum[2] / total_score)
                    send_auto_trasform_cb(transform)
            time.sleep(1)

    def save_part_worker(self, running, running_lock, part_q, output_file, part_num, save_part_progress_cb, save_part_finish_cb, save_part_complete_cb, error_cb, finalization_start_cb, finalization_progress_cb, finalization_complete_cb, task_status_server_type, task_status_server_port):
        symbol_type = self.image_decoder.symbol_codec.symbol_type
        dim = self.image_decoder.dim
        task = image_decode_task.Task(output_file)
        if os.path.isfile(task.task_path):
            task.load()
            if dim != task.dim or symbol_type != task.symbol_type or part_num != task.part_num:
                if error_cb:
                    s = 'inconsistent task config\n'
                    s += 'task:\n'
                    s += 'symbol_type={}\n'.format(task.symbol_type.name)
                    s += 'dim={}\n'.format(task.dim)
                    s += 'part_num={}\n'.format(task.part_num)
                    s += 'input:\n'
                    s += 'symbol_type={}\n'.format(symbol_type.name)
                    s += 'dim={}\n'.format(dim)
                    s += 'part_num={}\n'.format(part_num)
                    error_cb(s)
                running[0] = False
                while True:
                    data = part_q.get()
                    part_q.task_done()
                    if data is None:
                        break
                return
        else:
            task.init(symbol_type, dim, part_num)
            success = task.allocate_blob()
            if not success:
                if error_cb:
                    s = 'can\'t allocate file \'{}\'\n'.format(task.blob_path)
                    error_cb(s)
                running[0] = False
                while True:
                    data = part_q.get()
                    part_q.task_done()
                    if data is None:
                        break
                return
        task.set_finalization_cb(finalization_start_cb, finalization_progress_cb, finalization_complete_cb)
        task_status_server = None
        if task_status_server_type != server_utils.ServerType.NONE:
            task_status_server = image_decode_task_status_server.create_task_status_server(task_status_server_type)
            task_status_server.start(task_status_server_port)
        t0 = time.time()
        frame_num = 0
        fps = 0
        frame_num0 = 0
        done_fps = 0
        done_part_num0 = task.done_part_num
        bps = 0
        bpf = image_decode_task.get_part_byte_num(symbol_type, dim)
        left_days = 0
        left_hours = 0
        left_minutes = 0
        left_seconds = 0
        while True:
            data = part_q.get()
            part_q.task_done()
            if data is None:
                break
            success, part_id, part_bytes = data
            if success:
                task.update_part(part_id, part_bytes)
            frame_num += 1
            if frame_num & 0xf == 0:
                t1 = time.time()
                delta_t = max(t1 - t0, 0.001)
                t0 = t1
                delta_frame_num = frame_num - frame_num0
                frame_num0 = frame_num
                fps1 = delta_frame_num / delta_t
                fps = fps * 0.5 + fps1 * 0.5
                delta_done_part_num = task.done_part_num - done_part_num0
                done_part_num0 = task.done_part_num
                done_fps1 = delta_done_part_num / delta_t
                done_fps = done_fps * 0.5 + done_fps1 * 0.5
                bps = done_fps * bpf
                if done_fps > 0.01:
                    left_total_seconds = round((part_num - task.done_part_num) / done_fps)
                    left_days = left_total_seconds // (24 * 60 * 60)
                    left_total_seconds -= left_days * (24 * 60 * 60)
                    left_hours = left_total_seconds // (60 * 60)
                    left_total_seconds -= left_hours * (60 * 60)
                    left_minutes = left_total_seconds // 60
                    left_seconds = left_total_seconds - left_minutes * 60
                else:
                    left_days = 0
                    left_hours = 0
                    left_minutes = 0
                    left_seconds = 0
            if frame_num & 0x7 == 0:
                if save_part_progress_cb:
                    save_part_progress = SavePartProgress()
                    save_part_progress.frame_num = frame_num
                    save_part_progress.done_part_num = task.done_part_num
                    save_part_progress.part_num = part_num
                    save_part_progress.fps = fps
                    save_part_progress.done_fps = done_fps
                    save_part_progress.bps = bps
                    save_part_progress.left_days = left_days
                    save_part_progress.left_hours = left_hours
                    save_part_progress.left_minutes = left_minutes
                    save_part_progress.left_seconds = left_seconds
                    save_part_progress_cb(save_part_progress)
            if task_status_server and frame_num & 0x1f == 0:
                task_status_server.update_task_status(task.to_task_bytes())
            if task.is_done():
                if save_part_progress_cb:
                    save_part_progress = SavePartProgress()
                    save_part_progress.frame_num = frame_num
                    save_part_progress.done_part_num = task.done_part_num
                    save_part_progress.part_num = part_num
                    save_part_progress.fps = fps
                    save_part_progress.done_fps = done_fps
                    save_part_progress.bps = bps
                    save_part_progress.left_days = left_days
                    save_part_progress.left_hours = left_hours
                    save_part_progress.left_minutes = left_minutes
                    save_part_progress.left_seconds = left_seconds
                    save_part_progress_cb(save_part_progress)
                task.finalize()
                if save_part_complete_cb:
                    save_part_complete_cb()
                running[0] = False
                while True:
                    data = part_q.get()
                    part_q.task_done()
                    if data is None:
                        break
                break
        if task_status_server:
            task_status_server.stop()
        if not task.is_done():
            task.flush()
        if save_part_finish_cb:
            save_part_finish_cb()
