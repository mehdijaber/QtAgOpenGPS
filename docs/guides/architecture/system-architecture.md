# QtAgOpenGPS System Architecture

**Status**: IMPLEMENTED ✅
**Phase**: 6.0.45+
**Last Validated**: 2025-09-15

Comprehensive guide to QtAgOpenGPS multi-thread architecture, component organization, and communication patterns.

## Overview

QtAgOpenGPS implements a multi-threaded architecture with clear separation between UI coordination (main thread) and I/O operations (worker threads). The system uses Qt 6.8's modern QProperty/BINDABLE system for automatic QML synchronization and type-safe property binding.

**Core Architectural Patterns**:
- **Thread Coordinator Pattern**: Main thread coordination with worker thread I/O
- **Singleton Services**: Global access to configuration and hardware services
- **BINDABLE Properties**: Automatic QML updates without manual synchronization
- **Event-Driven I/O**: Non-blocking communication with zero-latency main thread access

## Architecture Diagram

```
┌─────────────────── MAIN THREAD ─────────────────────┐
│                                                      │
│  ┌─ QML Interface ─┐  ┌─ FormGPS (App Engine) ─┐   │
│  │ - UI Controls   │←→│ - 67 Q_PROPERTY        │   │
│  │ - User Input    │  │ - Main Logic           │   │
│  └─────────────────┘  └────────────────────────┘   │
│                                                      │
│  ┌─ SettingsManager ─┐  ┌─ Singletons ─────┐       │
│  │ - 389 properties  │  │ - CTrack        │       │
│  │ - .ini persistence│  │ - CVehicle      │       │
│  │ - Qt 6.8 BINDABLE │  │ - BINDABLE      │       │
│  └───────────────────┘  └─────────────────┘       │
│                                                      │
│  ┌─ AgIOService ─────┐  ┌─ AOGRenderer ────┐       │
│  │ - Thread Coord    │  │ - OpenGL 30Hz   │       │
│  │ - Command Pattern │  │ - Scene Graph   │       │
│  │ - BINDABLE        │  │ - BINDABLE      │       │
│  └───────────────────┘  └─────────────────┘       │
└──────────────────────────────────────────────────────┘
                           │
                           ▼ Qt::QueuedConnection
┌─────────────────── WORKER THREADS ─────────────────┐
│                                                     │
│  ┌─ NTRIPWorker ──────┐  ┌─ SerialWorker ─────┐   │
│  │ - RTK corrections  │  │ - Arduino modules  │   │
│  │ - Network I/O      │  │ - Serial I/O       │   │
│  └────────────────────┘  └────────────────────┘   │
│                                                     │
│  ┌─ UDP (Main Thread) ─┐                           │
│  │ - Event-driven      │                           │
│  │ - QUdpSocket        │                           │
│  │ - Port 9999/8888    │                           │
│  └─────────────────────┘                           │
└─────────────────────────────────────────────────────┘
```

## Core Components

### FormGPS - Application Engine

**File**: [formgps.h](../../../formgps.h#L78), [formgps.cpp](../../../formgps.cpp)

**Purpose**: Main application engine inheriting from `QQmlApplicationEngine`. Coordinates all components and contains the primary business logic.

**Key Characteristics**:
- **Inheritance**: `QQmlApplicationEngine` ([formgps.h:78](../../../formgps.h#L78))
- **Properties**: 67 Q_PROPERTY using Qt 6.8 BINDABLE pattern ([formgps.h:86-150](../../../formgps.h#L86-L150))
- **Thread**: Main thread
- **QML Access**: `formgps.*` via rootContext
- **Pattern**: Qt 6.8 QProperty + BINDABLE for automatic change notification

**Property Examples**:
```cpp
// formgps.h:86-89
Q_PROPERTY(bool isJobStarted READ isJobStarted WRITE setIsJobStarted
           NOTIFY isJobStartedChanged BINDABLE bindableIsJobStarted)
Q_PROPERTY(bool isBtnAutoSteerOn READ isBtnAutoSteerOn WRITE setIsBtnAutoSteerOn
           NOTIFY isBtnAutoSteerOnChanged BINDABLE bindableIsBtnAutoSteerOn)

// formgps.h:96-107 - Position GPS (6 properties)
Q_PROPERTY(double latitude READ latitude WRITE setLatitude
           NOTIFY latitudeChanged BINDABLE bindableLatitude)
Q_PROPERTY(double longitude READ longitude WRITE setLongitude
           NOTIFY longitudeChanged BINDABLE bindableLongitude)
```

**QML Integration**:
```qml
// Direct binding to FormGPS properties
Button {
    text: formGPS.isJobStarted ? "Stop" : "Start"
    onClicked: formGPS.isJobStarted = !formGPS.isJobStarted
}

Text {
    text: "Speed: " + formGPS.speedKph + " km/h"  // Reactive binding
}
```

**C++ Internal Access**:
```cpp
void FormGPS::updatePosition() {
    m_latitude = gpsData.latitude;    // Direct member access
    m_speedKph = vehicle->avgSpeed;   // Qt 6.8 notifies QML automatically
}
```

**Property Organization**:
- Core Application State (2 properties): `isJobStarted`, `isBtnAutoSteerOn`
- Position GPS (6 properties): `latitude`, `longitude`, `altitude`, `easting`, `northing`, `heading`
- Vehicle State (7 properties): `speedKph`, `fusedHeading`, `toolEasting`, `toolNorthing`, `toolHeading`, `offlineDistance`, `avgPivDistance`
- Steering Control (6 properties): `steerAngleActual`, `steerAngleSet`, `lblPWMDisplay`, `calcSteerAngleInner`, `calcSteerAngleOuter`, `diameter`
- IMU Data (5 properties): `imuRoll`, `imuPitch`, `imuHeading`, `imuRollDegrees`, `imuPitchDegrees`

### SettingsManager - Configuration Singleton

**File**: [classes/settingsmanager.h](../../../classes/settingsmanager.h), [classes/settingsmanager.cpp](../../../classes/settingsmanager.cpp)

**Purpose**: Singleton managing all application configuration with automatic .ini file persistence.

**Key Characteristics**:
- **Architecture**: Qt6 Pure with auto-generated properties ([settingsmanager.h:23](../../../classes/settingsmanager.h#L23))
- **Properties**: 389 Q_OBJECT_BINDABLE_PROPERTY ([settingsmanager.h:26](../../../classes/settingsmanager.h#L26))
- **Pattern**: QML_SINGLETON ([settingsmanager.h:35](../../../classes/settingsmanager.h#L35))
- **Persistence**: QSettings with automatic sync on every property change
- **Thread**: Main thread
- **QML Access**: `SettingsManager.*` (global singleton)

**Code Generation System**:
Settings properties are auto-generated from [settings_config.txt](../../../settings_config.txt) using [generate_settings.py](../../../generate_settings.py).

**Format**: `name|iniKey|defaultValue|type`

**Generated Files**:
- [classes/settingsmanager_properties.h](../../../classes/settingsmanager_properties.h) - Q_PROPERTY declarations
- [classes/settingsmanager_members.h](../../../classes/settingsmanager_members.h) - QProperty member variables
- [classes/settingsmanager_implementations.cpp](../../../classes/settingsmanager_implementations.cpp) - Getter/setter implementations

**Example Property**:
```cpp
// settingsmanager_properties.h (auto-generated)
Q_OBJECT_BINDABLE_PROPERTY(SettingsManager, double, vehicle_toolWidth,
                           &SettingsManager::vehicle_toolWidthChanged)
```

**QML Integration**:
```qml
SpinBox {
    value: SettingsManager.vehicle_numSections  // BINDABLE auto-sync
    onValueChanged: SettingsManager.vehicle_numSections = value
}
```

**C++ Access**:
```cpp
int sections = SettingsManager::instance()->vehicle_numSections();
int udpPort = SettingsManager::instance()->setUDP_listenPort();
```

**Property Categories**:
- Vehicle Configuration: `vehicle_*` (width, wheelbase, antennaOffset, etc.)
- Steering Settings: `as_*` (autoSteer parameters)
- UDP/Network: `setUDP_*` (ports, IP addresses)
- Display: `display_*` (colors, grid size)
- Field: `field_*` (boundaries, headlands)

**Persistence Location**:
- Windows: `C:\Users\<username>\Documents\QtAgOpenGPS\settings.ini`
- Linux: `~/.config/QtAgOpenGPS/settings.ini`
- Android: Application-specific storage

### AgIOService - Hardware Communication Coordinator

**File**: [classes/agioservice.h](../../../classes/agioservice.h), [classes/agioservice.cpp](../../../classes/agioservice.cpp)

**Purpose**: Main thread coordinator managing all hardware communication via specialized worker threads. Provides zero-latency access to position data for OpenGL rendering and AutoSteer.

**Key Characteristics**:
- **Pattern**: Thread Coordinator ([agioservice.h:30-33](../../../classes/agioservice.h#L30-L33))
- **Architecture**: Main thread coordination + worker thread I/O
- **Properties**: 54 Q_OBJECT_BINDABLE_PROPERTY for real-time status
- **QML Access**: `AgIOService.*` (singleton)
- **QML Pattern**: QML_SINGLETON ([agioservice.h:43-44](../../../classes/agioservice.h#L43-L44))

**Worker Thread Management**:
```cpp
// agioservice.h:1018-1028
QThread* m_ntripThread;
QThread* m_serialThread;
NTRIPWorker* m_ntripWorker;
SerialWorker* m_serialWorker;
QUdpSocket* m_udpSocket;  // Phase 6.0.24: Event-driven in main thread
```

**Phase 6.0.24 Architecture Changes**:
- **GPSWorker removed**: No longer needed (Phase 6.0.21, no I/O operations)
- **UDPWorker removed**: Replaced with event-driven `QUdpSocket` in main thread
- **UDP Event-Driven**: Zero-latency access for OpenGL rendering (30Hz)

**Communication Patterns**:

**Configuration (Main → Workers)**:
```cpp
// Command Pattern: AgIOService → Workers via signals
connect(this, &AgIOService::requestStartGPS,
        m_gpsWorker, &GPSWorker::startGPS, Qt::QueuedConnection);

connect(this, &AgIOService::requestStartNTRIP,
        m_ntripWorker, &NTRIPWorker::startNTRIP, Qt::QueuedConnection);
```

**Real-time Data (Workers → AgIOService → FormGPS)**:
```cpp
// Workers → AgIOService (Qt::DirectConnection - same thread context)
connect(m_gpsWorker, &GPSWorker::positionReceived,
        this, &AgIOService::updatePosition, Qt::DirectConnection);

// AgIOService → FormGPS (Qt::QueuedConnection - anti-reentrancy)
connect(m_agioService, &AgIOService::gpsDataChanged, this, [this]() {
    double lat = m_agioService->latitude();
    // Process and update formGPS properties
}, Qt::QueuedConnection);
```

**QML Integration**:
```qml
// Direct hardware monitoring
Text {
    text: "GPS Raw: " + AgIOService.latitude.toFixed(6)
    color: AgIOService.ntripConnected ? "green" : "red"
}

Button {
    text: "Configure NTRIP"
    onClicked: AgIOService.configureNTRIP()
}
```

**Property Categories**:
- GPS Status: `gpsConnected`, `gpsQuality`, `satellites`
- Connection Status: `bluetoothConnected`, `ethernetConnected`, `ntripConnected`
- NTRIP Status: `ntripStatus`, `ntripStatusText`, `rawTripCount`
- Module Status: `imuConnected`, `steerConnected`, `machineConnected`, `blockageConnected`
- Module Sources (Phase 6.0.22.3): `gpsSource`, `imuSource`, `steerSource`, `machineSource`
- Frequency Monitoring: `gpsFrequency`, `imuFrequency`, `steerFrequency`, `machineFrequency`

**Performance Characteristics**:
- GPS: 15Hz (67ms) - F9P chipset frequency
- WAS: 40Hz (25ms) - AutoSteer ultra-reactive
- NTRIP: 10s keep-alive, real-time RTK corrections
- UDP: Ports 9999 (listen) / 8888 (send)

### CTrack and CVehicle - Business Logic Singletons

**Files**:
- [classes/ctrack.h](../../../classes/ctrack.h), [classes/ctrack.cpp](../../../classes/ctrack.cpp)
- [classes/cvehicle.h](../../../classes/cvehicle.h), [classes/cvehicle.cpp](../../../classes/cvehicle.cpp)

**Purpose**: Singleton services managing vehicle and tracking business logic.

**QML Registration**:
```cpp
// main.cpp
qmlRegisterSingletonType<CTrack>("AOG", 1, 0, "TracksInterface", ...);
qmlRegisterSingletonType<CVehicle>("AOG", 1, 0, "VehicleInterface", ...);
```

**Pattern**: Qt 6.8 BINDABLE properties for automatic QML updates.

**Example (CVehicle)**:
```cpp
class CVehicle : public QObject {
    Q_OBJECT

    static CVehicle* instance() {
        static CVehicle* s_instance = new CVehicle(nullptr);
        return s_instance;
    }

    Q_PROPERTY(bool isHydLiftOn READ isHydLiftOn WRITE setIsHydLiftOn
               BINDABLE bindableIsHydLiftOn)

private:
    QProperty<bool> m_isHydLiftOn{false};
};
```

**QML Access**:
```qml
Button {
    enabled: TracksInterface.isAutoTrack  // CTrack
    text: VehicleInterface.isHydLiftOn ? "UP" : "DOWN"  // CVehicle
}
```

**C++ Access**:
```cpp
bool autoTrack = CTrack::instance()->isAutoTrack();
CVehicle::instance()->setIsHydLiftOn(true);
```

### AOGRenderer - OpenGL Component

**File**: [aogrenderer.h](../../../aogrenderer.h), [aogrenderer.cpp](../../../aogrenderer.cpp)

**Purpose**: QML component (not singleton) for OpenGL field rendering.

**Key Characteristics**:
- **Type**: QQuickFramebufferObject
- **Thread**: Dedicated render thread (Qt Scene Graph)
- **Frequency**: 30Hz (independent of GPS updates)
- **QML Pattern**: Component (instantiated in QML)
- **QML Registration**: `qmlRegisterType<AOGRendererInSG>("AOG", 1, 0, "AOGRenderer");`

**Properties**:
```cpp
Q_PROPERTY(double shiftX READ shiftX WRITE setShiftX BINDABLE bindableShiftX)
Q_PROPERTY(double shiftY READ shiftY WRITE setShiftY BINDABLE bindableShiftY)

private:
    QProperty<double> m_shiftX{0.0};
    QProperty<double> m_shiftY{0.0};
```

**QML Instance**:
```qml
AOGRenderer {
    id: renderer
    width: 800
    height: 600

    shiftX: mouseArea.dragX  // BINDABLE auto-sync
    shiftY: settingsManager.display_offsetY
}
```

**OpenGL Access to Singletons**:
```cpp
void AOGRenderer::render() {
    // Direct access to singletons (main thread safe)
    double sections = SettingsManager::instance()->vehicle_numSections();
    double lat = AgIOService::instance()->latitude();
    bool autoTrack = CTrack::instance()->isAutoTrack();

    // Render OpenGL 30Hz
    drawVehicle(lat, sections);
}
```

## Communication Patterns

### Thread Safety Architecture

```
Main Thread (Coordination)           Worker Threads (I/O)
    │                                    │
    ├─ FormGPS                           ├─ NTRIPWorker
    ├─ SettingsManager                   ├─ SerialWorker
    ├─ AgIOService (Coordinator) ────────┤
    ├─ CTrack/CVehicle                   │
    ├─ AOGRenderer                       │
    └─ QML Interface                     │
         │                                    │
         └──── Qt::QueuedConnection commands ──┘
               Qt::DirectConnection data ←────┘
```

### Connection Types Matrix

| Source | Destination | Connection Type | Purpose |
|--------|-------------|-----------------|---------|
| Workers | AgIOService | `Qt::DirectConnection` | Real-time data updates |
| AgIOService | Workers | `Qt::QueuedConnection` | Cross-thread commands |
| AgIOService | FormGPS | `Qt::QueuedConnection` | Anti-reentrancy protection |
| FormGPS | FormGPS | `Qt::QueuedConnection` | Anti-reentrancy protection |

### QML Synchronization

**Qt 6.8 QProperty + BINDABLE Benefits**:
1. **Automatic Binding**: No manual `setProperty()` calls needed
2. **Bidirectional Updates**: QML ↔ C++ automatic synchronization
3. **Type Safety**: Compile-time type checking
4. **Performance**: Direct member access, no QMetaObject overhead
5. **Thread Safety**: BINDABLE properties inherently thread-safe

### QML Access Patterns

| Interface | Usage | Pattern | Data Type |
|-----------|-------|---------|-----------|
| `formGPS.*` | Business UI | rootContext | Processed business data |
| `AgIOService.*` | Hardware monitoring | Singleton | Raw hardware data |
| `SettingsManager.*` | Configuration | Singleton | Application settings |
| `TracksInterface.*` | Tracking logic | Singleton | AB line/tracking state |
| `VehicleInterface.*` | Vehicle state | Singleton | Vehicle configuration |
| `rendererId.*` | OpenGL rendering | Instance | Rendering parameters |

**Future Migration**: `aog.*` → FormGPS alias (preserves syntax, improves performance)

## Component Summary Table

| Component | Type | Thread | QML Access | Properties Pattern |
|-----------|------|--------|------------|-------------------|
| FormGPS | App Engine | Main | `formGPS.*` | Qt 6.8 BINDABLE (67) |
| SettingsManager | Singleton | Main | `SettingsManager.*` | Qt 6.8 BINDABLE (389) |
| AgIOService | Singleton | Main | `AgIOService.*` | Qt 6.8 BINDABLE (54) |
| CTrack | Singleton | Main | `TracksInterface.*` | Qt 6.8 BINDABLE |
| CVehicle | Singleton | Main | `VehicleInterface.*` | Qt 6.8 BINDABLE |
| AOGRenderer | Component | Render | `id.*` | Qt 6.8 BINDABLE |

## Key Architectural Decisions

### 1. Thread Coordinator Pattern
**Decision**: Main thread coordination with worker thread I/O
**Rationale**: Zero-latency access for OpenGL rendering (30Hz) while non-blocking I/O operations
**Implementation**: AgIOService coordinates workers via Qt::QueuedConnection signals

### 2. Qt 6.8 BINDABLE Properties
**Decision**: Use QProperty + BINDABLE everywhere for automatic QML synchronization
**Rationale**: Eliminates manual synchronization code, type-safe, better performance
**Impact**: 389 SettingsManager properties, 67 FormGPS properties, 54 AgIOService properties

### 3. Event-Driven UDP (Phase 6.0.24)
**Decision**: Replace UDPWorker thread with event-driven QUdpSocket in main thread
**Rationale**: UDP is already non-blocking, dedicated thread adds latency
**Impact**: Zero-latency UDP access for AutoSteer (40Hz WAS), simplified architecture

### 4. GPSWorker Removal (Phase 6.0.21)
**Decision**: Eliminate GPSWorker thread (no I/O operations)
**Rationale**: GPS data processed by SerialWorker, dedicated thread was unnecessary overhead
**Impact**: Simpler architecture, reduced thread count

### 5. Singleton Services
**Decision**: Use strict singleton pattern for global services
**Rationale**: Global access for configuration and hardware state, thread-safe with Qt 6.8 BINDABLE
**Implementation**: SettingsManager, AgIOService, CTrack, CVehicle

### 6. Code Generation for Settings
**Decision**: Auto-generate 389 SettingsManager properties from configuration file
**Rationale**: Eliminate manual synchronization code, ensure consistency, reduce errors
**Tool**: [generate_settings.py](../../../generate_settings.py) processes [settings_config.txt](../../../settings_config.txt)

### 7. Automatic Persistence
**Decision**: Sync SettingsManager to .ini file on every property change
**Rationale**: Never lose configuration, no manual save required
**Implementation**: QSettings with automatic sync in every setter

### 8. Component vs Singleton for Rendering
**Decision**: AOGRenderer as QML component (not singleton)
**Rationale**: Allow multiple render instances, QML controls lifecycle
**Pattern**: QQuickFramebufferObject with BINDABLE properties

## Performance Characteristics

**Rendering**:
- OpenGL: 30Hz (dedicated render thread via Qt Scene Graph)
- GPS updates: 10Hz (position data)
- QML updates: Variable (UI interactions)

**I/O Frequencies**:
- GPS: 15Hz (F9P chipset native frequency)
- WAS (Wheel Angle Sensor): 40Hz (ultra-reactive AutoSteer)
- NTRIP: Keep-alive every 10 seconds, RTK corrections real-time
- UDP: Event-driven (ports 9999 listen / 8888 send)

**Thread Performance**:
- Main Thread: Zero-latency access to all singletons and properties
- Worker Threads: Non-blocking I/O operations
- Render Thread: Independent 30Hz rendering (no GPS data dependency)

## References

### Code Files
- [formgps.h](../../../formgps.h), [formgps.cpp](../../../formgps.cpp) - Application engine
- [classes/settingsmanager.h](../../../classes/settingsmanager.h), [classes/settingsmanager.cpp](../../../classes/settingsmanager.cpp) - Configuration singleton
- [classes/agioservice.h](../../../classes/agioservice.h), [classes/agioservice.cpp](../../../classes/agioservice.cpp) - Hardware coordinator
- [classes/ctrack.h](../../../classes/ctrack.h), [classes/cvehicle.h](../../../classes/cvehicle.h) - Business singletons
- [aogrenderer.h](../../../aogrenderer.h), [aogrenderer.cpp](../../../aogrenderer.cpp) - OpenGL component

### Configuration Files
- [settings_config.txt](../../../settings_config.txt) - SettingsManager property definitions
- [generate_settings.py](../../../generate_settings.py) - Code generation script

### Related Documentation
- [Threading and Timers](threading-timers.md) - Timer frequencies and threading details
- [AgIOService Architecture](agioservice-architecture.md) - Hardware communication details
- [QML Integration](qml-integration.md) - QML↔C++ binding patterns
- [Settings Properties](../../development/settings-properties.md) - SettingsManager code generation
- [Migration to Qt 6.8 Properties](migration-qt68-properties.md) - BINDABLE migration guide
