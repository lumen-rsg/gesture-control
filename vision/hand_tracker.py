from pathlib import Path

import mediapipe as mp
import numpy as np

from gestures import Hand, Landmark


class HandTracker:
    """
    Track a hand using MediaPipe Hand Landmarker.

    Returns a Hand containing 21 normalized landmarks.
    """

    def __init__(
        self,
        model_path: str = "models/hand_landmarker.task",
        min_hand_detection_confidence: float = 0.5,
        min_hand_presence_confidence: float = 0.5,
        min_tracking_confidence: float = 0.5,
    ):
        self.model_path = Path(model_path)

        if not self.model_path.exists():
            raise FileNotFoundError(
                f"Hand Landmarker model not found: "
                f"{self.model_path}"
            )

        base_options = mp.tasks.BaseOptions(
            model_asset_path=str(self.model_path)
        )

        options = mp.tasks.vision.HandLandmarkerOptions(
            base_options=base_options,
            running_mode=(
                mp.tasks.vision.RunningMode.VIDEO
            ),
            num_hands=1,
            min_hand_detection_confidence=(
                min_hand_detection_confidence
            ),
            min_hand_presence_confidence=(
                min_hand_presence_confidence
            ),
            min_tracking_confidence=(
                min_tracking_confidence
            ),
        )

        self.landmarker = (
            mp.tasks.vision.HandLandmarker.create_from_options(
                options
            )
        )

        self.timestamp_ms = 0

    def process(
        self,
        frame: np.ndarray,
    ) -> Hand | None:
        """
        Track a hand in an OpenCV BGR frame.

        Returns:
            Hand with 21 normalized landmarks,
            or None if no hand is detected.
        """

        rgb_frame = mp.Image(
            image_format=mp.ImageFormat.SRGB,
            data=frame[:, :, ::-1],
        )

        #
        # MediaPipe VIDEO mode requires monotonically
        # increasing timestamps.
        #
        self.timestamp_ms += 1

        result = self.landmarker.detect_for_video(
            rgb_frame,
            self.timestamp_ms,
        )

        if not result.hand_landmarks:
            return None

        #
        # We currently track only one hand.
        #
        hand_landmarks = result.hand_landmarks[0]

        landmarks = [
            Landmark(
                x=float(landmark.x),
                y=float(landmark.y),
                z=float(landmark.z),
            )
            for landmark in hand_landmarks
        ]

        #
        # Hand Landmarker should always return
        # exactly 21 landmarks.
        #
        if len(landmarks) != 21:
            return None

        return Hand(
            landmarks=landmarks
        )

    def close(self):
        self.landmarker.close()
