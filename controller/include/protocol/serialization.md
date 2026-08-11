# Gesture Control IPC Protocol — Serialization

## Version

Current protocol version:

```text
uint32 protocol_version = 2
```

## Byte order

All integer and floating-point values are encoded in **Little Endian** byte order.

Floating-point values use IEEE 754 single-precision representation for `float` fields.

The sender and receiver must use the exact field order defined below.

## Transport

The current transport is:

```text
TCP
```

TCP provides a reliable byte stream but does not preserve application-level message boundaries.

Therefore each protocol message consists of:

```text
MessageHeader
Payload
```

The header contains the payload size, allowing the receiver to determine the message boundary.

---

# Message Header

The header has the following format:

```text
uint32 protocol_version
uint8  message_type
uint8  reserved
uint16 reserved
uint32 payload_size
```

Byte layout:

| Offset | Size | Field            |
| -----: | ---: | ---------------- |
|      0 |    4 | protocol_version |
|      4 |    1 | message_type     |
|      5 |    1 | reserved         |
|      6 |    2 | reserved         |
|      8 |    4 | payload_size     |

Total size:

```text
12 bytes
```

All reserved fields must be transmitted as zero.

Receivers must ignore reserved fields.

---

# Message types

```text
0x00  RESULT
0x01  METRICS
```

Unknown message types must be rejected.

---

# RESULT serialization

The RESULT payload is serialized in the following order:

```text
uint64 frame_id
uint64 timestamp_ms

uint8  hand_present
uint8  reserved
uint8  reserved
uint8  reserved

uint32 class_id
float  confidence

float  x
float  y
float  width
float  height
```

Byte layout:

| Offset | Size | Field        |
| -----: | ---: | ------------ |
|      0 |    8 | frame_id     |
|      8 |    8 | timestamp_ms |
|     16 |    1 | hand_present |
|     17 |    3 | reserved     |
|     20 |    4 | class_id     |
|     24 |    4 | confidence   |
|     28 |    4 | x            |
|     32 |    4 | y            |
|     36 |    4 | width        |
|     40 |    4 | height       |

Total payload size:

```text
44 bytes
```

The corresponding message therefore occupies:

```text
12 byte header
+
44 byte payload
=
56 bytes
```

## RESULT semantics

### frame_id

Monotonically increasing identifier of the processed camera frame.

### timestamp_ms

Unix timestamp in milliseconds.

### hand_present

```text
0 = no hand
1 = hand detected
```

### class_id

Identifier of the recognized gesture.

```text
0xFFFFFFFF
```

means that no classification is available.

### confidence

Neural network confidence:

```text
0.0 ... 1.0
```

### Bounding box

The bounding box is encoded as:

```text
x
y
width
height
```

using normalized image coordinates.

---

# METRICS serialization

The METRICS payload is serialized in the following order:

```text
uint64 frame_id
uint64 timestamp_ms

float fps

uint32 capture_time_us
uint32 preprocess_time_us
uint32 inference_time_us
uint32 transfer_time_us
uint32 postprocess_time_us
uint32 total_latency_us

float cpu_usage
float memory_usage
float temperature
```

Byte layout:

| Offset | Size | Field               |
| -----: | ---: | ------------------- |
|      0 |    8 | frame_id            |
|      8 |    8 | timestamp_ms        |
|     16 |    4 | fps                 |
|     20 |    4 | capture_time_us     |
|     24 |    4 | preprocess_time_us  |
|     28 |    4 | inference_time_us   |
|     32 |    4 | transfer_time_us    |
|     36 |    4 | postprocess_time_us |
|     40 |    4 | total_latency_us    |
|     44 |    4 | cpu_usage           |
|     48 |    4 | memory_usage        |
|     52 |    4 | temperature         |

Total payload size:

```text
56 bytes
```

The corresponding message therefore occupies:

```text
12 byte header
+
56 byte payload
=
68 bytes
```

## METRICS semantics

### frame_id

Identifier of the camera frame associated with the metrics.

It must match the `frame_id` of the corresponding RESULT message.

### timestamp_ms

Unix timestamp in milliseconds.

### fps

Current processing rate in frames per second.

### Timing fields

All timing fields use microseconds:

```text
capture_time_us
preprocess_time_us
inference_time_us
transfer_time_us
postprocess_time_us
total_latency_us
```

Example:

```text
inference_time_us = 5255
```

means:

```text
5.255 ms
```

### System metrics

```text
cpu_usage
memory_usage
temperature
```

`cpu_usage` and `memory_usage` are expressed as percentages.

`temperature` is expressed in degrees Celsius.

Temperature is expressed in degrees Celsius.

---

# Frame association

A RESULT message and a METRICS message belonging to the same processed frame must contain the same `frame_id`.

Example:

```text
RESULT
    frame_id = 563
    class_id = 0
    confidence = 0.913899

METRICS
    frame_id = 563
    inference_time_us = 5255
    total_latency_us = 63921
```

The controller can therefore associate inference data and performance data without relying on TCP message timing.

---

# TCP framing

The receiver must first read exactly:

```text
12 bytes
```

to obtain the message header.

The receiver then reads exactly:

```text
payload_size
```

bytes.

A single call to `recv()` is not guaranteed to return all requested bytes.

The implementation must therefore continue receiving until the complete header or payload has been obtained.

Conceptually:

```text
read_exact(12)
       │
       ▼
MessageHeader
       │
       ▼
payload_size
       │
       ▼
read_exact(payload_size)
       │
       ▼
Payload
```

---

# Validation

The receiver must validate:

1. `protocol_version`
2. `message_type`
3. `payload_size`

The receiver must reject unsupported protocol versions.

The receiver must reject unknown message types.

The receiver must reject payloads whose size does not exactly match the expected size for the corresponding message type.

The receiver must enforce a maximum payload size before allocating memory.

Current implementation limit:

```text
1 MiB
```

Expected payload sizes:

```text
RESULT  = 44 bytes
METRICS = 56 bytes
```

---

# Error handling

A malformed message must not be passed to higher-level components.

Examples of invalid messages include:

```text
unsupported protocol version
unknown message type
incorrect payload size
payload larger than maximum allowed size
incomplete TCP message
connection closed during message reception
```

The IPC layer should report the error and terminate the current connection.

---

# Reserved fields

Reserved fields are included for future protocol extensions and alignment.

They must be transmitted as zero.

Receivers must ignore their values.

Future protocol versions may assign meanings to currently reserved fields.
