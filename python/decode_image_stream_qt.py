import os
import sys
import threading
import queue

from PySide6 import QtCore, QtGui, QtWidgets
import cv2 as cv

import image_codec_types
import symbol_codec
import transform_utils
import image_decoder
import image_decode_worker
import image_decode_task
import image_decode_task_status_server

class Widget(QtWidgets.QWidget):
    task_status_server_start = QtCore.Signal(bool)
    send_calibration = QtCore.Signal(image_decoder.Calibration)
    send_calibration_image_result = QtCore.Signal(object, bool, list)
    send_calibration_progress = QtCore.Signal(image_decode_worker.CalibrationProgress)
    send_decode_image_result = QtCore.Signal(object, bool, list)
    send_auto_transform = QtCore.Signal(transform_utils.Transform)
    send_save_part_progress = QtCore.Signal(image_decode_worker.SavePartProgress)
    send_save_part_complete = QtCore.Signal()
    send_save_part_error = QtCore.Signal(str)
    send_finalization_start = QtCore.Signal(image_decode_task.FinalizationProgress)
    send_finalization_progress = QtCore.Signal(image_decode_task.FinalizationProgress)

    def __init__(self, output_file, symbol_type, dim, part_num, mp):
        super().__init__()

        self.output_file = output_file
        self.image_decode_worker = image_decode_worker.ImageDecodeWorker(symbol_type, dim)
        self.tile_x_num, self.tile_y_num, self.tile_x_size, self.tile_y_size = dim
        self.part_num = part_num
        self.mp = mp

        self.transform = transform_utils.Transform()
        self.calibration = image_decoder.Calibration()
        self.image = None
        self.result_images = [[None for j in range(self.tile_x_num)] for i in range(self.tile_y_num)]

        self.frame_q = queue.Queue(maxsize=4)
        self.part_q = queue.Queue(maxsize=4)
        self.fetch_image_thread = None
        self.calibrate_thread = None
        self.decode_image_threads = []
        self.decode_image_result_thread = None
        self.auto_transform_thread = None
        self.save_part_thread = None
        self.transform_lock = threading.Lock()
        self.calibration_running = [False]
        self.task_running = [False]
        self.running_lock = threading.Lock()

        self.monitor_on = False
        self.task_status_server_on = False
        self.task_status_server = image_decode_task_status_server.TaskStatusServer()

        self.task_status_server_start.connect(self.on_task_status_server_started)
        self.send_calibration.connect(self.receive_calibration)
        self.send_calibration_image_result.connect(self.show_result)
        self.send_calibration_progress.connect(self.show_calibration_progress)
        self.send_decode_image_result.connect(self.show_result)
        self.send_auto_transform.connect(self.update_auto_transform)
        self.send_save_part_progress.connect(self.show_task_save_part_progress)
        self.send_save_part_complete.connect(self.task_save_part_complete)
        self.send_save_part_error.connect(self.error_msg)
        self.send_finalization_start.connect(self.task_finalization_start)
        self.send_finalization_progress.connect(self.task_finalization_progress)

        layout = QtWidgets.QVBoxLayout(self)

        image_layout = QtWidgets.QHBoxLayout()
        layout.addLayout(image_layout)

        image_w = 600
        image_h = 400

        image_group_box = QtWidgets.QGroupBox('image')
        image_layout.addWidget(image_group_box)

        image_layout1 = QtWidgets.QHBoxLayout(image_group_box)

        self.image_label = QtWidgets.QLabel()
        self.image_label.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Fixed)
        self.image_label.setFixedSize(image_w, image_h)
        self.image_label.setScaledContents(True)
        image_layout1.addWidget(self.image_label)

        result_image_group_box = QtWidgets.QGroupBox('result image')
        image_layout.addWidget(result_image_group_box)

        result_image_layout = QtWidgets.QVBoxLayout(result_image_group_box)

        self.result_image_labels = [[None for j in range(self.tile_x_num)] for i in range(self.tile_y_num)]
        for tile_y_id in range(self.tile_y_num):
            result_image_layout1 = QtWidgets.QHBoxLayout()
            result_image_layout.addLayout(result_image_layout1)
            for tile_x_id in range(self.tile_x_num):
                result_image_label = QtWidgets.QLabel()
                result_image_label.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Fixed)
                result_image_label.setFixedSize(round(image_w / self.tile_x_num), round(image_h / self.tile_y_num))
                result_image_label.setScaledContents(True)
                result_image_layout1.addWidget(result_image_label)
                self.result_image_labels[tile_y_id][tile_x_id] = result_image_label

        control_layout = QtWidgets.QHBoxLayout()
        layout.addLayout(control_layout)

        calibration_group_box = QtWidgets.QGroupBox('calibration')
        control_layout.addWidget(calibration_group_box)

        calibration_group_box_layout = QtWidgets.QHBoxLayout(calibration_group_box)

        self.calibrate_button = QtWidgets.QPushButton('calibrate')
        self.calibrate_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.calibrate_button.setFixedWidth(80)
        self.calibrate_button.setCheckable(True)
        self.calibrate_button.clicked.connect(self.toggle_calibration_start_stop)
        calibration_group_box_layout.addWidget(self.calibrate_button)

        calibration_button_layout = QtWidgets.QVBoxLayout()
        calibration_group_box_layout.addLayout(calibration_button_layout)

        self.save_calibration_button = QtWidgets.QPushButton('save calibration')
        self.save_calibration_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.save_calibration_button.setFixedWidth(120)
        self.save_calibration_button.setEnabled(False)
        self.save_calibration_button.clicked.connect(self.save_calibration)
        calibration_button_layout.addWidget(self.save_calibration_button)

        self.load_calibration_button = QtWidgets.QPushButton('load calibration')
        self.load_calibration_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.load_calibration_button.setFixedWidth(120)
        self.load_calibration_button.clicked.connect(self.load_calibration)
        calibration_button_layout.addWidget(self.load_calibration_button)

        self.clear_calibration_button = QtWidgets.QPushButton('clear calibration')
        self.clear_calibration_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.clear_calibration_button.setFixedWidth(120)
        self.clear_calibration_button.setEnabled(False)
        self.clear_calibration_button.clicked.connect(self.clear_calibration)
        calibration_button_layout.addWidget(self.clear_calibration_button)

        task_group_box = QtWidgets.QGroupBox('task')
        control_layout.addWidget(task_group_box)

        task_group_box_layout = QtWidgets.QHBoxLayout(task_group_box)

        self.task_button = QtWidgets.QPushButton('start')
        self.task_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.task_button.setFixedWidth(80)
        self.task_button.setCheckable(True)
        self.task_button.clicked.connect(self.toggle_task_start_stop)
        task_group_box_layout.addWidget(self.task_button)

        monitor_group_box = QtWidgets.QGroupBox('monitor')
        control_layout.addWidget(monitor_group_box)

        monitor_layout = QtWidgets.QVBoxLayout(monitor_group_box)

        self.monitor_button= QtWidgets.QPushButton('monitor')
        self.monitor_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.monitor_button.setFixedWidth(80)
        self.monitor_button.setCheckable(True)
        self.monitor_button.setChecked(True)
        self.monitor_button.clicked.connect(self.toggle_monitor)
        monitor_layout.addWidget(self.monitor_button)

        self.save_image_button = QtWidgets.QPushButton('save image')
        self.save_image_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.save_image_button.setFixedWidth(80)
        self.save_image_button.clicked.connect(self.save_image)
        monitor_layout.addWidget(self.save_image_button)

        task_status_server_group_box = QtWidgets.QGroupBox('server')
        control_layout.addWidget(task_status_server_group_box)

        task_status_server_layout = QtWidgets.QVBoxLayout(task_status_server_group_box)

        self.task_status_server_button = QtWidgets.QPushButton('server')
        self.task_status_server_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.task_status_server_button.setFixedWidth(80)
        self.task_status_server_button.setCheckable(True)
        self.task_status_server_button.clicked.connect(self.toggle_task_status_server)
        task_status_server_layout.addWidget(self.task_status_server_button)

        task_status_server_port_layout = QtWidgets.QHBoxLayout()
        task_status_server_layout.addLayout(task_status_server_port_layout)

        self.task_status_server_port_label = QtWidgets.QLabel('port:')
        task_status_server_port_layout.addWidget(self.task_status_server_port_label)

        self.task_status_server_port_line_edit = QtWidgets.QLineEdit('8123')
        self.task_status_server_port_line_edit.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.task_status_server_port_line_edit.setFixedWidth(40)
        task_status_server_port_layout.addWidget(self.task_status_server_port_line_edit)

        transform_group_box = QtWidgets.QGroupBox('transform')
        transform_group_box.setCheckable(True)
        control_layout.addWidget(transform_group_box)

        transform_layout = QtWidgets.QHBoxLayout(transform_group_box)

        transform_button_layout = QtWidgets.QVBoxLayout()
        transform_layout.addLayout(transform_button_layout)

        self.save_transform_button = QtWidgets.QPushButton('save transform')
        self.save_transform_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.save_transform_button.setFixedWidth(100)
        self.save_transform_button.clicked.connect(self.save_transform)
        transform_button_layout.addWidget(self.save_transform_button)

        self.load_transform_button = QtWidgets.QPushButton('load transform')
        self.load_transform_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.load_transform_button.setFixedWidth(100)
        self.load_transform_button.clicked.connect(self.load_transform)
        transform_button_layout.addWidget(self.load_transform_button)

        form_layout1 = QtWidgets.QFormLayout()
        transform_layout.addLayout(form_layout1)
        form_layout2 = QtWidgets.QFormLayout()
        transform_layout.addLayout(form_layout2)

        self.bbox_label = QtWidgets.QLabel('bbox:')
        self.bbox_line_edit = QtWidgets.QLineEdit()
        self.bbox_line_edit.editingFinished.connect(self.transform_changed)
        form_layout1.addRow(self.bbox_label, self.bbox_line_edit)

        self.sphere_label = QtWidgets.QLabel('sphere:')
        self.sphere_line_edit = QtWidgets.QLineEdit()
        self.sphere_line_edit.editingFinished.connect(self.transform_changed)
        form_layout1.addRow(self.sphere_label, self.sphere_line_edit)

        self.filter_level_label = QtWidgets.QLabel('filter_level:')
        self.filter_level_line_edit = QtWidgets.QLineEdit()
        self.filter_level_line_edit.editingFinished.connect(self.transform_changed)
        form_layout1.addRow(self.filter_level_label, self.filter_level_line_edit)

        self.binarization_threshold_label = QtWidgets.QLabel('binarization_threshold:')
        self.binarization_threshold_line_edit = QtWidgets.QLineEdit()
        self.binarization_threshold_line_edit.editingFinished.connect(self.transform_changed)
        form_layout2.addRow(self.binarization_threshold_label, self.binarization_threshold_line_edit)

        self.pixelization_threshold_label = QtWidgets.QLabel('pixelization_threshold:')
        self.pixelization_threshold_line_edit = QtWidgets.QLineEdit()
        self.pixelization_threshold_line_edit.editingFinished.connect(self.transform_changed)
        form_layout2.addRow(self.pixelization_threshold_label, self.pixelization_threshold_line_edit)

        self.update_transform_ui(self.transform)

        status_group_box = QtWidgets.QGroupBox('status')
        layout.addWidget(status_group_box)

        status_group_box_layout = QtWidgets.QHBoxLayout(status_group_box)

        result_frame = QtWidgets.QFrame()
        result_frame.setFrameStyle(QtWidgets.QFrame.Panel | QtWidgets.QFrame.Sunken)
        result_frame.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        result_frame.setFixedWidth(50)
        status_group_box_layout.addWidget(result_frame)

        result_frame_layout = QtWidgets.QHBoxLayout(result_frame)

        self.result_label = QtWidgets.QLabel('-')
        result_frame_layout.addWidget(self.result_label)

        status_frame = QtWidgets.QFrame()
        status_frame.setFrameStyle(QtWidgets.QFrame.Panel | QtWidgets.QFrame.Sunken)
        status_group_box_layout.addWidget(status_frame)

        status_frame_layout = QtWidgets.QHBoxLayout(status_frame)

        self.status_label = QtWidgets.QLabel('-')
        status_frame_layout.addWidget(self.status_label)

        self.task_save_part_progress_bar = QtWidgets.QProgressBar()
        self.task_save_part_progress_bar.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.task_save_part_progress_bar.setFixedWidth(500)
        self.task_save_part_progress_bar.setFormat('%p%')
        self.task_save_part_progress_bar.setRange(0, self.part_num)
        status_group_box_layout.addWidget(self.task_save_part_progress_bar)

        self.task_finalization_progress_dialog = None

        self.resize(1200, 600)

    def start_calibration(self):
        self.task_button.setEnabled(False)
        self.clear_calibration_button.setEnabled(False)
        self.save_calibration_button.setEnabled(False)
        self.load_calibration_button.setEnabled(False)

        with self.running_lock:
            self.calibration_running = [True]

        def get_transform_fn():
            with self.transform_lock:
                return self.transform.clone()

        self.fetch_image_thread = threading.Thread(target=self.image_decode_worker.fetch_image_worker, args=(self.calibration_running, self.running_lock, self.frame_q, 200))
        self.fetch_image_thread.start()

        calibrate_cb = lambda *args: self.send_calibration.emit(*args)
        send_calibration_image_result_cb = lambda *args: self.send_calibration_image_result.emit(*args)
        calibration_progress_cb = lambda *args: self.send_calibration_progress.emit(*args)
        self.calibrate_thread = threading.Thread(target=self.image_decode_worker.calibrate_worker, args=(self.frame_q, get_transform_fn, calibrate_cb, send_calibration_image_result_cb, calibration_progress_cb))
        self.calibrate_thread.start()

    def stop_calibration(self):
        with self.running_lock:
            self.calibration_running = [False]
        self.fetch_image_thread.join()
        self.fetch_image_thread = None
        self.frame_q.put(None)
        self.frame_q.join()
        self.calibrate_thread.join()
        self.calibrate_thread = None

        self.task_button.setEnabled(True)
        self.load_calibration_button.setEnabled(True)
        if self.calibration.valid:
            self.clear_calibration_button.setEnabled(True)
            self.save_calibration_button.setEnabled(True)
        self.clear_status()

    def save_calibration(self):
        self.calibration.Save(self.output_file + '.calibration')

    def load_calibration(self):
        calibration_path = self.output_file + '.calibration'
        if os.path.isfile(calibration_path):
            self.calibration.load(calibration_path)
            if self.calibration.valid:
                self.clear_calibration_button.setEnabled(True)
                self.save_calibration_button.setEnabled(True)
        else:
            QtWidgets.QMessageBox.warning(self, 'Warning', 'can\'t find calibration file \'{}\''.format(calibration_path))

    def clear_calibration(self):
        self.calibration.valid = False
        self.clear_calibration_button.setEnabled(False)
        self.save_calibration_button.setEnabled(False)

    def start_task(self):
        self.calibrate_button.setEnabled(False)
        self.clear_calibration_button.setEnabled(False)
        self.save_calibration_button.setEnabled(False)
        self.load_calibration_button.setEnabled(False)

        with self.running_lock:
            self.task_running[0] = True

        def get_transform_fn():
            with self.transform_lock:
                return self.transform.clone()

        self.fetch_image_thread = threading.Thread(target=self.image_decode_worker.fetch_image_worker, args=(self.task_running, self.running_lock, self.frame_q, 25))
        self.fetch_image_thread.start()
        for i in range(self.mp):
            t = threading.Thread(target=self.image_decode_worker.decode_image_worker, args=(self.part_q, self.frame_q, get_transform_fn, self.calibration))
            t.start()
            self.decode_image_threads.append(t)

        send_decode_image_result_cb = lambda *args: self.send_decode_image_result.emit(*args)
        self.decode_image_result_thread = threading.Thread(target=self.image_decode_worker.decode_result_worker, args=(self.part_q, self.frame_q, get_transform_fn, self.calibration, send_decode_image_result_cb, 300))
        self.decode_image_result_thread.start()

        send_auto_transform_cb = lambda *args: self.send_auto_transform.emit(*args)
        self.auto_transform_thread = threading.Thread(target=self.image_decode_worker.auto_transform_worker, args=(self.part_q, self.frame_q, get_transform_fn, self.calibration, send_auto_transform_cb, 300))
        self.auto_transform_thread.start()

        save_part_progress_cb = lambda *args: self.send_save_part_progress.emit(*args)
        save_part_complete_cb = lambda *args: self.send_save_part_complete.emit(*args)
        save_part_error_cb = lambda *args: self.send_save_part_error.emit(*args)
        finalization_start_cb = lambda *args: self.send_finalization_start.emit(*args)
        finalization_progress_cb = lambda *args: self.send_finalization_progress.emit(*args)
        self.save_part_thread = threading.Thread(target=self.image_decode_worker.save_part_worker, args=(self.task_running, self.running_lock, self.part_q, self.output_file, self.part_num, save_part_progress_cb, None, save_part_complete_cb, save_part_error_cb, finalization_start_cb, finalization_progress_cb, None, self.task_status_server))
        self.save_part_thread.start()

    def stop_task(self):
        with self.running_lock:
            self.task_running[0] = False
        self.fetch_image_thread.join()
        self.fetch_image_thread = None
        for i in range(self.mp + 2):
            self.frame_q.put(None)
        self.frame_q.join()
        for t in self.decode_image_threads:
            t.join()
        self.decode_image_threads.clear()
        self.decode_image_result_thread.join()
        self.decode_image_result_thread = None
        self.auto_transform_thread.join()
        self.auto_transform_thread = None
        self.part_q.put(None)
        self.part_q.join()
        self.save_part_thread.join()
        self.save_part_thread = None

        self.calibrate_button.setEnabled(True)
        self.load_calibration_button.setEnabled(True)
        if self.calibration.valid:
            self.clear_calibration_button.setEnabled(True)
            self.save_calibration_button.setEnabled(True)
        self.clear_status()

    def toggle_calibration_start_stop(self):
        with self.running_lock:
            calibration_running = self.calibration_running[0]
        if calibration_running:
            self.stop_calibration()
        else:
            self.start_calibration()

    def toggle_task_start_stop(self):
        with self.running_lock:
            task_running = self.task_running[0]
        if task_running:
            self.stop_task()
        else:
            self.start_task()

    def clear_status(self):
        self.clear_images()
        self.result_label.setText('-')
        self.status_label.setText('-')
        self.task_save_part_progress_bar.setValue(0)

    def clear_images(self):
        self.image_label.clear()
        for e1 in self.result_image_labels:
            for e2 in e1:
                e2.clear()

    def toggle_monitor(self):
        self.monitor_on = not self.monitor_on
        if not self.monitor_on:
            self.clear_images()
        self.save_image_button.setEnabled(self.monitor_on)

    def save_image(self):
        if self.image:
            cv.imwrite(self.output_file + '.transformed_image.bmp', self.image)
        for tile_y_id in range(self.tile_y_num):
            for tile_x_id in range(self.tile_x_num):
                if self.result_images[tile_y_id][tile_x_id]:
                    cv.imwrite('{}.result_image_{}_{}.bmp'.format(self.output_file, tile_x_id, tile_y_id), self.result_images[tile_y_id][tile_x_id])

    def toggle_task_status_server(self):
        self.task_status_server_button.setEnabled(False)
        if self.task_status_server_on:
            self.task_status_server.stop()
            self.task_status_server_on = False
            self.task_status_server_port_line_edit.setEnabled(True)
            self.task_status_server_button.setEnabled(True)
        else:
            port = None
            try:
                port = image_decode_task_status_server.parse_task_status_server_port(self.task_status_server_port_line_edit.text())
            except image_codec_types.InvalidImageCodecArgument:
                QtWidgets.QMessageBox.warning(self, 'Warning', 'invalid task status server port \'{}\''.format(self.task_status_server_port_line_edit.text()))
            if port is not None:
                server_start_cb = lambda *args: self.task_status_server_start.emit(*args)
                self.task_status_server_on = True
                self.task_status_server_port_line_edit.setEnabled(False)
                self.task_status_server.start(port, server_start_cb)

    def on_task_status_server_started(self, success):
        if not success:
            self.toggle_task_status_server()
            self.task_status_server_button.setChecked(False)
        self.task_status_server_button.setEnabled(True)

    def save_transform(self):
        with self.transform_lock:
            self.transform.save(self.output_file + '.transform')

    def load_transform(self):
        transform_path = self.output_file + '.transform'
        if os.path.isfile(transform_path):
            with self.transform_lock:
                self.transform.load(transform_path)
                transform = self.transform.clone()
            self.update_transform_ui(transform)
        else:
            QtWidgets.QMessageBox.warning(self, 'Warning', 'can\'t find transform file \'{}\''.format(transform_path))

    def update_transform(self, transform):
        with self.transform_lock:
            self.transform = transform

    def update_transform_ui(self, transform):
        self.bbox_line_edit.setText(transform_utils.get_bbox_str(transform.bbox))
        self.sphere_line_edit.setText(transform_utils.get_sphere_str(transform.sphere))
        self.filter_level_line_edit.setText(transform_utils.get_filter_level_str(transform.filter_level))
        self.binarization_threshold_line_edit.setText(transform_utils.get_binarization_threshold_str(transform.binarization_threshold))
        self.pixelization_threshold_line_edit.setText(transform_utils.get_pixelization_threshold_str(transform.pixelization_threshold))

    def receive_calibration(self, calibration):
        self.calibration = calibration

    def show_result(self, img, success, result_imgs):
        with self.running_lock:
            running = self.calibration_running[0] or self.task_running[0]
        if running:
            self.image = img
            if self.monitor_on:
                self.image_label.setPixmap(QtGui.QPixmap.fromImage(QtGui.QImage(img.data, img.shape[1], img.shape[0], img.shape[1] * img.shape[2], QtGui.QImage.Format_BGR888)))
            for tile_y_id in range(self.tile_y_num):
                for tile_x_id in range(self.tile_x_num):
                    result_img = result_imgs[tile_y_id][tile_x_id]
                    self.result_images[tile_y_id][tile_x_id] = result_img
                    if self.monitor_on:
                        self.result_image_labels[tile_y_id][tile_x_id].setPixmap(QtGui.QPixmap.fromImage(QtGui.QImage(result_img.data, result_img.shape[1], result_img.shape[0], result_img.shape[1] * result_img.shape[2], QtGui.QImage.Format_BGR888)))
            self.result_label.setText('pass' if success else  'fail')

    def show_calibration_progress(self, calibration_progress):
        with self.running_lock:
            calibration_running = self.calibration_running[0]
        if calibration_running:
            s = '{} frames processed, fps={:.2f}'.format(calibration_progress.frame_num, calibration_progress.fps)
            self.status_label.setText(s)

    def show_task_save_part_progress(self, task_save_part_progress):
        with self.running_lock:
            task_running = self.task_running[0]
        if task_running:
            s = '{} frames processed, {}/{} parts transferred, fps={:.2f}, done_fps={:.2f}, bps={:.0f}, left_time={:0>2d}d{:0>2d}h{:0>2d}m{:0>2d}s'.format(task_save_part_progress.frame_num, task_save_part_progress.done_part_num, task_save_part_progress.part_num, task_save_part_progress.fps, task_save_part_progress.done_fps, task_save_part_progress.bps, task_save_part_progress.left_days, task_save_part_progress.left_hours, task_save_part_progress.left_minutes, task_save_part_progress.left_seconds)
            self.status_label.setText(s)
            self.task_save_part_progress_bar.setValue(task_save_part_progress.done_part_num)

    def transform_changed(self):
        try:
            transform = transform_utils.Transform()
            transform.bbox = transform_utils.parse_bbox(self.bbox_line_edit.text())
            transform.sphere = transform_utils.parse_sphere(self.sphere_line_edit.text())
            transform.filter_level = transform_utils.parse_filter_level(self.filter_level_line_edit.text())
            transform.binarization_threshold = transform_utils.parse_binarization_threshold(self.binarization_threshold_line_edit.text())
            transform.pixelization_threshold = transform_utils.parse_pixelization_threshold(self.pixelization_threshold_line_edit.text())
            self.update_transform(transform)
        except image_codec_types.InvalidImageCodecArgument:
            pass

    def update_auto_transform(self, transform):
        self.update_transform(transform)
        self.update_transform_ui(transform)

    def task_save_part_complete(self):
        self.stop_task()
        self.task_button.setChecked(False)
        QtWidgets.QMessageBox.information(self, 'Info', 'transfer done', QtWidgets.QMessageBox.Ok, QtWidgets.QMessageBox.Ok)
        #self.close()

    def task_finalization_start(self, finalization_progress):
        #self.task_finalization_progress_dialog = QtWidgets.QProgressDialog('finalizing task', '', finalization_progress.done_block_num, finalization_progress.block_num, this)
        #self.task_finalization_progress_dialog.setMinimumDuration(0)
        #self.task_finalization_progress_dialog.setWindowModality(QtCore.Qt.WindowModal)
        pass

    def task_finalization_progress(self, finalization_progress):
        #self.task_finalization_progress_dialog.setValue(finalization_progress.done_block_num)
        pass

    def error_msg(self, msg):
        QtWidgets.QMessageBox.critical(self, 'Error', msg, QtWidgets.QMessageBox.Ok, QtWidgets.QMessageBox.Ok)
        self.close()

    def closeEvent(self, event):
        with self.running_lock:
            calibration_running = self.calibration_running[0]
            task_running = self.task_running[0]
        if calibration_running:
            self.stop_calibration()
        if task_running:
            self.stop_task()
        if self.task_status_server.is_running():
            self.task_status_server.stop()

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('output_file', help='output file')
    parser.add_argument('symbol_type', help='symbol type')
    parser.add_argument('dim', help='dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size')
    parser.add_argument('part_num', type=int, help='part num')
    parser.add_argument('--mp', type=int, default=1, help='multiprocessing')
    args = parser.parse_args()

    symbol_type = symbol_codec.parse_symbol_type(args.symbol_type)
    dim = image_codec_types.parse_dim(args.dim)

    app = QtWidgets.QApplication([])
    widget = Widget(args.output_file, symbol_type, dim, args.part_num, args.mp)
    widget.show()
    sys.exit(app.exec())
