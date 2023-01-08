#include <sstream>
#include <chrono>
#include <atomic>
#include <thread>
#include <filesystem>

#include "image_decode_worker.h"
#include "image_stream.h"

ImageDecodeWorker::ImageDecodeWorker(PixelType pixel_type) : m_image_decoder(pixel_type) {
}

void ImageDecodeWorker::FetchImageWorker(std::atomic<bool>& running, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, int interval) {
    cv::Mat frame;
    uint64_t frame_id = 0;
    while (running) {
        auto image_stream = create_image_stream();
        while (running) {
            frame = image_stream->GetFrame();
            if (frame.empty()) break;
            frame_q.Emplace(frame_id, frame);
            ++frame_id;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }
}

void ImageDecodeWorker::CalibrateWorker(ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, CalibrateCb calibrate_cb, SendCalibrationImageResultCb send_calibration_image_result_cb, CalibrationProgressCb calibration_progress_cb) {
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t frame_num = 0;
    float fps = 0;
    uint64_t frame_num0 = 0;
    while (true) {
        auto data = frame_q.Pop();
        if (!data) break;
        auto& [frame_id, frame] = data.value();
        auto [frame1, calibration, result_imgs] = m_image_decoder.Calibrate(frame, dim, get_transform_cb(), true);
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

void ImageDecodeWorker::DecodeImageWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration) {
    while (true) {
        auto data = frame_q.Pop();
        if (!data) break;
        auto& [frame_id, frame] = data.value();
        auto [success, part_id, part_bytes, part_pixels, frame1, result_imgs] = m_image_decoder.Decode(frame, dim, get_transform_cb(), calibration, false);
        part_q.Emplace(success, part_id, part_bytes);
    }
}

void ImageDecodeWorker::DecodeResultWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration, SendDecodeImageResultCb send_decode_image_result_cb, int interval) {
    while (true) {
        auto data = frame_q.Front();
        if (!data) {
            frame_q.Pop();
            break;
        }
        auto& [frame_id, frame] = data.value();
        auto [success, part_id, part_bytes, part_pixels, frame1, result_imgs] = m_image_decoder.Decode(frame, dim, get_transform_cb(), calibration, true);
        part_q.Emplace(success, part_id, part_bytes);
        send_decode_image_result_cb(frame1, success, result_imgs);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
}

void ImageDecodeWorker::AutoTransformWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration, SendAutoTransformCb send_auto_trasform_cb, int interval) {
    constexpr int THRESHOLD_NUM = 256;
    constexpr int LOOP_NUM = 4;
    uint64_t frame_num = 0;
    std::vector<float> pixelization_threshold_scores(THRESHOLD_NUM, 0.0f);
    int cur_pixelization_threshold = 0;
    while (true) {
        auto data = frame_q.Front();
        if (!data) {
            frame_q.Pop();
            break;
        }
        auto& [frame_id, frame] = data.value();
        auto transform = get_transform_cb();
        bool has_succeeded = false;
        for (int i = 0; i < LOOP_NUM; ++i) {
            transform.pixelization_threshold[0] = cur_pixelization_threshold;
            transform.pixelization_threshold[1] = cur_pixelization_threshold;
            transform.pixelization_threshold[2] = cur_pixelization_threshold;
            auto [success, part_id, part_bytes, part_pixels, frame1, result_imgs] = m_image_decoder.Decode(frame, dim, transform, calibration, false);
            if (!has_succeeded && (success || i == LOOP_NUM - 1)) {
                part_q.Emplace(success, part_id, part_bytes);
                has_succeeded = true;
            }
            pixelization_threshold_scores[cur_pixelization_threshold] = pixelization_threshold_scores[cur_pixelization_threshold] * 0.75f + static_cast<float>(success) * 0.25f;
            cur_pixelization_threshold = (cur_pixelization_threshold + 1) % THRESHOLD_NUM;
        }
        ++frame_num;
        if ((frame_num & 0x7f) == 0) {
            float total_score = 0.0f;
            float sum = 0.0f;
            for (int threshold = 0; threshold < THRESHOLD_NUM; threshold++) {
                total_score += pixelization_threshold_scores[threshold];
                sum += pixelization_threshold_scores[threshold] * threshold;
            }
            if (total_score > 0) {
                int threshold = sum / total_score;
                transform.pixelization_threshold[0] = threshold;
                transform.pixelization_threshold[1] = threshold;
                transform.pixelization_threshold[2] = threshold;
                send_auto_trasform_cb(transform);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
}

void ImageDecodeWorker::SavePartWorker(std::atomic<bool>& running, ThreadSafeQueue<DecodeResult>& part_q, std::string output_file, const Dim& dim, int pixel_size, int space_size, uint32_t part_num, SavePartProgressCb save_part_progress_cb, SavePartFinishCb save_part_finish_cb, SavePartCompleteCb save_part_complete_cb, SavePartErrorCb error_cb, Task::FinalizationStartCb finalization_start_cb, Task::FinalizationProgressCb finalization_progress_cb, Task::FinalizationCompleteCb finalization_complete_cb, TaskStatusServer* task_status_server) {
    PixelType pixel_type = m_image_decoder.GetPixelCodec().GetPixelType();
    Task task(output_file);
    if (std::filesystem::is_regular_file(task.TaskPath())) {
        task.Load();
        if (dim != task.GetDim() || pixel_size != task.GetPixelSize() || part_num != task.GetPartNum()) {
            if (error_cb) {
                std::ostringstream oss;
                oss << "inconsistent task config\n";
                oss << "task:\n";
                oss << "dim=" << task.GetDim() << "\n";
                oss << "pixel_type=" << get_pixel_type_str(task.GetPixelType()) << "\n";
                oss << "pixel_size=" << task.GetPixelSize() << "\n";
                oss << "space_size=" << task.GetSpaceSize() << "\n";
                oss << "part_num=" << task.GetPartNum() << "\n";
                oss << "input:\n";
                oss << "dim=" << dim << "\n";
                oss << "pixel_type=" << get_pixel_type_str(pixel_type) << "\n";
                oss << "pixel_size=" << pixel_size << "\n";
                oss << "space_size=" << space_size << "\n";
                oss << "part_num=" << part_num << "\n";
                error_cb(oss.str());
            }
            running = false;
            while (part_q.Pop());
            return;
        }
    } else {
        task.Init(dim, pixel_type, pixel_size, space_size, part_num);
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
    float bpf = static_cast<float>(get_part_byte_num(dim, pixel_type));
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
        if ((frame_num & 0x7f) == 0) {
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
        if ((frame_num & 0xf) == 0) {
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
