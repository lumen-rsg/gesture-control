import cv2


class Camera:
    def __init__(
        self,
        device: int = 0,
        width: int = 640,
        height: int = 480,
        fps: int = 30,
    ):
        self._cap = cv2.VideoCapture(device, cv2.CAP_V4L2)

        if not self._cap.isOpened():
            raise RuntimeError(f"Failed to open camera device {device}")

        # Desired camera settings (the driver may ignore them)
        self._cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        self._cap.set(cv2.CAP_PROP_FPS, fps)

    def read(self):
        success, frame = self._cap.read()

        if not success:
            raise RuntimeError("Failed to read frame from camera")

        return frame

    def release(self):
        if self._cap is not None:
            self._cap.release()
            self._cap = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.release()
