Gesture Control IPC Protocol
============================

Byte order:
    Little Endian

Frame format:

uint32  protocol_version

uint64  timestamp_ms

uint8   hand_count

Hand[2]

Hand:

21 landmarks

Landmark:

float x
float y
float z
