#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QThread>

#include "image_codec.h"

class CalibrateThread : public QThread
{
    Q_OBJECT

public:
    CalibrateThread(QObject* parent, std::function<void(CalibrateCb, SendResultCb, CalibrateProgressCb)> worker_fn);

signals:
    void SendCalibration(Calibration);
    void SendResult(cv::Mat, bool, std::vector<std::vector<cv::Mat>>);
    void SendProgress(uint64_t frame_num, float fps);

protected:
    void run() override;

private:
    std::function<void(CalibrateCb, SendResultCb, CalibrateProgressCb)> m_worker_fn;
};

class DecodeResultThread : public QThread
{
    Q_OBJECT

public:
    DecodeResultThread(QObject* parent, std::function<void(SendResultCb)> worker_fn);

signals:
    void SendResult(cv::Mat, bool, std::vector<std::vector<cv::Mat>>);

protected:
    void run() override;

private:
    std::function<void(SendResultCb)> m_worker_fn;
};

class SavePartThread : public QThread
{
    Q_OBJECT

public:
    SavePartThread(QObject* parent, std::function<void(SavePartProgressCb, SavePartCompleteCb, SavePartErrorCb)> worker_fn);

signals:
    void SendProgress(uint32_t done_part_num, uint32_t part_num, uint64_t frame_num, float fps, float done_fps, float bps, int64_t left_seconds);
    void SendComplete();
    void SendError(std::string msg);

protected:
    void run() override;

private:
    std::function<void(SavePartProgressCb, SavePartCompleteCb, SavePartErrorCb)> m_worker_fn;
};

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget* parent, const std::string& output_file, const Dim& dim, PixelType pixel_type, int pixel_size, int space_size, uint32_t part_num, const std::string& image_dir_path, int mp);

private slots:
    void ToggleCalibrationStartStop();
    void ToggleTaskStartStop();
    void closeEvent(QCloseEvent* event) override;
    void ReceiveCalibration(Calibration calibration);
    void ShowResult(cv::Mat img, bool success, std::vector<std::vector<cv::Mat>> result_imgs);
    void ShowCalibrationProgress(uint64_t frame_num, float fps);
    void ShowTaskProgress(uint32_t done_part_num, uint32_t part_num, uint64_t frame_num, float fps, float done_fps, float bps, int64_t left_seconds);
    void TaskComplete();
    void ErrorMsg(std::string msg);

private:
    void StartCalibration();
    void StopCalibration();
    void SaveCalibration();
    void LoadCalibration();
    void ClearCalibration();
    void StartTask();
    void StopTask();
    void ClearStatus();
    void SaveImage();
    void SaveTransform();
    void LoadTransform();
    void TransformChanged();

    std::string m_output_file;
    Dim m_dim;
    std::unique_ptr<PixelImageCodecWorker> m_pixel_image_codec_worker;
    int m_pixel_size = 0;
    int m_space_size = 0;
    uint32_t m_part_num = 0;
    std::string m_image_dir_path;
    int m_mp = 0;
    Transform m_transform;
    Calibration m_calibration;
    cv::Mat m_image;
    std::vector<std::vector<cv::Mat>> m_result_images;
    ThreadSafeQueue<std::pair<uint64_t, cv::Mat>> m_frame_q{60};
    ThreadSafeQueue<DecodeResult> m_part_q{60};

    std::unique_ptr<CalibrateThread> m_calibrate_thread;
    std::unique_ptr<DecodeResultThread> m_decode_result_thread;
    std::unique_ptr<std::thread> m_show_task_result_thread;
    std::unique_ptr<std::thread> m_fetch_image_thread;
    std::vector<std::thread> m_decode_image_threads;
    std::unique_ptr<SavePartThread> m_save_part_thread;
    std::mutex m_transform_mtx;
    std::atomic<bool> m_calibration_running = false;
    std::atomic<bool> m_task_running = false;

    QLabel* m_image_label = nullptr;
    std::vector<std::vector<QLabel*>> m_result_image_labels;
    QPushButton* m_calibrate_button = nullptr;
    QPushButton* m_clear_calibration_button = nullptr;
    QPushButton* m_save_calibration_button = nullptr;
    QPushButton* m_load_calibration_button = nullptr;
    QPushButton* m_task_button = nullptr;
    QPushButton* m_save_image_button = nullptr;
    QGroupBox* m_transform_group_box = nullptr;
    QPushButton* m_save_transform_button = nullptr;
    QPushButton* m_load_transform_button = nullptr;
    QLabel* m_bbox_label = nullptr;
    QLineEdit* m_bbox_line_edit = nullptr;
    QLabel* m_sphere_label = nullptr;
    QLineEdit* m_sphere_line_edit = nullptr;
    QLabel* m_filter_level_label = nullptr;
    QLineEdit* m_filter_level_line_edit = nullptr;
    QLabel* m_binarization_threshold_label = nullptr;
    QLineEdit* m_binarization_threshold_line_edit = nullptr;
    QLabel* m_pixelization_threshold_label = nullptr;
    QLineEdit* m_pixelization_threshold_line_edit = nullptr;
    QLabel* m_result_label = nullptr;
    QLabel* m_status_label = nullptr;
};
