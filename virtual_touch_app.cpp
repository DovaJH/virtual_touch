#include "virtual_touch_app.h"
#include "webcam_manager.h"
#include "mouse_controller.h"
#include "gesture_controller.h"

#include <iostream>
#include <chrono>
#include <functional>
#include <opencv2/opencv.hpp>

VirtualTouchApp::VirtualTouchApp() = default;
VirtualTouchApp::~VirtualTouchApp() {
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

    return true;
}

void VirtualTouchApp::run() {
    auto prev_time = std::chrono::high_resolution_clock::now();
    auto start_time = std::chrono::high_resolution_clock::now();
    std::cout << "🎬 가상 터치 시작... (q 키 또는 Ctrl+C로 종료)" << std::endl;
    
    cv::Mat frame;
    while (webcam_->get_next_frame(frame)) {
        cv::Mat flipped_mat;
        cv::flip(frame, flipped_mat, 1);

        auto now = std::chrono::high_resolution_clock::now();
        int64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        auto mp_image_frame = std::make_shared<mediapipe::ImageFrame>(
            mediapipe::ImageFormat::SRGB, flipped_mat.cols, flipped_mat.rows);
        memcpy(mp_image_frame->MutablePixelData(), flipped_mat.data, flipped_mat.total() * flipped_mat.elemSize());
        mediapipe::Image mp_image(mp_image_frame);
        
        landmarker_->DetectAsync(mp_image, timestamp_ms);

        std::vector<mediapipe::tasks::components::containers::NormalizedLandmark> landmarks_to_draw;
        {
            std::lock_guard<std::mutex> lock(landmarks_mutex_);
            landmarks_to_draw = latest_landmarks_;
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
        const auto& landmarks = result->hand_landmarks[0].landmarks;
        gesture_controller_->handle_gestures(landmarks);

        std::lock_guard<std::mutex> lock(landmarks_mutex_);
        latest_landmarks_ = landmarks;
    } else {
        std::lock_guard<std::mutex> lock(landmarks_mutex_);
        latest_landmarks_.clear();
    }
}