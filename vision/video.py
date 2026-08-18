import socket
import struct
import time

import cv2


class VideoClient:

    def __init__(
        self,
        host: str,
        port: int = 5001,
        width: int = 640,
        height: int = 480,
        fps: float = 15.0,
        jpeg_quality: int = 70,
    ):
        self.host = host
        self.port = port

        self.width = width
        self.height = height

        self.fps = fps
        self.jpeg_quality = jpeg_quality

        self.socket = None

        #
        # FPS limiter
        #
        self.last_send_time = 0.0


    def connect(
        self,
        retries: int = 2,
        delay: float = 0.5
    ) -> bool:

        self.close()


        for attempt in range(1, retries + 1):

            try:

                print(
                    f"Connecting video stream "
                    f"to {self.host}:{self.port}",
                    flush=True
                )


                self.socket = socket.socket(
                    socket.AF_INET,
                    socket.SOCK_STREAM
                )


                #
                # Prevent infinite blocking
                #
                self.socket.settimeout(2.0)


                self.socket.connect(
                    (
                        self.host,
                        self.port
                    )
                )


                print(
                    "Video connected",
                    flush=True
                )


                return True


            except OSError as e:

                print(
                    f"Video connection attempt "
                    f"{attempt}/{retries} failed: {e}",
                    flush=True
                )


                self.close()


                if attempt < retries:

                    time.sleep(delay)



        print(
            "Video connection failed",
            flush=True
        )


        return False



    def close(self):

        if self.socket:

            try:
                self.socket.shutdown(
                    socket.SHUT_RDWR
                )

            except OSError:
                pass


            self.socket.close()

            self.socket = None



    def send(
        self,
        frame
    ) -> bool:


        if not self.socket:
            return False


        #
        # FPS limiter
        #

        now = time.perf_counter()

        if (
            now - self.last_send_time
            <
            1.0 / self.fps
        ):
            return True


        self.last_send_time = now


        try:

            #
            # Resize preview only.
            #
            # Original frame is still used
            # by inference.
            #

            frame = cv2.resize(
                frame,
                (
                    self.width,
                    self.height
                )
            )


            success, encoded = cv2.imencode(
                ".jpg",
                frame,
                [
                    cv2.IMWRITE_JPEG_QUALITY,
                    self.jpeg_quality
                ]
            )


            if not success:
                return False



            payload = encoded.tobytes()


            header = struct.pack(
                "!I",
                len(payload)
            )


            self.socket.sendall(
                header + payload
            )


            return True



        except OSError as e:

            print(
                f"Video send failed: {e}",
                flush=True
            )

            self.close()

            return False
