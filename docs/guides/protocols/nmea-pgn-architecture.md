# NMEA/PGN Protocol Architecture

**Status**: IMPLEMENTED ✅
**Phase**: 6.0.21+ (Centralized Parser), 6.0.25+ (Separated Streams)
**Last Validated**: 2025-12-06

Complete architectural guide to NMEA and PGN protocol implementation in QtAgOpenGPS, including parser design, data flow, and system integration.

## Overview

QtAgOpenGPS implements a comprehensive NMEA/PGN protocol architecture for GPS positioning, IMU data, and hardware module communication. The system uses a **centralized parser** (Phase 6.0.21) with **separated data streams** (Phase 6.0.25) for optimal performance and maintainability.

### Supported Protocols

**NMEA-0183 Text Sentences** (GPS Data):
- **$GPGGA / $GNGGA**: GPS fix data (position, quality, satellites)
- **$GPVTG / $GNVTG**: Track and ground speed
- **$GPHDT / $GNHDT**: True heading
- **$PANDA**: AgOpenGPS single antenna + IMU fusion
- **$PAOGI**: AgOpenGPS dual antenna + IMU fusion
- **$AVR**: Trimble dual antenna attitude (PTNL,AVR)
- **$KSXT**: Unicore integrated GNSS/IMU

**PGN Binary Packets** (Hardware Modules):
- **PGN 211**: IMU data (roll, pitch, heading, yaw rate)
- **PGN 253**: AutoSteer status OUT (steer angle, switches, PWM)
- **PGN 254**: AutoSteer data IN (guidance commands)
- **PGN 214**: GPS main antenna OUT (position, heading)
- **PGN 239**: Machine data IN (section control, hydraulics)

### Architecture Principles

1. **Centralized Parsing**: Single PGNParser class for all NMEA and PGN parsing
2. **Separated Data Streams**: Optimized routing via specific signals (nmeaDataReady, imuDataReady, steerDataReady)
3. **Protocol-Centric Tracking**: Dynamic unlimited protocols via QMap<QString, ModuleStatus>
4. **Thread Safety**: UDP on main thread, real-time DirectConnection signal delivery
5. **Source Tracking**: Transport type (UDP/Serial), source ID, module type identification

## Parser Architecture

### Centralized Parser (Phase 6.0.21)

**Single Source of Truth**: PGNParser eliminates duplicate parsing code across GPSWorker, AgIOService, and UDPWorker.

**Implementation** ([classes/pgnparser.h:26-120](../../../classes/pgnparser.h#L26-L120)):

```cpp
class PGNParser : public QObject {
    Q_OBJECT

public:
    // Unified parsed data structure
    struct ParsedData {
        // GPS data
        double latitude, longitude, altitude;
        double speed, heading, headingDual, headingHDT;
        int quality, satellites;
        double hdop, age;

        // IMU data
        double imuHeading, imuRoll, imuPitch, yawRate;
        bool hasIMU;

        // AutoSteer data (PGN 253/250)
        int16_t steerAngleActual;
        uint8_t switchByte, pwmDisplay, sensorValue;
        bool hasSteerData;

        // Metadata
        QString sourceType;          // "NMEA" or "PGN"
        QString sentenceType;        // "GGA", "PANDA", etc.
        int pgnNumber;               // 211, 253, 254, etc.
        bool isValid;

        // Source tracking (Phase 6.0.22.1)
        QString sourceTransport;     // "UDP" or "Serial"
        QString sourceID;            // IP address or COM port
        QString moduleType;          // "GPS", "IMU", "Steer", etc.
        qint64 timestampMs;          // Reception timestamp
    };

    // Main parsers - auto-detect format
    ParsedData parse(const QByteArray& data);       // Auto-detect NMEA or PGN
    ParsedData parseNMEA(const QString& sentence);  // NMEA text
    ParsedData parsePGN(const QByteArray& binary);  // PGN binary

    // Validation
    static bool isValidNMEA(const QString& sentence);
    static bool isValidPGN(const QByteArray& data);
    static bool validateChecksum(const QString& sentence);
};
```

**Auto-Detection Logic**:

```cpp
ParsedData PGNParser::parse(const QByteArray& data) {
    // NMEA text detection (starts with '$')
    if (data.startsWith('$')) {
        QString sentence = QString::fromUtf8(data).trimmed();
        return parseNMEA(sentence);
    }

    // PGN binary detection (starts with 0x80 0x81)
    if (data.size() >= 2 && data[0] == 0x80 && data[1] == 0x81) {
        return parsePGN(data);
    }

    // Invalid format
    ParsedData invalidData;
    invalidData.isValid = false;
    return invalidData;
}
```

### NMEA Parser

**Supported Sentences** ([docs/reference/nmea-sentences.md](../../reference/nmea-sentences.md)):

| Sentence | Purpose | Key Fields | Frequency |
|----------|---------|------------|-----------|
| **$GPGGA** | GPS fix data | Lat/Lon, Quality, Satellites, HDOP, Age | 10 Hz |
| **$GPVTG** | Track/Speed | Speed (km/h), Course over ground | 10 Hz |
| **$GPHDT** | True heading | Heading (single antenna calculated) | 10 Hz |
| **$PANDA** | AgOpenGPS single antenna + IMU | Lat/Lon, Heading, Roll, Pitch, Yaw rate | 10 Hz |
| **$PAOGI** | AgOpenGPS dual antenna + IMU | Lat/Lon, Dual heading, Roll, Pitch | 10 Hz |

**NMEA Parsing Logic** ([classes/pgnparser.cpp:180-290](../../../classes/pgnparser.cpp#L180-L290)):

```cpp
ParsedData PGNParser::parseNMEA(const QString& sentence) {
    ParsedData data;
    data.sourceType = "NMEA";

    // Validate checksum
    if (!validateChecksum(sentence)) {
        data.isValid = false;
        return data;
    }

    // Extract sentence type
    data.sentenceType = extractSentenceType(sentence);  // "GGA", "VTG", etc.

    // Split into fields
    QStringList fields = splitNMEA(sentence);

    // Route to specific parser
    if (data.sentenceType == "GGA") {
        parseGGA(fields, data);
    } else if (data.sentenceType == "VTG") {
        parseVTG(fields, data);
    } else if (data.sentenceType == "HDT") {
        parseHDT(fields, data);
    } else if (data.sentenceType == "PANDA") {
        parsePANDA(fields, data);  // Includes IMU data
    } else if (data.sentenceType == "PAOGI") {
        parsePAOGI(fields, data);  // Dual antenna + IMU
    }
    // ... more sentence types

    data.isValid = true;
    return data;
}
```

**Example: $PANDA Parsing** (Single Antenna + IMU):

```cpp
void PGNParser::parsePANDA(const QStringList& fields, ParsedData& data) {
    // Field 0: $PANDA (sentence ID)
    // Field 1-2: Latitude (ddmm.mmmmmmm, N/S)
    // Field 3-4: Longitude (dddmm.mmmmmmm, E/W)
    // Field 5: Quality (0-9)
    // Field 6: Satellites
    // Field 7: HDOP
    // Field 8: Altitude
    // Field 9: Age
    // Field 10: Speed (km/h)
    // Field 11: Heading (degrees)
    // Field 12: IMU Heading (degrees)
    // Field 13: IMU Roll (degrees)
    // Field 14: IMU Pitch (degrees)
    // Field 15: Yaw Rate (degrees/sec × 10 as integer)

    data.latitude = convertNMEAtoDD(fields[1], fields[2]);
    data.longitude = convertNMEAtoDD(fields[3], fields[4]);
    data.quality = fields[5].toInt();
    data.satellites = fields[6].toInt();
    data.hdop = fields[7].toDouble();
    data.altitude = fields[8].toDouble();
    data.age = fields[9].toDouble();
    data.speed = fields[10].toDouble();
    data.heading = fields[11].toDouble();

    // IMU data (fields 12-15)
    data.imuHeading = fields[12].toDouble();
    data.imuRoll = fields[13].toDouble();
    data.imuPitch = fields[14].toDouble();
    data.yawRate = fields[15].toDouble() / 10.0;  // Integer × 10
    data.hasIMU = true;
}
```

### PGN Parser

**Supported PGN Packets** ([docs/reference/pgn-sentences.md](../../reference/pgn-sentences.md)):

| PGN | Direction | Purpose | Frequency |
|-----|-----------|---------|-----------|
| **211** | Module → AgIO | IMU data (roll, pitch, heading, yaw) | 10 Hz |
| **214** | Module → AgIO | GPS main antenna (position, heading) | 10 Hz |
| **253** | Module → AgIO | AutoSteer status (steer angle, switches, PWM) | 40 Hz |
| **254** | AgIO → Module | AutoSteer data (guidance commands) | 40 Hz |
| **239** | AgIO → Module | Machine data (section control, hydraulics) | 4 Hz |

**PGN Binary Format**:

```
Byte 0-1: Header (0x80 0x81)
Byte 2:   PGN Number (211, 253, 254, etc.)
Byte 3:   Data Length (N bytes)
Byte 4-N: Data payload
Byte N+1: Checksum (XOR of all bytes)
```

**PGN Parsing Logic**:

```cpp
ParsedData PGNParser::parsePGN(const QByteArray& binaryData) {
    ParsedData data;
    data.sourceType = "PGN";

    // Validate header
    if (binaryData.size() < 5 ||
        binaryData[0] != 0x80 || binaryData[1] != 0x81) {
        data.isValid = false;
        return data;
    }

    // Extract PGN number
    data.pgnNumber = static_cast<uint8_t>(binaryData[2]);

    // Validate checksum
    if (!validatePGNChecksum(binaryData)) {
        data.isValid = false;
        return data;
    }

    // Route to specific PGN parser
    switch (data.pgnNumber) {
        case 211: parsePGN211_IMU(binaryData, data); break;
        case 214: parsePGN214_GPS(binaryData, data); break;
        case 253: parsePGN253_AutoSteer(binaryData, data); break;
        // ... more PGN types
    }

    data.isValid = true;
    return data;
}
```

**Example: PGN 211 (IMU Data)**:

```cpp
void PGNParser::parsePGN211_IMU(const QByteArray& data, ParsedData& parsed) {
    // PGN 211: IMU Data
    // Byte 0-1: Header (0x80 0x81)
    // Byte 2: PGN 211
    // Byte 3: Length (8 bytes)
    // Byte 4-5: Roll (int16 × 100 degrees)
    // Byte 6-7: Pitch (int16 × 100 degrees)
    // Byte 8-9: Heading (int16 × 100 degrees)
    // Byte 10-11: Yaw Rate (int16 × 100 degrees/sec)
    // Byte 12: Checksum

    int16_t roll = (data[5] << 8) | data[4];    // Little endian
    int16_t pitch = (data[7] << 8) | data[6];
    int16_t heading = (data[9] << 8) | data[8];
    int16_t yawRate = (data[11] << 8) | data[10];

    parsed.imuRoll = roll / 100.0;
    parsed.imuPitch = pitch / 100.0;
    parsed.imuHeading = heading / 100.0;
    parsed.yawRate = yawRate / 100.0;
    parsed.hasIMU = true;
}
```

## Data Flow Architecture

### Complete Pipeline

**NMEA Text Data Flow** (GPS Data):

```
GPS Module (NMEA UART/USB)
    ↓
UDP Port 8888 ($PANDA, $GGA, $VTG)
    ↓
AgIOService::onUDPDataReceived()
    ↓
PGNParser::parseNMEA()
    ↓
ParsedData structure
    ↓
emit nmeaDataReady(parsedData)  // Phase 6.0.25 separated signal
    ↓
FormGPS::onNmeaDataReady() (Qt::DirectConnection)
    ↓
UpdateFixPosition() (10 Hz)
    ↓
Update FormGPS properties (Qt 6.8 auto-notifies QML)
    m_latitude, m_longitude, m_heading, m_speedKph
    ↓
QML UI updates automatically
```

**PGN Binary Data Flow** (IMU/AutoSteer):

```
IMU Module (PGN 211 binary)
    ↓
UDP Port 9999 (0x80 0x81 0xD3 ...)
    ↓
AgIOService::onUDPDataReceived()
    ↓
PGNParser::parsePGN()
    ↓
ParsedData structure (pgnNumber=211, imuRoll/Pitch/Heading)
    ↓
emit imuDataReady(parsedData)  // Phase 6.0.25 separated signal
    ↓
FormGPS::onImuDataReady() (Qt::DirectConnection)
    ↓
ahrs.ApplyIMU() - Roll compensation
    ↓
Update vehicle roll/pitch
```

**AutoSteer Data Flow** (PGN 253 Status):

```
AutoSteer Module (PGN 253 binary)
    ↓
UDP Port 9999
    ↓
AgIOService::onUDPDataReceived()
    ↓
PGNParser::parsePGN()
    ↓
ParsedData (pgnNumber=253, steerAngleActual, switchByte, pwmDisplay)
    ↓
emit steerDataReady(parsedData)  // Phase 6.0.25 separated signal
    ↓
FormGPS::onSteerDataReady() (Qt::DirectConnection)
    ↓
mc.ParseModuleArduinoReply() - Process steer response
    ↓
Update steer state properties
```

### Separated Data Streams (Phase 6.0.25)

**Optimized Routing**: Specific signals eliminate conditional routing overhead.

**Implementation** ([classes/agioservice.h:signals](../../../classes/agioservice.h)):

```cpp
class AgIOService : public QObject {
    Q_OBJECT

signals:
    // Phase 6.0.25: Separated data streams for optimal routing
    void parsedDataReady(const PGNParser::ParsedData& data);  // Broadcast (all data)
    void nmeaDataReady(const PGNParser::ParsedData& data);    // GPS position only
    void imuDataReady(const PGNParser::ParsedData& data);     // IMU data only
    void steerDataReady(const PGNParser::ParsedData& data);   // AutoSteer only
};
```

**Signal Emission Logic**:

```cpp
void AgIOService::onUDPDataReceived() {
    QByteArray data = m_udpSocket->readDatagram(...);

    // Parse data
    PGNParser::ParsedData parsedData = m_pgnParser->parse(data);

    if (!parsedData.isValid) return;

    // Broadcast to all listeners
    emit parsedDataReady(parsedData);

    // Phase 6.0.25: Emit separated signals for optimal routing
    if (parsedData.sourceType == "NMEA") {
        emit nmeaDataReady(parsedData);  // GPS position processing only
    } else if (parsedData.pgnNumber == 211) {
        emit imuDataReady(parsedData);   // IMU roll compensation only
    } else if (parsedData.pgnNumber == 253 || parsedData.pgnNumber == 250) {
        emit steerDataReady(parsedData); // AutoSteer processing only
    }
}
```

**Connection Pattern** ([formgps.cpp:58-68](../../../formgps.cpp#L58-L68)):

```cpp
// Phase 6.0.25: Connect separated data signals
connect(m_agioService, &AgIOService::nmeaDataReady,
        this, &FormGPS::onNmeaDataReady, Qt::DirectConnection);

connect(m_agioService, &AgIOService::imuDataReady,
        this, &FormGPS::onImuDataReady, Qt::DirectConnection);

connect(m_agioService, &AgIOService::steerDataReady,
        this, &FormGPS::onSteerDataReady, Qt::DirectConnection);
```

**Benefits**:
- **No Routing Overhead**: Each handler processes only relevant data (no if-else chains)
- **Type Safety**: Compile-time verification of signal/slot signatures
- **Clarity**: Single-purpose signal handlers improve code readability
- **Performance**: Direct signal delivery without conditional routing

## Protocol-Centric Tracking (Phase 6.0.22.12)

**Dynamic Unlimited Protocols**: QMap-based tracking replaces fixed 4-module array.

**Implementation** ([classes/agioservice.h:47-62](../../../classes/agioservice.h#L47-L62)):

```cpp
// Module status tracking structure
struct ModuleStatus {
    QString transport;          // "UDP" or "Serial"
    QString sourceID;           // "192.168.1.126" or "COM3"
    QString protocolID;         // "$PANDA", "PGN211", "$GGA", etc.
    QString description;        // "GPS Main Antenna", "IMU Data", etc.
    int frequency;              // Update frequency (Hz)
    qint64 lastSeenMs;          // Last reception timestamp
    bool isActive;              // Active within timeout period
};

// Dynamic protocol tracking (unlimited protocols)
QMap<QString, ModuleStatus> m_protocolStatusMap;  // Key: protocolID
```

**Protocol ID Generation**:

```cpp
QString generateProtocolID(const PGNParser::ParsedData& data) {
    QString protocolId;

    if (data.sourceType == "NMEA") {
        protocolId = "$" + data.sentenceType;  // "$PANDA", "$GGA", etc.
    } else if (data.sourceType == "PGN") {
        protocolId = "PGN" + QString::number(data.pgnNumber);  // "PGN211", etc.
    }

    return protocolId;
}
```

**Status Update Logic**:

```cpp
void AgIOService::updateProtocolStatus(const PGNParser::ParsedData& data) {
    QString protocolId = generateProtocolID(data);

    // Create or update status
    ModuleStatus& status = m_protocolStatusMap[protocolId];
    status.transport = data.sourceTransport;
    status.sourceID = data.sourceID;
    status.protocolID = protocolId;
    status.description = getProtocolDescription(protocolId);
    status.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    status.isActive = true;

    // Calculate frequency (smoothed)
    if (m_protocolLastUpdateMs.contains(protocolId)) {
        qint64 delta = status.lastSeenMs - m_protocolLastUpdateMs[protocolId];
        if (delta > 0) {
            int instantHz = 1000 / delta;
            status.frequency = 0.9 * status.frequency + 0.1 * instantHz;
        }
    }
    m_protocolLastUpdateMs[protocolId] = status.lastSeenMs();

    // Emit QML-accessible list
    emit activeProtocolsChanged();
}
```

**QML Integration**:

```qml
ListView {
    model: AgIOService.activeProtocols  // QVariantList of protocol status

    delegate: Row {
        Text { text: modelData.protocolID }      // "$PANDA"
        Text { text: modelData.sourceID }        // "192.168.1.126"
        Text { text: modelData.frequency + " Hz" }  // "10 Hz"
        Rectangle {
            color: modelData.isActive ? "green" : "red"
            width: 20; height: 20
        }
    }
}
```

## Integration Patterns

### Pattern 1: GPS Position Update (NMEA)

**Complete Flow** ([formgps.cpp:59-60](../../../formgps.cpp#L59-L60), [formgps_position.cpp:34-150](../../../formgps_position.cpp#L34-L150)):

```cpp
// Step 1: AgIOService receives UDP data
void AgIOService::onUDPDataReceived() {
    QByteArray data = m_udpSocket->readDatagram(...);
    PGNParser::ParsedData parsed = m_pgnParser->parseNMEA(data);

    // Step 2: Emit separated NMEA signal
    emit nmeaDataReady(parsed);
}

// Step 3: FormGPS receives GPS data
void FormGPS::onNmeaDataReady(const PGNParser::ParsedData& data) {
    // Update CNMEA structure
    pn.fix.latitude = data.latitude;
    pn.fix.longitude = data.longitude;
    pn.fix.altitude = data.altitude;
    pn.quality = data.quality;
    pn.satellites = data.satellites;
    pn.hdop = data.hdop;
    pn.age = data.age;

    // Step 4: Trigger position update (10 Hz)
    UpdateFixPosition();
}

// Step 5: Update FormGPS properties (Qt 6.8 auto-notifies QML)
void FormGPS::UpdateFixPosition() {
    // Calculate position, heading, speed
    CalculatePositionHeading();

    // Update properties - automatic QML notification
    setLatitude(pn.fix.latitude);
    setLongitude(pn.fix.longitude);
    setHeading(gpsHeading());
    setSpeedKph(pn.speed);

    // QML UI updates automatically via Qt 6.8 BINDABLE
}
```

### Pattern 2: IMU Data Update (PGN 211)

**Implementation**:

```cpp
// Step 1: AgIOService receives PGN 211 binary
void AgIOService::onUDPDataReceived() {
    QByteArray data = m_udpSocket->readDatagram(...);
    PGNParser::ParsedData parsed = m_pgnParser->parsePGN(data);  // PGN 211

    // Step 2: Emit separated IMU signal
    emit imuDataReady(parsed);
}

// Step 3: FormGPS applies IMU data
void FormGPS::onImuDataReady(const PGNParser::ParsedData& data) {
    // Apply roll compensation
    ahrs.ApplyIMU(data.imuRoll, data.imuPitch, data.imuHeading);

    // Update vehicle orientation
    vehicle->rollAngle = data.imuRoll;
    vehicle->pitchAngle = data.imuPitch;

    // Compensate tool position for roll
    CalculateToolPositionWithRoll();
}
```

### Pattern 3: AutoSteer Status (PGN 253)

**Implementation**:

```cpp
// Step 1: AgIOService receives PGN 253
void AgIOService::onUDPDataReceived() {
    PGNParser::ParsedData parsed = m_pgnParser->parsePGN(data);  // PGN 253

    // Step 2: Emit separated steer signal
    emit steerDataReady(parsed);
}

// Step 3: FormGPS processes steer status
void FormGPS::onSteerDataReady(const PGNParser::ParsedData& data) {
    // Update AutoSteer state
    mc.wasValue = data.steerAngleActual;
    mc.switchByte = data.switchByte;
    mc.pwmDisplay = data.pwmDisplay;

    // Check work/steer switch states (bit 0=work, bit 1=steer)
    bool workSwitchActive = (data.switchByte & 0x01) != 0;
    bool steerSwitchActive = (data.switchByte & 0x02) != 0;

    // Update UI properties
    setWorkSwitchOn(workSwitchActive);
    setSteerSwitchOn(steerSwitchActive);
}
```

## Real-World Examples

### Example 1: $PANDA Sentence Reception

**Scenario**: Single antenna GPS module with BNO085 IMU sends $PANDA sentence at 10 Hz.

**NMEA Sentence**:
```
$PANDA,5107.1234,N,00007.5678,W,4,12,0.8,100.5,1.2,15.3,045.2,045.1,2.5,-1.2,35*6A
```

**Parsed Fields**:
- Latitude: 51.118723° N (51°07.1234' N)
- Longitude: 0.126130° W (000°07.5678' W)
- Quality: 4 (RTK Fixed)
- Satellites: 12
- HDOP: 0.8
- Altitude: 100.5 m
- Age: 1.2 seconds
- Speed: 15.3 km/h
- Heading: 45.2° (GPS calculated)
- IMU Heading: 45.1° (BNO085)
- IMU Roll: 2.5°
- IMU Pitch: -1.2°
- Yaw Rate: 3.5°/s (35 ÷ 10)

**Code Flow**:
```cpp
// 1. UDP reception
AgIOService receives "$PANDA,5107.1234,N..." on port 8888

// 2. Parsing
PGNParser::parseNMEA() identifies sentence type "PANDA"
parsePANDA() extracts 16 fields
Creates ParsedData with all GPS + IMU values

// 3. Signal emission
emit nmeaDataReady(parsedData)  // GPS position
// Note: $PANDA includes IMU, but routed as NMEA GPS data

// 4. FormGPS processing
onNmeaDataReady() receives data
UpdateFixPosition() calculates position (10 Hz)
Properties updated: latitude=51.118723, longitude=-0.126130, heading=45.2

// 5. QML UI
Text { text: aog.latitude.toFixed(6) }  // Displays "51.118723"
```

### Example 2: PGN 211 IMU Data

**Scenario**: Standalone IMU module (Arduino Nano 33 BLE Sense) sends PGN 211 at 10 Hz.

**Binary Packet**:
```
0x80 0x81 0xD3 0x08 0xFA 0x00 0xF4 0xFF 0xB8 0x11 0x23 0x00 0xXX
 ^    ^    ^    ^    ^--- Roll (250 = 2.50°)
 |    |    |    |    |         Pitch (-12 = -0.12°)
 |    |    |    |    |         Heading (4536 = 45.36°)
 |    |    |    |    |         Yaw Rate (35 = 0.35°/s)
 |    |    |    |
 |    |    |    Data length (8 bytes)
 |    |    PGN 211 (0xD3 = 211)
 |    Header byte 2
 Header byte 1
```

**Parsed Values**:
- Roll: 2.50° (0x00FA = 250 → 250/100 = 2.50)
- Pitch: -0.12° (0xFFF4 = -12 → -12/100 = -0.12)
- Heading: 45.36° (0x11B8 = 4536 → 4536/100 = 45.36)
- Yaw Rate: 0.35°/s (0x0023 = 35 → 35/100 = 0.35)

**Code Flow**:
```cpp
// 1. UDP reception
AgIOService receives binary 0x80 0x81 0xD3... on port 9999

// 2. Parsing
PGNParser::parsePGN() identifies PGN 211
parsePGN211_IMU() extracts roll/pitch/heading/yaw
Creates ParsedData with imuRoll=2.50, imuPitch=-0.12, etc.

// 3. Signal emission
emit imuDataReady(parsedData)  // IMU only

// 4. FormGPS processing
onImuDataReady() receives data
ahrs.ApplyIMU(2.50, -0.12, 45.36)  // Roll compensation
vehicle->rollAngle = 2.50
CalculateToolPositionWithRoll()  // Adjust tool for roll

// 5. Field display
Tool position shifted laterally by: width/2 * sin(2.50°) = 0.044 * width
```

### Example 3: Protocol-Centric Tracking

**Scenario**: Multiple GPS sources send data simultaneously.

**Active Protocols**:
1. $PANDA from UDP 192.168.1.126 (10 Hz)
2. $GGA from Serial COM3 (5 Hz)
3. PGN 214 from UDP 192.168.1.124 (10 Hz)

**Protocol Status Map**:

```cpp
m_protocolStatusMap = {
    "$PANDA": {
        transport: "UDP",
        sourceID: "192.168.1.126",
        protocolID: "$PANDA",
        description: "Single Antenna + IMU",
        frequency: 10,
        lastSeenMs: 1701958234567,
        isActive: true
    },
    "$GGA": {
        transport: "Serial",
        sourceID: "COM3",
        protocolID: "$GGA",
        description: "GPS Fix Data",
        frequency: 5,
        lastSeenMs: 1701958234123,
        isActive: true
    },
    "PGN214": {
        transport: "UDP",
        sourceID: "192.168.1.124",
        protocolID: "PGN214",
        description: "GPS Main Antenna",
        frequency: 10,
        lastSeenMs: 1701958234890,
        isActive: true
    }
}
```

**QML Display**:
```qml
ListView {
    model: AgIOService.activeProtocols

    delegate: Row {
        Text { text: modelData.protocolID }     // "$PANDA"
        Text { text: modelData.sourceID }       // "192.168.1.126"
        Text { text: modelData.transport }      // "UDP"
        Text { text: modelData.frequency + " Hz" }  // "10 Hz"
        Rectangle {
            color: modelData.isActive ? "green" : "red"
            width: 20; height: 20
        }
    }
}
```

## Debugging Protocol Integration

### Enable Protocol Debug Logging

**Selective Logging** ([main.cpp:47-55](../../../main.cpp#L47-L55)):

```cpp
QLoggingCategory::setFilterRules(
    "*.debug=true\n"
    "agioservice.debug=true\n"  // Enable AgIOService protocol debug
    "*.warning=true\n"
);
```

**Output**:
```
[agioservice.debug] UDP received 78 bytes from 192.168.1.126
[agioservice.debug] NMEA detected: $PANDA,5107.1234,N,...
[agioservice.debug] Parsed: Lat=51.118723, Lon=-0.126130, Quality=4
[agioservice.debug] Protocol: $PANDA, Frequency: 10 Hz
[agioservice.debug] Emitting nmeaDataReady signal
```

### Common Issues

**Issue 1: No GPS Data Received**

**Symptoms**: QML displays 0.0 latitude/longitude despite GPS module connected.

**Debugging**:

```cpp
// 1. Check UDP socket binding
qDebug() << "UDP socket bound:" << m_udpSocket->state();
qDebug() << "Listening on port:" << m_udpSocket->localPort();

// 2. Check data reception
void AgIOService::onUDPDataReceived() {
    qDebug() << "Received" << data.size() << "bytes";
    qDebug() << "Data:" << QString::fromUtf8(data);
}

// 3. Check parser validity
PGNParser::ParsedData parsed = m_pgnParser->parse(data);
qDebug() << "Parsed valid:" << parsed.isValid;
qDebug() << "Latitude:" << parsed.latitude;
```

**Common Causes**:
- Wrong UDP port (check port 8888)
- Firewall blocking UDP traffic
- GPS module sending to wrong IP address
- NMEA checksum validation failing

**Issue 2: Separated Signals Not Working**

**Symptoms**: onNmeaDataReady() never called despite parsedDataReady() working.

**Debugging**:

```cpp
// Check signal emission logic
void AgIOService::onUDPDataReceived() {
    PGNParser::ParsedData parsed = m_pgnParser->parse(data);

    qDebug() << "Parsed sourceType:" << parsed.sourceType;
    qDebug() << "PGN number:" << parsed.pgnNumber;

    if (parsed.sourceType == "NMEA") {
        qDebug() << "Emitting nmeaDataReady";
        emit nmeaDataReady(parsed);
    } else if (parsed.pgnNumber == 211) {
        qDebug() << "Emitting imuDataReady";
        emit imuDataReady(parsed);
    }
}

// Check signal connection
connect(m_agioService, &AgIOService::nmeaDataReady,
        this, &FormGPS::onNmeaDataReady, Qt::DirectConnection);
qDebug() << "nmeaDataReady connection:" << isConnected;
```

**Common Causes**:
- Signal not connected (check connect() call)
- Wrong connection type (should be Qt::DirectConnection)
- sourceType not set correctly in parser

**Issue 3: Protocol Tracking Shows Wrong Frequency**

**Symptoms**: Protocol status shows 0 Hz despite data arriving.

**Debugging**:

```cpp
void AgIOService::updateProtocolStatus(const PGNParser::ParsedData& data) {
    QString protocolId = generateProtocolID(data);
    qDebug() << "Protocol ID:" << protocolId;

    ModuleStatus& status = m_protocolStatusMap[protocolId];
    qDebug() << "Last seen:" << status.lastSeenMs;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qDebug() << "Current time:" << now;

    qint64 delta = now - m_protocolLastUpdateMs.value(protocolId, now);
    qDebug() << "Delta:" << delta << "ms";

    if (delta > 0) {
        int instantHz = 1000 / delta;
        qDebug() << "Instant Hz:" << instantHz;
    }
}
```

**Common Causes**:
- Frequency calculation before first update (delta = 0)
- Integer division by zero protection missing
- Clock skew or timer resolution issues

## Best Practices

### DO

1. **Use PGNParser for all NMEA/PGN parsing** - Single source of truth
2. **Connect separated signals** (nmeaDataReady, imuDataReady, steerDataReady) for optimal routing
3. **Validate checksums** before processing data
4. **Use Qt::DirectConnection** for real-time signal delivery (10+ Hz)
5. **Track protocols dynamically** via QMap for unlimited protocol support

### DON'T

1. **Don't duplicate parsing logic** - Use centralized PGNParser
2. **Don't use parsedDataReady for everything** - Use separated signals
3. **Don't skip checksum validation** - Invalid data causes crashes
4. **Don't use Qt::QueuedConnection for high-frequency data** - Causes latency
5. **Don't hardcode 4 modules** - Use dynamic protocol tracking

## Architecture Achievements

**Phase 6.0.21+ Status**:

- **Centralized Parser**: Single PGNParser eliminates duplicate code
- **8 NMEA Sentences**: Full support for $GGA, $VTG, $HDT, $PANDA, $PAOGI, $AVR, $HPD, $KSXT
- **5+ PGN Packets**: Support for PGN 211 (IMU), 214 (GPS), 253/254 (AutoSteer), 239 (Machine)
- **Separated Data Streams**: Optimized routing via specific signals (Phase 6.0.25)
- **Protocol-Centric Tracking**: Dynamic unlimited protocol support (Phase 6.0.22.12)
- **10 Hz GPS Updates**: Real-time position with Qt::DirectConnection
- **Thread Safety**: Main thread UDP with worker thread I/O coordination

**NMEA/PGN Protocol Architecture Complete**: Centralized parsing, separated data streams, protocol-centric tracking, and optimal performance achieved.

## See Also

- [NMEA Sentences Reference](../../reference/nmea-sentences.md) - Complete sentence format specifications
- [PGN Sentences Reference](../../reference/pgn-sentences.md) - Complete PGN packet specifications
- [AgIOService Architecture](../architecture/agioservice-architecture.md) - Hardware coordinator details
- [System Integration](../architecture/system-integration.md) - Complete data flow patterns
- [Threading and Timers](../architecture/threading-timers.md) - 10 Hz GPS optimization
