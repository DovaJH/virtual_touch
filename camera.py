import cv2

class Camera:
    def __init__(self, camera_id=0):
        self.cap = cv2.VideoCapture(camera_id)
        self.frame = None
        self.ret = False

    def read_frame(self):
        self.ret, self.frame = self.cap.read()
        if self.ret:
            self.frame = cv2.resize(self.frame, (480,480))
            self.frame = cv2.flip(self.frame, 1)  # 좌우반전
        return self.ret, self.frame

    def get_frame(self):
        return self.frame

    def release(self):
        self.cap.release()

    def is_opened(self):
        return self.cap.isOpened() 