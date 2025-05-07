import cv2
import mediapipe as mp
import numpy as np
import pyautogui
import autopy
import time
import speech_recognition as sr

class GestureRecognizer:
    def __init__(self):

        # MediaPipe 손 인식 모델 설정
        self.mediaPipeHands = mp.solutions.hands
        self.handProcessor = self.mediaPipeHands.Hands(static_image_mode=False,  max_num_hands=1, min_detection_confidence=0.5, min_tracking_confidence=0.5)
        
        # 손가락 끝점의 랜드마크 인덱스 (엄지, 검지, 중지, 약지, 소지)
        self.fingerTipIndices = [4, 8, 12, 16, 20]


    def detectHands(self, frame, draw=True):
        # 프레임에서 손 감지
        frameRGB = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        self.processResults = self.handProcessor.process(frameRGB)

        return frame

    def getPositions(self, frame, draw=True):   
        # 손의 랜드마크 좌표 추출
        coordXList, coordYList, self.landmarkList  = [], [], []

        if self.processResults.multi_hand_landmarks:
            # 손 선택
            selectedHand = self.processResults.multi_hand_landmarks[0]
            height, width, _ = frame.shape          
            
            # 랜드마크를 스크린 좌표로 변환환
            for idx, landmark in enumerate(selectedHand.landmark):                
                coordX, coordY = int(landmark.x * width), int(landmark.y * height)
                coordXList.append(coordX)
                coordYList.append(coordY)
                self.landmarkList.append([idx, coordX, coordY])
                if draw:
                    # 각 랜드마크 위치에 점 그리기
                    cv2.circle(frame, (coordX, coordY), 5, (255, 0, 255), cv2.FILLED)
            
        return self.landmarkList

    def fingersRaised(self):
        # 각 손가락이 펴져있는지 확인
        fingers = []

        # 엄지 확인
        if self.landmarkList[self.fingerTipIndices[0]][1] > self.landmarkList[self.fingerTipIndices[0] - 1][1]:
            fingers.append(1)
        else:
            fingers.append(0)

        # 나머지 손가락들 확인
        for i in range(1, 5):
            if self.landmarkList[self.fingerTipIndices[i]][2] < self.landmarkList[self.fingerTipIndices[i] - 2][2]:
                fingers.append(1)
            else:
                fingers.append(0)

        return fingers

def main():
    # 기본 설정값
    frameR = 100  # 프레임 경계
    smooth = 8  # 마우스 이동 부드러움 정도
    smooth_alpha = 0.3  # 마우스 이동 부드러움 정도 조절
    prev_x, prev_y = 0, 0  # 이전 마우스 위치
    curr_x, curr_y = 0, 0  # 현재 마우스 위치
    prev_time = 0  # FPS 계산용 이전 시간
    stab_buf = []  # 마우스 위치 안정화를 위한 버퍼
    stab_thresh = 10  # 안정화 임계값
    stab_rad = 10  # 안정화 반경
    scroll_down_speed = -60  # 스크롤 다운 속도
    scroll_up_speed = 60  # 스크롤 업 속도
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
    cap = cv2.VideoCapture(0)
    cap.set(3, scr_w/4)
    cap.set(4, scr_h/4)
    cap.set(cv2.CAP_PROP_FPS, 30)

    # 프레임 크기 가져오기
    ret, frame = cap.read()
    if not ret:
        print("카메라 초기화 실패")
        exit()
    h, w, _ = frame.shape
 
    # 제스처 인식기 초기화
    detector = GestureRecognizer()            
  
    # 마우스 드래그 상태
    hold = False  

    while True:
        _, img = cap.read()
        img = detector.detectHands(img)
        lmList= detector.getPositions(img)      

        if len(lmList) != 0:
            x1, y1 = lmList[8][1:]  # 검지 끝점 좌표
            fingers = detector.fingersRaised()
            #cv2.rectangle(img, (frameR, frameR), (w - frameR, h - frameR), (255, 0, 255), 2)
            
            # 검지만 펴고 있을 때: 마우스 이동
            if fingers[0] == 0 and fingers[1] == 1 and fingers[2] == 0 and fingers[3] == 0 and fingers[4] == 0:
                # 화면 끝까지 손이 안 닿았을 때 좌표를 보정해줌
                x3 = np.interp(x1, (frameR, w - frameR), (0, scr_w))
                y3 = np.interp(y1, (frameR, h - frameR), (0, scr_h))

                # 마우스 이동 부드러움 처리
                """
                curr_x = prev_x + (x3 - prev_x) / smooth
                curr_y = prev_y + (y3 - prev_y) / smooth
                """

                # 마우스 이동 부드러움 처리 조절(EMA 사용)
                curr_x = prev_x + (x3 - prev_x) * smooth_alpha
                curr_y = prev_y + (y3 - prev_y) * smooth_alpha
                                

                autopy.mouse.move(scr_w - curr_x, curr_y)
                #cv2.circle(img, (x1, y1), 7, (255, 0, 255), cv2.FILLED)
                prev_x, prev_y = curr_x, curr_y

                stab_buf.append((curr_x, curr_y))
                if len(stab_buf) > stab_thresh:
                    stab_buf.pop(0)
            
            """
            # 검지와 중지를 펴고 있을 때: 왼쪽 클릭
            if fingers[1] == 1 and fingers[2] == 1:
                if len(stab_buf) == stab_thresh and all(
                    np.linalg.norm(np.array(pos) - np.array(stab_buf[0])) < stab_rad
                    for pos in stab_buf
                ):
                    cv2.circle(img, (x1, y1), 15, (0, 255, 0), cv2.FILLED)
                    autopy.mouse.click()
                    stab_buf.clear()
            
            # 검지와 소지만 펴고 있을 때: 더블 클릭
            if fingers[1] == 1 and fingers[4] == 1 and all(f == 0 for f in [fingers[0], fingers[2], fingers[3]]):
                autopy.mouse.click()
                autopy.mouse.click()
            
            # 엄지만 펴고 있을 때: 오른쪽 클릭
            if fingers[0] == 1 and all(f == 0 for f in fingers[1:]):
                if len(stab_buf) == stab_thresh and all(
                    np.linalg.norm(np.array(pos) - np.array(stab_buf[0])) < stab_rad
                    for pos in stab_buf
                ):
                    cv2.circle(img, (x1, y1), 15, (0, 255, 0), cv2.FILLED)
                    autopy.mouse.click(autopy.mouse.Button.RIGHT)
                    stab_buf.clear()
            
            # 엄지를 제외한 모든 손가락을 펴고 있을 때: 드래그
            if fingers[0] == 0 and all(f == 1 for f in fingers[1:]):
                if not hold:
                    autopy.mouse.toggle(down=True)
                    hold = True

                x3 = np.interp(x1, (frameR, w - frameR), (0, scr_w))
                y3 = np.interp(y1, (frameR, h - frameR), (0, scr_h))
                curr_x = prev_x + (x3 - prev_x) / smooth
                curr_y = prev_y + (y3 - prev_y) / smooth
                autopy.mouse.move(scr_w - curr_x, curr_y)
                cv2.circle(img, (x1, y1), 7, (255, 0, 255), cv2.FILLED)
                prev_x, prev_y = curr_x, curr_y
            else:
                if hold:
                    autopy.mouse.toggle(down=False)
                    hold = False

            # 모든 손가락을 접고 있을 때: 스크롤 다운
            if all(f == 0 for f in fingers):
                pyautogui.scroll(scroll_down_speed)
            
            # 모든 손가락을 펴고 있을 때: 스크롤 업
            if all(f == 1 for f in fingers):
                pyautogui.scroll(scroll_up_speed)

            # 중지만 펴고 있을 때: 음성 인식
            if fingers[2] == 1 and all(f == 0 for f in [fingers[0], fingers[1], fingers[3], fingers[4]]):
                text = speech_to_text()
                if text:
                    pyautogui.write(text, interval=interval)
            """
        # FPS 표시
        curr_time = time.time()
        fps = 1 / (curr_time - prev_time)
        prev_time = curr_time
        cv2.putText(img, str(int(fps)), (20, 50), cv2.FONT_HERSHEY_PLAIN, 3, (0, 0, 0), 3)
        cv2.imshow("Archand", img)
        
        # 'q' 키를 누르면 종료
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()