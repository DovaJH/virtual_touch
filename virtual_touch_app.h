// virtual_touch_app.h

#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <chrono> 
// #include <thread> // 제거
// #include <condition_variable> // 제거
// #include <atomic> // 제거

// 1. Status, COUNT 등의 매크로가 포함된 X11 관련 헤더들을 먼저 모두 포함합니다.
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

// 2. X11 헤더 포함이 끝난 후, 이름 충돌을 막기 위해 매크로들을 해제합니다.
#undef Status
#undef COUNT 

#include "mediapipe/framework/formats/image.h"
#include "mediapipe/tasks/cc/components/containers/landmark.h"
#include "mediapipe/tasks/cc/vision/hand_landmarker/hand_landmarker.h"

// Forward declarations
class WebcamManager;
class MouseController;
class GestureController;

class VirtualTouchApp {
public:
    VirtualTouchApp();
    ~VirtualTouchApp();

    bool setup();
    void run();

private:
    void on_landmarks_detected(
        absl::StatusOr<mediapipe::tasks::vision::hand_landmarker::HandLandmarkerResult> result,
        const mediapipe::Image& image, int64_t timestamp_ms);

    // 마우스 제어 스레드 관련 멤버 모두 제거
    // void mouse_control_thread_func(); // 제거
    // std::thread mouse_control_thread_; // 제거
    // std::atomic<bool> stop_thread_ = false; // 제거
    // std::condition_variable cv_; // 제거
    // bool new_landmarks_available_ = false; // 제거
    // std::string latest_hand_label_ = ""; // 제거

    std::unique_ptr<WebcamManager> webcam_;
    std::unique_ptr<MouseController> mouse_controller_;
    std::unique_ptr<GestureController> gesture_controller_;
    std::unique_ptr<mediapipe::tasks::vision::hand_landmarker::HandLandmarker> landmarker_;
    
    std::mutex landmarks_mutex_;
    std::vector<mediapipe::tasks::components::containers::NormalizedLandmark> latest_landmarks_;
    std::string latest_hand_label_ = ""; // ✨ 화면 출력을 위해 레이블은 남겨둡니다.
};