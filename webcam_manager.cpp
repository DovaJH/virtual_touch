#include "webcam_manager.h"
#include <iostream>
#include <chrono> // chrono 라이브러리 포함

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
}

WebcamManager::WebcamManager(int width, int height, int fps)
    : width_(width), height_(height), fps_(fps) {}

WebcamManager::~WebcamManager() {
    av_frame_free(&rgb_frame_);
    av_frame_free(&frame_);
    av_packet_free(&pkt_);
    sws_freeContext(sws_ctx_);
    avcodec_free_context(&codec_ctx_);
    avformat_close_input(&fmt_ctx_);
}

bool WebcamManager::initialize() {
    avdevice_register_all();
    const char* dev_name = "/dev/video0";
    const AVInputFormat* inputFormat = av_find_input_format("v4l2");
    
    AVDictionary* options = nullptr;
    std::string video_size = std::to_string(width_) + "x" + std::to_string(height_);
    av_dict_set(&options, "video_size", video_size.c_str(), 0);
    av_dict_set(&options, "framerate", std::to_string(fps_).c_str(), 0);

    if (avformat_open_input(&fmt_ctx_, dev_name, inputFormat, &options) != 0) {
        std::cerr << "❌ 웹캠 연결 실패!" << std::endl; return false;
    }
    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        std::cerr << "⚠️ 스트림 정보 읽기 실패!" << std::endl; return false;
    }

    video_stream_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index_ < 0) {
        std::cerr << "⛔ 비디오 스트림을 찾을 수 없습니다!" << std::endl; return false;
    }

    AVCodecParameters* codecpar = fmt_ctx_->streams[video_stream_index_]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    codec_ctx_ = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx_, codecpar);
    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        std::cerr << "⛔ 코덱 초기화 실패!" << std::endl; return false;
    }

    pkt_ = av_packet_alloc();
    frame_ = av_frame_alloc();
    rgb_frame_ = av_frame_alloc();
    
    buffer_.resize(av_image_get_buffer_size(AV_PIX_FMT_RGB24, width_, height_, 1));
    av_image_fill_arrays(rgb_frame_->data, rgb_frame_->linesize, buffer_.data(), AV_PIX_FMT_RGB24, width_, height_, 1);
    
    sws_ctx_ = sws_getContext(width_, height_, codec_ctx_->pix_fmt, width_, height_, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    return true;
}

bool WebcamManager::get_next_frame(cv::Mat& out_frame) {
    if (av_read_frame(fmt_ctx_, pkt_) >= 0) { 

        if (pkt_->stream_index == video_stream_index_) {
            if (avcodec_send_packet(codec_ctx_, pkt_) == 0) {
                if (avcodec_receive_frame(codec_ctx_, frame_) == 0) {

                    sws_scale(sws_ctx_, frame_->data, frame_->linesize, 0, height_, rgb_frame_->data, rgb_frame_->linesize);

                    cv::Mat rgb_mat(height_, width_, CV_8UC3, rgb_frame_->data[0], rgb_frame_->linesize[0]);
                    rgb_mat.copyTo(out_frame);                   
                   
                    av_packet_unref(pkt_);
                   
                    return true;
                }
            }
        }
        av_packet_unref(pkt_);
    }
    return false;
}