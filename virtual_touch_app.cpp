// virtual_touch_app.cpp (수정된 전체 내용)

#include "virtual_touch_app.h"
#include "webcam_manager.h"
#include "mouse_controller.h"
#include "gesture_controller.h"

#include <iostream>
#include <chrono>
#include <functional>
#include <opencv2/opencv.hpp>
#include "mediapipe/tasks/cc/core/base_options.h"

VirtualTouchApp::VirtualTouchApp() = default;
VirtualTouchApp::~VirtualTouchApp() {
    // ✨ 스레드 종료 로직 제거
    if(landmarker_) {
        landmarker_->Close();
    }
}

bool VirtualTouchApp::setup() {
    webcam_ = std::make_unique<WebcamManager>(640, 480, 30);
    if (!webcam_->initialize()) return false;

    mouse_controller_ = std::make_unique<MouseController>();
    if (!mouse_controller_->initialize()) return false;
    
    gesture_controller_ = std::make_unique<GestureController>(*mouse_controller_);
    
    auto options = std::make_unique<mediapipe::tasks::vision::hand_landmarker::HandLandmarkerOptions>();

    // --- ✨ 신뢰도 옵션 추가 ---
    options->min_hand_detection_confidence = 0.6f; // 손 탐지 신뢰도
    options->min_tracking_confidence = 0.6f;     // 손 추적 신뢰도
    // --- ✨ ---
    
    // --- ✨ GPU 사용 설정 ---
    options->base_options.delegate = mediapipe::tasks::core::BaseOptions::Delegate::GPU;
    // --- ✨ ---

    options->base_options.model_asset_path = "mediapipe/examples/desktop/my_virtual_touch/hand_landmarker.task";
    options->running_mode = mediapipe::tasks::vision::core::RunningMode::LIVE_STREAM;
    options->num_hands = 1;
    
    options->result_callback = 
        [this](absl::StatusOr<mediapipe::tasks::vision::hand_landmarker::HandLandmarkerResult> result,
               const mediapipe::Image& image, int64_t timestamp_ms) {
            this->on_landmarks_detected(std::move(result), image, timestamp_ms);
        };

    auto landmarker_result = mediapipe::tasks::vision::hand_landmarker::HandLandmarker::Create(std::move(options));
    if (!landmarker_result.ok()) {
        std::cerr << "⛔ HandLandmarker 생성 실패! " << landmarker_result.status() << std::endl;
        return false;
    }
    landmarker_ = std::move(landmarker_result.value());

    // ✨ 마우스 제어 스레드 시작 로직 제거
    
    return true;
}

void VirtualTouchApp::run() {
    auto prev_time = std::chrono::high_resolution_clock::now();
    auto start_time_ = std::chrono::high_resolution_clock::now();
    std::cout << "🎬 가상 터치 시작... (q 키 또는 Ctrl+C로 종료)" << std::endl;
    // ✨ 마우스 제어 스레드 시작 알림 제거
    
    cv::Mat frame;  //RGB 형식
    while (true) {

        webcam_->get_next_frame(frame);

        // ✨ --- 최적화된 프레임 처리 로직 (이미지 전처리) --- ✨
        auto now = std::chrono::high_resolution_clock::now();
        int64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();

        // 1. MediaPipe가 사용할 최종 이미지 프레임을 먼저 생성합니다.
        auto mp_image_frame = std::make_shared<mediapipe::ImageFrame>(
            mediapipe::ImageFormat::SRGB, frame.cols, frame.rows);
        
        // 2. 위에서 만든 MediaPipe 프레임의 메모리 버퍼를 직접 가리키는 cv::Mat을 생성합니다.
        cv::Mat destination_mat(frame.rows, frame.cols, CV_8UC3, mp_image_frame->MutablePixelData());

        // 3. 원본 웹캠 프레임(frame)을 좌우 반전시켜 destination_mat에 바로 씁니다.
        cv::flip(frame, destination_mat, 1);
        
        mediapipe::Image mp_image(mp_image_frame);

        
        // 비동기 랜드마크 감지를 호출합니다. (이미지 전처리는 여기서 끝)
        landmarker_->DetectAsync(mp_image, timestamp_ms);

        // 랜드마크 결과를 가져와 화면에 그릴 준비를 합니다. (화면 출력은 메인 스레드에서 계속)
        std::vector<mediapipe::tasks::components::containers::NormalizedLandmark> landmarks_to_draw;
        {
            std::lock_guard<std::mutex> lock(landmarks_mutex_);
            landmarks_to_draw = latest_landmarks_;
        }
        
        // 화면에 그리는 작업은 destination_mat을 사용합니다.
        if (!landmarks_to_draw.empty()) {
            for(const auto& landmark : landmarks_to_draw){
                cv::circle(destination_mat, cv::Point(landmark.x * destination_mat.cols, landmark.y * destination_mat.rows), 5, cv::Scalar(255,0,255), cv::FILLED);
            }
        }
        // ✨ --- 로직 종료 --- ✨

        // 화면 표시(imshow)를 위해 RGB 이미지를 BGR로 변환합니다.
        cv::Mat bgr_display_frame;
        cv::cvtColor(destination_mat, bgr_display_frame, cv::COLOR_RGB2BGR);


        // FPS 계산 및 표시
        auto curr_time = std::chrono::high_resolution_clock::now();
        double fps = 1.0 / std::chrono::duration_cast<std::chrono::duration<double>>(curr_time - prev_time).count();
        prev_time = curr_time;
        cv::putText(bgr_display_frame, std::to_string(static_cast<int>(fps)), cv::Point(20, 50), cv::FONT_HERSHEY_PLAIN, 3, cv::Scalar(0, 255, 0), 3);
        
        // 최종 결과 이미지를 화면에 보여줍니다.
        cv::imshow("Virtual Touch C++", bgr_display_frame);
        if (cv::waitKey(1) == 'q') break;
        
    }
    std::cout << "🛑 프로그램 종료" << std::endl;
}

void VirtualTouchApp::on_landmarks_detected(
    absl::StatusOr<mediapipe::tasks::vision::hand_landmarker::HandLandmarkerResult> result,
    const mediapipe::Image& image, int64_t timestamp_ms) {

    if (!result.ok()) {
        return;
    }
    
    if (!result->hand_landmarks.empty()) {
        
        // ✨ --- 제스처 분석 및 마우스 제어 로직 통합 --- ✨
        if (!result->handedness.empty() && !result->handedness[0].categories.empty()) {
            std::string hand_label = *result->handedness[0].categories[0].category_name;

            const auto& landmarks = result->hand_landmarks[0].landmarks;
            

            // MediaPipe 내부 스레드에서 즉시 제스처 분석 및 마우스 제어 실행
            gesture_controller_->handle_gestures(landmarks, hand_label);

            // 화면 그리기를 위해 랜드마크 저장
            std::lock_guard<std::mutex> lock(landmarks_mutex_);
            latest_landmarks_ = landmarks;
            latest_hand_label_ = hand_label;
        }
        // ✨ --- 통합 완료 --- ✨

    } else {
        // 손이 감지되지 않으면 랜드마크를 지웁니다.
        std::lock_guard<std::mutex> lock(landmarks_mutex_);
        latest_landmarks_.clear();
        latest_hand_label_ = "";
    }   
}

// ✨ mouse_control_thread_func 함수 제거