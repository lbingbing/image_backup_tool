import os
import sys
import io
from PySide6 import QtCore, QtGui, QtWidgets
import multiprocessing
import queue
import threading
import cv2
import PIL.Image
import PIL.ImageQt
import time

import transform_utils
import bit_codec
import image_decoder
import image_decode_task
import merge_parts

class MyWidget(QtWidgets.QWidget):
    def __init__(self, transform_q, mp, frame_q, task_result_q, row_num, col_num):
        super().__init__()

        self.row_num = row_num
        self.col_num = col_num

        self.transform_q = transform_q
        self.mp = mp
        self.frame_q = frame_q
        self.task_result_q = task_result_q

        layout = QtWidgets.QVBoxLayout(self)

        image_layout = QtWidgets.QHBoxLayout()
        self.image_label = QtWidgets.QLabel()
        self.image_label.setScaledContents(True)
        self.image_label.setMinimumSize(1, 1)
        image_layout.addWidget(self.image_label)
        self.result_image_label = QtWidgets.QLabel()
        self.result_image_label.setScaledContents(True)
        self.result_image_label.setMinimumSize(1, 1)
        image_layout.addWidget(self.result_image_label)
        layout.addLayout(image_layout)

        transform_group_box = QtWidgets.QGroupBox('transform')
        transform_layout = QtWidgets.QHBoxLayout()
        form_layout1 = QtWidgets.QFormLayout()
        transform_layout.addLayout(form_layout1)
        form_layout2 = QtWidgets.QFormLayout()
        transform_layout.addLayout(form_layout2)
        transform_group_box.setLayout(transform_layout)
        layout.addWidget(transform_group_box)

        binarization_threshold_label = QtWidgets.QLabel('binarization_threshold:')
        self.binarization_threshold_line_edit = QtWidgets.QLineEdit()
        self.binarization_threshold_line_edit.setText('128')
        self.binarization_threshold_line_edit.editingFinished.connect(self.transform_changed)
        form_layout1.addRow(binarization_threshold_label, self.binarization_threshold_line_edit)

        filter_level_label = QtWidgets.QLabel('filter_level:')
        self.filter_level_line_edit = QtWidgets.QLineEdit()
        self.filter_level_line_edit.setText('4')
        self.filter_level_line_edit.editingFinished.connect(self.transform_changed)
        form_layout2.addRow(filter_level_label, self.filter_level_line_edit)

        bbox_label = QtWidgets.QLabel('bbox:')
        self.bbox_line_edit = QtWidgets.QLineEdit()
        self.bbox_line_edit.setText('0,0,1,1')
        self.bbox_line_edit.editingFinished.connect(self.transform_changed)
        form_layout1.addRow(bbox_label, self.bbox_line_edit)

        sphere_label = QtWidgets.QLabel('sphere:')
        self.sphere_line_edit = QtWidgets.QLineEdit()
        self.sphere_line_edit.setText('0,0,0,0')
        self.sphere_line_edit.editingFinished.connect(self.transform_changed)
        form_layout2.addRow(sphere_label, self.sphere_line_edit)

        quad_label = QtWidgets.QLabel('quad:')
        self.quad_line_edit = QtWidgets.QLineEdit()
        self.quad_line_edit.setText('0,0,0,0,0,0,0,0')
        self.quad_line_edit.editingFinished.connect(self.transform_changed)
        form_layout1.addRow(quad_label, self.quad_line_edit)

        result_group_box = QtWidgets.QGroupBox('result')
        result_layout = QtWidgets.QHBoxLayout()
        self.result_label = QtWidgets.QLabel()
        result_layout.addWidget(self.result_label)
        result_group_box.setLayout(result_layout)
        layout.addWidget(result_group_box)

        task_group_box = QtWidgets.QGroupBox('task')
        task_layout = QtWidgets.QHBoxLayout()
        self.task_label = QtWidgets.QLabel()
        task_layout.addWidget(self.task_label)
        task_group_box.setLayout(task_layout)
        layout.addWidget(task_group_box)

        self.resize(1200, 650)

        self.transform_changed()

        self.show_image_t = threading.Thread(target=self.show_image)
        self.show_image_t.start()

        self.show_task_result_t = threading.Thread(target=self.show_task_result)
        self.show_task_result_t.start()

    @QtCore.Slot()
    def transform_changed(self):
        transform = transform_utils.Transform()
        transform.binarization_threshold = int(self.binarization_threshold_line_edit.text())
        transform.filter_level = int(self.filter_level_line_edit.text())
        transform.bbox = transform_utils.parse_bbox(self.bbox_line_edit.text())
        transform.sphere = transform_utils.parse_sphere(self.sphere_line_edit.text())
        transform.quad = transform_utils.parse_quad(self.quad_line_edit.text())
        self.transform = transform

        for i in range(self.mp):
            self.transform_q.put(transform)

    def show_image(self):
        while True:
            data = self.frame_q.get()
            if data is None:
                break
            frame_id, frame = data
            with PIL.Image.fromarray(frame) as img:
                self.image_label.setPixmap(QtGui.QPixmap.fromImage(PIL.ImageQt.ImageQt(img)))
                success, part_id, part_bytes, result_img = image_decoder.decode_image(img, self.row_num, self.col_num, self.transform, result_image=True)
                self.result_image_label.setPixmap(QtGui.QPixmap.fromImage(PIL.ImageQt.ImageQt(result_img)))
                self.result_label.setText('pass' if success else 'fail')
            time.sleep(0.2)

    def show_task_result(self):
        while True:
            result = self.task_result_q.get()
            if result is None:
                break
            done_part_num, part_num, frame_num = result
            self.task_label.setText('{}/{} parts transferred, {} frames processed'.format(done_part_num, part_num, frame_num))

    def stop_threads(self):
        self.show_image_t.join()
        self.show_task_result_t.join()

def capture_image_worker(frame_q, stop_q):
    url = 0
    #url = 'http://192.168.10.108:8080/video'
    capture = cv2.VideoCapture(url)
    #width, height = capture.get(3), capture.get(4)
    #capture.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    #capture.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    #print(width, height)

    frame_id = 0
    while True:
        ret, frame = capture.read()
        frame_q.put((frame_id, frame))
        if frame_id % 10 == 0:
            try:
                stop_q.get(False)
                break
            except queue.Empty:
                pass
        frame_id += 1
        cv2.waitKey(100)

def decode_image_worker(part_q, frame_q, transform_q, image_dir_path, row_num, col_num):
    transform = transform_utils.Transform()
    while True:
        try:
            transform = transform_q.get(False)
        except queue.Empty:
            pass
        data = frame_q.get()
        if data is None:
            break
        frame_id, frame = data
        with PIL.Image.fromarray(frame) as img:
            if image_dir_path:
                img.save(os.path.join(image_dir_path, '{}.jpg'.format(frame_id)))
            success, part_id, part_bytes, result_img = image_decoder.decode_image(img, row_num, col_num, transform)
        part_q.put((success, part_id, part_bytes))

def save_part_worker(stop_q, task_result_q, part_q, output_file, row_num, col_num, part_num):
    task = image_decode_task.Task()
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
                f.seek(part_id * len(part_bytes), io.SEEK_SET)
                f.write(part_bytes)
                task.set_part_done(part_id)
            frame_num += 1
            task_result_q.put((task.done_part_num, task.part_num, frame_num))
            if task.done_part_num == task.part_num:
                stop_q.put(None)
                break
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
    parser.add_argument('--mp', type=int, default=1, help='multiprocessing')
    args = parser.parse_args()

    row_num, col_num = image_decoder.parse_dim(args.dim)

    frame_q = multiprocessing.Queue(20)
    part_q = multiprocessing.Queue(20)
    stop_q = multiprocessing.Queue(1)
    transform_q = multiprocessing.Queue(args.mp * 2)
    task_result_q = multiprocessing.Queue(5)

    capture_image_p = multiprocessing.Process(target=capture_image_worker, args=(frame_q, stop_q))
    capture_image_p.start()

    decode_image_ps = []
    for i in range(args.mp):
        decode_image_ps.append(multiprocessing.Process(target=decode_image_worker, args=(part_q, frame_q, transform_q, args.image_dir_path, row_num, col_num)))
    for e in decode_image_ps:
        e.start()

    save_part_p = multiprocessing.Process(target=save_part_worker, args=(stop_q, task_result_q, part_q, args.output_file, row_num, col_num, args.part_num))
    save_part_p.start()

    app = QtWidgets.QApplication([])
    widget = MyWidget(transform_q, args.mp, frame_q, task_result_q, row_num, col_num)
    widget.show()
    exit_v = app.exec()

    stop_q.put(None)
    capture_image_p.join()
    for i in range(args.mp + 1):
        frame_q.put(None)
    for e in decode_image_ps:
        e.join()
    task_result_q.put(None)
    widget.stop_threads()
    part_q.put(None)
    save_part_p.join()

    sys.exit(exit_v)
