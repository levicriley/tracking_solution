#include "Tracker.hpp"
#include <nlohmann/json.hpp>
#include <CLI/CLI.hpp>
#include <opencv2/opencv.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <regex>

// ───────────────── timestamps ───────────────────────────────────────
static double parse_iso(const std::string& s)
{
    std::tm tm{}; char dot; double frac = 0.0;
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.peek() == '.') { ss >> dot; std::string us; ss >> us; frac = std::stod("0."+us); }
    return static_cast<double>(timegm(&tm)) + frac;
}

static std::string format_iso(double sec)
{
    std::time_t ti = static_cast<std::time_t>(sec);
    double frac = sec - ti;
    std::tm *tm = gmtime(&ti);
    char buf[32]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S",tm);
    std::ostringstream out; out << buf << '.' << std::setw(6) << std::setfill('0')
                                << int(frac*1e6 + 0.5);
    return out.str();
}

// ───────────────── INI helper ───────────────────────────────────────
static std::string ini(const std::string& sec, const std::string& key,
                       const std::string& path="defaults.ini")
{
    std::ifstream f(path); if(!f) return "";
    std::regex      re_sec(R"(\[(.+?)\])");
    std::regex      re_kv (R"(^\s*([^=]+?)\s*=\s*(.*?)\s*(?:[#;].*)?$)");
    std::string line, cur; std::smatch m;
    while (std::getline(f,line)) {
        if (std::regex_match(line,m,re_sec))        cur = m[1];
        else if (cur==sec && std::regex_match(line,m,re_kv) && m[1]==key) return m[2];
    }
    return "";
}

// ───────────────── I/O helpers ──────────────────────────────────────
struct Frame { double ts; std::vector<Detection> dets; };

static std::vector<Frame> load_frames(const std::string& path)
{
    std::ifstream in(path); nlohmann::json j; in >> j;
    std::vector<Frame> v;
    for (auto& f : j) {
        Frame fr; fr.ts = parse_iso(f["timestamp"]);
        for (auto& d : f["detections"])
            fr.dets.push_back({d["x"], d["y"], d["w"], d["h"]});
        v.push_back(std::move(fr));
    }
    return v;
}

static void draw_vis(const std::string& dir,int idx,
                     const std::vector<Track>& trks,int W=800,int H=600)
{
    cv::Mat img(H,W,CV_8UC3, cv::Scalar(35,35,35));
    for (auto& t:trks) {
        int x=int(t.rect.at<double>(0)*W), y=int(t.rect.at<double>(1)*H);
        int w=int(t.rect.at<double>(2)*W), h=int(t.rect.at<double>(3)*H);
        cv::rectangle(img,{x,y,w,h}, {0,255,0},2);
        cv::putText(img,std::to_string(t.id),{x,y-5},
                    cv::FONT_HERSHEY_SIMPLEX,0.5,{0,255,255},1);
    }
    std::ostringstream fn; fn<<dir<<"/frame_"<<std::setw(4)<<std::setfill('0')<<idx<<".png";
    cv::imwrite(fn.str(), img);
}

// ────────────────────────────────────────────────────────────────────
int main(int argc,char** argv)
{
    // defaults from INI
    std::string in   = ini("tracker","input");
    std::string out  = ini("tracker","output");
    std::string vis  = ini("tracker","vis-dir");
    double max_dist  = ini("tracker","max-dist").empty()?0.15:std::stod(ini("tracker","max-dist"));
    int    max_age   = ini("tracker","max-age").empty()?5:std::stoi(ini("tracker","max-age"));
    double alpha     = ini("tracker","alpha").empty()?0.7:std::stod(ini("tracker","alpha"));

    CLI::App app{"tracking-solution"};
    app.add_option("--input",   in,  "input JSON");
    app.add_option("--output",  out, "output JSON");
    app.add_option("--vis-dir", vis, "visualisation directory");
    app.add_option("--max-dist",max_dist,"centre-distance threshold");
    app.add_option("--max-age", max_age,"frames to keep unmatched track");
    app.add_option("--alpha",   alpha,  "weight between IoU and distance");
    CLI11_PARSE(app,argc,argv);

    std::filesystem::create_directories(vis);

    // load & run
    auto frames = load_frames(in);
    Tracker tracker(max_dist,max_age,alpha);
    nlohmann::ordered_json dump = nlohmann::json::array();

    for (size_t i=0;i<frames.size();++i)
    {
        auto labels = tracker.step(frames[i].ts, frames[i].dets);

        // build output object with RAW rectangle
        nlohmann::ordered_json obj;
        obj["timestamp"] = format_iso(frames[i].ts);
        for (auto& L : labels) {
            obj["tracks"].push_back({
                {"id", L.track_id},
                {"x",  L.det.x},
                {"y",  L.det.y},
                {"w",  L.det.w},
                {"h",  L.det.h}
            });
        }
        dump.push_back(std::move(obj));

        draw_vis(vis, static_cast<int>(i), tracker.tracks());
    }

    std::ofstream(out) << std::setw(2) << dump;
    std::cout << "Tracking complete – " << frames.size() << " frames processed.\n";
    return 0;
}
