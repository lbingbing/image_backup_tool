import os
import sys
import enum
import struct
import re
import time
import configparser

from PySide6 import QtCore, QtWidgets, QtGui

import image_codec_types
import symbol_codec
import image_decode_task
import server_utils
import image_decode_task_status_client

class Parameters:
    def __init__(self):
        self.tile_x_num_range = (1, 10)
        self.tile_y_num_range = (1, 10)
        self.tile_x_size_range = (1, 500)
        self.tile_y_size_range = (1, 500)
        self.pixel_size_range = (1, 100)
        self.space_size_range = (1, 10)
        self.calibration_pixel_size_range = (1, 10)
        self.interval_range = (1, 1000)

@enum.unique
class State(enum.Enum):
    CONFIG = 0
    CALIBRATE = 1
    DISPLAY = 2

class Context:
    def __init__(self):
        self.state = State.CONFIG
        self.symbol_type = None
        self.tile_x_num = None
        self.tile_y_num = None
        self.tile_x_size = None
        self.tile_y_size = None
        self.pixel_size = None
        self.space_size = None
        self.calibration_pixel_size = None
        self.task_status_server = None

@enum.unique
class CalibrationMode(enum.Enum):
    POSITION = 0
    COLOR = 1

class CalibrationPage(QtWidgets.QWidget):
    calibration_started = QtCore.Signal(CalibrationMode, list)
    calibration_stopped = QtCore.Signal()

    def __init__(self, parameters, context):
        super().__init__()

        self.parameters = parameters
        self.context = context

        layout = QtWidgets.QHBoxLayout(self)

        calibration_button = QtWidgets.QPushButton('calibrate')
        calibration_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        calibration_button.setFixedWidth(100)
        calibration_button.setCheckable(True)
        calibration_button.clicked.connect(self.toggle_calibration_start_stop)
        layout.addWidget(calibration_button)

        self.config_frame = QtWidgets.QFrame()
        self.config_frame.setFrameStyle(QtWidgets.QFrame.Box | QtWidgets.QFrame.Sunken)
        layout.addWidget(self.config_frame)

        config_frame_layout = QtWidgets.QHBoxLayout(self.config_frame)

        calibration_mode_label = QtWidgets.QLabel('calibration_mode')
        calibration_mode_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(calibration_mode_label)
        self.calibration_mode_combo_box = QtWidgets.QComboBox()
        self.calibration_mode_combo_box.addItem('position', CalibrationMode.POSITION)
        self.calibration_mode_combo_box.addItem('color', CalibrationMode.COLOR)
        self.calibration_mode_combo_box.setCurrentIndex(0)
        config_frame_layout.addWidget(self.calibration_mode_combo_box)

        symbol_type_label = QtWidgets.QLabel('symbol_type')
        symbol_type_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(symbol_type_label)
        self.symbol_type_combo_box = QtWidgets.QComboBox()
        for t in symbol_codec.SymbolType:
            self.symbol_type_combo_box.addItem(t.name.lower())
        self.symbol_type_combo_box.setCurrentIndex(self.context.symbol_type.value)
        config_frame_layout.addWidget(self.symbol_type_combo_box)

        tile_x_num_label = QtWidgets.QLabel('tile_x_num')
        tile_x_num_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(tile_x_num_label)
        self.tile_x_num_spin_box = QtWidgets.QSpinBox()
        self.tile_x_num_spin_box.setRange(*self.parameters.tile_x_num_range)
        self.tile_x_num_spin_box.setValue(self.context.tile_x_num)
        config_frame_layout.addWidget(self.tile_x_num_spin_box)

        tile_y_num_label = QtWidgets.QLabel('tile_y_num')
        tile_y_num_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(tile_y_num_label)
        self.tile_y_num_spin_box = QtWidgets.QSpinBox()
        self.tile_y_num_spin_box.setRange(*self.parameters.tile_y_num_range)
        self.tile_y_num_spin_box.setValue(self.context.tile_y_num)
        config_frame_layout.addWidget(self.tile_y_num_spin_box)

        tile_x_size_label = QtWidgets.QLabel('tile_x_size')
        tile_x_size_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(tile_x_size_label)
        self.tile_x_size_spin_box = QtWidgets.QSpinBox()
        self.tile_x_size_spin_box.setRange(*self.parameters.tile_x_size_range)
        self.tile_x_size_spin_box.setValue(self.context.tile_x_size)
        config_frame_layout.addWidget(self.tile_x_size_spin_box)

        tile_y_size_label = QtWidgets.QLabel('tile_y_size')
        tile_y_size_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(tile_y_size_label)
        self.tile_y_size_spin_box = QtWidgets.QSpinBox()
        self.tile_y_size_spin_box.setRange(*self.parameters.tile_y_size_range)
        self.tile_y_size_spin_box.setValue(self.context.tile_y_size)
        config_frame_layout.addWidget(self.tile_y_size_spin_box)

        pixel_size_label = QtWidgets.QLabel('pixel_size')
        pixel_size_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(pixel_size_label)
        self.pixel_size_spin_box = QtWidgets.QSpinBox()
        self.pixel_size_spin_box.setRange(*self.parameters.pixel_size_range)
        self.pixel_size_spin_box.setValue(self.context.pixel_size)
        config_frame_layout.addWidget(self.pixel_size_spin_box)

        space_size_label = QtWidgets.QLabel('space_size')
        space_size_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(space_size_label)
        self.space_size_spin_box = QtWidgets.QSpinBox()
        self.space_size_spin_box.setRange(*self.parameters.space_size_range)
        self.space_size_spin_box.setValue(self.context.space_size)
        config_frame_layout.addWidget(self.space_size_spin_box)

        calibration_pixel_size_label = QtWidgets.QLabel('calibration_pixel_size')
        calibration_pixel_size_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(calibration_pixel_size_label)
        self.calibration_pixel_size_spin_box = QtWidgets.QSpinBox()
        self.calibration_pixel_size_spin_box.setRange(*self.parameters.calibration_pixel_size_range)
        self.calibration_pixel_size_spin_box.setValue(self.context.calibration_pixel_size)
        config_frame_layout.addWidget(self.calibration_pixel_size_spin_box)

    def toggle_calibration_start_stop(self):
        if self.context.state == State.CONFIG:
            self.context.state = State.CALIBRATE
            self.config_frame.setEnabled(False)
            if self.calibration_mode_combo_box.currentIndex() == CalibrationMode.POSITION.value:
                self.calibration_started.emit(CalibrationMode.POSITION, [])
            elif self.calibration_mode_combo_box.currentIndex() == CalibrationMode.COLOR.value:
                codec = symbol_codec.create_symbol_codec(self.context.symbol_type)
                data = [(x + y + tile_x_id + tile_y_id) % codec.get_symbol_value_num() for tile_y_id in range(self.context.tile_y_num) for tile_x_id in range(self.context.tile_x_num) for y in range(self.context.tile_y_size) for x in range(self.context.tile_x_size)]
                self.calibration_started.emit(CalibrationMode.COLOR, data)
        elif self.context.state == State.CALIBRATE:
            self.context.state = State.CONFIG
            self.config_frame.setEnabled(True)
            self.calibration_stopped.emit()
        else:
            assert 0

@enum.unique
class DisplayMode(enum.Enum):
    MANUAL = 0
    AUTO = 1

class TaskPage(QtWidgets.QWidget):
    task_started = QtCore.Signal()
    part_navigated = QtCore.Signal(list)
    task_stopped = QtCore.Signal()

    def __init__(self, parameters, context):
        super().__init__()

        self.parameters = parameters
        self.context = context

        self.target_file_path = None
        self.raw_bytes = None
        self.part_byte_num = None
        self.part_num = None
        self.task_status_client = None
        self.task_status_auto_update_threshold = 200
        self.undone_part_ids = None
        self.cur_undone_part_id_index = None
        self.cur_part_id = None
        self.display_mode = DisplayMode.MANUAL
        self.interval = 50

        self.symbol_codec = None

        self.auto_navigate_update_fps_interval = int(2000 / self.interval)
        self.auto_navigate_frame_num = None
        self.auto_navigate_time = None
        self.auto_navigate_fps = None

        self.timer = QtCore.QTimer(self)

        layout = QtWidgets.QHBoxLayout(self)

        self.task_button = QtWidgets.QPushButton('start')
        self.task_button.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        self.task_button.setFixedWidth(100)
        self.task_button.setCheckable(True)
        self.task_button.clicked.connect(self.toggle_task_start_stop)
        layout.addWidget(self.task_button)

        control_frame_layout = QtWidgets.QVBoxLayout()
        layout.addLayout(control_frame_layout)

        self.task_frame = QtWidgets.QFrame()
        self.task_frame.setFrameStyle(QtWidgets.QFrame.Box | QtWidgets.QFrame.Sunken)
        control_frame_layout.addWidget(self.task_frame)

        task_frame_layout = QtWidgets.QVBoxLayout(self.task_frame)

        target_file_layout = QtWidgets.QHBoxLayout()
        task_frame_layout.addLayout(target_file_layout)

        target_file_label = QtWidgets.QLabel('target_file')
        target_file_layout.addWidget(target_file_label)
        self.target_file_line_edit = QtWidgets.QLineEdit()
        self.target_file_line_edit.setReadOnly(True)
        open_file_action = self.target_file_line_edit.addAction(qApp.style().standardIcon(QtWidgets.QStyle.SP_DirOpenIcon), QtWidgets.QLineEdit.TrailingPosition)
        open_file_action.triggered.connect(self.open_target_file)
        target_file_layout.addWidget(self.target_file_line_edit)

        task_layout = QtWidgets.QHBoxLayout()
        task_frame_layout.addLayout(task_layout)

        existing_task_checkbox = QtWidgets.QCheckBox('existing_task')
        existing_task_checkbox.stateChanged.connect(self.toggle_task_mode)
        task_layout.addWidget(existing_task_checkbox)

        task_layout1 = QtWidgets.QVBoxLayout()
        task_layout.addLayout(task_layout1)

        self.config_frame = QtWidgets.QFrame()
        self.config_frame.setFrameStyle(QtWidgets.QFrame.Box | QtWidgets.QFrame.Sunken)
        task_layout1.addWidget(self.config_frame)

        config_frame_layout = QtWidgets.QHBoxLayout(self.config_frame)

        symbol_type_label = QtWidgets.QLabel('symbol_type')
        symbol_type_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(symbol_type_label)
        self.symbol_type_combo_box = QtWidgets.QComboBox()
        for t in symbol_codec.SymbolType:
            self.symbol_type_combo_box.addItem(t.name.lower())
        self.symbol_type_combo_box.setCurrentIndex(self.context.symbol_type.value)
        config_frame_layout.addWidget(self.symbol_type_combo_box)

        tile_x_num_label = QtWidgets.QLabel('tile_x_num')
        tile_x_num_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(tile_x_num_label)
        self.tile_x_num_spin_box = QtWidgets.QSpinBox()
        self.tile_x_num_spin_box.setRange(*self.parameters.tile_x_num_range)
        self.tile_x_num_spin_box.setValue(self.context.tile_x_num)
        config_frame_layout.addWidget(self.tile_x_num_spin_box)

        tile_y_num_label = QtWidgets.QLabel('tile_y_num')
        tile_y_num_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(tile_y_num_label)
        self.tile_y_num_spin_box = QtWidgets.QSpinBox()
        self.tile_y_num_spin_box.setRange(*self.parameters.tile_y_num_range)
        self.tile_y_num_spin_box.setValue(self.context.tile_y_num)
        config_frame_layout.addWidget(self.tile_y_num_spin_box)

        tile_x_size_label = QtWidgets.QLabel('tile_x_size')
        tile_x_size_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(tile_x_size_label)
        self.tile_x_size_spin_box = QtWidgets.QSpinBox()
        self.tile_x_size_spin_box.setRange(*self.parameters.tile_x_size_range)
        self.tile_x_size_spin_box.setValue(self.context.tile_x_size)
        config_frame_layout.addWidget(self.tile_x_size_spin_box)

        tile_y_size_label = QtWidgets.QLabel('tile_y_size')
        tile_y_size_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(tile_y_size_label)
        self.tile_y_size_spin_box = QtWidgets.QSpinBox()
        self.tile_y_size_spin_box.setRange(*self.parameters.tile_y_size_range)
        self.tile_y_size_spin_box.setValue(self.context.tile_y_size)
        config_frame_layout.addWidget(self.tile_y_size_spin_box)

        pixel_size_label = QtWidgets.QLabel('pixel_size')
        pixel_size_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(pixel_size_label)
        self.pixel_size_spin_box = QtWidgets.QSpinBox()
        self.pixel_size_spin_box.setRange(*self.parameters.pixel_size_range)
        self.pixel_size_spin_box.setValue(self.context.pixel_size)
        config_frame_layout.addWidget(self.pixel_size_spin_box)

        space_size_label = QtWidgets.QLabel('space_size')
        space_size_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        config_frame_layout.addWidget(space_size_label)
        self.space_size_spin_box = QtWidgets.QSpinBox()
        self.space_size_spin_box.setRange(*self.parameters.space_size_range)
        self.space_size_spin_box.setValue(self.context.space_size)
        config_frame_layout.addWidget(self.space_size_spin_box)

        self.task_file_frame = QtWidgets.QFrame()
        self.task_file_frame.setFrameStyle(QtWidgets.QFrame.Box | QtWidgets.QFrame.Sunken)
        self.task_file_frame.setEnabled(False)
        task_layout1.addWidget(self.task_file_frame)

        task_file_frame_layout = QtWidgets.QHBoxLayout(self.task_file_frame)

        task_file_label = QtWidgets.QLabel('task_file')
        task_file_frame_layout.addWidget(task_file_label)
        self.task_file_line_edit = QtWidgets.QLineEdit()
        self.task_file_line_edit.setReadOnly(True)
        open_file_action = self.task_file_line_edit.addAction(qApp.style().standardIcon(QtWidgets.QStyle.SP_DirOpenIcon), QtWidgets.QLineEdit.TrailingPosition)
        open_file_action.triggered.connect(self.open_task_file)
        task_file_frame_layout.addWidget(self.task_file_line_edit)

        task_status_server_layout = QtWidgets.QHBoxLayout()
        task_frame_layout.addLayout(task_status_server_layout)

        task_status_server_type_label = QtWidgets.QLabel('task_status_server_type')
        task_status_server_type_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        task_status_server_layout.addWidget(task_status_server_type_label)
        self.task_status_server_type_combo_box = QtWidgets.QComboBox()
        for t in server_utils.ServerType:
            self.task_status_server_type_combo_box.addItem(t.name.lower())
        self.task_status_server_type_combo_box.setCurrentIndex(server_utils.ServerType.NONE.value)
        self.task_status_server_type_combo_box.currentIndexChanged.connect(lambda index: self.task_status_server_frame.setEnabled(index != server_utils.ServerType.NONE.value))
        task_status_server_layout.addWidget(self.task_status_server_type_combo_box)

        self.task_status_server_frame = QtWidgets.QFrame()
        self.task_status_server_frame.setFrameStyle(QtWidgets.QFrame.Box | QtWidgets.QFrame.Sunken)
        self.task_status_server_frame.setEnabled(False)
        task_status_server_layout.addWidget(self.task_status_server_frame)

        task_status_server_frame_layout = QtWidgets.QHBoxLayout(self.task_status_server_frame)

        task_status_server_label = QtWidgets.QLabel('server')
        task_status_server_label.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        task_status_server_frame_layout.addWidget(task_status_server_label)
        self.task_status_server_line_edit = QtWidgets.QLineEdit(self.context.task_status_server)
        self.task_status_server_line_edit.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        task_status_server_frame_layout.addWidget(self.task_status_server_line_edit)

        self.task_status_auto_update_checkbox = QtWidgets.QCheckBox('auto_update')
        self.task_status_auto_update_checkbox.setSizePolicy(QtWidgets.QSizePolicy.Fixed, QtWidgets.QSizePolicy.Preferred)
        task_status_server_frame_layout.addWidget(self.task_status_auto_update_checkbox)

        task_status_server_layout.addStretch(1)

        self.display_config_frame = QtWidgets.QFrame()
        self.display_config_frame.setFrameStyle(QtWidgets.QFrame.Box | QtWidgets.QFrame.Sunken)
        self.display_config_frame.setEnabled(False)
        control_frame_layout.addWidget(self.display_config_frame)

        display_config_frame_layout = QtWidgets.QHBoxLayout(self.display_config_frame)

        self.display_mode_button = QtWidgets.QPushButton()
        self.display_mode_button.setText('auto navigate')
        self.display_mode_button.setCheckable(True)
        self.display_mode_button.clicked.connect(self.toggle_display_mode)
        display_config_frame_layout.addWidget(self.display_mode_button)

        interval_label = QtWidgets.QLabel('interval')
        interval_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        display_config_frame_layout.addWidget(interval_label)
        self.interval_spin_box = QtWidgets.QSpinBox()
        self.interval_spin_box.setRange(*self.parameters.interval_range)
        self.interval_spin_box.setValue(self.interval)
        self.interval_spin_box.valueChanged.connect(self.set_interval)
        display_config_frame_layout.addWidget(self.interval_spin_box)

        auto_navigate_fps_label = QtWidgets.QLabel('auto_navigate_fps')
        auto_navigate_fps_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        display_config_frame_layout.addWidget(auto_navigate_fps_label)
        self.auto_navigate_fps_value_label = QtWidgets.QLabel('-')
        self.auto_navigate_fps_value_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        display_config_frame_layout.addWidget(self.auto_navigate_fps_value_label)

        cur_part_id_label = QtWidgets.QLabel('part_id')
        cur_part_id_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        display_config_frame_layout.addWidget(cur_part_id_label)
        self.cur_part_id_spin_box = QtWidgets.QSpinBox()
        self.cur_part_id_spin_box.setWrapping(True)
        self.cur_part_id_spin_box.valueChanged.connect(self.navigate_part)
        display_config_frame_layout.addWidget(self.cur_part_id_spin_box)
        self.max_part_id_label = QtWidgets.QLabel('-')
        self.max_part_id_label.setSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Preferred)
        self.max_part_id_label.setMinimumWidth(100)
        display_config_frame_layout.addWidget(self.max_part_id_label)

        undone_part_id_num_label = QtWidgets.QLabel('undone_part_id_num')
        undone_part_id_num_label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        display_config_frame_layout.addWidget(undone_part_id_num_label)
        self.undone_part_id_num_label = QtWidgets.QLabel('-')
        display_config_frame_layout.addWidget(self.undone_part_id_num_label)

    def open_target_file(self):
        file_path = QtWidgets.QFileDialog.getOpenFileName(self, 'Open File')
        if file_path[0]:
            self.target_file_line_edit.setText(file_path[0])
            self.target_file_path = file_path[0]

    def toggle_task_mode(self, state):
        if state == QtCore.Qt.Checked.value:
            self.config_frame.setEnabled(False)
            self.task_file_frame.setEnabled(True)
        else:
            self.config_frame.setEnabled(True)
            self.task_file_frame.setEnabled(False)

    def open_task_file(self):
        file_path = QtWidgets.QFileDialog.getOpenFileName(self, 'Open File', filter='Tasks (*.task)')
        if file_path[0]:
            task_file_path = file_path[0]
            self.task_file_line_edit.setText(task_file_path)
            task = image_decode_task.Task(task_file_path[:task_file_path.rindex('.task')])
            task.load()
            self.symbol_type_combo_box.setCurrentIndex(task.symbol_type.value)
            tile_x_num, tile_y_num, tile_x_size, tile_y_size = task.dim
            self.tile_x_num_spin_box.setValue(tile_x_num)
            self.tile_y_num_spin_box.setValue(tile_y_num)
            self.tile_x_size_spin_box.setValue(tile_x_size)
            self.tile_y_size_spin_box.setValue(tile_y_size)
            self.part_num = task.part_num
            self.undone_part_ids = [part_id for part_id in range(self.part_num) if not task.is_part_done(part_id)]
            assert self.undone_part_ids

    def toggle_task_start_stop(self):
        if self.context.state == State.CONFIG:
            self.symbol_codec = symbol_codec.create_symbol_codec(self.context.symbol_type)
            self.part_byte_num = image_decode_task.get_part_byte_num(self.context.symbol_type, (self.context.tile_x_num, self.context.tile_y_num, self.context.tile_x_size, self.context.tile_y_size))
            if self.validate_config():
                self.context.state = State.DISPLAY
                self.raw_bytes, part_num = image_decode_task.get_task_bytes(self.target_file_path, self.part_byte_num)
                if self.undone_part_ids:
                    assert part_num == self.part_num
                    self.cur_undone_part_id_index = 0
                    self.cur_part_id = self.undone_part_ids[self.cur_undone_part_id_index]
                    self.undone_part_id_num_label.setText(str(len(self.undone_part_ids)))
                else:
                    self.part_num = part_num
                    self.cur_part_id = 0
                    self.undone_part_id_num_label.setText(str(self.part_num))
                if self.is_task_status_server_on():
                    ip, port = server_utils.parse_server_addr(self.task_status_server_line_edit.text())
                    self.task_status_client = image_decode_task_status_client.create_task_status_client(server_utils.ServerType(self.task_status_server_type_combo_box.currentIndex()), ip, port)
                    if self.is_task_status_auto_update():
                        self.fetch_task_status()
                self.task_frame.setEnabled(False)
                self.display_config_frame.setEnabled(True)
                self.cur_part_id_spin_box.setRange(0, self.part_num - 1)
                self.cur_part_id_spin_box.setValue(self.cur_part_id)
                self.max_part_id_label.setText(str(self.part_num - 1))
                self.task_started.emit()
                self.draw(self.cur_part_id)
            else:
                self.task_button.setChecked(False)
        elif self.context.state == State.DISPLAY:
            self.context.state = State.CONFIG
            self.symbol_codec = None
            if self.display_mode == DisplayMode.AUTO:
                self.toggle_display_mode()
            self.task_frame.setEnabled(True)
            self.display_config_frame.setEnabled(False)
            self.max_part_id_label.setText('-')
            self.task_stopped.emit()
        else:
            assert 0

    def validate_config(self):
        valid = True
        if valid:
            if self.target_file_path is None:
                valid = False
                QtWidgets.QMessageBox.warning(self, 'Warning', 'please select target file')
            elif not os.path.isfile(self.target_file_path):
                valid = False
                QtWidgets.QMessageBox.warning(self, 'Warning', 'can\'t find target file \'{}\''.format(self.target_file_path))
        if valid:
            if self.part_byte_num < image_decode_task.Task.min_part_byte_num:
                valid = False
                QtWidgets.QMessageBox.warning(self, 'Warning', 'invalid part_byte_num \'{}\''.format(self.part_byte_num))
        if valid:
            if self.is_task_status_server_on():
                try:
                    server_utils.parse_server_addr(self.task_status_server_line_edit.text())
                except image_codec_types.InvalidImageCodecArgument:
                    valid = False
                    QtWidgets.QMessageBox.warning(self, 'Warning', 'invalid task status server \'{}\''.format(self.task_status_server_line_edit.text()))
        return valid

    def toggle_display_mode(self):
        if self.display_mode == DisplayMode.MANUAL:
            self.display_mode = DisplayMode.AUTO
            self.display_mode_button.setChecked(True)
            self.interval_spin_box.setEnabled(False)
            self.timer.timeout.connect(self.navigate_next_part)
            self.timer.start(self.interval)
            self.auto_navigate_frame_num = 0
            self.auto_navigate_time = time.time()
            self.auto_navigate_fps = 0
        else:
            self.display_mode = DisplayMode.MANUAL
            self.display_mode_button.setChecked(False)
            self.interval_spin_box.setEnabled(True)
            self.timer.stop()
            self.timer.timeout.disconnect()
            self.auto_navigate_frame_num = None
            self.auto_navigate_time = None
            self.auto_navigate_fps = None
            self.auto_navigate_fps_value_label.setText('-')

    def set_interval(self, interval):
        self.interval = interval

    def navigate_part(self, part_id):
        self.cur_part_id = part_id
        self.draw(part_id)

    def draw(self, part_id):
        part_bytes = bytes(self.raw_bytes[part_id*self.part_byte_num:(part_id+1)*self.part_byte_num])
        symbols = self.symbol_codec.encode(part_id, part_bytes, self.context.tile_x_num * self.context.tile_y_num * self.context.tile_x_size * self.context.tile_y_size)
        data = symbols
        self.part_navigated.emit(data)

    def navigate_next_part(self):
        if self.undone_part_ids:
            need_normal_navigate = True
            if self.is_task_status_auto_update() and len(self.undone_part_ids) > self.task_status_auto_update_threshold and self.cur_undone_part_id_index == len(self.undone_part_ids) - 1:
                need_normal_navigate = not self.update_task_status()
            if need_normal_navigate:
                self.cur_undone_part_id_index = self.cur_undone_part_id_index + 1 if self.cur_undone_part_id_index < len(self.undone_part_ids) - 1 else 0
                cur_part_id = self.undone_part_ids[self.cur_undone_part_id_index]
                self.cur_part_id_spin_box.setValue(cur_part_id)
        else:
            need_normal_navigate = True
            if self.is_task_status_auto_update() and self.part_num > self.task_status_auto_update_threshold and self.cur_part_id == self.part_num - 1:
                need_normal_navigate = not self.update_task_status()
            if need_normal_navigate:
                cur_part_id = self.cur_part_id + 1 if self.cur_part_id < self.part_num - 1 else 0
                self.cur_part_id_spin_box.setValue(cur_part_id)
        if self.display_mode == DisplayMode.AUTO:
            self.auto_navigate_frame_num += 1
            if self.auto_navigate_frame_num == self.auto_navigate_update_fps_interval:
                t = time.time()
                delta_t = max(t - self.auto_navigate_time, 0.001)
                self.auto_navigate_time = t
                fps = self.auto_navigate_frame_num / delta_t
                self.auto_navigate_fps = self.auto_navigate_fps * 0.5 + fps * 0.5
                self.auto_navigate_frame_num = 0
                self.auto_navigate_fps_value_label.setText('{:.2f}'.format(self.auto_navigate_fps))

    def navigate_prev_part(self):
        if self.undone_part_ids:
            self.cur_undone_part_id_index = self.cur_undone_part_id_index - 1 if self.cur_undone_part_id_index > 0 else len(self.undone_part_ids) - 1
            cur_part_id = self.undone_part_ids[self.cur_undone_part_id_index]
        else:
            cur_part_id = self.cur_part_id - 1 if self.cur_part_id > 0 else self.part_num - 1
        self.cur_part_id_spin_box.setValue(cur_part_id)

    def is_task_status_server_on(self):
        return self.task_status_server_type_combo_box.currentIndex() != server_utils.ServerType.NONE.value

    def is_task_status_auto_update(self):
        return self.task_status_auto_update_checkbox.checkState() == QtCore.Qt.Checked

    def fetch_task_status(self):
        task_bytes = self.task_status_client.get_task_status()
        if task_bytes:
            symbol_type, dim, part_num, done_part_num, task_status_bytes = image_decode_task.from_task_bytes(task_bytes)
            assert symbol_type == self.context.symbol_type
            assert dim == (self.context.tile_x_num, self.context.tile_y_num, self.context.tile_x_size, self.context.tile_y_size)
            assert part_num == self.part_num
            assert len(task_status_bytes) == (self.part_num + 7) // 8
            self.undone_part_ids = [part_id for part_id in range(self.part_num) if not image_decode_task.is_part_done(task_status_bytes, part_id)]
            self.cur_undone_part_id_index = 0
            self.cur_part_id = self.undone_part_ids[self.cur_undone_part_id_index]
            self.cur_part_id_spin_box.setValue(self.cur_part_id)
            self.undone_part_id_num_label.setText(str(len(self.undone_part_ids)))
            return True
        else:
            return False

    def update_task_status(self):
        if self.is_task_status_server_on():
            self.display_config_frame.setEnabled(False)
            if self.display_mode == DisplayMode.AUTO:
                self.timer.stop()
            success = self.fetch_task_status()
            if self.display_mode == DisplayMode.AUTO:
                self.timer.start(self.interval)
            self.display_config_frame.setEnabled(True)
            return success
        else:
            return False

class Canvas(QtWidgets.QWidget):
    def __init__(self, parameters, context):
        super().__init__()

        self.parameters = parameters
        self.context = context

        self.is_calibration = False
        self.calibration_mode = None
        self.data = None

    def draw_calibration(self, calibration_mode, data):
        self.is_calibration = True
        self.calibration_mode = calibration_mode
        self.data = data
        self.update()

    def draw_part(self, data):
        self.is_calibration = False
        self.data = data
        self.update()

    def clear(self):
        self.is_calibration = False
        self.data = None
        self.update()

    def is_tile_border(self, x, y):
        return y == 0 or y == self.context.tile_y_size + 1 or x == 0 or x == self.context.tile_x_size + 1

    def paintEvent(self, event):
        painter = QtGui.QPainter(self)
        painter.setPen(QtCore.Qt.NoPen)

        painter.setBrush(QtGui.QColor(255, 255, 255))
        painter.drawRect(self.rect())

        if self.is_calibration or self.data:
            painter.save()
            painter.setBrush(QtGui.QColor(0, 0, 0))
            painter.translate(
                (self.width() - (self.context.tile_x_num * (self.context.tile_x_size + 2) + (self.context.tile_x_num - 1) * self.context.space_size) * self.context.pixel_size) / 2,
                (self.height() - (self.context.tile_y_num * (self.context.tile_y_size + 2) + (self.context.tile_y_num - 1) * self.context.space_size) * self.context.pixel_size) / 2,
                )
            for tile_y_id in range(self.context.tile_y_num):
                tile_y = tile_y_id * (self.context.tile_y_size + 2 + self.context.space_size) * self.context.pixel_size
                for tile_x_id in range(self.context.tile_x_num):
                    tile_x = tile_x_id * (self.context.tile_x_size + 2 + self.context.space_size) * self.context.pixel_size
                    for y in range(self.context.tile_y_size + 2):
                        for x in range(self.context.tile_x_size + 2):
                            if self.is_calibration and self.calibration_mode == CalibrationMode.POSITION:
                                if not self.is_tile_border(x, y):
                                    painter.drawRect(tile_x + x * self.context.pixel_size + (self.context.pixel_size - self.context.calibration_pixel_size) / 2, tile_y + y * self.context.pixel_size + (self.context.pixel_size - self.context.calibration_pixel_size) / 2, self.context.calibration_pixel_size, self.context.calibration_pixel_size)
                            else:
                                if self.is_tile_border(x, y):
                                    painter.setBrush(QtGui.QColor(0, 0, 0))
                                else:
                                    symbol = self.data[((tile_y_id * self.context.tile_x_num + tile_x_id) * self.context.tile_y_size + (y - 1)) * self.context.tile_x_size + (x - 1)]
                                    if symbol == image_codec_types.PixelColor.WHITE.value:
                                        painter.setBrush(QtGui.QColor(255, 255, 255))
                                    elif symbol == image_codec_types.PixelColor.BLACK.value:
                                        painter.setBrush(QtGui.QColor(0, 0, 0))
                                    elif symbol == image_codec_types.PixelColor.RED.value:
                                        painter.setBrush(QtGui.QColor(255, 0, 0))
                                    elif symbol == image_codec_types.PixelColor.BLUE.value:
                                        painter.setBrush(QtGui.QColor(0, 0, 255))
                                    elif symbol == image_codec_types.PixelColor.GREEN.value:
                                        painter.setBrush(QtGui.QColor(0, 255, 0))
                                    elif symbol == image_codec_types.PixelColor.CYAN.value:
                                        painter.setBrush(QtGui.QColor(0, 255, 255))
                                    elif symbol == image_codec_types.PixelColor.MAGENTA.value:
                                        painter.setBrush(QtGui.QColor(255, 0, 255))
                                    elif symbol == image_codec_types.PixelColor.YELLOW.value:
                                        painter.setBrush(QtGui.QColor(255, 255, 0))
                                    else:
                                        assert 0, 'invalid symbol \'{}\''.format(symbol)
                                painter.drawRect(tile_x + x * self.context.pixel_size, tile_y + y * self.context.pixel_size, self.context.pixel_size, self.context.pixel_size)
            painter.restore()

class Widget(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()

        self.parameters = Parameters()
        self.context = Context()

        self.load_config()

        layout = QtWidgets.QVBoxLayout(self)
        layout.setSpacing(0)
        layout.setContentsMargins(0, 0, 0, 0)

        self.calibration_page = CalibrationPage(self.parameters, self.context)
        self.task_page = TaskPage(self.parameters, self.context)

        self.calibration_page.symbol_type_combo_box.currentIndexChanged.connect(self.task_page.symbol_type_combo_box.setCurrentIndex)
        self.calibration_page.symbol_type_combo_box.currentIndexChanged.connect(self.set_symbol_type)
        self.calibration_page.tile_x_num_spin_box.valueChanged.connect(self.task_page.tile_x_num_spin_box.setValue)
        self.calibration_page.tile_x_num_spin_box.valueChanged.connect(self.set_tile_x_num)
        self.calibration_page.tile_y_num_spin_box.valueChanged.connect(self.task_page.tile_y_num_spin_box.setValue)
        self.calibration_page.tile_y_num_spin_box.valueChanged.connect(self.set_tile_y_num)
        self.calibration_page.tile_x_size_spin_box.valueChanged.connect(self.task_page.tile_x_size_spin_box.setValue)
        self.calibration_page.tile_x_size_spin_box.valueChanged.connect(self.set_tile_x_size)
        self.calibration_page.tile_y_size_spin_box.valueChanged.connect(self.task_page.tile_y_size_spin_box.setValue)
        self.calibration_page.tile_y_size_spin_box.valueChanged.connect(self.set_tile_y_size)
        self.calibration_page.pixel_size_spin_box.valueChanged.connect(self.task_page.pixel_size_spin_box.setValue)
        self.calibration_page.pixel_size_spin_box.valueChanged.connect(self.set_pixel_size)
        self.calibration_page.space_size_spin_box.valueChanged.connect(self.task_page.space_size_spin_box.setValue)
        self.calibration_page.space_size_spin_box.valueChanged.connect(self.set_space_size)
        self.calibration_page.calibration_pixel_size_spin_box.valueChanged.connect(self.set_calibration_pixel_size)
        self.task_page.symbol_type_combo_box.currentIndexChanged.connect(self.calibration_page.symbol_type_combo_box.setCurrentIndex)
        self.task_page.symbol_type_combo_box.currentIndexChanged.connect(self.set_symbol_type)
        self.task_page.tile_x_num_spin_box.valueChanged.connect(self.calibration_page.tile_x_num_spin_box.setValue)
        self.task_page.tile_x_num_spin_box.valueChanged.connect(self.set_tile_x_num)
        self.task_page.tile_y_num_spin_box.valueChanged.connect(self.calibration_page.tile_y_num_spin_box.setValue)
        self.task_page.tile_y_num_spin_box.valueChanged.connect(self.set_tile_y_num)
        self.task_page.tile_x_size_spin_box.valueChanged.connect(self.calibration_page.tile_x_size_spin_box.setValue)
        self.task_page.tile_x_size_spin_box.valueChanged.connect(self.set_tile_x_size)
        self.task_page.tile_y_size_spin_box.valueChanged.connect(self.calibration_page.tile_y_size_spin_box.setValue)
        self.task_page.tile_y_size_spin_box.valueChanged.connect(self.set_tile_y_size)
        self.task_page.pixel_size_spin_box.valueChanged.connect(self.calibration_page.pixel_size_spin_box.setValue)
        self.task_page.pixel_size_spin_box.valueChanged.connect(self.set_pixel_size)
        self.task_page.space_size_spin_box.valueChanged.connect(self.calibration_page.space_size_spin_box.setValue)
        self.task_page.space_size_spin_box.valueChanged.connect(self.set_space_size)

        self.control_tab = QtWidgets.QTabWidget()
        self.control_tab.setSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Fixed)
        self.control_tab.addTab(self.calibration_page, 'calibration')
        self.control_tab.addTab(self.task_page, 'task')
        layout.addWidget(self.control_tab)

        self.canvas = Canvas(self.parameters, self.context)
        self.canvas.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.canvas.setMinimumSize(300, 300)
        self.canvas.setFocusPolicy(QtCore.Qt.ClickFocus)
        self.canvas.installEventFilter(self)
        layout.addWidget(self.canvas)

        self.calibration_page.calibration_started.connect(self.start_calibration)
        self.calibration_page.calibration_stopped.connect(self.stop_calibration)
        self.task_page.task_started.connect(self.start_task)
        self.task_page.part_navigated.connect(self.canvas.draw_part)
        self.task_page.task_stopped.connect(self.stop_task)

    def load_config(self):
        config = configparser.ConfigParser()
        config.read('display_qt.ini')
        self.context.symbol_type = symbol_codec.SymbolType[config.get('DEFAULT', 'symbol_type').upper()]
        self.context.tile_x_num = config.getint('DEFAULT', 'tile_x_num')
        self.context.tile_y_num = config.getint('DEFAULT', 'tile_y_num')
        self.context.tile_x_size = config.getint('DEFAULT', 'tile_x_size')
        self.context.tile_y_size = config.getint('DEFAULT', 'tile_y_size')
        self.context.pixel_size = config.getint('DEFAULT', 'pixel_size')
        self.context.space_size = config.getint('DEFAULT', 'space_size')
        self.context.calibration_pixel_size = config.getint('DEFAULT', 'calibration_pixel_size')
        self.context.task_status_server = config.get('DEFAULT', 'task_status_server')

    def set_symbol_type(self, index):
        self.context.symbol_type = symbol_codec.SymbolType(index)

    def set_tile_x_num(self, tile_x_num):
        self.context.tile_x_num = tile_x_num

    def set_tile_y_num(self, tile_y_num):
        self.context.tile_y_num = tile_y_num

    def set_tile_x_size(self, tile_x_size):
        self.context.tile_x_size = tile_x_size

    def set_tile_y_size(self, tile_y_size):
        self.context.tile_y_size = tile_y_size

    def set_pixel_size(self, pixel_size):
        self.context.pixel_size = pixel_size

    def set_space_size(self, space_size):
        self.context.space_size = space_size

    def set_calibration_pixel_size(self, calibration_pixel_size):
        self.context.calibration_pixel_size = calibration_pixel_size

    def start_calibration(self, calibration_mode, data):
        self.control_tab.setTabEnabled(1, False)
        self.canvas.draw_calibration(calibration_mode, data)

    def stop_calibration(self):
        self.control_tab.setTabEnabled(1, True)
        self.canvas.clear()

    def start_task(self):
        self.control_tab.setTabEnabled(0, False)

    def stop_task(self):
        self.control_tab.setTabEnabled(0, True)
        self.canvas.clear()

    def toggle_full_screen(self):
        self.control_tab.setVisible(not self.control_tab.isVisible())
        if self.isFullScreen():
            self.showNormal()
        else:
            self.showFullScreen()

    def eventFilter(self, obj, event):
        if self.context.state == State.DISPLAY:
            if event.type() == QtCore.QEvent.KeyPress:
                if event.key() == QtCore.Qt.Key_F:
                    self.toggle_full_screen()
                elif event.key() == QtCore.Qt.Key_Space:
                    self.task_page.toggle_display_mode()
                elif event.key() == QtCore.Qt.Key_Left:
                    self.task_page.navigate_prev_part()
                elif event.key() == QtCore.Qt.Key_Right:
                    self.task_page.navigate_next_part()
                elif event.key() == QtCore.Qt.Key_T:
                    self.task_page.update_task_status()
        elif self.context.state == State.CALIBRATE:
            if event.type() == QtCore.QEvent.KeyPress:
                if event.key() == QtCore.Qt.Key_F:
                    self.toggle_full_screen()
        return super().eventFilter(obj, event)

if __name__ == '__main__':
    app = QtWidgets.QApplication([])
    widget = Widget()
    widget.show()
    sys.exit(app.exec())
