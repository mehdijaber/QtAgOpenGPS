# AgIOService Architecture

**Status**: IMPLEMENTED ✅
**Phase**: 6.0.45+ (includes Phase 6.0.24 event-driven UDP, Phase 6.0.22.12 protocol tracking)
**Last Validated**: 2025-09-15

Comprehensive guide to AgIOService hardware communication coordinator, worker thread management, protocol parsing, and real-time data distribution.

## Overview

AgIOService is the **main thread coordinator** for all hardware communication in QtAgOpenGPS. It manages GPS data reception, RTK corrections (NTRIP), serial module communication, and UDP networking through specialized worker threads and event-driven I/O.

**Core Design Pattern**: **Thread Coordinator** - Main thread coordination with worker thread I/O for zero-latency access

**Key Architecture Decisions**:
- **Phase 6.0.21**: GPSWorker removed (no actual I/O operations)
- **Phase 6.0.24**: UDPWorker removed, replaced with event-driven `QUdpSocket` in main thread
- **Phase 6.0.22.12**: Protocol-centric tracking (dynamic unlimited protocols, not fixed 4 modules)
- **Phase 6.0.25**: Separated data streams for optimal routing (NMEA, IMU, Steer)

## Architecture Diagram

```
┌─────────────────── MAIN THREAD ─────────────────────┐
│                                                      │
│  ┌─ AgIOService (Coordinator) ───────────────────┐  │
│  │ - Thread management                           │  │
│  │ - PGNParser (centralized NMEA/PGN parsing)   │  │
│  │ - QUdpSocket (event-driven, ports 9999/8888) │  │
│  │ - Protocol tracking (dynamic, unlimited)      │  │
│  │ - 54 BINDABLE properties (status monitoring)  │  │
│  └──────────┬───────────────────────────────────┬─┘  │
│             │ Commands                          │     │
│             │ (Qt::QueuedConnection)            │     │
│             ↓                                   ↓     │
│  ┌─ NTRIPWorker ──┐            ┌─ SerialWorker ──┐   │
│  │ (Worker Thread)│            │ (Worker Thread) │   │
│  │ - RTK data     │            │ - Arduino I/O   │   │
│  │ - Network I/O  │            │ - Serial ports  │   │
│  └────────┬───────┘            └────────┬────────┘   │
│           │ Data                        │ Data        │
│           │ (Qt::DirectConnection)      │             │
│           ↓                             ↓             │
│  ┌─ Data Distribution (Main Thread) ─────────────┐   │
│  │ - parsedDataReady(ParsedData) [broadcast]    │   │
│  │ - nmeaDataReady(ParsedData) [GPS position]   │   │
│  │ - imuDataReady(ParsedData) [external IMU]    │   │
│  │ - steerDataReady(ParsedData) [AutoSteer]     │   │
│  └──────────────────────────────────────────────┘   │
│             ↓                                        │
│  ┌─ FormGPS (App Engine) ──────────────────────┐   │
│  │ - onParsedDataReady() → UpdateFixPosition() │   │
│  │ - Business logic processing                  │   │
│  └──────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────┘

┌─────────────── EXTERNAL HARDWARE ───────────────┐
│  Module (AIO Firmware)                          │
│  ↓ UDP 9999 (NMEA text + PGN binary)            │
│  AgIOService receives and parses                │
│  ↓ UDP 8888 (PGN commands to modules)           │
│  Module processes commands                      │
└──────────────────────────────────────────────────┘
```

## Core Components

### AgIOService Main Class

**File**: [classes/agioservice.h](../../../classes/agioservice.h), [classes/agioservice.cpp](../../../classes/agioservice.cpp)

**Purpose**: Main thread coordinator managing hardware communication

**QML Access**: `AgIOService.*` (singleton)

**Pattern**: QML_SINGLETON ([agioservice.h:43-44](../../../classes/agioservice.h#L43-L44))

**Key Characteristics**:
- **Thread**: Main thread (zero-latency access for OpenGL 30Hz rendering)
- **Properties**: 54 Q_OBJECT_BINDABLE_PROPERTY for real-time status
- **Workers**: Manages NTRIPWorker and SerialWorker threads
- **UDP**: Event-driven QUdpSocket (Phase 6.0.24, replaced UDPWorker thread)
- **Parser**: Centralized PGNParser for NMEA text and PGN binary

### PGNParser - Centralized Protocol Parser

**File**: [classes/pgnparser.h](../../../classes/pgnparser.h), [classes/pgnparser.cpp](../../../classes/pgnparser.cpp)

**Purpose**: Unified parser for NMEA sentences (text) and PGN packets (binary)

**Phase**: 6.0.21 (centralized architecture)

**Supported Protocols**:

**NMEA Sentences** (text format, `$`-prefixed):
- **$GGA**: GPS Fix Data (position, altitude, fix quality)
- **$VTG**: Track and Speed (velocity, course over ground)
- **$HDT**: Heading True (compass heading)
- **$PANDA**: Single Antenna + IMU (GPS + inertial data)
- **$PAOGI**: Dual Antenna + IMU (precision heading)
- **$AVR**: Trimble Dual Antenna Attitude
- **$HPD**: High Precision Distance
- **$KSXT**: Integrated GNSS/IMU

**PGN Packets** (binary format, `0x80 0x81` header):
- **PGN 254 (0xFE)**: Steer Data IN (real-time steering commands)
- **PGN 253 (0xFD)**: AutoSteer Status OUT (~700 Hz, actual steer angle, IMU, switches, PWM)
- **PGN 211 (0xD3)**: External IMU Data OUT (~10 Hz, heading, roll, gyro)
- **PGN 214 (0xD6)**: Main Antenna OUT (GPS position data)
- **PGN 239 (0xEF)**: Machine Data IN (machine state and control)
- **PGN 229 (0xE5)**: 64 Sections IN (extended section control)
- **PGN 200-203**: Communication (Hello, Subnet Change, Scan)

**ParsedData Structure**:
```cpp
struct ParsedData {
    QString sourceType;        // "NMEA" or "PGN"
    QString sentenceType;      // "GGA", "PANDA", etc. (NMEA only)
    int pgnNumber;             // 211, 214, 253, etc. (PGN only)
    QString transport;         // "UDP", "Serial", "NTRIP"
    QString sourceID;          // IP address or COM port
    QMap<QString, QVariant> data;  // Parsed field values
    qint64 timestampMs;        // Reception timestamp
};
```

### Worker Threads

#### NTRIPWorker - RTK Correction Data

**File**: [classes/ntripworker.h](../../../classes/ntripworker.h), [classes/ntripworker.cpp](../../../classes/ntripworker.cpp)

**Purpose**: Network I/O for RTK correction data (NTRIP protocol)

**Thread**: Dedicated worker thread (managed by AgIOService)

**Characteristics**:
- **Protocol**: NTRIP (Networked Transport of RTCM via Internet Protocol)
- **Data**: RTCM correction messages for high-precision GPS
- **Frequency**: Keep-alive every 10 seconds, real-time corrections
- **Connection**: HTTP-based streaming connection to NTRIP caster

**Communication**:
```cpp
// Main → Worker (Qt::QueuedConnection)
emit requestStartNTRIP(url, user, password, mount, port);
emit requestStopNTRIP();

// Worker → Main (Qt::DirectConnection)
connect(m_ntripWorker, &NTRIPWorker::ntripDataReceived,
        this, &AgIOService::onNtripDataReceived, Qt::DirectConnection);
connect(m_ntripWorker, &NTRIPWorker::ntripStatusChanged,
        this, &AgIOService::onNtripStatusChanged, Qt::DirectConnection);
```

#### SerialWorker - Arduino Module Communication

**File**: [classes/serialworker.h](../../../classes/serialworker.h), [classes/serialworker.cpp](../../../classes/serialworker.cpp)

**Purpose**: Serial I/O for Arduino-based modules (AutoSteer, IMU, Machine)

**Thread**: Dedicated worker thread (managed by AgIOService)

**Supported Ports**:
- **GPS**: Primary GPS receiver (COM port)
- **GPS2**: Secondary GPS receiver (dual antenna systems)
- **IMU**: Inertial measurement unit
- **AutoSteer**: Steering control module
- **Machine**: Implement control module
- **Radio**: Wireless communication module
- **RTCM**: RTK correction receiver
- **Tool**: Tool-specific sensors

**Communication**:
```cpp
// Main → Worker (Qt::QueuedConnection)
emit requestSerialWorkerStart();
bool openSerialPortAsync(portType, portName, baudRate);

// Worker → Main (Qt::DirectConnection)
connect(m_serialWorker, &SerialWorker::serialIMUDataReceived,
        this, &AgIOService::onSerialIMUDataReceived, Qt::DirectConnection);
connect(m_serialWorker, &SerialWorker::serialAutosteerResponse,
        this, &AgIOService::onSerialAutosteerResponse, Qt::DirectConnection);
```

### UDP Event-Driven Architecture (Phase 6.0.24)

**Implementation**: Main thread `QUdpSocket` (event-driven, not worker thread)

**Rationale**: UDP is non-blocking, dedicated thread adds latency. Event-driven provides zero-latency access for OpenGL rendering (30Hz) and AutoSteer (40Hz WAS).

**Ports**:
- **9999**: Listen (receive from modules)
- **8888**: Send (transmit to modules)

**Socket Initialization**: [agioservice.cpp](../../../classes/agioservice.cpp) `initializeUdpSocket()`
```cpp
m_udpSocket = new QUdpSocket(this);
m_udpSocket->bind(QHostAddress::Any, m_listenPort);  // 9999

connect(m_udpSocket, &QUdpSocket::readyRead,
        this, &AgIOService::onUdpDataReady, Qt::DirectConnection);
connect(m_udpSocket, &QUdpSocket::errorOccurred,
        this, &AgIOService::onUdpError, Qt::DirectConnection);
```

**Data Reception**:
```cpp
void AgIOService::onUdpDataReady() {
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(),
                                 &sender, &senderPort);

        // Parse with PGNParser
        PGNParser::ParsedData data = m_pgnParser->parse(datagram, "UDP", sender.toString());

        // Update protocol tracking
        updateModuleStatus(protocolId, "UDP", sender.toString(), currentTime);

        // Broadcast to consumers
        emit parsedDataReady(data);
    }
}
```

## Protocol Tracking System (Phase 6.0.22.12)

### Dynamic Protocol-Centric Architecture

**Design Change**: From fixed 4-module tracking to dynamic unlimited protocol tracking

**Before** (Phase 6.0.22.10):
```cpp
// Fixed 4 properties
Q_PROPERTY(QString gpsProtocol ...)
Q_PROPERTY(QString imuProtocol ...)
Q_PROPERTY(QString steerProtocol ...)
Q_PROPERTY(QString machineProtocol ...)

// Module-centric map (4 keys only)
QMap<QString, ModuleStatus> m_moduleStatusMap;  // "GPS", "IMU", "Steer", "Machine"
```

**After** (Phase 6.0.22.12):
```cpp
// Single dynamic property
Q_PROPERTY(QVariantList activeProtocols READ activeProtocols
           NOTIFY activeProtocolsChanged)

// Protocol-centric map (unlimited keys)
QMap<QString, ModuleStatus> m_protocolStatusMap;  // "$PANDA", "$GGA", "PGN211", "PGN253", etc.
```

### ModuleStatus Structure

**File**: [agioservice.h:47-62](../../../classes/agioservice.h#L47-L62)

```cpp
struct ModuleStatus {
    QString transport;         // "UDP" or "Serial"
    QString sourceID;          // IP address or COM port
    qint64 lastSeenMs = 0;     // Last data reception timestamp
    qint64 packetCount = 0;    // Total packets received
    double frequency = 0.0;    // Calculated Hz
    bool isActive = false;     // Active if data received within 2 seconds

    // Frequency calculation window (1 second)
    qint64 freqStartTime = 0;
    qint64 freqPacketCount = 0;
};
```

### Protocol ID Generation

**Logic**:
```cpp
QString protocolId;
if (parsedData.sourceType == "NMEA") {
    protocolId = "$" + parsedData.sentenceType;  // "$PANDA", "$GGA", "$VTG"
} else if (parsedData.sourceType == "PGN") {
    protocolId = "PGN" + QString::number(parsedData.pgnNumber);  // "PGN211", "PGN253"
}
```

### Active Protocols Property

**QML Access**: `AgIOService.activeProtocols` (dynamic list)

**Return Format**:
```javascript
[
    {
        id: "$PANDA",
        description: "Single Antenna + IMU",
        source: "UDP:192.168.1.126",
        frequency: 10.0
    },
    {
        id: "PGN253",
        description: "AutoSteer Status OUT",
        source: "UDP:192.168.1.121",
        frequency: 700.0
    },
    {
        id: "PGN211",
        description: "IMU Data OUT",
        source: "Serial:COM3",
        frequency: 10.0
    }
]
```

**Implementation**: [agioservice.cpp](../../../classes/agioservice.cpp) `activeProtocols()`
```cpp
QVariantList AgIOService::activeProtocols() const
{
    QVariantList protocols;

    for (auto it = m_protocolStatusMap.constBegin(); it != m_protocolStatusMap.constEnd(); ++it) {
        const ModuleStatus& status = it.value();

        // Only include active protocols (received data within last 2 seconds)
        if (!status.isActive) continue;

        QVariantMap protocol;
        protocol["id"] = it.key();  // "$PANDA" or "PGN211"
        protocol["description"] = getProtocolDescription(it.key());
        protocol["source"] = QString("%1:%2").arg(status.transport).arg(status.sourceID);
        protocol["frequency"] = status.frequency;

        protocols.append(protocol);
    }

    return protocols;
}
```

### Protocol Descriptions

**Hardcoded descriptions** extracted from official documentation ([NMEA_Sentences.md](../../reference/nmea-sentences.md), [PGN_Sentences.md](../../reference/pgn-sentences.md)):

| Protocol ID | Description | Source |
|-------------|-------------|--------|
| **$GGA** | GPS Fix Data | NMEA |
| **$VTG** | Track and Speed | NMEA |
| **$HDT** | Heading True | NMEA |
| **$PANDA** | Single Antenna + IMU | NMEA |
| **$PAOGI** | Dual Antenna + IMU | NMEA |
| **PGN254** | Steer Data IN | PGN |
| **PGN253** | AutoSteer Status OUT | PGN |
| **PGN211** | IMU Data OUT | PGN |
| **PGN214** | Main Antenna OUT | PGN |
| **PGN239** | Machine Data IN | PGN |
| **PGN229** | 64 Sections IN | PGN |

**Implementation**: [agioservice.cpp](../../../classes/agioservice.cpp) `getProtocolDescription()`

## Data Flow and Signal Routing (Phase 6.0.25)

### Separated Data Streams

**Phase 6.0.25 Optimization**: Instead of broadcasting ALL packets to FormGPS, separate streams for optimal routing:

**1. parsedDataReady(ParsedData)** - Broadcast (all consumers)
- **Frequency**: Variable (10 Hz GPS + 700 Hz AutoSteer + events)
- **Consumers**: All components that need raw protocol data
- **Usage**: Logging, debugging, protocol monitoring

**2. nmeaDataReady(ParsedData)** - GPS Position Data (NMEA only)
- **Frequency**: ~10 Hz
- **Consumers**: FormGPS → UpdateFixPosition()
- **Filters**: Only NMEA sentences ($GGA, $PANDA, etc.)
- **Purpose**: Position updates for navigation

**3. imuDataReady(ParsedData)** - External IMU Data (PGN 211 only)
- **Frequency**: ~10 Hz
- **Consumers**: FormGPS IMU processing
- **Filters**: Only PGN 211 (External IMU Data OUT)
- **Purpose**: Heading, roll, gyro updates

**4. steerDataReady(ParsedData)** - AutoSteer Feedback (PGN 253/250)
- **Frequency**: ~700 Hz (high-frequency real-time data)
- **Consumers**: FormGPS steering logic
- **Filters**: Only PGN 253 (AutoSteer Status OUT) and PGN 250 (Sensor OUT)
- **Purpose**: Real-time steering angle, switches, PWM feedback
- **Important**: Does NOT trigger UpdateFixPosition() (performance optimization)

### Signal Emission Logic

```cpp
void AgIOService::onDataReceived(const QByteArray& data, const QString& transport, const QString& sourceID) {
    // Parse with PGNParser
    PGNParser::ParsedData parsedData = m_pgnParser->parse(data, transport, sourceID);

    // Update protocol tracking
    QString protocolId = generateProtocolId(parsedData);
    updateModuleStatus(protocolId, transport, sourceID, currentTime);

    // Broadcast to all consumers (universal signal)
    emit parsedDataReady(parsedData);

    // Route to specific consumers based on data type (Phase 6.0.25)
    if (parsedData.sourceType == "NMEA") {
        emit nmeaDataReady(parsedData);  // GPS position → UpdateFixPosition()
    } else if (parsedData.sourceType == "PGN") {
        if (parsedData.pgnNumber == 211) {
            emit imuDataReady(parsedData);  // External IMU → IMU processing
        } else if (parsedData.pgnNumber == 253 || parsedData.pgnNumber == 250) {
            emit steerDataReady(parsedData);  // AutoSteer → Steering logic (NO UpdateFixPosition)
        }
    }
}
```

## Command Pattern for Worker Coordination

### Main Thread → Worker Thread (Commands)

**Pattern**: Qt::QueuedConnection for cross-thread safety

**NTRIP Commands**:
```cpp
// agioservice.h
signals:
    void requestStartNTRIP(const QString& url, const QString& user,
                          const QString& password, const QString& mount, int port);
    void requestStopNTRIP();

// agioservice.cpp
void AgIOService::configureNTRIP() {
    QString url = SettingsManager::instance()->setNTRIP_url();
    QString user = SettingsManager::instance()->setNTRIP_userName();
    // ... get configuration from SettingsManager

    emit requestStartNTRIP(url, user, password, mount, port);  // → NTRIPWorker thread
}
```

**Serial Commands**:
```cpp
signals:
    void requestSerialWorkerStart();
    void requestSerialWorkerStop();

// Async port opening (CDC-compliant non-blocking)
void AgIOService::openSerialPortAsync(const QString& portType, const QString& portName, int baudRate) {
    // Emit signal to SerialWorker thread
    emit requestPortOpen(portType, portName, baudRate);

    // Result returned via signal
    connect(m_serialWorker, &SerialWorker::portOpenResult,
            this, &AgIOService::onPortOpenResult, Qt::QueuedConnection);
}
```

### Worker Thread → Main Thread (Data/Status)

**Pattern**: Qt::DirectConnection for real-time updates (worker thread calls main thread method directly)

**NTRIP Data**:
```cpp
// Worker → Main (Qt::DirectConnection)
connect(m_ntripWorker, &NTRIPWorker::ntripDataReceived,
        this, &AgIOService::onNtripDataReceived, Qt::DirectConnection);

void AgIOService::onNtripDataReceived(const QByteArray& rtcmData) {
    // Process RTCM corrections
    // Forward to GPS modules via UDP or Serial
}
```

**Serial Data**:
```cpp
// Worker → Main (Qt::DirectConnection)
connect(m_serialWorker, &SerialWorker::serialIMUDataReceived,
        this, &AgIOService::onSerialIMUDataReceived, Qt::DirectConnection);

void AgIOService::onSerialIMUDataReceived(const QByteArray& imuData) {
    // Parse with PGNParser
    PGNParser::ParsedData data = m_pgnParser->parse(imuData, "Serial", portName);

    // Broadcast
    emit parsedDataReady(data);
    emit imuDataReady(data);  // Specific routing
}
```

## QML Integration

### Direct Hardware Monitoring

**Access Pattern**: `AgIOService.*` (singleton properties)

```qml
import QtQuick
import AOG 1.0

Item {
    // GPS status
    Text {
        text: "GPS: " + (AgIOService.gpsConnected ? "Connected" : "Disconnected")
        color: AgIOService.gpsQuality > 3 ? "green" : "orange"
    }

    Text {
        text: "Satellites: " + AgIOService.satellites
    }

    // NTRIP status
    Text {
        text: "NTRIP: " + AgIOService.ntripStatusText
        color: AgIOService.ntripConnected ? "green" : "red"
    }

    // Module status
    Text {
        text: "IMU: " + (AgIOService.imuConnected ? "Active" : "Inactive")
    }
    Text {
        text: "AutoSteer: " + (AgIOService.steerConnected ? "Active" : "Inactive")
    }

    // Protocol monitoring (Phase 6.0.22.12)
    ListView {
        model: AgIOService.activeProtocols
        delegate: Row {
            Text { text: modelData.id }
            Text { text: modelData.description }
            Text { text: modelData.source }
            Text { text: modelData.frequency.toFixed(1) + " Hz" }
        }
    }

    // Q_INVOKABLE methods
    Button {
        text: "Configure NTRIP"
        onClicked: AgIOService.configureNTRIP()
    }

    Button {
        text: "Scan Modules"
        onClicked: AgIOService.startModuleDiscovery()
    }
}
```

### Module Source and Frequency Tracking (Phase 6.0.22.3)

**Properties**:
```cpp
Q_PROPERTY(QString gpsSource ...)        // "UDP:192.168.1.126"
Q_PROPERTY(double gpsFrequency ...)      // 10.0 Hz
Q_PROPERTY(QString imuSource ...)        // "Serial:COM3"
Q_PROPERTY(double imuFrequency ...)      // 10.0 Hz
Q_PROPERTY(QString steerSource ...)      // "UDP:192.168.1.121"
Q_PROPERTY(double steerFrequency ...)    // 700.0 Hz
```

**QML Usage**:
```qml
Text {
    text: "GPS: " + AgIOService.gpsSource + " @ " + AgIOService.gpsFrequency.toFixed(1) + " Hz"
}
```

## Performance Characteristics

### Frequencies

| Data Stream | Frequency | Protocol | Purpose |
|-------------|-----------|----------|---------|
| **GPS Position** | ~10 Hz | NMEA ($GGA, $PANDA) | Navigation, UpdateFixPosition() |
| **External IMU** | ~10 Hz | PGN 211 | Heading, roll, gyro |
| **AutoSteer Feedback** | ~700 Hz | PGN 253, 250 | Real-time steering control |
| **Machine Status** | 4 Hz | PGN 239, 229 | Implement control |
| **NTRIP** | Keep-alive 10s | RTCM | RTK corrections |

### Thread Performance

**Main Thread** (zero-latency access):
- UDP event processing: Event-driven, non-blocking
- Protocol parsing: 5-20ms typical (PGNParser)
- Signal emission: <1ms (Qt internal queue)

**Worker Threads**:
- NTRIPWorker: Network I/O (blocking acceptable in separate thread)
- SerialWorker: Serial I/O (blocking acceptable in separate thread)

**Cross-Thread Communication**:
- Command signals: Qt::QueuedConnection (asynchronous, thread-safe)
- Data signals: Qt::DirectConnection (synchronous, direct call)

## References

### Code Files
- [classes/agioservice.h](../../../classes/agioservice.h), [classes/agioservice.cpp](../../../classes/agioservice.cpp) - Main coordinator
- [classes/pgnparser.h](../../../classes/pgnparser.h), [classes/pgnparser.cpp](../../../classes/pgnparser.cpp) - Protocol parser
- [classes/ntripworker.h](../../../classes/ntripworker.h), [classes/ntripworker.cpp](../../../classes/ntripworker.cpp) - NTRIP worker
- [classes/serialworker.h](../../../classes/serialworker.h), [classes/serialworker.cpp](../../../classes/serialworker.cpp) - Serial worker
- [classes/ctraffic.h](../../../classes/ctraffic.h), [classes/ctraffic.cpp](../../../classes/ctraffic.cpp) - UDP traffic monitoring

### Related Documentation
- [System Architecture](system-architecture.md) - Overall threading and component architecture
- [NMEA Protocol Reference](../../reference/nmea-sentences.md) - NMEA sentence specifications
- [PGN Protocol Reference](../../reference/pgn-sentences.md) - PGN packet specifications
- [Threading and Timers](threading-timers.md) - Timer frequencies and performance

### External Resources
- [NTRIP Protocol Documentation](https://www.use-snip.com/kb/knowledge-base/ntrip-rev-1-protocol/)
- [NMEA 0183 Standard](https://www.nmea.org/content/STANDARDS/NMEA_0183_Standard)
- [AgOpenGPS Forum - Communication Protocols](https://discourse.agopengps.com)
