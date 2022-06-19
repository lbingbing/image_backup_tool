#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QProgressBar>
#include <QThread>

#include "image_codec.h"

class CalibrateThread : public QThread
{
    Q_OBJECT

public:
    CalibrateThread(QObject* parent, std::function<void(CalibrateCb, SendCalibrationImageResultCb, CalibrationProgressCb)> worker_fn);

signals:
    void SendCalibration(Calibration);
    void SendCalibrationImageResult(cv::Mat, bool, std::vector<std::vector<cv::Mat>>);
    void SendCalibrationProgress(CalibrationProgress calibration_progress);

protected:
    void run() override;

private:
    std::function<void(CalibrateCb, SendCalibrationImageResultCb, CalibrationProgressCb)> m_worker_fn;
};

class DecodeImageResultThread : public QThread
{
    Q_OBJECT

public:
    DecodeImageResultThread(QObject* parent, std::function<void(SendDecodeImageResultCb)> worker_fn);

signals:
    void SendDecodeImageResult(cv::Mat, bool, std::vector<std::vector<cv::Mat>>);

protected:
    void run() override;

private:
    std::function<void(SendDecodeImageResultCb)> m_worker_fn;
};

class AutoTransformThread : public QThread
{
    Q_OBJECT

public:
    AutoTransformThread(QObject* parent, std::function<void(SendAutoTransformCb)> worker_fn);

signals:
    void SendAutoTransform(Transform transform);

protected:
    void run() override;

private:
    std::function<void(SendAutoTransformCb)> m_worker_fn;
};

class SavePartThread : public QThread
{
    Q_OBJECT

public:
    SavePartThread(QObject* parent, std::function<void(SavePartProgressCb, SavePartCompleteCb, SavePartErrorCb)> worker_fn);

signals:
    void SendTaskProgress(TaskProgress task_progress);
    void SendTaskComplete();
    void SendTaskError(std::string msg);

protected:
    void run() override;

private:
    std::function<void(SavePartProgressCb, SavePartCompleteCb, SavePartErrorCb)> m_worker_fn;
};

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget* parent, const std::string& output_file, const Dim& dim, PixelType pixel_type, int pixel_size, int space_size, uint32_t part_num, int mp);

private slots:
    void ToggleCalibrationStartStop();
    void SaveCalibration();
    void LoadCalibration();
    void ClearCalibration();
    void ToggleTaskStartStop();
    void ReceiveCalibration(Calibration calibration);
    void ShowResult(cv::Mat img, bool success, std::vector<std::vector<cv::Mat>> result_imgs);
    void ShowCalibrationProgress(CalibrationProgress calibration_progress);
    void ShowTaskProgress(TaskProgress task_progress);
    void ToggleMonitor();
    void SaveImage();
    void ToggleTaskStatusServer();
    void SaveTransform();
    void LoadTransform();
    void TransformChanged();
    void UpdateAutoTransform(Transform transform);
    void TaskComplete();
    void ErrorMsg(std::string msg);
    void closeEvent(QCloseEvent* event) override;

private:
    void StartCalibration();
    void StopCalibration();
    void StartTask();
    void StopTask();
    void ClearStatus();
    void ClearImages();
    void UpdateTransform(const Transform& transform);
    void UpdateTransformUI(const Transform& transform);

    std::string m_output_file;
    Dim m_dim;
    std::unique_ptr<PixelImageCodecWorker> m_pixel_image_codec_worker;
    int m_pixel_size = 0;
    int m_space_size = 0;
    uint32_t m_part_num = 0;
    int m_mp = 0;
    Transform m_transform;
    Calibration m_calibration;
    cv::Mat m_image;
    std::vector<std::vector<cv::Mat>> m_result_images;
    ThreadSafeQueue<std::pair<uint64_t, cv::Mat>> m_frame_q{128};
    ThreadSafeQueue<DecodeResult> m_part_q{128};

    std::unique_ptr<std::thread> m_fetch_image_thread;
    std::unique_ptr<CalibrateThread> m_calibrate_thread;
    std::vector<std::thread> m_decode_image_threads;
    std::unique_ptr<DecodeImageResultThread> m_decode_image_result_thread;
    std::unique_ptr<AutoTransformThread> m_auto_transform_thread;
    std::unique_ptr<SavePartThread> m_save_part_thread;
    std::mutex m_transform_mtx;
    std::atomic<bool> m_calibration_running = false;
    std::atomic<bool> m_task_running = false;
    bool m_monitor_on = true;
    bool m_task_status_server_on = false;
    TaskStatusServer m_task_status_server;

    QLabel* m_image_label = nullptr;
    std::vector<std::vector<QLabel*>> m_result_image_labels;
    QPushButton* m_calibrate_button = nullptr;
    QPushButton* m_clear_calibration_button = nullptr;
    QPushButton* m_save_calibration_button = nullptr;
    QPushButton* m_load_calibration_button = nullptr;
    QPushButton* m_task_button = nullptr;
    QPushButton* m_monitor_button = nullptr;
    QPushButton* m_save_image_button = nullptr;
    QPushButton* m_task_status_server_button = nullptr;
    QLabel* m_task_status_server_port_label = nullptr;
    QLineEdit* m_task_status_server_port_line_edit = nullptr;
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
    QProgressBar* m_task_progress_bar = nullptr;
};
