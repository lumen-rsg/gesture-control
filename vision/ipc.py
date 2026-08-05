import socket
import struct
import time

from gestures import Hand

PROTOCOL_VERSION = 1

LANDMARK_COUNT = 21
MAX_HANDS = 2


class FrameIPC:
    HEADER_FORMAT = "<IQB"
    LANDMARK_FORMAT = "<fff"

    def __init__(self, socket_path="/tmp/gesture.sock"):
        self.socket_path = socket_path
        self.socket = None

    def connect(self):
        self.socket = socket.socket(
            socket.AF_UNIX,
            socket.SOCK_STREAM
        )

        self.socket.connect(self.socket_path)

    def send_frame(self, hands: list[Hand]):
        if self.socket is None:
            raise RuntimeError("IPC is not connected")

        packet = bytearray()

        # Protocol version
        packet += struct.pack(
            self.HEADER_FORMAT,
            PROTOCOL_VERSION,
            int(time.monotonic_ns() // 1_000_000),
            len(hands)
        )

        # Всегда отправляем MAX_HANDS рук
        for hand_index in range(MAX_HANDS):

            if hand_index < len(hands):
                hand = hands[hand_index]

                for landmark in hand.landmarks:
                    packet += struct.pack(
                        self.LANDMARK_FORMAT,
                        landmark.x,
                        landmark.y,
                        landmark.z
                    )

            else:
                # Пустая рука
                for _ in range(LANDMARK_COUNT):
                    packet += struct.pack(
                        self.LANDMARK_FORMAT,
                        0.0,
                        0.0,
                        0.0
                    )

        self.socket.sendall(packet)

    def close(self):
        if self.socket:
            self.socket.close()
            self.socket = None
