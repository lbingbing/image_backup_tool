#include <iostream>
#include <exception>
#include <filesystem>
#include <boost/program_options.hpp>

#include "image_codec.h"

int main(int argc, char** argv) {
    try {
        std::string image_file;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("image_file", boost::program_options::value<std::string>(&image_file), "image file");
        desc_handler("dim", boost::program_options::value<std::string>(), "dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size");
        add_transform_options(desc_handler);
        boost::program_options::positional_options_description p_desc;
        p_desc.add("image_file", 1);
        p_desc.add("dim", 1);
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

        if (!std::filesystem::is_regular_file(image_file)) {
            throw std::invalid_argument("image file '" + image_file + "' not found");
        }

        if (!vm.count("dim")) {
            throw std::invalid_argument("dim not specified");
        }

        auto dim = parse_dim(vm["dim"].as<std::string>());
        Transform transform = get_transform(vm);

        cv::Mat img = cv::imread(image_file, cv::IMREAD_COLOR);
        auto [img1, calibration, result_imgs] = create_pixel_image_codec(PixelType::PIXEL2)->Calibrate(img, dim, transform);
        auto pos = image_file.rfind(".");
        auto image_file_prefix = image_file.substr(0, pos);
        auto image_file_suffix = image_file.substr(pos);
        cv::imwrite(image_file_prefix + "_transformed" + image_file_suffix, img1);
        auto [tile_x_num, tile_y_num, tile_x_size, tile_y_size] = dim;
        if (calibration.valid) {
            std::cout << "pass\n";
            std::string file_name = image_file_prefix + "_result_0_0" + image_file_suffix;
            cv::imwrite(file_name, result_imgs[0][0]);
        } else {
            std::cout << "fail\n";
        }
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
