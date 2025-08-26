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

// 1. Status 매크로가 필요한 X11 관련 헤더들을 먼저 모두 포함합니다.
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

// 2. X11 헤더 포함이 끝난 후, 이름 충돌을 막기 위해 Status 매크로를 해제합니다.
#undef Status

// --- 🔽 [수정] 올바른 헤더 파일 경로로 변경 🔽 ---
#include "mediapipe/framework/formats/image.h"
#include "mediapipe/tasks/cc/components/containers/landmark.h"
#include "mediapipe/tasks/cc/vision/hand_landmarker/hand_landmarker.h"
// --- 🔼 여기까지 수정 🔼 ---

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

    void click_mouse(Display* display, unsigned int button) {
        XTestFakeButtonEvent(display, button, True, CurrentTime);
        XTestFakeButtonEvent(display, button, False, CurrentTime);
        XFlush(display);
    }
    
    // --- 🔽 [수정] API 변경에 맞춰 .x -> .x(), .y -> .y() 로 수정 🔽 ---
    using LandmarkList = std::vector<mediapipe::tasks::components::containers::NormalizedLandmark>;

    std::vector<int> get_raised_fingers(const LandmarkList& landmarks) {
        std::vector<int> fingers(5, 0);
        const int fingertip_indices[] = {4, 8, 12, 16, 20};
        if (landmarks.size() < 21) return fingers;

        if (landmarks[fingertip_indices[0]].x > landmarks[fingertip_indices[0] - 1].x) {
            fingers[0] = 1;
        }
        for (int i = 1; i < 5; ++i) {
            if (landmarks[fingertip_indices[i]].y < landmarks[fingertip_indices[i] - 2].y) {
                fingers[i] = 1;
            }
        }
        return fingers;
    }

    void handle_mouse_event(const std::vector<int>& fingers, const LandmarkList& landmarks,
                              Display* display, Window root, int scr_w, int scr_h,
                              float& prev_x, float& prev_y, bool& mouse_hold_state) {
        if (landmarks.size() < 9) return;
        
        float index_finger_x = landmarks[8].x * CAM_WIDTH;
        float index_finger_y = landmarks[8].y * CAM_HEIGHT;
        float curr_x = prev_x, curr_y = prev_y;

        bool is_drag_gesture = (fingers[0] == 0 && fingers[1] == 1 && fingers[2] == 1 && fingers[3] == 0 && fingers[4] == 0);

        if (is_drag_gesture) {
            if (!mouse_hold_state) {
                XTestFakeButtonEvent(display, 1, True, CurrentTime);
                mouse_hold_state = true;
            }
            float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, scr_w);
            float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, scr_h);
            curr_x = prev_x + (new_x - prev_x) * SMOOTH_ALPHA;
            curr_y = prev_y + (new_y - prev_y) * SMOOTH_ALPHA;
            
            XWarpPointer(display, NULL, root, 0, 0, 0, 0, scr_w - curr_x, curr_y);
            XFlush(display);
        } else {
            if (mouse_hold_state) {
                XTestFakeButtonEvent(display, 1, False, CurrentTime);
                XFlush(display);
                mouse_hold_state = false;
            }
            if (fingers[0] == 1) {
                if (fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) click_mouse(display, 1);
                else if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 1) click_mouse(display, 3);
            } else {
                if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) {
                    float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, scr_w);
                    float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, scr_h);
                    curr_x = prev_x + (new_x - prev_x) * SMOOTH_ALPHA;
                    curr_y = prev_y + (new_y - prev_y) * SMOOTH_ALPHA;
                    XWarpPointer(display, NULL, root, 0, 0, 0, 0, scr_w - curr_x, curr_y);
                    XFlush(display);
                } else if (fingers == std::vector<int>{0,0,0,0,0}) click_mouse(display, 5);
                else if (fingers[4] == 1 && fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0) click_mouse(display, 4);
            }
        }
        prev_x = curr_x;
        prev_y = curr_y;
    }
    // --- 🔼 여기까지 수정 🔼 ---
}

int main() {
    avdevice_register_all();
    const char* dev_name = "/dev/video0";
    const AVInputFormat* inputFormat = av_find_input_format("v4l2");
    AVFormatContext* fmt_ctx = nullptr;
    AVDictionary* ffmpeg_options = nullptr; // <-- [수정] 변수 이름 변경
    av_dict_set(&ffmpeg_options, "video_size", "640x480", 0);
    av_dict_set(&ffmpeg_options, "framerate", "30", 0);

    if (avformat_open_input(&fmt_ctx, dev_name, inputFormat, &ffmpeg_options) != 0) {
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
    SwsContext* sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

     // 1. 옵션 객체를 std::unique_ptr로 생성
     auto hand_landmarker_options = std::make_unique<mediapipe::tasks::vision::hand_landmarker::HandLandmarkerOptions>();
    
     // 2. 포인터이므로 '->'를 사용하여 멤버에 접근
     hand_landmarker_options->base_options.model_asset_path = "mediapipe/examples/desktop/my_virtual_touch/hand_landmarker.task";
     hand_landmarker_options->running_mode = mediapipe::tasks::vision::core::RunningMode::IMAGE;
     hand_landmarker_options->num_hands = 1;

     // 3. std::move를 사용하여 Create 함수에 객체 소유권 이전
     auto landmarker_result = mediapipe::tasks::vision::hand_landmarker::HandLandmarker::Create(std::move(hand_landmarker_options));
    
    if (!landmarker_result.ok()) {
        std::cerr << "⛔ HandLandmarker 생성 실패! " << landmarker_result.status() << std::endl;
        return -1;
    }
    std::unique_ptr<mediapipe::tasks::vision::hand_landmarker::HandLandmarker> landmarker = std::move(landmarker_result.value());

    Display* display = XOpenDisplay(NULL);
    if (display == NULL) { std::cerr << "⛔ X 서버에 연결할 수 없습니다!" << std::endl; return -1; }
    Window root = DefaultRootWindow(display);
    int screen_width, screen_height;
    Window dummy_win; int dummy_int; unsigned int dummy_uint;
    XGetGeometry(display, root, &dummy_win, &dummy_int, &dummy_int, (unsigned int*)&screen_width, (unsigned int*)&screen_height, &dummy_uint, &dummy_uint);
    
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

                    auto mp_image_frame = std::make_shared<mediapipe::ImageFrame>(mediapipe::ImageFormat::SRGB, flipped_mat.cols, flipped_mat.rows, flipped_mat.step, flipped_mat.data, [](uint8_t*){});
                    mediapipe::Image mp_image(mp_image_frame);
                    auto detection_result = landmarker->Detect(mp_image);

                    if (detection_result.ok()) {
                        if (!detection_result->hand_landmarks.empty()) {
                            // --- 🔽 [수정] .landmarks 멤버에 접근하여 실제 vector를 가져옴 🔽 ---
                            const auto& hand_landmarks = detection_result->hand_landmarks[0].landmarks;
                            
                            std::vector<int> fingers = GestureControl::get_raised_fingers(hand_landmarks);
                            GestureControl::handle_mouse_event(fingers, hand_landmarks, display, root, screen_width, screen_height, prev_x, prev_y, mouse_hold_state);

                            for(const auto& landmark : hand_landmarks){
                                cv::circle(flipped_mat, cv::Point(landmark.x * flipped_mat.cols, landmark.y * flipped_mat.rows), 5, cv::Scalar(255,0,255), cv::FILLED);
                            }
                            // --- 🔼 여기까지 수정 🔼 ---
                        }
                    } else {
                        std::cerr << "⚠️ 랜드마크 검출 실패: " << detection_result.status() << std::endl;
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