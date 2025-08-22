#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <memory>

// FFmpeg
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
}

// OpenCV
#include <opencv2/opencv.hpp>

// MediaPipe
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_buffer.h"
#include "mediapipe/gpu/gpu_shared_data_internal.h"
#include "absl/memory/memory.h"

// libxdo (마우스 제어)
#include <xdo.h>

// Python의 np.interp와 동일한 기능을 하는 함수
float linear_interp(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// 제스처 인식 및 마우스 이벤트 처리를 위한 네임스페이스
namespace GestureControl {
    const int BOUNDARY_REVISION = 170;
    const float SMOOTH_ALPHA = 0.2f;
    const int CAM_WIDTH = 640;
    const int CAM_HEIGHT = 480;

    std::vector<int> get_raised_fingers(const mediapipe::NormalizedLandmarkList& landmarks) {
        std::vector<int> fingers(5, 0);
        const int fingertip_indices[] = {4, 8, 12, 16, 20};
        if (landmarks.landmark_size() < 21) return fingers;

        if (landmarks.landmark(fingertip_indices[0]).x() > landmarks.landmark(fingertip_indices[0] - 1).x()) {
            fingers[0] = 1;
        }

        for (int i = 1; i < 5; ++i) {
            if (landmarks.landmark(fingertip_indices[i]).y() < landmarks.landmark(fingertip_indices[i] - 2).y()) {
                fingers[i] = 1;
            }
        }
        return fingers;
    }

    void handle_mouse_event(const std::vector<int>& fingers, const mediapipe::NormalizedLandmarkList& landmarks,
                              xdo_t* xdo, int scr_w, int scr_h,
                              float& prev_x, float& prev_y, bool& mouse_hold_state) {
        if (landmarks.landmark_size() < 9) return;
        
        float index_finger_x = landmarks.landmark(8).x() * CAM_WIDTH;
        float index_finger_y = landmarks.landmark(8).y() * CAM_HEIGHT;
        float curr_x = prev_x, curr_y = prev_y;

        bool is_drag_gesture = (fingers[0] == 0 && fingers[1] == 1 && fingers[2] == 1 && fingers[3] == 0 && fingers[4] == 0);

        if (is_drag_gesture) {
            if (!mouse_hold_state) {
                xdo_mouse_down(xdo, CURRENTWINDOW, 1);
                mouse_hold_state = true;
            }
            float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, scr_w);
            float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, scr_h);
            curr_x = prev_x + (new_x - prev_x) * SMOOTH_ALPHA;
            curr_y = prev_y + (new_y - prev_y) * SMOOTH_ALPHA;
            xdo_move_mouse(xdo, scr_w - curr_x, curr_y, 0);
        } else {
            if (mouse_hold_state) {
                xdo_mouse_up(xdo, CURRENTWINDOW, 1);
                mouse_hold_state = false;
            }
            
            if (fingers[0] == 1) {
                if (fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) {
                    xdo_click_window(xdo, CURRENTWINDOW, 1);
                } else if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 1) {
                    xdo_click_window(xdo, CURRENTWINDOW, 3);
                }
            } else {
                if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) {
                    float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, scr_w);
                    float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, scr_h);
                    curr_x = prev_x + (new_x - prev_x) * SMOOTH_ALPHA;
                    curr_y = prev_y + (new_y - prev_y) * SMOOTH_ALPHA;
                    xdo_move_mouse(xdo, scr_w - curr_x, curr_y, 0);
                } else if (fingers == std::vector<int>{0,0,0,0,0}) {
                    xdo_click_window(xdo, CURRENTWINDOW, 5);
                } else if (fingers[4] == 1 && fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0) {
                    xdo_click_window(xdo, CURRENTWINDOW, 4);
                }
            }
        }
        prev_x = curr_x;
        prev_y = curr_y;
    }
}

int main() {
    avdevice_register_all();
    const char* dev_name = "/dev/video0";
    const AVInputFormat* inputFormat = av_find_input_format("v4l2");
    AVFormatContext* fmt_ctx = nullptr;
    AVDictionary* options = nullptr;
    av_dict_set(&options, "video_size", "640x480", 0);
    av_dict_set(&options, "framerate", "30", 0);

    if (avformat_open_input(&fmt_ctx, dev_name, inputFormat, &options) != 0) {
        std::cerr << "❌ 웹캠 연결 실패!" << std::endl; return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "⚠️ 스트림 정보 읽기 실패!" << std::endl; return -1;
    }

    int video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
        std::cerr << "⛔ 비디오 스트림을 찾을 수 없습니다!" << std::endl; return -1;
    }
    AVCodecParameters* codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "⛔ 코덱 초기화 실패!" << std::endl; return -1;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* rgb_frame = av_frame_alloc();
    std::vector<uint8_t> buffer(av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1));
    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, buffer.data(), AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
    
    SwsContext* sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                                     codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);

    // MediaPipe 그래프 설정: HandLandmarkTrackingGpu 사용
    const std::string graph_config_str = R"pb(
        input_stream: "input_video"
        output_stream: "landmarks"
        node {
            calculator: "HandLandmarkTrackingGpu"
            input_stream: "IMAGE:input_video"
            output_stream: "LANDMARKS:landmarks"
        }
    )pb";
    mediapipe::CalculatorGraphConfig config;
    if (!google::protobuf::TextFormat::ParseFromString(graph_config_str, &config)) {
        std::cerr << "⛔ MediaPipe 그래프 설정 파싱 실패!" << std::endl; return -1;
    }

    mediapipe::CalculatorGraph graph;
    if (!graph.Initialize(config).ok()) {
         std::cerr << "⛔ MediaPipe 그래프 초기화 실패!" << std::endl; return -1;
    }

    auto gpu_resources_status = mediapipe::GpuResources::Create();
    if (!gpu_resources_status.ok()) {
        std::cerr << "⛔ MediaPipe GPU 리소스 생성 실패!" << std::endl; return -1;
    }
    if(!graph.SetGpuResources(gpu_resources_status.value()).ok()){
        std::cerr << "⛔ MediaPipe GPU 리소스 설정 실패!" << std::endl; return -1;
    }

    auto poller_status = graph.AddOutputStreamPoller("landmarks");
    if (!poller_status.ok()) {
        std::cerr << "⛔ MediaPipe 출력 스트림 폴러 추가 실패!" << std::endl; return -1;
    }
    mediapipe::OutputStreamPoller poller = std::move(poller_status.value());
    if(!graph.StartRun({}).ok()){
        std::cerr << "⛔ MediaPipe 그래프 실행 실패!" << std::endl; return -1;
    }

    xdo_t* xdo = xdo_new(NULL);
    unsigned int screen_width, screen_height; // int -> unsigned int로 변경
    int screen_num = 0; // Use int for screen_num as per function signature
    xdo_get_viewport_dimensions(xdo, &screen_width, &screen_height, screen_num);
    

    float prev_x = 0.0f, prev_y = 0.0f;
    bool mouse_hold_state = false;
    auto prev_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "🎬 가상 터치 시작... (q 키 또는 Ctrl+C로 종료)" << std::endl;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, rgb_frame->data, rgb_frame->linesize);
                    cv::Mat input_mat(codec_ctx->height, codec_ctx->width, CV_8UC3, rgb_frame->data[0], rgb_frame->linesize[0]);
                    cv::Mat flipped_mat;
                    cv::flip(input_mat, flipped_mat, 1);

                    auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
                        mediapipe::ImageFormat::SRGB, flipped_mat.cols, flipped_mat.rows,
                        mediapipe::ImageFrame::kDefaultAlignmentBoundary);
                    cv::Mat input_frame_mat = mediapipe::formats::MatView(input_frame.get());
                    flipped_mat.copyTo(input_frame_mat);
                    
                    size_t frame_timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                    
                    if (!graph.AddPacketToInputStream("input_video", mediapipe::Adopt(input_frame.release()).At(mediapipe::Timestamp(frame_timestamp_us))).ok()) {
                        std::cerr << "⛔ 입력 스트림에 패킷 추가 실패!" << std::endl;
                        break;
                    }

                    mediapipe::Packet packet;
                    if (poller.Next(&packet)) {
                        auto& output_landmarks_vec = packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
                        if (!output_landmarks_vec.empty()) {
                            const auto& hand_landmarks = output_landmarks_vec[0];
                            std::vector<int> fingers = GestureControl::get_raised_fingers(hand_landmarks);
                            GestureControl::handle_mouse_event(fingers, hand_landmarks, xdo, screen_width, screen_height, prev_x, prev_y, mouse_hold_state);

                            for(int i=0; i<hand_landmarks.landmark_size(); ++i){
                                const auto& landmark = hand_landmarks.landmark(i);
                                cv::circle(flipped_mat, cv::Point(landmark.x() * flipped_mat.cols, landmark.y() * flipped_mat.rows), 5, cv::Scalar(255,0,255), cv::FILLED);
                            }
                        }
                    }

                    auto curr_time = std::chrono::high_resolution_clock::now();
                    double fps = 1.0 / std::chrono::duration_cast<std::chrono::duration<double>>(curr_time - prev_time).count();
                    prev_time = curr_time;
                    cv::putText(flipped_mat, std::to_string(static_cast<int>(fps)), cv::Point(20, 50), cv::FONT_HERSHEY_PLAIN, 3, cv::Scalar(0, 255, 0), 3);

                    cv::imshow("Virtual Touch C++", flipped_mat);
                    if (cv::waitKey(1) == 'q') goto end;
                }
            }
        }
        av_packet_unref(pkt);
    }

end:
    std::cout << "🛑 프로그램 종료" << std::endl;
    graph.CloseInputStream("input_video");
    graph.WaitUntilDone();
    xdo_free(xdo);
    av_frame_free(&rgb_frame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    cv::destroyAllWindows();
    return 0;
}