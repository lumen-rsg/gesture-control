import cv2

from models import InferenceResult


class DebugRenderer:
    def __init__(
        self,
        class_names: dict[int, str] | None = None,
        font_scale: float = 0.6,
        line_thickness: int = 2,
    ):
        self.class_names = class_names or {}

        self.font_scale = font_scale
        self.line_thickness = line_thickness

    def _class_name(
        self,
        class_id: int,
    ) -> str:
        return self.class_names.get(
            class_id,
            f"id={class_id}",
        )

    def draw(
        self,
        frame,
        result: InferenceResult,
    ):
        """
        Draw EfficientDet inference results.

        Detection coordinates are expressed in pixels
        of the original camera frame.
        """

        for detection in result.detections:

            x1 = int(detection.x1)
            y1 = int(detection.y1)

            x2 = int(detection.x2)
            y2 = int(detection.y2)

            cv2.rectangle(
                frame,
                (x1, y1),
                (x2, y2),
                (0, 255, 0),
                self.line_thickness,
            )

            label = (
                f"{self._class_name(detection.class_id)} "
                f"{detection.confidence:.2f}"
            )

            cv2.putText(
                frame,
                label,
                (x1, max(20, y1 - 8)),
                cv2.FONT_HERSHEY_SIMPLEX,
                self.font_scale,
                (0, 255, 0),
                self.line_thickness,
            )

        return frame
