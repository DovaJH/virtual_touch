import cv2
import mediapipe as mp
import numpy as np
import pyautogui
import autopy
import time
#import speech_recognition as sr
from gesture_recognizer import GestureRecognizer

def main():
    # 기본 설정값
    prev_x, prev_y = 0, 0  # 이전 마우스 위치
    prev_time = 0  # FPS 계산용 이전 시간
    
    interval = 0.01  # 타이핑 시 키 입력 간격
    timeout = 5  # 음성 인식 타임아웃
    phrase_time_limit = 10  # 음성 인식 최대 시간
    
    """
    # 음성 인식기 초기화
    r = sr.Recognizer()
    m = sr.Microphone()

    def speech_to_text():
        # 음성을 텍스트로 변환하는 함수
        with m as source:
            r.adjust_for_ambient_noise(source)
            try:
                audio = r.listen(source, timeout=timeout, phrase_time_limit=phrase_time_limit)
                return r.recognize_google(audio)
            except (sr.UnknownValueError, sr.RequestError, sr.WaitTimeoutError):
                return ''
    """
    # 화면 크기 가져오기
    scr_w, scr_h = autopy.screen.size()  

    # 카메라 설정
    # 해상도 고정 크기로 받아오기 -> 카메라 별로 지원 해상도가 다름
    cap = cv2.VideoCapture(0)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_FPS, 30)

    # 프레임 크기 가져오기
    ret, frame = cap.read()
    if not ret:
        print("카메라 초기화 실패")
        exit()
 
    # 제스처 인식기 초기화
    detector = GestureRecognizer(scr_w, scr_h)            

    while True:
        _, img = cap.read()
        img = detector.detect_hands(img)
        lm_list = detector.get_landmarks(img)      

        
        if len(lm_list) != 0:
            fingers = detector.is_fingers_raised()
            prev_x, prev_y = detector.mouse_event(fingers, prev_x, prev_y)
        
        # FPS 표시
        curr_time = time.time()
        fps = 1 / (curr_time - prev_time)
        prev_time = curr_time
        cv2.putText(img, str(int(fps)), (20, 50), cv2.FONT_HERSHEY_PLAIN, 3, (0, 0, 0), 3)
        cv2.imshow("virtual_touch", img)
        
        # 'q' 키를 누르면 종료
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()