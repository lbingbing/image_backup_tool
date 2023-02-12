#include <iostream>
#include <exception>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>

#include <boost/program_options.hpp>

#include "image_codec.h"

class App {
public:
    App(const std::string& output_file, SymbolType symbol_type, const Dim& dim, uint32_t part_num, int mp, const Transform& transform) : m_output_file(output_file), m_image_decode_worker(symbol_type, dim), m_part_num(part_num), m_mp(mp), m_transform(transform) {
    }

    bool IsRunning() { return m_running; }

    void Start() {
        m_running = true;
        auto get_transform_fn = [this] {
            std::lock_guard<std::mutex> lock(m_transform_mtx);
            return m_transform;
        };
        m_fetch_image_thread = std::make_unique<std::thread>(&ImageDecodeWorker::FetchImageWorker, &m_image_decode_worker, std::ref(m_running), std::ref(m_frame_q), 25);
        for (int i = 0; i < m_mp; ++i) {
            m_decode_image_threads.emplace_back(&ImageDecodeWorker::DecodeImageWorker, &m_image_decode_worker, std::ref(m_part_q), std::ref(m_frame_q), get_transform_fn, m_calibration);
        }
        auto save_part_progress_cb = [](const ImageDecodeWorker::SavePartProgress& save_part_progress){
            std::cout << save_part_progress.frame_num << " frames processed, " << save_part_progress.done_part_num << "/" << save_part_progress.part_num << " parts transferred, fps=" << std::fixed << std::setprecision(2) << save_part_progress.fps << ", done_fps=" << std::fixed << std::setprecision(2) << save_part_progress.done_fps << ", bps=" << std::fixed << std::setprecision(0) << save_part_progress.bps << ", left_time=" << std::setfill('0') << std::setw(2) << save_part_progress.left_days << "d" << std::setw(2) << save_part_progress.left_hours << "h" << std::setw(2) << save_part_progress.left_minutes << "m" << std::setw(2) << save_part_progress.left_seconds << "s" << std::setfill(' ') << "\n";
        };
        auto save_part_complete_cb = []() {
            std::cout << "transfer done\n";
        };
        auto save_part_error_cb = [](const std::string& msg) {
            std::cout << "error\n";
            std::cout << msg << "\n";
        };
        m_save_part_thread = std::make_unique<std::thread>(&ImageDecodeWorker::SavePartWorker, &m_image_decode_worker, std::ref(m_running), std::ref(m_part_q), m_output_file, m_part_num, save_part_progress_cb, nullptr, save_part_complete_cb, save_part_error_cb, nullptr, nullptr, nullptr, nullptr);
    }

    void Stop() {
        m_running = false;
        m_fetch_image_thread->join();
        m_fetch_image_thread.reset();
        for (size_t i = 0; i < m_mp; ++i) {
            m_frame_q.PushNull();
        }
        for (auto& t : m_decode_image_threads) {
            t.join();
        }
        m_decode_image_threads.clear();
        m_part_q.PushNull();
        m_save_part_thread->join();
        m_save_part_thread.reset();
    }

private:
    std::string m_output_file;
    ImageDecodeWorker m_image_decode_worker;
    uint32_t m_part_num = 0;
    int m_mp = 0;
    Transform m_transform;
    Calibration m_calibration;
    ThreadSafeQueue<std::pair<uint64_t, cv::Mat>> m_frame_q{128};
    ThreadSafeQueue<DecodeResult> m_part_q{128};
    std::unique_ptr<std::thread> m_fetch_image_thread;
    std::vector<std::thread> m_decode_image_threads;
    std::unique_ptr<std::thread> m_save_part_thread;
    std::mutex m_transform_mtx;
    std::atomic<bool> m_running = false;
};

int main(int argc, char** argv) {
    try {
        std::string output_file;
        std::string symbol_type_str;
        std::string dim_str;
        uint32_t part_num = 0;
        int mp = 1;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("output_file", boost::program_options::value<std::string>(&output_file), "output file");
        desc_handler("symbol_type", boost::program_options::value<std::string>(&symbol_type_str), "symbol type");
        desc_handler("dim", boost::program_options::value<std::string>(&dim_str), "dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size");
        desc_handler("part_num", boost::program_options::value<uint32_t>(&part_num), "part num");
        desc_handler("mp", boost::program_options::value<int>(&mp), "multiprocessing");
        add_transform_options(desc_handler);
        boost::program_options::positional_options_description p_desc;
        p_desc.add("output_file", 1);
        p_desc.add("symbol_type", 1);
        p_desc.add("dim", 1);
        p_desc.add("part_num", 1);
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p_desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        if (!vm.count("output_file")) {
            throw std::invalid_argument("output_file not specified");
        }

        if (!vm.count("symbol_type")) {
            throw std::invalid_argument("symbol_type not specified");
        }

        if (!vm.count("dim")) {
            throw std::invalid_argument("dim not specified");
        }

        if (!vm.count("part_num")) {
            throw std::invalid_argument("part_num not specified");
        }

        auto symbol_type = parse_symbol_type(symbol_type_str);
        auto dim = parse_dim(vm["dim"].as<std::string>());
        Transform transform = get_transform(vm);
        App app(output_file, symbol_type, dim, part_num, mp, transform);
        std::cout << "start\n";
        app.Start();
        while (app.IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (std::filesystem::is_regular_file("decode_image_stream.stop")) {
                std::cout << "stop\n";
                break;
            }
        }
        app.Stop();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
