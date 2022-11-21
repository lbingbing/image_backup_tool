#include <iostream>
#include <exception>
#include <thread>
#include <atomic>
#include <boost/program_options.hpp>

#include "image_codec.h"

void show_image_worker(std::atomic<bool>& running, ThreadSafeQueue<cv::Mat>& show_image_q) {
    cv::namedWindow("Video Player");
    while (true) {
        auto data = show_image_q.Pop();
        if (!data) break;
        const auto& frame = data.value();
        cv::imshow("Video Player", frame);
        char c = static_cast<char>(cv::waitKey(20));
        if (c == 27) {
            running = false;
            while (show_image_q.Pop());
            break;
        }
    }
}

void start(const std::string& output_file, const Dim& dim, PixelType pixel_type, int pixel_size, int space_size, uint32_t part_num, int mp, const Transform& transform, const Calibration& calibration) {
    PixelImageCodecWorker pixel_image_codec_worker(pixel_type);
    ThreadSafeQueue<std::pair<uint64_t, cv::Mat>> frame_q(20);
    ThreadSafeQueue<cv::Mat> show_image_q(5);
    ThreadSafeQueue<DecodeResult> part_q(20);
    std::atomic<bool> running = true;
    std::thread fetch_image_thread(&PixelImageCodecWorker::FetchImageWorker, &pixel_image_codec_worker, std::ref(running), std::ref(frame_q), 20);
    auto get_transform_fn = [transform] {
        return transform;
    };
    std::vector<std::thread> decode_image_threads;
    for (int i = 0; i < mp; ++i) {
        decode_image_threads.emplace_back(&PixelImageCodecWorker::DecodeImageWorker, &pixel_image_codec_worker, std::ref(part_q), std::ref(frame_q), dim, get_transform_fn, calibration);
    }
    auto send_decode_image_result_cb = [&show_image_q](const cv::Mat& img, bool success, const std::vector<std::vector<cv::Mat>>& result_imgs) {
        show_image_q.Push(img);
    };
    std::thread decode_image_result_thread(&PixelImageCodecWorker::DecodeResultWorker, &pixel_image_codec_worker, std::ref(part_q), std::ref(frame_q), dim, get_transform_fn, calibration, send_decode_image_result_cb, 500);
    std::thread show_image_thread(show_image_worker, std::ref(running), std::ref(show_image_q));
    auto save_part_progress_cb = [](const PixelImageCodecWorker::SavePartProgress& task_save_part_progress){
        std::cerr << "\r" << task_save_part_progress.frame_num << " frames processed, " << task_save_part_progress.done_part_num << "/" << task_save_part_progress.part_num << " parts transferred, fps=" << std::fixed << std::setprecision(2) << task_save_part_progress.fps << ", done_fps=" << std::fixed << std::setprecision(2) << task_save_part_progress.done_fps << ", bps=" << std::fixed << std::setprecision(0) << task_save_part_progress.bps << ", left_time=" << std::setfill('0') << std::setw(2) << task_save_part_progress.left_days << "d" << std::setw(2) << task_save_part_progress.left_hours << "h" << std::setw(2) << task_save_part_progress.left_minutes << "m" << std::setw(2) << task_save_part_progress.left_seconds << "s" << std::setfill(' ');
        std::cerr.flush();
    };
    auto save_part_finish_cb = []() {
        std::cerr << "\n";
    };
    auto save_part_error_cb = [](const std::string& msg) {
        std::cerr << msg << "\n";
    };
    auto finalization_start_cb = [](const Task::FinalizationProgress& finalization_progress) {
        std::cerr << "\n";
    };
    auto finalization_progress_cb = [](const Task::FinalizationProgress& finalization_progress){
        std::cerr << "\r" << finalization_progress.done_block_num << "/" << finalization_progress.block_num << " block processed";
        std::cerr.flush();
    };
    auto finalization_complete_cb = [] {
        std::cerr << "\ntransfer done";
    };
    std::thread save_part_thread(&PixelImageCodecWorker::SavePartWorker, &pixel_image_codec_worker, std::ref(running), std::ref(part_q), output_file, dim, pixel_size, space_size, part_num, save_part_progress_cb, save_part_finish_cb, nullptr, save_part_error_cb, finalization_start_cb, finalization_progress_cb, finalization_complete_cb, nullptr);
    fetch_image_thread.join();
    for (size_t i = 0; i < decode_image_threads.size() + 1; ++i) {
        frame_q.PushNull();
    }
    for (auto& e : decode_image_threads) {
        e.join();
    }
    decode_image_result_thread.join();
    show_image_q.PushNull();
    show_image_thread.join();
    part_q.PushNull();
    save_part_thread.join();
}

int main(int argc, char** argv) {
    try {
        std::string output_file;
        std::string dim_str;
        std::string pixel_type_str;
        int pixel_size = 0;
        int space_size = 0;
        uint32_t part_num = 0;
        int mp = 1;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("output_file", boost::program_options::value<std::string>(&output_file), "output file");
        desc_handler("dim", boost::program_options::value<std::string>(&dim_str), "dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size");
        desc_handler("pixel_type", boost::program_options::value<std::string>(&pixel_type_str), "pixel type");
        desc_handler("pixel_size", boost::program_options::value<int>(&pixel_size), "pixel size");
        desc_handler("space_size", boost::program_options::value<int>(&space_size), "space size");
        desc_handler("part_num", boost::program_options::value<uint32_t>(&part_num), "part num");
        desc_handler("mp", boost::program_options::value<int>(&mp), "multiprocessing");
        add_transform_options(desc_handler);
        boost::program_options::positional_options_description p_desc;
        p_desc.add("output_file", 1);
        p_desc.add("dim", 1);
        p_desc.add("pixel_type", 1);
        p_desc.add("pixel_size", 1);
        p_desc.add("space_size", 1);
        p_desc.add("part_num", 1);
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p_desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        if (!vm.count("output_file")) {
            throw std::invalid_argument("output_file not specified");
        }

        if (!vm.count("dim")) {
            throw std::invalid_argument("dim not specified");
        }

        if (!vm.count("pixel_type")) {
            throw std::invalid_argument("pixel_type not specified");
        }

        if (!vm.count("pixel_size")) {
            throw std::invalid_argument("pixel_size not specified");
        }

        if (!vm.count("space_size")) {
            throw std::invalid_argument("space_size not specified");
        }

        if (!vm.count("part_num")) {
            throw std::invalid_argument("part_num not specified");
        }

        auto dim = parse_dim(vm["dim"].as<std::string>());
        auto pixel_type = parse_pixel_type(pixel_type_str);
        Transform transform = get_transform(vm);
        Calibration calibration;

        start(output_file, dim, pixel_type, pixel_size, space_size, part_num, mp, transform, calibration);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
