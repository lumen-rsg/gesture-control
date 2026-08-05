import cv2
import mediapipe as mp
import time

from mediapipe.tasks import python
from mediapipe.tasks.python import vision

from gestures import Hand, Landmark


class HandDetector:
    def __init__(
        self,
        model_path: str = "models/hand_landmarker.task",
        max_num_hands: int = 1,
        min_detection_confidence: float = 0.7,
        min_tracking_confidence: float = 0.5,
    ):
        base_options = python.BaseOptions(
            model_asset_path=model_path
        )

        options = vision.HandLandmarkerOptions(
            base_options=base_options,
            num_hands=max_num_hands,
            min_hand_detection_confidence=min_detection_confidence,
            min_tracking_confidence=min_tracking_confidence,
            running_mode=vision.RunningMode.VIDEO,
        )

        self._detector = vision.HandLandmarker.create_from_options(
            options
        )

        self._timestamp = 0


    def process(self, frame) -> list[Hand]:
        """
        Process OpenCV BGR frame.

        Returns:
            list[Hand]
        """

        rgb = cv2.cvtColor(
            frame,
            cv2.COLOR_BGR2RGB
        )

        mp_image = mp.Image(
            image_format=mp.ImageFormat.SRGB,
            data=rgb
        )

        timestamp = int(time.time() * 1000)

        result = self._detector.detect_for_video(
            mp_image,
            timestamp
        )

        hands = []

        for hand_landmarks in result.hand_landmarks:
            landmarks = []

            for lm in hand_landmarks:
                landmarks.append(
                    Landmark(
                        x=lm.x,
                        y=lm.y,
                        z=lm.z
                    )
                )

            hands.append(
                Hand(
                    landmarks=landmarks
                )
            )

        return hands


    def close(self):
        self._detector.close()
