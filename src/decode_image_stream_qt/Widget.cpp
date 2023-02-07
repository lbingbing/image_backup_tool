#include <sstream>
#include <filesystem>

#include <QtWidgets/QHboxLayout>
#include <QtWidgets/QVboxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMessageBox>

#include "Widget.h"

CalibrateThread::CalibrateThread(QObject* parent, std::function<void(ImageDecodeWorker::CalibrateCb, ImageDecodeWorker::SendCalibrationImageResultCb, ImageDecodeWorker::CalibrationProgressCb)> worker_fn) : m_worker_fn(worker_fn) {
}

void CalibrateThread::run() {
    auto send_calibrate_cb = [this](const Calibration& calibration) {
        emit SendCalibration(calibration);
    };
    auto send_calibration_image_result_cb = [this](const cv::Mat& img, bool success, const std::vector<std::vector<cv::Mat>>& result_imgs) {
        emit SendCalibrationImageResult(img.clone(), success, result_imgs);
    };
    auto send_calibrate_progress_cb = [this](const ImageDecodeWorker::CalibrationProgress& calibration_progress) {
        emit SendCalibrationProgress(calibration_progress);
    };
    m_worker_fn(send_calibrate_cb, send_calibration_image_result_cb, send_calibrate_progress_cb);
}

DecodeImageResultThread::DecodeImageResultThread(QObject* parent, std::function<void(ImageDecodeWorker::SendDecodeImageResultCb)> worker_fn) : m_worker_fn(worker_fn) {
}

void DecodeImageResultThread::run() {
    auto send_decode_image_result_cb = [this](const cv::Mat& img, bool success, const std::vector<std::vector<cv::Mat>>& result_imgs) {
        emit SendDecodeImageResult(img.clone(), success, result_imgs);
    };
    m_worker_fn(send_decode_image_result_cb);
}

AutoTransformThread::AutoTransformThread(QObject* parent, std::function<void(ImageDecodeWorker::SendAutoTransformCb)> worker_fn) : m_worker_fn(worker_fn) {
}

void AutoTransformThread::run() {
    auto cb = [this](const Transform& transform) {
        emit SendAutoTransform(transform);
    };
    m_worker_fn(cb);
}

SavePartThread::SavePartThread(QObject* parent, std::function<void(ImageDecodeWorker::SavePartProgressCb, ImageDecodeWorker::SavePartCompleteCb, ImageDecodeWorker::SavePartErrorCb, Task::FinalizationStartCb, Task::FinalizationProgressCb)> worker_fn) : m_worker_fn(worker_fn) {
}

void SavePartThread::run() {
    auto save_part_progress_cb = [this](const ImageDecodeWorker::SavePartProgress& save_part_progress) {
        emit SendSavePartProgress(save_part_progress);
    };
    auto save_part_complete_cb = [this] {
        emit SendSavePartComplete();
    };
    auto save_part_error_cb = [this](const std::string& msg) {
        emit SendSavePartError(msg);
    };
    auto finalization_start_cb = [this](const Task::FinalizationProgress& finalization_progress) {
        emit SendFinalizationStart(finalization_progress);
    };
    auto finalization_progress_cb = [this](const Task::FinalizationProgress& finalization_progress) {
        emit SendFinalizationProgress(finalization_progress);
    };
    m_worker_fn(save_part_progress_cb, save_part_complete_cb, save_part_error_cb, finalization_start_cb, finalization_progress_cb);
}

Widget::Widget(QWidget* parent, const std::string& output_file, SymbolType symbol_type, const Dim& dim, uint32_t part_num, int mp) : QWidget(parent), m_output_file(output_file), m_image_decode_worker(symbol_type), m_dim(dim), m_part_num(part_num), m_mp(mp) {
    m_result_images.resize(m_dim.tile_y_num);
    for (int tile_y_id = 0; tile_y_id < m_dim.tile_y_num; ++tile_y_id) {
        m_result_images[tile_y_id].resize(m_dim.tile_x_num);
    }

    auto layout = new QVBoxLayout(this);

    auto image_layout = new QHBoxLayout();
    layout->addLayout(image_layout);

    const int image_w = 600;
    const int image_h = 400;

    auto image_group_box = new QGroupBox("image");
    image_layout->addWidget(image_group_box);

    auto image_layout1 = new QHBoxLayout(image_group_box);

    m_image_label = new QLabel();
    m_image_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_image_label->setFixedSize(image_w, image_h);
    m_image_label->setScaledContents(true);
    image_layout1->addWidget(m_image_label);

    auto result_image_group_box = new QGroupBox("result image");
    image_layout->addWidget(result_image_group_box);

    auto result_image_layout = new QVBoxLayout(result_image_group_box);

    m_result_image_labels.resize(m_dim.tile_y_num);
    for (int tile_y_id = 0; tile_y_id < m_dim.tile_y_num; ++tile_y_id) {
        m_result_image_labels[tile_y_id].resize(m_dim.tile_x_num);
        auto result_image_layout1 = new QHBoxLayout();
        result_image_layout->addLayout(result_image_layout1);
        for (int tile_x_id = 0; tile_x_id < m_dim.tile_x_num; ++tile_x_id) {
            auto result_image_label = new QLabel();
            result_image_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            result_image_label->setFixedSize(image_w / m_dim.tile_x_num, image_h / m_dim.tile_y_num);
            result_image_label->setScaledContents(true);
            result_image_layout1->addWidget(result_image_label);
            m_result_image_labels[tile_y_id][tile_x_id] = result_image_label;
        }
    }

    auto control_layout = new QHBoxLayout();
    layout->addLayout(control_layout);

    auto calibration_group_box = new QGroupBox("calibration");
    control_layout->addWidget(calibration_group_box);

    auto calibration_group_box_layout = new QHBoxLayout(calibration_group_box);

    m_calibrate_button = new QPushButton("calibrate");
    m_calibrate_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_calibrate_button->setFixedWidth(80);
    m_calibrate_button->setCheckable(true);
    connect(m_calibrate_button, &QPushButton::clicked, this, &Widget::ToggleCalibrationStartStop);
    calibration_group_box_layout->addWidget(m_calibrate_button);

    auto calibration_button_layout = new QVBoxLayout();
    calibration_group_box_layout->addLayout(calibration_button_layout);

    m_save_calibration_button = new QPushButton("save calibration");
    m_save_calibration_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_save_calibration_button->setFixedWidth(120);
    m_save_calibration_button->setEnabled(false);
    connect(m_save_calibration_button, &QPushButton::clicked, this, &Widget::SaveCalibration);
    calibration_button_layout->addWidget(m_save_calibration_button);

    m_load_calibration_button = new QPushButton("load calibration");
    m_load_calibration_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_load_calibration_button->setFixedWidth(120);
    connect(m_load_calibration_button, &QPushButton::clicked, this, &Widget::LoadCalibration);
    calibration_button_layout->addWidget(m_load_calibration_button);

    m_clear_calibration_button = new QPushButton("clear calibration");
    m_clear_calibration_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_clear_calibration_button->setFixedWidth(120);
    m_clear_calibration_button->setEnabled(false);
    connect(m_clear_calibration_button, &QPushButton::clicked, this, &Widget::ClearCalibration);
    calibration_button_layout->addWidget(m_clear_calibration_button);

    auto task_group_box = new QGroupBox("task");
    control_layout->addWidget(task_group_box);

    auto task_group_box_layout = new QHBoxLayout(task_group_box);

    m_task_button = new QPushButton("start");
    m_task_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_task_button->setFixedWidth(80);
    m_task_button->setCheckable(true);
    connect(m_task_button, &QPushButton::clicked, this, &Widget::ToggleTaskStartStop);
    task_group_box_layout->addWidget(m_task_button);

    auto monitor_group_box = new QGroupBox("monitor");
    control_layout->addWidget(monitor_group_box);

    auto monitor_layout = new QVBoxLayout(monitor_group_box);

    m_monitor_button= new QPushButton("monitor");
    m_monitor_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_monitor_button->setFixedWidth(80);
    m_monitor_button->setCheckable(true);
    m_monitor_button->setChecked(true);
    connect(m_monitor_button, &QPushButton::clicked, this, &Widget::ToggleMonitor);
    monitor_layout->addWidget(m_monitor_button);

    m_save_image_button = new QPushButton("save image");
    m_save_image_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_save_image_button->setFixedWidth(80);
    connect(m_save_image_button, &QPushButton::clicked, this, &Widget::SaveImage);
    monitor_layout->addWidget(m_save_image_button);

    auto task_status_server_group_box = new QGroupBox("server");
    control_layout->addWidget(task_status_server_group_box);

    auto task_status_server_layout = new QVBoxLayout(task_status_server_group_box);

    m_task_status_server_button = new QPushButton("server");
    m_task_status_server_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_task_status_server_button->setFixedWidth(80);
    m_task_status_server_button->setCheckable(true);
    connect(m_task_status_server_button, &QPushButton::clicked, this, &Widget::ToggleTaskStatusServer);
    task_status_server_layout->addWidget(m_task_status_server_button);

    auto task_status_server_port_layout = new QHBoxLayout();
    task_status_server_layout->addLayout(task_status_server_port_layout);

    m_task_status_server_port_label = new QLabel("port:");
    task_status_server_port_layout->addWidget(m_task_status_server_port_label);

    m_task_status_server_port_line_edit = new QLineEdit("8123");
    m_task_status_server_port_line_edit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_task_status_server_port_line_edit->setFixedWidth(40);
    task_status_server_port_layout->addWidget(m_task_status_server_port_line_edit);

    auto transform_group_box = new QGroupBox("transform");
    transform_group_box->setCheckable(true);
    control_layout->addWidget(transform_group_box);

    auto transform_layout = new QHBoxLayout(transform_group_box);

    auto transform_button_layout = new QVBoxLayout();
    transform_layout->addLayout(transform_button_layout);

    m_save_transform_button = new QPushButton("save transform");
    m_save_transform_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_save_transform_button->setFixedWidth(100);
    connect(m_save_transform_button, &QPushButton::clicked, this, &Widget::SaveTransform);
    transform_button_layout->addWidget(m_save_transform_button);

    m_load_transform_button = new QPushButton("load transform");
    m_load_transform_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_load_transform_button->setFixedWidth(100);
    connect(m_load_transform_button, &QPushButton::clicked, this, &Widget::LoadTransform);
    transform_button_layout->addWidget(m_load_transform_button);

    auto form_layout1 = new QFormLayout();
    transform_layout->addLayout(form_layout1);
    auto form_layout2 = new QFormLayout();
    transform_layout->addLayout(form_layout2);

    m_bbox_label = new QLabel("bbox:");
    m_bbox_line_edit = new QLineEdit();
    connect(m_bbox_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout1->addRow(m_bbox_label, m_bbox_line_edit);

    m_sphere_label = new QLabel("sphere:");
    m_sphere_line_edit = new QLineEdit();
    connect(m_sphere_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout1->addRow(m_sphere_label, m_sphere_line_edit);

    m_filter_level_label = new QLabel("filter_level:");
    m_filter_level_line_edit = new QLineEdit();
    connect(m_filter_level_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout1->addRow(m_filter_level_label, m_filter_level_line_edit);

    m_binarization_threshold_label = new QLabel("binarization_threshold:");
    m_binarization_threshold_line_edit = new QLineEdit();
    connect(m_binarization_threshold_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout2->addRow(m_binarization_threshold_label, m_binarization_threshold_line_edit);

    m_pixelization_threshold_label = new QLabel("pixelization_threshold:");
    m_pixelization_threshold_line_edit = new QLineEdit();
    connect(m_pixelization_threshold_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout2->addRow(m_pixelization_threshold_label, m_pixelization_threshold_line_edit);

    UpdateTransformUI(m_transform);

    auto status_group_box = new QGroupBox("status");
    layout->addWidget(status_group_box);

    auto status_group_box_layout = new QHBoxLayout(status_group_box);

    auto result_frame = new QFrame();
    result_frame->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    result_frame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    result_frame->setFixedWidth(50);
    status_group_box_layout->addWidget(result_frame);

    auto result_frame_layout = new QHBoxLayout(result_frame);

    m_result_label = new QLabel("-");
    result_frame_layout->addWidget(m_result_label);

    auto status_frame = new QFrame();
    status_frame->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    status_group_box_layout->addWidget(status_frame);

    auto status_frame_layout = new QHBoxLayout(status_frame);

    m_status_label = new QLabel("-");
    status_frame_layout->addWidget(m_status_label);

    m_task_save_part_progress_bar = new QProgressBar();
    m_task_save_part_progress_bar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_task_save_part_progress_bar->setFixedWidth(500);
    m_task_save_part_progress_bar->setFormat("%p%");
    m_task_save_part_progress_bar->setRange(0, m_part_num);
    status_group_box_layout->addWidget(m_task_save_part_progress_bar);

    resize(1200, 600);
}

void Widget::StartCalibration() {
    m_task_button->setEnabled(false);
    m_clear_calibration_button->setEnabled(false);
    m_save_calibration_button->setEnabled(false);
    m_load_calibration_button->setEnabled(false);

    m_calibration_running = true;
    auto get_transform_fn = [this] {
        std::lock_guard<std::mutex> lock(m_transform_mtx);
        return m_transform;
    };
    m_fetch_image_thread = std::make_unique<std::thread>(&ImageDecodeWorker::FetchImageWorker, &m_image_decode_worker, std::ref(m_calibration_running), std::ref(m_frame_q), 200);
    auto calibrate_worker_fn = [this, get_transform_fn](ImageDecodeWorker::CalibrateCb calibrate_cb, ImageDecodeWorker::SendCalibrationImageResultCb send_calibration_image_result_cb, ImageDecodeWorker::CalibrationProgressCb calibration_progress_cb) {
        m_image_decode_worker.CalibrateWorker(m_frame_q, m_dim, get_transform_fn, calibrate_cb, send_calibration_image_result_cb, calibration_progress_cb);
    };
    m_calibrate_thread = std::make_unique<CalibrateThread>(this, calibrate_worker_fn);
    connect(m_calibrate_thread.get(), &CalibrateThread::SendCalibration, this, &Widget::ReceiveCalibration);
    connect(m_calibrate_thread.get(), &CalibrateThread::SendCalibrationImageResult, this, &Widget::ShowResult);
    connect(m_calibrate_thread.get(), &CalibrateThread::SendCalibrationProgress, this, &Widget::ShowCalibrationProgress);
    m_calibrate_thread->start();
}

void Widget::StopCalibration() {
    m_calibration_running = false;
    m_fetch_image_thread->join();
    m_fetch_image_thread.reset();
    m_frame_q.PushNull();
    m_calibrate_thread->quit();
    m_calibrate_thread->wait();
    m_calibrate_thread.reset();

    m_task_button->setEnabled(true);
    m_load_calibration_button->setEnabled(true);
    if (m_calibration.valid) {
        m_clear_calibration_button->setEnabled(true);
        m_save_calibration_button->setEnabled(true);
    }
    ClearStatus();
}

void Widget::SaveCalibration() {
    m_calibration.Save(m_output_file + ".calibration");
}

void Widget::LoadCalibration() {
    auto calibration_path = m_output_file + ".calibration";
    if (std::filesystem::is_regular_file(calibration_path)) {
        m_calibration.Load(calibration_path);
        if (m_calibration.valid) {
            m_clear_calibration_button->setEnabled(true);
            m_save_calibration_button->setEnabled(true);
        }
    } else {
        QMessageBox::warning(this, "Warning", std::string("can't find calibration file '" + calibration_path + "'").c_str(), QMessageBox::Ok, QMessageBox::Ok);
    }
}

void Widget::ClearCalibration() {
    m_calibration.valid = false;
    m_clear_calibration_button->setEnabled(false);
    m_save_calibration_button->setEnabled(false);
}

void Widget::StartTask() {
    m_calibrate_button->setEnabled(false);
    m_clear_calibration_button->setEnabled(false);
    m_save_calibration_button->setEnabled(false);
    m_load_calibration_button->setEnabled(false);

    m_task_running = true;
    auto get_transform_fn = [this] {
        std::lock_guard<std::mutex> lock(m_transform_mtx);
        return m_transform;
    };
    m_fetch_image_thread = std::make_unique<std::thread>(&ImageDecodeWorker::FetchImageWorker, &m_image_decode_worker, std::ref(m_task_running), std::ref(m_frame_q), 25);
    for (int i = 0; i < m_mp; ++i) {
        m_decode_image_threads.emplace_back(&ImageDecodeWorker::DecodeImageWorker, &m_image_decode_worker, std::ref(m_part_q), std::ref(m_frame_q), m_dim, get_transform_fn, m_calibration);
    }
    auto decode_image_result_worker_fn = [this, get_transform_fn](ImageDecodeWorker::SendDecodeImageResultCb send_decode_image_result_cb) {
        m_image_decode_worker.DecodeResultWorker(m_part_q, m_frame_q, m_dim, get_transform_fn, m_calibration, send_decode_image_result_cb, 300);
    };
    m_decode_image_result_thread = std::make_unique<DecodeImageResultThread>(this, decode_image_result_worker_fn);
    connect(m_decode_image_result_thread.get(), &DecodeImageResultThread::SendDecodeImageResult, this, &Widget::ShowResult);
    m_decode_image_result_thread->start();
    auto auto_transform_worker_fn = [this, get_transform_fn](ImageDecodeWorker::SendAutoTransformCb send_auto_transform_cb) {
        m_image_decode_worker.AutoTransformWorker(m_part_q, m_frame_q, m_dim, get_transform_fn, m_calibration, send_auto_transform_cb, 300);
    };
    m_auto_transform_thread = std::make_unique<AutoTransformThread>(this, auto_transform_worker_fn);
    connect(m_auto_transform_thread.get(), &AutoTransformThread::SendAutoTransform, this, &Widget::UpdateAutoTransform);
    m_auto_transform_thread->start();
    auto save_part_worker_fn = [this](ImageDecodeWorker::SavePartProgressCb save_part_progress_cb, ImageDecodeWorker::SavePartCompleteCb save_part_complete_cb, ImageDecodeWorker::SavePartErrorCb save_part_error_cb, Task::FinalizationStartCb finalization_start_cb, Task::FinalizationProgressCb finalization_progress_cb) {
        m_image_decode_worker.SavePartWorker(m_task_running, m_part_q, m_output_file, m_dim, m_part_num, save_part_progress_cb, nullptr, save_part_complete_cb, save_part_error_cb, finalization_start_cb, finalization_progress_cb, nullptr, &m_task_status_server);
    };
    m_save_part_thread = std::make_unique<SavePartThread>(this, save_part_worker_fn);
    connect(m_save_part_thread.get(), &SavePartThread::SendSavePartProgress, this, &Widget::ShowTaskSavePartProgress);
    connect(m_save_part_thread.get(), &SavePartThread::SendSavePartComplete, this, &Widget::TaskSavePartComplete);
    connect(m_save_part_thread.get(), &SavePartThread::SendSavePartError, this, &Widget::ErrorMsg);
    connect(m_save_part_thread.get(), &SavePartThread::SendFinalizationStart, this, &Widget::TaskFinalizationStart);
    connect(m_save_part_thread.get(), &SavePartThread::SendFinalizationProgress, this, &Widget::TaskFinalizationProgress);
    m_save_part_thread->start();
}

void Widget::StopTask() {
    m_task_running = false;
    m_fetch_image_thread->join();
    m_fetch_image_thread.reset();
    for (size_t i = 0; i < m_mp + 2; ++i) {
        m_frame_q.PushNull();
    }
    for (auto& t : m_decode_image_threads) {
        t.join();
    }
    m_decode_image_threads.clear();
    m_decode_image_result_thread->quit();
    m_decode_image_result_thread->wait();
    m_decode_image_result_thread.reset();
    m_auto_transform_thread->quit();
    m_auto_transform_thread->wait();
    m_auto_transform_thread.reset();
    m_part_q.PushNull();
    m_save_part_thread->quit();
    m_save_part_thread->wait();
    m_save_part_thread.reset();

    m_calibrate_button->setEnabled(true);
    m_load_calibration_button->setEnabled(true);
    if (m_calibration.valid) {
        m_clear_calibration_button->setEnabled(true);
        m_save_calibration_button->setEnabled(true);
    }
    ClearStatus();
}

void Widget::ToggleCalibrationStartStop() {
    if (m_calibration_running) StopCalibration();
    else                       StartCalibration();
}

void Widget::ToggleTaskStartStop() {
    if (m_task_running) StopTask();
    else                StartTask();
}

void Widget::ClearStatus() {
    ClearImages();
    m_result_label->setText("-");
    m_status_label->setText("-");
    m_task_save_part_progress_bar->setValue(0);
}

void Widget::ClearImages() {
    m_image_label->clear();
    for (auto e1 : m_result_image_labels) {
        for (auto e2 : e1) {
            e2->clear();
        }
    }
}

void Widget::ToggleMonitor() {
    m_monitor_on = !m_monitor_on;
    if (!m_monitor_on) ClearImages();
    m_save_image_button->setEnabled(m_monitor_on);
}

void Widget::SaveImage() {
    if (!m_image.empty()) {
        cv::imwrite(m_output_file + ".transformed_image.bmp", m_image);
    }
    for (int tile_y_id = 0; tile_y_id < m_dim.tile_y_num; ++tile_y_id) {
        for (int tile_x_id = 0; tile_x_id < m_dim.tile_x_num; ++tile_x_id) {
            if (!m_result_images[tile_y_id][tile_x_id].empty()) {
                cv::imwrite(m_output_file + ".result_image_" + std::to_string(tile_x_id) + "_" + std::to_string(tile_y_id) + ".bmp", m_result_images[tile_y_id][tile_x_id]);
            }
        }
    }
}

void Widget::ToggleTaskStatusServer() {
    if (m_task_status_server_on) {
        m_task_status_server.Stop();
        m_task_status_server_on = false;
        m_task_status_server_port_line_edit->setEnabled(true);
    } else {
        int port = -1;
        try {
            port = parse_task_status_server_port(m_task_status_server_port_line_edit->text().toStdString());
        }
        catch (const invalid_image_codec_argument& e) {
            QMessageBox::warning(this, "Warning", std::string("invalid task status server port '" + m_task_status_server_port_line_edit->text().toStdString() + "'").c_str(), QMessageBox::Ok, QMessageBox::Ok);
        }
        if (port > 0) {
            m_task_status_server.Start(port);
            m_task_status_server_on = true;
            m_task_status_server_port_line_edit->setEnabled(false);
        }
    }
}

void Widget::SaveTransform() {
    std::lock_guard<std::mutex> lock(m_transform_mtx);
    m_transform.Save(m_output_file + ".transform");
}

void Widget::LoadTransform() {
    auto transform_path = m_output_file + ".transform";
    if (std::filesystem::is_regular_file(transform_path)) {
        Transform transform;
        {
            std::lock_guard<std::mutex> lock(m_transform_mtx);
            m_transform.Load(transform_path);
            transform = m_transform;
        }
        UpdateTransformUI(transform);
    } else {
        QMessageBox::warning(this, "Warning", std::string("can't find transform file '" + transform_path + "'").c_str(), QMessageBox::Ok, QMessageBox::Ok);
    }
}

void Widget::UpdateTransform(const Transform& transform) {
    std::lock_guard<std::mutex> lock(m_transform_mtx);
    m_transform = transform;
}

void Widget::UpdateTransformUI(const Transform& transform) {
    m_bbox_line_edit->setText(get_bbox_str(transform.bbox).c_str());
    m_sphere_line_edit->setText(get_sphere_str(transform.sphere).c_str());
    m_filter_level_line_edit->setText(get_filter_level_str(transform.filter_level).c_str());
    m_binarization_threshold_line_edit->setText(get_binarization_threshold_str(transform.binarization_threshold).c_str());
    m_pixelization_threshold_line_edit->setText(get_pixelization_threshold_str(transform.pixelization_threshold).c_str());
}

void Widget::ReceiveCalibration(Calibration calibration) {
    m_calibration = calibration;
}

void Widget::ShowResult(cv::Mat img, bool success, std::vector<std::vector<cv::Mat>> result_imgs) {
    if (m_calibration_running || m_task_running) {
        m_image = img;
        if (m_monitor_on) {
            m_image_label->setPixmap(QPixmap::fromImage(QImage(img.data, img.cols, img.rows, img.cols * img.channels(), QImage::Format_BGR888)));
        }
        for (int tile_y_id = 0; tile_y_id < m_dim.tile_y_num; ++tile_y_id) {
            for (int tile_x_id = 0; tile_x_id < m_dim.tile_x_num; ++tile_x_id) {
                const auto& result_img = result_imgs[tile_y_id][tile_x_id];
                m_result_images[tile_y_id][tile_x_id] = result_img;
                if (m_monitor_on) {
                    m_result_image_labels[tile_y_id][tile_x_id]->setPixmap(QPixmap::fromImage(QImage(result_img.data, result_img.cols, result_img.rows, result_img.cols * result_img.channels(), QImage::Format_BGR888)));
                }
            }
        }
        m_result_label->setText(success ? "pass" : "fail");
    }
};

void Widget::ShowCalibrationProgress(ImageDecodeWorker::CalibrationProgress calibration_progress) {
    if (m_calibration_running) {
        std::ostringstream oss;
        oss << calibration_progress.frame_num << " frames processed, fps=" << std::fixed << std::setprecision(2) << calibration_progress.fps;
        m_status_label->setText(oss.str().c_str());
    }
}

void Widget::ShowTaskSavePartProgress(ImageDecodeWorker::SavePartProgress task_save_part_progress) {
    if (m_task_running) {
        std::ostringstream oss;
        oss << task_save_part_progress.frame_num << " frames processed, " << task_save_part_progress.done_part_num << "/" << task_save_part_progress.part_num << " parts transferred, fps=" << std::fixed << std::setprecision(2) << task_save_part_progress.fps << ", done_fps=" << std::fixed << std::setprecision(2) << task_save_part_progress.done_fps << ", bps=" << std::fixed << std::setprecision(0) << task_save_part_progress.bps << ", left_time=" << std::setfill('0') << std::setw(2) << task_save_part_progress.left_days << "d" << std::setw(2) << task_save_part_progress.left_hours << "h" << std::setw(2) << task_save_part_progress.left_minutes << "m" << std::setw(2) << task_save_part_progress.left_seconds << "s" << std::setfill(' ');
        m_status_label->setText(oss.str().c_str());
        m_task_save_part_progress_bar->setValue(task_save_part_progress.done_part_num);
    }
}

void Widget::TransformChanged() {
    try {
        Transform transform;
        transform.bbox = parse_bbox(m_bbox_line_edit->text().toStdString());
        transform.sphere = parse_sphere(m_sphere_line_edit->text().toStdString());
        transform.filter_level = parse_filter_level(m_filter_level_line_edit->text().toStdString());
        transform.binarization_threshold = parse_binarization_threshold(m_binarization_threshold_line_edit->text().toStdString());
        transform.pixelization_threshold = parse_pixelization_threshold(m_pixelization_threshold_line_edit->text().toStdString());
        UpdateTransform(transform);
    }
    catch (const invalid_image_codec_argument& e) {
    }
}

void Widget::UpdateAutoTransform(Transform transform) {
    UpdateTransform(transform);
    UpdateTransformUI(transform);
}

void Widget::TaskSavePartComplete() {
    StopTask();
    m_task_button->setChecked(false);
    QMessageBox::information(this, "Info", "transfer done", QMessageBox::Ok, QMessageBox::Ok);
    //close();
}

void Widget::TaskFinalizationStart(Task::FinalizationProgress finalization_progress) {
    //m_task_finalization_progress_dialog = new QProgressDialog("finalizing task", QString(), finalization_progress.done_block_num, finalization_progress.block_num, this);
    //m_task_finalization_progress_dialog->setMinimumDuration(0);
    //m_task_finalization_progress_dialog->setWindowModality(Qt::WindowModal);
}

void Widget::TaskFinalizationProgress(Task::FinalizationProgress finalization_progress) {
    //m_task_finalization_progress_dialog->setValue(finalization_progress.done_block_num);
}

void Widget::ErrorMsg(std::string msg) {
    QMessageBox::critical(this, "Error", msg.c_str(), QMessageBox::Ok, QMessageBox::Ok);
    close();
}

void Widget::closeEvent(QCloseEvent* event) {
    if (m_calibration_running) StopCalibration();
    if (m_task_running) StopTask();
}
