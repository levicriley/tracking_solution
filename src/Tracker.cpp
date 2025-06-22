#include "Tracker.hpp"
#include "hungarian.hpp"          // minimal Hungarian solver
#include <algorithm>
#include <cmath>
#include <limits>

using namespace std;

// ───────────────── utility helpers ──────────────────────────────────
double Tracker::centre_dist(const Detection& d, const Track& t)
{
    const double cx_d = d.x + d.w * 0.5,
                 cy_d = d.y + d.h * 0.5;

    const double cx_t = t.rect.at<double>(0) + t.rect.at<double>(2) * 0.5,
                 cy_t = t.rect.at<double>(1) + t.rect.at<double>(3) * 0.5;

    return std::hypot(cx_d - cx_t, cy_d - cy_t);
}

double Tracker::iou(const Track& t, const Detection& d)
{
    const double ax=t.rect.at<double>(0), ay=t.rect.at<double>(1),
                 aw=t.rect.at<double>(2), ah=t.rect.at<double>(3);
    const double bx=d.x, by=d.y, bw=d.w, bh=d.h;

    const double x1 = std::max(ax,bx),
                 y1 = std::max(ay,by),
                 x2 = std::min(ax+aw, bx+bw),
                 y2 = std::min(ay+ah, by+bh);

    const double inter = std::max(0.0,x2-x1) * std::max(0.0,y2-y1);
    const double uni   = aw*ah + bw*bh - inter;

    return (uni>0.0 ? inter/uni : 0.0);
}

cv::Mat Tracker::det2meas(const Detection& d)
{
    cv::Mat m(4,1,CV_64F);
    m.at<double>(0)=d.x; m.at<double>(1)=d.y;
    m.at<double>(2)=d.w; m.at<double>(3)=d.h;
    return m;
}

void Tracker::set_F(cv::KalmanFilter& kf,double dt)
{
    auto& F = kf.transitionMatrix;
    F.at<double>(0,2)=dt;
    F.at<double>(1,3)=dt;
    F.at<double>(4,6)=dt;
    F.at<double>(5,7)=dt;
}

void Tracker::set_Q(cv::KalmanFilter& kf,double dt,double s)
{
    const double dt2=dt*dt, dt3=dt2*dt, dt4=dt2*dt2;
    cv::Mat Q = cv::Mat::zeros(8,8,CV_64F);

    // pos-vel (x,y)
    Q.at<double>(0,0)=Q.at<double>(1,1)=dt4/4*s;
    Q.at<double>(0,2)=Q.at<double>(1,3)=dt3/2*s;
    Q.at<double>(2,0)=Q.at<double>(3,1)=dt3/2*s;
    Q.at<double>(2,2)=Q.at<double>(3,3)=dt2*s;

    // size-vel (w,h)
    Q.at<double>(4,4)=Q.at<double>(5,5)=dt4/4*s;
    Q.at<double>(4,6)=Q.at<double>(5,7)=dt3/2*s;
    Q.at<double>(6,4)=Q.at<double>(7,5)=dt3/2*s;
    Q.at<double>(6,6)=Q.at<double>(7,7)=dt2*s;

    kf.processNoiseCov = Q;
}

cv::KalmanFilter Tracker::create_kf(const Detection& d)
{
    cv::KalmanFilter kf(8,4,0,CV_64F);

    // F
    kf.transitionMatrix = (cv::Mat_<double>(8,8) <<
         1,0,1,0,0,0,0,0,
         0,1,0,1,0,0,0,0,
         0,0,1,0,0,0,0,0,
         0,0,0,1,0,0,0,0,
         0,0,0,0,1,0,1,0,
         0,0,0,0,0,1,0,1,
         0,0,0,0,0,0,1,0,
         0,0,0,0,0,0,0,1);

    // H
    kf.measurementMatrix = cv::Mat::zeros(4,8,CV_64F);
    kf.measurementMatrix.at<double>(0,0)=1;
    kf.measurementMatrix.at<double>(1,1)=1;
    kf.measurementMatrix.at<double>(2,4)=1;
    kf.measurementMatrix.at<double>(3,5)=1;

    // noise / cov
    kf.measurementNoiseCov = cv::Mat::eye(4,4,CV_64F) * 1e-2;
    kf.errorCovPost        = cv::Mat::eye(8,8,CV_64F);

    kf.statePost = (cv::Mat_<double>(8,1) << d.x,d.y,0,0,d.w,d.h,0,0);
    set_Q(kf,1.0);

    return kf;
}

// ───────────────── constructor ──────────────────────────────────────
Tracker::Tracker(double md,int ma,double a)
    : max_dist_(md), alpha_(a), max_age_(ma), next_id_(0) {}

// ───────────────── main step ────────────────────────────────────────
std::vector<Label> Tracker::step(double ts,const vector<Detection>& dets)
{
    // ─── 1. predict ────────────────────────────────────────────────
    for (auto& tr : tracks_)
    {
        double dt = ts - tr.last_ts;
        if (dt<=0) dt = 1e-6;

        set_F(tr.kf,dt);
        set_Q(tr.kf,dt);
        cv::Mat p = tr.kf.predict();

        tr.rect = (cv::Mat_<double>(4,1) <<
                    p.at<double>(0),
                    p.at<double>(1),
                    p.at<double>(4),
                    p.at<double>(5));
        tr.age++;
        tr.time_since_update++;
    }

    // ─── 2. build cost matrix ──────────────────────────────────────
    const int nT = static_cast<int>(tracks_.size());
    const int nD = static_cast<int>(dets.size());
    const int N  = std::max(nT,nD);
    const double BIG = 1e9;

    vector<vector<double>> C(N, vector<double>(N,BIG));

    for (int ti=0; ti<nT; ++ti)
        for (int di=0; di<nD; ++di)
        {
            double dist = centre_dist(dets[di], tracks_[ti]);
            if (dist > max_dist_) continue;

            double j = iou(tracks_[ti], dets[di]);
            if (j < 0.01)         continue;

            C[ti][di] = alpha_*(1.0-j) + (1.0-alpha_)*dist;
        }

    // pad zeros for dummy rows/cols
    for (int i=nT;i<N;++i) std::fill(C[i].begin(), C[i].end(), 0.0);
    for (int i=0;i<N;++i) for (int j=nD;j<N;++j) C[i][j]=0.0;

    // ─── 3. Hungarian assign ───────────────────────────────────────
    vector<int> assign; double tot = 0.0;
    hungarian(C,assign,tot);

    vector<int> tr2det(nT,-1), det2tr(nD,-1);
    for (int ti=0; ti<nT; ++ti){
        int di = assign[ti];
        if (di>=0 && di<nD && C[ti][di] < BIG){
            tr2det[ti]=di; det2tr[di]=ti;
        }
    }

    // ─── 4. update matched ─────────────────────────────────────────
    for (int ti=0; ti<nT; ++ti){
        int di = tr2det[ti];
        if (di!=-1){
            cv::Mat corr = tracks_[ti].kf.correct(det2meas(dets[di]));
            tracks_[ti].rect = (cv::Mat_<double>(4,1) <<
                                 corr.at<double>(0),
                                 corr.at<double>(1),
                                 corr.at<double>(4),
                                 corr.at<double>(5));
            tracks_[ti].last_ts = ts;
            tracks_[ti].time_since_update = 0;
        }
    }

    // ─── 5. add new tracks for unmatched detections ────────────────
    for (int di=0; di<nD; ++di) if (det2tr[di]==-1)
    {
        Track tr;
        tr.id    = next_id_++;
        tr.kf    = create_kf(dets[di]);
        tr.rect  = det2meas(dets[di]).clone();
        tr.last_ts = ts;
        tracks_.push_back(std::move(tr));

        det2tr[di] = static_cast<int>(tracks_.size()) - 1; // index of new track
    }

    // ─── 6. prepare labels (RAW rectangles) ────────────────────────
    labels_.clear();
    for (int di=0; di<nD; ++di)
        if (det2tr[di] != -1)         // actually associated
            labels_.push_back( { tracks_[det2tr[di]].id, dets[di] } );

    // ─── 7. cull stale tracks ──────────────────────────────────────
    tracks_.erase( remove_if(tracks_.begin(), tracks_.end(),
                  [&](const Track& t){ return t.time_since_update > max_age_; }),
                  tracks_.end() );

    return labels_;
}
