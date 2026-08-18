from pathlib import Path

import cv2
import numpy as np
import onnxruntime as ort
import torch
from torchvision.ops import batched_nms

from models import Detection, InferenceResult


class EfficientDetDetector:
    """
    EfficientDet-D0 gesture detector running through ONNX Runtime.

    The detector receives the complete camera frame.

    It is responsible only for gesture classification.
    Hand position is obtained separately by HandTracker
    and HandMapper.
    """

    IMAGE_SIZE = 512

    NUM_CLASSES = 4
    NUM_ANCHORS = 9

    IOU_THRESHOLD = 0.5
    SCORE_THRESHOLD = 0.5
    MAX_DETECTIONS = 100

    #
    # ImageNet normalization.
    #

    MEAN = np.array(
        [0.485, 0.456, 0.406],
        dtype=np.float32,
    )

    STD = np.array(
        [0.229, 0.224, 0.225],
        dtype=np.float32,
    )

    #
    # EfficientDet-D0 feature levels.
    #

    LEVELS = (
        (64, 64),
        (32, 32),
        (16, 16),
        (8, 8),
        (4, 4),
    )

    MIN_LEVEL = 3
    ANCHOR_SCALE = 4.0

    #
    # Three aspect ratios × three scales = 9 anchors.
    #

    ASPECT_RATIOS = (
        (1.0, 1.0),
        (1.4, 0.7),
        (0.7, 1.4),
    )

    SCALE_FACTORS = (
        1.0,
        2 ** (1.0 / 3.0),
        2 ** (2.0 / 3.0),
    )

    def __init__(
        self,
        model_path: str | Path,
        confidence_threshold: float = SCORE_THRESHOLD,
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
                "CPUExecutionProvider",
            ],
        )

        inputs = self.session.get_inputs()

        if len(inputs) != 1:
            raise RuntimeError(
                f"Expected one ONNX input, got {len(inputs)}"
            )

        self.input_name = inputs[0].name
        self.input_shape = inputs[0].shape

        outputs = self.session.get_outputs()

        if len(outputs) != 10:
            raise RuntimeError(
                "Expected 10 EfficientDet outputs "
                f"(5 classification + 5 box), got {len(outputs)}"
            )

        self.output_names = [
            output.name
            for output in outputs
        ]

        self.anchors = self._generate_anchors()

        expected_anchor_count = sum(
            height * width
            for height, width in self.LEVELS
        ) * self.NUM_ANCHORS

        if self.anchors.shape[0] != expected_anchor_count:
            raise RuntimeError(
                "Unexpected number of generated anchors: "
                f"{self.anchors.shape[0]} "
                f"(expected {expected_anchor_count})"
            )

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def process(
        self,
        frame: np.ndarray,
    ) -> InferenceResult:
        """
        Run EfficientDet inference on the complete camera frame.

        The frame is NOT cropped.

        Returns:
            InferenceResult containing only gesture
            classification.

            Hand position is filled later by main.py.
        """

        original_height, original_width = frame.shape[:2]

        input_tensor, scale, pad_x, pad_y = (
            self._preprocess(frame)
        )

        raw_outputs = self.session.run(
            self.output_names,
            {
                self.input_name: input_tensor,
            },
        )

        cls_outputs, box_outputs = (
            self._merge_outputs(raw_outputs)
        )

        detections = self._postprocess(
            cls_outputs,
            box_outputs,
            original_width=original_width,
            original_height=original_height,
            scale=scale,
            pad_x=pad_x,
            pad_y=pad_y,
        )

        #
        # No valid gesture detected.
        #

        if not detections:
            return InferenceResult(
                hand_present=False,
                class_id=0xFFFFFFFF,
                confidence=0.0,
                hand=None,
            )

        #
        # EfficientDet detections are already sorted
        # by NMS score sufficiently for our use case.
        #
        # Select the highest-confidence detection.
        #

        best_detection = max(
            detections,
            key=lambda detection: detection.confidence,
        )

        return InferenceResult(
            hand_present=False,
            class_id=best_detection.class_id,
            confidence=best_detection.confidence,
            hand=None,
        )

    # ------------------------------------------------------------------
    # Preprocessing
    # ------------------------------------------------------------------

    def _preprocess(
        self,
        frame: np.ndarray,
    ):
        """
        Resize the complete frame while preserving aspect ratio,
        then pad it to 512x512.

        Padding is added to the bottom/right.
        """

        original_height, original_width = frame.shape[:2]

        scale = min(
            self.IMAGE_SIZE / original_width,
            self.IMAGE_SIZE / original_height,
        )

        resized_width = int(
            round(original_width * scale)
        )

        resized_height = int(
            round(original_height * scale)
        )

        image = cv2.cvtColor(
            frame,
            cv2.COLOR_BGR2RGB,
        )

        image = cv2.resize(
            image,
            (resized_width, resized_height),
            interpolation=cv2.INTER_LINEAR,
        )

        pad_x = self.IMAGE_SIZE - resized_width
        pad_y = self.IMAGE_SIZE - resized_height

        image = cv2.copyMakeBorder(
            image,
            0,
            pad_y,
            0,
            pad_x,
            cv2.BORDER_CONSTANT,
            value=(
                123.675,
                116.28,
                103.53,
            ),
        )

        image = image.astype(
            np.float32
        ) / 255.0

        image = (
            image - self.MEAN
        ) / self.STD

        #
        # HWC -> CHW
        #

        image = np.transpose(
            image,
            (2, 0, 1),
        )

        #
        # CHW -> NCHW
        #

        image = np.expand_dims(
            image,
            axis=0,
        )

        return (
            image.astype(np.float32),
            scale,
            0.0,
            0.0,
        )

    # ------------------------------------------------------------------
    # ONNX output processing
    # ------------------------------------------------------------------

    def _merge_outputs(
        self,
        raw_outputs,
    ):
        """
        Convert EfficientDet feature-level outputs into:

            classifications:
                [batch, anchors, classes]

            box regressions:
                [batch, anchors, 4]

        ONNX output order:

            0..4 -> classification
            5..9 -> box regression
        """

        cls_outputs = raw_outputs[:5]
        box_outputs = raw_outputs[5:]

        cls_levels = []
        box_levels = []

        for output in cls_outputs:
            (
                batch_size,
                channels,
                height,
                width,
            ) = output.shape

            expected_channels = (
                self.NUM_ANCHORS
                * self.NUM_CLASSES
            )

            if channels != expected_channels:
                raise RuntimeError(
                    "Unexpected classification output shape: "
                    f"{output.shape}; expected "
                    f"{expected_channels} channels"
                )

            output = np.transpose(
                output,
                (0, 2, 3, 1),
            )

            output = output.reshape(
                batch_size,
                height * width * self.NUM_ANCHORS,
                self.NUM_CLASSES,
            )

            cls_levels.append(output)

        for output in box_outputs:
            (
                batch_size,
                channels,
                height,
                width,
            ) = output.shape

            expected_channels = (
                self.NUM_ANCHORS * 4
            )

            if channels != expected_channels:
                raise RuntimeError(
                    "Unexpected box output shape: "
                    f"{output.shape}; expected "
                    f"{expected_channels} channels"
                )

            output = np.transpose(
                output,
                (0, 2, 3, 1),
            )

            output = output.reshape(
                batch_size,
                height * width * self.NUM_ANCHORS,
                4,
            )

            box_levels.append(output)

        return (
            np.concatenate(
                cls_levels,
                axis=1,
            ),
            np.concatenate(
                box_levels,
                axis=1,
            ),
        )

    # ------------------------------------------------------------------
    # Anchors
    # ------------------------------------------------------------------

    def _generate_anchors(self) -> np.ndarray:
        """
        Generate EfficientDet-D0 anchors.

        Returns:
            [N, 4] array containing:

                x1, y1, x2, y2

            in 512x512 model coordinates.
        """

        anchors = []

        for level_index, (height, width) in enumerate(
            self.LEVELS
        ):
            level = self.MIN_LEVEL + level_index

            stride = 2 ** level

            base_size = (
                self.ANCHOR_SCALE * stride
            )

            for y in range(height):
                for x in range(width):

                    center_x = (
                        (x + 0.5) * stride
                    )

                    center_y = (
                        (y + 0.5) * stride
                    )

                    for ratio_width, ratio_height in (
                        self.ASPECT_RATIOS
                    ):
                        for scale_factor in (
                            self.SCALE_FACTORS
                        ):

                            anchor_width = (
                                base_size
                                * scale_factor
                                * ratio_width
                            )

                            anchor_height = (
                                base_size
                                * scale_factor
                                * ratio_height
                            )

                            x1 = (
                                center_x
                                - anchor_width / 2.0
                            )

                            y1 = (
                                center_y
                                - anchor_height / 2.0
                            )

                            x2 = (
                                center_x
                                + anchor_width / 2.0
                            )

                            y2 = (
                                center_y
                                + anchor_height / 2.0
                            )

                            anchors.append(
                                (
                                    x1,
                                    y1,
                                    x2,
                                    y2,
                                )
                            )

        return np.asarray(
            anchors,
            dtype=np.float32,
        )

    # ------------------------------------------------------------------
    # Postprocessing
    # ------------------------------------------------------------------

    def _postprocess(
        self,
        cls_outputs: np.ndarray,
        box_outputs: np.ndarray,
        original_width: int,
        original_height: int,
        scale: float,
        pad_x: float,
        pad_y: float,
    ) -> list[Detection]:
        """
        Decode EfficientDet outputs and perform NMS.
        """

        cls_tensor = torch.from_numpy(
            cls_outputs[0]
        ).float()

        box_tensor = torch.from_numpy(
            box_outputs[0]
        ).float()

        anchors = torch.from_numpy(
            self.anchors
        ).float()

        if cls_tensor.shape[0] != anchors.shape[0]:
            raise RuntimeError(
                "Classification output contains "
                f"{cls_tensor.shape[0]} anchors, but "
                f"{anchors.shape[0]} anchors were generated."
            )

        if box_tensor.shape[0] != anchors.shape[0]:
            raise RuntimeError(
                "Box output contains "
                f"{box_tensor.shape[0]} anchors, but "
                f"{anchors.shape[0]} anchors were generated."
            )

        #
        # Decode box regression.
        #

        boxes = self._decode_boxes(
            box_tensor,
            anchors,
        )

        #
        # Convert logits to probabilities.
        #

        scores = torch.sigmoid(
            cls_tensor
        )

        class_scores, class_ids = scores.max(
            dim=1
        )

        #
        # EfficientDet labels are 1-based.
        #

        class_ids = class_ids + 1

        #
        # Confidence threshold.
        #

        score_mask = (
            class_scores
            >= self.confidence_threshold
        )

        if not torch.any(score_mask):
            return []

        boxes = boxes[score_mask]
        class_scores = class_scores[score_mask]
        class_ids = class_ids[score_mask]

        #
        # NMS.
        #

        keep = batched_nms(
            boxes,
            class_scores,
            class_ids,
            self.IOU_THRESHOLD,
        )

        keep = keep[:self.MAX_DETECTIONS]

        boxes = boxes[keep]
        class_scores = class_scores[keep]
        class_ids = class_ids[keep]

        detections = []

        for box, score, class_id in zip(
            boxes,
            class_scores,
            class_ids,
        ):
            x1, y1, x2, y2 = (
                box.tolist()
            )

            #
            # Remove padding.
            #

            x1 -= pad_x
            x2 -= pad_x

            y1 -= pad_y
            y2 -= pad_y

            #
            # Convert model coordinates back
            # to original camera coordinates.
            #

            x1 /= scale
            x2 /= scale

            y1 /= scale
            y2 /= scale

            #
            # Clamp to original image.
            #

            x1 = max(
                0.0,
                min(float(original_width), x1),
            )

            y1 = max(
                0.0,
                min(float(original_height), y1),
            )

            x2 = max(
                0.0,
                min(float(original_width), x2),
            )

            y2 = max(
                0.0,
                min(float(original_height), y2),
            )

            if x2 <= x1 or y2 <= y1:
                continue

            detections.append(
                Detection(
                    class_id=int(class_id),
                    confidence=float(score),
                    x1=x1,
                    y1=y1,
                    x2=x2,
                    y2=y2,
                )
            )

        #
        # Debug information.
        #

        print(
            "Candidates after NMS:",
            len(detections),
        )

        for detection in detections[:20]:
            print(
                "NMS:",
                "class=",
                detection.class_id,
                "score=",
                detection.confidence,
                "box=",
                (
                    detection.x1,
                    detection.y1,
                    detection.x2,
                    detection.y2,
                ),
            )

        return detections

    # ------------------------------------------------------------------
    # Box decoding
    # ------------------------------------------------------------------

    @staticmethod
    def _decode_boxes(
        box_outputs: torch.Tensor,
        anchors: torch.Tensor,
    ) -> torch.Tensor:
        """
        Decode EfficientDet box regression outputs.

        box_outputs:
            [N, 4] = dy, dx, dh, dw

        anchors:
            [N, 4] = x1, y1, x2, y2
        """

        anchor_x1 = anchors[:, 0]
        anchor_y1 = anchors[:, 1]
        anchor_x2 = anchors[:, 2]
        anchor_y2 = anchors[:, 3]

        anchor_width = (
            anchor_x2 - anchor_x1
        )

        anchor_height = (
            anchor_y2 - anchor_y1
        )

        anchor_center_x = (
            anchor_x1
            + anchor_width * 0.5
        )

        anchor_center_y = (
            anchor_y1
            + anchor_height * 0.5
        )

        dy = box_outputs[:, 0]
        dx = box_outputs[:, 1]
        dh = box_outputs[:, 2]
        dw = box_outputs[:, 3]

        #
        # EfficientDet box variance.
        #

        center_y = (
            dy * 0.1 * anchor_height
            + anchor_center_y
        )

        center_x = (
            dx * 0.1 * anchor_width
            + anchor_center_x
        )

        height = (
            torch.exp(
                dh * 0.2
            )
            * anchor_height
        )

        width = (
            torch.exp(
                dw * 0.2
            )
            * anchor_width
        )

        x1 = (
            center_x
            - width * 0.5
        )

        y1 = (
            center_y
            - height * 0.5
        )

        x2 = (
            center_x
            + width * 0.5
        )

        y2 = (
            center_y
            + height * 0.5
        )

        return torch.stack(
            [
                x1,
                y1,
                x2,
                y2,
            ],
            dim=1,
        )
