import argparse
import glob
import time

import cv2
import psutil

from camera import Camera
from hand_tracker import HandTracker
from hand_mapping import HandMapper
from inference import EfficientDetDetector
from ipc import IPCClient, Metrics
from models import Hand, InferenceResult
from renderer import DebugRenderer
from video import VideoClient
from labels import CLASS_NAMES


def parse_args():
    parser = argparse.ArgumentParser(
        description="Gesture Control Vision Module"
    )

    parser.add_argument(
        "--debug",
        action="store_true",
        help="Show debug visualization window",
    )

    parser.add_argument(
        "--camera",
        type=int,
        default=0,
        help="Camera device index",
    )

    parser.add_argument(
        "--model",
        type=str,
        default="models/efficientdet_d0_hagrid.onnx",
        help="Path to EfficientDet ONNX model",
    )

    parser.add_argument(
        "--hand-model",
        type=str,
        default="models/hand_landmarker.task",
        help="Path to MediaPipe Hand Landmarker model",
    )

    parser.add_argument(
        "--host",
        type=str,
        default="192.168.50.1",
        help="Controller IP address",
    )

    parser.add_argument(
        "--port",
        type=int,
        default=5000,
        help="Controller TCP port",
    )

    parser.add_argument(
        "--video-port",
        type=int,
        default=5001,
        help="Video stream TCP port",
    )

    parser.add_argument(
        "--video-width",
        type=int,
        default=640,
        help="Video stream width",
    )

    parser.add_argument(
        "--video-height",
        type=int,
        default=480,
        help="Video stream height",
    )

    parser.add_argument(
        "--video-fps",
        type=float,
        default=10.0,
        help="Video stream FPS limit",
    )

    parser.add_argument(
        "--video-quality",
        type=int,
        default=60,
        help="JPEG quality",
    )

    parser.add_argument(
        "--score-thr",
        type=float,
        default=0.5,
        help="Gesture confidence threshold",
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

            temperatures.append(value / 1000.0)

        except (OSError, ValueError):
            continue

    if not temperatures:
        return 0.0

    return max(temperatures)


def main():
    args = parse_args()

    #
    # Camera
    #

    camera = Camera(
        device=args.camera,
    )

    #
    # Gesture detector
    #
    # EfficientDet receives the COMPLETE camera frame.
    # It determines which gesture is currently shown.
    #

    detector = EfficientDetDetector(
        model_path=args.model,
        confidence_threshold=args.score_thr,
    )

    #
    # Hand tracker
    #
    # MediaPipe receives the COMPLETE camera frame.
    # It is responsible only for obtaining hand landmarks.
    #
    # HandTracker now returns:
    #
    #     Hand | None
    #

    hand_tracker = HandTracker(
        model_path=args.hand_model,
    )

    #
    # Hand mapper
    #
    # Converts hand landmarks into normalized
    # coordinates in [0, 1].
    #

    hand_mapper = HandMapper()

    #
    # IPC
    #

    ipc = IPCClient(
        host=args.host,
        port=args.port,
    )

    ipc.connect()

    #
    # Video
    #

    video = VideoClient(
        host=args.host,
        port=args.video_port,
        width=args.video_width,
        height=args.video_height,
        fps=args.video_fps,
        jpeg_quality=args.video_quality,
    )

    video.connect()

    #
    # Debug renderer
    #

    renderer = None

    if args.debug:
        renderer = DebugRenderer(
            CLASS_NAMES,
        )

    #
    # FPS measurement
    #

    fps_start = time.perf_counter()
    fps_frames = 0
    fps = 0.0

    try:

        while True:

            #
            # Frame start
            #

            frame_start = time.perf_counter_ns()

            #
            # 1. Capture
            #

            capture_start = time.perf_counter_ns()

            frame = camera.read()

            capture_end = time.perf_counter_ns()

            #
            # 2. Gesture classification
            #
            # Full camera frame -> EfficientDet
            #

            inference_start = time.perf_counter_ns()

            detection_result = detector.process(
                frame
            )

            #
            # 3. Hand tracking
            #
            # Full camera frame -> MediaPipe
            #

            tracked_hand = hand_tracker.process(
                frame
            )

            inference_end = time.perf_counter_ns()

            #
            # 4. Hand mapping
            #

            postprocess_start = time.perf_counter_ns()

            hand_position = hand_mapper.process(
                tracked_hand
            )

            #
            # Convert mapped position into
            # the Hand model used by IPC.
            #

            hand = None

            if hand_position is not None:

                hand = Hand(
                    x=hand_position.x,
                    y=hand_position.y,
                )

            #
            # Build final inference result.
            #
            # Gesture:
            #     EfficientDet
            #
            # Hand position:
            #     MediaPipe + HandMapper
            #

            if hand is not None:

                ipc_result = InferenceResult(
                    hand_present=True,

                    class_id=(
                        detection_result.class_id
                    ),

                    confidence=(
                        detection_result.confidence
                    ),

                    hand=hand,
                )

            else:

                ipc_result = InferenceResult(
                    hand_present=False,

                    class_id=0xFFFFFFFF,

                    confidence=0.0,

                    hand=None,
                )

            postprocess_end = time.perf_counter_ns()

            #
            # FPS
            #

            fps_frames += 1

            fps_elapsed = (
                time.perf_counter()
                - fps_start
            )

            if fps_elapsed >= 1.0:

                fps = (
                    fps_frames
                    / fps_elapsed
                )

                fps_start = time.perf_counter()
                fps_frames = 0

            #
            # Timing
            #

            capture_time_us = (
                capture_end
                - capture_start
            ) // 1_000

            inference_time_us = (
                inference_end
                - inference_start
            ) // 1_000

            postprocess_time_us = (
                postprocess_end
                - postprocess_start
            ) // 1_000

            preprocess_time_us = 0

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
            # Total processing latency
            #

            processing_time_us = (
                capture_time_us
                + preprocess_time_us
                + inference_time_us
                + postprocess_time_us
            )

            #
            # Metrics
            #

            metrics = Metrics(
                fps=fps,

                capture_time_us=(
                    capture_time_us
                ),

                preprocess_time_us=(
                    preprocess_time_us
                ),

                inference_time_us=(
                    inference_time_us
                ),

                transfer_time_us=0,

                postprocess_time_us=(
                    postprocess_time_us
                ),

                total_latency_us=(
                    processing_time_us
                ),

                cpu_usage=cpu_usage,

                memory_usage=memory_usage,

                temperature=temperature,
            )

            #
            # 5. Send result + metrics
            #

            ipc.send(
                ipc_result,
                metrics,
            )

            #
            # 6. Video
            #

            video.send(frame)

            #
            # 7. Debug visualization
            #

            if renderer:

                renderer.draw(
                    frame,
                    ipc_result,
                )

                cv2.imshow(
                    "Gesture Control",
                    frame,
                )

                if (
                    cv2.waitKey(1) & 0xFF
                    == 27
                ):
                    break

    except KeyboardInterrupt:
        pass

    finally:

        video.close()

        ipc.close()

        hand_tracker.close()

        camera.release()

        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
