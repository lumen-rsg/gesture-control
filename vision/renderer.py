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
            f"id={class_id}"
        )

    def draw(
        self,
        frame,
        result: InferenceResult,
    ):
        """
        Draw inference result.

        Parameters:
            frame: OpenCV BGR image
            result: InferenceResult

        Returns:
            Annotated frame.
        """

        height, width, _ = frame.shape

        #
        # Детекции
        #
        for detection in result.detections:

            x1 = int(detection.x * width)
            y1 = int(detection.y * height)

            x2 = int((detection.x + detection.width) * width)
            y2 = int((detection.y + detection.height) * height)

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

        #
        # Классификации
        #
        y = 30

        for classification in result.classifications:

            label = (
                f"{self._class_name(classification.class_id)} "
                f"{classification.confidence:.2f}"
            )

            cv2.putText(
                frame,
                label,
                (10, y),
                cv2.FONT_HERSHEY_SIMPLEX,
                self.font_scale,
                (255, 255, 0),
                self.line_thickness,
            )

            y += 25

        return frame
