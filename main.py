import cv2
import time
import pyautogui
from camera import Camera
from hand_detector import HandDetector
#from gesture_detector import GestureDetector
from mouse_controller import MouseController
from gesture_detector_ex import GestureDetectorEx


def main():
    # 객체 초기화
    camera = Camera()
    hand_detector = HandDetector()
    #gesture_detector = GestureDetector()
    esture_detector = GestureDetectorEx()
    mouse_controller = MouseController()
    

    # 화면 해상도 가져오기
    screen_width, screen_height = pyautogui.size()
    resize_width = screen_width // 2
    resize_height = screen_height // 2

    # 시간 측정을 위한 변수
    repeat_time = 0
    capture_list = []
    land_list = []
    gesture_list = []
    output_list = []

    try:
        while camera.is_opened():
            start = 0
            end = 0

            start = time.time()

            ret, frame = camera.read_frame()
            if not ret:
                break

            end = time.time()
            capture_list.append(end - start)

            start = time.time()
            processed = hand_detector.process_frame(frame)
            landmark_list = hand_detector.get_landmark_list(processed)

            if processed.multi_hand_landmarks:

                hand_detector.draw_landmarks(frame, processed.multi_hand_landmarks[0])


            end = time.time()
            land_list.append(end - start)

            start = time.time()

            #gesture_detector.detect_gesture(frame, landmark_list, processed, hand_detector, mouse_controller)
            gesture_detector.detect_gesture(frame, landmark_list, processed, mouse_controller)

            end = time.time()
            gesture_list.append(end - start)


            start = time.time()

            # 화면 해상도의 1/2 크기로 조절
            #frame = cv2.resize(frame, (resize_width, resize_height))
            #frame = cv2.resize(frame, (screen_width, screen_height))
            cv2.imshow('Frame', frame)

            end = time.time()
            output_list.append(end - start)

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    finally:
        # delay 계산
        capture_average = sum(capture_list)/len(capture_list)
        capture_max = max(capture_list)
        capture_min = min(capture_list)

        land_average = sum(land_list)/len(land_list)
        land_max = max(land_list)
        land_min = min(land_list)

        gesture_average = sum(gesture_list)/len(gesture_list)
        gesture_max = max(gesture_list)
        gesture_min = min(gesture_list)

        output_average = sum(output_list)/len(output_list)
        output_max = max(output_list)
        output_min = min(output_list)

        # newfile.py
        f = open("gesture_time.txt", 'w')
        f.write("capture average : {:.5f}    ".format(capture_average))
        f.write("capture max : {:.5f}    ".format(capture_max))
        f.write("capture min : {:.5f}\n".format(capture_min))

        f.write("land average : {:.5f}    ".format(land_average))
        f.write("land max : {:.5f}    ".format(land_max))
        f.write("land min : {:.5f}\n".format(land_min))

        f.write("gesture average : {:.5f}    ".format(gesture_average))
        f.write("gesture max : {:.5f}    ".format(gesture_max))
        f.write("gesture min : {:.5f}\n".format(gesture_min))

        f.write("output average : {:.5f}    ".format(output_average))
        f.write("output max : {:.5f}    ".format(output_max))
        f.write("output min : {:.5f}\n".format(output_min))
        f.close()

        camera.release()
        cv2.destroyAllWindows()

if __name__ == '__main__':
    main()