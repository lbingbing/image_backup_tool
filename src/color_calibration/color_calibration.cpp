#include <iostream>
#include <map>
#include <exception>
#include <thread>
#include <boost/program_options.hpp>

#include "image_codec.h"

void decode(PixelImageCodec* pixel_image_codec, const cv::Mat& img, const Dim& dim, const Transform transform, const Calibration& calibration, bool save_result_image, const std::string& image_file, int print_detailed_mismatch_info_level) {
    auto [dummy_success, part_id, part_bytes, part_pixels, img1, result_imgs] = pixel_image_codec->Decode(img, dim, transform, calibration, true);
    if (save_result_image) {
        auto pos = image_file.rfind(".");
        auto image_file_prefix = image_file.substr(0, pos);
        auto image_file_suffix = image_file.substr(pos);
        cv::imwrite(image_file_prefix + "_transformed" + image_file_suffix, img1);
        auto [tile_x_num, tile_y_num, tile_x_size, tile_y_size] = dim;
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
    if (part_pixels.empty()) {
        std::cout << "pixels decode fail\n";
    } else {
        int mismatch_count = 0;
        std::map<Pixel, std::map<Pixel, int>> mismatch_counts;
        for (int tile_y_id = 0; tile_y_id < dim.tile_y_num; ++tile_y_id) {
            for (int tile_x_id = 0; tile_x_id < dim.tile_x_num; ++tile_x_id) {
                for (int y = 0; y < dim.tile_y_size; ++y) {
                    for (int x = 0; x < dim.tile_x_size; ++x) {
                        Pixel golden_pixel = (x + y + tile_x_id + tile_y_id) % pixel_image_codec->GetPixelCodec().PixelValueNum();
                        int index = ((tile_y_id * dim.tile_x_num + tile_x_id) * dim.tile_y_size + y) * dim.tile_x_size + x;
                        auto actual_pixel = part_pixels[index];
                        if (actual_pixel != golden_pixel) {
                            if (print_detailed_mismatch_info_level >= 2) {
                                std::cout << "pixel_" << tile_x_id << "_" << tile_y_id << "_" << x << "_" << y << ": " << get_pixel_name(golden_pixel) << " -> " << get_pixel_name(actual_pixel) << "\n";
                            }
                            ++mismatch_count;
                            ++mismatch_counts[golden_pixel][actual_pixel];
                        }
                    }
                }
            }
        }
        if (print_detailed_mismatch_info_level >= 1) {
            for (const auto& [golden_pixel, e] : mismatch_counts) {
                for (const auto& [actual_pixel, count] : e) {
                    std::cout << "mismatch " << get_pixel_name(golden_pixel) << " -> " << get_pixel_name(actual_pixel) << ": " << count << "\n";
                }
            }
        }
        std::cout << "mismatch count " << mismatch_count << "\n";
        if (mismatch_counts.empty()) {
            std::cout << "color calibration pass\n";
        } else {
            std::cout << "color calibration fail\n";
        }
    }
}


bool is_mismatch(const Pixels& part_pixels, PixelImageCodec* pixel_image_codec, const Dim& dim) {
    for (int tile_y_id = 0; tile_y_id < dim.tile_y_num; ++tile_y_id) {
        for (int tile_x_id = 0; tile_x_id < dim.tile_x_num; ++tile_x_id) {
            for (int y = 0; y < dim.tile_y_size; ++y) {
                for (int x = 0; x < dim.tile_x_size; ++x) {
                    Pixel golden_pixel = (x + y + tile_x_id + tile_y_id) % pixel_image_codec->GetPixelCodec().PixelValueNum();
                    int index = ((tile_y_id * dim.tile_x_num + tile_x_id) * dim.tile_y_size + y) * dim.tile_x_size + x;
                    auto actual_pixel = part_pixels[index];
                    if (actual_pixel != golden_pixel) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

using InputQueue = ThreadSafeQueue<std::tuple<int, int, int>>;
using ResultQueue = ThreadSafeQueue<std::tuple<bool, int, int, int>>;

void scan_worker(ResultQueue& q_result, InputQueue& q_in, PixelImageCodec* pixel_image_codec, const cv::Mat& img, const Dim& dim, const Transform transform, const Calibration& calibration) {
    Transform transform1 = transform;
    while (true) {
        auto data = q_in.Pop();
        if (!data) break;
        auto [b, g, r] = data.value();
        transform1.pixelization_threshold[0] = b;
        transform1.pixelization_threshold[1] = g;
        transform1.pixelization_threshold[2] = r;
        auto [dummy_success, part_id, part_bytes, part_pixels, img1, result_imgs] = pixel_image_codec->Decode(img, dim, transform1, calibration, true);
        if (!part_pixels.empty() && !is_mismatch(part_pixels, pixel_image_codec, dim)) {
            q_result.Emplace(true, b, g, r);
        } else {
            q_result.Emplace(false, 0, 0, 0);
        }
    }
}

void result_worker(ResultQueue& q_result, int total) {
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

void scan(PixelImageCodec* pixel_image_codec, const cv::Mat& img, const Dim& dim, const Transform transform, const Calibration& calibration, int scan_radius) {
    int b0 = std::max(transform.pixelization_threshold[0] - scan_radius, 0);
    int b1 = std::min(transform.pixelization_threshold[0] + scan_radius, 255);
    int g0 = std::max(transform.pixelization_threshold[1] - scan_radius, 0);
    int g1 = std::min(transform.pixelization_threshold[1] + scan_radius, 255);
    int r0 = std::max(transform.pixelization_threshold[2] - scan_radius, 0);
    int r1 = std::min(transform.pixelization_threshold[2] + scan_radius, 255);
    int total = (b1 - b0) * (g1 - g0) * (r1 - r0);

    constexpr int worker_num = 4;
    InputQueue q_in(1024);
    ResultQueue q_result(1024);
    std::vector<std::thread> scan_worker_threads;
    for (int i = 0; i < worker_num; ++i) {
        scan_worker_threads.emplace_back(scan_worker, std::ref(q_result), std::ref(q_in), pixel_image_codec, img, dim, transform, calibration);
    }
    std::thread result_worker_thread(result_worker, std::ref(q_result), total);
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

int main(int argc, char** argv)
{
    try {
        std::string image_file;
        std::string calibration_file;
        bool save_result_image = false;
        int scan_radius = 0;
        int print_detailed_mismatch_info_level = 0;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("image_file", boost::program_options::value<std::string>(&image_file), "image file");
        desc_handler("dim", boost::program_options::value<std::string>(), "dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size");
        desc_handler("pixel_type", boost::program_options::value<std::string>(), "pixel type");
        desc_handler("calibration_file", boost::program_options::value<std::string>(&calibration_file), "calibration file");
        desc_handler("save_result_image", boost::program_options::value<bool>(&save_result_image), "dump result image");
        desc_handler("scan_radius", boost::program_options::value<int>(&scan_radius), "scan radius");
        desc_handler("print_detailed_mismatch_info_level", boost::program_options::value<int>(&print_detailed_mismatch_info_level), "print detailed mismatch info");
        add_transform_options(desc_handler);
        boost::program_options::positional_options_description p_desc;
        p_desc.add("image_file", 1);
        p_desc.add("dim", 1);
        p_desc.add("pixel_type", 1);
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p_desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        if (!vm.count("image_file")) {
            throw std::invalid_argument("image_file not specified");
        }

        if (!vm.count("dim")) {
            throw std::invalid_argument("dim not specified");
        }

        if (!vm.count("pixel_type")) {
            throw std::invalid_argument("pixel_type not specified");
        }

        auto dim = parse_dim(vm["dim"].as<std::string>());
        auto pixel_image_codec = create_pixel_image_codec(parse_pixel_type(vm["pixel_type"].as<std::string>()));
        Transform transform = get_transform(vm);
        Calibration calibration;
        if (vm.count("calibration_file")) {
            calibration.Load(calibration_file);
        }

        cv::Mat img = cv::imread(image_file, cv::IMREAD_COLOR);
        if (scan_radius) {
            scan(pixel_image_codec.get(), img, dim, transform, calibration, scan_radius);
        } else {
            decode(pixel_image_codec.get(), img, dim, transform, calibration, save_result_image, image_file, print_detailed_mismatch_info_level);
        }
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
