# Gesture Control IPC Protocol

This document describes the logical data structures exchanged between the Vision module and the Controller.

The protocol is versioned and transported over a TCP byte stream.

## Protocol version

Current protocol version:

```text
2
```

## Message types

The protocol currently defines two message types:

```text
0x00  RESULT
0x01  METRICS
```

## RESULT

A `RESULT` message contains the result of neural network inference for a single camera frame.

```text
Result
├── frame_id
├── timestamp_ms
├── hand_present
├── classification
└── hand
```

### Fields

```text
uint64 frame_id

uint64 timestamp_ms

uint8 hand_present
```

`hand_present` is:

```text
0  No hand detected
1  Hand detected
```

Three additional bytes are reserved for alignment.

### Classification

```text
Classification
├── uint32 class_id
└── float confidence
```

`class_id` identifies the recognized gesture.

The special value:

```text
0xFFFFFFFF
```

means that no classification is available.

`confidence` contains the neural network confidence in the range:

```text
0.0 ... 1.0
```

### Bounding box

```text
BoundingBox
├── float x
├── float y
├── float width
└── float height
```

The bounding box describes the detected hand in normalized image coordinates.

### Complete RESULT structure

```text
uint64 frame_id
uint64 timestamp_ms

uint8  hand_present
uint8  reserved[3]

uint32 class_id
float  confidence

float  x
float  y
float  width
float  height
```

Payload size:

```text
44 bytes
```

## METRICS

A `METRICS` message contains performance and system information for a processed camera frame.

```text
Metrics
├── frame_id
├── timestamp_ms
├── fps
├── capture_time_us
├── preprocess_time_us
├── inference_time_us
├── transfer_time_us
├── postprocess_time_us
├── total_latency_us
├── cpu_usage
├── memory_usage
└── temperature
```

### Complete METRICS structure

```text
uint64 frame_id
uint64 timestamp_ms

float  fps

uint32 capture_time_us
uint32 preprocess_time_us
uint32 inference_time_us
uint32 transfer_time_us
uint32 postprocess_time_us
uint32 total_latency_us

float  cpu_usage
float  memory_usage
float  temperature
```

Payload size:

```text
56 bytes
```

## Frame relationship

`frame_id` uniquely identifies a processed camera frame.

A `RESULT` message and the corresponding `METRICS` message use the same `frame_id`.

Example:

```text
RESULT
    frame_id = 563

METRICS
    frame_id = 563
```

This allows the controller and visualization layer to associate inference results with their corresponding performance measurements.

## Timing

`timestamp_ms` is a Unix timestamp in milliseconds.

All fields ending in `_time_us` contain durations in microseconds.

Example:

```text
inference_time_us = 5255
```

means:

```text
5.255 ms
```

## System metrics

The following fields describe the state of the vision device:

```text
cpu_usage
memory_usage
temperature
```

`cpu_usage` and `memory_usage` are expressed as percentages.

`temperature` is expressed in degrees Celsius.

## Current gesture classes

The protocol does not define the meaning of individual `class_id` values.

Class identifiers are defined by the vision model and its label configuration.

The current model contains:

```text
class_id = 0    fist
class_id = 1    open_hand
```

The protocol remains independent of the particular neural network model.
