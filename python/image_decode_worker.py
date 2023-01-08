import os
import time

import image_decoder
import image_stream
import image_decode_task

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
    def __init__(self, pixel_type):
        self.image_decoder = image_decoder.ImageDecoder(pixel_type)

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

    def calibrate_worker(self, frame_q, dim, get_transform_cb, calibrate_cb, send_calibration_image_result_cb, calibration_progress_cb):
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
            frame1, calibration, result_imgs = self.image_decoder.calibrate(frame, dim, get_transform_cb(), True)
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

    def decode_image_worker(self, part_q, frame_q, dim, get_transform_cb, calibration):
        while True:
            data = frame_q.get()
            frame_q.task_done()
            if data is None:
                break
            frame_id, frame = data
            success, part_id, part_bytes, part_pixels, frame1, result_imgs = self.image_decoder.decode(frame, dim, get_transform_cb(), calibration, False)
            part_q.put((success, part_id, part_bytes))

    def decode_result_worker(self, part_q, frame_q, dim, get_transform_cb, calibration, send_decode_image_result_cb, interval):
        while True:
            data = frame_q.get()
            frame_q.task_done()
            if data is None:
                break
            frame_id, frame = data
            success, part_id, part_bytes, part_pixels, frame1, result_imgs = self.image_decoder.decode(frame, dim, get_transform_cb(), calibration, True)
            part_q.put((success, part_id, part_bytes))
            send_decode_image_result_cb(frame1, success, result_imgs)
            time.sleep(interval / 1000)

    def auto_transform_worker(self, part_q, frame_q, dim, get_transform_cb, calibration, send_auto_trasform_cb, interval):
        THRESHOLD_NUM = 256
        LOOP_NUM = 4
        frame_num = 0
        pixelization_threshold_scores = [0 for i in range(THRESHOLD_NUM)]
        cur_pixelization_threshold = 0
        while True:
            data = frame_q.get()
            frame_q.task_done()
            if data is None:
                break
            frame_id, frame = data
            transform = get_transform_cb()
            has_succeeded = False
            for i in range(LOOP_NUM):
                transform.pixelization_threshold = (cur_pixelization_threshold, ) * 3
                success, part_id, part_bytes, part_pixels, frame1, result_imgs = self.image_decoder.decode(frame, dim, transform, calibration, False)
                if not has_succeeded and (success or i == LOOP_NUM - 1):
                    part_q.put((success, part_id, part_bytes))
                    has_succeeded = True
                pixelization_threshold_scores[cur_pixelization_threshold] = pixelization_threshold_scores[cur_pixelization_threshold] * 0.75 + float(success) * 0.25
                cur_pixelization_threshold = (cur_pixelization_threshold + 1) % THRESHOLD_NUM
            frame_num += 1
            if frame_num & 0x7f == 0:
                total_score = 0
                sum1 = 0
                for threshold in range(THRESHOLD_NUM):
                    total_score += pixelization_threshold_scores[threshold]
                    sum1 += pixelization_threshold_scores[threshold] * threshold
                if total_score > 0:
                    threshold = round(sum1 / total_score)
                    transform1 = transform.clone()
                    transform1.pixelization_threshold = (threshold, ) * 3
                    send_auto_trasform_cb(transform1)
            time.sleep(interval / 1000)

    def save_part_worker(self, running, running_lock, part_q, output_file, dim, pixel_size, space_size, part_num, save_part_progress_cb, save_part_finish_cb, save_part_complete_cb, error_cb, finalization_start_cb, finalization_progress_cb, finalization_complete_cb, task_status_server):
        pixel_type = self.image_decoder.pixel_codec.get_pixel_type()
        task = image_decode_task.Task(output_file)
        if os.path.isfile(task.task_path):
            task.load()
            if dim != task.dim or pixel_size != task.pixel_size or part_num != task.part_num:
                if error_cb:
                    s = 'inconsistent task config\n'
                    s += 'task:\n'
                    s += 'dim={}\n'.format(task.dim)
                    s += 'pixel_type={}\n'.format(task.pixel_type.name)
                    s += 'pixel_size={}\n'.format(task.pixel_size)
                    s += 'space_size={}\n'.format(task.space_size)
                    s += 'part_num={}\n'.format(task.part_num)
                    s += 'input:\n'
                    s += 'dim={}\n'.format(dim)
                    s += 'pixel_type={}\n'.format(pixel_type.name)
                    s += 'pixel_size={}\n'.format(pixel_size)
                    s += 'space_size={}\n'.format(space_size)
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
            task.init(dim, pixel_type, pixel_size, space_size, part_num)
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
        t0 = time.time()
        frame_num = 0
        fps = 0
        frame_num0 = 0
        done_fps = 0
        done_part_num0 = task.done_part_num
        bps = 0
        tile_x_num, tile_y_num, tile_x_size, tile_y_size = dim
        bpf = image_decode_task.get_part_byte_num(tile_x_num, tile_y_num, tile_x_size, tile_y_size, pixel_type)
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
            if frame_num & 0x7f == 0:
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
            if frame_num & 0xf == 0:
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
            if task_status_server and (task_status_server.need_update_task_status() or (task_status_server.is_running() and frame_num & 0xff == 0)):
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
        if not task.is_done():
            task.flush()
        if save_part_finish_cb:
            save_part_finish_cb()
