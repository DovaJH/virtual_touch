#include "gesture_controller.h"
#include <cmath>

GestureController::GestureController(MouseController& mouse_controller)
    : mouse_controller_(mouse_controller) {}

float GestureController::linear_interp(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

std::vector<int> GestureController::get_raised_fingers(const LandmarkList& landmarks, const std::string& hand_label) {
    std::vector<int> fingers(5, 0);
    const int fingertip_indices[] = {4, 8, 12, 16, 20};
    if (landmarks.size() < 21) return fingers;

    // Thumb ( 엄지 )
    if (hand_label == "Right") {
        // 오른손: 엄지 끝(4)이 엄지 첫 번째 마디(3)보다 x값이 크면 편 것으로 간주
        if (landmarks[fingertip_indices[0]].x > landmarks[fingertip_indices[0] - 1].x) {
            fingers[0] = 1;
        }
    } else { // "Left"
        // 왼손: 엄지 끝(4)이 엄지 첫 번째 마디(3)보다 x값이 작으면 편 것으로 간주
        if (landmarks[fingertip_indices[0]].x < landmarks[fingertip_indices[0] - 1].x) {
            fingers[0] = 1;
        }
    }

    // Other fingers ( 나머지 손가락 )
    for (int i = 1; i < 5; ++i) {
        if (landmarks[fingertip_indices[i]].y < landmarks[fingertip_indices[i] - 2].y) {
            fingers[i] = 1;
        }
    }
    return fingers;
}

void GestureController::handle_gestures(const LandmarkList& landmarks, const std::string& hand_label) {
    if (landmarks.size() < 9) return;

    std::vector<int> fingers = get_raised_fingers(landmarks, hand_label);

    float index_finger_x = landmarks[8].x * CAM_WIDTH;
    float index_finger_y = landmarks[8].y * CAM_HEIGHT;
    float curr_x = prev_x_, curr_y = prev_y_;

    bool is_drag_gesture = (fingers[0] == 0 && fingers[1] == 1 && fingers[2] == 1 && fingers[3] == 0 && fingers[4] == 0);
    
    // 엄지 손가락이 펴진 상태
    if (fingers[0] == 1){
        // 검지와 엄지를 핀 상태에서 검지를 접었을 때 : 좌클릭
        if (fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) mouse_controller_.click(1);
        // 검지와 엄지, 소지지를 핀 상태: 오른쪽 클릭
        else if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 1) mouse_controller_.click(3);
    }
    // 엄지 손가락이 접힌 상태
    else{
        // 검지 단독: 마우스 이동
        if (fingers[1] == 1 && fingers[2] == 0 && fingers[3] == 0 && fingers[4] == 0) {
            float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, mouse_controller_.get_screen_width());
            float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, mouse_controller_.get_screen_height());
            curr_x = prev_x_ + (new_x - prev_x_) * SMOOTH_ALPHA;
            curr_y = prev_y_ + (new_y - prev_y_) * SMOOTH_ALPHA;
            mouse_controller_.move(curr_x, curr_y);
        }
        // 주먹: 스크롤 다운
        else if (fingers == std::vector<int>{0,0,0,0,0}) mouse_controller_.click(5);
        // 새끼 단독: 스크롤 업
        else if (fingers[4] == 1 && fingers[1] == 0 && fingers[2] == 0 && fingers[3] == 0) mouse_controller_.click(4);
    }

    if(is_drag_gesture){
        if(!mouse_hold_state_){
            mouse_controller_.press(1);
            mouse_hold_state_ = true;
        }
        float new_x = linear_interp(index_finger_x, BOUNDARY_REVISION, CAM_WIDTH - BOUNDARY_REVISION, 0, mouse_controller_.get_screen_width());
        float new_y = linear_interp(index_finger_y, BOUNDARY_REVISION, CAM_HEIGHT - BOUNDARY_REVISION, 0, mouse_controller_.get_screen_height());
        curr_x = prev_x_ + (new_x - prev_x_) * SMOOTH_ALPHA;
        curr_y = prev_y_ + (new_y - prev_y_) * SMOOTH_ALPHA;
        mouse_controller_.move(curr_x, curr_y);
    }
    else{
        if(mouse_hold_state_){
            mouse_controller_.release(1);
            mouse_hold_state_ = false;
        }
    }
    
    prev_x_ = curr_x;
    prev_y_ = curr_y;
}