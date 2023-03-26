#pragma once

#include <array>
#include <chrono>

#include <QtWidgets/QWidget>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QTimer>

#include "image_codec.h"

struct Parameters {
    std::array<int, 2> tile_x_num_range{1, 10};
    std::array<int, 2> tile_y_num_range{1, 10};
    std::array<int, 2> tile_x_size_range{1, 500};
    std::array<int, 2> tile_y_size_range{1, 500};
    std::array<int, 2> pixel_size_range{1, 100};
    std::array<int, 2> space_size_range{1, 10};
    std::array<int, 2> calibration_pixel_size_range{1, 10};
    std::array<int, 2> interval_range{1, 1000};
};

enum class State {
    CONFIG = 0,
    CALIBRATE = 1,
    DISPLAY = 2,
};

struct Context {
    State state = State::CONFIG;
    SymbolType symbol_type = SymbolType::SYMBOL1;
    int tile_x_num = 0;
    int tile_y_num = 0;
    int tile_x_size = 0;
    int tile_y_size = 0;
    int pixel_size = 0;
    int space_size = 0;
    int calibration_pixel_size = 0;
    std::string task_status_server;
};

enum class CalibrationMode {
    POSITION = 0,
    COLOR = 1,
};

class CalibrationPage : public QWidget {
    Q_OBJECT

public:
    CalibrationPage(QWidget* parent, const Parameters& parameters, Context& context);
    void ToggleCalibrationStartStop();

    QComboBox* m_symbol_type_combo_box = nullptr;
    QSpinBox* m_tile_x_num_spin_box = nullptr;
    QSpinBox* m_tile_y_num_spin_box = nullptr;
    QSpinBox* m_tile_x_size_spin_box = nullptr;
    QSpinBox* m_tile_y_size_spin_box = nullptr;
    QSpinBox* m_pixel_size_spin_box = nullptr;
    QSpinBox* m_space_size_spin_box = nullptr;
    QSpinBox* m_calibration_pixel_size_spin_box = nullptr;

signals:
    void CalibrationStarted(CalibrationMode calibration_mode, std::vector<uint8_t> data);
    void CalibrationStopped();

private:
    const Parameters& m_parameters;
    Context& m_context;

    QFrame* m_config_frame = nullptr;
    QComboBox* m_calibration_mode_combo_box = nullptr;
};

enum class DisplayMode {
    MANUAL = 0,
    AUTO = 1,
};

class TaskPage : public QWidget {
    Q_OBJECT

public:
    TaskPage(QWidget* parent, const Parameters& parameters, Context& context);
    void NavigateNextPart();
    void NavigatePrevPart();
    bool UpdateTaskStatus();

    QComboBox* m_symbol_type_combo_box = nullptr;
    QSpinBox* m_tile_x_num_spin_box = nullptr;
    QSpinBox* m_tile_y_num_spin_box = nullptr;
    QSpinBox* m_tile_x_size_spin_box = nullptr;
    QSpinBox* m_tile_y_size_spin_box = nullptr;
    QSpinBox* m_pixel_size_spin_box = nullptr;
    QSpinBox* m_space_size_spin_box = nullptr;

public slots:
    void ToggleDisplayMode();

signals:
    void TaskStarted();
    void PartNavigated(std::vector<uint8_t> data);
    void TaskStopped();

private slots:
    void OpenTargetFile();
    void ToggleTaskMode(int state);
    void OpenTaskFile();
    void ToggleTaskStartStop();
    void SetInterval(int interval);
    void NavigatePart(uint32_t part_id);

private:
    bool ValidateConfig();
    void Draw(uint32_t part_id);
    bool IsTaskStatusServerOn();
    bool IsTaskStatusAutoUpdate();
    bool FetchTaskStatus();

    const Parameters& m_parameters;
    Context& m_context;

    std::string m_target_file_path;
    Bytes m_raw_bytes;
    int m_part_byte_num = 0;
    uint32_t m_part_num = 0;
    std::unique_ptr<TaskStatusClient> m_task_status_client;
    size_t m_task_status_auto_update_threshold = 200;
    std::vector<uint32_t> m_undone_part_ids;
    size_t m_cur_undone_part_id_index = 0;
    uint32_t m_cur_part_id = 0;
    DisplayMode m_display_mode = DisplayMode::MANUAL;
    int m_interval = 50;

    std::unique_ptr<SymbolCodec> m_symbol_codec;

    int m_auto_navigate_update_fps_interval = int(2000.f / m_interval);
    int m_auto_navigate_frame_num = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_auto_navigate_time;
    float m_auto_navigate_fps = 0;

    QTimer m_timer;

    QPushButton* m_task_button = nullptr;
    QFrame* m_task_frame = nullptr;
    QLineEdit* m_target_file_line_edit = nullptr;
    QFrame* m_config_frame = nullptr;
    QFrame* m_task_file_frame = nullptr;
    QLineEdit* m_task_file_line_edit = nullptr;
    QComboBox* m_task_status_server_type_combo_box = nullptr;
    QFrame* m_task_status_server_frame = nullptr;
    QLineEdit* m_task_status_server_line_edit = nullptr;
    QCheckBox* m_task_status_auto_update_checkbox = nullptr;
    QFrame* m_display_config_frame = nullptr;
    QPushButton* m_display_mode_button = nullptr;
    QSpinBox* m_interval_spin_box = nullptr;
    QLabel* m_auto_navigate_fps_value_label = nullptr;
    QSpinBox* m_cur_part_id_spin_box = nullptr;
    QLabel* m_max_part_id_label = nullptr;
    QLabel* m_undone_part_id_num_label = nullptr;
};

class Canvas : public QWidget {
    Q_OBJECT

public:
    Canvas(QWidget* parent, const Parameters& parameters, const Context& context);
    void DrawCalibration(CalibrationMode calibration_mode, const std::vector<uint8_t>& data);

public slots:
    void DrawPart(std::vector<uint8_t> data);
    void Clear();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    bool IsTileBorder(int x, int y);

    const Parameters& m_parameters;
    const Context& m_context;

    bool m_is_calibration = false;
    CalibrationMode m_calibration_mode;
    std::vector<uint8_t> m_data;
};

class Widget : public QWidget {
    Q_OBJECT

public:
    Widget(QWidget* parent);
    bool eventFilter(QObject* obj, QEvent* event);

private slots:
    void StartCalibration(CalibrationMode calibration_mode, std::vector<uint8_t> data);
    void StopCalibration();
    void StartTask();
    void StopTask();
    void ToggleFullScreen();

private:
    void LoadConfig();
    void SetSymbolType(int index) { m_context.symbol_type = static_cast<SymbolType>(index); }
    void SetTileXNum(int tile_x_num) { m_context.tile_x_num = tile_x_num; }
    void SetTileYNum(int tile_y_num) { m_context.tile_y_num = tile_y_num; }
    void SetTileXSize(int tile_x_size) { m_context.tile_x_size = tile_x_size; }
    void SetTileYSize(int tile_y_size) { m_context.tile_y_size = tile_y_size; }
    void SetPixelSize(int pixel_size) { m_context.pixel_size = pixel_size; }
    void SetSpaceSize(int space_size) { m_context.space_size = space_size; }
    void SetCalibrationPixelSize(int calibration_pixel_size) { m_context.calibration_pixel_size = calibration_pixel_size; }

    Parameters m_parameters;
    Context m_context;

    CalibrationPage* m_calibration_page = nullptr;
    TaskPage* m_task_page = nullptr;
    Canvas* m_canvas = nullptr;
    QTabWidget* m_control_tab = nullptr;
};
