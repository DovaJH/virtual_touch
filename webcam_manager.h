#pragma once
#include <opencv2/opencv.hpp>
#include <string>

// FFmpeg 헤더 전방 선언
struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;
struct AVFrame;
struct AVPacket;
struct AVInputFormat;

class WebcamManager {
public:
    WebcamManager(int width, int height, int fps);
    ~WebcamManager();

    bool initialize();
    bool get_next_frame(cv::Mat& frame);

    int get_width() const { return width_; }
    int get_height() const { return height_; }

private:
    int width_;
    int height_;
    int fps_;
    
    AVFormatContext* fmt_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    SwsContext* sws_ctx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVFrame* rgb_frame_ = nullptr;
    AVPacket* pkt_ = nullptr;
    int video_stream_index_ = -1;
    std::vector<uint8_t> buffer_;
};