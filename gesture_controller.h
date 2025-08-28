#pragma once
#include <vector>
#include "mediapipe/tasks/cc/components/containers/landmark.h"
#include "mouse_controller.h"

class GestureController {
public:
    GestureController(MouseController& mouse_controller);

    void handle_gestures(const std::vector<mediapipe::tasks::components::containers::NormalizedLandmark>& landmarks);

private:
    using LandmarkList = std::vector<mediapipe::tasks::components::containers::NormalizedLandmark>;

    std::vector<int> get_raised_fingers(const LandmarkList& landmarks);
    float linear_interp(float x, float in_min, float in_max, float out_min, float out_max);

    MouseController& mouse_controller_;
    float prev_x_ = 0.0f, prev_y_ = 0.0f;
    bool mouse_hold_state_ = false;

    // 상수 정의
    static const int CAM_WIDTH = 640;
    static const int CAM_HEIGHT = 480;
    static const int BOUNDARY_REVISION = 170;
    static constexpr float SMOOTH_ALPHA = 0.2f;
};