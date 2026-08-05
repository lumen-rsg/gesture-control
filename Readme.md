# Gesture Control

A computer control system using hand gestures based on computer vision.

The project is divided into two independent modules:

* **Vision module (Python)** — camera image capture and hand position recognition.
* **Controller module (C++)** — interpreting hand position data and computer control.


## Architecture

General operating principle:

```
Camera
  |
  v
MediaPipe Hand Landmarker
  |
  v
Vision module (Python)
  |
  |  Raw hand landmarks
  v
IPC (Unix domain socket)
  |
  v
Controller module (C++)
  |
  v
Gesture recognizer
  |
  v
Input controller
  |
  v
Linux uinput
```

## Project structure:

```
.
├── config
│   └── config.json          # Настройки управления
│
├── controller               # C++ модуль управления
│   ├── CMakeLists.txt
│   ├── main.cpp
│   │
│   ├── include
│   │   ├── config.hpp
│   │   ├── gesture_state.hpp
│   │   ├── input.hpp
│   │   ├── ipc.hpp
│   │   ├── json.hpp
│   │   ├── recognizer.hpp
│   │   └── protocol
│   │       ├── gesture.hpp
│   │       ├── protocol.hpp
│   │       ├── protocol.md
│   │       └── serialization.md
│   │
│   └── src
│       ├── config.cpp
│       ├── input.cpp
│       ├── ipc.cpp
│       └── recognizer.cpp
│
└── vision                   # Python модуль компьютерного зрения
    ├── camera.py
    ├── detector.py
    ├── gestures.py
    ├── ipc.py
    ├── main.py
    ├── renderer.py
    ├── requirements.txt
    └── models
        └── hand_landmarker.task
```

# Vision module

## Purpose

The Vision module is only responsible for receiving hand position data.

It does not:

* control the mouse;
* recognize gestures;
* interact with the OS.

It outputs raw data:

* number of detected hands;
* coordinates of 21 landmarks for each hand;
* frame timestamp.

Example data structure:

```
Frame
 ├── timestamp
 ├── hand_count
 └── hands
      └── landmarks[21]
           ├── x
           ├── y
           └── z
```

## Launch

Creating a virtual environment:

```bash
cd vision

python3 -m venv .venv

source .venv/bin/activate

pip install -r requirements.txt
```

Launch:

```bash
python3 main.py
```

To start debug mode with visualization:

```bash
python3 main.py --debug
```

## Controller module

## Purpose

The Controller module receives raw data from the vision module and is responsible for:

* gesture recognition;
* storing gesture states;
* converting gestures into actions;
* managing the virtual input device.

Control mechanism used:

```
Linux uinput
```

The controller creates a virtual mouse, which the system treats as a regular input device.

## Build

Requirements:

* C++17
* CMake
* Linux kernel with uinput support

Build:

```bash
cd controller

mkdir build
cd build

cmake ..

make
```

Launch:

```bash
./controller/controller
```

To work, you need access to:

```text
/dev/uinput
```

For example:

```bash
sudo modprobe uinput
```

or configuring udev rules.

# IPC Protocol

Communication between modules is carried out via a Unix domain socket.

Transport:

```
AF_UNIX SOCK_STREAM
```

Only hand position data is transmitted over the connection.

The controller does not receive ready-made mouse commands.

Data flow:

```
Frame
 |
 v
GestureRecognizer
 |
 v
GestureState
 |
 v
InputController
```

## Current Features

Implemented:

* [x] Camera image capture
* [x] Hand detection via MediaPipe
* [x] Landmark data transfer via IPC
* [x] C++ processing of received data
* [x] Virtual mouse via uinput
* [x] Cursor movement based on index finger position

In development:

* [ ] Gesture recognition
* [ ] Mouse button holding via pinch gesture
* [ ] Additional gestures
* [ ] Control profile configuration

# Design principles

Key project principles:

## Separation of concerns

Vision is only responsible for:

> What does the camera see?

Controller is responsible for:

> What should the computer do?

## Minimal coupling

Vision and Controller communicate only via a data protocol.

The computer vision module can be replaced without changing the controller.

## Extensibility

New gestures should be added to the Controller without changing the Vision module.
