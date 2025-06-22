#include "Tracker.hpp"
#include <nlohmann/json.hpp>
#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

// Parse ISO timestamp string to seconds-since-epoch (double)
static double parse_iso(const std::string& s)
{
    std::tm tm{}; double frac=0.0; char dot;
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if(ss.peek()=='.'){ ss>>dot; std::string micros; ss>>micros; frac=std::stod("0."+micros); }
    std::time_t t = timegm(&tm);
    return double(t)+frac;
}
static std::string format_iso(double sec)
{
    std::time_t t_int = static_cast<std::time_t>(sec);
    double frac = sec - t_int;
    std::tm *tm = gmtime(&t_int);
    char buf[32];
    strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S",tm);
    std::ostringstream out;
    out<<buf<<'.'<<std::setw(6)<<std::setfill('0')<<int(frac*1e6+0.5);
    return out.str();
}

struct Frame{double ts; std::vector<Detection> dets;};

static std::vector<Frame> load_frames(const std::string& path)
{
    std::ifstream in(path);
    nlohmann::json j; in>>j;
    std::vector<Frame> frames;
    for(auto& f:j){
        Frame fr; fr.ts = parse_iso(f.at("timestamp"));
        for(auto& d:f.at("detections")){
            fr.dets.push_back({d.at("x"),d.at("y"),d.at("w"),d.at("h")});
        }
        frames.push_back(std::move(fr));
    }
    return frames;
}

static void save_tracks(const std::string& path,
                        const std::vector<std::pair<double,std::vector<Track>>>& dump)
{
    nlohmann::json j=nlohmann::json::array();
    for(auto& [ts,trks]:dump){
        nlohmann::json o; o["timestamp"]=format_iso(ts);
        for(auto& t:trks){
            o["tracks"].push_back({{"id",t.id},
                                   {"x",t.rect.at<double>(0)},
                                   {"y",t.rect.at<double>(1)},
                                   {"w",t.rect.at<double>(2)},
                                   {"h",t.rect.at<double>(3)}});
        }
        j.push_back(o);
    }
    std::ofstream(path) << std::setw(2) << j;
}

static void draw_vis(const std::string& dir,int idx,const std::vector<Track>& trks,
                     int W=800,int H=600)
{
    cv::Mat img(H, W, CV_8UC3, cv::Scalar(30,30,30));
    for(const auto& t:trks){
        int x = int(t.rect.at<double>(0)*W);
        int y = int(t.rect.at<double>(1)*H);
        int w = int(t.rect.at<double>(2)*W);
        int h = int(t.rect.at<double>(3)*H);
        cv::rectangle(img,{x,y,w,h},cv::Scalar(0,255,0),2);
        cv::putText(img,std::to_string(t.id),{x,y-5},
            cv::FONT_HERSHEY_SIMPLEX,0.5,cv::Scalar(0,255,255),1);
    }
    std::ostringstream fn; fn<<dir<<"/frame_"<<std::setw(4)<<std::setfill('0')<<idx<<".png";
    cv::imwrite(fn.str(),img);
}

int main(int argc,char** argv)
{
    CLI::App app{"Tracking solution"};
    std::string in_path, out_path, vis_dir;
    double max_dist=0.15, alpha=0.7; int max_age=5;
    app.add_option("--input",in_path)->required();
    app.add_option("--output",out_path)->required();
    app.add_option("--vis-dir",vis_dir)->required();
    app.add_option("--max-dist",max_dist);
    app.add_option("--max-age",max_age);
    app.add_option("--alpha",alpha);
    CLI11_PARSE(app,argc,argv);

    std::vector<Frame> frames = load_frames(in_path);
    Tracker tracker(max_dist,max_age,alpha);
    std::vector<std::pair<double,std::vector<Track>>> dump;
    std::filesystem::create_directories(vis_dir);

    for(size_t i=0;i<frames.size();++i){
        tracker.step(frames[i].ts, frames[i].dets);
        dump.emplace_back(frames[i].ts, tracker.tracks());
        draw_vis(vis_dir,i,tracker.tracks());
    }
    save_tracks(out_path,dump);
    std::cout<<"Tracking complete. Frames: "<<frames.size()<<"\n";
    return 0;
}
