import cv2

from gestures import Hand, HAND_CONNECTIONS


class DebugRenderer:
    def __init__(
        self,
        point_radius: int = 5,
        line_thickness: int = 2,
    ):
        self.point_radius = point_radius
        self.line_thickness = line_thickness


    def draw(self, frame, hands: list[Hand]):
        """
        Draw hands using OpenCV.

        Parameters:
            frame: OpenCV BGR image
            hands: list[Hand]

        Returns:
            frame with visualization
        """

        height, width, _ = frame.shape

        for hand in hands:
            points = []

            for landmark in hand.landmarks:
                x = int(landmark.x * width)
                y = int(landmark.y * height)

                points.append((x, y))

                cv2.circle(
                    frame,
                    (x, y),
                    self.point_radius,
                    (0, 255, 0),
                    -1,
                )

            for start, end in HAND_CONNECTIONS:
                cv2.line(
                    frame,
                    points[start],
                    points[end],
                    (255, 0, 0),
                    self.line_thickness,
                )

        return frame
