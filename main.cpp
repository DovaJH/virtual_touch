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

// X11 라이브러리 (libxdo 대체)
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

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

    // X11 클릭을 위한 헬퍼 함수
    void click_mouse(Display* display, unsigned int button) {
        XTestFakeButtonEvent(display, button, True, CurrentTime); // 버튼 누르기
        XTestFakeButtonEvent(display, button, False, CurrentTime); // 버튼 떼기
        XFlush(display); // 이벤트 즉시 전송
    }

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

    // handle_mouse_event의 파라미터를 xdo_t* 대신 Display*로 변경
    void handle_mouse_event(const std::vector<int>& fingers, const mediapipe::NormalizedLandmarkList& landmarks,
                              Display* display, Window root, int scr_w, int scr_h,
                              float& prev_x, float& prev_y, bool& mouse_hold_state) {
        if (landmarks.landmark_size() < 9) return;
        
        float index_finger_x = landmarks.landmark(8).x() * CAM_WIDTH;
        float index_finger_y = landmarks.landmark(8).y() * CAM_HEIGHT;
        float curr_x = prev_x, curr_y = prev_y;

        bool is_drag_gesture = (fingers[0] == 0 && fingers[1] == 1 && fingers[2] == 1 && fingers[3] == 0 && fingers[4] == 0);

        if (is_drag_gesture) {
            if (!mouse_hold_state) {
                // xdo_mouse_down -> XTestFakeButtonEvent
                XTestFakeButtonEvent(display, 1, True, CurrentTime); // 왼쪽 버튼 누르기
                mouse_hold_state = true;
            }
            float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, scr_w);
            float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, scr_h);
            curr_x = prev_x + (new_x - prev_x) * SMOOTH_ALPHA;
            curr_y = prev_y + (new_y - prev_y) * SMOOTH_ALPHA;
            
            // xdo_move_mouse -> XWarpPointer
            XWarpPointer(display, None, root, 0, 0, 0, 0, scr_w - curr_x, curr_y);
            XFlush(display);
        } else {
            if (mouse_hold_state) {
                // xdo_mouse_up -> XTestFakeButtonEvent
                XTestFakeButtonEvent(display, 1, False, CurrentTime); // 왼쪽 버튼 떼기
                XFlush(display);
                mouse_hold_state = false;
            }
            
            if (fingers[0] == 1) {
                if (fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) {
                    click_mouse(display, 1); // 왼쪽 클릭
                } else if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 1) {
                    click_mouse(display, 3); // 오른쪽 클릭
                }
            } else {
                if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) {
                    float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, scr_w);
                    float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, scr_h);
                    curr_x = prev_x + (new_x - prev_x) * SMOOTH_ALPHA;
                    curr_y = prev_y + (new_y - prev_y) * SMOOTH_ALPHA;
                    
                    XWarpPointer(display, None, root, 0, 0, 0, 0, scr_w - curr_x, curr_y);
                    XFlush(display);
                } else if (fingers == std::vector<int>{0,0,0,0,0}) {
                    click_mouse(display, 5); // 스크롤 다운
                } else if (fingers[4] == 1 && fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0) {
                    click_mouse(display, 4); // 스크롤 업
                }
            }
        }
        prev_x = curr_x;
        prev_y = curr_y;
    }
}

int main() {
    // ... (FFmpeg, MediaPipe 초기화 코드는 동일) ...
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
    const std::string graph_config_str = R"pb(
        input_stream: "input_video"
        output_stream: "hand_landmarks"

        node {
            calculator: "FlowLimiterCalculator"
            input_stream: "input_video"
            input_stream: "FINISHED:hand_landmarks"
            input_stream_info: {
            tag_index: "FINISHED"
            back_edge: true
            }
            output_stream: "throttled_input_video"
        }

        node {
            calculator: "HandLandmarkerGpu"
            input_stream: "IMAGE:throttled_input_video"
            output_stream: "LANDMARKS:hand_landmarks"
            output_stream: "WORLD_LANDMARKS:world_landmarks"
            output_stream: "HANDEDNESS:handedness"
            # ✅ [수정] 아래 node_options를 추가해주세요!
            node_options: {
                [type.googleapis.com/mediapipe.tasks.vision.hand_landmarker.proto.HandLandmarkerGraphOptions] {
                    base_options {
                        # 모델 파일 경로를 지정합니다. 
                        # 실행 파일과 같은 위치에 모델을 둘 경우 "./hand_landmarker.task"로 설정하세요.
                        model_asset_path: "./hand_landmarker.task"
                    }
                }
            }
        }
    )pb";
    mediapipe::CalculatorGraphConfig config;
    if (!google::protobuf::TextFormat::ParseFromString(graph_config_str, &config)) {
        std::cerr << "⛔ MediaPipe 그래프 설정 파싱 실패!" << std::endl; return -1;
    }

    mediapipe::CalculatorGraph graph;
    // 1. GPU 리소스를 먼저 생성하고 그래프에 설정합니다.
    auto gpu_resources_status = mediapipe::GpuResources::Create();
    if (!gpu_resources_status.ok()) {
        std::cerr << "⛔ MediaPipe GPU 리소스 생성 실패!" << std::endl; return -1;
    }
    if (!graph.SetGpuResources(std::move(gpu_resources_status).value()).ok()) {
        std::cerr << "⛔ MediaPipe GPU 리소스 설정 실패!" << std::endl; return -1;
    }

    // 2. GPU 리소스가 준비된 상태에서 그래프를 초기화합니다.
    if (!graph.Initialize(config).ok()) {
        std::cerr << "⛔ MediaPipe 그래프 초기화 실패!" << std::endl; return -1;
    }

    auto poller_status = graph.AddOutputStreamPoller("hand_landmarks");
    if (!poller_status.ok()) {
        std::cerr << "⛔ MediaPipe 출력 스트림 폴러 추가 실패!" << std::endl; return -1;
    }
    mediapipe::OutputStreamPoller poller = std::move(poller_status.value());
    if(!graph.StartRun({}).ok()){
        std::cerr << "⛔ MediaPipe 그래프 실행 실패!" << std::endl; return -1;
    }


    // xdo_new -> XOpenDisplay
    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        std::cerr << "⛔ X 서버에 연결할 수 없습니다!" << std::endl;
        return -1;
    }
    Window root = DefaultRootWindow(display);
    
    // xdo_get_viewport_dimensions -> XGetGeometry
    int screen_width, screen_height;
    Window dummy_win;
    int dummy_int;
    unsigned int dummy_uint;
    XGetGeometry(display, root, &dummy_win, &dummy_int, &dummy_int,
                 (unsigned int*)&screen_width, (unsigned int*)&screen_height, &dummy_uint, &dummy_uint);

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
                            
                            // handle_mouse_event 호출 시 display와 root를 전달
                            GestureControl::handle_mouse_event(fingers, hand_landmarks, display, root, screen_width, screen_height, prev_x, prev_y, mouse_hold_state);

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
    // 수정 후
    if (!graph.CloseInputStream("input_video").ok()) {
        std::cerr << "⚠️ 입력 스트림 닫기 실패!" << std::endl;
    }
    if (!graph.WaitUntilDone().ok()) {
        std::cerr << "⚠️ 그래프 종료 대기 실패!" << std::endl;
    }
    
    // xdo_free -> XCloseDisplay
    XCloseDisplay(display);

    av_frame_free(&rgb_frame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    cv::destroyAllWindows();
    return 0;
}