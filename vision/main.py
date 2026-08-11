import argparse
import glob
import time

import cv2
import psutil

from video import VideoClient
from camera import Camera
from crop import crop_detection
from hand_tracker import HandTracker
from inference import EfficientNetClassifier
from ipc import IPCClient, Metrics
from models import InferenceResult
from renderer import DebugRenderer
from labels import CLASS_NAMES


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

    parser.add_argument(
        "--model",
        type=str,
        default="models/gesture_model.onnx",
        help="Path to ONNX gesture model"
    )

    parser.add_argument(
        "--host",
        type=str,
        default="192.168.50.1",
        help="Controller IP address"
    )

    parser.add_argument(
        "--port",
        type=int,
        default=5000,
        help="Controller TCP port"
    )
    
    parser.add_argument(
        "--video-port",
        type=int,
        default=5001,
        help="Video stream TCP port"
    )

    parser.add_argument(
        "--video-width",
        type=int,
        default=640,
        help="Video stream width"
    )

    parser.add_argument(
        "--video-height",
        type=int,
        default=480,
        help="Video stream height"
    )

    parser.add_argument(
        "--video-fps",
        type=float,
        default=10.0,
        help="Video stream FPS limit"
    )

    parser.add_argument(
        "--video-quality",
        type=int,
        default=60,
        help="JPEG quality"
    )

    return parser.parse_args()


def read_temperature():
    temperatures = []

    for path in glob.glob(
        "/sys/class/thermal/thermal_zone*/temp"
    ):
        try:
            with open(path, "r") as file:
                value = int(file.read().strip())

            temperatures.append(
                value / 1000.0
            )

        except (OSError, ValueError):
            continue

    if not temperatures:
        return 0.0

    return max(temperatures)


def main():
    args = parse_args()

    camera = Camera(
        device=args.camera
    )

    tracker = HandTracker()

    classifier = EfficientNetClassifier(
        model_path=args.model
    )

    ipc = IPCClient(
        host=args.host,
        port=args.port,
    )

    ipc.connect()

    video = VideoClient(
        host=args.host,
        port=args.video_port,
        width=args.video_width,
        height=args.video_height,
        fps=args.video_fps,
        jpeg_quality=args.video_quality,
    )

    video.connect()

    renderer = None

    if args.debug:
        renderer = DebugRenderer(
            CLASS_NAMES
        )

    #
    # FPS measurement.
    #
    fps_start = time.perf_counter()
    fps_frames = 0

    try:
        while True:

            frame_start = time.perf_counter_ns()

            #
            # 1. Capture
            #
            capture_start = time.perf_counter_ns()

            frame = camera.read()

            capture_end = time.perf_counter_ns()


            #
            # 2. Hand tracking + preprocessing
            #
            preprocess_start = time.perf_counter_ns()

            detections = tracker.process(
                frame
            )

            crop = None

            if detections:
                crop = crop_detection(
                    frame,
                    detections[0]
                )

            preprocess_end = time.perf_counter_ns()


            #
            # 3. Prepare result
            #
            result = InferenceResult(
                classifications=[],
                detections=detections,
            )


            #
            # 4. Neural network inference
            #
            inference_start = time.perf_counter_ns()

            if crop is not None:

                classification_result = (
                    classifier.process(crop)
                )

                result.classifications = (
                    classification_result.classifications
                )

            inference_end = time.perf_counter_ns()


            #
            # 5. Postprocess
            #
            postprocess_start = time.perf_counter_ns()

            #
            # Result is already prepared.
            #
            # Keep this stage for future processing:
            #
            # - confidence filtering
            # - gesture smoothing
            # - temporal filtering
            #

            postprocess_end = time.perf_counter_ns()


            #
            # FPS
            #
            fps_frames += 1

            fps_elapsed = (
                time.perf_counter() - fps_start
            )

            if fps_elapsed >= 1.0:

                fps = (
                    fps_frames / fps_elapsed
                )

                fps_start = time.perf_counter()
                fps_frames = 0

            #
            # Keep the previous FPS value
            # during the current one-second window.
            #
            #else:
                #fps = 0.0


            #
            # Timing
            #
            capture_time_us = (
                capture_end - capture_start
            ) // 1_000

            preprocess_time_us = (
                preprocess_end - preprocess_start
            ) // 1_000

            inference_time_us = (
                inference_end - inference_start
            ) // 1_000

            postprocess_time_us = (
                postprocess_end - postprocess_start
            ) // 1_000


            #
            # System metrics
            #
            cpu_usage = psutil.cpu_percent(
                interval=None
            )

            memory_usage = (
                psutil.virtual_memory().percent
            )

            temperature = read_temperature()


            #
            # Total latency up to the point
            # before network transmission.
            #
            processing_time_us = (
                capture_time_us
                + preprocess_time_us
                + inference_time_us
                + postprocess_time_us
            )


            metrics = Metrics(
                fps=fps,

                capture_time_us=capture_time_us,
                preprocess_time_us=preprocess_time_us,
                inference_time_us=inference_time_us,

                transfer_time_us=0,

                postprocess_time_us=postprocess_time_us,

                total_latency_us=processing_time_us,

                cpu_usage=cpu_usage,
                memory_usage=memory_usage,
                temperature=temperature,
            )


            #
            # 6. Send result + metrics
            #
            ipc.send( result, metrics)
            
            video.send(frame)


            #
            # 7. Debug visualization
            #
            if renderer:

                renderer.draw(
                    frame,
                    result
                )

                cv2.imshow(
                    "Gesture Control",
                    frame
                )

                if cv2.waitKey(1) & 0xFF == 27:
                    break


    except KeyboardInterrupt:
        pass

    finally:
        video.close()
        ipc.close()

        tracker.close()
        camera.release()

        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
