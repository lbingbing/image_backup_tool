#include <set>
#include <queue>
#include <iostream>
#include <fstream>
#include <cmath>

#include "image_decoder.h"

void Calibration::Load(const std::string& path) {
    std::ifstream f(path, std::ios_base::binary);
    int vld = 0;
    f.read(reinterpret_cast<char*>(&vld), sizeof(vld));
    valid = vld;
    if (valid) {
        f.read(reinterpret_cast<char*>(&dim), sizeof(Dim));
        tiles.resize(dim.tile_y_num);
        for (int tile_y_id = 0; tile_y_id < dim.tile_y_num; ++tile_y_id) {
            tiles[tile_y_id].resize(dim.tile_x_num);
            for (int tile_x_id = 0; tile_x_id < dim.tile_x_num; ++tile_x_id) {
                auto& centers = tiles[tile_y_id][tile_x_id].centers;
                centers.resize(dim.tile_y_size);
                for (int y = 0; y < dim.tile_y_size; ++y) {
                    centers[y].resize(dim.tile_x_size);
                    for (int x = 0; x < dim.tile_x_size; ++x) {
                        auto& center = centers[y][x];
                        f.read(reinterpret_cast<char*>(&center[0]), sizeof(center[0]));
                        f.read(reinterpret_cast<char*>(&center[1]), sizeof(center[1]));
                    }
                }
            }
        }
    }
}

void Calibration::Save(const std::string& path) {
    std::ofstream f(path, std::ios_base::binary);
    int vld = valid;
    f.write(reinterpret_cast<char*>(&vld), sizeof(vld));
    if (valid) {
        f.write(reinterpret_cast<char*>(&dim), sizeof(Dim));
        for (int tile_y_id = 0; tile_y_id < dim.tile_y_num; ++tile_y_id) {
            for (int tile_x_id = 0; tile_x_id < dim.tile_x_num; ++tile_x_id) {
                const auto& centers = tiles[tile_y_id][tile_x_id].centers;
                for (int y = 0; y < dim.tile_y_size; ++y) {
                    for (int x = 0; x < dim.tile_x_size; ++x) {
                        const auto& center = centers[y][x];
                        f.write(reinterpret_cast<const char*>(&center[0]), sizeof(center[0]));
                        f.write(reinterpret_cast<const char*>(&center[1]), sizeof(center[1]));
                    }
                }
            }
        }
    }
}

int find_non_white_boundary_n(const cv::Mat& img) {
    for (int i = 0; i < img.rows; ++i) {
        for (int j = 0; j < img.cols; ++j) {
            if (is_non_white(img, j, i)) {
                return i;
            }
        }
    }
    return img.rows;
}

int find_non_white_boundary_s(const cv::Mat& img) {
    for (int i = img.rows - 1; i >= 0; --i) {
        for (int j = 0; j < img.cols; ++j) {
            if (is_non_white(img, j, i)) {
                return i;
            }
        }
    }
    return -1;
}

int find_non_white_boundary_w(const cv::Mat& img) {
    for (int j = 0; j < img.cols; ++j) {
        for (int i = 0; i < img.rows; ++i) {
            if (is_non_white(img, j, i)) {
                return j;
            }
        }
    }
    return img.cols;
}

int find_non_white_boundary_e(const cv::Mat& img) {
    for (int j = img.cols - 1; j >= 0; --j) {
        for (int i = 0; i < img.rows; ++i) {
            if (is_non_white(img, j, i)) {
                return j;
            }
        }
    }
    return -1;
}

std::array<int, 4> find_tile_bbox1(const cv::Mat& img) {
    int y0 = find_non_white_boundary_n(img);
    int y1 = find_non_white_boundary_s(img);
    int x0 = find_non_white_boundary_w(img);
    int x1 = find_non_white_boundary_e(img);
    return {x0, y0, x1, y1};
}

std::array<int, 4> get_tile_bbox2(const std::array<int, 4>& bbox1, int tile_x_size, int tile_y_size) {
    int x0 = bbox1[0];
    int y0 = bbox1[1];
    int x1 = bbox1[2];
    int y1 = bbox1[3];
    float unit_h = static_cast<float>((y1 - y0)) / (tile_y_size + 2);
    float unit_w = static_cast<float>((x1 - x0)) / (tile_x_size + 2);
    y0 += static_cast<int>(std::round(unit_h));
    y1 -= static_cast<int>(std::round(unit_h));
    x0 += static_cast<int>(std::round(unit_w));
    x1 -= static_cast<int>(std::round(unit_w));
    return {x0, y0, x1, y1};
}

std::array<std::vector<int>, 2> get_spaces(const cv::Mat& img) {
    std::vector<int> space_xs;
    int x_space_start_x = -1;
    for (int j = 0; j < img.cols; ++j) {
        bool is_space = true;
        for (int i = 0; i < img.rows; ++i) {
            if (is_non_white(img, j, i)) {
                is_space = false;
                break;
            }
        }
        if (x_space_start_x >= 0) {
            if (!is_space) {
                space_xs.push_back((x_space_start_x + j) / 2);
                x_space_start_x = -1;
            }
        } else {
            if (is_space) {
                x_space_start_x = j;
            }
        }
    }
    if (x_space_start_x >= 0) space_xs.push_back((x_space_start_x + img.cols) / 2);
    std::vector<int> space_ys;
    int y_space_start_y = -1;
    for (int i = 0; i < img.rows; ++i) {
        bool is_space = true;
        for (int j = 0; j < img.cols; ++j) {
            if (is_non_white(img, j, i)) {
                is_space = false;
                break;
            }
        }
        if (y_space_start_y >= 0) {
            if (!is_space) {
                space_ys.push_back((y_space_start_y + i) / 2);
                y_space_start_y = -1;
            }
        } else {
            if (is_space) {
                y_space_start_y = i;
            }
        }
    }
    if (y_space_start_y >= 0) space_ys.push_back((y_space_start_y + img.rows) / 2);
    return {std::move(space_xs), std::move(space_ys)};
}

std::pair<bool, std::vector<std::vector<std::array<int, 4>>>> get_tile_bboxes(const cv::Mat& img, int tile_x_num, int tile_y_num) {
    auto [space_xs, space_ys] = get_spaces(img);
    if (static_cast<int>(space_ys.size()) != tile_y_num - 1 ||
        static_cast<int>(space_xs.size()) != tile_x_num - 1) {
        return {false, {}};
    }
    std::vector<std::vector<std::array<int, 4>>> tile_bboxs(tile_y_num);
    for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
        tile_bboxs[tile_y_id].resize(tile_x_num);
        for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
            int x0 = tile_x_id > 0 ? space_xs[tile_x_id - 1] : 0;
            int y0 = tile_y_id > 0 ? space_ys[tile_y_id - 1] : 0;
            int x1 = tile_x_id < tile_x_num - 1 ? space_xs[tile_x_id] : img.cols;
            int y1 = tile_y_id < tile_y_num - 1 ? space_ys[tile_y_id] : img.rows;
            tile_bboxs[tile_y_id][tile_x_id] = {x0, y0, x1, y1};
        }
    }
    return std::make_pair(true, std::move(tile_bboxs));
}

std::tuple<int, int, int, int> get_non_white_region_bbox(const cv::Mat& img, int x, int y) {
    int x0 = x;
    int y0 = y;
    int x1 = x;
    int y1 = y;
    std::set<std::array<int, 2>> visited;
    std::queue<std::array<int, 2>> q;
    q.push({x, y});
    while (!q.empty()) {
        auto [px, py] = q.front();
        q.pop();
        if (px - 1 >= 0 && visited.insert({px - 1, py}).second && is_non_white(img, px - 1, py)) {
            x0 = std::min(x0, px - 1);
            q.push({px - 1, py});
        }
        if (px + 1 < img.cols && visited.insert({px + 1, py}).second && is_non_white(img, px + 1, py)) {
            x1 = std::max(x1, px + 1);
            q.push({px + 1, py});
        }
        if (py - 1 >= 0 && visited.insert({px, py - 1}).second && is_non_white(img, px, py - 1)) {
            y0 = std::min(y0, py - 1);
            q.push({px, py - 1});
        }
        if (py + 1 < img.rows && visited.insert({px, py + 1}).second && is_non_white(img, px, py + 1)) {
            y1 = std::max(y1, py + 1);
            q.push({px, py + 1});
        }
    }
    return {x0, y0, x1 + 1, y1 + 1};
}

Symbol get_symbol(const cv::Mat& img, float cx, float cy) {
    float radius = 1.5;
    int x0 = std::max(static_cast<int>(std::round(cx - radius)), 0);
    int y0 = std::max(static_cast<int>(std::round(cy - radius)), 0);
    int x1 = std::min(static_cast<int>(std::round(cx + radius + 1)), img.cols);
    int y1 = std::min(static_cast<int>(std::round(cy + radius + 1)), img.rows);
    int color_num[static_cast<int>(PixelColor::NUM)] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (is_white(img, x, y)) {
                ++color_num[static_cast<int>(PixelColor::WHITE)];
            } else if (is_black(img, x, y)) {
                ++color_num[static_cast<int>(PixelColor::BLACK)];
            } else if (is_red(img, x, y)) {
                ++color_num[static_cast<int>(PixelColor::RED)];
            } else if (is_blue(img, x, y)) {
                ++color_num[static_cast<int>(PixelColor::BLUE)];
            } else if (is_green(img, x, y)) {
                ++color_num[static_cast<int>(PixelColor::GREEN)];
            } else if (is_cyan(img, x, y)) {
                ++color_num[static_cast<int>(PixelColor::CYAN)];
            } else if (is_magenta(img, x, y)) {
                ++color_num[static_cast<int>(PixelColor::MAGENTA)];
            } else if (is_yellow(img, x, y)) {
                ++color_num[static_cast<int>(PixelColor::YELLOW)];
            } else {
                ++color_num[static_cast<int>(PixelColor::UNKNOWN)];
            }
        }
    }
    Symbol symbol = static_cast<int>(PixelColor::UNKNOWN);
    int max_num = 0;
    for (int i = 0; i < static_cast<int>(PixelColor::NUM); ++i) {
        if (color_num[i] > max_num) {
            symbol = i;
            max_num = color_num[i];
        }
    }
    return symbol;
}

Symbols get_tile_symbols(const cv::Mat& img, int tile_x_size, int tile_y_size) {
    float unit_w = static_cast<float>(img.cols) / tile_x_size;
    float unit_h = static_cast<float>(img.rows) / tile_y_size;
    Symbols symbols;
    for (int y = 0; y < tile_y_size; ++y) {
        for (int x = 0; x < tile_x_size; ++x) {
            Symbol symbol = static_cast<int>(PixelColor::UNKNOWN);
            if (img.cols && img.rows) {
                float cx = (x + 0.5f) * unit_w;
                float cy = (y + 0.5f) * unit_h;
                symbol = get_symbol(img, cx, cy);
            }
            symbols.push_back(symbol);
        }
    }
    return symbols;
}

Symbols get_tile_symbols(const cv::Mat& img, const std::vector<std::vector<std::array<float, 2>>>& centers) {
    Symbols symbols;
    for (size_t y = 0; y < centers.size(); ++y) {
        for (size_t x = 0; x < centers[y].size(); ++x) {
            const auto& center = centers[y][x];
            Symbol symbol = get_symbol(img, center[0], center[1]);
            symbols.push_back(symbol);
        }
    }
    return symbols;
}

cv::Mat get_result_image(const cv::Mat& img, int tile_x_size, int tile_y_size, const std::array<int, 4>& bbox1, const std::array<int, 4>& bbox2, const Symbols& symbols) {
    cv::Mat img1 = img.clone();
    float unit_h = static_cast<float>(bbox2[3] - bbox2[1]) / tile_y_size;
    float unit_w = static_cast<float>(bbox2[2] - bbox2[0]) / tile_x_size;
    if (!symbols.empty()) {
        int radius = 2;
        for (int i = 0; i < tile_y_size; ++i) {
            for (int j = 0; j < tile_x_size; ++j) {
                int index = i * tile_x_size + j;
                int y = static_cast<int>(std::round(bbox2[1] + (i + 0.5) * unit_h));
                int x = static_cast<int>(std::round(bbox2[0] + (j + 0.5) * unit_w));
                auto color = cv::Scalar(0, 0, 0);
                auto marker = cv::MARKER_TILTED_CROSS;
                if (symbols[index] == static_cast<int>(PixelColor::WHITE))        color = cv::Scalar(255, 255, 255);
                else if (symbols[index] == static_cast<int>(PixelColor::BLACK))   color = cv::Scalar(0, 0, 0);
                else if (symbols[index] == static_cast<int>(PixelColor::RED))     color = cv::Scalar(0, 0, 255);
                else if (symbols[index] == static_cast<int>(PixelColor::BLUE))    color = cv::Scalar(255, 0, 0);
                else if (symbols[index] == static_cast<int>(PixelColor::GREEN))   color = cv::Scalar(0, 255, 0);
                else if (symbols[index] == static_cast<int>(PixelColor::CYAN))    color = cv::Scalar(255, 255, 0);
                else if (symbols[index] == static_cast<int>(PixelColor::MAGENTA)) color = cv::Scalar(255, 0, 255);
                else if (symbols[index] == static_cast<int>(PixelColor::YELLOW))  color = cv::Scalar(0, 255, 255);
                else                                                              { std::cout << static_cast<int>(symbols[index]) << "\n"; marker = cv::MARKER_CROSS; }
                cv::drawMarker(img1, cv::Point(x, y), color, marker, radius * 2);
            }
        }
    }
    for (int i = 0; i <= tile_y_size; ++i) {
        int x0 = static_cast<int>(bbox2[0]);
        int x1 = static_cast<int>(std::round(bbox2[0] + tile_x_size * unit_w));
        int y = static_cast<int>(std::round(bbox2[1] + i * unit_h));
        cv::line(img1, cv::Point(x0, y), cv::Point(x1, y), cv::Scalar(0, 0, 255));
    }
    for (int j = 0; j <= tile_x_size; ++j) {
        int x = static_cast<int>(std::round(bbox2[0] + j * unit_w));
        int y0 = static_cast<int>(bbox2[1]);
        int y1 = static_cast<int>(std::round(bbox2[1] + tile_y_size * unit_h));
        cv::line(img1, cv::Point(x, y0), cv::Point(x, y1), cv::Scalar(0, 0, 255));
    }
    cv::rectangle(img1, cv::Point(bbox1[0], bbox1[1]), cv::Point(bbox1[2], bbox1[3]), cv::Scalar(0, 0, 255));
    return img1;
}

cv::Mat get_result_image(const cv::Mat& img, const Dim& dim, const Calibration& calibration, const Symbols& symbols) {
    cv::Mat img1 = img.clone();
    auto [tile_x_num, tile_y_num, tile_x_size, tile_y_size] = dim;
    const int radius = 2;
    for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
        for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
            for (int y = 0; y < tile_y_size; ++y) {
                for (int x = 0; x < tile_x_size; ++x) {
                    int index = ((tile_y_id * tile_x_num + tile_x_id) * tile_y_size + y) * tile_x_size + x;
                    auto [cx, cy] = calibration.tiles[tile_y_id][tile_x_id].centers[y][x];
                    auto color = cv::Scalar(0, 0, 0);
                    auto marker = cv::MARKER_TILTED_CROSS;
                    if (symbols[index] == static_cast<int>(PixelColor::WHITE))        color = cv::Scalar(255, 255, 255);
                    else if (symbols[index] == static_cast<int>(PixelColor::BLACK))   color = cv::Scalar(0, 0, 0);
                    else if (symbols[index] == static_cast<int>(PixelColor::RED))     color = cv::Scalar(0, 0, 255);
                    else if (symbols[index] == static_cast<int>(PixelColor::BLUE))    color = cv::Scalar(255, 0, 0);
                    else if (symbols[index] == static_cast<int>(PixelColor::GREEN))   color = cv::Scalar(0, 255, 0);
                    else if (symbols[index] == static_cast<int>(PixelColor::CYAN))    color = cv::Scalar(255, 255, 0);
                    else if (symbols[index] == static_cast<int>(PixelColor::MAGENTA)) color = cv::Scalar(255, 0, 255);
                    else if (symbols[index] == static_cast<int>(PixelColor::YELLOW))  color = cv::Scalar(0, 255, 255);
                    else                                                              marker = cv::MARKER_CROSS;
                    cv::drawMarker(img1, cv::Point2f(cx, cy), color, marker, radius * 2);
                }
            }
        }
    }
    return img1;
}

ImageDecoder::ImageDecoder(SymbolType symbol_type) : m_symbol_codec(create_symbol_codec(symbol_type)) {
}

CalibrateResult ImageDecoder::Calibrate(const cv::Mat& img, const Dim& dim, const Transform& transform, bool result_image) {
    auto [tile_x_num, tile_y_num, tile_x_size, tile_y_size] = dim;
    cv::Mat img1 = transform_image(img, transform);
    cv::Mat img1_b = do_binarize(img1, transform.binarization_threshold);
    Calibration calibration;
    std::vector<std::vector<cv::Mat>> result_imgs(tile_y_num);
    for (auto& e : result_imgs) e.resize(tile_x_num);
    std::vector<std::array<float, 2>> centers;
    int start_x = 0;
    std::vector<int> start_y(tile_x_num * tile_x_size, 0);
    for (int y = 0; y < tile_y_num * tile_y_size; ++y) {
        for (int x = 0; x < tile_x_num * tile_x_size; ++x) {
            if (start_x >= img1_b.cols || start_y[x] >= img1_b.rows) {
                if (result_image) {
                    cv::cvtColor(img1_b, result_imgs[0][0], cv::COLOR_GRAY2BGR);
                }
                return std::make_tuple(std::move(img1), std::move(calibration), std::move(result_imgs));
            }
            cv::Mat img2_b = do_crop(img1_b, {start_x, start_y[x], img1.cols, img1.rows});
            auto [corner_x, corner_y] = detect_non_white_corner_nw(img2_b);
            auto [x0, y0, x1, y1] = get_non_white_region_bbox(img2_b, corner_x, corner_y);
            float cx = static_cast<float>(x0 + x1) / 2 + start_x;
            float cy = static_cast<float>(y0 + y1) / 2 + start_y[x];
            centers.push_back({cx, cy});
            start_x += x1;
            start_y[x] += y1;
        }
        start_x = 0;
    }
    calibration.valid = true;
    calibration.dim = dim;
    calibration.tiles.resize(tile_y_num);
    cv::Mat result_img = img1.clone();
    const int radius = 2;
    for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
        calibration.tiles[tile_y_id].resize(tile_x_num);
        for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
            calibration.tiles[tile_y_id][tile_x_id].centers.resize(tile_y_size);
            for (int y = 0; y < tile_y_size; ++y) {
                calibration.tiles[tile_y_id][tile_x_id].centers[y].resize(tile_x_size);
                for (int x = 0; x < tile_x_size; ++x) {
                    int index = ((tile_y_id * tile_y_size + y) * tile_x_num + tile_x_id) * tile_x_size + x;
                    calibration.tiles[tile_y_id][tile_x_id].centers[y][x] = centers[index];
                    if (result_image) {
                        cv::circle(result_img, cv::Point2f(centers[index][0], centers[index][1]), radius, cv::Scalar(0, 0, 255), cv::FILLED);
                    }
                }
            }
        }
    }
    if (result_image) {
        result_imgs[0][0] = result_img;
    }
    return std::make_tuple(std::move(img1), std::move(calibration), std::move(result_imgs));
}

ImageDecodeResult ImageDecoder::Decode(const cv::Mat& img, const Dim& dim, const Transform& transform, const Calibration& calibration, bool result_image) {
    auto [tile_x_num, tile_y_num, tile_x_size, tile_y_size] = dim;
    cv::Mat img1 = transform_image(img, transform);
    Symbols symbols;
    std::vector<std::vector<cv::Mat>> result_imgs(tile_y_num);
    for (auto& e : result_imgs) e.resize(tile_x_num);
    if (calibration.valid) {
        cv::Mat img1_p = do_pixelize(img1, m_symbol_codec->GetSymbolType(), transform.pixelization_threshold);
        for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
            for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
                auto tile_symbols = get_tile_symbols(img1_p, calibration.tiles[tile_y_id][tile_x_id].centers);
                symbols.insert(symbols.end(), tile_symbols.begin(), tile_symbols.end());
            }
        }
        if (result_image) {
            result_imgs[0][0] = get_result_image(img1, dim, calibration, symbols);
        }
    } else {
        img1 = do_auto_quad(img1, transform.binarization_threshold);
        cv::Mat img1_b = do_binarize(img1, transform.binarization_threshold);
        auto [tiling_success, tile_bboxes] = get_tile_bboxes(img1_b, tile_x_num, tile_y_num);
        if (!tiling_success) return std::make_tuple(false, 0, Bytes(), std::move(symbols), std::move(img1), std::move(result_imgs));
        for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
            for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
                cv::Mat tile_img = do_crop(img1, tile_bboxes[tile_y_id][tile_x_id]);
                cv::Mat tile_img1 = do_auto_quad(tile_img, transform.binarization_threshold);
                std::array<int, 4> bbox1 = {0, 0, tile_img1.cols, tile_img1.rows};
                std::array<int, 4> bbox2 = get_tile_bbox2(bbox1, tile_x_size, tile_y_size);
                cv::Mat tile_img2 = do_crop(tile_img1, bbox2);
                tile_img2 = do_pixelize(tile_img2, m_symbol_codec->GetSymbolType(), transform.pixelization_threshold);
                Symbols tile_symbols = get_tile_symbols(tile_img2, tile_x_size, tile_y_size);
                symbols.insert(symbols.end(), tile_symbols.begin(), tile_symbols.end());
                if (result_image) {
                    result_imgs[tile_y_id][tile_x_id] = get_result_image(tile_img1, tile_x_size, tile_y_size, bbox1, bbox2, tile_symbols);
                }
            }
        }
    }
    auto [success, part_id, part_bytes] = m_symbol_codec->Decode(symbols);
    return std::make_tuple(success, part_id, std::move(part_bytes), std::move(symbols), std::move(img1), std::move(result_imgs));
}
