#pragma once
#include <opencv2/opencv.hpp>
#include <vector>

struct Detection
{
    double x, y, w, h;   // normalised
};

struct Track
{
    int             id          = -1;
    cv::KalmanFilter kf;
    cv::Mat         rect;       // [x y w h] from the *KF state*
    double          last_ts     = 0.0;
    int             age         = 0;
    int             time_since_update = 0;
};

struct Label            // <-- new: what we emit each frame
{
    int        track_id; // stable ID
    Detection  det;      // the raw detection that was matched
};

class Tracker
{
public:
    Tracker(double max_dist = 0.15,
            int    max_age  = 5,
            double alpha    = 0.7);

    /** Process one frame, return the labels that should be written. */
    std::vector<Label> step(double ts,
                            const std::vector<Detection>& dets);

    /** Access to internal tracks (for visualisation only). */
    const std::vector<Track>& tracks() const { return tracks_; }

private:
    // ─── helpers (implemented in Tracker.cpp) ────────────────────────
    static double centre_dist(const Detection& d, const Track& t);
    static double iou(const Track& t, const Detection& d);
    static cv::Mat det2meas(const Detection& d);
    static void    set_F(cv::KalmanFilter& kf, double dt);
    static void    set_Q(cv::KalmanFilter& kf, double dt, double s = 1e-2);
    static cv::KalmanFilter create_kf(const Detection& d);

    // ─── data ───────────────────────────────────────────────────────
    double max_dist_, alpha_;
    int    max_age_;
    int    next_id_;
    std::vector<Track>  tracks_;
    std::vector<Label>  labels_;      // reused every frame
};
