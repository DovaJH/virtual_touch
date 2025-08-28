#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <memory>
#include <mutex> // [추가] 스레드 동기화를 위한 뮤텍스 헤더

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

#include "mediapipe/framework/formats/image.h"
#include "mediapipe/tasks/cc/components/containers/landmark.h"
#include "mediapipe/tasks/cc/vision/hand_landmarker/hand_landmarker.h"

// [추가] 스레드 간 데이터 및 상태 공유를 위한 전역 변수
std::mutex landmarks_mutex;    // 메인 스레드와 콜백 스레드 간 데이터 동기화를 위한 뮤텍스
std::vector<mediapipe::tasks::components::containers::NormalizedLandmark> latest_landmarks;
Display* global_display = nullptr; // X11 디스플레이 포인터
Window global_root_window; // 루트 윈도우 핸들
int global_screen_width, global_screen_height; // 화면 크기
float global_prev_x = 0.0f, global_prev_y = 0.0f; // 이전 마우스 위치
bool global_mouse_hold_state = false; // 마우스 홀드 상태

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

    using LandmarkList = std::vector<mediapipe::tasks::components::containers::NormalizedLandmark>;

    void click_mouse(Display* display, unsigned int button) {
        XTestFakeButtonEvent(display, button, True, CurrentTime);
        XTestFakeButtonEvent(display, button, False, CurrentTime);
        XFlush(display);
    }

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

    // [수정] 마우스 제어 함수가 전역 변수를 사용하도록 변경
    void handle_mouse_event(const std::vector<int>& fingers, const LandmarkList& landmarks) {
        if (landmarks.size() < 9) return;
        
        float index_finger_x = landmarks[8].x * CAM_WIDTH;
        float index_finger_y = landmarks[8].y * CAM_HEIGHT;
        float curr_x = global_prev_x, curr_y = global_prev_y;

        bool is_drag_gesture = (fingers[0] == 0 && fingers[1] == 1 && fingers[2] == 1 && fingers[3] == 0 && fingers[4] == 0);

        if (is_drag_gesture) {
            if (!global_mouse_hold_state) {
                XTestFakeButtonEvent(global_display, 1, True, CurrentTime);
                global_mouse_hold_state = true;
            }
            float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, global_screen_width);
            float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, global_screen_height);
            curr_x = global_prev_x + (new_x - global_prev_x) * SMOOTH_ALPHA;
            curr_y = global_prev_y + (new_y - global_prev_y) * SMOOTH_ALPHA;
            
            XWarpPointer(global_display, NULL, global_root_window, 0, 0, 0, 0, curr_x, curr_y);
            XFlush(global_display);
        } else {
            if (global_mouse_hold_state) {
                XTestFakeButtonEvent(global_display, 1, False, CurrentTime);
                XFlush(global_display);
                global_mouse_hold_state = false;
            }
            if (fingers[0] == 1) {
                if (fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) click_mouse(global_display, 1);
                else if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 1) click_mouse(global_display, 3);
            } else {
                if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) {
                    float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, global_screen_width);
                    float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, global_screen_height);
                    curr_x = global_prev_x + (new_x - global_prev_x) * SMOOTH_ALPHA;
                    curr_y = global_prev_y + (new_y - global_prev_y) * SMOOTH_ALPHA;
                    XWarpPointer(global_display, NULL, global_root_window, 0, 0, 0, 0, curr_x, curr_y);
                    XFlush(global_display);
                } else if (fingers == std::vector<int>{0,0,0,0,0}) click_mouse(global_display, 5);
                else if (fingers[4] == 1 && fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0) click_mouse(global_display, 4);
            }
        }
        global_prev_x = curr_x;
        global_prev_y = curr_y;
    }

    // [추가] LIVE_STREAM 모드를 위한 콜백 함수 정의
    void OnLandmarksDetected(
        absl::StatusOr<mediapipe::tasks::vision::hand_landmarker::HandLandmarkerResult> result, // <-- [수정] 타입 변경
        const mediapipe::Image& image, int64_t timestamp_ms) {
    
        // [추가] 결과가 유효한지 (OK인지) 먼저 확인합니다.
        if (!result.ok()) {
            // 에러가 발생했으면 처리 (예: 로그 출력)
            // std::cerr << "Landmark detection failed: " << result.status() << std::endl;
            return;
        }
    
        // [수정] 결과에 접근할 때 .value()를 사용합니다.
        if (!result->hand_landmarks.empty()) {
            const auto& current_frame_landmarks = result->hand_landmarks[0].landmarks;
    
            // 제스처 분석 및 마우스 제어 로직 (이전과 동일)
            std::vector<int> fingers = get_raised_fingers(current_frame_landmarks);
            handle_mouse_event(fingers, current_frame_landmarks);
    
            // 화면에 그리기 위한 데이터를 메인 스레드로 전달 (뮤텍스 사용)
            std::lock_guard<std::mutex> lock(landmarks_mutex);
            latest_landmarks = current_frame_landmarks;
        } else {
            // 손이 감지되지 않으면 그리기용 데이터도 비움
            std::lock_guard<std::mutex> lock(landmarks_mutex);
            latest_landmarks.clear();
        }
    }
}

int main() {
    avdevice_register_all();
    const char* dev_name = "/dev/video0";
    const AVInputFormat* inputFormat = av_find_input_format("v4l2");
    AVFormatContext* fmt_ctx = nullptr;
    AVDictionary* ffmpeg_options = nullptr;
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

    // [수정] X11 초기화 후 콜백 함수에서 사용할 수 있도록 전역 변수에 할당
    Display* display = XOpenDisplay(NULL);
    if (display == NULL) { std::cerr << "⛔ X 서버에 연결할 수 없습니다!" << std::endl; return -1; }
    global_display = display;
    global_root_window = DefaultRootWindow(display);
    Window dummy_win; int dummy_int; unsigned int dummy_uint;
    XGetGeometry(display, global_root_window, &dummy_win, &dummy_int, &dummy_int, (unsigned int*)&global_screen_width, (unsigned int*)&global_screen_height, &dummy_uint, &dummy_uint);
    
    // [수정] HandLandmarker 옵션을 LIVE_STREAM 모드로 설정
    auto hand_landmarker_options = std::make_unique<mediapipe::tasks::vision::hand_landmarker::HandLandmarkerOptions>();
    hand_landmarker_options->base_options.model_asset_path = "mediapipe/examples/desktop/my_virtual_touch/hand_landmarker.task";
    hand_landmarker_options->running_mode = mediapipe::tasks::vision::core::RunningMode::LIVE_STREAM;
    hand_landmarker_options->num_hands = 1;
    hand_landmarker_options->result_callback = GestureControl::OnLandmarksDetected; // 콜백 함수 지정

    auto landmarker_result = mediapipe::tasks::vision::hand_landmarker::HandLandmarker::Create(std::move(hand_landmarker_options));
    if (!landmarker_result.ok()) {
        std::cerr << "⛔ HandLandmarker 생성 실패! " << landmarker_result.status() << std::endl;
        return -1;
    }
    std::unique_ptr<mediapipe::tasks::vision::hand_landmarker::HandLandmarker> landmarker = std::move(landmarker_result.value());

    auto prev_time = std::chrono::high_resolution_clock::now();
    auto start_time = std::chrono::high_resolution_clock::now();
    std::cout << "🎬 가상 터치 시작... (q 키 또는 Ctrl+C로 종료)" << std::endl;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, rgb_frame->data, rgb_frame->linesize);
                    cv::Mat input_mat(codec_ctx->height, codec_ctx->width, CV_8UC3, rgb_frame->data[0], rgb_frame->linesize[0]);

                     // [추가] OpenCV의 BGR 형식에 맞게 색상 채널 순서를 RGB에서 BGR로 변환합니다.
                     cv::Mat bgr_mat;
                     cv::cvtColor(input_mat, bgr_mat, cv::COLOR_RGB2BGR);
 

                    cv::Mat flipped_mat;
                    cv::flip(bgr_mat, flipped_mat, 1);

                    // [수정] DetectAsync를 사용하여 비동기적으로 랜드마크 검출 요청
                    auto now = std::chrono::high_resolution_clock::now();
                    int64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                    
                    // 1. MediaPipe가 소유할 수 있는 새로운 ImageFrame을 생성합니다.
                    //    이렇게 하면 이 프레임의 생명주기는 MediaPipe가 관리하게 됩니다.
                    auto mp_image_frame = std::make_shared<mediapipe::ImageFrame>(
                    mediapipe::ImageFormat::SRGB, flipped_mat.cols, flipped_mat.rows);

                    // 2. OpenCV Mat의 데이터를 새로 생성한 ImageFrame의 버퍼로 직접 복사합니다.
                    memcpy(mp_image_frame->MutablePixelData(), flipped_mat.data, flipped_mat.total() * flipped_mat.elemSize());

                    mediapipe::Image mp_image(mp_image_frame);
                    
                    landmarker->DetectAsync(mp_image, timestamp_ms);

                    // [수정] 메인 스레드는 화면에 그리는 역할만 수행
                    GestureControl::LandmarkList landmarks_to_draw;
                    {
                        // 뮤텍스로 보호하며 콜백 스레드가 업데이트한 최신 랜드마크 데이터를 복사
                        std::lock_guard<std::mutex> lock(landmarks_mutex);
                        landmarks_to_draw = latest_landmarks;
                    }
                    
                    if (!landmarks_to_draw.empty()) {
                        for(const auto& landmark : landmarks_to_draw){
                            cv::circle(flipped_mat, cv::Point(landmark.x * flipped_mat.cols, landmark.y * flipped_mat.rows), 5, cv::Scalar(255,0,255), cv::FILLED);
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
    
    // [추가] HandLandmarker 리소스 정리
    landmarker->Close();

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