#include <cassert>
#include <sstream>
#include <fstream>
#include <filesystem>

#include <boost/program_options.hpp>

#include <QtWidgets/QApplication>
#include <QtWidgets/QHboxLayout>
#include <QtWidgets/QVboxLayout>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtGui/QAction>
#include <QtGui/QPainter>
#include <QtGui/QKeyEvent>

#include "Widget.h"

CalibrationPage::CalibrationPage(QWidget* parent, const Parameters& parameters, Context& context) : QWidget(parent), m_parameters(parameters), m_context(context) {
    auto layout = new QHBoxLayout(this);

    auto calibration_button = new QPushButton("calibrate");
    calibration_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    calibration_button->setFixedWidth(100);
    calibration_button->setCheckable(true);
    connect(calibration_button, &QPushButton::clicked, this, &CalibrationPage::ToggleCalibrationStartStop);
    layout->addWidget(calibration_button);

    m_config_frame = new QFrame();
    m_config_frame->setFrameStyle(QFrame::Box | QFrame::Sunken);
    layout->addWidget(m_config_frame);

    auto config_frame_layout = new QHBoxLayout(m_config_frame);

    auto calibration_mode_label = new QLabel("calibration_mode");
    calibration_mode_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(calibration_mode_label);
    m_calibration_mode_combo_box = new QComboBox();
    m_calibration_mode_combo_box->addItem("position");
    m_calibration_mode_combo_box->addItem("color");
    m_calibration_mode_combo_box->setCurrentIndex(0);
    config_frame_layout->addWidget(m_calibration_mode_combo_box);

    auto symbol_type_label = new QLabel("symbol_type");
    symbol_type_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(symbol_type_label);
    m_symbol_type_combo_box = new QComboBox();
    for (int i = static_cast<int>(SymbolType::SYMBOL1); i < static_cast<int>(SymbolType::SYMBOL3); ++i) {
        m_symbol_type_combo_box->addItem(get_symbol_type_str(static_cast<SymbolType>(i)).c_str());
    }
    m_symbol_type_combo_box->setCurrentIndex(static_cast<int>(m_context.symbol_type));
    config_frame_layout->addWidget(m_symbol_type_combo_box);

    auto tile_x_num_label = new QLabel("tile_x_num");
    tile_x_num_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(tile_x_num_label);
    m_tile_x_num_spin_box = new QSpinBox();
    m_tile_x_num_spin_box->setRange(m_parameters.tile_x_num_range[0], m_parameters.tile_x_num_range[1]);
    m_tile_x_num_spin_box->setValue(m_context.tile_x_num);
    config_frame_layout->addWidget(m_tile_x_num_spin_box);

    auto tile_y_num_label = new QLabel("tile_y_num");
    tile_y_num_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(tile_y_num_label);
    m_tile_y_num_spin_box = new QSpinBox();
    m_tile_y_num_spin_box->setRange(m_parameters.tile_y_num_range[0], m_parameters.tile_y_num_range[1]);
    m_tile_y_num_spin_box->setValue(m_context.tile_y_num);
    config_frame_layout->addWidget(m_tile_y_num_spin_box);

    auto tile_x_size_label = new QLabel("tile_x_size");
    tile_x_size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(tile_x_size_label);
    m_tile_x_size_spin_box = new QSpinBox();
    m_tile_x_size_spin_box->setRange(m_parameters.tile_x_size_range[0], m_parameters.tile_x_size_range[1]);
    m_tile_x_size_spin_box->setValue(m_context.tile_x_size);
    config_frame_layout->addWidget(m_tile_x_size_spin_box);

    auto tile_y_size_label = new QLabel("tile_y_size");
    tile_y_size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(tile_y_size_label);
    m_tile_y_size_spin_box = new QSpinBox();
    m_tile_y_size_spin_box->setRange(m_parameters.tile_y_size_range[0], m_parameters.tile_y_size_range[1]);
    m_tile_y_size_spin_box->setValue(m_context.tile_y_size);
    config_frame_layout->addWidget(m_tile_y_size_spin_box);

    auto pixel_size_label = new QLabel("pixel_size");
    pixel_size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(pixel_size_label);
    m_pixel_size_spin_box = new QSpinBox();
    m_pixel_size_spin_box->setRange(m_parameters.pixel_size_range[0], m_parameters.pixel_size_range[1]);
    m_pixel_size_spin_box->setValue(m_context.pixel_size);
    config_frame_layout->addWidget(m_pixel_size_spin_box);

    auto space_size_label = new QLabel("space_size");
    space_size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(space_size_label);
    m_space_size_spin_box = new QSpinBox();
    m_space_size_spin_box->setRange(m_parameters.space_size_range[0], m_parameters.space_size_range[1]);
    m_space_size_spin_box->setValue(m_context.space_size);
    config_frame_layout->addWidget(m_space_size_spin_box);

    auto calibration_pixel_size_label = new QLabel("calibration_pixel_size");
    calibration_pixel_size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(calibration_pixel_size_label);
    m_calibration_pixel_size_spin_box = new QSpinBox();
    m_calibration_pixel_size_spin_box->setRange(m_parameters.calibration_pixel_size_range[0], m_parameters.calibration_pixel_size_range[1]);
    m_calibration_pixel_size_spin_box->setValue(m_context.calibration_pixel_size);
    config_frame_layout->addWidget(m_calibration_pixel_size_spin_box);
}

void CalibrationPage::ToggleCalibrationStartStop() {
    if (m_context.state == State::CONFIG) {
        m_context.state = State::CALIBRATE;
        m_config_frame->setEnabled(false);
        if (m_calibration_mode_combo_box->currentIndex() == static_cast<int>(CalibrationMode::POSITION)) {
            emit CalibrationStarted(CalibrationMode::POSITION, std::vector<uint8_t>());
        } else if (m_calibration_mode_combo_box->currentIndex() == static_cast<int>(CalibrationMode::COLOR)) {
            auto symbol_codec = create_symbol_codec(m_context.symbol_type);
            std::vector<uint8_t> data;
            for (int tile_y_id = 0; tile_y_id < m_context.tile_y_num; ++tile_y_id) {
                for (int tile_x_id = 0; tile_x_id < m_context.tile_x_num; ++tile_x_id) {
                    for (int y = 0; y < m_context.tile_y_size; ++y) {
                        for (int x = 0; x < m_context.tile_x_size; ++x) {
                            data.push_back((x + y + tile_x_id + tile_y_id) % symbol_codec->SymbolValueNum());
                        }
                    }
                }
            }
            emit CalibrationStarted(CalibrationMode::COLOR, data);
        }
    } else if (m_context.state == State::CALIBRATE) {
        m_context.state = State::CONFIG;
        m_config_frame->setEnabled(true);
        emit CalibrationStopped();
    } else {
        assert(false);
    }
}

TaskPage::TaskPage(QWidget* parent, const Parameters& parameters, Context& context) : QWidget(parent), m_parameters(parameters), m_context(context), m_timer(this) {
    auto layout = new QHBoxLayout(this);

    m_task_button = new QPushButton("start");
    m_task_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_task_button->setFixedWidth(100);
    m_task_button->setCheckable(true);
    connect(m_task_button, &QPushButton::clicked, this, &TaskPage::ToggleTaskStartStop);
    layout->addWidget(m_task_button);

    auto control_frame_layout = new QVBoxLayout();
    layout->addLayout(control_frame_layout);

    m_task_frame = new QFrame();
    m_task_frame->setFrameStyle(QFrame::Box | QFrame::Sunken);
    control_frame_layout->addWidget(m_task_frame);

    auto task_frame_layout = new QVBoxLayout(m_task_frame);

    auto target_file_layout = new QHBoxLayout();
    task_frame_layout->addLayout(target_file_layout);

    auto target_file_label = new QLabel("target_file");
    target_file_layout->addWidget(target_file_label);
    m_target_file_line_edit = new QLineEdit();
    m_target_file_line_edit->setReadOnly(true);
    auto open_file_action = m_target_file_line_edit->addAction(QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon), QLineEdit::TrailingPosition);
    connect(open_file_action, &QAction::triggered, this, &TaskPage::OpenTargetFile);
    target_file_layout->addWidget(m_target_file_line_edit);

    auto task_layout = new QHBoxLayout();
    task_frame_layout->addLayout(task_layout);

    auto existing_task_checkbox = new QCheckBox("existing_task");
    connect(existing_task_checkbox, &QCheckBox::stateChanged, this, &TaskPage::ToggleTaskMode);
    task_layout->addWidget(existing_task_checkbox);

    auto task_layout1 = new QVBoxLayout();
    task_layout->addLayout(task_layout1);

    m_config_frame = new QFrame();
    m_config_frame->setFrameStyle(QFrame::Box | QFrame::Sunken);
    task_layout1->addWidget(m_config_frame);

    auto config_frame_layout = new QHBoxLayout(m_config_frame);

    auto symbol_type_label = new QLabel("symbol_type");
    symbol_type_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(symbol_type_label);
    m_symbol_type_combo_box = new QComboBox();
    for (int i = static_cast<int>(SymbolType::SYMBOL1); i < static_cast<int>(SymbolType::SYMBOL3); ++i) {
        m_symbol_type_combo_box->addItem(get_symbol_type_str(static_cast<SymbolType>(i)).c_str());
    }
    m_symbol_type_combo_box->setCurrentIndex(static_cast<int>(m_context.symbol_type));
    config_frame_layout->addWidget(m_symbol_type_combo_box);

    auto tile_x_num_label = new QLabel("tile_x_num");
    tile_x_num_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(tile_x_num_label);
    m_tile_x_num_spin_box = new QSpinBox();
    m_tile_x_num_spin_box->setRange(m_parameters.tile_x_num_range[0], m_parameters.tile_x_num_range[1]);
    m_tile_x_num_spin_box->setValue(m_context.tile_x_num);
    config_frame_layout->addWidget(m_tile_x_num_spin_box);

    auto tile_y_num_label = new QLabel("tile_y_num");
    tile_y_num_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(tile_y_num_label);
    m_tile_y_num_spin_box = new QSpinBox();
    m_tile_y_num_spin_box->setRange(m_parameters.tile_y_num_range[0], m_parameters.tile_y_num_range[1]);
    m_tile_y_num_spin_box->setValue(m_context.tile_y_num);
    config_frame_layout->addWidget(m_tile_y_num_spin_box);

    auto tile_x_size_label = new QLabel("tile_x_size");
    tile_x_size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(tile_x_size_label);
    m_tile_x_size_spin_box = new QSpinBox();
    m_tile_x_size_spin_box->setRange(m_parameters.tile_x_size_range[0], m_parameters.tile_x_size_range[1]);
    m_tile_x_size_spin_box->setValue(m_context.tile_x_size);
    config_frame_layout->addWidget(m_tile_x_size_spin_box);

    auto tile_y_size_label = new QLabel("tile_y_size");
    tile_y_size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(tile_y_size_label);
    m_tile_y_size_spin_box = new QSpinBox();
    m_tile_y_size_spin_box->setRange(m_parameters.tile_y_size_range[0], m_parameters.tile_y_size_range[1]);
    m_tile_y_size_spin_box->setValue(m_context.tile_y_size);
    config_frame_layout->addWidget(m_tile_y_size_spin_box);

    auto pixel_size_label = new QLabel("pixel_size");
    pixel_size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(pixel_size_label);
    m_pixel_size_spin_box = new QSpinBox();
    m_pixel_size_spin_box->setRange(m_parameters.pixel_size_range[0], m_parameters.pixel_size_range[1]);
    m_pixel_size_spin_box->setValue(m_context.pixel_size);
    config_frame_layout->addWidget(m_pixel_size_spin_box);

    auto space_size_label = new QLabel("space_size");
    space_size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    config_frame_layout->addWidget(space_size_label);
    m_space_size_spin_box = new QSpinBox();
    m_space_size_spin_box->setRange(m_parameters.space_size_range[0], m_parameters.space_size_range[1]);
    m_space_size_spin_box->setValue(m_context.space_size);
    config_frame_layout->addWidget(m_space_size_spin_box);

    m_task_file_frame = new QFrame();
    m_task_file_frame->setFrameStyle(QFrame::Box | QFrame::Sunken);
    m_task_file_frame->setEnabled(false);
    task_layout1->addWidget(m_task_file_frame);

    auto task_file_frame_layout = new QHBoxLayout(m_task_file_frame);

    auto task_file_label = new QLabel("task_file");
    task_file_frame_layout->addWidget(task_file_label);
    m_task_file_line_edit = new QLineEdit();
    m_task_file_line_edit->setReadOnly(true);
    open_file_action = m_task_file_line_edit->addAction(QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon), QLineEdit::TrailingPosition);
    connect(open_file_action, &QAction::triggered, this, &TaskPage::OpenTaskFile);
    task_file_frame_layout->addWidget(m_task_file_line_edit);

    auto task_status_server_layout = new QHBoxLayout();
    task_frame_layout->addLayout(task_status_server_layout);

    auto task_status_server_checkbox = new QCheckBox("task_status_server");
    task_status_server_checkbox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    task_status_server_checkbox->setFixedWidth(140);
    connect(task_status_server_checkbox, &QCheckBox::stateChanged, this, &TaskPage::ToggleTaskStatusServer);
    task_status_server_layout->addWidget(task_status_server_checkbox);

    m_task_status_server_frame = new QFrame();
    m_task_status_server_frame->setFrameStyle(QFrame::Box | QFrame::Sunken);
    task_status_server_layout->addWidget(m_task_status_server_frame);

    auto task_status_server_frame_layout = new QHBoxLayout(m_task_status_server_frame);

    auto task_status_server_label = new QLabel("server");
    task_status_server_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    task_status_server_label->setFixedWidth(50);
    task_status_server_frame_layout->addWidget(task_status_server_label);

    m_task_status_server_line_edit = new QLineEdit(m_context.task_status_server.c_str());
    m_task_status_server_line_edit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_task_status_server_line_edit->setFixedWidth(140);
    task_status_server_frame_layout->addWidget(m_task_status_server_line_edit);

    auto task_status_auto_update_checkbox = new QCheckBox("auto_update");
    task_status_auto_update_checkbox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    task_status_auto_update_checkbox->setFixedWidth(100);
    connect(task_status_auto_update_checkbox, &QCheckBox::stateChanged, this, &TaskPage::ToggleTaskStatusAutoUpdate);
    task_status_server_frame_layout->addWidget(task_status_auto_update_checkbox);

    task_status_server_layout->addStretch(1);

    m_display_config_frame = new QFrame();
    m_display_config_frame->setFrameStyle(QFrame::Box | QFrame::Sunken);
    m_display_config_frame->setEnabled(false);
    control_frame_layout->addWidget(m_display_config_frame);

    auto display_config_frame_layout = new QHBoxLayout(m_display_config_frame);

    m_display_mode_button = new QPushButton("auto navigate");
    m_display_mode_button->setCheckable(true);
    connect(m_display_mode_button, &QPushButton::clicked, this, &TaskPage::ToggleDisplayMode);
    display_config_frame_layout->addWidget(m_display_mode_button);

    auto interval_label = new QLabel("interval");
    interval_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    display_config_frame_layout->addWidget(interval_label);
    m_interval_spin_box = new QSpinBox();
    m_interval_spin_box->setRange(m_parameters.interval_range[0], m_parameters.interval_range[1]);
    m_interval_spin_box->setValue(m_interval);
    connect(m_interval_spin_box, &QSpinBox::valueChanged, this, &TaskPage::SetInterval);
    display_config_frame_layout->addWidget(m_interval_spin_box);

    auto auto_navigate_fps_label = new QLabel("auto_navigate_fps");
    auto_navigate_fps_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    display_config_frame_layout->addWidget(auto_navigate_fps_label);
    m_auto_navigate_fps_value_label = new QLabel("-");
    m_auto_navigate_fps_value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_auto_navigate_fps_value_label->setFixedWidth(50);
    display_config_frame_layout->addWidget(m_auto_navigate_fps_value_label);

    auto cur_part_id_label = new QLabel("part_id");
    cur_part_id_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    display_config_frame_layout->addWidget(cur_part_id_label);
    m_cur_part_id_spin_box = new QSpinBox();
    m_cur_part_id_spin_box->setWrapping(true);
    connect(m_cur_part_id_spin_box, &QSpinBox::valueChanged, this, &TaskPage::NavigatePart);
    display_config_frame_layout->addWidget(m_cur_part_id_spin_box);
    m_max_part_id_label = new QLabel("-");
    m_max_part_id_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_max_part_id_label->setMinimumWidth(100);
    display_config_frame_layout->addWidget(m_max_part_id_label);

    auto undone_part_id_num_label = new QLabel("undone_part_id_num");
    undone_part_id_num_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    display_config_frame_layout->addWidget(undone_part_id_num_label);
    m_undone_part_id_num_label = new QLabel("-");
    display_config_frame_layout->addWidget(m_undone_part_id_num_label);
}

void TaskPage::OpenTargetFile() {
    auto file_path = QFileDialog::getOpenFileName(this, "Open File");
    if (!file_path.isEmpty()) {
        m_target_file_line_edit->setText(file_path);
        m_target_file_path = file_path.toStdString();
    }
}

void TaskPage::ToggleTaskMode(int state) {
    if (state == Qt::Checked) {
        m_config_frame->setEnabled(false);
        m_task_file_frame->setEnabled(true);
    } else {
        m_config_frame->setEnabled(true);
        m_task_file_frame->setEnabled(false);
    }
}

void TaskPage::ToggleTaskStatusServer(int state) {
    if (state == Qt::Checked) {
        m_task_status_server_on = true;
        m_task_status_server_frame->setEnabled(false);
    } else {
        m_task_status_server_on = false;
        m_task_status_server_frame->setEnabled(true);
    }
}

void TaskPage::ToggleTaskStatusAutoUpdate(int state) {
    if (state == Qt::Checked) {
        m_task_status_auto_update = true;
    } else {
        m_task_status_auto_update = false;
    }
}

void TaskPage::OpenTaskFile() {
    auto file_path = QFileDialog::getOpenFileName(this, "Open File", "Tasks (*.task)");
    if (!file_path.isEmpty()) {
        auto task_file_path = file_path.toStdString();
        m_task_file_line_edit->setText(file_path);
        Task task(task_file_path.substr(0, task_file_path.rfind(".task")));
        task.Load();
        m_symbol_type_combo_box->setCurrentIndex(static_cast<int>(task.GetSymbolType()));
        auto [tile_x_num, tile_y_num, tile_x_size, tile_y_size] = task.GetDim();
        m_tile_x_num_spin_box->setValue(tile_x_num);
        m_tile_y_num_spin_box->setValue(tile_y_num);
        m_tile_x_size_spin_box->setValue(tile_x_size);
        m_tile_y_size_spin_box->setValue(tile_y_size);
        m_part_num = task.GetPartNum();
        m_undone_part_ids.clear();
        for (uint32_t part_id = 0; part_id < m_part_num; ++part_id) {
            if (!task.IsPartDone(part_id)) {
                m_undone_part_ids.push_back(part_id);
            }
        }
        assert(!m_undone_part_ids.empty());
    }
}

void TaskPage::ToggleTaskStartStop() {
    if (m_context.state == State::CONFIG) {
        m_symbol_codec = create_symbol_codec(m_context.symbol_type);
        m_part_byte_num = get_part_byte_num(m_context.symbol_type, {m_context.tile_x_num, m_context.tile_y_num, m_context.tile_x_size, m_context.tile_y_size});
        if (ValidateConfig()) {
            m_context.state = State::DISPLAY;
            auto res = get_task_bytes(m_target_file_path, m_part_byte_num);
            m_raw_bytes = std::move(std::get<0>(res));
            auto part_num = std::get<1>(res);
            if (!m_undone_part_ids.empty()) {
                assert(part_num == m_part_num);
                m_cur_undone_part_id_index = 0;
                m_cur_part_id = m_undone_part_ids[m_cur_undone_part_id_index];
                m_undone_part_id_num_label->setText(std::to_string(m_undone_part_ids.size()).c_str());
            } else {
                m_part_num = part_num;
                m_cur_part_id = 0;
                m_undone_part_id_num_label->setText(std::to_string(m_part_num).c_str());
            }
            if (m_task_status_server_on) {
                auto task_status_server_text = m_task_status_server_line_edit->text().toStdString();
                std::smatch match;
                bool success = std::regex_match(task_status_server_text, match, m_task_status_server_pattern);
                assert(success);
                auto ip = match[1].str();
                auto port = std::stoi(match[2].str());
                m_task_status_client = std::make_unique<TaskStatusClient>(ip, port);
                if (m_task_status_auto_update) {
                    FetchTaskStatus();
                }
            }
            m_task_frame->setEnabled(false);
            m_display_config_frame->setEnabled(true);
            m_cur_part_id_spin_box->setRange(0, m_part_num - 1);
            m_cur_part_id_spin_box->setValue(m_cur_part_id);
            m_max_part_id_label->setText(std::to_string(m_part_num - 1).c_str());
            emit TaskStarted();
            Draw(m_cur_part_id);
        } else {
            m_task_button->setChecked(false);
        }
    } else if (m_context.state == State::DISPLAY) {
        m_context.state = State::CONFIG;
        m_symbol_codec.reset();
        if (m_display_mode == DisplayMode::AUTO) {
            ToggleDisplayMode();
        }
        m_task_frame->setEnabled(true);
        m_display_config_frame->setEnabled(false);
        m_max_part_id_label->setText("-");
        emit TaskStopped();
    } else {
        assert(false);
    }
}

bool TaskPage::ValidateConfig() {
    bool valid = true;
    if (valid) {
        if (m_target_file_path.empty()) {
            valid = false;
            QMessageBox::warning(this, "Warning", "please select target file");
        } else if (!std::filesystem::is_regular_file(m_target_file_path)) {
            valid = false;
            QMessageBox::warning(this, "Warning", std::string("can't find target file '" + m_target_file_path + "'").c_str());
        }
    }
    if (valid) {
        if (m_part_byte_num < Task::MIN_PART_BYTE_NUM) {
            valid = false;
            QMessageBox::warning(this, "Warning", std::string("invalid part_byte_num '" + std::to_string(m_part_byte_num) + "'").c_str());
        }
    }
    if (valid) {
        if (m_task_status_server_on) {
            auto task_status_server_text = m_task_status_server_line_edit->text().toStdString();
            std::smatch match;
            bool success = std::regex_match(task_status_server_text, match, m_task_status_server_pattern);
            if (!success) {
                valid = false;
                QMessageBox::warning(this, "Warning", std::string("invalid task status server '" + m_task_status_server_line_edit->text().toStdString() + "'").c_str());
            }
        }
    }
    return valid;
}

void TaskPage::ToggleDisplayMode() {
    if (m_display_mode == DisplayMode::MANUAL) {
        m_display_mode = DisplayMode::AUTO;
        m_display_mode_button->setChecked(true);
        m_interval_spin_box->setEnabled(false);
        connect(&m_timer, &QTimer::timeout, this, &TaskPage::NavigateNextPart);
        m_timer.start(m_interval);
        m_auto_navigate_frame_num = 0;
        m_auto_navigate_time = std::chrono::high_resolution_clock::now();
        m_auto_navigate_fps = 0;
    } else {
        m_display_mode = DisplayMode::MANUAL;
        m_display_mode_button->setChecked(false);
        m_interval_spin_box->setEnabled(true);
        m_timer.stop();
        disconnect(&m_timer, &QTimer::timeout, this, &TaskPage::NavigateNextPart);
        m_auto_navigate_frame_num = 0;
        m_auto_navigate_fps = 0;
        m_auto_navigate_fps_value_label->setText("-");
    }
}

void TaskPage::SetInterval(int interval) {
    m_interval = interval;
}

void TaskPage::NavigatePart(uint32_t part_id) {
    m_cur_part_id = part_id;
    Draw(part_id);
}

void TaskPage::Draw(uint32_t part_id) {
    Bytes part_bytes(m_raw_bytes.begin()+part_id*m_part_byte_num, m_raw_bytes.begin()+(part_id+1)*m_part_byte_num);
    auto symbols = m_symbol_codec->Encode(part_id, part_bytes, m_context.tile_x_num * m_context.tile_y_num * m_context.tile_x_size * m_context.tile_y_size);
    emit PartNavigated(symbols);
}

void TaskPage::NavigateNextPart() {
    if (!m_undone_part_ids.empty()) {
        bool need_normal_navigate = true;
        if (m_task_status_auto_update && m_undone_part_ids.size() > m_task_status_auto_update_threshold && m_cur_undone_part_id_index == m_undone_part_ids.size() - 1) {
            need_normal_navigate = !UpdateTaskStatus();
        }
        if (need_normal_navigate) {
            m_cur_undone_part_id_index = m_cur_undone_part_id_index < m_undone_part_ids.size() - 1 ? m_cur_undone_part_id_index + 1 : 0;
            uint32_t cur_part_id = m_undone_part_ids[m_cur_undone_part_id_index];
            m_cur_part_id_spin_box->setValue(cur_part_id);
        }
    } else {
        bool need_normal_navigate = true;
        if (m_task_status_auto_update && m_part_num > m_task_status_auto_update_threshold && m_cur_part_id == m_part_num - 1) {
            need_normal_navigate = !UpdateTaskStatus();
        }
        if (need_normal_navigate) {
            uint32_t cur_part_id = m_cur_part_id < m_part_num - 1 ? m_cur_part_id + 1 : 0;
            m_cur_part_id_spin_box->setValue(cur_part_id);
        }
    }
    if (m_display_mode == DisplayMode::AUTO) {
        m_auto_navigate_frame_num += 1;
        if (m_auto_navigate_frame_num == m_auto_navigate_update_fps_interval) {
            auto t = std::chrono::high_resolution_clock::now();
            auto delta_t = std::max(std::chrono::duration_cast<std::chrono::duration<float>>(t - m_auto_navigate_time).count(), 0.001f);
            m_auto_navigate_time = t;
            float fps = m_auto_navigate_frame_num / delta_t;
            m_auto_navigate_fps = m_auto_navigate_fps * 0.5f + fps * 0.5f;
            m_auto_navigate_frame_num = 0;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << m_auto_navigate_fps;
            m_auto_navigate_fps_value_label->setText(oss.str().c_str());
        }
    }
}

void TaskPage::NavigatePrevPart() {
    uint32_t cur_part_id = 0;
    if (!m_undone_part_ids.empty()) {
        m_cur_undone_part_id_index = m_cur_undone_part_id_index > 0 ? m_cur_undone_part_id_index - 1 : m_undone_part_ids.size() - 1;
        cur_part_id = m_undone_part_ids[m_cur_undone_part_id_index];
    } else {
        cur_part_id = m_cur_part_id > 0 ? m_cur_part_id - 1 : m_part_num - 1;
    }
    m_cur_part_id_spin_box->setValue(cur_part_id);
}

bool TaskPage::FetchTaskStatus() {
    Bytes task_bytes = m_task_status_client->GetTaskStatus();
    if (!task_bytes.empty()) {
        auto [symbol_type, dim, part_num, done_part_num, task_status_bytes] = from_task_bytes(task_bytes);
        assert(symbol_type == m_context.symbol_type);
        assert(dim.tile_x_num == m_context.tile_x_num &&
               dim.tile_y_num == m_context.tile_y_num &&
               dim.tile_x_size == m_context.tile_x_size &&
               dim.tile_y_size == m_context.tile_y_size);
        assert(part_num == m_part_num);
        assert(task_status_bytes.size() == (m_part_num + 7) / 8);
        m_undone_part_ids.clear();
        for (uint32_t part_id = 0; part_id < m_part_num; ++part_id) {
            if (!is_part_done(task_status_bytes, part_id)) {
                m_undone_part_ids.push_back(part_id);
            }
        }
        m_cur_undone_part_id_index = 0;
        m_cur_part_id = m_undone_part_ids[m_cur_undone_part_id_index];
        m_cur_part_id_spin_box->setValue(m_cur_part_id);
        m_undone_part_id_num_label->setText(std::to_string(m_undone_part_ids.size()).c_str());
        return true;
    } else {
        return false;
    }
}

bool TaskPage::UpdateTaskStatus() {
    if (m_task_status_server_on) {
        m_display_config_frame->setEnabled(false);
        if (m_display_mode == DisplayMode::AUTO) {
            m_timer.stop();
        }
        auto success = FetchTaskStatus();
        if (m_display_mode == DisplayMode::AUTO) {
            m_timer.start(m_interval);
        }
        m_display_config_frame->setEnabled(true);
        return success;
    } else {
        return false;
    }
}

Canvas::Canvas(QWidget* parent, const Parameters& parameters, const Context& context) : QWidget(parent), m_parameters(parameters), m_context(context) {
}

void Canvas::DrawCalibration(CalibrationMode calibration_mode, const std::vector<uint8_t>& data) {
    m_is_calibration = true;
    m_calibration_mode = calibration_mode;
    m_data = data;
    update();
}

void Canvas::DrawPart(std::vector<uint8_t> data) {
    m_is_calibration = false;
    m_data = std::move(data);
    update();
}

void Canvas::Clear() {
    m_is_calibration = false;
    m_data.clear();
    update();
}

bool Canvas::IsTileBorder(int x, int y) {
    return y == 0 || y == m_context.tile_y_size + 1 || x == 0 || x == m_context.tile_x_size + 1;
}

void Canvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setPen(Qt::NoPen);

    painter.setBrush(QColor(255, 255, 255));
    painter.drawRect(rect());

    if (m_is_calibration || !m_data.empty()) {
        painter.save();
        painter.setBrush(QColor(0, 0, 0));
        painter.translate(
            (width() - (m_context.tile_x_num * (m_context.tile_x_size + 2) + (m_context.tile_x_num - 1) * m_context.space_size) * m_context.pixel_size) / 2,
            (height() - (m_context.tile_y_num * (m_context.tile_y_size + 2) + (m_context.tile_y_num - 1) * m_context.space_size) * m_context.pixel_size) / 2
            );
        for (int tile_y_id = 0; tile_y_id < m_context.tile_y_num; ++tile_y_id) {
            int tile_y = tile_y_id * (m_context.tile_y_size + 2 + m_context.space_size) * m_context.pixel_size;
            for (int tile_x_id = 0; tile_x_id < m_context.tile_x_num; ++tile_x_id) {
                int tile_x = tile_x_id * (m_context.tile_x_size + 2 + m_context.space_size) * m_context.pixel_size;
                for (int y = 0; y < m_context.tile_y_size + 2; ++y) {
                    for (int x = 0; x < m_context.tile_x_size + 2; ++x) {
                        if (m_is_calibration && m_calibration_mode == CalibrationMode::POSITION) {
                            if (!IsTileBorder(x, y)) {
                                painter.drawRect(tile_x + x * m_context.pixel_size + (m_context.pixel_size - m_context.calibration_pixel_size) / 2, tile_y + y * m_context.pixel_size + (m_context.pixel_size - m_context.calibration_pixel_size) / 2, m_context.calibration_pixel_size, m_context.calibration_pixel_size);
                            }
                        } else {
                            if (IsTileBorder(x, y)) {
                                painter.setBrush(QColor(0, 0, 0));
                            } else {
                                Symbol symbol = m_data[((tile_y_id * m_context.tile_x_num + tile_x_id) * m_context.tile_y_size + (y - 1)) * m_context.tile_x_size + (x - 1)];
                                if (symbol == static_cast<int>(PixelColor::WHITE)) {
                                    painter.setBrush(QColor(255, 255, 255));
                                } else if (symbol == static_cast<int>(PixelColor::BLACK)) {
                                    painter.setBrush(QColor(0, 0, 0));
                                } else if (symbol == static_cast<int>(PixelColor::RED)) {
                                    painter.setBrush(QColor(255, 0, 0));
                                } else if (symbol == static_cast<int>(PixelColor::BLUE)) {
                                    painter.setBrush(QColor(0, 0, 255));
                                } else if (symbol == static_cast<int>(PixelColor::GREEN)) {
                                    painter.setBrush(QColor(0, 255, 0));
                                } else if (symbol == static_cast<int>(PixelColor::CYAN)) {
                                    painter.setBrush(QColor(0, 255, 255));
                                } else if (symbol == static_cast<int>(PixelColor::MAGENTA)) {
                                    painter.setBrush(QColor(255, 0, 255));
                                } else if (symbol == static_cast<int>(PixelColor::YELLOW)) {
                                    painter.setBrush(QColor(255, 255, 0));
                                } else {
                                    std::cerr << "invalid symbol '" << symbol << "'\n";
                                    assert(false);
                                }
                            }
                            painter.drawRect(tile_x + x * m_context.pixel_size, tile_y + y * m_context.pixel_size, m_context.pixel_size, m_context.pixel_size);
                        }
                    }
                }
            }
        }
        painter.restore();
    }
}

Widget::Widget(QWidget* parent) : QWidget(parent) {
    LoadConfig();

    auto layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_calibration_page = new CalibrationPage(nullptr, m_parameters, m_context);
    m_task_page = new TaskPage(nullptr, m_parameters, m_context);

    connect(m_calibration_page->m_symbol_type_combo_box, &QComboBox::currentIndexChanged, m_task_page->m_symbol_type_combo_box, &QComboBox::setCurrentIndex);
    connect(m_calibration_page->m_symbol_type_combo_box, &QComboBox::currentIndexChanged, this, &Widget::SetSymbolType);
    connect(m_calibration_page->m_tile_x_num_spin_box, &QSpinBox::valueChanged, m_task_page->m_tile_x_num_spin_box, &QSpinBox::setValue);
    connect(m_calibration_page->m_tile_x_num_spin_box, &QSpinBox::valueChanged, this, &Widget::SetTileXNum);
    connect(m_calibration_page->m_tile_y_num_spin_box, &QSpinBox::valueChanged, m_task_page->m_tile_y_num_spin_box, &QSpinBox::setValue);
    connect(m_calibration_page->m_tile_y_num_spin_box, &QSpinBox::valueChanged, this, &Widget::SetTileYNum);
    connect(m_calibration_page->m_tile_x_size_spin_box, &QSpinBox::valueChanged, m_task_page->m_tile_x_size_spin_box, &QSpinBox::setValue);
    connect(m_calibration_page->m_tile_x_size_spin_box, &QSpinBox::valueChanged, this, &Widget::SetTileXSize);
    connect(m_calibration_page->m_tile_y_size_spin_box, &QSpinBox::valueChanged, m_task_page->m_tile_y_size_spin_box, &QSpinBox::setValue);
    connect(m_calibration_page->m_tile_y_size_spin_box, &QSpinBox::valueChanged, this, &Widget::SetTileYSize);
    connect(m_calibration_page->m_pixel_size_spin_box, &QSpinBox::valueChanged, m_task_page->m_pixel_size_spin_box, &QSpinBox::setValue);
    connect(m_calibration_page->m_pixel_size_spin_box, &QSpinBox::valueChanged, this, &Widget::SetPixelSize);
    connect(m_calibration_page->m_space_size_spin_box, &QSpinBox::valueChanged, m_task_page->m_space_size_spin_box, &QSpinBox::setValue);
    connect(m_calibration_page->m_space_size_spin_box, &QSpinBox::valueChanged, this, &Widget::SetSpaceSize);
    connect(m_calibration_page->m_calibration_pixel_size_spin_box, &QSpinBox::valueChanged, this, &Widget::SetCalibrationPixelSize);
    connect(m_task_page->m_symbol_type_combo_box, &QComboBox::currentIndexChanged, m_calibration_page->m_symbol_type_combo_box, &QComboBox::setCurrentIndex);
    connect(m_task_page->m_symbol_type_combo_box, &QComboBox::currentIndexChanged, this, &Widget::SetSymbolType);
    connect(m_task_page->m_tile_x_num_spin_box, &QSpinBox::valueChanged, m_calibration_page->m_tile_x_num_spin_box, &QSpinBox::setValue);
    connect(m_task_page->m_tile_x_num_spin_box, &QSpinBox::valueChanged, this, &Widget::SetTileXNum);
    connect(m_task_page->m_tile_y_num_spin_box, &QSpinBox::valueChanged, m_calibration_page->m_tile_y_num_spin_box, &QSpinBox::setValue);
    connect(m_task_page->m_tile_y_num_spin_box, &QSpinBox::valueChanged, this, &Widget::SetTileYNum);
    connect(m_task_page->m_tile_x_size_spin_box, &QSpinBox::valueChanged, m_calibration_page->m_tile_x_size_spin_box, &QSpinBox::setValue);
    connect(m_task_page->m_tile_x_size_spin_box, &QSpinBox::valueChanged, this, &Widget::SetTileXSize);
    connect(m_task_page->m_tile_y_size_spin_box, &QSpinBox::valueChanged, m_calibration_page->m_tile_y_size_spin_box, &QSpinBox::setValue);
    connect(m_task_page->m_tile_y_size_spin_box, &QSpinBox::valueChanged, this, &Widget::SetTileYSize);
    connect(m_task_page->m_pixel_size_spin_box, &QSpinBox::valueChanged, m_calibration_page->m_pixel_size_spin_box, &QSpinBox::setValue);
    connect(m_task_page->m_pixel_size_spin_box, &QSpinBox::valueChanged, this, &Widget::SetPixelSize);
    connect(m_task_page->m_space_size_spin_box, &QSpinBox::valueChanged, m_calibration_page->m_space_size_spin_box, &QSpinBox::setValue);
    connect(m_task_page->m_space_size_spin_box, &QSpinBox::valueChanged, this, &Widget::SetSpaceSize);

    m_control_tab = new QTabWidget();
    m_control_tab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_control_tab->addTab(m_calibration_page, "calibration");
    m_control_tab->addTab(m_task_page, "task");
    layout->addWidget(m_control_tab);

    m_canvas = new Canvas(nullptr, m_parameters, m_context);
    m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_canvas->setMinimumSize(400, 400);
    m_canvas->setFocusPolicy(Qt::ClickFocus);
    m_canvas->installEventFilter(this);
    layout->addWidget(m_canvas);

    connect(m_calibration_page, &CalibrationPage::CalibrationStarted, this, &Widget::StartCalibration);
    connect(m_calibration_page, &CalibrationPage::CalibrationStopped, this, &Widget::StopCalibration);
    connect(m_task_page, &TaskPage::TaskStarted, this, &Widget::StartTask);
    connect(m_task_page, &TaskPage::PartNavigated, m_canvas, &Canvas::DrawPart);
    connect(m_task_page, &TaskPage::TaskStopped, this, &Widget::StopTask);
}

void Widget::LoadConfig() {
    std::ifstream cfg_file("display_qt.ini");
    std::string symbol_type_str;
    boost::program_options::options_description desc;
    auto desc_handler = desc.add_options();
    desc_handler("DEFAULT.symbol_type", boost::program_options::value<std::string>(&symbol_type_str));
    desc_handler("DEFAULT.tile_x_num", boost::program_options::value<int>(&m_context.tile_x_num));
    desc_handler("DEFAULT.tile_y_num", boost::program_options::value<int>(&m_context.tile_y_num));
    desc_handler("DEFAULT.tile_x_size", boost::program_options::value<int>(&m_context.tile_x_size));
    desc_handler("DEFAULT.tile_y_size", boost::program_options::value<int>(&m_context.tile_y_size));
    desc_handler("DEFAULT.pixel_size", boost::program_options::value<int>(&m_context.pixel_size));
    desc_handler("DEFAULT.space_size", boost::program_options::value<int>(&m_context.space_size));
    desc_handler("DEFAULT.calibration_pixel_size", boost::program_options::value<int>(&m_context.calibration_pixel_size));
    desc_handler("DEFAULT.task_status_server", boost::program_options::value<std::string>(&m_context.task_status_server));
    boost::program_options::variables_map vm;
    store(parse_config_file(cfg_file, desc, false), vm);
    notify(vm);
    if (!vm.count("DEFAULT.symbol_type")) throw std::invalid_argument("DEFAULT.symbol_type not found");
    if (!vm.count("DEFAULT.tile_x_num")) throw std::invalid_argument("DEFAULT.tile_x_num not found");
    if (!vm.count("DEFAULT.tile_y_num")) throw std::invalid_argument("DEFAULT.tile_y_num not found");
    if (!vm.count("DEFAULT.tile_x_size")) throw std::invalid_argument("DEFAULT.tile_x_size not found");
    if (!vm.count("DEFAULT.tile_y_size")) throw std::invalid_argument("DEFAULT.tile_y_size not found");
    if (!vm.count("DEFAULT.pixel_size")) throw std::invalid_argument("DEFAULT.pixel_size not found");
    if (!vm.count("DEFAULT.space_size")) throw std::invalid_argument("DEFAULT.space_size not found");
    if (!vm.count("DEFAULT.calibration_pixel_size")) throw std::invalid_argument("DEFAULT.calibration_pixel_size not found");
    if (!vm.count("DEFAULT.task_status_server")) throw std::invalid_argument("DEFAULT.task_status_server not found");
    m_context.symbol_type = parse_symbol_type(symbol_type_str);
}

void Widget::StartCalibration(CalibrationMode calibration_mode, std::vector<uint8_t> data) {
    m_control_tab->setTabEnabled(1, false);
    m_canvas->DrawCalibration(calibration_mode, data);
}

void Widget::StopCalibration() {
    m_control_tab->setTabEnabled(1, true);
    m_canvas->Clear();
}

void Widget::StartTask() {
    m_control_tab->setTabEnabled(0, false);
}

void Widget::StopTask() {
    m_control_tab->setTabEnabled(0, true);
    m_canvas->Clear();
}

void Widget::ToggleFullScreen() {
    m_control_tab->setVisible(!m_control_tab->isVisible());
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

bool Widget::eventFilter(QObject* obj, QEvent* event) {
    if (m_context.state == State::DISPLAY) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
            if (key_event->key() == Qt::Key_F) {
                ToggleFullScreen();
            } else if (key_event->key() == Qt::Key_Space) {
                m_task_page->ToggleDisplayMode();
            } else if (key_event->key() == Qt::Key_Left) {
                m_task_page->NavigatePrevPart();
            } else if (key_event->key() == Qt::Key_Right) {
                m_task_page->NavigateNextPart();
            } else if (key_event->key() == Qt::Key_T) {
                m_task_page->UpdateTaskStatus();
            }
        }
    } else if (m_context.state == State::CALIBRATE) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
            if (key_event->key() == Qt::Key_F) {
                ToggleFullScreen();
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
