#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <algorithm>

#include <boost/program_options.hpp>
#include <opencv2/opencv.hpp>

#include "image_codec.h"

GenPartImageFn2 gen_images(const std::string& image_dir_path) {
    std::vector<std::filesystem::path> img_file_paths;
    for (const auto& entry : std::filesystem::directory_iterator(image_dir_path)) {
        if (entry.path().extension() == ".png" && entry.is_regular_file()) {
            img_file_paths.push_back(entry.path());
        }
    }
    std::sort(img_file_paths.begin(), img_file_paths.end());
    size_t cur_index = 0;
    return [img_file_paths, cur_index]() mutable {
        if (cur_index < img_file_paths.size()) {
            const auto& path = img_file_paths[cur_index];
            auto img = cv::imread(path.string());
            ++cur_index;
            return std::make_optional<std::pair<std::string, cv::Mat>>(path.filename().string(), std::move(img));
        } else {
            return std::optional<std::pair<std::string, cv::Mat>>();
        }
    };
}

int main(int argc, char** argv) {
    try {
        std::string image_dir_path;
        int port = 0;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("image_dir_path", boost::program_options::value<std::string>(&image_dir_path), "image dir path");
        desc_handler("port", boost::program_options::value<int>(&port), "port");
        boost::program_options::positional_options_description p_desc;
        p_desc.add("image_dir_path", 1);
        p_desc.add("port", 1);
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p_desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        if (!vm.count("image_dir_path")) {
            throw std::invalid_argument("image_dir_path not specified");
        }

        if (!std::filesystem::is_directory(image_dir_path)) {
            throw std::invalid_argument("image_dir_path '" + image_dir_path + "' is not dir");
        }

        if (!vm.count("port")) {
            throw std::invalid_argument("port not specified");
        }

        auto gen_image_fn = gen_images(image_dir_path);
        start_image_stream_server(gen_image_fn, port);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
