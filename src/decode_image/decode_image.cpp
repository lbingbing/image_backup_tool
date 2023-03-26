#include <iostream>
#include <exception>
#include <thread>
#include <filesystem>

#include <boost/program_options.hpp>

#include "image_codec.h"

void decode(ImageDecoder* image_decoder, const cv::Mat& img, const Transform transform, const Calibration& calibration, bool save_result_image, const std::string& image_file) {
    auto [success, part_id, part_bytes, part_symbols, img1, result_imgs] = image_decoder->Decode(img, transform, calibration, true);
    if (save_result_image) {
        auto pos = image_file.rfind(".");
        auto image_file_prefix = image_file.substr(0, pos);
        auto image_file_suffix = image_file.substr(pos);
        cv::imwrite(image_file_prefix + "_transformed" + image_file_suffix, img1);
        auto [tile_x_num, tile_y_num, tile_x_size, tile_y_size] = image_decoder->GetDim();
        if (!result_imgs.empty()) {
            for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
                for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
                    if (!result_imgs[tile_y_id][tile_x_id].empty()) {
                        std::string file_name = image_file_prefix + "_result_" + std::to_string(tile_x_id) + "_" + std::to_string(tile_y_id) + image_file_suffix;
                        cv::imwrite(file_name, result_imgs[tile_y_id][tile_x_id]);
                    }
                }
            }
        }
    }
    if (success) {
        std::cout << "pass part_id=" << part_id << "\n";
    } else {
        std::cout << "fail\n";
    }
}

using InputQueue = ThreadSafeQueue<std::tuple<int, int, int>>;
using ResultQueue = ThreadSafeQueue<std::tuple<bool, int, int, int>>;

void scan1_worker(ResultQueue& q_result, InputQueue& q_in, ImageDecoder* image_decoder, const cv::Mat& img, const Transform transform, const Calibration& calibration) {
    Transform transform1 = transform;
    while (true) {
        auto data = q_in.Pop();
        if (!data) break;
        auto [b, g, r] = data.value();
        transform1.pixelization_threshold[0] = b;
        transform1.pixelization_threshold[1] = g;
        transform1.pixelization_threshold[2] = r;
        auto [success, part_id, part_bytes, part_symbols, img1, result_imgs] = image_decoder->Decode(img, transform1, calibration, true);
        if (success) {
            q_result.Emplace(true, b, g, r);
        } else {
            q_result.Emplace(false, 0, 0, 0);
        }
    }
}

void scan1_result_worker(ResultQueue& q_result, int total) {
    int done = 0;
    while (true) {
        auto data = q_result.Pop();
        if (!data) break;
        auto [success, b, g, r] = data.value();
        if (success) {
            std::cout << "\nbgr: " << b << "," << g << "," << r << "\n";
        }
        ++done;
        std::cerr << "\r" << done << "/" << total;
    }
    std::cerr << "\n";
}

void scan1(ImageDecoder* image_decoder, const cv::Mat& img, const Transform transform, const Calibration& calibration, int scan_bgr_radius) {
    int b0 = std::max(transform.pixelization_threshold[0] - scan_bgr_radius, 0);
    int b1 = std::min(transform.pixelization_threshold[0] + scan_bgr_radius, 255);
    int g0 = std::max(transform.pixelization_threshold[1] - scan_bgr_radius, 0);
    int g1 = std::min(transform.pixelization_threshold[1] + scan_bgr_radius, 255);
    int r0 = std::max(transform.pixelization_threshold[2] - scan_bgr_radius, 0);
    int r1 = std::min(transform.pixelization_threshold[2] + scan_bgr_radius, 255);
    int total = (b1 - b0) * (g1 - g0) * (r1 - r0);

    constexpr int worker_num = 4;
    InputQueue q_in(1024);
    ResultQueue q_result(1024);
    std::vector<std::thread> scan_worker_threads;
    for (int i = 0; i < worker_num; ++i) {
        scan_worker_threads.emplace_back(scan1_worker, std::ref(q_result), std::ref(q_in), image_decoder, img, transform, calibration);
    }
    std::thread result_worker_thread(scan1_result_worker, std::ref(q_result), total);
    for (int b = b0; b < b1; ++b) {
        for (int g = g0; g < g1; ++g) {
            for (int r = r0; r < r1; ++r) {
                q_in.Emplace(b, g, r);
            }
        }
    }
    for (int i = 0; i < worker_num; ++i) {
        q_in.PushNull();
    }
    for (auto& e : scan_worker_threads) {
        e.join();
    }
    q_result.PushNull();
    result_worker_thread.join();
}

void scan2(ImageDecoder* image_decoder, const cv::Mat& img, const Transform transform, const Calibration& calibration) {
    for (int c = 0; c < 256; ++c) {
        Transform transform1 = transform;
        transform1.pixelization_threshold[0] = c;
        transform1.pixelization_threshold[1] = c;
        transform1.pixelization_threshold[2] = c;
        auto [success, part_id, part_bytes, part_symbols, img1, result_imgs] = image_decoder->Decode(img, transform1, calibration, true);
        std::cout << "bgr: " << c << "," << c << "," << c << " ";
        if (success) {
            std::cout << "pass";
        } else {
            std::cout << "fail";
        }
        std::cout << "\n";
    }
}

int main(int argc, char** argv) {
    try {
        std::string image_file;
        std::string symbol_type_str;
        std::string dim_str;
        std::string calibration_file;
        bool save_result_image = false;
        int scan_mode = 0;
        int scan_bgr_radius = 0;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("image_file", boost::program_options::value<std::string>(&image_file), "image file");
        desc_handler("symbol_type", boost::program_options::value<std::string>(&symbol_type_str), "symbol type");
        desc_handler("dim", boost::program_options::value<std::string>(&dim_str), "dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size");
        desc_handler("calibration_file", boost::program_options::value<std::string>(&calibration_file), "calibration file");
        desc_handler("save_result_image", boost::program_options::value<bool>(&save_result_image), "dump result image");
        desc_handler("scan_mode", boost::program_options::value<int>(&scan_mode), "scan mode, 1: bgr; 2: gray");
        desc_handler("scan_bgr_radius", boost::program_options::value<int>(&scan_bgr_radius), "scan radius");
        add_transform_options(desc_handler);
        boost::program_options::positional_options_description p_desc;
        p_desc.add("image_file", 1);
        p_desc.add("symbol_type", 1);
        p_desc.add("dim", 1);
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p_desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        check_positional_options(p_desc, vm);
        check_is_file(image_file);

        if (vm.count("calibration_file")) {
            check_is_file(calibration_file);
        }

        auto dim = parse_dim(dim_str);
        ImageDecoder image_decoder(parse_symbol_type(symbol_type_str), dim);
        Transform transform = get_transform(vm);
        Calibration calibration;
        if (vm.count("calibration_file")) {
            calibration.Load(calibration_file);
        }

        cv::Mat img = cv::imread(image_file, cv::IMREAD_COLOR);
        if (scan_mode == 0) {
            decode(&image_decoder, img, transform, calibration, save_result_image, image_file);
        } else if (scan_mode == 1) {
            scan1(&image_decoder, img, transform, calibration, scan_bgr_radius);
        } else if (scan_mode == 2) {
            scan2(&image_decoder, img, transform, calibration);
        }
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
