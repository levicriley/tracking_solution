#include "Tracker.hpp"
#include "hungarian.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace std;

// -------- utility functions --------------

double Tracker::centre_dist(const Detection& d, const Track& t)
{
    double cx_d = d.x + d.w * 0.5;
    double cy_d = d.y + d.h * 0.5;
    double cx_t = t.rect.at<double>(0) + t.rect.at<double>(2) * 0.5;
    double cy_t = t.rect.at<double>(1) + t.rect.at<double>(3) * 0.5;
    return hypot(cx_d - cx_t, cy_d - cy_t);
}

double Tracker::iou(const Track& t, const Detection& d)
{
    double ax = t.rect.at<double>(0), ay = t.rect.at<double>(1);
    double aw = t.rect.at<double>(2), ah = t.rect.at<double>(3);
    double bx = d.x, by = d.y, bw = d.w, bh = d.h;

    double x1 = max(ax, bx), y1 = max(ay, by);
    double x2 = min(ax + aw, bx + bw), y2 = min(ay + ah, by + bh);
    double inter = max(0.0, x2 - x1) * max(0.0, y2 - y1);
    double uni = aw * ah + bw * bh - inter;
    return uni > 0 ? inter / uni : 0.0;
}

// Converts detection to measurement vector
cv::Mat Tracker::det2meas(const Detection& d)
{
    cv::Mat m(4, 1, CV_64F);
    m.at<double>(0) = d.x;
    m.at<double>(1) = d.y;
    m.at<double>(2) = d.w;
    m.at<double>(3) = d.h;
    return m;
}

// Sets F matrix to account for dt
void Tracker::set_F(cv::KalmanFilter& kf, double dt)
{
    auto& F = kf.transitionMatrix;
    F.at<double>(0, 2) = dt;
    F.at<double>(1, 3) = dt;
    F.at<double>(4, 6) = dt;
    F.at<double>(5, 7) = dt;
}

// Sets process noise Q with appropriate dynamics
void Tracker::set_Q(cv::KalmanFilter& kf, double dt, double s)
{
    double dt2 = dt * dt, dt3 = dt2 * dt, dt4 = dt2 * dt2;
    cv::Mat Q = cv::Mat::zeros(8, 8, CV_64F);

    // Position (x, y) and velocity (dx, dy)
    Q.at<double>(0, 0) = Q.at<double>(1, 1) = dt4 / 4 * s;
    Q.at<double>(0, 2) = Q.at<double>(1, 3) = dt3 / 2 * s;
    Q.at<double>(2, 0) = Q.at<double>(3, 1) = dt3 / 2 * s;
    Q.at<double>(2, 2) = Q.at<double>(3, 3) = dt2 * s;

    // Width (w, h) and velocity (dw, dh)
    Q.at<double>(4, 4) = Q.at<double>(5, 5) = dt4 / 4 * s;
    Q.at<double>(4, 6) = Q.at<double>(5, 7) = dt3 / 2 * s;
    Q.at<double>(6, 4) = Q.at<double>(7, 5) = dt3 / 2 * s;
    Q.at<double>(6, 6) = Q.at<double>(7, 7) = dt2 * s;

    kf.processNoiseCov = Q;
}

// Creates Kalman filter with 8D state: [x, y, dx, dy, w, h, dw, dh]
cv::KalmanFilter Tracker::create_kf(const Detection& d)
{
    cv::KalmanFilter kf(8, 4, 0, CV_64F);

    // Transition matrix (F)
    kf.transitionMatrix = (cv::Mat_<double>(8, 8) <<
        1, 0, 1, 0, 0, 0, 0, 0,
        0, 1, 0, 1, 0, 0, 0, 0,
        0, 0, 1, 0, 0, 0, 0, 0,
        0, 0, 0, 1, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 1, 0,
        0, 0, 0, 0, 0, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 1);

    // Measurement matrix (H)
    kf.measurementMatrix = cv::Mat::zeros(4, 8, CV_64F);
    kf.measurementMatrix.at<double>(0, 0) = 1; // x
    kf.measurementMatrix.at<double>(1, 1) = 1; // y
    kf.measurementMatrix.at<double>(2, 4) = 1; // w
    kf.measurementMatrix.at<double>(3, 5) = 1; // h

    // Initial state
    kf.statePost = (cv::Mat_<double>(8, 1) << d.x, d.y, 0, 0, d.w, d.h, 0, 0);

    // Covariances
    kf.errorCovPost = cv::Mat::eye(8, 8, CV_64F);
    kf.measurementNoiseCov = cv::Mat::eye(4, 4, CV_64F) * 1e-2;
    set_Q(kf, 1.0); // Initial Q with dt=1

    return kf;
}

// -------- Tracker implementation ------------

Tracker::Tracker(double max_dist, int max_age, double alpha)
    : max_dist_(max_dist), alpha_(alpha), max_age_(max_age), next_id_(0) {}

void Tracker::step(double ts, const vector<Detection>& dets)
{
    // Predict all tracks forward in time
    for (auto& tr : tracks_) {
        double dt = ts - tr.last_ts;
        if (dt <= 0) dt = 1e-6;
        set_F(tr.kf, dt);
        set_Q(tr.kf, dt);
        cv::Mat pred = tr.kf.predict();
        tr.rect = (cv::Mat_<double>(4, 1) <<
            pred.at<double>(0),  // x
            pred.at<double>(1),  // y
            pred.at<double>(4),  // w
            pred.at<double>(5)); // h
        tr.age++;
        tr.time_since_update++;
    }

    // Hungarian matching
    int nT = tracks_.size(), nD = dets.size(), N = max(nT, nD);
    const double BIG = 1e6;
    vector<vector<double>> cost(N, vector<double>(N, BIG));

    for (int ti = 0; ti < nT; ++ti) {
        for (int di = 0; di < nD; ++di) {
            double dist = centre_dist(dets[di], tracks_[ti]);
            if (dist > max_dist_) continue;
            double i = iou(tracks_[ti], dets[di]);
            if (i < 0.01) continue;
            cost[ti][di] = alpha_ * (1.0 - i) + (1.0 - alpha_) * dist;
        }
    }
    for (int i = nT; i < N; ++i) fill(cost[i].begin(), cost[i].end(), 0);
    for (int i = 0; i < N; ++i) for (int j = nD; j < N; ++j) cost[i][j] = 0;

    vector<int> assign; double tot;
    hungarian(cost, assign, tot);

    vector<int> tr2det(nT, -1), det2tr(nD, -1);
    for (int i = 0; i < nT; ++i) {
        int j = assign[i];
        if (j >= 0 && j < nD && cost[i][j] < BIG) {
            tr2det[i] = j;
            det2tr[j] = i;
        }
    }

    // Update matched tracks
    for (int ti = 0; ti < nT; ++ti) {
        int di = tr2det[ti];
        if (di != -1) {
            cv::Mat corr = tracks_[ti].kf.correct(det2meas(dets[di]));
            tracks_[ti].rect = (cv::Mat_<double>(4, 1) <<
                corr.at<double>(0),  // x
                corr.at<double>(1),  // y
                corr.at<double>(4),  // w
                corr.at<double>(5)); // h
            tracks_[ti].last_ts = ts;
            tracks_[ti].time_since_update = 0;
        }
    }

    // Add unmatched detections as new tracks
    for (int di = 0; di < nD; ++di) {
        if (det2tr[di] == -1) {
            Track tr;
            tr.id = next_id_++;
            tr.kf = create_kf(dets[di]);
            tr.rect = det2meas(dets[di]).clone();
            tr.last_ts = ts;
            tracks_.push_back(std::move(tr));
        }
    }

    // Remove stale tracks
    tracks_.erase(remove_if(tracks_.begin(), tracks_.end(),
        [&](const Track& t) { return t.time_since_update > max_age_; }),
        tracks_.end());
}
