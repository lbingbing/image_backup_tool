#include <sstream>
#include <chrono>
#include <filesystem>

#include <QtWidgets/QHboxLayout>
#include <QtWidgets/QVboxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QMessageBox>

#include "Widget.h"

CalibrateThread::CalibrateThread(QObject* parent, std::function<void(CalibrateCb, SendResultCb, CalibrateProgressCb)> worker_fn) : m_worker_fn(worker_fn) {
}

void CalibrateThread::run() {
    auto calibrate_cb = [this](const Calibration& calibration) {
        emit SendCalibration(calibration);
    };
    auto send_result_cb = [this](const cv::Mat& img, bool success, const std::vector<std::vector<cv::Mat>>& result_imgs) {
        emit SendResult(img.clone(), success, result_imgs);
    };
    auto calibrate_progress_cb = [this](uint64_t frame_num, float fps) {
        emit SendProgress(frame_num, fps);
    };
    m_worker_fn(calibrate_cb, send_result_cb, calibrate_progress_cb);
}

DecodeResultThread::DecodeResultThread(QObject* parent, std::function<void(SendResultCb)> worker_fn) : m_worker_fn(worker_fn) {
}

void DecodeResultThread::run() {
    auto cb = [this](const cv::Mat& img, bool success, const std::vector<std::vector<cv::Mat>>& result_imgs) {
        emit SendResult(img.clone(), success, result_imgs);
    };
    m_worker_fn(cb);
}

SavePartThread::SavePartThread(QObject* parent, std::function<void(SavePartProgressCb, SavePartCompleteCb, SavePartErrorCb)> worker_fn) : m_worker_fn(worker_fn) {
}

void SavePartThread::run() {
    auto save_part_progress_cb = [this](uint32_t done_part_num, uint32_t part_num, uint64_t frame_num, float fps, float done_fps, float bps, int64_t left_seconds) {
        emit SendProgress(done_part_num, part_num, frame_num, fps, done_fps, bps, left_seconds);
    };
    auto save_part_complete_cb = [this] {
        emit SendComplete();
    };
    auto save_part_error_cb = [this](std::string msg) {
        emit SendError(msg);
    };
    m_worker_fn(save_part_progress_cb, save_part_complete_cb, save_part_error_cb);
}

Widget::Widget(QWidget* parent, const std::string& output_file, const Dim& dim, PixelType pixel_type, int pixel_size, int space_size, uint32_t part_num, const std::string& image_dir_path, int mp)
    : QWidget(parent), m_output_file(output_file), m_dim(dim), m_pixel_image_codec_worker(create_pixel_image_codec_worker(pixel_type)), m_pixel_size(pixel_size), m_space_size(space_size), m_part_num(part_num), m_image_dir_path(image_dir_path), m_mp(mp)
{
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

    auto image_layout1 = new QHBoxLayout();
    image_group_box->setLayout(image_layout1);

    m_image_label = new QLabel();
    m_image_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_image_label->setFixedSize(image_w, image_h);
    m_image_label->setScaledContents(true);
    image_layout1->addWidget(m_image_label);

    auto result_image_group_box = new QGroupBox("result image");
    image_layout->addWidget(result_image_group_box);

    auto result_image_layout = new QVBoxLayout();
    result_image_group_box->setLayout(result_image_layout);

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

    auto calibration_group_box_layout = new QHBoxLayout();
    calibration_group_box->setLayout(calibration_group_box_layout);

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

    auto task_group_box_layout = new QHBoxLayout();
    task_group_box->setLayout(task_group_box_layout);

    m_task_button = new QPushButton("start");
    m_task_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_task_button->setFixedWidth(80);
    m_task_button->setCheckable(true);
    connect(m_task_button, &QPushButton::clicked, this, &Widget::ToggleTaskStartStop);
    task_group_box_layout->addWidget(m_task_button);

    m_save_image_button = new QPushButton("save image");
    m_save_image_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_save_image_button->setFixedWidth(80);
    connect(m_save_image_button, &QPushButton::clicked, this, &Widget::SaveImage);
    task_group_box_layout->addWidget(m_save_image_button);

    m_transform_group_box = new QGroupBox("transform");
    m_transform_group_box->setCheckable(true);
    control_layout->addWidget(m_transform_group_box);

    auto transform_layout = new QHBoxLayout(m_transform_group_box);

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
    m_bbox_line_edit = new QLineEdit(get_bbox_str(m_transform.bbox).c_str());
    connect(m_bbox_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout1->addRow(m_bbox_label, m_bbox_line_edit);

    m_sphere_label = new QLabel("sphere:");
    m_sphere_line_edit = new QLineEdit(get_sphere_str(m_transform.sphere).c_str());
    connect(m_sphere_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout1->addRow(m_sphere_label, m_sphere_line_edit);

    m_filter_level_label = new QLabel("filter_level:");
    m_filter_level_line_edit = new QLineEdit(get_filter_level_str(m_transform.filter_level).c_str());
    connect(m_filter_level_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout1->addRow(m_filter_level_label, m_filter_level_line_edit);

    m_binarization_threshold_label = new QLabel("binarization_threshold:");
    m_binarization_threshold_line_edit = new QLineEdit(get_binarization_threshold_str(m_transform.binarization_threshold).c_str());
    connect(m_binarization_threshold_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout2->addRow(m_binarization_threshold_label, m_binarization_threshold_line_edit);

    m_pixelization_threshold_label = new QLabel("pixelization_threshold:");
    m_pixelization_threshold_line_edit = new QLineEdit(get_pixelization_threshold_str(m_transform.pixelization_threshold).c_str());
    connect(m_pixelization_threshold_line_edit, &QLineEdit::editingFinished, this, &Widget::TransformChanged);
    form_layout2->addRow(m_pixelization_threshold_label, m_pixelization_threshold_line_edit);

    auto result_group_box = new QGroupBox("result");
    auto result_group_box_layout = new QHBoxLayout();
    m_result_label = new QLabel("-");
    result_group_box_layout->addWidget(m_result_label);
    result_group_box->setLayout(result_group_box_layout);
    layout->addWidget(result_group_box);

    auto status_group_box = new QGroupBox("status");
    auto status_group_box_layout = new QHBoxLayout();
    m_status_label = new QLabel("-");
    status_group_box_layout->addWidget(m_status_label);
    status_group_box->setLayout(status_group_box_layout);
    layout->addWidget(status_group_box);

    resize(1200, 600);
}

void Widget::StartCalibration() {
    m_task_button->setEnabled(false);
    m_clear_calibration_button->setEnabled(false);
    m_save_calibration_button->setEnabled(false);
    m_load_calibration_button->setEnabled(false);

    m_calibration_running = true;
    auto pixel_image_codec_worker = m_pixel_image_codec_worker.get();
    auto get_transform_fn = [this] {
        std::lock_guard<std::mutex> lock(m_transform_mtx);
        return m_transform;
    };
    m_fetch_image_thread = std::make_unique<std::thread>(&PixelImageCodecWorker::FetchImageWorker, pixel_image_codec_worker, std::ref(m_calibration_running), std::ref(m_frame_q));
    auto calibrate_worker_fn = [this, pixel_image_codec_worker, get_transform_fn](CalibrateCb calibrate_cb, SendResultCb send_result_cb, CalibrateProgressCb calibrate_progress_cb) {
        pixel_image_codec_worker->CalibrateWorker(m_frame_q, m_dim, get_transform_fn, calibrate_cb, send_result_cb, calibrate_progress_cb);
    };
    m_calibrate_thread = std::make_unique<CalibrateThread>(this, calibrate_worker_fn);
    connect(m_calibrate_thread.get(), &CalibrateThread::SendCalibration, this, &Widget::ReceiveCalibration);
    connect(m_calibrate_thread.get(), &CalibrateThread::SendResult, this, &Widget::ShowResult);
    connect(m_calibrate_thread.get(), &CalibrateThread::SendProgress, this, &Widget::ShowCalibrationProgress);
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
        //m_transform_group_box->setEnabled(false);
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
    //m_transform_group_box->setEnabled(true);
    m_clear_calibration_button->setEnabled(false);
    m_save_calibration_button->setEnabled(false);
}

void Widget::StartTask() {
    m_calibrate_button->setEnabled(false);
    m_clear_calibration_button->setEnabled(false);
    m_save_calibration_button->setEnabled(false);
    m_load_calibration_button->setEnabled(false);

    m_task_running = true;
    auto pixel_image_codec_worker = m_pixel_image_codec_worker.get();
    auto get_transform_fn = [this] {
        std::lock_guard<std::mutex> lock(m_transform_mtx);
        return m_transform;
    };
    m_fetch_image_thread = std::make_unique<std::thread>(&PixelImageCodecWorker::FetchImageWorker, pixel_image_codec_worker, std::ref(m_task_running), std::ref(m_frame_q));
    auto decode_result_worker_fn = [this, pixel_image_codec_worker, get_transform_fn](SendResultCb send_result_cb) {
        pixel_image_codec_worker->DecodeResultWorker(m_frame_q, m_dim, get_transform_fn, m_calibration, send_result_cb, 500);
    };
    m_decode_result_thread = std::make_unique<DecodeResultThread>(this, decode_result_worker_fn);
    connect(m_decode_result_thread.get(), &DecodeResultThread::SendResult, this, &Widget::ShowResult);
    m_decode_result_thread->start();
    for (int i = 0; i < m_mp; ++i) {
        m_decode_image_threads.emplace_back(&PixelImageCodecWorker::DecodeImageWorker, pixel_image_codec_worker, std::ref(m_part_q), std::ref(m_frame_q), m_image_dir_path, m_dim, get_transform_fn, m_calibration);
    }
    auto save_part_worker_fn = [this, pixel_image_codec_worker](SavePartProgressCb save_part_progress_cb, SavePartCompleteCb save_part_complete_cb, SavePartErrorCb save_part_error_cb) {
        pixel_image_codec_worker->SavePartWorker(m_task_running, m_part_q, m_output_file, m_dim, m_pixel_size, m_space_size, m_part_num, save_part_progress_cb, [] {}, save_part_complete_cb, save_part_error_cb);
    };
    m_save_part_thread = std::make_unique<SavePartThread>(this, save_part_worker_fn);
    connect(m_save_part_thread.get(), &SavePartThread::SendProgress, this, &Widget::ShowTaskProgress);
    connect(m_save_part_thread.get(), &SavePartThread::SendComplete, this, &Widget::TaskComplete);
    connect(m_save_part_thread.get(), &SavePartThread::SendError, this, &Widget::ErrorMsg);
    m_save_part_thread->start();
}

void Widget::StopTask() {
    m_task_running = false;
    m_fetch_image_thread->join();
    m_fetch_image_thread.reset();
    for (size_t i = 0; i < m_mp + 1; ++i) {
        m_frame_q.PushNull();
    }
    m_decode_result_thread->quit();
    m_decode_result_thread->wait();
    m_decode_result_thread.reset();
    for (auto& e : m_decode_image_threads) {
        e.join();
    }
    m_decode_image_threads.clear();
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

void Widget::closeEvent(QCloseEvent* event) {
    if (m_calibration_running) StopCalibration();
    if (m_task_running) StopTask();
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
    m_image_label->clear();
    m_result_label->setText("-");
    for (auto e1 : m_result_image_labels) {
        for (auto e2 : e1) {
            e2->clear();
        }
    }
    m_status_label->setText("-");
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

void Widget::SaveTransform() {
    std::lock_guard<std::mutex> lock(m_transform_mtx);
    m_transform.Save(m_output_file + ".transform");
}

void Widget::LoadTransform() {
    auto transform_path = m_output_file + ".transform";
    if (std::filesystem::is_regular_file(transform_path)) {
        std::lock_guard<std::mutex> lock(m_transform_mtx);
        m_transform.Load(transform_path);
        m_bbox_line_edit->setText(get_bbox_str(m_transform.bbox).c_str());
        m_sphere_line_edit->setText(get_sphere_str(m_transform.sphere).c_str());
        m_filter_level_line_edit->setText(get_filter_level_str(m_transform.filter_level).c_str());
        m_binarization_threshold_line_edit->setText(get_binarization_threshold_str(m_transform.binarization_threshold).c_str());
        m_pixelization_threshold_line_edit->setText(get_pixelization_threshold_str(m_transform.pixelization_threshold).c_str());
    } else {
        QMessageBox::warning(this, "Warning", std::string("can't find transform file '" + transform_path + "'").c_str(), QMessageBox::Ok, QMessageBox::Ok);
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
        {
            std::lock_guard<std::mutex> lock(m_transform_mtx);
            m_transform = transform;
        };
    }
    catch (const invalid_transform_argument& e) {
    }
}

void Widget::ReceiveCalibration(Calibration calibration) {
    m_calibration = calibration;
}

void Widget::ShowResult(cv::Mat img, bool success, std::vector<std::vector<cv::Mat>> result_imgs) {
    if (m_calibration_running || m_task_running) {
        m_image = img;
        m_image_label->setPixmap(QPixmap::fromImage(QImage(img.data, img.cols, img.rows, img.cols * img.channels(), QImage::Format_BGR888)));
        m_result_label->setText(success ? "pass" : "fail");
        for (int tile_y_id = 0; tile_y_id < m_dim.tile_y_num; ++tile_y_id) {
            for (int tile_x_id = 0; tile_x_id < m_dim.tile_x_num; ++tile_x_id) {
                const auto& result_img = result_imgs[tile_y_id][tile_x_id];
                m_result_images[tile_y_id][tile_x_id] = result_img;
                m_result_image_labels[tile_y_id][tile_x_id]->setPixmap(QPixmap::fromImage(QImage(result_img.data, result_img.cols, result_img.rows, result_img.cols * result_img.channels(), QImage::Format_BGR888)));
            }
        }
    }
};

void Widget::ShowCalibrationProgress(uint64_t frame_num, float fps) {
    if (m_calibration_running) {
        std::ostringstream oss;
        oss << frame_num << " frames processed, fps=" << std::fixed << std::setprecision(2) << fps;
        m_status_label->setText(oss.str().c_str());
    }
}

void Widget::ShowTaskProgress(uint32_t done_part_num, uint32_t part_num, uint64_t frame_num, float fps, float done_fps, float bps, int64_t left_seconds) {
    if (m_task_running) {
        std::ostringstream oss;
        oss << done_part_num << "/" << part_num << " parts transferred, " << frame_num << " frames processed, fps=" << std::fixed << std::setprecision(2) << fps << ", done_fps=" << std::fixed << std::setprecision(2) << done_fps << ", bps=" << std::fixed << std::setprecision(0) << bps << ", left_seconds=" << left_seconds;
        m_status_label->setText(oss.str().c_str());
    }
}

void Widget::TaskComplete() {
    QMessageBox::information(this, "Info", "transfer done", QMessageBox::Ok, QMessageBox::Ok);
    //close();
    StopTask();
}

void Widget::ErrorMsg(std::string msg) {
    QMessageBox::critical(this, "Error", msg.c_str(), QMessageBox::Ok, QMessageBox::Ok);
    close();
}
