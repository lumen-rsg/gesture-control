import cv2

from models import Detection


def crop_detection(
    frame,
    detection: Detection,
    padding: float = 0.0,
):
    """
    Crop a detection from an OpenCV frame.

    Detection coordinates are expected to be normalized
    to [0, 1].

    Returns:
        Cropped BGR image, or None if the box is invalid.
    """

    height, width = frame.shape[:2]

    x = detection.x
    y = detection.y
    box_width = detection.width
    box_height = detection.height

    if box_width <= 0 or box_height <= 0:
        return None

    #
    # Optional additional padding.
    #
    pad_x = box_width * padding
    pad_y = box_height * padding

    x1 = max(
        0.0,
        x - pad_x
    )

    y1 = max(
        0.0,
        y - pad_y
    )

    x2 = min(
        1.0,
        x + box_width + pad_x
    )

    y2 = min(
        1.0,
        y + box_height + pad_y
    )

    #
    # Convert normalized coordinates
    # to pixel coordinates.
    #
    px1 = int(x1 * width)
    py1 = int(y1 * height)

    px2 = int(x2 * width)
    py2 = int(y2 * height)

    if px2 <= px1 or py2 <= py1:
        return None

    return frame[
        py1:py2,
        px1:px2
    ]
