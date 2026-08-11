# Gesture Control

A distributed computer control system using computer vision and neural network-based hand gesture recognition.

The project is designed as a prototype for a **neuromorphic AI demonstrator**. The vision and neural network inference pipeline runs on a separate Linux-based AArch64 device, while the PC receives inference results and performance metrics over Ethernet.

The project is divided into two independent modules:

* **Vision module (Python)** — camera capture, hand detection, image preprocessing, neural network inference and performance measurement.
* **Controller module (C++)** — receiving inference results, interpreting gestures, controlling the computer and providing a visualization layer.

## Architecture

The system is designed to run on two separate devices connected via Ethernet.

```text
┌─────────────────────────────┐
│         Vision Device       │
│      Linux / AArch64        │
│                             │
│  Camera                     │
│     │                       │
│     ▼                       │
│  Hand Detection             │
│     │                       │
│     ▼                       │
│  Image Preprocessing        │
│     │                       │
│     ▼                       │
│  Neural Network             │
│  Gesture Classification     │
│     │                       │
│     ├── Inference Result    │
│     │                       │
│     └── Performance Metrics │
│              │              │
└──────────────┼──────────────┘
               │
               │ Ethernet / TCP
               │
┌──────────────▼──────────────┐
│             PC              │
│                             │
│       IPC Server            │
│          │                  │
│     ┌────┴────┐             │
│     │         │             │
│ Result     Metrics          │
│     │         │             │
│     ▼         ▼             │
│ Gesture   Dashboard         │
│Recognizer (ImGui)           │
│     │                       │
│     ▼                       │
│ Input Controller            │
│     │                       │
│     ▼                       │
│ Linux uinput                │
└─────────────────────────────┘
```

The vision device does not require a desktop environment. It can operate as a headless Linux system connected to a camera.

The PC is responsible for user interaction, input control and visualization.

## Project structure

```text
.
├── build
│
├── config
│   └── config.json              # Control settings
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
    ├── crop.py
    ├── gestures.py
    ├── hand_tracker.py
    ├── inference.py
    ├── ipc.py
    ├── labels.py
    ├── main.py
    ├── models.py
    ├── renderer.py
    ├── requirements.txt
    └── models
        ├── gesture_model.onnx
        └── hand_landmarker.task
```

# Vision module

## Purpose

The Vision module is responsible for the complete computer vision and inference pipeline.

It performs:

* camera image capture;
* hand detection;
* hand image extraction;
* image preprocessing;
* neural network inference;
* gesture classification;
* performance measurement;
* transmission of inference results and metrics to the controller.

The vision module does not:

* control the mouse;
* interact with the operating system input subsystem;
* interpret gestures as computer actions.

### Processing pipeline

```text
Camera
  │
  ▼
Hand Tracker
  │
  ▼
Hand Detection
  │
  ▼
Crop
  │
  ▼
Neural Network
  │
  ▼
Gesture Classification
  │
  ├───────────────┐
  ▼               ▼
Result          Metrics
  │               │
  └───────┬───────┘
          ▼
       TCP IPC
```

The current neural network is an EfficientNet-B0 based classifier exported to ONNX.

The current gesture classes are:

```text
fist
open_hand
```

## Performance metrics

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

These metrics are transmitted to the controller together with the inference results and are intended for real-time visualization.

## Launch

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
python3 main.py --host 192.168.50.1 --port 5000
```

For local testing:

```bash
python3 main.py --host localhost --port 5000
```

Debug visualization can be enabled with:

```bash
python3 main.py --debug
```

The debug mode displays the camera image and vision processing results on the vision device. It is optional and is not required for headless operation.

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

## Gesture recognition

The controller receives the classified gesture from the vision module.

For example:

```text
class_id = 0
confidence = 0.91
```

The `GestureRecognizer` converts the inference result into a `GestureState`.

```text
Inference Result
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

This keeps the neural network and computer control logic independent.

## Virtual input

The controller creates a virtual mouse using:

```text
Linux uinput
```

The operating system treats the resulting device as a regular mouse.

The controller can therefore generate mouse movement and button events without requiring direct integration with the graphical desktop environment.

## Build

Requirements:

* C++17;
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

# IPC

Communication between the vision device and the controller is performed over a TCP connection.

Transport:

```text
IPv4
TCP
```

Default port:

```text
5000
```

The connection is intended to work both between two separate devices connected by Ethernet and locally on the same machine for development.

Example:

```text
Vision device                    PC

192.168.50.2                     192.168.50.1
      │                                │
      │       TCP port 5000            │
      └───────────────────────────────►│
```

No Internet connection is required. The devices can communicate directly over Ethernet using static or otherwise locally assigned IP addresses.

## Message types

The protocol currently defines two message types:

```text
RESULT
METRICS
```

### RESULT

Contains the result of neural network inference:

```text
frame_id
timestamp
hand_present

class_id
confidence

bounding box:
    x
    y
    width
    height
```

### METRICS

Contains performance and system information:

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

## Data flow

```text
Vision
  │
  ├── RESULT
  │
  └── METRICS
       │
       ▼
     TCP
       │
       ▼
Controller IPC
       │
       ├──────────────► GestureRecognizer
       │
       └──────────────► Dashboard
```

The IPC layer is responsible only for transporting and deserializing data.

It does not contain gesture recognition or visualization logic.

# Visualization

The controller is intended to provide a real-time dashboard for the neuromorphic AI demonstrator.

The dashboard will display:

* current camera/inference state;
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
│        CAMERA                │       NEURAL NETWORK         │
│                              │                              │
│       [ image ]              │       ████████████           │
│                              │       FIST       97.3%       │
│                              │       OPEN HAND  2.7%        │
│                              │                              │
├──────────────────────────────┴──────────────────────────────┤
│ PERFORMANCE                                                 │
│                                                             │
│ FPS             28.7                                        │
│ Latency         3.09 ms                                     │
│ Inference       1.84 ms                                     │
│ Transfer        0.73 ms                                     │
│                                                             │
│ ─────────────── latency history ───────────────────────     │
│ 3.2ms   ▂▃▂▂▃▅▃▂▂▃▂▁▂▃▂                                    │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│ DEVICE                                                      │
│                                                             │
│ FPGA / AArch64       NPU ACTIVE                             │
│ CPU                  31%                                     │
│ Memory               47.8%                                   │
│ Temperature          54.3°C                                 │
└─────────────────────────────────────────────────────────────┘
```

The planned visualization layer uses **Dear ImGui**.

Visualization is performed on the PC and does not require a desktop environment on the vision device.

# Current status

Implemented:

* [x] Camera image capture
* [x] Hand detection
* [x] Hand image cropping
* [x] Neural network gesture classification
* [x] EfficientNet-B0 inference via ONNX
* [x] TCP communication between vision and controller
* [x] Binary IPC protocol
* [x] Inference result transmission
* [x] Performance metric transmission
* [x] C++ deserialization of results and metrics
* [x] Gesture recognition in the controller
* [x] Virtual mouse via uinput
* [x] Cursor control
* [x] Ethernet-based distributed operation

In development:

* [ ] ImGui dashboard
* [ ] Real-time latency graph
* [ ] Inference confidence visualization
* [ ] Device status visualization
* [ ] Additional gestures
* [ ] Control profile configuration
* [ ] Deployment on the target neuromorphic platform

# Design principles

## Separation of concerns

The vision module answers:

> What does the camera see?

and:

> What did the neural network recognize?

The controller answers:

> What should the computer do?

The dashboard answers:

> What is the system doing right now?

## Minimal coupling

Vision and Controller communicate through a versioned binary protocol.

The vision module does not depend on the controller implementation, and the controller does not depend on the internal implementation of the vision pipeline.

The vision implementation can therefore be replaced without changing the controller, provided that the IPC protocol remains compatible.

## Hardware independence

The vision module is designed to run on a separate Linux-based AArch64 device.

During development, an Orange Pi can be used as a hardware stand-in for the target neuromorphic platform.

The controller runs independently on the PC.

## Extensibility

The protocol supports multiple message types, allowing new telemetry and inference data to be added without coupling them to the input-control subsystem.

New gestures and neural network models can be introduced without changing the underlying TCP transport.

## Demonstrator-oriented design

The project is not only intended to perform gesture-based computer control.

It is also designed to demonstrate:

* neural network inference on dedicated hardware;
* distributed processing;
* low-latency communication;
* inference performance;
* system resource usage;
* real-time telemetry visualization.

The final system should make the complete processing pipeline visible to the observer:

```text
Camera
  ↓
Vision
  ↓
Neural Network
  ↓
Inference
  ↓
Ethernet
  ↓
Controller
  ↓
Computer Action
```

while simultaneously exposing the performance characteristics of each stage.
