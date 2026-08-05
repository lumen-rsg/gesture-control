import argparse

import cv2

from camera import Camera
from detector import HandDetector
from renderer import DebugRenderer
from ipc import FrameIPC

def parse_args():
    parser = argparse.ArgumentParser(
        description="Gesture Control Vision Module"
    )

    parser.add_argument(
        "--debug",
        action="store_true",
        help="Show debug visualization window"
    )

    parser.add_argument(
        "--camera",
        type=int,
        default=0,
        help="Camera device index"
    )

    return parser.parse_args()


def main():
    args = parse_args()

    camera = Camera(
        device=args.camera
    )

    detector = HandDetector()
    ipc = FrameIPC()

    ipc.connect()

    renderer = None

    if args.debug:
        renderer = DebugRenderer()

    try:
        while True:
            frame = camera.read()

            hands = detector.process(frame)

            ipc.send_frame(hands)
            print(hands)

            if renderer:
                renderer.draw(frame, hands)

                cv2.imshow(
                    "Gesture Control",
                    frame
                )

                if cv2.waitKey(1) & 0xFF == 27:
                    break

    except KeyboardInterrupt:
        pass

    finally:
        ipc.close()

        detector.close()
        camera.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
