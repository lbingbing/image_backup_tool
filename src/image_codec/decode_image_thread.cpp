#include <sstream>
#include <atomic>
#include <filesystem>
#include <chrono>
#include <thread>

#include "decode_image_thread.h"
#include "image_fetcher.h"

void PixelImageCodecWorker::FetchImageWorker(std::atomic<bool>& running, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q) {
    while (running) {
        auto image_fetcher = create_image_fetcher();
        cv::Mat frame;
        uint64_t frame_id = 0;
        while (running) {
            frame = image_fetcher->GetFrame();
            if (frame.empty()) break;
            frame_q.Emplace(frame_id, frame);
            ++frame_id;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void PixelImageCodecWorker::CalibrateWorker(ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, CalibrateCb calibrate_cb, SendResultCb send_result_cb, CalibrateProgressCb calibrate_progress_cb) {
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t frame_num = 0;
    float fps = 0;
    uint64_t frame_num0 = 0;
    while (true) {
        auto data = frame_q.Pop();
        if (!data) break;
        auto& [frame_id, frame] = data.value();
        auto [frame1, calibration, result_imgs] = GetPixelImageCodec().Calibrate(frame, dim, get_transform_cb(), true);
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
        send_result_cb(frame1, calibration.valid, result_imgs);
        calibrate_progress_cb(frame_num, fps);
    }
}

void PixelImageCodecWorker::DecodeResultWorker(ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration, SendResultCb send_result_cb, int delay) {
    while (true) {
        auto data = frame_q.Front();
        if (!data) {
            frame_q.Pop();
            break;
        }
        auto& [frame_id, frame] = data.value();
        auto [success, part_id, part_bytes, part_pixels, frame1, result_imgs] = GetPixelImageCodec().Decode(frame, dim, get_transform_cb(), calibration, true);
        send_result_cb(frame1, success, result_imgs);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
}

void PixelImageCodecWorker::DecodeImageWorker(ThreadSafeQueue<DecodeResult>& part_q, ThreadSafeQueue<std::pair<uint64_t, cv::Mat>>& frame_q, std::string image_dir_path, const Dim& dim, GetTransformCb get_transform_cb, const Calibration& calibration) {
    while (true) {
        auto data = frame_q.Pop();
        if (!data) break;
        auto& [frame_id, frame] = data.value();
        if (!image_dir_path.empty()) {
            cv::imwrite(image_dir_path + "/" + std::to_string(frame_id) + ".jpg", frame);
        }
        auto [success, part_id, part_bytes, part_pixels, frame1, result_imgs] = GetPixelImageCodec().Decode(frame, dim, get_transform_cb(), calibration, false);
        part_q.Emplace(success, part_id, part_bytes);
    }
}

void PixelImageCodecWorker::SavePartWorker(std::atomic<bool>& running, ThreadSafeQueue<DecodeResult>& part_q, std::string output_file, const Dim& dim, int pixel_size, int space_size, uint32_t part_num, SavePartProgressCb progress_cb, SavePartFinishCb finish_cb, SavePartCompleteCb complete_cb, SavePartErrorCb error_cb, TaskStatusServer* task_status_server) {
    PixelType pixel_type = GetPixelImageCodec().GetPixelCodec().GetPixelType();
    Task task(output_file);
    if (std::filesystem::is_regular_file(task.TaskPath())) {
        task.Load();
        if (dim != task.GetDim() || pixel_size != task.GetPixelSize() || part_num != task.GetPartNum()) {
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
            running = false;
            while (part_q.Pop());
            return;
        }
    } else {
        task.Init(dim, pixel_type, pixel_size, space_size, part_num);
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    uint64_t frame_num = 0;
    float fps = 0;
    uint64_t frame_num0 = 0;
    float done_fps = 0;
    uint32_t done_part_num0 = task.DonePartNum();
    float bps = 0;
    float bpf = static_cast<float>(dim.tile_x_num * dim.tile_y_num * dim.tile_x_size * dim.tile_y_size * GetPixelImageCodec().GetPixelCodec().BitNumPerPixel()) / 8.0;
    uint64_t left_seconds = -1;
    while (true) {
        auto data = part_q.Pop();
        if (!data) break;
        auto& [success, part_id, part_bytes] = data.value();
        if (success) task.UpdatePart(part_id, part_bytes);
        ++frame_num;
        if ((frame_num & 0x1f) == 0) {
            auto t1 = std::chrono::high_resolution_clock::now();
            auto delta_t = std::chrono::duration_cast<std::chrono::duration<float>>(t1 - t0).count();
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
            left_seconds = static_cast<int64_t>(done_fps > 0.1f ? (part_num - task.DonePartNum()) / done_fps : -1);
        }
        progress_cb(task.DonePartNum(), part_num, frame_num, fps, done_fps, bps, left_seconds);
        if (task_status_server && task_status_server->IsRunning() && (frame_num & 0xff) == 0) {
            task_status_server->UpdateTaskStatus(task.GetTaskStatusBytes());
        }
        if (task.IsDone()) {
            complete_cb();
            running = false;
            while (part_q.Pop());
            break;
        }
    }
    finish_cb();
}

std::unique_ptr<PixelImageCodecWorker> create_pixel_image_codec_worker(PixelType pixel_type) {
    std::unique_ptr<PixelImageCodecWorker> pixel_image_codec_worker;
    if (pixel_type == PixelType::PIXEL2) {
        pixel_image_codec_worker = std::make_unique<Pixel2ImageCodecWorker>();
    } else if (pixel_type == PixelType::PIXEL4) {
        pixel_image_codec_worker = std::make_unique<Pixel4ImageCodecWorker>();
    } else if (pixel_type == PixelType::PIXEL8) {
        pixel_image_codec_worker = std::make_unique<Pixel8ImageCodecWorker>();
    } else {
        throw std::invalid_argument("invalid pixel type " + std::to_string(static_cast<int>(pixel_type)));
    }
    return pixel_image_codec_worker;
}
