from dataclasses import dataclass


@dataclass
class Classification:
    class_id: int
    confidence: float


@dataclass
class Detection:
    class_id: int
    confidence: float

    x: float
    y: float
    width: float
    height: float

@dataclass
class InferenceResult:
    classifications: list[Classification]
    detections: list[Detection]
