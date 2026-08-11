from pathlib import Path

import cv2
import numpy as np
import onnxruntime as ort

from models import (
    Classification,
    InferenceResult,
)


class EfficientNetClassifier:
    """
    EfficientNet image classifier running through ONNX Runtime.

    Input:
        OpenCV BGR image.

    Output:
        InferenceResult containing the most confident classification.
    """

    IMAGE_SIZE = 224

    # ImageNet normalization.
    MEAN = np.array(
        [0.485, 0.456, 0.406],
        dtype=np.float32,
    )

    STD = np.array(
        [0.229, 0.224, 0.225],
        dtype=np.float32,
    )

    def __init__(
        self,
        model_path: str | Path,
        confidence_threshold: float = 0.0,
    ):
        self.model_path = Path(model_path)
        self.confidence_threshold = confidence_threshold

        if not self.model_path.exists():
            raise FileNotFoundError(
                f"Model not found: {self.model_path}"
            )

        self.session = ort.InferenceSession(
            str(self.model_path),
            providers=[
                "CPUExecutionProvider"
            ],
        )

        self.input_name = (
            self.session.get_inputs()[0].name
        )

        self.output_name = (
            self.session.get_outputs()[0].name
        )

    def process(
        self,
        frame: np.ndarray,
    ) -> InferenceResult:
        """
        Run inference on an OpenCV BGR frame.
        """

        input_tensor = self._preprocess(
            frame
        )

        outputs = self.session.run(
            [self.output_name],
            {
                self.input_name: input_tensor
            },
        )

        logits = outputs[0][0]

        probabilities = self._softmax(
            logits
        )

        class_id = int(
            np.argmax(probabilities)
        )

        confidence = float(
            probabilities[class_id]
        )

        classifications = []

        if confidence >= self.confidence_threshold:
            classifications.append(
                Classification(
                    class_id=class_id,
                    confidence=confidence,
                )
            )

        return InferenceResult(
            classifications=classifications,
            detections=[],
        )

    def _preprocess(
        self,
        frame: np.ndarray,
    ) -> np.ndarray:
        """
        Convert OpenCV BGR image into EfficientNet input tensor.
        """

        image = cv2.cvtColor(
            frame,
            cv2.COLOR_BGR2RGB,
        )

        image = cv2.resize(
            image,
            (
                self.IMAGE_SIZE,
                self.IMAGE_SIZE,
            ),
            interpolation=cv2.INTER_LINEAR,
        )

        image = image.astype(
            np.float32
        ) / 255.0

        image = (
            image - self.MEAN
        ) / self.STD

        # HWC -> CHW
        image = np.transpose(
            image,
            (2, 0, 1),
        )

        # Add batch dimension:
        # CHW -> NCHW
        image = np.expand_dims(
            image,
            axis=0,
        )

        return image.astype(
            np.float32
        )

    @staticmethod
    def _softmax(
        logits: np.ndarray,
    ) -> np.ndarray:
        """
        Convert model logits to probabilities.
        """

        logits = logits - np.max(
            logits
        )

        exp = np.exp(logits)

        return exp / np.sum(exp)
