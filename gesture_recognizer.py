import cv2
import mediapipe as mp
import numpy as np
import pyautogui
import autopy

class GestureRecognizer:
    def __init__(self, scr_w, scr_h):
        # MediaPipe 손 인식 모델 설정
        self.mediapipe_hands = mp.solutions.hands
        self.hand_processor = self.mediapipe_hands.Hands(
            static_image_mode=False,
            max_num_hands=1,
            min_detection_confidence=0.5,
            min_tracking_confidence=0.5
        )
        
        # 손가락 끝점의 랜드마크 인덱스 (엄지, 검지, 중지, 약지, 소지)
        self.fingertip_index = [4, 8, 12, 16, 20]

        # 기본 변수 설정
        self.boundary_revision = 170  # 프레임 경계
        self.smooth_alpha = 0.2  # 마우스 이동 부드러움 정도 조절
        self.scroll_down_speed = -70  # 스크롤 다운 속도
        self.scroll_up_speed = 70  # 스크롤 업 속도

        self.scr_w = scr_w
        self.scr_h = scr_h
        self.w = 640
        self.h = 480

    def detect_hands(self, frame, draw=True):
        # 이미지 전처리 후 손 감지
        frameRGB = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        self.process_results = self.hand_processor.process(frameRGB)
        return frame

    def get_landmarks(self, frame, draw=True):   
        coord_x_list, coord_y_list, self.landmark_list = [], [], []
        if self.process_results.multi_hand_landmarks:
            # 손 선택
            selectedHand = self.process_results.multi_hand_landmarks[0]        
            
            # 랜드마크를 스크린 좌표로 변환
            for idx, landmark in enumerate(selectedHand.landmark):                
                coord_x, coord_y = int(landmark.x * self.w), int(landmark.y * self.h)
                coord_x_list.append(coord_x)
                coord_y_list.append(coord_y)
                self.landmark_list.append([idx, coord_x, coord_y])
                if draw:
                    # 각 랜드마크 위치에 점 그리기
                    cv2.circle(frame, (coord_x, coord_y), 5, (255, 0, 255), cv2.FILLED)
           
        return self.landmark_list

    def is_fingers_raised(self):
        # 각 손가락이 펴져있는지 확인
        fingers = []

        # 엄지 확인
        # x 좌표 비교
        if self.landmark_list[self.fingertip_index[0]][1] > self.landmark_list[self.fingertip_index[0] - 1][1]:
            fingers.append(1)
        else:
            fingers.append(0)

        # 나머지 손가락들 확인
        # y 좌표 비교
        for i in range(1, 5):
            if self.landmark_list[self.fingertip_index[i]][2] < self.landmark_list[self.fingertip_index[i] - 2][2]:
                fingers.append(1)
            else:
                fingers.append(0)

        return fingers
    
    def mouse_event(self, fingers, prev_x, prev_y):
        index_finger_x, index_finger_y = self.landmark_list[8][1:]  # 검지 끝점 좌표
        curr_x, curr_y = prev_x, prev_y

        # 검지만 펴고 있을 때: 마우스 이동 (기본자세)
        if fingers[0] == 0 and fingers[1] == 1 and fingers[2] == 0 and fingers[3] == 0 and fingers[4] == 0:
            # 선형 보간으로 좌표 보정
            new_index_finger_x = np.interp(index_finger_x, (self.boundary_revision, self.w - self.boundary_revision), (0, self.scr_w))
            new_index_finger_y = np.interp(index_finger_y, (self.boundary_revision, self.h - self.boundary_revision), (0, self.scr_h))

            # 마우스 이동 부드러움 처리 조절(EMA 사용)
            curr_x = prev_x + (new_index_finger_x - prev_x) * self.smooth_alpha
            curr_y = prev_y + (new_index_finger_y - prev_y) * self.smooth_alpha
                                
            autopy.mouse.move(self.scr_w - curr_x, curr_y)
        
        # 검지와 엄지를 핀 상태에서 검지를 접었을 때 : 좌클릭
        elif fingers[0] == 1 and fingers[1] == 0 and fingers[2] == 0 and fingers[3] == 0 and fingers[4] == 0:
            autopy.mouse.click()

        # 검지와 엄지, 소지지를 핀 상태: 오른쪽 클릭
        elif fingers[0] == 1 and fingers[1] == 1 and fingers[2] == 0 and fingers[3] == 0 and fingers[4] == 1:
            autopy.mouse.click(autopy.mouse.Button.RIGHT)

        # 엄지를 제외한 모든 손가락을 펴고 있을 때: 드래그
        elif fingers[0] == 0 and fingers[1] == 1 and fingers[2] == 1 and fingers[3] == 1 and fingers[4] == 1:
            autopy.mouse.toggle(down=True)

            # 선형 보간으로 좌표 보정
            new_index_finger_x = np.interp(index_finger_x, (self.boundary_revision, self.w - self.boundary_revision), (0, self.scr_w))
            new_index_finger_y = np.interp(index_finger_y, (self.boundary_revision, self.h - self.boundary_revision), (0, self.scr_h))

            # 마우스 이동 부드러움 처리 조절(EMA 사용)
            curr_x = prev_x + (new_index_finger_x - prev_x) * self.smooth_alpha
            curr_y = prev_y + (new_index_finger_y - prev_y) * self.smooth_alpha
                                
            autopy.mouse.move( self.scr_w - curr_x, curr_y)    
                
            autopy.mouse.toggle(down=False)           

        # 모든 손가락을 접고 있을 때: 스크롤 다운
        elif fingers[0] == 0 and fingers[1] == 0 and fingers[2] == 0 and fingers[3] == 0 and fingers[4] == 0:
            pyautogui.scroll(self.scroll_down_speed)
            
        # 소지만 폈을 때: 스크롤 업
        elif fingers[0] == 0 and fingers[1] == 0 and fingers[2] == 0 and fingers[3] == 0 and fingers[4] == 1:
            pyautogui.scroll(self.scroll_up_speed)
        else:
            pass

        return curr_x, curr_y 