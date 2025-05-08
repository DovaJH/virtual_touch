import mediapipe as mp
import cv2

class HandDetector:
    def __init__(self):
        self.mpHands = mp.solutions.hands
        self.hands = self.mpHands.Hands(
            static_image_mode=False,
            model_complexity=1,
            min_detection_confidence=0.5,
            min_tracking_confidence=0.5,
            max_num_hands=1
        )
        self.draw = mp.solutions.drawing_utils

    def process_frame(self, frame):
        frameRGB = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        processed = self.hands.process(frameRGB)
        return processed

    def draw_landmarks(self, frame, hand_landmarks):
        if hand_landmarks:
            # 표시할 랜드마크 인덱스들
            landmarks = [
                self.mpHands.HandLandmark.WRIST,  # 손바닥 중심점
                self.mpHands.HandLandmark.THUMB_TIP,
                self.mpHands.HandLandmark.INDEX_FINGER_TIP,
                self.mpHands.HandLandmark.MIDDLE_FINGER_TIP,
                self.mpHands.HandLandmark.RING_FINGER_TIP,
                self.mpHands.HandLandmark.PINKY_TIP
            ]
            
            for landmark_idx in landmarks:
                landmark = hand_landmarks.landmark[landmark_idx]
                x, y = int(landmark.x * frame.shape[1]), int(landmark.y * frame.shape[0])
                cv2.circle(frame, (x, y), 5, (255, 255, 0), -1, cv2.LINE_AA)

    def get_landmark_list(self, processed):
        landmark_list = []
        if processed.multi_hand_landmarks:
            hand_landmarks = processed.multi_hand_landmarks[0]
            # 손바닥 중심점과 손가락 끝점들을 가져옴
            landmarks = [
                self.mpHands.HandLandmark.WRIST,  # 손바닥 중심점
                self.mpHands.HandLandmark.THUMB_TIP,
                self.mpHands.HandLandmark.INDEX_FINGER_TIP,
                self.mpHands.HandLandmark.MIDDLE_FINGER_TIP,
                self.mpHands.HandLandmark.RING_FINGER_TIP,
                self.mpHands.HandLandmark.PINKY_TIP
            ]
            
            for landmark_idx in landmarks:
                landmark = hand_landmarks.landmark[landmark_idx]
                landmark_list.append((landmark.x, landmark.y))
                
        return landmark_list

    def find_finger_tip(self, processed):
        if processed.multi_hand_landmarks:
            hand_landmarks = processed.multi_hand_landmarks[0]
            index_finger_tip = hand_landmarks.landmark[self.mpHands.HandLandmark.INDEX_FINGER_TIP]
            return index_finger_tip
        return None 