# Gesture Control

A distributed computer control system based on computer vision and neural network hand gesture recognition.

The project is designed as a prototype for a **neuromorphic AI demonstrator**. The vision pipeline runs on a separate Linux-based AArch64 device, while a PC receives the recognized gesture, hand coordinates and performance data over Ethernet and converts them into computer input events.

The project consists of two independent modules:

* **Vision module (Python)** — captures frames from a webcam, performs neural network gesture recognition and calculates the hand position.
* **Controller module (C++)** — receives the vision data over TCP, converts gestures and hand coordinates into mouse input through Linux `uinput`, and provides a real-time monitoring dashboard.

---

# Architecture

The system is designed to run on two separate devices connected through Ethernet.

```text
┌──────────────────────────────────────────────┐
│                Vision Device                 │
│                 Linux / AArch64              │
│                                              │
│  Web Camera                                  │
│      │                                       │
│      ├──────────────────────┐                │
│      │                      │                │
│      ▼                      ▼                │
│  Gesture Neural         Hand Mapping         │
│     Network                  │               │
│      │                      │                │
│      ▼                      ▼                │
│  Gesture + Confidence   Hand Coordinates     │
│      │                      │                │
│      └───────────┬──────────┘                │
│                  │                           │
│                  ▼                           │
│             TCP Client                      │
│                  │                           │
└──────────────────┼───────────────────────────┘
                   │
          ┌────────┴────────┐
          │                 │
          │ TCP 5000        │ TCP 5001
          │                 │
┌─────────▼─────────────────▼───────────────────┐
│                    PC                         │
│                                              │
│              TCP Server                     │
│                  │                           │
│          ┌───────┴────────┐                  │
│          │                │                  │
│     Inference Data     Video Stream          │
│          │                │                  │
│          ▼                ▼                  │
│   Gesture Recognizer   Dashboard             │
│          │             (ImGui)               │
│          │                                   │
│          ▼                                   │
│    Input Controller                         │
│          │                                   │
│          ▼                                   │
│      Linux uinput                           │
│          │                                   │
│          ▼                                   │
│       Computer                              │
└──────────────────────────────────────────────┘
````

The vision device can operate as a headless Linux system. It only requires a webcam and network connection to the controller.

The PC is responsible for computer input control and visualization.

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
│   │   ├── video.hpp
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

The Vision module is responsible for obtaining information from the webcam and preparing it for the Controller module.

It performs two independent tasks:

1. gesture recognition;
2. hand position calculation.

The vision module does not control the computer or generate input events.

---

## Processing pipeline

The webcam frame is processed by two independent pipelines.

```text
                    Camera frame
                         │
              ┌──────────┴──────────┐
              │                     │
              ▼                     ▼
       Gesture Network         Hand Mapping
              │                     │
              ▼                     ▼
       Gesture class          Hand position
       Confidence             X / Y
              │                     │
              └──────────┬──────────┘
                         │
                         ▼
                  Vision Result
                         │
                         ▼
                      TCP 5000
```

The optional camera image is transmitted separately:

```text
Camera frame
     │
     ▼
Video encoding
     │
     ▼
TCP 5001
```

---

# Gesture recognition

The current gesture recognition pipeline uses an **EfficientDet-D0** neural network exported to ONNX.

The network receives the camera image and determines which supported gesture is present.

The current gesture classes are:

```text
1 - PALM
2 - FIST
3 - OK
4 - ONE
```

The corresponding semantic gestures are:

| Class ID | Gesture | Controller action  |
| -------- | ------- | ------------------ |
| `1`      | PALM    | Cursor movement    |
| `2`      | FIST    | Left mouse button  |
| `3`      | OK      | Scroll             |
| `4`      | ONE     | Right mouse button |

The gesture classification result contains:

```text
class_id
confidence
```

The confidence value is represented as a floating-point value in the range:

```text
[0, 1]
```

---

# EfficientDet inference

The current gesture detector is based on **EfficientDet-D0** and is executed using ONNX Runtime.

The ONNX model expects:

```text
Input:
[1, 3, 512, 512]
```

The input camera frame is:

1. converted from BGR to RGB;
2. resized while preserving its aspect ratio;
3. padded to `512 × 512`;
4. normalized using ImageNet mean and standard deviation;
5. converted from HWC to NCHW format;
6. passed to ONNX Runtime.

The detector processes the complete camera frame.

The current implementation does not require a separately cropped hand image for gesture recognition.

---

## Detection postprocessing

EfficientDet produces classification and bounding-box outputs for multiple feature levels.

The inference pipeline performs:

1. output merging;
2. anchor generation;
3. bounding-box decoding;
4. sigmoid conversion of classification logits;
5. confidence filtering;
6. class-aware Non-Maximum Suppression (NMS);
7. selection of the highest-confidence detection.

The final gesture is selected from the highest-confidence valid detection.

The bounding box is retained internally as part of the detection result but the Controller currently uses the gesture class, confidence and hand coordinates rather than the bounding box for mouse control.

---

# Hand position

Hand position is calculated independently from gesture recognition.

The hand mapping pipeline determines the position of the hand in normalized coordinates.

```text
Camera frame
     │
     ▼
Hand landmarks
     │
     ▼
HandMapper
     │
     ▼
Normalized hand position
     │
     ├── X ∈ [0, 1]
     └── Y ∈ [0, 1]
```

The resulting position is used by the Controller to move the cursor.

The hand coordinates are intentionally independent from the gesture classification. This allows the gesture recognition model and hand position calculation to be modified separately.

---

# Vision result

The Vision module combines the gesture classification and hand position into a single inference result.

Conceptually:

```text
InferenceResult
├── hand_present
├── class_id
├── confidence
└── hand
    ├── x
    └── y
```

The hand position is represented by normalized coordinates:

```text
x ∈ [0, 1]
y ∈ [0, 1]
```

If no valid hand is available:

```text
hand_present = false
```

The Controller can then release active input states and stop cursor interaction.

---

# Communication

The Vision module communicates with the Controller module through TCP.

Two separate TCP connections are used.

```text
TCP 5000
    │
    └── Inference data

TCP 5001
    │
    └── Optional camera video
```

The inference connection carries the structured binary protocol containing:

* gesture class;
* gesture confidence;
* hand presence;
* normalized hand coordinates;
* frame information;
* performance metrics.

The video connection is used only for the optional camera preview displayed by the Controller dashboard.

The video stream is not required for computer control.

---

# Vision module launch

Create a Python virtual environment:

```bash
cd vision

python3 -m venv .venv

source .venv/bin/activate

pip install -r requirements.txt
```

Start the vision module:

```bash
python3 main.py
```

Specify the Controller address:

```bash
python3 main.py \
    --host 192.168.50.1 \
    --port 5000
```

The video connection uses port `5001` by default.

For local development:

```bash
python3 main.py \
    --host localhost \
    --port 5000
```

The camera device can be selected with:

```bash
python3 main.py --camera 0
```

The gesture confidence threshold can be changed with:

```bash
python3 main.py --score-thr 0.5
```

---

## Video configuration

The optional video stream can be configured with:

```bash
python3 main.py \
    --video-width 640 \
    --video-height 480 \
    --video-fps 10 \
    --video-quality 60
```

The video connection is independent from the inference connection.

The video stream can therefore be disabled or omitted when the dashboard camera preview is not required.

---

# Controller module

## Purpose

The Controller module is written in C++ and runs on the PC.

It acts as a TCP server for the Vision module.

The Controller:

* accepts inference data from TCP port `5000`;
* optionally accepts the camera stream on TCP port `5001`;
* interprets gesture classes;
* converts hand coordinates into cursor coordinates;
* generates mouse events through Linux `uinput`;
* maintains input state;
* provides a real-time dashboard.

The Controller does not perform neural network inference.

---

# Gesture control

The Controller converts the recognized gestures into mouse actions.

The current mapping is:

```text
PALM
  │
  └── Cursor movement

FIST
  │
  └── Left mouse button

ONE
  │
  └── Right mouse button

OK
  │
  └── Mouse wheel scrolling
```

The semantic meaning of the gestures is therefore:

### PALM

The open palm is the neutral movement gesture.

The cursor follows the normalized hand position.

No mouse button is pressed.

```text
PALM
  ↓
Hand coordinates
  ↓
Cursor movement
```

### FIST

A closed fist controls the left mouse button.

```text
FIST
  ↓
BTN_LEFT
```

### ONE

The one-finger gesture controls the right mouse button.

```text
ONE
  ↓
BTN_RIGHT
```

### OK

The OK gesture activates scrolling.

```text
OK
  ↓
REL_WHEEL
```

Vertical hand movement determines the scrolling direction.

### NONE

When no valid gesture or hand is available, the controller does not generate an active gesture action and releases active button states when necessary.

---

# Cursor control

The cursor position is calculated from the normalized hand coordinates received from the Vision module.

The Controller converts:

```text
x ∈ [0, 1]
y ∈ [0, 1]
```

into the configured absolute input range.

The mapping is controlled by the controller configuration.

For example:

```json
{
  "input": {
    "invert_x": true,
    "invert_y": false,

    "move_scale": 0.25,

    "screen_width": 32767,
    "screen_height": 32767
  },

  "dashboard": {
    "camera_preview": true,
    "video_port": 5001
  }
}
```

---

## Input configuration

### `invert_x`

Controls horizontal axis inversion.

```json
"invert_x": true
```

### `invert_y`

Controls vertical axis inversion.

```json
"invert_y": false
```

### `move_scale`

Controls the sensitivity of cursor movement.

```json
"move_scale": 0.25
```

### `screen_width`

Defines the maximum absolute X coordinate used by the virtual input device.

```json
"screen_width": 32767
```

### `screen_height`

Defines the maximum absolute Y coordinate used by the virtual input device.

```json
"screen_height": 32767
```

---

# Virtual mouse

The Controller creates a virtual mouse using Linux:

```text
uinput
```

The operating system treats the virtual device as a regular mouse.

The Controller can generate:

* absolute cursor movement;
* left mouse button events;
* right mouse button events;
* mouse wheel events.

The virtual device is independent from the physical mouse.

---

## uinput requirements

The Linux kernel must provide `uinput`.

For example:

```bash
sudo modprobe uinput
```

The user running the Controller must have permission to access:

```text
/dev/uinput
```

This can be configured using an appropriate udev rule.

---

# Controller configuration

The Controller configuration is stored in:

```text
config/config.json
```

Example:

```json
{
  "input": {
    "invert_x": true,
    "invert_y": false,

    "move_scale": 0.25,

    "screen_width": 32767,
    "screen_height": 32767
  },

  "dashboard": {
    "camera_preview": true,
    "video_port": 5001
  }
}
```

The configuration separates input behaviour from the controller implementation.

This makes it possible to adjust cursor sensitivity, axis orientation and dashboard video settings without modifying the source code.

---

# Dashboard

The Controller includes a real-time dashboard implemented using **Dear ImGui**.

The dashboard runs entirely on the PC.

It does not require a graphical desktop environment on the Vision device.

The dashboard can display:

* camera preview;
* current recognized gesture;
* gesture confidence;
* FPS;
* inference latency;
* transfer latency;
* total latency;
* latency history;
* CPU usage;
* memory usage;
* device temperature;
* connection state.

The camera preview is received through the separate TCP video connection on port `5001`.

---

## Dashboard example

Conceptually, the dashboard looks like:

```text
┌──────────────────────────────────┐
│  NEUROMORPHIC AI DEMONSTRATOR    │
├──────────────────────────────────┤
│ CAMERA                           │
│                                  │
│        [ camera preview ]        │
│                                  │
├──────────────────────────────────┤
│ NEURAL NETWORK                   │
│                                  │
│ GESTURE: FIST                    │
│ CONFIDENCE: 99.7%                │
│                                  │
├──────────────────────────────────┤
│ PERFORMANCE                      │
│                                  │
│ FPS:             14.2            │
│ Inference:        5.2 ms         │
│ Transfer:         0.1 ms         │
│ Total latency:   63.9 ms         │
│                                  │
│ latency history                  │
│ ▂▃▂▂▃▅▃▂▂▃▂▁▂▃▂                 │
│                                  │
├──────────────────────────────────┤
│ DEVICE                           │
│                                  │
│ CPU:             31.0%           │
│ Memory:          47.8%           │
│ Temperature:     54.3 C          │
│                                  │
│ Connection: CONNECTED            │
└──────────────────────────────────┘
```

The dashboard is intended both for debugging and for demonstrating the operation and performance of the distributed vision system.

---

# IPC protocol

The inference connection uses a versioned binary TCP protocol.

The protocol separates transport from the processing logic.

The Controller receives structured inference and telemetry data from the Vision module.

The protocol contains information corresponding to:

```text
Inference result
    ├── frame information
    ├── hand presence
    ├── gesture class
    ├── confidence
    └── hand coordinates

Performance metrics
    ├── FPS
    ├── capture time
    ├── preprocessing time
    ├── inference time
    ├── transfer time
    ├── postprocessing time
    ├── total latency
    ├── CPU usage
    ├── memory usage
    └── temperature
```

The detailed binary format is documented in:

```text
controller/include/protocol/protocol.md
controller/include/protocol/serialization.md
```

The IPC layer is responsible for transporting and deserializing data.

It does not contain gesture recognition or input-control logic.

---

# Video protocol

The camera preview uses a separate TCP connection.

```text
Vision
   │
   │ TCP 5001
   ▼
VideoServer
   │
   ▼
Dashboard
```

The Vision module encodes camera frames for transmission.

The Controller receives the encoded frames and decodes them for display.

The video connection is optional.

If:

```json
"camera_preview": false
```

is configured, the dashboard does not require the camera preview.

The inference channel on TCP port `5000` remains responsible for computer control.

---

# Build

## Requirements

Controller requirements:

* C++20;
* CMake;
* Linux;
* OpenGL;
* GLFW;
* Dear ImGui;
* OpenCV;
* Linux kernel with `uinput` support.

Vision requirements:

* Python 3;
* OpenCV;
* NumPy;
* ONNX Runtime;
* PyTorch / torchvision;
* MediaPipe;
* psutil.

---

## Build controller

```bash
cd build

cmake ..

make -j$(nproc)
```

Launch the Controller:

```bash
./controller/controller
```

The Controller starts the TCP services and waits for the Vision module to connect.

Default ports:

```text
5000 - inference / telemetry
5001 - camera video
```

---

# Distributed operation

A typical deployment consists of:

```text
Vision Device                         PC
─────────────                         ──

Linux / AArch64                       Linux
Webcam                                Controller
   │                                     │
   │                                     │
   │ TCP 5000                            │
   ├────────────────────────────────────►│
   │                                     │
   │ TCP 5001                            │
   ├────────────────────────────────────►│
   │                                     │
   │                                  Dashboard
   │                                     │
   │                                  uinput
```

The devices do not require an Internet connection.

They only need network connectivity between the Vision device and the PC.

The system can also be tested locally by running both modules on the same machine.

---

# Current status

Implemented:

* [x] Webcam image capture
* [x] Headless vision operation
* [x] EfficientDet-D0 gesture recognition
* [x] ONNX Runtime inference
* [x] EfficientDet output decoding
* [x] Anchor generation
* [x] Confidence filtering
* [x] Class-aware NMS
* [x] Best gesture detection
* [x] Four gesture classes
* [x] Hand coordinate calculation
* [x] Normalized hand coordinates
* [x] Combined gesture and hand-position result
* [x] TCP inference connection
* [x] TCP video connection
* [x] Binary inference protocol
* [x] Inference result transmission
* [x] Performance metric transmission
* [x] Camera video transmission
* [x] C++ TCP server
* [x] C++ protocol deserialization
* [x] Gesture recognition in the Controller
* [x] Virtual mouse through Linux uinput
* [x] Cursor movement
* [x] Left mouse button control
* [x] Right mouse button control
* [x] Mouse wheel scrolling
* [x] Controller configuration
* [x] ImGui dashboard
* [x] Camera preview
* [x] Real-time performance metrics
* [x] Ethernet-based distributed operation

In development:

* [ ] More gesture actions
* [ ] Additional telemetry
* [ ] Improved dashboard visualization
* [ ] More configurable control profiles
* [ ] Deployment on the target neuromorphic platform
* [ ] Hardware-accelerated inference

---

# Design principles

## Separation of concerns

The Vision module answers:

> What does the camera see?

and:

> Which gesture is present?

The hand mapping pipeline answers:

> Where is the hand?

The Controller answers:

> What should the computer do?

The Dashboard answers:

> What is the system doing right now?

Each responsibility is implemented independently.

---

## Independent gesture and position pipelines

Gesture recognition and hand position calculation are deliberately separated.

```text
                 Camera
                   │
          ┌────────┴────────┐
          │                 │
          ▼                 ▼
     Gesture Network     Hand Mapping
          │                 │
          ▼                 ▼
       Gesture          Hand Position
          │                 │
          └────────┬────────┘
                   │
                   ▼
             TCP Result
```

This allows the two pipelines to evolve independently.

The gesture recognition model can be replaced without changing cursor mapping.

The hand position algorithm can also be replaced without changing the gesture classifier.

---

## Minimal coupling

Vision and Controller communicate through a defined TCP protocol.

The Vision module does not depend on the internal implementation of the Controller.

The Controller does not depend on the internal implementation of the Vision pipeline.

As long as the communication protocol remains compatible, either module can be modified or replaced independently.

---

## Hardware independence

The Vision module is designed to run on a separate Linux-based AArch64 device.

During development, an Orange Pi can be used as a hardware stand-in for the target neuromorphic platform.

The Controller runs independently on the PC.

---

## Extensibility

New gesture classes can be added to the neural network and mapped to new Controller actions without changing the underlying TCP transport.

The hand mapping pipeline can also be replaced independently.

Additional telemetry and message fields can be introduced through the versioned protocol.

The camera video stream is independent from the inference channel and can therefore be enabled or disabled separately.

---

## Demonstrator-oriented design

The project is not only intended to provide gesture-based computer control.

It is also intended to demonstrate:

* neural network inference on dedicated hardware;
* distributed computer vision;
* real-time gesture recognition;
* low-latency network communication;
* computer input emulation;
* system resource monitoring;
* real-time telemetry visualization.

The complete processing chain is:

```text
                         Vision Device
                              │
                         Web Camera
                              │
               ┌──────────────┴──────────────┐
               │                             │
               ▼                             ▼
        Gesture Network                 Hand Mapping
               │                             │
               ▼                             ▼
          Gesture                     Hand Coordinates
               │                             │
               └──────────────┬──────────────┘
                              │
                              ▼
                         TCP 5000
                              │
                              ▼
                          Controller
                              │
                     ┌────────┴────────┐
                     │                 │
                     ▼                 ▼
              Gesture Logic       Input Mapping
                     │                 │
                     └────────┬────────┘
                              │
                              ▼
                          Linux uinput
                              │
                              ▼
                       Computer Control
```

The optional camera visualization follows a separate path:

```text
Camera
  │
  ▼
Video Encoding
  │
  ▼
TCP 5001
  │
  ▼
Controller
  │
  ▼
Dashboard
```

At the same time, performance metrics are transmitted through the inference connection and displayed by the Controller dashboard.

---

# Summary

Gesture Control is a distributed computer vision and input-control system consisting of two independent modules.

The **Vision module** runs on a Linux-based AArch64 device:

```text
Webcam
  │
  ├──────────────────────┐
  │                      │
  ▼                      ▼
Gesture Network       Hand Mapping
  │                      │
  ▼                      ▼
Gesture               Hand Position
  │                      │
  └──────────┬───────────┘
             │
             ▼
          TCP 5000
```

The **Controller module** runs on the PC:

```text
TCP 5000
   │
   ▼
Controller
   │
   ├── Gesture recognition
   ├── Cursor mapping
   └── Input control
           │
           ▼
       Linux uinput
           │
           ▼
        Computer
```

The optional camera stream uses:

```text
Camera
  │
  ▼
TCP 5001
  │
  ▼
Dashboard
```

The current gesture control scheme is:

```text
PALM ──► Cursor movement
FIST ──► Left mouse button
ONE  ──► Right mouse button
OK   ──► Mouse wheel scrolling
```

The architecture keeps computer vision, hand tracking, network communication, input control and visualization separated while allowing the complete system to operate as a distributed real-time demonstrator.
