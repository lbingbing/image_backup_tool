#include <sstream>
#include <map>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <algorithm>

#include "image_decode_worker.h"
#include "image_stream.h"

ImageDecodeWorker::ImageDecodeWorker(SymbolType symbol_type, const Dim& dim) : m_image_decoder(symbol_type, dim) {
}

void ImageDecodeWorker::FetchImageWorker(std::atomic<bool>& running, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, int interval) {
    uint64_t frame_id = 0;
    while (running) {
        auto image_stream = create_image_stream();
        while (running) {
            auto frame = image_stream->GetFrame();
            if (frame.empty()) break;
            frame_q.Emplace(frame_id, frame);
            ++frame_id;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }
}

void ImageDecodeWorker::CalibrateWorker(ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, GetTransformCb get_transform_cb, CalibrateCb calibrate_cb, SendCalibrationImageResultCb send_calibration_image_result_cb, CalibrationProgressCb calibration_progress_cb) {
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t frame_num = 0;
    float fps = 0;
    uint64_t frame_num0 = 0;
    while (true) {
        auto data = frame_q.Pop();
        if (!data) break;
        auto& [frame_id, frame] = data.value();
        auto [frame1, calibration, result_imgs] = m_image_decoder.Calibrate(frame, get_transform_cb(), true);
        ++frame_num;
        if ((frame_num & 0x1f) == 0) {
            auto t1 = std::chrono::high_resolution_clock::now();
            auto delta_t = std::chrono::duration_cast<std::chrono::duration<float>>(t1 - t0).count();
            t0 = t1;
            auto delta_frame_num = frame_num - frame_num0;
            frame_num0 = frame_num;
            auto fps1 = delta_frame_num / delta_t;
            fps = fps * 0.5f + fps1 * 0.5f;
        }
        calibrate_cb(calibration);
        send_calibration_image_result_cb(frame1, calibration.valid, result_imgs);
        calibration_progress_cb({frame_num, fps});
    }
}

void ImageDecodeWorker::DecodeImageWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, GetTransformCb get_transform_cb, const Calibration& calibration) {
    uint64_t frame_num = 0;
    Transform transform = get_transform_cb();
    while (true) {
        auto data = frame_q.Pop();
        if (!data) break;
        auto& [frame_id, frame] = data.value();
        auto [success, part_id, part_bytes, part_symbols, frame1, result_imgs] = m_image_decoder.Decode(frame, transform, calibration, false);
        part_q.Emplace(success, part_id, part_bytes);
        ++frame_num;
        if ((frame_num & 0x1f) == 0) {
            transform = get_transform_cb();
        }
    }
}

void ImageDecodeWorker::DecodeResultWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, GetTransformCb get_transform_cb, const Calibration& calibration, SendDecodeImageResultCb send_decode_image_result_cb) {
    while (true) {
        auto data = frame_q.Front();
        if (!data) {
            frame_q.Pop();
            break;
        }
        auto& [frame_id, frame] = data.value();
        auto [success, part_id, part_bytes, part_symbols, frame1, result_imgs] = m_image_decoder.Decode(frame, get_transform_cb(), calibration, true);
        part_q.Emplace(success, part_id, part_bytes);
        send_decode_image_result_cb(frame1, success, result_imgs);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void ImageDecodeWorker::AutoTransformWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, GetTransformCb get_transform_cb, const Calibration& calibration, SendAutoTransformCb send_auto_trasform_cb) {
    constexpr std::array<int, 2> PIXELIZATION_CHANNEL_RANGE{150, 180};
    constexpr int PIXELIZATION_CHANNEL_DIFF = 3;
    constexpr int LOOP_NUM = 8;
    uint64_t frame_num = 0;
    std::vector<Transform::PixelizationThreshold> pixelization_thresholds;
    for (int t = PIXELIZATION_CHANNEL_RANGE[0]; t < PIXELIZATION_CHANNEL_RANGE[1]; ++t) {
        pixelization_thresholds.push_back({t, t, t});
    }
    for (int t1 = PIXELIZATION_CHANNEL_RANGE[0]; t1 < PIXELIZATION_CHANNEL_RANGE[1]; ++t1) {
        for (int t2 = PIXELIZATION_CHANNEL_RANGE[0]; t2 < PIXELIZATION_CHANNEL_RANGE[1]; ++t2) {
            for (int t3 = PIXELIZATION_CHANNEL_RANGE[0]; t3 < PIXELIZATION_CHANNEL_RANGE[1]; ++t3) {
                if (t1 != t2 && t2 != t3 && t3 != t1 &&
                    std::abs(t1 - t2) <= PIXELIZATION_CHANNEL_DIFF &&
                    std::abs(t2 - t3) <= PIXELIZATION_CHANNEL_DIFF &&
                    std::abs(t3 - t1) <= PIXELIZATION_CHANNEL_DIFF) {
                    pixelization_thresholds.push_back({t1, t2, t3});
                }
            }
        }
    }
    std::vector<AutoTransform> auto_transforms;
    for (const auto& e1 : pixelization_thresholds) {
        auto_transforms.emplace_back(e1);
    }
    int cur_auto_transform_index = 0;
    std::map<AutoTransform, float> auto_transform_scores;
    while (true) {
        auto data = frame_q.Front();
        if (!data) {
            frame_q.Pop();
            break;
        }
        auto& [frame_id, frame] = data.value();
        auto transform = get_transform_cb();
        bool has_succeeded = false;
        for (int loop_id = 0; loop_id < LOOP_NUM; ++loop_id) {
            const auto& auto_transform = auto_transforms[cur_auto_transform_index];
            std::tie(transform.pixelization_threshold) = auto_transform;
            auto [success, part_id, part_bytes, part_symbols, frame1, result_imgs] = m_image_decoder.Decode(frame, transform, calibration, false);
            if (!has_succeeded && (success || loop_id == LOOP_NUM - 1)) {
                part_q.Emplace(success, part_id, part_bytes);
                has_succeeded = true;
            }
            auto_transform_scores[auto_transform] = auto_transform_scores[auto_transform] * 0.75f + static_cast<float>(success) * 0.25f;
            cur_auto_transform_index = (cur_auto_transform_index + 1) % auto_transforms.size();
        }
        ++frame_num;
        if ((frame_num & 0x1f) == 0) {
            auto it = std::max_element(auto_transform_scores.begin(), auto_transform_scores.end(), [](const std::pair<AutoTransform, float>& e1, const std::pair<AutoTransform, float>& e2) { return e1.second < e2.second; });
            const auto& max_score_auto_transform = it->first;
            const auto& max_score = it->second;
            if (max_score > 0) {
                std::tie(transform.pixelization_threshold) = max_score_auto_transform;
                send_auto_trasform_cb(transform);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void ImageDecodeWorker::SavePartWorker(std::atomic<bool>& running, ThreadSafeQueue<DecodeResult>& part_q, std::string output_file, uint32_t part_num, SavePartProgressCb save_part_progress_cb, SavePartFinishCb save_part_finish_cb, SavePartCompleteCb save_part_complete_cb, SavePartErrorCb error_cb, Task::FinalizationStartCb finalization_start_cb, Task::FinalizationProgressCb finalization_progress_cb, Task::FinalizationCompleteCb finalization_complete_cb, TaskStatusServer* task_status_server) {
    auto symbol_type = m_image_decoder.GetSymbolCodec().GetSymbolType();
    auto dim = m_image_decoder.GetDim();
    Task task(output_file);
    if (std::filesystem::is_regular_file(task.TaskPath())) {
        task.Load();
        if (symbol_type != task.GetSymbolType() || dim != task.GetDim() || part_num != task.GetPartNum()) {
            if (error_cb) {
                std::ostringstream oss;
                oss << "inconsistent task config\n";
                oss << "task:\n";
                oss << "symbol_type=" << get_symbol_type_str(task.GetSymbolType()) << "\n";
                oss << "dim=" << task.GetDim() << "\n";
                oss << "part_num=" << task.GetPartNum() << "\n";
                oss << "input:\n";
                oss << "symbol_type=" << get_symbol_type_str(symbol_type) << "\n";
                oss << "dim=" << dim << "\n";
                oss << "part_num=" << part_num << "\n";
                error_cb(oss.str());
            }
            running = false;
            while (part_q.Pop());
            return;
        }
    } else {
        task.Init(symbol_type, dim, part_num);
        bool success = task.AllocateBlob();
        if (!success) {
            if (error_cb) {
                std::ostringstream oss;
                oss << "can't allocate file '" << task.BlobPath() << "'\n";
                error_cb(oss.str());
            }
            running = false;
            while (part_q.Pop());
            return;
        }
    }
    task.SetFinalizationCb(finalization_start_cb, finalization_progress_cb, finalization_complete_cb);
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t frame_num = 0;
    float fps = 0;
    uint64_t frame_num0 = 0;
    float done_fps = 0;
    uint32_t done_part_num0 = task.DonePartNum();
    float bps = 0;
    float bpf = static_cast<float>(get_part_byte_num(symbol_type, dim));
    int left_days = 0;
    int left_hours = 0;
    int left_minutes = 0;
    int left_seconds = 0;
    while (true) {
        auto data = part_q.Pop();
        if (!data) break;
        auto& [success, part_id, part_bytes] = data.value();
        if (success) task.UpdatePart(part_id, part_bytes);
        ++frame_num;
        if ((frame_num & 0x3f) == 0) {
            auto t1 = std::chrono::high_resolution_clock::now();
            auto delta_t = std::max(std::chrono::duration_cast<std::chrono::duration<float>>(t1 - t0).count(), 0.001f);
            t0 = t1;
            auto delta_frame_num = frame_num - frame_num0;
            frame_num0 = frame_num;
            auto fps1 = delta_frame_num / delta_t;
            fps = fps * 0.5f + fps1 * 0.5f;
            auto delta_done_part_num = task.DonePartNum() - done_part_num0;
            done_part_num0 = task.DonePartNum();
            auto done_fps1 = delta_done_part_num / delta_t;
            done_fps = done_fps * 0.5f + done_fps1 * 0.5f;
            bps = done_fps * bpf;
            if (done_fps > 0.01f) {
                int64_t left_total_seconds = static_cast<int64_t>((part_num - task.DonePartNum()) / done_fps);
                left_days = left_total_seconds / (24 * 60 * 60);
                left_total_seconds -= left_days * (24 * 60 * 60);
                left_hours = left_total_seconds / (60 * 60);
                left_total_seconds -= left_hours * (60 * 60);
                left_minutes = left_total_seconds / 60;
                left_seconds = left_total_seconds - left_minutes * 60;
            } else {
                left_days = 0;
                left_hours = 0;
                left_minutes = 0;
                left_seconds = 0;
            }
        }
        if ((frame_num & 0x1f) == 0) {
            if (save_part_progress_cb) save_part_progress_cb({frame_num, task.DonePartNum(), part_num, fps, done_fps, bps, left_days, left_hours, left_minutes, left_seconds});
        }
        if (task_status_server && (task_status_server->NeedUpdateTaskStatus() || (task_status_server->IsRunning() && (frame_num & 0xff) == 0))) {
            task_status_server->UpdateTaskStatus(task.ToTaskBytes());
        }
        if (task.IsDone()) {
            if (save_part_progress_cb) save_part_progress_cb({frame_num, task.DonePartNum(), part_num, fps, done_fps, bps, left_days, left_hours, left_minutes, left_seconds});
            task.Finalize();
            if (save_part_complete_cb) save_part_complete_cb();
            running = false;
            while (part_q.Pop());
            break;
        }
    }
    if (!task.IsDone()) {
        task.Flush();
    }
    if (save_part_finish_cb) save_part_finish_cb();
}
