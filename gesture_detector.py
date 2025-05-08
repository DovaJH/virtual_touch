import util

class GestureDetector:
    def __init__(self):
        pass

    def is_left_click(self, landmark_list, thumb_index_dist):
        return (
            util.get_angle(landmark_list[5], landmark_list[6], landmark_list[8]) < 50 and
            util.get_angle(landmark_list[9], landmark_list[10], landmark_list[12]) > 90 and
            thumb_index_dist > 50
        )

    def is_right_click(self, landmark_list, thumb_index_dist):
        return (
            util.get_angle(landmark_list[9], landmark_list[10], landmark_list[12]) < 50 and
            util.get_angle(landmark_list[5], landmark_list[6], landmark_list[8]) > 90 and
            thumb_index_dist > 50
        )

    def is_double_click(self, landmark_list, thumb_index_dist):
        return (
            util.get_angle(landmark_list[5], landmark_list[6], landmark_list[8]) < 50 and
            util.get_angle(landmark_list[9], landmark_list[10], landmark_list[12]) < 50 and
            thumb_index_dist > 50
        )

    def is_screenshot(self, landmark_list, thumb_index_dist):
        return (
            util.get_angle(landmark_list[5], landmark_list[6], landmark_list[8]) < 50 and
            util.get_angle(landmark_list[9], landmark_list[10], landmark_list[12]) < 50 and
            thumb_index_dist < 50
        )

    def detect_gesture(self, frame, landmark_list, processed, hand_detector, mouse_controller):
        if len(landmark_list) >= 21:
            index_finger_tip = hand_detector.find_finger_tip(processed)
            thumb_index_dist = util.get_distance([landmark_list[4], landmark_list[5]])

            if util.get_distance([landmark_list[4], landmark_list[5]]) < 50 and util.get_angle(landmark_list[5], landmark_list[6], landmark_list[8]) > 90:
                mouse_controller.move_mouse(index_finger_tip)
            elif self.is_left_click(landmark_list, thumb_index_dist):
                mouse_controller.left_click()
                cv2.putText(frame, "Left Click", (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
            elif self.is_right_click(landmark_list, thumb_index_dist):
                mouse_controller.right_click()
                cv2.putText(frame, "Right Click", (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            elif self.is_double_click(landmark_list, thumb_index_dist):
                mouse_controller.double_click()
                cv2.putText(frame, "Double Click", (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 0), 2)
            elif self.is_screenshot(landmark_list, thumb_index_dist):
                mouse_controller.take_screenshot()
                cv2.putText(frame, "Screenshot Taken", (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 0), 2) 