#include "Tracker.hpp"
#include <nlohmann/json.hpp>
#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <regex>

// -----------------------------------------------------------------------------
// Parse ISO timestamp string to seconds-since-epoch (double)
static double parse_iso(const std::string& s)
{
    std::tm tm{}; double frac = 0.0; char dot;
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.peek() == '.') {
        ss >> dot;
        std::string micros;
        ss >> micros;
        frac = std::stod("0." + micros);
    }
    std::time_t t = timegm(&tm);
    return double(t) + frac;
}

static std::string format_iso(double sec)
{
    std::time_t t_int = static_cast<std::time_t>(sec);
    double frac = sec - t_int;
    std::tm *tm = gmtime(&t_int);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
    std::ostringstream out;
    out << buf << '.' << std::setw(6) << std::setfill('0') << int(frac * 1e6 + 0.5);
    return out.str();
}

// -----------------------------------------------------------------------------
// Read defaults from .ini config
std::string get_ini_value(const std::string& section, const std::string& key, const std::string& path = "defaults.ini") {
    std::ifstream file(path);
    if (!file.is_open()) return "";

    std::cout << "loading [" << section << "][" << key << "] from ini" << std::endl;
    std::string line, current_section;
    std::regex section_re(R"(\[(.*?)\])");
    std::regex keyval_re(R"(^\s*([^=]+?)\s*=\s*(.*?)\s*(?:[#;].*)?$)");
    std::smatch match;

    while (std::getline(file, line)) {
        if (std::regex_match(line, match, section_re)) {
            current_section = match[1];
        } else if (current_section == section && std::regex_match(line, match, keyval_re)) {
            if (match[1] == key) {
                return match[2];
            }
        }
    }
    return "";
}

// -----------------------------------------------------------------------------

struct Frame { double ts; std::vector<Detection> dets; };

static std::vector<Frame> load_frames(const std::string& path)
{
    std::ifstream in(path);
    nlohmann::ordered_json j; in >> j;
    std::vector<Frame> frames;
    for (auto& f : j) {
        Frame fr; fr.ts = parse_iso(f.at("timestamp"));
        for (auto& d : f.at("detections")) {
            double w = d.at("w");
            double h = d.at("h");

            if (w <= 0.0 || h <= 0.0) {
                std::ostringstream msg;
                msg << "Invalid detection at " << f.at("timestamp")
                    << " with w=" << w << ", h=" << h;
                throw std::runtime_error(msg.str());
            }

            fr.dets.push_back({d.at("x"), d.at("y"), w, h});
        }

        frames.push_back(std::move(fr));
    }
    return frames;
}

static void save_tracks(const std::string& path,
                        const std::vector<std::pair<double, std::vector<Track>>>& dump)
{
    nlohmann::ordered_json j = nlohmann::json::array();
    for (auto& [ts, trks] : dump) {
        nlohmann::ordered_json o; o["timestamp"] = format_iso(ts);
        for (auto& t : trks) {
            o["tracks"].push_back({
                {"id", t.id},
                {"x", t.rect.at<double>(0)},
                {"y", t.rect.at<double>(1)},
                {"w", t.rect.at<double>(2)},
                {"h", t.rect.at<double>(3)}
            });
        }
        j.push_back(o);
    }
    std::ofstream(path) << std::setw(2) << j;
}

static void draw_vis(const std::string& dir, int idx, const std::vector<Track>& trks,
                     int W = 800, int H = 600)
{
    cv::Mat img(H, W, CV_8UC3, cv::Scalar(30, 30, 30));
    for (const auto& t : trks) {
        int x = int(t.rect.at<double>(0) * W);
        int y = int(t.rect.at<double>(1) * H);
        int w = int(t.rect.at<double>(2) * W);
        int h = int(t.rect.at<double>(3) * H);
        cv::rectangle(img, {x, y, w, h}, cv::Scalar(0, 255, 0), 2);
        cv::putText(img, std::to_string(t.id), {x, y - 5},
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1);
    }
    std::ostringstream fn; fn << dir << "/frame_" << std::setw(4)
                              << std::setfill('0') << idx << ".png";
    cv::imwrite(fn.str(), img);
}

// -----------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Read defaults from .ini
    std::string ini_in    = get_ini_value("tracker", "input");
    std::string ini_out   = get_ini_value("tracker", "output");
    std::string ini_vis   = get_ini_value("tracker", "vis-dir");
    std::string ini_dist  = get_ini_value("tracker", "max-dist");
    std::string ini_alpha = get_ini_value("tracker", "alpha");
    std::string ini_age   = get_ini_value("tracker", "max-age");

    // Defaults for CLI
    std::string in_path  = ini_in;
    std::string out_path = ini_out;
    std::string vis_dir  = ini_vis;
    double max_dist      = ini_dist.empty()  ? 0.15 : std::stod(ini_dist);
    double alpha         = ini_alpha.empty() ? 0.7  : std::stod(ini_alpha);
    int max_age          = ini_age.empty()   ? 5    : std::stoi(ini_age);

    CLI::App app{"Tracking solution"};
    app.add_option("--input", in_path, "Input JSON path");
    app.add_option("--output", out_path, "Output JSON path");
    app.add_option("--vis-dir", vis_dir, "Visualization directory");
    app.add_option("--max-dist", max_dist, "Max distance threshold");
    app.add_option("--max-age", max_age, "Max age before removing track");
    app.add_option("--alpha", alpha, "Weight of distance vs IoU");
    CLI11_PARSE(app, argc, argv);

    // Main tracking loop
    std::vector<Frame> frames = load_frames(in_path);
    Tracker tracker(max_dist, max_age, alpha);
    std::vector<std::pair<double, std::vector<Track>>> dump;
    std::filesystem::create_directories(vis_dir);

    for (size_t i = 0; i < frames.size(); ++i) {
        tracker.step(frames[i].ts, frames[i].dets);
        dump.emplace_back(frames[i].ts, tracker.tracks());
        draw_vis(vis_dir, i, tracker.tracks());
    }

    save_tracks(out_path, dump);
    std::cout << "Tracking complete. Frames: " << frames.size() << "\n";
    return 0;
}
