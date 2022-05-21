import sys
from PySide6 import QtCore, QtGui, QtWidgets
import multiprocessing
import queue
import threading
import cv2
import PIL.Image
import PIL.ImageQt

import transform_utils
import decode_image
import bit_codec

class MyWidget(QtWidgets.QWidget):
    def __init__(self, frame_q, row_num, col_num):
        super().__init__()

        self.row_num = row_num
        self.col_num = col_num

        self.frame_q = frame_q

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
        self.binarization_threshold_line_edit.setText('128');
        self.binarization_threshold_line_edit.editingFinished.connect(self.transform_changed)
        form_layout1.addRow(binarization_threshold_label, self.binarization_threshold_line_edit)

        filter_level_label = QtWidgets.QLabel('filter_level:')
        self.filter_level_line_edit = QtWidgets.QLineEdit()
        self.filter_level_line_edit.setText('4');
        self.filter_level_line_edit.editingFinished.connect(self.transform_changed)
        form_layout2.addRow(filter_level_label, self.filter_level_line_edit)

        bbox_label = QtWidgets.QLabel('bbox:')
        self.bbox_line_edit = QtWidgets.QLineEdit()
        self.bbox_line_edit.setText('0,0,1,1');
        self.bbox_line_edit.editingFinished.connect(self.transform_changed)
        form_layout1.addRow(bbox_label, self.bbox_line_edit)

        sphere_label = QtWidgets.QLabel('sphere:')
        self.sphere_line_edit = QtWidgets.QLineEdit()
        self.sphere_line_edit.setText('0,0');
        self.sphere_line_edit.editingFinished.connect(self.transform_changed)
        form_layout2.addRow(sphere_label, self.sphere_line_edit)

        quad_label = QtWidgets.QLabel('quad:')
        self.quad_line_edit = QtWidgets.QLineEdit()
        self.quad_line_edit.setText('0,0,0,0,0,0,0,0');
        self.quad_line_edit.editingFinished.connect(self.transform_changed)
        form_layout1.addRow(quad_label, self.quad_line_edit)

        result_group_box = QtWidgets.QGroupBox('result')
        result_layout = QtWidgets.QHBoxLayout()
        self.result_label = QtWidgets.QLabel()
        result_layout.addWidget(self.result_label)
        result_group_box.setLayout(result_layout)
        layout.addWidget(result_group_box)

        self.transform_changed()

        self.show_image_t = threading.Thread(target=self.show_image)
        self.show_image_t.start()

    def show_image(self):
        while True:
            data = self.frame_q.get()
            if data is None:
                break
            frame_id, frame = data
            with PIL.Image.fromarray(frame) as img:
                self.image_label.setPixmap(QtGui.QPixmap.fromImage(PIL.ImageQt.ImageQt(img)))
                success, part_id, part_bytes, result_img = decode_image.decode_image(img, self.row_num, self.col_num, self.transform, result_image=True)
                self.result_image_label.setPixmap(QtGui.QPixmap.fromImage(PIL.ImageQt.ImageQt(result_img)))
                self.result_label.setText('pass' if success else 'fail')

    def stop_show_image_thread(self):
        self.show_image_t.join()

    @QtCore.Slot()
    def transform_changed(self):
        transform = transform_utils.Transform()
        transform.binarization_threshold = int(self.binarization_threshold_line_edit.text())
        transform.filter_level = int(self.filter_level_line_edit.text())
        transform.bbox = transform_utils.parse_bbox(self.bbox_line_edit.text())
        transform.sphere = transform_utils.parse_sphere(self.sphere_line_edit.text())
        transform.quad = transform_utils.parse_quad(self.quad_line_edit.text())
        self.transform = transform

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
        frame_q.put((frame_id, frame))
        if frame_id % 10 == 0:
            try:
                stop_q.get(False)
                break
            except queue.Empty:
                pass
        frame_id += 1
        cv2.waitKey(100)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('dim', help='dim as row_num,col_num')
    args = parser.parse_args()

    row_num, col_num = decode_image.parse_dim(args.dim)

    frame_q = multiprocessing.Queue(1)
    stop_q = multiprocessing.Queue(1)

    capture_image_p = multiprocessing.Process(target=capture_image_worker, args=(frame_q, stop_q))
    capture_image_p.start()

    app = QtWidgets.QApplication([])
    widget = MyWidget(frame_q, row_num, col_num)
    widget.resize(1200, 650)
    widget.show()
    exit_v = app.exec()

    stop_q.put(None)
    capture_image_p.join()
    frame_q.put(None)
    widget.stop_show_image_thread()

    sys.exit(exit_v)
