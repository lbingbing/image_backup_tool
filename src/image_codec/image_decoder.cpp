#include <set>
#include <queue>
#include <iostream>
#include <fstream>
#include <exception>

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
                        f.read(reinterpret_cast<char*>(&center.first), sizeof(center.first));
                        f.read(reinterpret_cast<char*>(&center.second), sizeof(center.second));
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
                        f.write(reinterpret_cast<const char*>(&center.first), sizeof(center.first));
                        f.write(reinterpret_cast<const char*>(&center.second), sizeof(center.second));
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

std::vector<int> find_tile_bbox1(const cv::Mat& img) {
    int y0 = find_non_white_boundary_n(img);
    int y1 = find_non_white_boundary_s(img);
    int x0 = find_non_white_boundary_w(img);
    int x1 = find_non_white_boundary_e(img);
    std::vector<int> bbox1{ x0, y0, x1, y1 };
    return bbox1;
}

std::vector<int> get_tile_bbox2(const std::vector<int>& bbox1, int tile_x_size, int tile_y_size) {
    int x0 = bbox1[0];
    int y0 = bbox1[1];
    int x1 = bbox1[2];
    int y1 = bbox1[3];
    float unit_h = static_cast<float>((y1 - y0)) / (tile_y_size + 2);
    float unit_w = static_cast<float>((x1 - x0)) / (tile_x_size + 2);
    y0 += static_cast<int>(round(unit_h));
    y1 -= static_cast<int>(round(unit_h));
    x0 += static_cast<int>(round(unit_w));
    x1 -= static_cast<int>(round(unit_w));
    std::vector<int> bbox2{ x0, y0, x1, y1 };
    return bbox2;
}

std::pair<std::vector<int>, std::vector<int>> get_spaces(const cv::Mat& img) {
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
    return std::make_pair(std::move(space_xs), std::move(space_ys));
}

std::pair<bool, std::vector<std::vector<std::vector<int>>>> get_tile_bboxes(const cv::Mat& img, int tile_x_num, int tile_y_num) {
    auto [space_xs, space_ys] = get_spaces(img);
    if (static_cast<int>(space_ys.size()) != tile_y_num - 1 ||
        static_cast<int>(space_xs.size()) != tile_x_num - 1) {
        return { false, {} };
    }
    std::vector<std::vector<std::vector<int>>> tile_bboxs(tile_y_num);
    for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
        tile_bboxs[tile_y_id].resize(tile_x_num);
        for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
            int x0 = tile_x_id > 0 ? space_xs[tile_x_id - 1] : 0;
            int y0 = tile_y_id > 0 ? space_ys[tile_y_id - 1] : 0;
            int x1 = tile_x_id < tile_x_num - 1 ? space_xs[tile_x_id] : img.cols;
            int y1 = tile_y_id < tile_y_num - 1 ? space_ys[tile_y_id] : img.rows;
            tile_bboxs[tile_y_id][tile_x_id] = { x0, y0, x1, y1 };
        }
    }
    return std::make_pair(true, std::move(tile_bboxs));
}

std::tuple<int, int, int, int> get_non_white_region_bbox(const cv::Mat& img, int x, int y) {
    int x0 = x;
    int y0 = y;
    int x1 = x;
    int y1 = y;
    std::set<std::pair<int, int>> visited;
    std::queue<std::pair<int, int>> q;
    q.emplace(x, y);
    while (!q.empty()) {
        auto [px, py] = q.front();
        q.pop();
        if (px - 1 >= 0 && visited.emplace(px - 1, py).second && is_non_white(img, px - 1, py)) {
            x0 = std::min(x0, px - 1);
            q.emplace(px - 1, py);
        }
        if (px + 1 < img.cols && visited.emplace(px + 1, py).second && is_non_white(img, px + 1, py)) {
            x1 = std::max(x1, px + 1);
            q.emplace(px + 1, py);
        }
        if (py - 1 >= 0 && visited.emplace(px, py - 1).second && is_non_white(img, px, py - 1)) {
            y0 = std::min(y0, py - 1);
            q.emplace(px, py - 1);
        }
        if (py + 1 < img.rows && visited.emplace(px, py + 1).second && is_non_white(img, px, py + 1)) {
            y1 = std::max(y1, py + 1);
            q.emplace(px, py + 1);
        }
    }
    return { x0, y0, x1 + 1, y1 + 1 };
}

Pixel get_pixel(const cv::Mat& img, float cx, float cy) {
    float radius = 1.5;
    int x0 = std::max(static_cast<int>(std::round(cx - radius)), 0);
    int y0 = std::max(static_cast<int>(std::round(cy - radius)), 0);
    int x1 = std::min(static_cast<int>(std::round(cx + radius + 1)), img.cols);
    int y1 = std::min(static_cast<int>(std::round(cy + radius + 1)), img.rows);
    int color_num[PIXEL_NUM] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (is_white(img, x, y)) {
                ++color_num[PIXEL_WHITE];
            } else if (is_black(img, x, y)) {
                ++color_num[PIXEL_BLACK];
            } else if (is_red(img, x, y)) {
                ++color_num[PIXEL_RED];
            } else if (is_blue(img, x, y)) {
                ++color_num[PIXEL_BLUE];
            } else if (is_green(img, x, y)) {
                ++color_num[PIXEL_GREEN];
            } else if (is_cyan(img, x, y)) {
                ++color_num[PIXEL_CYAN];
            } else if (is_magenta(img, x, y)) {
                ++color_num[PIXEL_MAGENTA];
            } else if (is_yellow(img, x, y)) {
                ++color_num[PIXEL_YELLOW];
            } else {
                ++color_num[PIXEL_UNKNOWN];
            }
        }
    }
    Pixel pixel = PIXEL_UNKNOWN;
    int max_num = 0;
    for (int i = 0; i < PIXEL_NUM; ++i) {
        if (color_num[i] > max_num) {
            pixel = i;
            max_num = color_num[i];
        }
    }
    return pixel;
}

Pixels get_tile_pixels(const cv::Mat& img, int tile_x_size, int tile_y_size) {
    float unit_w = static_cast<float>(img.cols) / tile_x_size;
    float unit_h = static_cast<float>(img.rows) / tile_y_size;
    Pixels pixels;
    for (int y = 0; y < tile_y_size; ++y) {
        for (int x = 0; x < tile_x_size; ++x) {
            Pixel pixel = PIXEL_UNKNOWN;
            if (img.cols && img.rows) {
                float cx = (x + 0.5f) * unit_w;
                float cy = (y + 0.5f) * unit_h;
                pixel = get_pixel(img, cx, cy);
            }
            pixels.push_back(pixel);
        }
    }
    return pixels;
}

Pixels get_tile_pixels(const cv::Mat& img, const std::vector<std::vector<std::pair<float, float>>>& centers) {
    Pixels pixels;
    for (size_t y = 0; y < centers.size(); ++y) {
        for (size_t x = 0; x < centers[y].size(); ++x) {
            const auto& center = centers[y][x];
            Pixel pixel = get_pixel(img, center.first, center.second);
            pixels.push_back(pixel);
        }
    }
    return pixels;
}

cv::Mat get_result_image(const cv::Mat& img, int tile_x_size, int tile_y_size, const std::vector<int>& bbox1, const std::vector<int>& bbox2, const Pixels& pixels) {
    cv::Mat img1 = img.clone();
    float unit_h = static_cast<float>(bbox2[3] - bbox2[1]) / tile_y_size;
    float unit_w = static_cast<float>(bbox2[2] - bbox2[0]) / tile_x_size;
    if (!pixels.empty()) {
        int radius = 2;
        for (int i = 0; i < tile_y_size; ++i) {
            for (int j = 0; j < tile_x_size; ++j) {
                int index = i * tile_x_size + j;
                int y = static_cast<int>(round(bbox2[1] + (i + 0.5) * unit_h));
                int x = static_cast<int>(round(bbox2[0] + (j + 0.5) * unit_w));
                auto color = cv::Scalar(0, 0, 0);
                auto marker = cv::MARKER_TILTED_CROSS;
                if (pixels[index] == PIXEL_WHITE)        color = cv::Scalar(255, 255, 255);
                else if (pixels[index] == PIXEL_BLACK)   color = cv::Scalar(0, 0, 0);
                else if (pixels[index] == PIXEL_RED)     color = cv::Scalar(0, 0, 255);
                else if (pixels[index] == PIXEL_BLUE)    color = cv::Scalar(255, 0, 0);
                else if (pixels[index] == PIXEL_GREEN)   color = cv::Scalar(0, 255, 0);
                else if (pixels[index] == PIXEL_CYAN)    color = cv::Scalar(255, 255, 0);
                else if (pixels[index] == PIXEL_MAGENTA) color = cv::Scalar(255, 0, 255);
                else if (pixels[index] == PIXEL_YELLOW)  color = cv::Scalar(0, 255, 255);
                else                                     {std::cout << static_cast<int>(pixels[index]) << "\n";marker = cv::MARKER_CROSS;}
                cv::drawMarker(img1, cv::Point(x, y), color, marker, radius * 2);
            }
        }
    }
    for (int i = 0; i <= tile_y_size; ++i) {
        int x0 = static_cast<int>(bbox2[0]);
        int x1 = static_cast<int>(round(bbox2[0] + tile_x_size * unit_w));
        int y = static_cast<int>(round(bbox2[1] + i * unit_h));
        cv::line(img1, cv::Point(x0, y), cv::Point(x1, y), cv::Scalar(0, 0, 255));
    }
    for (int j = 0; j <= tile_x_size; ++j) {
        int x = static_cast<int>(bbox2[0] + j * unit_w);
        int y0 = static_cast<int>(bbox2[1]);
        int y1 = static_cast<int>(bbox2[1] + tile_y_size * unit_h);
        cv::line(img1, cv::Point(x, y0), cv::Point(x, y1), cv::Scalar(0, 0, 255));
    }
    cv::rectangle(img1, cv::Point(bbox1[0], bbox1[1]), cv::Point(bbox1[2], bbox1[3]), cv::Scalar(0, 0, 255));
    return img1;
}

cv::Mat get_result_image(const cv::Mat& img, const Dim& dim, const Calibration& calibration, const Pixels& pixels) {
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
                    if (pixels[index] == PIXEL_WHITE)        color = cv::Scalar(255, 255, 255);
                    else if (pixels[index] == PIXEL_BLACK)   color = cv::Scalar(0, 0, 0);
                    else if (pixels[index] == PIXEL_RED)     color = cv::Scalar(0, 0, 255);
                    else if (pixels[index] == PIXEL_BLUE)    color = cv::Scalar(255, 0, 0);
                    else if (pixels[index] == PIXEL_GREEN)   color = cv::Scalar(0, 255, 0);
                    else if (pixels[index] == PIXEL_CYAN)    color = cv::Scalar(255, 255, 0);
                    else if (pixels[index] == PIXEL_MAGENTA) color = cv::Scalar(255, 0, 255);
                    else if (pixels[index] == PIXEL_YELLOW)  color = cv::Scalar(0, 255, 255);
                    else                                     marker = cv::MARKER_CROSS;
                    cv::drawMarker(img1, cv::Point2f(cx, cy), color, marker, radius * 2);
                }
            }
        }
    }
    return img1;
}

ImageDecoder::ImageDecoder(PixelType pixel_type) : m_pixel_codec(create_pixel_codec(pixel_type)) {
}

CalibrateResult ImageDecoder::Calibrate(const cv::Mat& img, const Dim& dim, const Transform& transform, bool result_image) {
    auto [tile_x_num, tile_y_num, tile_x_size, tile_y_size] = dim;
    cv::Mat img1 = transform_image(img, transform);
    cv::Mat img1_b = do_binarize(img1, transform.binarization_threshold);
    Calibration calibration;
    std::vector<std::vector<cv::Mat>> result_imgs(tile_y_num);
    for (auto& e : result_imgs) e.resize(tile_x_num);
    std::vector<std::pair<float, float>> centers;
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
            cv::Mat img2_b = do_crop(img1_b, { start_x, start_y[x], img1.cols, img1.rows });
            auto [corner_x, corner_y] = detect_non_white_corner_nw(img2_b);
            auto [x0, y0, x1, y1] = get_non_white_region_bbox(img2_b, corner_x, corner_y);
            float cx = static_cast<float>(x0 + x1) / 2 + start_x;
            float cy = static_cast<float>(y0 + y1) / 2 + start_y[x];
            centers.emplace_back(cx, cy);
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
                        cv::circle(result_img, cv::Point2f(centers[index].first, centers[index].second), radius, cv::Scalar(0, 0, 255), cv::FILLED);
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
    Pixels pixels;
    std::vector<std::vector<cv::Mat>> result_imgs(tile_y_num);
    for (auto& e : result_imgs) e.resize(tile_x_num);
    if (calibration.valid) {
        cv::Mat img1_p = do_pixelize(img1, m_pixel_codec->GetPixelType(), transform.pixelization_threshold);
        for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
            for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
                auto tile_pixels = get_tile_pixels(img1_p, calibration.tiles[tile_y_id][tile_x_id].centers);
                pixels.insert(pixels.end(), tile_pixels.begin(), tile_pixels.end());
            }
        }
        if (result_image) {
            result_imgs[0][0] = get_result_image(img1, dim, calibration, pixels);
        }
    } else {
        img1 = do_auto_quad(img1, transform.binarization_threshold);
        cv::Mat img1_b = do_binarize(img1, transform.binarization_threshold);
        auto [tiling_success, tile_bboxes] = get_tile_bboxes(img1_b, tile_x_num, tile_y_num);
        if (!tiling_success) return std::make_tuple(false, 0, Bytes(), std::move(pixels), std::move(img1), std::move(result_imgs));
        for (int tile_y_id = 0; tile_y_id < tile_y_num; ++tile_y_id) {
            for (int tile_x_id = 0; tile_x_id < tile_x_num; ++tile_x_id) {
                cv::Mat tile_img = do_crop(img1, tile_bboxes[tile_y_id][tile_x_id]);
                cv::Mat tile_img1 = do_auto_quad(tile_img, transform.binarization_threshold);
                std::vector<int> bbox1 = { 0, 0, tile_img1.cols, tile_img1.rows };
                std::vector<int> bbox2 = get_tile_bbox2(bbox1, tile_x_size, tile_y_size);
                cv::Mat tile_img2 = do_crop(tile_img1, bbox2);
                tile_img2 = do_pixelize(tile_img2, m_pixel_codec->GetPixelType(), transform.pixelization_threshold);
                Pixels tile_pixels = get_tile_pixels(tile_img2, tile_x_size, tile_y_size);
                pixels.insert(pixels.end(), tile_pixels.begin(), tile_pixels.end());
                if (result_image) {
                    result_imgs[tile_y_id][tile_x_id] = get_result_image(tile_img1, tile_x_size, tile_y_size, bbox1, bbox2, tile_pixels);
                }
            }
        }
    }
    auto [success, part_id, part_bytes] = m_pixel_codec->Decode(pixels);
    return std::make_tuple(success, part_id, std::move(part_bytes), std::move(pixels), std::move(img1), std::move(result_imgs));
}
