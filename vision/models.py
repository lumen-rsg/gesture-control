from dataclasses import dataclass


@dataclass
class Detection:
    """
    Single EfficientDet detection.

    Coordinates are expressed in pixels of the
    original camera frame.
    """

    class_id: int
    confidence: float

    x1: float
    y1: float
    x2: float
    y2: float


@dataclass
class Hand:
    """
    Mapped hand position.

    Coordinates are normalized to [0, 1].
    """

    x: float
    y: float


@dataclass
class InferenceResult:
    """
    Combined result of gesture recognition and
    hand tracking for a single frame.
    """

    hand_present: bool

    class_id: int
    confidence: float

    hand: Hand | None
