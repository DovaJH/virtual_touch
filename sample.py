import cv2
import multiprocessing as mup
import time


def preprocess_worker(frame_queue, display_queue):
    prev_time = 0
    while True:
        frame = frame_queue.get()
        if frame is None:
            break
        
        # FPS 계산
        current_time = time.time()
        fps = 1 / (current_time - prev_time) if prev_time > 0 else 0
        prev_time = current_time
        
        # FPS를 화면에 표시
        cv2.putText(frame, f'FPS: {int(fps)}', (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        
        # 처리된 프레임을 큐에 전달
        display_queue.put(frame)


def camera_reader(queue):
    cap = cv2.VideoCapture(0)   # 기본 카메라

    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        queue.put(frame)    # 큐에 프레임 추가
    
    cap.release()
    for _ in range(mup.cpu_count()):
        queue.put(None)     # 종료 신호


if __name__ == '__main__':
    frame_queue = mup.Queue(maxsize=10)    # 큐 크기 제한으로 메모리 제어
    display_queue = mup.Queue(maxsize=1)    # 화면 표시용 큐

    workers=[]
    
    for _ in range(5):    # 프로세스 5개
        p = mup.Process(target=preprocess_worker, args=(frame_queue, display_queue))
        p.start()
        workers.append(p)
    
    # 카메라 리더 프로세스 시작
    camera_process = mup.Process(target=camera_reader, args=(frame_queue,))
    camera_process.start()

    # 메인 프로세스에서 화면 표시
    while True:
        frame = display_queue.get()
        if frame is None:
            break
        cv2.imshow('frame', frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    # 모든 프로세스 종료
    for p in workers:
        p.join()
    camera_process.join()
    
    cv2.destroyAllWindows()