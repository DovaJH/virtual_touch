#pragma once

#include <memory>
#include <mutex>
#include <vector>

// 1. Status, COUNT 등의 매크로가 포함된 X11 관련 헤더들을 먼저 모두 포함합니다.
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

// 2. X11 헤더 포함이 끝난 후, 이름 충돌을 막기 위해 매크로들을 해제합니다.
#undef Status
#undef COUNT // <-- 이 줄을 추가하세요!

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

    std::unique_ptr<WebcamManager> webcam_;
    std::unique_ptr<MouseController> mouse_controller_;
    std::unique_ptr<GestureController> gesture_controller_;
    std::unique_ptr<mediapipe::tasks::vision::hand_landmarker::HandLandmarker> landmarker_;
    
    std::mutex landmarks_mutex_;
    std::vector<mediapipe::tasks::components::containers::NormalizedLandmark> latest_landmarks_;
};