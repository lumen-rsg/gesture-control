import socket
import struct
import time
from dataclasses import dataclass

from models import InferenceResult


PROTOCOL_VERSION = 3

MESSAGE_RESULT = 0
MESSAGE_METRICS = 1


#
# Message header:
#
# uint32 protocol_version
# uint8  message_type
# uint8  reserved
# uint16 reserved
# uint32 payload_size
#

HEADER_FORMAT = "<IBBHI"

HEADER_SIZE = struct.calcsize(
    HEADER_FORMAT
)


#
# RESULT:
#
# uint64 frame_id
# uint64 timestamp_ms
# uint8  hand_present
# 3 bytes padding
#
# uint32 class_id
# float  confidence
#
# float hand_x
# float hand_y
# float hand_width
# float hand_height
#

RESULT_FORMAT = "<QQB3xI5f"

RESULT_SIZE = struct.calcsize(
    RESULT_FORMAT
)

assert RESULT_SIZE == 44


#
# METRICS:
#
# uint64 frame_id
# uint64 timestamp_ms
# float  fps
#
# uint32 capture_time_us
# uint32 preprocess_time_us
# uint32 inference_time_us
# uint32 transfer_time_us
# uint32 postprocess_time_us
# uint32 total_latency_us
#
# float cpu_usage
# float memory_usage
# float temperature
#

METRICS_FORMAT = "<QQfIIIIIIfff"

METRICS_SIZE = struct.calcsize(
    METRICS_FORMAT
)

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
            socket.SOCK_STREAM,
        )

        self.socket.connect(
            (
                self.host,
                self.port,
            )
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

        timestamp_ms = (
            time.time_ns()
            // 1_000_000
        )

        #
        # RESULT
        #

        transfer_start = (
            time.perf_counter_ns()
        )

        self.send_result(
            result,
            frame_id,
            timestamp_ms,
        )

        transfer_end = (
            time.perf_counter_ns()
        )

        metrics.transfer_time_us = (
            transfer_end
            - transfer_start
        ) // 1_000

        #
        # METRICS
        #

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
        #
        # Hand presence.
        #

        hand_present = (
            1
            if result.hand_present
            else 0
        )

        #
        # Classification.
        #

        if result.hand_present:
            class_id = result.class_id
            confidence = result.confidence
        else:
            class_id = 0xFFFFFFFF
            confidence = 0.0

        #
        # Hand position.
        #

        hand_x = 0.0
        hand_y = 0.0

        if result.hand is not None:
            hand_x = result.hand.x
            hand_y = result.hand.y

        #
        # We no longer use a bounding box for
        # cursor positioning.
        #
        # Keep width/height at zero because the
        # current C++ protocol still contains these
        # four fields.
        #

        hand_width = 0.0
        hand_height = 0.0

        payload = struct.pack(
            RESULT_FORMAT,

            frame_id,
            timestamp_ms,

            hand_present,

            class_id,
            confidence,

            hand_x,
            hand_y,
            hand_width,
            hand_height,
        )

        self.send_message(
            MESSAGE_RESULT,
            payload,
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
            payload,
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
