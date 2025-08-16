#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

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

// libxdo (마우스 제어)
#include <xdo.h>

// Python의 np.interp와 동일한 기능을 하는 함수
float linear_interp(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// 제스처 인식 및 마우스 이벤트 처리를 위한 네임스페이스
namespace GestureControl {
    // 설정값 (Python의 GestureRecognizer 클래스 변수와 동일)
    const int BOUNDARY_REVISION = 170;
    const float SMOOTH_ALPHA = 0.2f;
    const int SCROLL_DOWN_SPEED = -70;
    const int SCROLL_UP_SPEED = 70;
    const int CAM_WIDTH = 640;
    const int CAM_HEIGHT = 480;

    // 손가락이 펴졌는지 확인하는 함수 (is_fingers_raised)
    std::vector<int> get_raised_fingers(const mediapipe::NormalizedLandmarkList& landmarks) {
        std::vector<int> fingers(5, 0);
        const int fingertip_indices[] = {4, 8, 12, 16, 20};

        // 1. 엄지 (x 좌표 비교)
        if (landmarks.landmark(fingertip_indices[0]).x() > landmarks.landmark(fingertip_indices[0] - 1).x()) {
            fingers[0] = 1;
        }

        // 2. 나머지 네 손가락 (y 좌표 비교)
        for (int i = 1; i < 5; ++i) {
            if (landmarks.landmark(fingertip_indices[i]).y() < landmarks.landmark(fingertip_indices[i] - 2).y()) {
                fingers[i] = 1;
            }
        }
        return fingers;
    }

    // 마우스 이벤트 처리 함수 (mouse_event)
    void handle_mouse_event(const std::vector<int>& fingers, const mediapipe::NormalizedLandmarkList& landmarks,
                              xdo_t* xdo, int scr_w, int scr_h,
                              float& prev_x, float& prev_y, bool& mouse_hold_state) {
        
        float index_finger_x = landmarks.landmark(8).x() * CAM_WIDTH;
        float index_finger_y = landmarks.landmark(8).y() * CAM_HEIGHT;
        float curr_x = prev_x, curr_y = prev_y;

        bool is_drag_gesture = (fingers[0] == 0 && fingers[1] == 1 && fingers[2] == 1 && fingers[3] == 0 && fingers[4] == 0);

        // 드래그 상태 처리
        if (is_drag_gesture) {
            if (!mouse_hold_state) {
                xdo_mouse_down(xdo, CURRENTWINDOW, 1); // 왼쪽 버튼 누르기
                mouse_hold_state = true;
            }
             // 마우스 이동 로직 (아래 기본 이동 로직과 동일)
            float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, scr_w);
            float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, scr_h);
            curr_x = prev_x + (new_x - prev_x) * SMOOTH_ALPHA;
            curr_y = prev_y + (new_y - prev_y) * SMOOTH_ALPHA;
            xdo_move_mouse(xdo, scr_w - curr_x, curr_y, 0);
        } else {
            if (mouse_hold_state) {
                xdo_mouse_up(xdo, CURRENTWINDOW, 1); // 왼쪽 버튼 떼기
                mouse_hold_state = false;
            }
            
            // 일반 제스처 처리
            if (fingers[0] == 1) { // 엄지를 편 상태
                if (fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) {
                    xdo_click_window(xdo, CURRENTWINDOW, 1); // 좌클릭
                } else if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 1) {
                    xdo_click_window(xdo, CURRENTWINDOW, 3); // 우클릭
                }
            } else { // 엄지를 접은 상태
                if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) { // 검지만 폄: 마우스 이동
                    float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, scr_w);
                    float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, scr_h);
                    
                    curr_x = prev_x + (new_x - prev_x) * SMOOTH_ALPHA;
                    curr_y = prev_y + (new_y - prev_y) * SMOOTH_ALPHA;
                    
                    xdo_move_mouse(xdo, scr_w - curr_x, curr_y, 0); // Python 코드의 scr_w - curr_x 반영
                } else if (fingers == std::vector<int>{0,0,0,0,0}) { // 주먹: 스크롤 다운
                    xdo_click_window(xdo, CURRENTWINDOW, 5); // 5번 버튼이 스크롤 다운
                } else if (fingers[4] == 1 && fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0) { // 소지만 폄: 스크롤 업
                    xdo_click_window(xdo, CURRENTWINDOW, 4); // 4번 버튼이 스크롤 업
                }
            }
        }
        prev_x = curr_x;
        prev_y = curr_y;
    }
}


int main() {
    // 1. FFmpeg 초기화 (linux_input.cpp 기반)
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

    // 2. MediaPipe 그래프 초기화
    mediapipe::CalculatorGraphConfig config;
    config.ParseFromString(R"pb(
        input_stream: "input_video"
        output_stream: "output_landmarks"

        # GPU 가속을 위한 설정
        node {
            calculator: "FlowLimiterCalculator"
            input_stream: "input_video"
            input_stream: "FINISHED:output_landmarks"
            input_stream_info: { tag_index: "FINISHED" back_edge: true }
            output_stream: "throttled_input_video"
        }

        node {
            calculator: "HandLandmarkTrackingGpu"
            input_stream: "IMAGE:throttled_input_video"
            output_stream: "LANDMARKS:output_landmarks"
            options: {
                [mediapipe.HandLandmarkTrackingGpuOptions.ext] {
                    max_num_hands: 1
                }
            }
        }
    )pb");

    mediapipe::CalculatorGraph graph;
    graph.Initialize(config);

    // GPU 리소스 설정
    auto gpu_resources = mediapipe::GpuResources::Create().ValueOrDie();
    graph.SetGpuResources(std::move(gpu_resources));
    mediapipe::GlCalculatorHelper gpu_helper;
    gpu_helper.InitializeForTest(graph.GetGpuResources().get());

    // 입출력 스트림 폴러 추가
    auto poller = graph.AddOutputStreamPoller("output_landmarks").ValueOrDie();
    graph.StartRun({});

    // 3. 마우스 제어 및 변수 초기화
    xdo_t* xdo = xdo_new(NULL);
    int screen_width, screen_height;
    xdo_get_desktop_dimensions(xdo, &screen_width, &screen_height, 0);

    float prev_x = 0.0f, prev_y = 0.0f;
    bool mouse_hold_state = false;
    auto prev_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "🎬 가상 터치 시작... (q 키 또는 Ctrl+C로 종료)" << std::endl;

    // 4. 메인 루프
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    // FFmpeg 프레임을 OpenCV Mat으로 변환 (RGB)
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, rgb_frame->data, rgb_frame->linesize);
                    cv::Mat input_mat(codec_ctx->height, codec_ctx->width, CV_8UC3, rgb_frame->data[0], rgb_frame->linesize[0]);
                    cv::Mat flipped_mat;
                    cv::flip(input_mat, flipped_mat, 1); // 좌우 반전

                    // MediaPipe에 프레임 전송
                    auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
                        mediapipe::ImageFormat::SRGB, flipped_mat.cols, flipped_mat.rows,
                        mediapipe::ImageFrame::kGlDefaultAlignmentBoundary);
                    cv::Mat input_frame_mat = mediapipe::formats::MatView(input_frame.get());
                    flipped_mat.copyTo(input_frame_mat);
                    
                    size_t frame_timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                    
                    graph.AddPacketToInputStream("input_video", mediapipe::Adopt(input_frame.release()).At(mediapipe::Timestamp(frame_timestamp_us))).Ok();

                    // MediaPipe에서 결과 수신
                    mediapipe::Packet packet;
                    if (poller->Next(&packet)) {
                        auto& output_landmarks = packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
                        if (!output_landmarks.empty()) {
                            const auto& hand_landmarks = output_landmarks[0];
                            
                            // 제스처 분석 및 마우스 제어
                            std::vector<int> fingers = GestureControl::get_raised_fingers(hand_landmarks);
                            GestureControl::handle_mouse_event(fingers, hand_landmarks, xdo, screen_width, screen_height, prev_x, prev_y, mouse_hold_state);

                            // 랜드마크 그리기 (시각화)
                            for(const auto& landmark : hand_landmarks.landmark()){
                                cv::circle(flipped_mat, cv::Point(landmark.x() * flipped_mat.cols, landmark.y() * flipped_mat.rows), 5, cv::Scalar(255,0,255), cv::FILLED);
                            }
                        }
                    }

                    // FPS 계산 및 표시
                    auto curr_time = std::chrono::high_resolution_clock::now();
                    double fps = 1.0 / std::chrono::duration_cast<std::chrono::duration<double>>(curr_time - prev_time).count();
                    prev_time = curr_time;
                    cv::putText(flipped_mat, std::to_string(static_cast<int>(fps)), cv::Point(20, 50), cv::FONT_HERSHEY_PLAIN, 3, cv::Scalar(0, 0, 0), 3);

                    cv::imshow("Virtual Touch C++", flipped_mat);
                    if (cv::waitKey(1) == 'q') goto end;
                }
            }
        }
        av_packet_unref(pkt);
    }

end:
    // 5. 리소스 해제
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