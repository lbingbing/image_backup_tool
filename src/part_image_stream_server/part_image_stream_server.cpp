#include <iostream>
#include <exception>
#include <filesystem>

#include <boost/program_options.hpp>

#include "image_codec.h"

GenPartImageFn2 gen_images(GenPartImageFn1 gen_image_fn, uint32_t part_num) {
    return [gen_image_fn, part_num](){
        auto data = gen_image_fn();
        if (data) {
            auto& [part_id, img] = data.value();
            auto img_file_name = get_part_image_file_name(part_num, part_id);
            return std::make_optional<std::pair<std::string, cv::Mat>>(std::move(img_file_name), std::move(img));
        } else {
            return std::optional<std::pair<std::string, cv::Mat>>();
        }
    };
}

int main(int argc, char** argv) {
    try {
        std::string target_file;
        std::string symbol_type_str;
        std::string dim_str;
        int pixel_size = 0;
        int space_size = 0;
        int port = 0;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("target_file", boost::program_options::value<std::string>(&target_file), "target file");
        desc_handler("symbol_type", boost::program_options::value<std::string>(&symbol_type_str), "symbol type");
        desc_handler("dim", boost::program_options::value<std::string>(&dim_str), "dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size");
        desc_handler("pixel_size", boost::program_options::value<int>(&pixel_size), "pixel size");
        desc_handler("space_size", boost::program_options::value<int>(&space_size), "space size");
        desc_handler("port", boost::program_options::value<int>(&port), "port");
        boost::program_options::positional_options_description p_desc;
        p_desc.add("target_file", 1);
        p_desc.add("symbol_type", 1);
        p_desc.add("dim", 1);
        p_desc.add("pixel_size", 1);
        p_desc.add("space_size", 1);
        p_desc.add("port", 1);
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p_desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        check_positional_options(p_desc, vm);
        check_is_file(target_file);

        auto symbol_type = parse_symbol_type(symbol_type_str);
        auto dim = parse_dim(dim_str);
        auto [symbol_codec, part_byte_num, raw_bytes, part_num] = prepare_part_images(target_file, symbol_type, dim);
        auto gen_image_fn1 = generate_part_images(dim, pixel_size, space_size, symbol_codec.get(), part_byte_num, raw_bytes, part_num);
        auto gen_image_fn2 = gen_images(gen_image_fn1, part_num);
        start_image_stream_server(gen_image_fn2, port);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
