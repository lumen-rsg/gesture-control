from pathlib import Path

import mediapipe as mp

from models import Detection


class HandTracker:
    def __init__(
        self,
        model_path: str = "models/hand_landmarker.task",
        padding: float = 0.15,
        min_hand_detection_confidence: float = 0.5,
        min_hand_presence_confidence: float = 0.5,
        min_tracking_confidence: float = 0.5,
    ):
        self.model_path = Path(model_path)
        self.padding = padding

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
            num_hands=2,
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

    def process(self, frame) -> list[Detection]:
        """
        Detect hands and return normalized bounding boxes.

        Coordinates are normalized to [0, 1].

        Padding is added around the bounding box as a
        fraction of its width and height.
        """

        rgb_frame = mp.Image(
            image_format=mp.ImageFormat.SRGB,
            data=frame[:, :, ::-1]
        )

        self.timestamp_ms += 1

        result = self.landmarker.detect_for_video(
            rgb_frame,
            self.timestamp_ms
        )

        detections = []

        if not result.hand_landmarks:
            return detections

        for hand_landmarks in result.hand_landmarks:

            xs = [
                landmark.x
                for landmark in hand_landmarks
            ]

            ys = [
                landmark.y
                for landmark in hand_landmarks
            ]

            min_x = max(0.0, min(xs))
            min_y = max(0.0, min(ys))

            max_x = min(1.0, max(xs))
            max_y = min(1.0, max(ys))

            width = max_x - min_x
            height = max_y - min_y

            #
            # Add padding.
            #
            pad_x = width * self.padding
            pad_y = height * self.padding

            min_x = max(
                0.0,
                min_x - pad_x
            )

            min_y = max(
                0.0,
                min_y - pad_y
            )

            max_x = min(
                1.0,
                max_x + pad_x
            )

            max_y = min(
                1.0,
                max_y + pad_y
            )

            width = max_x - min_x
            height = max_y - min_y

            detections.append(
                Detection(
                    class_id=0,
                    confidence=1.0,
                    x=min_x,
                    y=min_y,
                    width=width,
                    height=height,
                )
            )

        return detections

    def close(self):
        self.landmarker.close()
