#include <opencv2/opencv.hpp>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <numeric>
#include <algorithm>

cv::Mat letterbox(const cv::Mat& img, int target_w = 1280, int target_h = 720) {
    double scale = std::min(static_cast<double>(target_w) / img.cols,
                            static_cast<double>(target_h) / img.rows);
    int new_w = static_cast<int>(img.cols * scale);
    int new_h = static_cast<int>(img.rows * scale);

    cv::Mat resized;
    cv::resize(img, resized, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);

    cv::Mat canvas(target_h, target_w, img.type(), cv::Scalar(0, 0, 0));
    int x_offset = (target_w - new_w) / 2;
    int y_offset = (target_h - new_h) / 2;
    resized.copyTo(canvas(cv::Rect(x_offset, y_offset, new_w, new_h)));
    return canvas;
}

void run_orb_scenario(
    const std::string& title,
    cv::Ptr<cv::ORB> orb,
    const cv::Mat& ref_720p,
    const cv::Mat& scene_720p,
    int num_frames,
    int warmup_frames
) {
    std::vector<cv::KeyPoint> ref_kpts;
    cv::Mat ref_descs;
    orb->detectAndCompute(ref_720p, cv::noArray(), ref_kpts, ref_descs);

    cv::Ptr<cv::BFMatcher> bf = cv::BFMatcher::create(cv::NORM_HAMMING, true);

    std::vector<double> t_extract(num_frames, 0.0);
    std::vector<double> t_match(num_frames, 0.0);
    std::vector<double> t_total(num_frames, 0.0);

    size_t total_scene_descs = 0;
    size_t total_good_matches = 0;

    int total_iterations = warmup_frames + num_frames;
    using clock = std::chrono::steady_clock;
    clock::time_point bench_start;

    std::vector<cv::KeyPoint> scene_kpts;
    cv::Mat scene_descs;
    std::vector<cv::DMatch> matches;

    for (int iter = 0; iter < total_iterations; ++iter) {
        if (iter == warmup_frames) {
            bench_start = clock::now();
        }

        auto t0 = clock::now();
        scene_kpts.clear();
        orb->detectAndCompute(scene_720p, cv::noArray(), scene_kpts, scene_descs);
        auto t1 = clock::now();

        int good_matches = 0;
        matches.clear();
        if (!ref_descs.empty() && !scene_descs.empty()) {
            bf->match(ref_descs, scene_descs, matches);
            for (const auto& m : matches) {
                if (m.distance < 60.0f) {
                    good_matches++;
                }
            }
        }
        auto t2 = clock::now();

        if (iter >= warmup_frames) {
            int f = iter - warmup_frames;
            t_extract[f] = std::chrono::duration<double, std::micro>(t1 - t0).count();
            t_match[f]   = std::chrono::duration<double, std::micro>(t2 - t1).count();
            t_total[f]   = std::chrono::duration<double, std::micro>(t2 - t0).count();

            total_scene_descs  += scene_descs.rows;
            total_good_matches += good_matches;
        }
    }

    auto bench_end = clock::now();
    double total_sec = std::chrono::duration<double>(bench_end - bench_start).count();

    auto print_stats = [&](const char* name, const std::vector<double>& v) {
        double min_v = *std::min_element(v.begin(), v.end());
        double max_v = *std::max_element(v.begin(), v.end());
        double avg_v = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        std::cout << std::left << std::setw(20) << name
                  << " | Min: " << std::setw(8) << std::fixed << std::setprecision(1) << min_v << " us"
                  << " | Moy: " << std::setw(8) << avg_v << " us"
                  << " | Max: " << std::setw(8) << max_v << " us\n";
    };

    std::cout << title << "\n";
    std::cout << "Frames testees          : " << num_frames << " (OpenCV threads: " << cv::getNumThreads() << ")\n";
    std::cout << "Descripteurs Ref        : " << ref_descs.rows << "\n";
    std::cout << "Descripteurs Scene (moy): " << (total_scene_descs / num_frames) << " / frame\n";
    std::cout << "Matches < 60 (moy)      : " << (total_good_matches / num_frames) << " / frame\n";
    print_stats("1. Extraction ORB", t_extract);
    print_stats("2. Matching BF", t_match);
    print_stats("Total / Frame", t_total);
    std::cout << "Debit global            : " << std::fixed << std::setprecision(2) << (num_frames / total_sec) << " FPS\n";
}

int main(int argc, char** argv) {
    std::string ref_path   = (argc > 1) ? argv[1] : "images/coke_ref.jpg";
    std::string scene_path = (argc > 2) ? argv[2] : "images/park_and_coke.jpg";
    int num_frames         = (argc > 3) ? std::stoi(argv[3]) : 50;
    int warmup_frames      = (argc > 4) ? std::stoi(argv[4]) : 5;

    cv::Mat ref_raw   = cv::imread(ref_path, cv::IMREAD_COLOR);
    cv::Mat scene_raw = cv::imread(scene_path, cv::IMREAD_COLOR);

    if (ref_raw.empty()) {
        std::cerr << "Impossible de charger l'image de reference : " << ref_path << "\n";
        return 1;
    }
    if (scene_raw.empty()) {
        std::cerr << "Impossible de charger l'image de scene : " << scene_path << "\n";
        return 1;
    }

    cv::Mat ref_720p   = letterbox(ref_raw, 1280, 720);
    cv::Mat scene_720p = letterbox(scene_raw, 1280, 720);

    std::cout << "Benchmark OpenCV C++ sur ARM (" << scene_path << ", 1280x720)...\n";

    cv::Ptr<cv::ORB> orb_tuned = cv::ORB::create(5000);
    orb_tuned->setFastThreshold(60);
    orb_tuned->setNLevels(2);
    orb_tuned->setScaleFactor(2.0f);
    orb_tuned->setScoreType(cv::ORB::FAST_SCORE);

    run_orb_scenario(
        "OPENCV C++ - ORB TUNED (Iso-Hardware : 2 scales, 2.0x, FAST, thr=60)",
        orb_tuned, ref_720p, scene_720p, num_frames, warmup_frames
    );
    cv::Ptr<cv::ORB> orb_std = cv::ORB::create();

    run_orb_scenario(
        "OPENCV C++ - ORB STANDARD (Defaut OpenCV : 8 scales, 1.2x, Harris)",
        orb_std, ref_720p, scene_720p, num_frames, warmup_frames
    );

    return 0;
}
