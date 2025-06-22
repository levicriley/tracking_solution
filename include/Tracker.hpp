#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

struct Detection { double x{}, y{}, w{}, h{}; };

struct Track {
    int id{};
    int age{0};
    int time_since_update{0};
    double last_ts{0.0};
    cv::KalmanFilter kf;
    cv::Mat rect;    // 4×1 [x y w h]
};

class Tracker {
public:
    Tracker(double max_dist=0.15, int max_age=5, double alpha=0.7);

    void step(double ts, const std::vector<Detection>& dets);
    const std::vector<Track>& tracks() const { return tracks_; }

private:
    double max_dist_, alpha_;
    int max_age_;
    int next_id_;
    std::vector<Track> tracks_;

    static cv::KalmanFilter create_kf(const Detection& d);
    static void set_F(cv::KalmanFilter& kf, double dt);
    static void set_Q(cv::KalmanFilter& kf, double dt, double sigma_acc=1e-2);
    static cv::Mat det2meas(const Detection& d);
    static double centre_dist(const Detection& d, const Track& t);
    static double iou(const Track& t, const Detection& d);
};
