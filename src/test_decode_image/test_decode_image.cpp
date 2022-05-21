#include <iostream>
#include <map>
#include <exception>
#include <boost/program_options.hpp>

#include "image_codec.h"

int main(int argc, char** argv)
{
    try {
        std::string image_file;
        std::string calibration_file;
        bool save_result_image = false;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("image_file", boost::program_options::value<std::string>(&image_file), "image file");
        desc_handler("dim", boost::program_options::value<std::string>(), "dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size");
        desc_handler("pixel_type", boost::program_options::value<std::string>(), "pixel type");
        desc_handler("calibration_file", boost::program_options::value<std::string>(&calibration_file), "calibration file");
        desc_handler("save_result_image", boost::program_options::value<bool>(&save_result_image), "dump result image");
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
        auto [success, part_id, part_bytes, part_pixels, img1, result_imgs] = pixel_image_codec->Decode(img, dim, transform, calibration, true);
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
        if (success) {
            std::cout << "pass part_id=" << part_id << "\n";
        } else {
            std::cout << "fail\n";
        }
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
