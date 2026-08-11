import socket
import struct
import time
from dataclasses import dataclass

from models import InferenceResult


PROTOCOL_VERSION = 2

MESSAGE_RESULT = 0
MESSAGE_METRICS = 1

NO_CLASSIFICATION = 0xFFFFFFFF

HEADER_FORMAT = "<IBBHI"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

RESULT_FORMAT = "<QQB3xIfffff"
RESULT_SIZE = struct.calcsize(RESULT_FORMAT)

assert RESULT_SIZE == 44

METRICS_FORMAT = "<QQfIIIIIIfff"
METRICS_SIZE = struct.calcsize(METRICS_FORMAT)

assert METRICS_SIZE == 56

@dataclass
class Metrics:
    fps: float = 0.0

    capture_time_us: int = 0
    preprocess_time_us: int = 0
    inference_time_us: int = 0
    transfer_time_us: int = 0
    postprocess_time_us: int = 0
    total_latency_us: int = 0

    cpu_usage: float = 0.0
    memory_usage: float = 0.0
    temperature: float = 0.0


class IPCClient:
    def __init__(
        self,
        host: str = "192.168.50.1",
        port: int = 5000,
    ):
        self.host = host
        self.port = port

        self.socket = None
        self.frame_id = 0

    def connect(self):
        self.socket = socket.socket(
            socket.AF_INET,
            socket.SOCK_STREAM
        )

        self.socket.connect(
            (self.host, self.port)
        )

    def close(self):
        if self.socket:
            self.socket.close()
            self.socket = None

    def send(
        self,
        result: InferenceResult,
        metrics: Metrics,
    ):
        if not self.socket:
            raise RuntimeError(
                "IPC socket is not connected"
            )

        self.frame_id += 1

        frame_id = self.frame_id
        timestamp_ms = time.time_ns() // 1_000_000

        transfer_start = time.perf_counter_ns()

        self.send_result(
            result,
            frame_id,
            timestamp_ms,
        )

        transfer_end = time.perf_counter_ns()

        metrics.transfer_time_us = (
            transfer_end - transfer_start
        ) // 1_000

        self.send_metrics(
            metrics,
            frame_id,
            timestamp_ms,
        )

    def send_result(
        self,
        result: InferenceResult,
        frame_id: int,
        timestamp_ms: int,
    ):
        hand_present = 0

        class_id = NO_CLASSIFICATION
        confidence = 0.0

        x = 0.0
        y = 0.0
        width = 0.0
        height = 0.0

        if result.classifications:
            classification = result.classifications[0]

            class_id = classification.class_id
            confidence = classification.confidence

        if result.detections:
            detection = result.detections[0]

            hand_present = 1

            x = detection.x
            y = detection.y
            width = detection.width
            height = detection.height

        payload = struct.pack(
            RESULT_FORMAT,

            frame_id,
            timestamp_ms,

            hand_present,

            class_id,
            confidence,

            x,
            y,
            width,
            height,
        )

        self.send_message(
            MESSAGE_RESULT,
            payload
        )

    def send_metrics(
        self,
        metrics: Metrics,
        frame_id: int,
        timestamp_ms: int,
    ):
        payload = struct.pack(
            METRICS_FORMAT,

            frame_id,
            timestamp_ms,

            metrics.fps,

            metrics.capture_time_us,
            metrics.preprocess_time_us,
            metrics.inference_time_us,
            metrics.transfer_time_us,
            metrics.postprocess_time_us,
            metrics.total_latency_us,

            metrics.cpu_usage,
            metrics.memory_usage,
            metrics.temperature,
        )

        self.send_message(
            MESSAGE_METRICS,
            payload
        )

    def send_message(
        self,
        message_type: int,
        payload: bytes,
    ):
        header = struct.pack(
            HEADER_FORMAT,

            PROTOCOL_VERSION,
            message_type,

            0,
            0,

            len(payload),
        )

        self.socket.sendall(
            header + payload
        )
