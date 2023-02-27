#include <boost/asio.hpp>

#include "part_image_utils.h"
#include "base64.h"
#include "image_decode_task.h"

namespace {

bool is_tile_border(int tile_x_size, int tile_y_size, int x, int y) {
    return y == 0 || y == tile_y_size + 1 || x == 0 || x == tile_x_size + 1;
}

cv::Mat generate_part_image(const Dim& dim, int pixel_size, int space_size, SymbolCodec* symbol_codec, uint32_t part_id, const Bytes& part_bytes) {
    auto [tile_x_num, tile_y_num, tile_x_size, tile_y_size] = dim;
    auto symbols = symbol_codec->Encode(part_id, part_bytes, tile_x_num * tile_y_num * tile_x_size * tile_y_size);
    int img_h = (tile_y_num * (tile_y_size + 2) + (tile_y_num - 1) * space_size + 2) * pixel_size;
    int img_w = (tile_x_num * (tile_x_size + 2) + (tile_x_num - 1) * space_size + 2) * pixel_size;
    cv::Mat img(img_h, img_w, CV_8UC3);
    cv::rectangle(img, cv::Rect(0, 0, img_w, pixel_size), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::rectangle(img, cv::Rect(0, img_h - pixel_size, img_w, pixel_size), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::rectangle(img, cv::Rect(0, 0, pixel_size, img_h), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::rectangle(img, cv::Rect(img_w - pixel_size, 0, pixel_size, img_h), cv::Scalar(255, 255, 255), cv::FILLED);
    for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
        int tile_y = (tile_y_id * (tile_y_size + 2 + space_size) + 1) * pixel_size;
        for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
            int tile_x = (tile_x_id * (tile_x_size + 2 + space_size) + 1) * pixel_size;
            for (int y = 0; y < tile_y_size + 2; ++y) {
                for (int x = 0; x < tile_x_size + 2; ++x) {
                    cv::Scalar color;
                    if (is_tile_border(tile_x_size, tile_y_size, x, y)) {
                        color = cv::Scalar(0, 0, 0);
                    } else {
                        auto symbol = symbols[((tile_y_id * tile_x_num + tile_x_id) * tile_y_size + (y - 1)) * tile_x_size + (x - 1)];
                        if (symbol == static_cast<int>(PixelColor::WHITE)) {
                            color = cv::Scalar(255, 255, 255);
                        } else if (symbol == static_cast<int>(PixelColor::BLACK)) {
                            color = cv::Scalar(0, 0, 0);
                        } else if (symbol == static_cast<int>(PixelColor::RED)) {
                            color = cv::Scalar(0, 0, 255);
                        } else if (symbol == static_cast<int>(PixelColor::BLUE)) {
                            color = cv::Scalar(255, 0, 0);
                        } else if (symbol == static_cast<int>(PixelColor::GREEN)) {
                            color = cv::Scalar(0, 255, 0);
                        } else if (symbol == static_cast<int>(PixelColor::CYAN)) {
                            color = cv::Scalar(255, 255, 0);
                        } else if (symbol == static_cast<int>(PixelColor::MAGENTA)) {
                            color = cv::Scalar(255, 0, 255);
                        } else if (symbol == static_cast<int>(PixelColor::YELLOW)) {
                            color = cv::Scalar(0, 255, 255);
                        }
                    }
                    cv::rectangle(img, cv::Rect(tile_x + x * pixel_size, tile_y + y * pixel_size, pixel_size, pixel_size), color, cv::FILLED);
                }
            }
        }
    }
    return img;
}

Bytes img_to_msg_bytes(const cv::Mat& img) {
    Bytes img_bytes;
    cv::imencode(".png", img, img_bytes);
    auto base64_img_bytes = b64encode(img_bytes);
    uint64_t len = base64_img_bytes.size();
    Bytes len_bytes(reinterpret_cast<Byte*>(&len), reinterpret_cast<Byte*>(&len)+sizeof(len));
    Bytes msg_bytes = len_bytes;
    msg_bytes.insert(msg_bytes.end(), base64_img_bytes.begin(), base64_img_bytes.end());
    return msg_bytes;
}

}

std::tuple<std::unique_ptr<SymbolCodec>, int, Bytes, uint32_t> prepare_part_images(const std::string& target_file, SymbolType symbol_type, const Dim& dim) {
    auto symbol_codec = create_symbol_codec(symbol_type);
    auto part_byte_num = get_part_byte_num(symbol_type, dim);
    if (part_byte_num < Task::MIN_PART_BYTE_NUM) throw invalid_image_codec_argument("invalid part_byte_num '" + std::to_string(part_byte_num) + "'");
    auto res = get_task_bytes(target_file, part_byte_num);
    auto& raw_bytes = std::get<0>(res);
    auto part_num = std::get<1>(res);
    std::cout << part_num << " parts\n";
    return std::make_tuple(std::move(symbol_codec), part_byte_num, std::move(raw_bytes), part_num);
}

GenPartImageFn1 generate_part_images(const Dim& dim, int pixel_size, int space_size, SymbolCodec* symbol_codec, int part_byte_num, const Bytes& raw_bytes, uint32_t part_num) {
    uint32_t cur_part_id = 0;
    return [dim, pixel_size, space_size, symbol_codec, part_byte_num, &raw_bytes, part_num, cur_part_id]() mutable {
        if (cur_part_id < part_num) {
            auto part_id = cur_part_id;
            Bytes part_bytes(raw_bytes.begin()+part_id*part_byte_num, raw_bytes.begin()+(part_id+1)*part_byte_num);
            auto img = generate_part_image(dim, pixel_size, space_size, symbol_codec, part_id, part_bytes);
            ++cur_part_id;
            return std::make_optional<std::pair<uint32_t, cv::Mat>>({part_id, std::move(img)});
        } else {
            return std::optional<std::pair<uint32_t, cv::Mat>>();
        }
    };
}

std::string get_part_image_file_name(uint32_t part_num, uint32_t part_id) {
    int part_id_width = 1;
    while (true) {
        part_num /= 10;
        if (!part_num) break;
        ++part_id_width;
    }
    std::ostringstream oss;
    oss << "part" << std::setw(part_id_width) << std::setfill('0') << part_id << ".png";
    return oss.str();
}

void start_image_stream_server(GenPartImageFn2 gen_image_fn, int port) {
    std::cout << "start server\n";
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
    boost::asio::ip::tcp::socket socket(io_context);
    acceptor.accept(socket);
    bool running = true;
    while (running) {
        auto cur_gen_image_fn = gen_image_fn;
        while (true) {
            auto data = cur_gen_image_fn();
            if (!data) break;
            auto& [img_file_name, img] = data.value();
            auto msg_bytes = img_to_msg_bytes(img);
            std::cout << img_file_name << "\n";
            try {
                uint64_t done = 0;
                while (done < msg_bytes.size()) {
                    done += boost::asio::write(socket, boost::asio::buffer(msg_bytes.data() + done, msg_bytes.size() - done));
                }
            }
            catch (std::exception& e) {
                std::cout << "client disconnected\n";
                running = false;
                break;
            }
        }
    }
}
