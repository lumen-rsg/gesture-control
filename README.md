# Gesture Control

A distributed computer control system using computer vision and neural network-based hand gesture recognition.

The project is designed as a prototype for a **neuromorphic AI demonstrator**. The vision and neural network inference pipeline runs on a separate Linux-based AArch64 device, while the PC receives inference results and performance metrics over Ethernet.

The project is divided into two independent modules:

* **Vision module (Python)** — camera capture, gesture detection, hand landmark tracking, neural network inference and performance measurement.
* **Controller module (C++)** — receiving inference results, interpreting gestures, controlling the computer and providing a visualization layer.

---

# Architecture

The system is designed to run on two separate devices connected via Ethernet.

```text
┌──────────────────────────────────────┐
│            Vision Device             │
│             Linux / AArch64          │
│                                      │
│  Camera                              │
│    │                                 │
│    ├───────────────┐                 │
│    │               │                 │
│    ▼               ▼                 │
│ EfficientDet   Hand Landmarker       │
│    │               │                 │
│    ▼               ▼                 │
│ Gesture         21 Hand              │
│ Classification  Landmarks            │
│    │               │                 │
│    │               ▼                 │
│    │           Hand Mapper           │
│    │               │                 │
│    └───────┬───────┘                 │
│            ▼                         │
│     Inference Result                 │
│            │                         │
│            ├── Gesture               │
│            ├── Confidence            │
│            └── Hand Position         │
│                                      │
│     Performance Metrics              │
│            │                         │
└────────────┼─────────────────────────┘
             │
             │ Ethernet / TCP
             │
┌────────────▼─────────────────────────┐
│                 PC                   │
│                                      │
│            IPC Server                │
│                │                     │
│        ┌───────┴────────┐            │
│        │                │            │
│     Result           Metrics        │
│        │                │            │
│        ▼                ▼            │
│ GestureRecognizer   Dashboard       │
│        │             (ImGui)         │
│        ▼                             │
│ InputController                      │
│        │                             │
│        ▼                             │
│     Linux uinput                     │
└──────────────────────────────────────┘
```

The vision device does not require a desktop environment. It can operate as a headless Linux system connected to a camera.

The PC is responsible for user interaction, input control and visualization.

---

# Project structure

```text
.
├── build
│
├── config
│   └── config.json              # Controller configuration
│
├── controller                   # C++ controller module
│   ├── CMakeLists.txt
│   ├── main.cpp
│   │
│   ├── include
│   │   ├── config.hpp
│   │   ├── gesture_state.hpp
│   │   ├── input.hpp
│   │   ├── ipc.hpp
│   │   ├── json.hpp
│   │   ├── recognizer.hpp
│   │   └── protocol
│   │       ├── inference.hpp
│   │       ├── protocol.hpp
│   │       ├── protocol.md
│   │       └── serialization.md
│   │
│   └── src
│       ├── config.cpp
│       ├── input.cpp
│       ├── ipc.cpp
│       └── recognizer.cpp
│
└── vision                       # Python vision module
    ├── camera.py
    ├── hand_mapping.py
    ├── hand_tracker.py
    ├── inference.py
    ├── ipc.py
    ├── labels.py
    ├── main.py
    ├── models.py
    ├── renderer.py
    ├── video.py
    ├── requirements.txt
    │
    └── models
        ├── efficientdet_d0_hagrid.onnx
        └── hand_landmarker.task
```

---

# Vision module

## Purpose

The Vision module is responsible for the complete computer vision and inference pipeline.

It performs:

* camera image capture;
* gesture detection using EfficientDet;
* hand landmark tracking;
* palm position calculation;
* image preprocessing;
* neural network inference;
* gesture classification;
* performance measurement;
* transmission of inference results and metrics to the controller.

The vision module does not:

* control the mouse;
* interact with the operating system input subsystem;
* interpret gestures as computer actions.

---

## Processing pipeline

The current vision pipeline uses two independent components.

### Gesture recognition

The complete camera frame is passed to EfficientDet-D0.

```text
Camera frame
     │
     ▼
Preprocessing
     │
     ▼
EfficientDet-D0
     │
     ▼
Detection decoding
     │
     ▼
Confidence filtering
     │
     ▼
NMS
     │
     ▼
Best gesture detection
     │
     ├── class_id
     └── confidence
```

EfficientDet is responsible only for recognizing the gesture.

It does not provide the final cursor position.

### Hand position tracking

The same complete camera frame is independently passed to MediaPipe Hand Landmarker.

```text
Camera frame
     │
     ▼
MediaPipe Hand Landmarker
     │
     ▼
21 hand landmarks
     │
     ▼
HandMapper
     │
     ▼
Palm center
     │
     ├── x ∈ [0, 1]
     └── y ∈ [0, 1]
```

The palm position is calculated from the following landmarks:

```text
0   Wrist
5   Index finger MCP
9   Middle finger MCP
13  Ring finger MCP
17  Little finger MCP
```

The average position of these landmarks is used as the cursor position.

This makes the position relatively independent from finger extension and the current gesture shape.

---

## Combined inference result

The two processing branches are combined in `main.py`.

```text
                    Camera frame
                         │
              ┌──────────┴──────────┐
              │                     │
              ▼                     ▼
        EfficientDet          Hand Landmarker
              │                     │
              ▼                     ▼
       Gesture result          Landmarks
              │                     │
              │                     ▼
              │                 HandMapper
              │                     │
              └──────────┬──────────┘
                         ▼
                  InferenceResult
                         │
                         ▼
                       TCP
```

The Python model is defined as:

```python
@dataclass
class InferenceResult:
    hand_present: bool

    class_id: int
    confidence: float

    hand: Hand | None
```

The hand position is represented by:

```python
@dataclass
class Hand:
    x: float
    y: float
```

Coordinates are normalized to the range `[0, 1]`.

The detector itself does not return the hand position. It returns the best detected gesture, while `HandTracker` and `HandMapper` provide the hand position.

---

# EfficientDet inference

The current gesture detector is based on **EfficientDet-D0** and is exported to ONNX.

The ONNX model expects:

```text
Input:
[1, 3, 512, 512]
```

The input frame is:

1. converted from BGR to RGB;
2. resized while preserving the aspect ratio;
3. padded to `512 × 512`;
4. normalized using ImageNet mean and standard deviation;
5. converted from HWC to NCHW format.

ONNX Runtime is used for inference.

The detector processes the complete camera frame rather than a previously cropped hand image.

---

## Detection postprocessing

The EfficientDet output consists of five feature levels for classification and five feature levels for bounding-box regression.

The outputs are merged into:

```text
Classification:
[batch, anchors, classes]

Bounding boxes:
[batch, anchors, 4]
```

The detector then:

1. decodes anchor-relative bounding boxes;
2. applies the sigmoid function to classification logits;
3. selects the highest scoring class for each anchor;
4. applies the confidence threshold;
5. performs class-aware NMS;
6. selects the highest-confidence detection.

The final detection is represented as:

```python
@dataclass
class Detection:
    class_id: int
    confidence: float

    x1: float
    y1: float
    x2: float
    y2: float
```

Bounding box coordinates are expressed in pixels of the original camera frame.

---

# Hand tracking

Hand tracking is performed independently from gesture classification.

The current implementation uses **MediaPipe Hand Landmarker**.

The hand tracker returns:

```text
Hand | None
```

where `Hand` contains the 21 detected hand landmarks.

The `HandMapper` converts these landmarks into a normalized palm position.

This separation allows the gesture classifier and cursor tracking system to evolve independently.

For example, a different hand tracking implementation can be introduced without changing the EfficientDet inference pipeline.

---

# Performance metrics

The vision module collects timing and system metrics for every frame.

Current metrics include:

* FPS;
* capture time;
* preprocessing time;
* inference time;
* transfer time;
* postprocessing time;
* total latency;
* CPU usage;
* memory usage;
* temperature.

The metrics are transmitted to the controller together with the inference result and are intended for real-time visualization.

The current timing pipeline is approximately:

```text
Camera capture
      │
      ▼
Gesture inference
      │
      ├── EfficientDet
      └── Hand Landmarker
      │
      ▼
Postprocessing
      │
      ▼
IPC
```

---

# Launch

Create a virtual environment:

```bash
cd vision

python3 -m venv .venv

source .venv/bin/activate

pip install -r requirements.txt
```

Launch the vision module:

```bash
python3 main.py
```

The controller host and port can be specified using command-line arguments:

```bash
python3 main.py \
    --host 192.168.50.1 \
    --port 5000
```

For local testing:

```bash
python3 main.py \
    --host localhost \
    --port 5000
```

The video stream uses a separate TCP connection and port:

```text
5001
```

The video parameters can be configured using:

```bash
python3 main.py \
    --video-width 640 \
    --video-height 480 \
    --video-fps 10 \
    --video-quality 60
```

Debug visualization can be enabled with:

```bash
python3 main.py --debug
```

The debug mode displays the camera image and processing results on the vision device. It is optional and is not required for headless operation.

---

# Controller module

## Purpose

The Controller module runs on the PC.

It is responsible for:

* receiving inference results;
* receiving performance metrics;
* interpreting recognized gestures;
* converting gestures into computer actions;
* managing the virtual input device;
* providing system and inference visualization.

The controller does not perform computer vision or neural network inference.

---

# Gesture recognition

The controller receives the gesture classification from the vision module.

The classification contains:

```text
class_id
confidence
```

The `GestureRecognizer` converts the inference result into a `GestureState`.

```text
InferenceResult
       │
       ▼
GestureRecognizer
       │
       ▼
GestureState
       │
       ▼
InputController
       │
       ▼
Linux uinput
```

The recognizer maps neural-network class IDs to semantic gesture types.

The current controller supports the following gesture states:

```text
PALM
FIST
OK
ONE
NONE
```

The exact class IDs are defined by the controller/vision configuration and must remain consistent with the model labels.

This separation keeps neural network inference independent from computer-control logic.

---

# Input control

The controller creates a virtual mouse using:

```text
Linux uinput
```

The operating system treats the resulting device as a regular mouse.

The controller can therefore generate mouse movement and button events without requiring direct integration with the graphical desktop environment.

The current input controller supports:

* absolute cursor movement;
* left mouse button;
* right mouse button;
* mouse wheel scrolling.

The cursor position is derived from the normalized palm position received from the vision module.

---

## Gesture actions

The current controller maps gestures to actions approximately as follows:

```text
PALM
  │
  └── Cursor movement

FIST
  │
  └── Left mouse button

OK
  │
  └── Scrolling

ONE
  │
  └── Right mouse button

NONE
  │
  └── Release active buttons
```

The controller maintains button state internally to ensure that buttons are released when the gesture changes or when the hand disappears.

Scrolling is implemented using the relative mouse wheel event:

```text
REL_WHEEL
```

Hand movement in the vertical direction controls the scroll direction.

---

# Build

Requirements:

* C++20;
* CMake;
* Linux;
* Linux kernel with uinput support.

Build:

```bash
cd build

cmake ..
make -j$(nproc)
```

Launch:

```bash
./controller/controller
```

The controller listens for vision data on TCP port `5000` by default.

The video server listens on TCP port `5001`.

Access to:

```text
/dev/uinput
```

is required.

For example:

```bash
sudo modprobe uinput
```

or configure an appropriate udev rule.

---

# IPC

Communication between the vision device and the controller is performed over TCP.

Transport:

```text
IPv4
TCP
```

Default ports:

```text
5000  - inference and metrics
5001  - video stream
```

The inference connection and video connection are separate TCP connections.

```text
Vision device                         PC

              TCP 5000
Inference ───────────────────────────────► IPC Server


              TCP 5001
Video ──────────────────────────────────► Video Server
```

The inference connection carries structured binary protocol messages.

The video connection carries the camera stream used by the controller visualization.

No Internet connection is required.

The devices can communicate directly over Ethernet using static or otherwise locally assigned IP addresses.

---

# Inference protocol

The inference protocol is versioned and binary.

The protocol currently defines two message types:

```text
RESULT
METRICS
```

## RESULT

The result message contains the combined output of gesture classification and hand tracking.

Conceptually:

```text
frame_id
timestamp

hand_present

class_id
confidence

hand:
    x
    y
```

The hand coordinates are normalized:

```text
x ∈ [0, 1]
y ∈ [0, 1]
```

When no valid hand is available:

```text
hand_present = false
```

and the hand payload is considered unavailable.

---

## METRICS

The metrics message contains performance and system information:

```text
frame_id
timestamp

fps

capture_time_us
preprocess_time_us
inference_time_us
transfer_time_us
postprocess_time_us
total_latency_us

cpu_usage
memory_usage
temperature
```

Both message types share a common protocol header.

The detailed binary format is documented in:

```text
controller/include/protocol/protocol.md
controller/include/protocol/serialization.md
```

---

# Data flow

```text
                    Vision Device
                         │
             ┌───────────┴───────────┐
             │                       │
             ▼                       ▼
        EfficientDet           Hand Landmarker
             │                       │
             ▼                       ▼
          Gesture                Palm Position
             │                       │
             └───────────┬───────────┘
                         │
                         ▼
                  Inference Result
                         │
                         ├──────────────┐
                         │              │
                         ▼              ▼
                       TCP 5000      TCP 5001
                         │              │
                         ▼              ▼
                    IPC Server     Video Server
                         │
              ┌──────────┴──────────┐
              │                     │
              ▼                     ▼
       GestureRecognizer        Dashboard
              │                  (ImGui)
              ▼
       InputController
              │
              ▼
          Linux uinput
```

The IPC layer is responsible only for transporting and deserializing data.

It does not contain gesture recognition or visualization logic.

---

# Visualization

The controller provides a visualization layer for the neuromorphic AI demonstrator.

The dashboard is intended to display:

* camera stream;
* current hand position;
* recognized gesture;
* classification confidence;
* FPS;
* total latency;
* inference time;
* transfer time;
* latency history;
* CPU usage;
* memory usage;
* device temperature;
* neural network/device status.

Conceptually:

```text
┌─────────────────────────────────────────────────────────────┐
│              NEUROMORPHIC AI DEMONSTRATOR                  │
├──────────────────────────────┬──────────────────────────────┤
│                              │                              │
│          CAMERA              │       INFERENCE             │
│                              │                              │
│        [ video ]             │       GESTURE: FIST         │
│                              │       CONFIDENCE: 99.7%     │
│         ● hand               │                              │
│                              │                              │
├──────────────────────────────┴──────────────────────────────┤
│ PERFORMANCE                                                 │
│                                                             │
│ FPS             14.2                                        │
│ Latency         63.9 ms                                     │
│ Inference       5.2 ms                                      │
│ Transfer        0.1 ms                                      │
│                                                             │
│ ─────────────── latency history ───────────────────────     │
│ 64ms   ▂▃▂▂▃▅▃▂▂▃▂▁▂▃▂                                    │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│ DEVICE                                                      │
│                                                             │
│ Architecture        AArch64                                 │
│ CPU                 31%                                     │
│ Memory              47.8%                                   │
│ Temperature         54.3°C                                 │
└─────────────────────────────────────────────────────────────┘
```

The visualization layer uses **Dear ImGui** on the controller side.

Visualization is performed on the PC and does not require a desktop environment on the vision device.

---

# Current status

Implemented:

* [x] Camera image capture
* [x] Headless vision operation
* [x] EfficientDet-D0 gesture detection
* [x] EfficientDet ONNX inference
* [x] EfficientDet output decoding
* [x] Anchor generation
* [x] Confidence filtering
* [x] Class-aware NMS
* [x] Best gesture selection
* [x] MediaPipe hand landmark tracking
* [x] 21-point hand landmark processing
* [x] Palm center calculation
* [x] Normalized hand position
* [x] Combined gesture + hand position result
* [x] TCP communication between vision and controller
* [x] Separate TCP video stream
* [x] Binary IPC protocol
* [x] Inference result transmission
* [x] Performance metric transmission
* [x] C++ deserialization of results and metrics
* [x] Gesture recognition in the controller
* [x] Virtual mouse via uinput
* [x] Cursor control
* [x] Left mouse button control
* [x] Right mouse button control
* [x] Mouse wheel scrolling
* [x] Ethernet-based distributed operation

In development:

* [ ] ImGui dashboard
* [ ] Real-time latency graph
* [ ] Inference confidence visualization
* [ ] Device status visualization
* [ ] Additional gestures
* [ ] Control profile configuration
* [ ] Deployment on the target neuromorphic platform
* [ ] Hardware-accelerated inference

---

# Design principles

## Separation of concerns

The vision module answers:

> What does the camera see?

and:

> What gesture did the neural network recognize?

The hand tracker answers:

> Where is the hand?

The controller answers:

> What should the computer do?

The dashboard answers:

> What is the system doing right now?

---

## Minimal coupling

Vision and Controller communicate through a versioned binary protocol.

The vision module does not depend on the controller implementation, and the controller does not depend on the internal implementation of the vision pipeline.

The vision implementation can therefore be replaced without changing the controller, provided that the IPC protocol remains compatible.

---

## Independent gesture and position pipelines

Gesture classification and hand position tracking are deliberately separated.

```text
                 Camera
                   │
          ┌────────┴────────┐
          │                 │
          ▼                 ▼
     EfficientDet     Hand Landmarker
          │                 │
          ▼                 ▼
       Gesture          Landmarks
                            │
                            ▼
                       Hand Mapper
```

This allows the gesture classifier and cursor tracking implementation to be changed independently.

For example:

* EfficientDet can be replaced by another detector;
* MediaPipe can be replaced by another hand tracker;
* the palm-position algorithm can be changed without modifying the gesture classifier;
* new gestures can be added without changing the TCP transport.

---

## Hardware independence

The vision module is designed to run on a separate Linux-based AArch64 device.

During development, an Orange Pi can be used as a hardware stand-in for the target neuromorphic platform.

The controller runs independently on the PC.

---

## Extensibility

The protocol supports multiple message types, allowing new telemetry and inference data to be added without coupling them to the input-control subsystem.

New gestures and neural network models can be introduced without changing the underlying TCP transport.

The hand tracking pipeline can also be replaced independently from the gesture recognition model.

---

## Demonstrator-oriented design

The project is not only intended to perform gesture-based computer control.

It is also designed to demonstrate:

* neural network inference on dedicated hardware;
* distributed processing;
* low-latency communication;
* computer vision;
* real-time gesture recognition;
* inference performance;
* system resource usage;
* real-time telemetry visualization.

The final system should make the complete processing pipeline visible to the observer:

```text
Camera
  │
  ├──────────────► Hand Tracking
  │                    │
  │                    ▼
  │               Hand Position
  │
  ▼
EfficientDet
  │
  ▼
Gesture Classification
  │
  └──────────────┐
                 ▼
          Inference Result
                 │
                 ▼
              Ethernet
                 │
                 ▼
             Controller
                 │
                 ▼
           Gesture Logic
                 │
                 ▼
             uinput
                 │
                 ▼
          Computer Action
```

At the same time, the system exposes the performance characteristics of the individual processing stages through the telemetry channel.

---

# Summary

The current system consists of two independent processing paths on the vision device:

```text
Camera
  │
  ├───────────────────────┐
  │                       │
  ▼                       ▼
EfficientDet-D0       MediaPipe
  │                  Hand Landmarker
  │                       │
  ▼                       ▼
Gesture              21 landmarks
  │                       │
  │                       ▼
  │                  HandMapper
  │                       │
  └───────────┬───────────┘
              ▼
       InferenceResult
              │
              ▼
          TCP / Ethernet
              │
              ▼
          Controller
              │
              ▼
       GestureRecognizer
              │
              ▼
        InputController
              │
              ▼
          Linux uinput
```

This architecture keeps neural network inference, hand tracking, communication and computer input control separated while allowing the complete system to operate as a distributed real-time demonstrator.
