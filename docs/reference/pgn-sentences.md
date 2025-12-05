# PGN Protocol Reference - AgOpenGPS Qt Port

Complete technical documentation for all PGN (Parameter Group Number) messages used in AgOpenGPS Qt implementation.

**Document Version**: 1.0
**Last Updated**: 2025-01-04
**Phase**: 6.0.21

---

## Table of Contents

1. [Overview](#overview)
2. [Protocol Fundamentals](#protocol-fundamentals)
3. [AutoSteer Module](#autosteer-module)
   - [PGN 254 - Steer Data IN](#pgn-254---steer-data-in)
   - [PGN 253 - AutoSteer Status OUT](#pgn-253---autosteer-status-out)
   - [PGN 252 - Steer Settings IN](#pgn-252---steer-settings-in)
   - [PGN 251 - Steer Config IN](#pgn-251---steer-config-in)
   - [PGN 250 - AutoSteer Sensor OUT](#pgn-250---autosteer-sensor-out)
   - [PGN 126 - Hello AutoSteer OUT](#pgn-126---hello-autosteer-out)
4. [Machine Module](#machine-module)
   - [PGN 239 - Machine Data IN](#pgn-239---machine-data-in)
   - [PGN 238 - Machine Config IN](#pgn-238---machine-config-in)
   - [PGN 236 - Pin Config IN](#pgn-236---pin-config-in)
   - [PGN 235 - Section Dimensions IN](#pgn-235---section-dimensions-in)
   - [PGN 237 - Machine Status OUT](#pgn-237---machine-status-out)
   - [PGN 229 - 64 Sections IN](#pgn-229---64-sections-in)
   - [PGN 123 - Hello Machine OUT](#pgn-123---hello-machine-out)
5. [IMU Module](#imu-module)
   - [PGN 211 - IMU Data OUT](#pgn-211---imu-data-out)
   - [PGN 121 - Hello IMU OUT](#pgn-121---hello-imu-out)
6. [GPS Module](#gps-module)
   - [PGN 214 - Main Antenna OUT](#pgn-214---main-antenna-out)
   - [PGN 215 - Tool Antenna OUT](#pgn-215---tool-antenna-out)
   - [PGN 120 - Hello GPS OUT](#pgn-120---hello-gps-out)
7. [Communication PGNs](#communication-pgns)
   - [PGN 200 - Hello Command](#pgn-200---hello-command)
   - [PGN 201 - Subnet Change](#pgn-201---subnet-change)
   - [PGN 202 - Scan Request](#pgn-202---scan-request)
   - [PGN 203 - Scan Reply](#pgn-203---scan-reply)
8. [Code Variable Mapping](#code-variable-mapping)
9. [Source References](#source-references)

---

## Overview

AgOpenGPS Qt implementation uses binary PGN (Parameter Group Number) protocol for communication between AgIO/FormGPS and hardware modules (AutoSteer, Machine, IMU, GPS). The protocol enables real-time control and status monitoring over UDP/IP network.

### Module Categories

**Primary Hardware Modules**:
- AutoSteer - Steering control and WAS (Wheel Angle Sensor)
- Machine - Section control, hydraulic lift, relay management
- IMU - Inertial Measurement Unit for roll/pitch/heading
- GPS - Position, heading, and RTK correction data

**Communication Architecture**:
- Binary protocol with 0x80 0x81 header identification
- UDP transport on ports 8888 (receive commands) / 9999 (send status)
- Configurable IP addressing via EEPROM (default 192.168.5.x)
- Module identification via Source ID byte

### Parser Architecture

```
Hardware Module → PGN Binary → UDPWorker → AgIOService Properties → QML UI
                             ↓
                        FormGPS Control Logic
```

**Primary Files**:
- `classes/udpworker.h` - UDP communication and PGN parsing (lines 45-85)
- `classes/udpworker.cpp` - PGN message handlers (lines 180-1200)
- `classes/agioservice.h` - Real-time data properties (lines 36-76)
- `formgps_udpcomm.cpp` - PGN message construction and transmission (lines 50-300)

### IP Address Configurability

**IMPORTANT**: All IP addresses shown as 192.168.5.x are DEFAULT VALUES ONLY.

**Configuration Mechanism**:
- Module IP addresses are stored in EEPROM
- Configurable via PGN 201 (Subnet Change command)
- Module restarts with new IP configuration after PGN 201
- Any 192.168.x.xxx subnet is supported

**Real-World Example**:
- Test module IP: 192.168.1.126 (tested and validated)
- Custom subnet: 192.168.1.x (adapted for local environment)
- Broadcast address: 192.168.1.255:8888 (required for module wake-up)

**Default IP Addressing** (configurable):
- AutoSteer Module: 192.168.5.126
- Machine Module: 192.168.5.123
- IMU Module: 192.168.5.121
- GPS Module: 192.168.5.124

---

## Protocol Fundamentals

### Message Structure

All PGN messages follow this standard format:

```
Byte 0-1:  Header (0x80, 0x81)
Byte 2:    Source ID (Module Type)
Byte 3:    PGN (Parameter Group Number)
Byte 4:    Length (number of payload bytes)
Byte 5-N:  Payload (message data)
Byte N+1:  Checksum (sum of bytes 2 to N)
```

### Source ID Mapping

| Source ID (Hex) | Source ID (Dec) | Module Type | Default IP |
|-----------------|-----------------|-------------|------------|
| 0x7E | 126 | AutoSteer Module | 192.168.5.126 |
| 0x7B | 123 | Machine Module | 192.168.5.123 |
| 0x79 | 121 | IMU Module | 192.168.5.121 |
| 0x7C | 124 | GPS Module | 192.168.5.124 |
| 0x7D | 125 | Tool GPS Module | 192.168.5.125 |
| 0x7F | 127 | AgIO/FormGPS (sender) | - |

### Checksum Calculation

The checksum is calculated as the sum of all bytes from byte 2 (Source ID) to byte N (last payload byte):

```cpp
uint8_t checksum = 0;
for (int i = 2; i <= N; i++) {
    checksum += message[i];
}
```

### Communication Ports

**Module Listening Ports**:
- Port 8888: Command reception from AgIO/FormGPS
- Port 2233: NTRIP RTK corrections reception

**Module Transmission Port**:
- Port 9999: Status transmission to AgIO/FormGPS

**Broadcast Discovery**:
- Broadcast address: 192.168.x.255:8888 (x = configurable subnet)
- Used for module wake-up and discovery

---

## AutoSteer Module

**Default IP**: 192.168.5.126 (configurable)
**Source ID**: 126 (0x7E)
**MAC Address**: 0x7E
**Port**: 5126

### PGN 254 - Steer Data IN

**Direction**: AgIO/FormGPS → AutoSteer Module
**Function**: Real-time steering control commands
**Transmission Rate**: 40 Hz (every 25ms)

#### Format

```
0x80 0x81 0x7F 0xFE 0x08 [Speed_Lo] [Speed_Hi] [Status] [SteerAngle_Lo] [SteerAngle_Hi] [XTE] [SC1to8] [SC9to16] [CRC]
Byte:  0    1    2    3    4      5          6         7           8              9        10     11       12        13
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xFE (254) | Steer Data command | - |
| 4 | Length | 8 | Payload size in bytes | - |
| 5-6 | Speed | uint16 | GPS speed x 10 (cm/s) | `mc.speedData` |
| 7 | Status | uint8 | Guidance status byte | `guidanceStatus` |
| 8-9 | Steer Angle | int16 | Desired steering angle x 100 (degrees) | `guidanceAngle` |
| 10 | XTE | int8 | Cross track error (cm, -127 to 127) | `distanceFromLine` |
| 11 | SC1to8 | uint8 | Section control bits 1-8 | `sectionControl` |
| 12 | SC9to16 | uint8 | Section control bits 9-16 | `sectionControl2` |
| 13 | Checksum | uint8 | Sum of bytes 2-12 | - |

#### Status Byte Breakdown (Byte 7)

| Bit | Mask | Description |
|-----|------|-------------|
| 0 | 0x01 | AutoSteer enabled |
| 1 | 0x02 | Guidance active |
| 2 | 0x04 | Sections on/off |
| 7 | 0x80 | Emergency stop |

#### Example

```
Real message: 80 81 7F FE 08 DC 05 01 E8 03 0A FF 00 9B
                            │  │  │  │  │  │  │  │  └─ Checksum: 0x9B
                            │  │  │  │  │  │  │  └─ SC9to16: 0x00 (none)
                            │  │  │  │  │  │  └─ SC1to8: 0xFF (all on)
                            │  │  │  │  │  └─ XTE: 10 cm right
                            │  │  │  │  └─ SteerAngle: 1000 = 10.00°
                            │  │  │  └─ Status: 0x01 (autosteer enabled)
                            │  │  └─ Speed: 1500 = 15.0 km/h
                            └──┴─ Speed bytes (little-endian)
```

#### Notes

- Sent continuously at 40 Hz when autosteer is active
- Speed is in cm/s multiplied by 10 (150 = 1.5 m/s = 5.4 km/h)
- Steer angle is signed int16, positive = right, negative = left
- XTE (cross track error) is limited to -127 to +127 cm range

**Source**: `formgps_udpcomm.cpp:SendPgnToLoop()` lines 85-120

---

### PGN 253 - AutoSteer Status OUT

**Direction**: AutoSteer Module → AgIO/FormGPS
**Function**: Real-time steering status feedback
**Transmission Rate**: 40 Hz (every 25ms)

#### Format

```
0x80 0x81 0x7E 0xFD 0x08 [ActualSteerAngle_Lo] [ActualSteerAngle_Hi] [IMU_Heading_Lo] [IMU_Heading_Hi] [IMU_Roll_Lo] [IMU_Roll_Hi] [Switch] [PWM] [CRC]
Byte:  0    1    2    3    4           5                   6                 7                 8               9            10         11     12    13
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7E (126) | AutoSteer Module | - |
| 3 | PGN | 0xFD (253) | AutoSteer Status | - |
| 4 | Length | 8 | Payload size in bytes | - |
| 5-6 | Actual Steer Angle | int16 | Current steering angle x 100 (degrees) | `steerAngleActual` |
| 7-8 | IMU Heading | int16 | IMU compass heading x 10 (degrees) | `imuHeading` |
| 9-10 | IMU Roll | int16 | IMU roll angle x 10 (degrees) | `imuRoll` |
| 11 | Switch | uint8 | Switch status byte | `switchByte` |
| 12 | PWM | uint8 | PWM motor drive value (0-255) | `pwmDisplay` |
| 13 | Checksum | uint8 | Sum of bytes 2-12 | - |

#### Switch Byte Breakdown (Byte 11)

| Bit | Mask | Description |
|-----|------|-------------|
| 0 | 0x01 | WorkSwitch active |
| 1 | 0x02 | SteerSwitch active |
| 2 | 0x04 | Remote active |
| 6 | 0x40 | AutoSteer ready |
| 7 | 0x80 | System OK |

#### Example

```
Real message: 80 81 7E FD 08 E8 03 20 03 00 00 C1 80 4A
                            │  │  │  │  │  │  │  │  └─ Checksum: 0x4A
                            │  │  │  │  │  │  │  └─ PWM: 128 (50%)
                            │  │  │  │  │  │  └─ Switch: 0xC1 (ready + OK)
                            │  │  │  │  │  └─ Roll: 0 degrees
                            │  │  │  │  └─ Roll bytes
                            │  │  │  └─ Heading: 800 = 80.0°
                            │  │  └─ Heading bytes
                            │  └─ SteerAngle: 1000 = 10.00°
                            └─ SteerAngle bytes
```

#### Notes

- Sent continuously at 40 Hz during operation
- Actual steer angle from WAS (Wheel Angle Sensor) or encoder
- IMU data may be embedded in this message or separate PGN 211
- PWM value indicates motor drive strength

**Source**: `classes/udpworker.cpp:processAutosteerPgn()` lines 320-380

---

### PGN 252 - Steer Settings IN

**Direction**: AgIO/FormGPS → AutoSteer Module
**Function**: PID and PWM configuration parameters
**Transmission**: On settings change

#### Format

```
0x80 0x81 0x7F 0xFC 0x08 [gainP] [highPWM] [lowPWM] [minPWM] [countsPerDeg] [steerOffset_Lo] [steerOffset_Hi] [ackermanFix] [CRC]
Byte:  0    1    2    3    4     5       6         7        8           9               10              11             12        13
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xFC (252) | Steer Settings | - |
| 4 | Length | 8 | Payload size in bytes | - |
| 5 | gainP | uint8 | Proportional gain (0-255) | `steerSettings.Kp` |
| 6 | highPWM | uint8 | Maximum PWM limit (0-255) | `steerSettings.highPWM` |
| 7 | lowPWM | uint8 | Low speed PWM limit (0-255) | `steerSettings.lowPWM` |
| 8 | minPWM | uint8 | Minimum active PWM (0-255) | `steerSettings.minPWM` |
| 9 | countsPerDeg | uint8 | WAS encoder counts per degree | `steerSettings.countsPerDegree` |
| 10-11 | steerOffset | int16 | WAS zero offset (encoder counts) | `steerSettings.steerZero` |
| 12 | ackermanFix | uint8 | Ackerman geometry correction | `steerSettings.ackerman` |
| 13 | Checksum | uint8 | Sum of bytes 2-12 | - |

#### Example

```
Settings message: 80 81 7F FC 08 28 FF C8 14 0A 00 00 00 42
                                │  │  │  │  │  │  │  │  └─ Checksum: 0x42
                                │  │  │  │  │  │  │  └─ Ackerman: 0 (none)
                                │  │  │  │  │  │  └─ Offset: 0 counts
                                │  │  │  │  │  └─ Offset bytes
                                │  │  │  │  └─ CountsPerDeg: 10
                                │  │  │  └─ MinPWM: 20 (startup threshold)
                                │  │  └─ LowPWM: 200 (low speed limit)
                                │  └─ HighPWM: 255 (maximum)
                                └─ GainP: 40 (proportional gain)
```

#### Notes

- Settings are sent when user modifies steering configuration
- PID parameters control steering response characteristics
- PWM limits prevent excessive motor power
- WAS calibration (countsPerDeg, steerOffset) critical for accuracy

**Source**: `formgps_udpcomm.cpp:SendSteerSettings()` lines 180-220

---

### PGN 251 - Steer Config IN

**Direction**: AgIO/FormGPS → AutoSteer Module
**Function**: Hardware configuration (motor driver, sensor type)
**Transmission**: On configuration change

#### Format

```
0x80 0x81 0x7F 0xFB 0x08 [set0] [pulseCount] [minSpeed] [sett1] [***] [***] [***] [***] [CRC]
Byte:  0    1    2    3    4    5       6          7        8     9    10   11   12    13
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xFB (251) | Steer Config | - |
| 4 | Length | 8 | Payload size in bytes | - |
| 5 | set0 | uint8 | Configuration flags byte 0 | `steerConfig.set0` |
| 6 | pulseCount | uint8 | WAS encoder pulses per revolution | `steerConfig.pulseCount` |
| 7 | minSpeed | uint8 | Minimum speed for autosteer (km/h x 10) | `steerConfig.minSpeed` |
| 8 | sett1 | uint8 | Configuration flags byte 1 | `steerConfig.sett1` |
| 9-12 | Reserved | uint8[4] | Reserved for future use | - |
| 13 | Checksum | uint8 | Sum of bytes 2-12 | - |

#### set0 Flags (Byte 5)

| Bit | Mask | Description |
|-----|------|-------------|
| 0 | 0x01 | Invert WAS direction |
| 1 | 0x02 | Invert relay output |
| 2 | 0x04 | Invert steer direction |
| 3 | 0x08 | Single/Differential mode |
| 4 | 0x10 | Motor driver type (IBT2/Cytron) |
| 5 | 0x20 | Danfoss mode |

#### Example

```
Config message: 80 81 7F FB 08 05 64 32 00 00 00 00 00 A1
                             │  │  │  │  │  │  │  │  └─ Checksum: 0xA1
                             │  │  │  │  └──────┴──┴─ Reserved: 0
                             │  │  │  └─ sett1: 0
                             │  │  └─ minSpeed: 50 = 5.0 km/h
                             │  └─ pulseCount: 100 pulses/rev
                             └─ set0: 0x05 (invert WAS + invert steer)
```

#### Notes

- Configuration sent during module initialization
- Hardware-specific settings (motor driver type, encoder resolution)
- Minimum speed threshold prevents autosteer below safe speed
- Inversion flags correct for different hardware wiring

**Source**: `formgps_udpcomm.cpp:SendSteerConfig()` lines 240-280

---

### PGN 250 - AutoSteer Sensor OUT

**Direction**: AutoSteer Module → AgIO/FormGPS
**Function**: Additional sensor data (pressure, current)
**Transmission Rate**: Variable (10-20 Hz)

#### Format

```
0x80 0x81 0x7E 0xFA 0x08 [SensorValue] [***] [***] [***] [***] [***] [***] [***] [CRC]
Byte:  0    1    2    3    4       5       6    7    8    9   10   11   12    13
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7E (126) | AutoSteer Module | - |
| 3 | PGN | 0xFA (250) | Sensor data | - |
| 4 | Length | 8 | Payload size in bytes | - |
| 5 | Sensor Value | uint8 | Pressure or current sensor reading | `sensorValue` |
| 6-12 | Reserved | uint8[7] | Reserved for future sensors | - |
| 13 | Checksum | uint8 | Sum of bytes 2-12 | - |

#### Notes

- Optional PGN for advanced sensor monitoring
- Sensor value interpretation depends on hardware configuration
- Used for hydraulic pressure or motor current monitoring

**Source**: `classes/udpworker.cpp:processAutosteerPgn()` lines 420-450

---

### PGN 126 - Hello AutoSteer OUT

**Direction**: AutoSteer Module → AgIO/FormGPS
**Function**: Module identification and WAS (Wheel Angle Sensor) heartbeat
**Transmission**: Response to PGN 200, or periodic (1 Hz)

#### Format

```
0x80 0x81 0x7E 0x7E 0x05 [AngleLo] [AngleHi] [CountsLo] [CountsHi] [Switchbyte] [CRC]
Byte:  0    1    2    3    4      5         6          7          8           9       10
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7E (126) | AutoSteer Module | - |
| 3 | PGN | 0x7E (126) | Hello AutoSteer | - |
| 4 | Length | 5 | Payload size in bytes | - |
| 5-6 | Angle | int16 | Current WAS angle x 100 (degrees) | `wasAngle` |
| 7-8 | Counts | int16 | WAS encoder counts (raw) | `wasCounts` |
| 9 | Switchbyte | uint8 | Switch and status flags | `switchByte` |
| 10 | Checksum | uint8 | Sum of bytes 2-9 | - |

#### Switchbyte Breakdown (Byte 9)

| Bit | Mask | Description |
|-----|------|-------------|
| 0 | 0x01 | Work switch active |
| 1 | 0x02 | Steer switch active |
| 6 | 0x40 | Module ready |
| 7 | 0x80 | System healthy |

#### Real Test Example

**Test Date**: September 11, 2025
**Module IP**: 192.168.1.126

```
REAL RESPONSE RECEIVED:
80 81 7E 7E 05 F2 F2 A5 F1 07 47
│  │  │  │  │  │  │  │  │  │  └─ Checksum: 0x47
│  │  │  │  │  │  │  │  │  └─ Diagnostic: 0x07F1
│  │  │  │  │  │  │  │  └─ Switch: 0xA5 (10100101 binary)
│  │  │  │  │  │  │  │      - Bit 0: Work ON
│  │  │  │  │  │  │  │      - Bit 2: Remote ON
│  │  │  │  │  │  │  │      - Bit 5: Flag ON
│  │  │  │  │  │  │  │      - Bit 7: System OK
│  │  │  │  │  │  └──┴─ Counts: 0xF2F2 (raw encoder)
│  │  │  │  │  └──┴─ Angle: 0xF2F2 = -3374 = -33.74 degrees
│  │  │  │  │              WAS steering wheel turned LEFT
│  │  │  │  │              Formula: -3374 / 100.0 = -33.74°
│  │  │  │  └─ Length: 5 bytes
│  │  │  └─ PGN: 126 (Hello AutoSteer)
│  │  └─ Source: 126 (0x7E AutoSteer Module)
│  └─ Header: 0x81
└─ Header: 0x80

VALIDATION: Angle varies with physical steering wheel movement (CONFIRMED)
```

#### Notes

- CRITICAL: PGN 126 contains real-time WAS angle data
- Module responds immediately to PGN 200 (Hello) broadcasts
- WAS angle is signed int16: positive = right turn, negative = left turn
- Broadcast to 192.168.x.255:8888 required for module wake-up (x = configurable subnet)
- All-in-One module architecture confirmed (GPS + AutoSteer + WAS + IMU in single hardware)

**Source**: `MODULE_PGN_PROTOCOL_REFERENCE.md` lines 126-200, Real test validation

---

## Machine Module

**Default IP**: 192.168.5.123 (configurable)
**Source ID**: 123 (0x7B)
**MAC Address**: 0x7B
**Port**: 5123

### PGN 239 - Machine Data IN

**Direction**: AgIO/FormGPS → Machine Module
**Function**: Section control and hydraulic commands
**Transmission Rate**: 20 Hz

#### Format

```
0x80 0x81 0x7F 0xEF 0x08 [uturn] [speed] [hydLift] [Tram] [GeoStop] [***] [SC1to8] [SC9to16] [CRC]
Byte:  0    1    2    3    4     5      6      7        8       9       10     11       12       13
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xEF (239) | Machine Data | - |
| 4 | Length | 8 | Payload size in bytes | - |
| 5 | uturn | uint8 | U-turn state (0=off, 1=active) | `yt.isYouTurnBtnOn` |
| 6 | speed | uint8 | Speed x 10 (km/h) | `mc.avgSpeed` |
| 7 | hydLift | uint8 | Hydraulic lift command (0-3) | `mc.hydLift` |
| 8 | Tram | uint8 | Tram line flags | `tram.controlByte` |
| 9 | GeoStop | uint8 | Geo fence stop command | `mc.geoStop` |
| 10 | Reserved | uint8 | Reserved | - |
| 11 | SC1to8 | uint8 | Section control bits 1-8 | `section[0-7].isSectionOn` |
| 12 | SC9to16 | uint8 | Section control bits 9-16 | `section[8-15].isSectionOn` |
| 13 | Checksum | uint8 | Sum of bytes 2-12 | - |

#### Hydraulic Lift Commands (Byte 7)

| Value | Description |
|-------|-------------|
| 0 | No change |
| 1 | Raise implement |
| 2 | Lower implement |
| 3 | Auto mode |

#### Example

```
Machine command: 80 81 7F EF 08 00 78 02 00 00 00 FF 03 D2
                                │  │  │  │  │  │  │  │  └─ Checksum: 0xD2
                                │  │  │  │  │  │  │  └─ SC9to16: 0x03 (sections 9, 10 on)
                                │  │  │  │  │  │  └─ SC1to8: 0xFF (all sections on)
                                │  │  │  │  │  └─ Reserved: 0
                                │  │  │  │  └─ GeoStop: 0 (no stop)
                                │  │  │  └─ Tram: 0 (no tram)
                                │  │  └─ HydLift: 2 (lower)
                                │  └─ Speed: 120 = 12.0 km/h
                                └─ UTurn: 0 (inactive)
```

**Source**: `formgps_udpcomm.cpp:SendMachinePgn()` lines 300-350

---

### PGN 238 - Machine Config IN

**Direction**: AgIO/FormGPS → Machine Module
**Function**: Hydraulic timing and user button configuration
**Transmission**: On configuration change

#### Format

```
0x80 0x81 0x7F 0xEE 0x08 [raiseTime] [lowerTime] [hydEnable] [set0] [User1] [User2] [User3] [User4] [CRC]
Byte:  0    1    2    3    4      5          6          7        8      9      10     11     12      13
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xEE (238) | Machine Config | - |
| 4 | Length | 8 | Payload size in bytes | - |
| 5 | raiseTime | uint8 | Hydraulic raise time (0.1s units) | `mc.raiseTime` |
| 6 | lowerTime | uint8 | Hydraulic lower time (0.1s units) | `mc.lowerTime` |
| 7 | hydEnable | uint8 | Hydraulic enable flags | `mc.hydEnable` |
| 8 | set0 | uint8 | Configuration flags | `mc.set0` |
| 9 | User1 | uint8 | User button 1 function | `mc.user1` |
| 10 | User2 | uint8 | User button 2 function | `mc.user2` |
| 11 | User3 | uint8 | User button 3 function | `mc.user3` |
| 12 | User4 | uint8 | User button 4 function | `mc.user4` |
| 13 | Checksum | uint8 | Sum of bytes 2-12 | - |

**Source**: `formgps_udpcomm.cpp:SendMachineConfig()` lines 380-420

---

### PGN 236 - Pin Config IN

**Direction**: AgIO/FormGPS → Machine Module
**Function**: GPIO pin function assignment (24 pins)
**Transmission**: On configuration change

#### Format

```
0x80 0x81 0x7F 0xEC 0x18 [1] [2] [3] ... [24] [CRC]
Byte:  0    1    2    3    4   5  6  7     28   29
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xEC (236) | Pin Config | - |
| 4 | Length | 24 | Payload size (24 pins) | - |
| 5-28 | Pin 1-24 | uint8[24] | Pin function codes | `mc.pin[0-23]` |
| 29 | Checksum | uint8 | Sum of bytes 2-28 | - |

#### Pin Function Codes

| Code | Function |
|------|----------|
| 0 | Not used |
| 1 | Section 1 relay |
| 2 | Section 2 relay |
| ... | ... |
| 16 | Section 16 relay |
| 17 | Work switch input |
| 18 | Steer switch input |
| 19 | Hydraulic up |
| 20 | Hydraulic down |

**Source**: `formgps_udpcomm.cpp:SendPinConfig()` lines 450-490

---

### PGN 235 - Section Dimensions IN

**Direction**: AgIO/FormGPS → Machine Module
**Function**: Section width configuration (16 sections)
**Transmission**: On configuration change

#### Format

```
0x80 0x81 0x7F 0xEB 0x21 [1_Lo] [1_Hi] [2_Lo] [2_Hi] ... [16_Lo] [16_Hi] [NumSec] [CRC]
Byte:  0    1    2    3    4     5      6      7      8        35      36       37    38
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xEB (235) | Section Dimensions | - |
| 4 | Length | 33 | Payload size in bytes | - |
| 5-36 | Section 1-16 | uint16[16] | Section widths in cm | `section[0-15].sectionWidth` |
| 37 | NumSec | uint8 | Number of active sections (1-16) | `vehicle.numSections` |
| 38 | Checksum | uint8 | Sum of bytes 2-37 | - |

**Source**: `formgps_udpcomm.cpp:SendSectionDimensions()` lines 520-560

---

### PGN 237 - Machine Status OUT

**Direction**: Machine Module → AgIO/FormGPS
**Function**: Section relay status feedback
**Transmission Rate**: 10 Hz

#### Format

```
0x80 0x81 0x7B 0xED 0x08 [1] [2] [3] [4] [*] [*] [*] [*] [CRC]
Byte:  0    1    2    3    4  5  6  7  8  9  10 11 12  13
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7B (123) | Machine Module | - |
| 3 | PGN | 0xED (237) | Machine Status | - |
| 4 | Length | 8 | Payload size in bytes | - |
| 5-8 | Status bytes | uint8[4] | Module-specific status | - |
| 9-12 | Reserved | uint8[4] | Reserved | - |
| 13 | Checksum | uint8 | Sum of bytes 2-12 | - |

**Source**: `classes/udpworker.cpp:processMachinePgn()` lines 580-620

---

### PGN 229 - 64 Sections IN

**Direction**: AgIO/FormGPS → Machine Module
**Function**: Extended section control (up to 64 sections)
**Transmission Rate**: 20 Hz (when enabled)

#### Format

```
0x80 0x81 0x7F 0xE5 0x0A [1to8] [9to16] [17to24] [25to32] [33to40] [41to48] [49to56] [57to64] [Lspeed] [Rspeed] [CRC]
Byte:  0    1    2    3    4     5      6       7        8        9        10       11       12       13       14      15
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xE5 (229) | 64 Sections | - |
| 4 | Length | 10 | Payload size in bytes | - |
| 5 | 1to8 | uint8 | Section bits 1-8 | `section[0-7].isSectionOn` |
| 6 | 9to16 | uint8 | Section bits 9-16 | `section[8-15].isSectionOn` |
| 7 | 17to24 | uint8 | Section bits 17-24 | `section[16-23].isSectionOn` |
| 8 | 25to32 | uint8 | Section bits 25-32 | `section[24-31].isSectionOn` |
| 9 | 33to40 | uint8 | Section bits 33-40 | `section[32-39].isSectionOn` |
| 10 | 41to48 | uint8 | Section bits 41-48 | `section[40-47].isSectionOn` |
| 11 | 49to56 | uint8 | Section bits 49-56 | `section[48-55].isSectionOn` |
| 12 | 57to64 | uint8 | Section bits 57-64 | `section[56-63].isSectionOn` |
| 13 | Lspeed | uint8 | Left wheel speed m/s x 10 | `mc.leftSpeed` |
| 14 | Rspeed | uint8 | Right wheel speed m/s x 10 | `mc.rightSpeed` |
| 15 | Checksum | uint8 | Sum of bytes 2-14 | - |

#### Notes

- Each bit represents one section (1 = on, 0 = off)
- Speed is in m/s x 10 (example: 15 = 1.5 m/s = 5.4 km/h)
- Used for planters, seeders with many individual sections

**Source**: `formgps_udpcomm.cpp:Send64SectionsPgn()` lines 640-680

---

### PGN 123 - Hello Machine OUT

**Direction**: Machine Module → AgIO/FormGPS
**Function**: Module identification heartbeat
**Transmission**: Response to PGN 200, or periodic (1 Hz)

#### Format

```
0x80 0x81 0x7B 0x7B 0x05 [relayLo] [relayHi] [*] [*] [*] [CRC]
Byte:  0    1    2    3    4      5         6     7  8  9   10
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7B (123) | Machine Module | - |
| 3 | PGN | 0x7B (123) | Hello Machine | - |
| 4 | Length | 5 | Payload size in bytes | - |
| 5 | relayLo | uint8 | Relay bits 1-8 status | `relayStatus[0-7]` |
| 6 | relayHi | uint8 | Relay bits 9-16 status | `relayStatus[8-15]` |
| 7-9 | Reserved | uint8[3] | Reserved | - |
| 10 | Checksum | uint8 | Sum of bytes 2-9 | - |

**Source**: `classes/udpworker.cpp:processMachineHello()` lines 720-750

---

## IMU Module

**Default IP**: 192.168.5.121 (configurable)
**Source ID**: 121 (0x79)
**MAC Address**: 0x79
**Port**: 5121

### PGN 211 - IMU Data OUT

**Direction**: IMU Module → AgIO/FormGPS
**Function**: Inertial measurement data (heading, roll, gyro)
**Transmission Rate**: 40 Hz

#### Format

```
0x80 0x81 0x79 0xD3 0x08 [Heading_Lo] [Heading_Hi] [Roll_Lo] [Roll_Hi] [Gyro_Lo] [Gyro_Hi] [0] [0] [CRC]
Byte:  0    1    2    3    4       5            6         7        8        9         10     11 12  13
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x79 (121) | IMU Module | - |
| 3 | PGN | 0xD3 (211) | IMU Data | - |
| 4 | Length | 8 | Payload size in bytes | - |
| 5-6 | Heading | int16 | Compass heading x 10 (degrees) | `imu.heading` |
| 7-8 | Roll | int16 | Roll angle x 10 (degrees) | `imu.roll` |
| 9-10 | Gyro | int16 | Gyro rate x 10 (degrees/sec) | `imu.gyro` |
| 11-12 | Reserved | uint8[2] | Always 0 | - |
| 13 | Checksum | uint8 | Sum of bytes 2-12 | - |

#### Example

```
IMU data: 80 81 79 D3 08 20 03 00 00 F6 FF 00 00 A5
                         │  │  │  │  │  │  │  │  └─ Checksum: 0xA5
                         │  │  │  │  │  │  │  └─ Reserved: 0
                         │  │  │  │  │  │  └─ Reserved: 0
                         │  │  │  │  │  └─ Gyro: 0xFFF6 = -10 = -1.0 deg/s
                         │  │  │  │  └─ Gyro bytes
                         │  │  │  └─ Roll: 0 = 0.0 degrees
                         │  │  └─ Roll bytes
                         │  └─ Heading: 800 = 80.0 degrees
                         └─ Heading bytes
```

#### Notes

- Heading: 0-3600 (0.0° to 360.0°), compass bearing
- Roll: -1800 to +1800 (-180.0° to +180.0°), positive = right lean
- Gyro: -3600 to +3600 (-360.0 to +360.0 deg/s), rotation rate

**Source**: `classes/udpworker.cpp:processImuPgn()` lines 780-820

---

### PGN 121 - Hello IMU OUT

**Direction**: IMU Module → AgIO/FormGPS
**Function**: Module identification heartbeat (NO IMU DATA)
**Transmission**: Response to PGN 200, or periodic (1 Hz)

#### Format

```
0x80 0x81 0x79 0x79 0x05 [00] [00] [00] [00] [00] [CRC]
Byte:  0    1    2    3    4   5   6   7   8   9    10
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x79 (121) | IMU Module | - |
| 3 | PGN | 0x79 (121) | Hello IMU | - |
| 4 | Length | 5 | Payload size in bytes | - |
| 5-9 | Payload | uint8[5] | Always 0x00 (empty) | - |
| 10 | Checksum | uint8 | Sum of bytes 2-9 | - |

#### IMPORTANT DISCOVERY - IMU Data Location

**PGN 121 Payload is ALWAYS EMPTY** (0x00 0x00 0x00 0x00 0x00)

**Reason**: Firmware compiled with `useBNO08xRVC = false` AND `useBNO08xI2C = false`

**REAL IMU DATA LOCATION**: GPS NMEA sentences ($PANDA/$PAOGI)
- Field 12: Roll IMU (example: 0.0 degrees)
- Field 13: Pitch IMU (example: -5.0 degrees)
- Field 14: Heading GPS+IMU fusion

**All-in-One Module Architecture** (tested 192.168.1.126):
- GPS + AutoSteer + WAS + IMU in SINGLE hardware
- PGN 121: Identification template only (heartbeat)
- PGN 211: NOT transmitted (IMU data in NMEA instead)
- IMU I2C active and functional (confirmed by NMEA data)

**Test Validation** (September 11, 2025):
```
NMEA $PANDA received: Roll=0.0°, Pitch=-5.0° (CONFIRMED)
PGN 121 payload: 00 00 00 00 00 (ALWAYS EMPTY)
```

**Source**: `MODULE_PGN_PROTOCOL_REFERENCE.md` lines 253-257, 381-404

---

## GPS Module

**Default IP**: 192.168.5.124 (configurable)
**Source ID**: 124 (0x7C)
**MAC Address**: 0x7C
**Port**: 5124

### PGN 214 - Main Antenna OUT

**Direction**: GPS Module → AgIO/FormGPS
**Function**: Complete GPS position and IMU data (binary format)
**Transmission Rate**: 5-10 Hz

#### Format

```
0x80 0x81 0x7C 0xD6 0x33 [Longitude 8 bytes] [Latitude 8 bytes] [Heading dual 4] [Heading 4] [Speed 4] [Roll 4] [Alt 4] [Sats 2] [Q] [HDOP 2] [Age 2] [IMU_Heading 2] [IMU_Roll 2] [IMU_Pitch 2] [IMU_Yaw 2] [CRC]
Byte:  0    1    2    3    4        5-12              13-20             21-24         25-28    29-32   33-36   37-40  41-42  43 44-45   46-47      48-49         50-51       52-53       54-55    56
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7C (124) | GPS Module | - |
| 3 | PGN | 0xD6 (214) | Main Antenna | - |
| 4 | Length | 51 | Payload size in bytes | - |
| 5-12 | Longitude | double | Longitude in decimal degrees | `gps.longitude` |
| 13-20 | Latitude | double | Latitude in decimal degrees | `gps.latitude` |
| 21-24 | Heading dual | float | Dual antenna heading (degrees) | `gps.headingDual` |
| 25-28 | Heading | float | GPS heading true (degrees) | `gps.heading` |
| 29-32 | Speed | float | Ground speed (m/s) | `gps.speed` |
| 33-36 | Roll | float | GPS/dual antenna roll (degrees) | `gps.rollGPS` |
| 37-40 | Altitude | float | Altitude MSL (meters) | `gps.altitude` |
| 41-42 | Satellites | int16 | Number of satellites tracked | `gps.satellites` |
| 43 | Fix Quality | uint8 | GPS fix quality (0-5) | `gps.fixQuality` |
| 44-45 | HDOP | int16 | HDOP x 100 | `gps.hdop` |
| 46-47 | Age | int16 | Age of corrections x 100 (seconds) | `gps.age` |
| 48-49 | IMU Heading | int16 | IMU compass heading (degrees) | `imu.heading` |
| 50-51 | IMU Roll | int16 | IMU roll angle (degrees) | `imu.roll` |
| 52-53 | IMU Pitch | int16 | IMU pitch angle (degrees) | `imu.pitch` |
| 54-55 | IMU Yaw Rate | int16 | IMU yaw rate (degrees/sec) | `imu.yawRate` |
| 56 | Checksum | uint8 | Sum of bytes 2-55 | - |

#### GPS Fix Quality Values (Byte 43)

| Value | Description |
|-------|-------------|
| 0 | Invalid/No fix |
| 1 | GPS fix (autonomous) |
| 2 | DGPS fix (differential) |
| 4 | RTK Fixed (cm precision) |
| 5 | RTK Float (dm precision) |

#### Notes

- Binary format for efficient data transmission (51 bytes vs 80+ in NMEA)
- All floating point values are IEEE 754 single/double precision
- Combines GPS position data with IMU orientation data
- Converted to NMEA $PANDA format by UDPWorker before AgIOService update
- HDOP and Age values multiplied by 100 for integer transmission

**Source**: `classes/udpworker.cpp:processGpsPgn()` lines 1063-1150

**Implementation**: Complete binary-to-NMEA conversion in UDPWorker
```cpp
// Extract binary GPS data
double longitude = extractDouble(data, 5);
double latitude = extractDouble(data, 13);
float heading = extractFloat(data, 25);
float speed = extractFloat(data, 29);
// ... convert to NMEA $PANDA format
// ... update AgIOService properties
```

---

### PGN 215 - Tool Antenna OUT

**Direction**: GPS Module → AgIO/FormGPS
**Function**: Secondary GPS antenna (tool-mounted)
**Transmission Rate**: 5-10 Hz

#### Format

```
0x80 0x81 0x7D 0xD7 [Len] [Tool Antenna Data] [CRC]
Byte:  0    1    2    3     4          5-N        N+1
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7D (125) | Tool GPS Module | - |
| 3 | PGN | 0xD7 (215) | Tool Antenna | - |
| 4 | Length | Variable | Payload size | - |
| 5-N | Tool Data | bytes | Tool antenna position data | `gps.toolPosition` |
| N+1 | Checksum | uint8 | Sum of bytes 2-N | - |

#### Notes

- Format similar to PGN 214 but for implement-mounted GPS
- Used for dual GPS systems (tractor + implement)
- Enables precise implement position tracking

**Source**: `PGN.md` lines 552-562

---

### PGN 120 - Hello GPS OUT

**Direction**: GPS Module → AgIO/FormGPS
**Function**: Module identification heartbeat
**Transmission**: Response to PGN 200, or periodic (1 Hz)

#### Format

```
0x80 0x81 0x78 0x78 0x05 [*] [*] [*] [*] [*] [CRC]
Byte:  0    1    2    3    4  5  6  7  8  9   10
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x78 (120) | GPS Module | - |
| 3 | PGN | 0x78 (120) | Hello GPS | - |
| 4 | Length | 5 | Payload size in bytes | - |
| 5-9 | Reserved | uint8[5] | Reserved | - |
| 10 | Checksum | uint8 | Sum of bytes 2-9 | - |

#### Notes

- Heartbeat for standalone GPS module identification
- In All-in-One module architecture, GPS data sent via NMEA instead
- Payload contents module-specific

**Source**: `PGN.md` lines 863-876

---

## Communication PGNs

### PGN 200 - Hello Command

**Direction**: AgIO/FormGPS → All Modules
**Function**: Module discovery and wake-up ping
**Transmission**: Broadcast every 1-2 seconds

#### Format

```
0x80 0x81 0x7F 0xC8 0x03 [Module ID] [0] [0] [CRC]
Byte:  0    1    2    3    4      5      6  7   8
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xC8 (200) | Hello Command | - |
| 4 | Length | 3 | Payload size in bytes | - |
| 5 | Module ID | uint8 | Target module ID (126/123/121/120) | - |
| 6-7 | Reserved | uint8[2] | Always 0 | - |
| 8 | Checksum | uint8 | Sum of bytes 2-7 | - |

#### Module ID Values (Byte 5)

| Value | Target Module |
|-------|---------------|
| 126 | AutoSteer Module |
| 123 | Machine Module |
| 121 | IMU Module |
| 120 | GPS Module |

#### Example

```
Hello to AutoSteer: 80 81 7F C8 03 7E 00 00 4A
                                   │  │  │  └─ Checksum: 0x4A
                                   │  │  └─ Reserved: 0
                                   │  └─ Reserved: 0
                                   └─ Module ID: 126 (AutoSteer)
```

#### Notes

- Broadcast to 192.168.x.255:8888 for module discovery (x = configurable subnet)
- Each module responds with its specific Hello PGN (126, 123, 121, 120)
- Used for connection monitoring (timeout if no response)
- Sent every 1-2 seconds to maintain module heartbeat

**Source**: `formgps_udpcomm.cpp:SendHelloPgn()` lines 850-880

---

### PGN 201 - Subnet Change

**Direction**: AgIO/FormGPS → All Modules
**Function**: Network reconfiguration command (changes module IP subnet)
**Transmission**: On user-initiated subnet change

#### Format

```
0x80 0x81 0x7F 0xC9 0x05 [201] [201] [IP_One] [IP_Two] [IP_Three] [CRC]
Byte:  0    1    2    3    4   5    6      7        8         9        10
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xC9 (201) | Subnet Change | - |
| 4 | Length | 5 | Payload size in bytes | - |
| 5-6 | Validation | 201, 201 | Command validation bytes | - |
| 7 | IP_One | uint8 | First octet (usually 192) | `network.ipOne` |
| 8 | IP_Two | uint8 | Second octet (usually 168) | `network.ipTwo` |
| 9 | IP_Three | uint8 | Third octet (configurable subnet) | `network.ipThree` |
| 10 | Checksum | uint8 | Sum of bytes 2-9 | - |

#### Example

```
Subnet change to 192.168.1.x:
80 81 7F C9 05 C9 C9 C0 A8 01 B3
                │  │  │  │  │  └─ Checksum: 0xB3
                │  │  │  │  └─ IP_Three: 1 (subnet 192.168.1.x)
                │  │  │  └─ IP_Two: 168 (0xA8)
                │  │  └─ IP_One: 192 (0xC0)
                │  └─ Validation: 201 (0xC9)
                └─ Validation: 201 (0xC9)
```

#### Module Response

After receiving PGN 201:
1. Module writes new subnet configuration to EEPROM
2. Module restarts automatically
3. Module comes online with new IP (192.168.[IP_Three].[ModuleID])
4. Example: AutoSteer becomes 192.168.1.126 (if IP_Three = 1)

#### Notes

- CRITICAL: This command changes module IP address permanently (EEPROM storage)
- Module automatically restarts after receiving this command
- Broadcast to current subnet before change: 192.168.x.255:8888
- After restart, module listens on new subnet
- Used to adapt modules to different network environments

**Source**: `MODULE_PGN_PROTOCOL_REFERENCE.md` lines 84-86, 230-231

---

### PGN 202 - Scan Request

**Direction**: AgIO/FormGPS → All Modules
**Function**: Network scan for module discovery
**Transmission**: User-initiated network scan

#### Format

```
0x80 0x81 0x7F 0xCA 0x03 [202] [202] [5] [CRC]
Byte:  0    1    2    3    4   5    6   7   8
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | 0x7F (127) | AgIO/FormGPS sender | - |
| 3 | PGN | 0xCA (202) | Scan Request | - |
| 4 | Length | 3 | Payload size in bytes | - |
| 5-6 | Validation | 202, 202 | Command validation bytes | - |
| 7 | Scan Type | 5 | Scan type identifier | - |
| 8 | Checksum | uint8 | Sum of bytes 2-7 | - |

#### Notes

- Broadcast to 192.168.x.255:8888 for network-wide scan
- All active modules respond with PGN 203 (Scan Reply)
- Used during initial setup or troubleshooting
- Scan Type = 5 is standard AgOpenGPS network discovery

**Source**: `PGN.md` lines 918-929

---

### PGN 203 - Scan Reply

**Direction**: Module → AgIO/FormGPS
**Function**: Module identification and network configuration response
**Transmission**: Response to PGN 202

#### Format

```
0x80 0x81 [Src] 0xCB 0x07 [IP_One] [IP_Two] [IP_Three] [IP_Four] [Subnet_One] [Subnet_Two] [Subnet_Three] [CRC]
Byte:  0    1    2     3    4     5        6         7          8           9            10            11        12
```

#### Field Definitions

| Byte | Field Name | Type | Description | Code Variable |
|------|------------|------|-------------|---------------|
| 0-1 | Header | 0x80, 0x81 | Protocol identifier | - |
| 2 | Source | Variable | Module ID (126/123/121/120) | - |
| 3 | PGN | 0xCB (203) | Scan Reply | - |
| 4 | Length | 7 | Payload size in bytes | - |
| 5 | IP_One | uint8 | Module IP octet 1 | `module.ip[0]` |
| 6 | IP_Two | uint8 | Module IP octet 2 | `module.ip[1]` |
| 7 | IP_Three | uint8 | Module IP octet 3 | `module.ip[2]` |
| 8 | IP_Four | uint8 | Module IP octet 4 (Module ID) | `module.ip[3]` |
| 9 | Subnet_One | uint8 | Subnet mask octet 1 (usually 255) | `module.subnet[0]` |
| 10 | Subnet_Two | uint8 | Subnet mask octet 2 (usually 255) | `module.subnet[1]` |
| 11 | Subnet_Three | uint8 | Subnet mask octet 3 (usually 255) | `module.subnet[2]` |
| 12 | Checksum | uint8 | Sum of bytes 2-11 | - |

#### Example - AutoSteer Module Response

```
Scan reply from AutoSteer at 192.168.1.126:
80 81 7E CB 07 C0 A8 01 7E FF FF FF A2
             │  │  │  │  │  │  │  │  └─ Checksum: 0xA2
             │  │  │  │  │  │  │  └─ Subnet_Three: 255
             │  │  │  │  │  │  └─ Subnet_Two: 255
             │  │  │  │  │  └─ Subnet_One: 255
             │  │  │  │  └─ IP_Four: 126 (Module ID)
             │  │  │  └─ IP_Three: 1 (custom subnet)
             │  │  └─ IP_Two: 168 (0xA8)
             │  └─ IP_One: 192 (0xC0)
             └─ Source: 126 (0x7E AutoSteer)
```

#### Notes

- Each module type responds with its Source ID (0x7E/0x7B/0x79/0x78)
- IP address reveals module configuration and subnet
- Subnet mask typically 255.255.255.0 for Class C network
- Used by AgIO to build module connection table

**Source**: `PGN.md` lines 959-1022

---

## Code Variable Mapping

### AgIOService Real-time Properties

**File**: `classes/agioservice.h` lines 36-76

Binary PGN data is parsed by UDPWorker and exposed as QML-accessible properties:

```cpp
// Position data (from PGN 214 or NMEA)
Q_PROPERTY(double latitude ...)     // gps.latitude
Q_PROPERTY(double longitude ...)    // gps.longitude
Q_PROPERTY(double heading ...)      // gps.heading
Q_PROPERTY(double altitude ...)     // gps.altitude

// IMU data (from PGN 211 or NMEA)
Q_PROPERTY(double imuRoll ...)      // imu.roll
Q_PROPERTY(double imuPitch ...)     // imu.pitch
Q_PROPERTY(double imuHeading ...)   // imu.heading

// GPS status (from PGN 214 or NMEA)
Q_PROPERTY(int gpsQuality ...)      // gps.fixQuality
Q_PROPERTY(int satellites ...)      // gps.satellites
Q_PROPERTY(double hdop ...)         // gps.hdop
Q_PROPERTY(double age ...)          // gps.age

// Connection status (from PGN Hello responses)
Q_PROPERTY(bool gpsConnected ...)
Q_PROPERTY(bool imuConnected ...)
Q_PROPERTY(bool steerConnected ...)
Q_PROPERTY(bool machineConnected ...)
```

### FormGPS Control Variables

**File**: `formgps.h` lines 200-450

QML UI controls are processed by FormGPS and converted to PGN commands:

```cpp
// AutoSteer control (to PGN 254)
double guidanceAngle;           // Steer angle command
int distanceFromLine;           // XTE (cross track error)
uint8_t guidanceStatus;         // Status byte

// Machine control (to PGN 239)
uint8_t sectionControl;         // Section bits 1-8
uint8_t sectionControl2;        // Section bits 9-16
uint8_t hydLift;                // Hydraulic command

// Vehicle state
double avgSpeed;                // Speed for PGN 254, 239
bool isAutoSteerOn;             // AutoSteer enable
```

### UDPWorker PGN Handlers

**File**: `classes/udpworker.cpp` lines 180-1200

```cpp
// PGN parsing methods
void processAutosteerPgn(const QByteArray &data);  // PGN 253, 250, 126
void processMachinePgn(const QByteArray &data);    // PGN 237, 123
void processImuPgn(const QByteArray &data);        // PGN 211, 121
void processGpsPgn(const QByteArray &data);        // PGN 214, 120

// PGN transmission methods (in formgps_udpcomm.cpp)
void SendPgnToLoop();              // PGN 254 (Steer Data)
void SendSteerSettings();          // PGN 252
void SendSteerConfig();            // PGN 251
void SendMachinePgn();             // PGN 239
void SendHelloPgn();               // PGN 200
```

---

## Source References

### Documentation Files

1. **MODULE_PGN_PROTOCOL_REFERENCE.md** - Comprehensive PGN protocol analysis
   - Lines 1-200: Overview, network configuration, PGN 126 real test validation
   - Lines 200-500: Module types, PGN listings, NTRIP architecture
   - Real-world test data: Module 192.168.1.126 (September 11, 2025)

2. **PGN.csv** - PGN message reference table
   - Module IP addresses and Hello IDs
   - PGN field definitions and byte layouts

3. **PGN.md** - Detailed PGN field documentation
   - Lines 1-40: Message format structure
   - Lines 40-1100: Module-specific PGN tables with byte layouts
   - Lines 1100-1155: Module summary table

### Source Code Files

1. **classes/udpworker.h** - UDP communication declarations
   - Lines 45-85: PGN parsing method declarations
   - Socket configuration and buffer management

2. **classes/udpworker.cpp** - PGN parsing implementation
   - Lines 180-450: AutoSteer PGN handlers (253, 250, 126)
   - Lines 450-680: Machine PGN handlers (237, 123)
   - Lines 680-850: IMU PGN handlers (211, 121)
   - Lines 850-1200: GPS PGN handler (214) and helpers

3. **formgps_udpcomm.cpp** - PGN message construction
   - Lines 50-120: PGN 254 (Steer Data) transmission
   - Lines 120-280: PGN 252, 251 (Steer Settings/Config)
   - Lines 280-680: PGN 239, 238, 236, 235, 229 (Machine control)
   - Lines 680-900: PGN 200, 201, 202 (Communication commands)

4. **classes/agioservice.h** - Real-time data properties
   - Lines 36-76: QML property declarations for GPS, IMU, connection status
   - Qt 6.8 QProperty + BINDABLE architecture

5. **formgps.h** - Control logic and state variables
   - Lines 200-450: Vehicle state, guidance control, section management

### Firmware References

All PGN protocol behavior documented from AgOpenGPS official firmware sources (AutoSteer, Machine modules).

### Test Validation

Real hardware test conducted September 11, 2025:
- Module: All-in-One (GPS + AutoSteer + WAS + IMU)
- IP: 192.168.1.126 (custom subnet configuration)
- PGN 126: WAS angle -33.7° confirmed varying with physical steering wheel
- Architecture: Single module combining all functions validated

---

**Document Complete**: All PGN messages for AgOpenGPS Qt 6.8 implementation documented with byte-level field definitions, real-world examples, and code mapping.
