# PGN Protocol Implementation Analysis

**Status**: CRITICAL ERRORS IDENTIFIED
**Phase**: 6.0.21
**Last Updated**: 2025-12-06
**Objective**: Document fundamental protocol implementation errors requiring immediate fixes

---

## Executive Summary

UDPWorker PGN protocol implementation contains critical structural errors that prevent correct PGN message processing. The implementation incorrectly extracts PGN numbers from byte 2 (Source ID) instead of byte 3 (PGN Number), and uses invented PGN types (0x81-0x85) not present in the official protocol specification.

**Impact**: Hardware modules cannot communicate properly with FormGPS/AgIO due to incorrect protocol parsing.

**Severity**: CRITICAL - Protocol structure violations blocking module communication

---

## Critical Issues Summary

| Issue | Severity | Location | Impact |
|-------|----------|----------|--------|
| Incorrect PGN byte extraction | CRITICAL | [classes/udpworker.cpp:480](../../../classes/udpworker.cpp#L480) | All PGN messages misidentified |
| handlePGNProtocol() fundamentally broken | CRITICAL | [classes/udpworker.cpp:911-958](../../../classes/udpworker.cpp#L911-L958) | Switch on header byte instead of PGN |
| Missing checksum validation | HIGH | Throughout udpworker.cpp | Corrupted messages accepted |
| Missing essential PGN handlers | HIGH | Various | Cannot process AutoSteer, IMU, GPS data |
| Incorrect hello message handling | MEDIUM | [classes/udpworker.cpp:1695-1735](../../../classes/udpworker.cpp#L1695-L1735) | Single-byte logic never matches real messages |

---

## Critical Issue 1: Incorrect PGN Number Extraction

### Problem Location

File: [classes/udpworker.cpp:476-484](../../../classes/udpworker.cpp#L476-L484)

### Current Implementation (INCORRECT)

```cpp
else if (datagram.size() >= 3 &&
         static_cast<unsigned char>(datagram[0]) == 0x80 &&
         static_cast<unsigned char>(datagram[1]) == 0x81) {
    // PGN binary format (starts with 0x80 0x81)
    int pgnNumber = static_cast<unsigned char>(datagram[2]);  // WRONG BYTE
    qDebug() << "PGN" << pgnNumber << "received from" << senderIP
             << "- Size:" << datagram.size() << "bytes";
    emit pgnBinaryReceived(datagram, senderIP);
}
```

### Protocol Structure (From PGN_Sentences.md)

```
Byte 0:   0x80          Header byte 1
Byte 1:   0x81          Header byte 2
Byte 2:   Source ID     Module identifier (0x7E, 0x7B, 0x79, 0x7C, 0x7F)
Byte 3:   PGN Number    Message type (0xFE, 0xFD, 0xFC, etc.)
Byte 4:   Length        Payload size in bytes
Bytes 5-N: Payload      Message data
Byte N+1: Checksum      Sum of bytes 2 to N
```

### Error Analysis

**Line 480 Error**: `datagram[2]` extracts **Source ID**, NOT PGN number

**Source ID Values**:
- 0x7E (126) - AutoSteer module
- 0x7B (123) - Machine module
- 0x79 (121) - IMU module
- 0x7C (124) - GPS module
- 0x7F (127) - AgOpenGPS/AgIO

**Real PGN Numbers** (at `datagram[3]`):
- 254 (0xFE) - Steer Data IN
- 253 (0xFD) - AutoSteer Status OUT
- 126 (0x7E) - Hello AutoSteer OUT
- 211 (0xD3) - IMU Data OUT
- 214 (0xD6) - GPS Main Antenna OUT

### Impact Example

**Current Behavior**:
- PGN 253 from AutoSteer (0x7E) extracts 0x7E (126) as "PGN number"
- PGN 126 from AutoSteer (0x7E) extracts 0x7E (126) as "PGN number"
- **Both messages appear identical** due to extracting Source ID instead of PGN

**Correct Behavior**:
- PGN 253 from AutoSteer: Source=126, PGN=253 (distinct)
- PGN 126 from AutoSteer: Source=126, PGN=126 (distinct)

### Correct Implementation

```cpp
else if (datagram.size() >= 5 &&  // Minimum: header(2) + src(1) + pgn(1) + len(1)
         static_cast<unsigned char>(datagram[0]) == 0x80 &&
         static_cast<unsigned char>(datagram[1]) == 0x81) {
    // PGN binary format (starts with 0x80 0x81)
    unsigned char sourceId = static_cast<unsigned char>(datagram[2]);   // Source ID
    unsigned char pgnNumber = static_cast<unsigned char>(datagram[3]);  // PGN Number
    unsigned char length = static_cast<unsigned char>(datagram[4]);     // Payload length

    qDebug() << "PGN" << pgnNumber << "from source" << sourceId
             << "- Length:" << length << "Size:" << datagram.size() << "bytes";

    emit pgnBinaryReceived(datagram, senderIP);

    // Process PGN based on actual number
    handlePGNProtocol(datagram);
}
```

---

## Critical Issue 2: handlePGNProtocol() Fundamentally Broken

### Problem Location

File: [classes/udpworker.cpp:911-958](../../../classes/udpworker.cpp#L911-L958)

### Current Implementation (COMPLETELY WRONG)

```cpp
void UDPWorker::handlePGNProtocol(const QByteArray& data)
{
    if (data.size() < 3) {
        return; // Too short for PGN
    }

    // Check PGN header
    if (data[0] == char(0x80)) {  // Only checks FIRST header byte
        quint8 pgnType = static_cast<quint8>(data[1]);       // WRONG! This is 0x81 (header), NOT PGN
        quint8 sourceAddr = static_cast<quint8>(data[2]);    // Correct

        qDebug() << "PGN received - Type:" << QString::number(pgnType, 16)
                 << "Source:" << QString::number(sourceAddr, 16);

        switch (pgnType) {  // Switching on 0x81, not real PGN!
            case 0x81: // Hello message      INVENTED - not in protocol
                qDebug() << "Hello message from" << QString::number(sourceAddr, 16);
                break;

            case 0x82: // Module response    INVENTED - not in protocol
                processModuleResponse(data);
                break;

            case 0x83: // GPS data           INVENTED - not in protocol
                qDebug() << "GPS data from module" << QString::number(sourceAddr, 16);
                emit pgnDataReceived(data);
                incrementGpsCounters(data.size(), false);
                break;

            case 0x84: // IMU data           INVENTED - not in protocol
                qDebug() << "IMU data from module" << QString::number(sourceAddr, 16);
                emit pgnDataReceived(data);
                break;

            case 0x85: // Section control    INVENTED - not in protocol
                qDebug() << "Section data from module" << QString::number(sourceAddr, 16);
                emit pgnDataReceived(data);
                break;

            default:
                qDebug() << "Unknown PGN type:" << QString::number(pgnType, 16);
                emit pgnDataReceived(data);
                break;
        }
    }
}
```

### Identified Problems

1. **Line 918**: Only checks `data[0] == 0x80`, should validate BOTH header bytes (0x80 0x81)
2. **Line 919**: `pgnType = data[1]` extracts 0x81 (second header byte), NOT PGN number
3. **Line 925**: `switch(pgnType)` always switches on 0x81, never matches real PGNs
4. **Cases 0x81-0x85**: Completely invented numbers not present in official PGN specification

### Official PGN Numbers (From Protocol Specification)

**AutoSteer Module** (Source ID 0x7E = 126):
- 254 (0xFE) - Steer Data IN: Guidance commands from FormGPS
- 253 (0xFD) - AutoSteer Status OUT: Actual steer angle, IMU, PWM
- 252 (0xFC) - Steer Settings IN: PID parameters
- 251 (0xFB) - Steer Config IN: Hardware configuration
- 250 (0xFA) - AutoSteer Sensor OUT: Sensor readings
- 126 (0x7E) - Hello AutoSteer OUT: WAS angle + identification

**Machine Module** (Source ID 0x7B = 123):
- 239 (0xEF) - Machine Data IN: Section control commands
- 238 (0xEE) - Machine Config IN: Hydraulic timing
- 237 (0xED) - Machine Status OUT: Relay status feedback
- 236 (0xEC) - Pin Config IN: GPIO pin assignments (24 pins)
- 235 (0xEB) - Section Dimensions IN: Section widths (16 sections)
- 229 (0xE5) - 64 Sections IN: Extended section control
- 123 (0x7B) - Hello Machine OUT: Relay status

**IMU Module** (Source ID 0x79 = 121):
- 211 (0xD3) - IMU Data OUT: Heading, roll, gyro
- 121 (0x79) - Hello IMU OUT: Identification (empty payload)

**GPS Module** (Source ID 0x7C = 124):
- 214 (0xD6) - Main Antenna OUT: 51 bytes GPS + IMU binary data
- 215 (0xD7) - Tool Antenna OUT: Secondary GPS
- 120 (0x78) - Hello GPS OUT: Identification

**Communication** (Source ID 0x7F = 127 from AgIO/FormGPS):
- 200 (0xC8) - Hello Command: Module discovery ping
- 201 (0xC9) - Subnet Change: IP reconfiguration (EEPROM)
- 202 (0xCA) - Scan Request: Network scan
- 203 (0xCB) - Scan Reply: Module response with IP info

### Correct Implementation

```cpp
void UDPWorker::handlePGNProtocol(const QByteArray& data)
{
    // Minimum size validation
    if (data.size() < 5) {
        qDebug() << "PGN too short:" << data.size() << "bytes (minimum 5)";
        return;
    }

    // Validate PGN header (BOTH bytes)
    if (static_cast<quint8>(data[0]) != 0x80 ||
        static_cast<quint8>(data[1]) != 0x81) {
        qDebug() << "Invalid PGN header: 0x" << QString::number(static_cast<quint8>(data[0]), 16)
                 << "0x" << QString::number(static_cast<quint8>(data[1]), 16);
        return;
    }

    // Extract protocol fields
    quint8 sourceId = static_cast<quint8>(data[2]);      // Module ID
    quint8 pgnNumber = static_cast<quint8>(data[3]);     // PGN type
    quint8 length = static_cast<quint8>(data[4]);        // Payload length

    // Validate size matches declared length
    int expectedSize = 5 + length + 1; // header(2) + src + pgn + len + payload + checksum
    if (data.size() != expectedSize) {
        qDebug() << "PGN size mismatch: declared" << length
                 << "total expected" << expectedSize << "got" << data.size();
        return;
    }

    // Validate checksum
    if (!validatePGNChecksum(data)) {
        qDebug() << "PGN" << pgnNumber << "checksum validation failed";
        return;
    }

    qDebug() << "PGN" << pgnNumber << "from source" << sourceId
             << "length" << length << "validated";

    // Process PGN based on actual number
    switch (pgnNumber) {
        // AutoSteer Module PGNs
        case 253: // AutoSteer Status OUT
            processAutoSteerStatus(data, sourceId);
            break;

        case 250: // AutoSteer Sensor OUT
            processAutoSteerSensor(data, sourceId);
            break;

        case 126: // Hello AutoSteer OUT (with WAS angle)
            processHelloAutoSteer(data, sourceId);
            break;

        // Machine Module PGNs
        case 237: // Machine Status OUT
            processMachineStatus(data, sourceId);
            break;

        case 123: // Hello Machine OUT
            processHelloMachine(data, sourceId);
            break;

        // IMU Module PGNs
        case 211: // IMU Data OUT
            processImuData(data, sourceId);
            break;

        case 121: // Hello IMU OUT
            processHelloIMU(data, sourceId);
            break;

        // GPS Module PGNs
        case 214: // Main Antenna OUT (51 bytes)
            processGpsMainAntenna(data, sourceId);
            break;

        case 215: // Tool Antenna OUT
            processGpsToolAntenna(data, sourceId);
            break;

        case 120: // Hello GPS OUT
            processHelloGPS(data, sourceId);
            break;

        // Communication PGNs
        case 203: // Scan Reply (response to PGN 202)
            processScanReply(data, sourceId);
            break;

        default:
            qDebug() << "Unhandled PGN:" << pgnNumber << "from source" << sourceId;
            emit pgnDataReceived(data);
            break;
    }
}
```

---

## Critical Issue 3: Missing Checksum Validation

### Problem

No checksum validation found throughout [udpworker.cpp](../../../classes/udpworker.cpp). All PGN messages accepted without integrity verification.

### Protocol Requirement

**Checksum Calculation** (from PGN_Sentences.md):
```
Checksum = sum of bytes 2 to N (all bytes except 0x80 0x81 header and checksum byte)

Example:
0x80 0x81 0x7F 0xC8 0x03 0x7E 0x00 0x00 [CRC]
         └────────── Sum these bytes ──────┘
         0x7F + 0xC8 + 0x03 + 0x7E + 0x00 + 0x00 = 0x24A → 0x4A (low byte)
```

### Required Implementation

```cpp
bool UDPWorker::validatePGNChecksum(const QByteArray& data)
{
    if (data.size() < 6) {
        return false; // Too short for valid PGN
    }

    // Calculate checksum (sum bytes 2 to N-1)
    quint8 calculatedSum = 0;
    for (int i = 2; i < data.size() - 1; i++) {
        calculatedSum += static_cast<quint8>(data[i]);
    }

    // Get received checksum (last byte)
    quint8 receivedChecksum = static_cast<quint8>(data[data.size() - 1]);

    // Validate
    if (calculatedSum != receivedChecksum) {
        qDebug() << "Checksum mismatch - Calculated: 0x" << QString::number(calculatedSum, 16)
                 << "Received: 0x" << QString::number(receivedChecksum, 16);
        return false;
    }

    return true;
}
```

### Impact

**Current**: Corrupted messages accepted and processed, potential crashes or incorrect data interpretation

**Fixed**: Corrupted messages rejected, system stability and data integrity improved

---

## High Priority: Missing Essential PGN Handlers

### PGN 126 - Hello AutoSteer with WAS Angle

**Protocol Specification**: [PGN_Sentences.md lines 233-290](../../../PGN_Sentences.md#L233-L290)

**Real Test Data** (September 11, 2025 - Module 192.168.1.126):
```
80 81 7E 7E 05 F2 F2 A5 F1 07 47
      │  │  │  └──┴── WAS Angle: -33.74 degrees
      │  │  └─ Length: 5 bytes
      │  └─ PGN: 126 (Hello AutoSteer)
      └─ Source: 126 (0x7E AutoSteer)

WAS Angle Calculation:
Bytes 5-6: 0xF2F2 = -3374 (signed int16)
Angle = -3374 / 100.0 = -33.74 degrees (left turn)
```

**Required Implementation**:

```cpp
void UDPWorker::processHelloAutoSteer(const QByteArray& data, quint8 sourceId)
{
    // PGN 126: 11 bytes total
    // 0x80 0x81 [Src] 0x7E 0x05 [AngleLo] [AngleHi] [CountsLo] [CountsHi] [Switch] [CRC]
    if (data.size() != 11) {
        qDebug() << "PGN 126 invalid size:" << data.size() << "(expected 11)";
        return;
    }

    // Extract WAS angle (bytes 5-6, signed int16, value × 100)
    qint16 wasAngleRaw = static_cast<qint16>(
        (static_cast<quint8>(data[6]) << 8) | static_cast<quint8>(data[5])
    );
    double wasAngle = wasAngleRaw / 100.0;

    // Extract encoder counts (bytes 7-8, signed int16)
    qint16 wasCounts = static_cast<qint16>(
        (static_cast<quint8>(data[8]) << 8) | static_cast<quint8>(data[7])
    );

    // Extract switch status byte
    quint8 switchByte = static_cast<quint8>(data[9]);
    bool workSwitch = (switchByte & 0x01) != 0;
    bool steerSwitch = (switchByte & 0x02) != 0;
    bool moduleReady = (switchByte & 0x40) != 0;
    bool systemOK = (switchByte & 0x80) != 0;

    qDebug() << "PGN 126 Hello AutoSteer from" << sourceId;
    qDebug() << "   - WAS Angle:" << wasAngle << "degrees";
    qDebug() << "   - Encoder Counts:" << wasCounts;
    qDebug() << "   - Work:" << workSwitch << "Steer:" << steerSwitch
             << "Ready:" << moduleReady << "OK:" << systemOK;

    // Reset hello counter for traffic monitoring
    if (m_traffic->helloFromAutoSteer() != 0) {
        m_traffic->setHelloFromAutoSteer(0);
        emit moduleSteerStatusChanged(true);
    }

    // Emit WAS angle for real-time monitoring
    emit wasAngleReceived(wasAngle, wasCounts);
}
```

### PGN 253 - AutoSteer Status OUT

**Protocol Specification**: [PGN_Sentences.md lines 133-180](../../../PGN_Sentences.md#L133-L180)

**Format**:
```
0x80 0x81 0x7E 0xFD 0x08 [ActualAngle_Lo] [ActualAngle_Hi] [IMU_Heading_Lo] [IMU_Heading_Hi]
                         [IMU_Roll_Lo] [IMU_Roll_Hi] [Switch] [PWM] [CRC]
```

**Required Implementation**:

```cpp
void UDPWorker::processAutoSteerStatus(const QByteArray& data, quint8 sourceId)
{
    // PGN 253: 14 bytes total
    if (data.size() != 14) {
        qDebug() << "PGN 253 invalid size:" << data.size() << "(expected 14)";
        return;
    }

    // Extract actual steer angle (bytes 5-6, signed int16 × 100)
    qint16 steerAngleRaw = static_cast<qint16>(
        (static_cast<quint8>(data[6]) << 8) | static_cast<quint8>(data[5])
    );
    double steerAngle = steerAngleRaw / 100.0;

    // Extract IMU heading (bytes 7-8, signed int16 × 10)
    qint16 imuHeadingRaw = static_cast<qint16>(
        (static_cast<quint8>(data[8]) << 8) | static_cast<quint8>(data[7])
    );
    double imuHeading = imuHeadingRaw / 10.0;

    // Extract IMU roll (bytes 9-10, signed int16 × 10)
    qint16 imuRollRaw = static_cast<qint16>(
        (static_cast<quint8>(data[10]) << 8) | static_cast<quint8>(data[9])
    );
    double imuRoll = imuRollRaw / 10.0;

    // Extract switch and PWM
    quint8 switchByte = static_cast<quint8>(data[11]);
    quint8 pwm = static_cast<quint8>(data[12]);

    qDebug() << "PGN 253 AutoSteer Status from" << sourceId;
    qDebug() << "   - Actual Steer Angle:" << steerAngle << "degrees";
    qDebug() << "   - IMU Heading:" << imuHeading << "degrees";
    qDebug() << "   - IMU Roll:" << imuRoll << "degrees";
    qDebug() << "   - PWM:" << pwm << "Switch: 0x" << QString::number(switchByte, 16);

    // Update AgIOService properties
    emit autosteerStatusReceived(steerAngle, imuHeading, imuRoll, pwm, switchByte);
}
```

### PGN 211 - IMU Data OUT

**Protocol Specification**: [PGN_Sentences.md lines 505-560](../../../PGN_Sentences.md#L505-L560)

**Required Implementation**:

```cpp
void UDPWorker::processImuData(const QByteArray& data, quint8 sourceId)
{
    // PGN 211: 14 bytes total
    if (data.size() != 14) {
        qDebug() << "PGN 211 invalid size:" << data.size() << "(expected 14)";
        return;
    }

    // Extract IMU heading (bytes 5-6, signed int16 × 10)
    qint16 headingRaw = static_cast<qint16>(
        (static_cast<quint8>(data[6]) << 8) | static_cast<quint8>(data[5])
    );
    double heading = headingRaw / 10.0;

    // Extract roll (bytes 7-8, signed int16 × 10)
    qint16 rollRaw = static_cast<qint16>(
        (static_cast<quint8>(data[8]) << 8) | static_cast<quint8>(data[7])
    );
    double roll = rollRaw / 10.0;

    // Extract gyro (bytes 9-10, signed int16 × 10)
    qint16 gyroRaw = static_cast<qint16>(
        (static_cast<quint8>(data[10]) << 8) | static_cast<quint8>(data[9])
    );
    double gyro = gyroRaw / 10.0;

    qDebug() << "PGN 211 IMU Data from" << sourceId;
    qDebug() << "   - Heading:" << heading << "degrees";
    qDebug() << "   - Roll:" << roll << "degrees";
    qDebug() << "   - Gyro:" << gyro << "degrees/sec";

    // Update AgIOService IMU properties
    emit imuDataReceived(heading, roll, gyro);
}
```

### PGN 214 - GPS Main Antenna OUT (51 bytes binary)

**Protocol Specification**: [PGN_Sentences.md lines 591-715](../../../PGN_Sentences.md#L591-L715)

**Required Implementation**:

```cpp
void UDPWorker::processGpsMainAntenna(const QByteArray& data, quint8 sourceId)
{
    // PGN 214: 57 bytes total (header 5 + payload 51 + checksum 1)
    if (data.size() != 57) {
        qDebug() << "PGN 214 invalid size:" << data.size() << "(expected 57)";
        return;
    }

    // Extract binary data (bytes 5-55)
    // Longitude: bytes 5-12 (8 bytes double)
    double longitude;
    memcpy(&longitude, &data.constData()[5], sizeof(double));

    // Latitude: bytes 13-20 (8 bytes double)
    double latitude;
    memcpy(&latitude, &data.constData()[13], sizeof(double));

    // Heading: bytes 25-28 (4 bytes float)
    float heading;
    memcpy(&heading, &data.constData()[25], sizeof(float));

    // Speed: bytes 29-32 (4 bytes float)
    float speed;
    memcpy(&speed, &data.constData()[29], sizeof(float));

    // Fix quality: byte 43
    quint8 fixQuality = static_cast<quint8>(data[43]);

    // Satellites: bytes 41-42 (2 bytes int16)
    qint16 satellites = static_cast<qint16>(
        (static_cast<quint8>(data[42]) << 8) | static_cast<quint8>(data[41])
    );

    // IMU Heading: bytes 48-49 (2 bytes int16)
    qint16 imuHeading = static_cast<qint16>(
        (static_cast<quint8>(data[49]) << 8) | static_cast<quint8>(data[48])
    );

    // IMU Roll: bytes 50-51 (2 bytes int16)
    qint16 imuRoll = static_cast<qint16>(
        (static_cast<quint8>(data[51]) << 8) | static_cast<quint8>(data[50])
    );

    qDebug() << "PGN 214 GPS Main Antenna from" << sourceId;
    qDebug() << "   - Position:" << latitude << "," << longitude;
    qDebug() << "   - Heading:" << heading << "Speed:" << speed;
    qDebug() << "   - Fix Quality:" << fixQuality << "Satellites:" << satellites;
    qDebug() << "   - IMU Heading:" << imuHeading << "Roll:" << imuRoll;

    // Convert to NMEA $PANDA format for AgIOService
    QString nmeaPanda = buildNMEAFromPGN214(latitude, longitude, heading, speed,
                                             fixQuality, satellites, imuHeading, imuRoll);
    emit nmeaReceived(nmeaPanda, QString::number(sourceId));
}
```

---

## Medium Priority: processHelloMessage() Issues

### Problem Location

File: [classes/udpworker.cpp:1645-1736](../../../classes/udpworker.cpp#L1645-L1736)

### What Works Correctly

**PGN 203 Scan Reply** (lines 1647-1691):
```cpp
// CORRECT implementation for PGN 203
if (data.size() == 13 &&
    static_cast<quint8>(data[0]) == 0x80 &&
    static_cast<quint8>(data[1]) == 0x81 &&
    static_cast<quint8>(data[3]) == 203) {  // Correct: PGN at byte 3

    quint8 moduleId = static_cast<quint8>(data[2]);  // Correct: Source at byte 2
    // ... processing
}
```

### What's Incorrect

**Single-byte hello handling** (lines 1695-1735):
```cpp
// INCORRECT: Hello messages are NOT single-byte!
if (data.size() == 1) {
    quint8 moduleId = static_cast<quint8>(data[0]);
    // ... This will NEVER match real PGN hello messages
}
```

**Real Hello Message Sizes** (from protocol specification):
- PGN 126 (AutoSteer Hello): 11 bytes
- PGN 123 (Machine Hello): 11 bytes
- PGN 121 (IMU Hello): 11 bytes

### Recommended Fix

Remove single-byte handling and rely on proper PGN handlers in `handlePGNProtocol()`:

```cpp
void UDPWorker::processHelloMessage(const QByteArray& data)
{
    // Only handle PGN 203 scan replies here
    // Real hello messages (PGN 126, 123, 121) handled in handlePGNProtocol()

    if (data.size() == 13 &&
        static_cast<quint8>(data[0]) == 0x80 &&
        static_cast<quint8>(data[1]) == 0x81 &&
        static_cast<quint8>(data[3]) == 203) {

        quint8 moduleId = static_cast<quint8>(data[2]);
        bool trafficChanged = false;

        qDebug() << "PGN 203 scan reply from module:"
                 << QString::number(moduleId, 16);

        switch (moduleId) {
            case 123: // Machine
                if (m_traffic->helloFromMachine() != 0) {
                    m_traffic->setHelloFromMachine(0);
                    trafficChanged = true;
                }
                break;

            case 126: // AutoSteer
                if (m_traffic->helloFromAutoSteer() != 0) {
                    m_traffic->setHelloFromAutoSteer(0);
                    trafficChanged = true;
                }
                break;

            case 121: // IMU
                if (m_traffic->helloFromIMU() != 0) {
                    m_traffic->setHelloFromIMU(0);
                    trafficChanged = true;
                }
                break;
        }

        if (trafficChanged) {
            emitTrafficChangedThrottled();
        }
    }
}
```

---

## Implementation Roadmap

### Phase 1: IMMEDIATE (Critical Protocol Structure)

**Estimated Effort**: 1 day

1. Fix PGN number extraction - [classes/udpworker.cpp:480](../../../classes/udpworker.cpp#L480)
2. Rewrite `handlePGNProtocol()` - [classes/udpworker.cpp:911-958](../../../classes/udpworker.cpp#L911-L958)
3. Add `validatePGNChecksum()` method - New implementation
4. Remove single-byte hello handling - [classes/udpworker.cpp:1695-1735](../../../classes/udpworker.cpp#L1695-L1735)

### Phase 2: HIGH PRIORITY (Essential PGN Handlers)

**Estimated Effort**: 1-2 days

1. Implement PGN 126 handler (Hello AutoSteer with WAS angle)
2. Implement PGN 253 handler (AutoSteer Status)
3. Implement PGN 211 handler (IMU Data)
4. Implement PGN 214 handler (GPS binary data)

### Phase 3: MEDIUM PRIORITY (Complete Protocol Support)

**Estimated Effort**: 2-3 days

1. Implement PGN 254 handler (Steer Data IN commands)
2. Implement PGN 239 handler (Machine Data IN commands)
3. Implement PGN 237 handler (Machine Status OUT)
4. Implement remaining module hello handlers (121, 123, 120)

### Phase 4: LOW PRIORITY (Configuration PGNs)

**Estimated Effort**: 1-2 days

1. Implement PGN 252 handler (Steer Settings)
2. Implement PGN 251 handler (Steer Config)
3. Implement PGN 238 handler (Machine Config)
4. Implement PGN 236, 235, 229 handlers (Machine configuration)

**Total Estimated Effort**: 5-8 days for complete protocol implementation

---

## Testing Strategy

### Unit Tests Required

**Test 1: PGN Number Extraction**
```cpp
// Test data: PGN 126 from AutoSteer (0x7E)
QByteArray testPgn126 = QByteArray::fromHex("80817E7E05F2F2A5F10747");
// Expected: sourceId=126 (0x7E), pgnNumber=126 (0x7E)
// Incorrect extraction would yield: sourceId=126, pgnNumber=126 (but reading wrong bytes)
```

**Test 2: Checksum Validation**
```cpp
// Valid PGN with correct checksum
QByteArray validPgn = QByteArray::fromHex("80817FC803007E00004A");
// Checksum: 0x7F + 0xC8 + 0x03 + 0x7E + 0x00 + 0x00 = 0x24A → 0x4A (VALID)

// Corrupted PGN with invalid checksum
QByteArray corruptedPgn = QByteArray::fromHex("80817FC803007E0000FF");
// Checksum: 0xFF (wrong) - should be rejected
```

**Test 3: PGN 126 WAS Angle Decoding**
```cpp
QByteArray pgn126Test = QByteArray::fromHex("80817E7E05F2F2A5F10747");
// Expected WAS angle: 0xF2F2 = -3374 = -33.74 degrees (signed int16 / 100.0)
```

**Test 4: PGN 253 AutoSteer Status Decoding**
```cpp
QByteArray pgn253Test = QByteArray::fromHex("80817EFD08E8032003000000C18047");
// Expected: steerAngle=10.00°, imuHeading=80.0°, imuRoll=0°, pwm=128
```

### Integration Tests Required

1. **Real Module Communication Test**:
   - Connect to AutoSteer module at 192.168.1.126
   - Send PGN 200 (Hello) broadcast
   - Verify PGN 126 response with correct WAS angle extraction

2. **Multi-PGN Stream Test**:
   - Simulate mixed PGN traffic (253, 126, 211, 203)
   - Verify each PGN routed to correct handler
   - Verify checksum validation rejects corrupted messages

3. **Subnet Configuration Test**:
   - Send PGN 202 (Scan Request) to 192.168.x.255
   - Verify PGN 203 (Scan Reply) correct parsing
   - Extract module IP from PGN 203 payload

---

## Code Quality Assessment

### Strengths

1. **Traffic Monitoring**: Qt 6.8 QProperty + BINDABLE architecture well-implemented
2. **Multi-subnet Discovery**: Comprehensive subnet scanning logic (lines 1004+)
3. **Error Logging**: Good debug output for troubleshooting
4. **Thread Safety**: Proper Qt signal/slot usage for cross-thread communication

### Critical Weaknesses

1. **Protocol Compliance**: Does not follow PGN_Sentences.md specification
2. **Checksum Validation**: Missing - security and reliability concern
3. **PGN Handler Coverage**: Only 1 PGN (203) properly handled out of 20+ defined
4. **Magic Numbers**: Should use named constants for PGN numbers and Source IDs

---

## Recommended Code Structure

### Header Constants (udpworker.h)

```cpp
// PGN Protocol Constants
namespace PGN {
    // AutoSteer Module PGNs
    constexpr quint8 STEER_DATA_IN = 254;           // 0xFE
    constexpr quint8 AUTOSTEER_STATUS_OUT = 253;    // 0xFD
    constexpr quint8 STEER_SETTINGS_IN = 252;       // 0xFC
    constexpr quint8 STEER_CONFIG_IN = 251;         // 0xFB
    constexpr quint8 AUTOSTEER_SENSOR_OUT = 250;    // 0xFA
    constexpr quint8 HELLO_AUTOSTEER_OUT = 126;     // 0x7E

    // Machine Module PGNs
    constexpr quint8 MACHINE_DATA_IN = 239;         // 0xEF
    constexpr quint8 MACHINE_CONFIG_IN = 238;       // 0xEE
    constexpr quint8 PIN_CONFIG_IN = 236;           // 0xEC
    constexpr quint8 SECTION_DIMENSIONS_IN = 235;   // 0xEB
    constexpr quint8 MACHINE_STATUS_OUT = 237;      // 0xED
    constexpr quint8 SECTIONS_64_IN = 229;          // 0xE5
    constexpr quint8 HELLO_MACHINE_OUT = 123;       // 0x7B

    // IMU Module PGNs
    constexpr quint8 IMU_DATA_OUT = 211;            // 0xD3
    constexpr quint8 HELLO_IMU_OUT = 121;           // 0x79

    // GPS Module PGNs
    constexpr quint8 GPS_MAIN_ANTENNA_OUT = 214;    // 0xD6
    constexpr quint8 GPS_TOOL_ANTENNA_OUT = 215;    // 0xD7
    constexpr quint8 HELLO_GPS_OUT = 120;           // 0x78

    // Communication PGNs
    constexpr quint8 HELLO_COMMAND = 200;           // 0xC8
    constexpr quint8 SUBNET_CHANGE = 201;           // 0xC9
    constexpr quint8 SCAN_REQUEST = 202;            // 0xCA
    constexpr quint8 SCAN_REPLY = 203;              // 0xCB
}

// Source ID Constants
namespace SourceID {
    constexpr quint8 AGOPENGPS = 127;     // 0x7F
    constexpr quint8 AUTOSTEER = 126;     // 0x7E
    constexpr quint8 MACHINE = 123;       // 0x7B
    constexpr quint8 IMU = 121;           // 0x79
    constexpr quint8 GPS = 124;           // 0x7C
    constexpr quint8 TOOL_GPS = 125;      // 0x7D
}
```

### Method Declarations (udpworker.h)

```cpp
private:
    // PGN Protocol Methods
    bool validatePGNChecksum(const QByteArray& data);

    // PGN Handlers - AutoSteer
    void processHelloAutoSteer(const QByteArray& data, quint8 sourceId);
    void processAutoSteerStatus(const QByteArray& data, quint8 sourceId);
    void processAutoSteerSensor(const QByteArray& data, quint8 sourceId);

    // PGN Handlers - Machine
    void processHelloMachine(const QByteArray& data, quint8 sourceId);
    void processMachineStatus(const QByteArray& data, quint8 sourceId);

    // PGN Handlers - IMU
    void processHelloIMU(const QByteArray& data, quint8 sourceId);
    void processImuData(const QByteArray& data, quint8 sourceId);

    // PGN Handlers - GPS
    void processHelloGPS(const QByteArray& data, quint8 sourceId);
    void processGpsMainAntenna(const QByteArray& data, quint8 sourceId);
    void processGpsToolAntenna(const QByteArray& data, quint8 sourceId);

    // PGN Handlers - Communication
    void processScanReply(const QByteArray& data, quint8 sourceId);

signals:
    // PGN-specific signals
    void wasAngleReceived(double angle, qint16 counts);
    void autosteerStatusReceived(double angle, double heading, double roll, quint8 pwm, quint8 switchByte);
    void imuDataReceived(double heading, double roll, double gyro);
```

---

## Impact Assessment

**Current State**: Module communication non-functional due to fundamental protocol parsing errors

**After Phase 1 Fixes**: Basic protocol structure correct, checksum validation active

**After Phase 2 Implementation**: Essential module communication (AutoSteer, IMU, GPS) operational

**After Phase 3 Completion**: Full protocol support with all defined PGN handlers

**Risk**: HIGH - Current implementation blocks hardware module integration

**Priority**: CRITICAL - Required for basic system functionality

---

## References

**Related Documentation**:
- [PGN Protocol Reference](../protocols/nmea-pgn-architecture.md) - Complete protocol specification
- [AgIOService Architecture](../architecture/agioservice-architecture.md) - Communication coordinator
- [Threading Architecture](../implementation/threading-architecture.md) - Thread communication patterns

**Source Files**:
- [classes/udpworker.h](../../../classes/udpworker.h) - UDPWorker header declarations
- [classes/udpworker.cpp](../../../classes/udpworker.cpp) - UDPWorker implementation
- [formgps_udpcomm.cpp](../../../formgps_udpcomm.cpp) - PGN message construction (FormGPS side)

**Protocol Documentation**:
- [PGN_Sentences.md](../../../PGN_Sentences.md) - Official PGN protocol specification
- [PGN.csv](../../../PGN.csv) - PGN message reference table
- [PGN.md](../../../PGN.md) - Detailed field layouts

**Test Data Sources**:
- Real hardware test: September 11, 2025 - Module 192.168.1.126
- PGN 126 Response: `80 81 7E 7E 05 F2 F2 A5 F1 07 47`
- WAS Angle: -33.74 degrees (confirmed with physical steering wheel)

---

**Status**: CRITICAL ERRORS IDENTIFIED
**Analysis Phase**: 6.0.21
**Implementation Required**: YES - Immediate fixes needed for module communication
**Estimated Total Effort**: 5-8 days (all phases)
