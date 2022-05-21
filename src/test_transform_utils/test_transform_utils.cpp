#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <boost/program_options.hpp>

#include "image_codec.h"

int main(int argc, char** argv) {
    try {
        std::string image_file;
        bool binarization_on = false;
        bool pixelization_on = false;
        std::string pixel_type_str = "pixel2";
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("image_file", boost::program_options::value<std::string>(&image_file), "image file");
        desc_handler("binarization_on", boost::program_options::value<bool>(&binarization_on), "binarization on");
        desc_handler("pixelization_on", boost::program_options::value<bool>(&pixelization_on), "pixelization on");
        desc_handler("pixel_type", boost::program_options::value<std::string>(&pixel_type_str), "pixel type");
        add_transform_options(desc_handler);
        boost::program_options::positional_options_description p_desc;
        p_desc.add("image_file", 1);
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

        Transform transform = get_transform(vm);
        std::cout << transform << "\n";
        cv::Mat img = cv::imread(image_file, cv::IMREAD_COLOR);
        cv::Mat img1 = transform_image(img, transform);
        if (binarization_on) {
            img1 = do_binarize(img1, transform.binarization_threshold);
        }
        if (pixelization_on) {
            auto pixel_type = parse_pixel_type(pixel_type_str);
            img1 = do_pixelize(img1, pixel_type, transform.pixelization_threshold);
        }
        //cv::imshow("image", img1);
        //cv::waitKey();
        cv::imwrite("z_transformed.bmp", img1);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
