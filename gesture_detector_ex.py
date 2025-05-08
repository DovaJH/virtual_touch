import util

class GestureDetectorEx:
    def __init__(self):
        pass
    """
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
    """
    def detect_gesture(self, frame, landmark_list, processed, mouse_controller):
        if len(landmark_list) >= 6:
            # 손목에서 엄지까지의 거리와 손목에서 검지까지의 거리를 비교
            thumb_to_pinky = util.get_distance([landmark_list[1], landmark_list[5]])
            index_to_pinky = util.get_distance([landmark_list[2], landmark_list[5]])
            
            
            # 주먹을 쥐고 검지 손가락만 핀 상태인 경우 (손목에서 검지까지의 거리가 손목에서 엄지까지의 거리보다 큰 경우)
            if index_to_pinky > thumb_to_pinky:
                mouse_controller.move_mouse(landmark_list[2])  # 검지 손가락의 좌표로 마우스 이동

            """
            elif self.is_left_click(landmark_list, thumb_index_dist):
                mouse_controller.left_click()
            elif self.is_right_click(landmark_list, thumb_index_dist):
                mouse_controller.right_click()
            elif self.is_double_click(landmark_list, thumb_index_dist):
                mouse_controller.double_click()
            elif self.is_screenshot(landmark_list, thumb_index_dist):
                mouse_controller.take_screenshot()            """
