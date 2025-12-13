# Proposal: Backend Singleton with Q_GADGET Data Containers

**Status**: PENDING
**Author**: Michael Torrie, Mehdi Jaber
**Date**: 2025-12-13
**Related Issue**: TBD

---

## Problem Statement

### Current Architecture Issues

QtAgOpenGPS currently exposes application state through multiple scattered singletons with flat property structures:

1. **FormGPS** (QQmlApplicationEngine): 67 flat Q_PROPERTY declarations
2. **CVehicle**: 7 vehicle-related properties
3. **SettingsManager**: 389 configuration properties (well-organized, auto-generated)
4. **AgIOService**: 54 hardware status properties

**Problems**:
- **Poor Organization**: Properties scattered across multiple classes without logical grouping
- **No Namespacing**: QML accesses flat properties like `aog.latitude`, `VehicleInterface.speed` without context
- **High Coupling**: Business logic classes tightly coupled to QML exposure layer
- **Qt Design Studio Incompatibility**: Current architecture prevents integration with Qt Design Studio workflow
- **Maintenance Burden**: Adding/removing properties requires changes across header, implementation, and QML files

**Example of Current Flat Structure**:
```qml
// Scattered, unorganized access
Text { text: aog.latitude }              // GPS data
Text { text: VehicleInterface.speed }    // Vehicle data
Text { text: aog.workedArea }            // Field data
Text { text: AgIOService.gpsFixQuality } // Hardware status
```

---

## Proposed Solution

### Architecture Overview

Introduce a **Backend** singleton as the primary QML interface, containing structured data via **Q_GADGET** containers with **Qt 6.8 QProperty + BINDABLE** support.

```
Backend (QML_SINGLETON, QObject)
├─ rawGPS (Q_GADGET)         → QProperty<RawGPSData>
├─ blockage (Q_GADGET)       → QProperty<BlockageData>
├─ vehicle (Q_GADGET)        → QProperty<VehicleState>
├─ field (Q_GADGET)          → QProperty<FieldData>
├─ guidance (Q_GADGET)       → QProperty<GuidanceData>
└─ hardware (Q_GADGET)       → QProperty<HardwareStatus>
```

**QML Access Pattern**:
```qml
Backend.rawGPS.latitude       // Structured, namespaced
Backend.vehicle.speed         // Clear context
Backend.field.workedArea      // Logical grouping
Backend.hardware.fixQuality   // Semantic organization
```

### Core Components

#### 1. Q_GADGET Data Containers

**Q_GADGET** = Lightweight data structure with Qt meta-object support but without QObject overhead.

**Example: RawGPSData**
```cpp
// rawgpsdata.h
struct RawGPSData
{
    Q_GADGET

    Q_PROPERTY(double latitude MEMBER latitude)
    Q_PROPERTY(double longitude MEMBER longitude)
    Q_PROPERTY(double altitude MEMBER altitude)
    Q_PROPERTY(double speed MEMBER speed)
    Q_PROPERTY(double heading MEMBER heading)
    Q_PROPERTY(int fixQuality MEMBER fixQuality)
    Q_PROPERTY(int satelliteCount MEMBER satelliteCount)
    Q_PROPERTY(double hdop MEMBER hdop)

public:
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double speed = 0.0;
    double heading = 0.0;
    int fixQuality = 0;
    int satelliteCount = 0;
    double hdop = 99.9;

    RawGPSData() = default;
};

Q_DECLARE_METATYPE(RawGPSData)
```

**Key Characteristics**:
- Value type (stack allocation, copy semantics)
- Zero QObject overhead (~40 bytes saved per instance)
- Direct member access from C++
- QML-accessible via Q_PROPERTY
- Read-only from QML (write requires Q_INVOKABLE methods)

#### 2. Backend Singleton Container

**Backend** = QObject singleton hosting Q_GADGET containers with Qt 6.8 BINDABLE support.

```cpp
// backend.h
class Backend : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Qt 6.8 BINDABLE properties for Q_GADGET containers
    Q_PROPERTY(RawGPSData rawGPS READ rawGPS WRITE setRawGPS
               NOTIFY rawGPSChanged BINDABLE bindableRawGPS)

    Q_PROPERTY(BlockageData blockage READ blockage WRITE setBlockage
               NOTIFY blockageChanged BINDABLE bindableBlockage)

    Q_PROPERTY(VehicleState vehicle READ vehicle WRITE setVehicle
               NOTIFY vehicleChanged BINDABLE bindableVehicle)

    Q_PROPERTY(FieldData field READ field WRITE setField
               NOTIFY fieldChanged BINDABLE bindableField)

public:
    static Backend* create(QQmlEngine*, QJSEngine*);
    static Backend* instance();

    // Getters - Return by value from QProperty
    RawGPSData rawGPS() const { return m_rawGPS.value(); }
    BlockageData blockage() const { return m_blockage.value(); }
    VehicleState vehicle() const { return m_vehicle.value(); }
    FieldData field() const { return m_field.value(); }

    // Setters - QProperty auto-emits changed signal
    void setRawGPS(const RawGPSData& data) { m_rawGPS = data; }
    void setBlockage(const BlockageData& data) { m_blockage = data; }
    void setVehicle(const VehicleState& data) { m_vehicle = data; }
    void setField(const FieldData& data) { m_field = data; }

    // Bindables - Qt 6.8 binding support
    QBindable<RawGPSData> bindableRawGPS() { return &m_rawGPS; }
    QBindable<BlockageData> bindableBlockage() { return &m_blockage; }
    QBindable<VehicleState> bindableVehicle() { return &m_vehicle; }
    QBindable<FieldData> bindableField() { return &m_field; }

    // Q_INVOKABLE methods for QML write access
    Q_INVOKABLE void updateGPSLatitude(double lat);
    Q_INVOKABLE void updateVehicleSpeed(double speed);

signals:
    void rawGPSChanged();
    void blockageChanged();
    void vehicleChanged();
    void fieldChanged();

private:
    explicit Backend(QObject* parent = nullptr);
    static Backend* m_instance;

    // QProperty containers - Qt 6.8 binding magic
    QProperty<RawGPSData> m_rawGPS;
    QProperty<BlockageData> m_blockage;
    QProperty<VehicleState> m_vehicle;
    QProperty<FieldData> m_field;
};
```

### Proposed Q_GADGET Containers

#### 1. RawGPSData
**Purpose**: Raw GPS sensor data (NMEA/PGN parsed values)

**Members**: latitude, longitude, altitude, speed, heading, fixQuality, satelliteCount, hdop, ageOfDifferential

**Update Frequency**: 10 Hz (GPS data rate)

#### 2. BlockageData
**Purpose**: Seed planter blockage monitoring data

**Members**: sectionCounts[16], averageCount, maxCount, minCount, isBlocked[16]

**Update Frequency**: Variable (sensor-driven, 1-10 Hz)

**Helper Methods**: `updateFromSensorData(section, count)`, `calculateStats()`

#### 3. VehicleState
**Purpose**: Current vehicle physical state

**Members**: wheelbase, antennaOffset, trackWidth, speed, isReverse, isHydLiftOn, hydLiftDown, leftTramState, rightTramState

**Update Frequency**: 10-30 Hz (position calculation loop)

#### 4. FieldData
**Purpose**: Current field operation metrics

**Members**: workedArea, totalArea, remainingArea, isJobStarted, fieldName, currentPassNumber, estimatedTimeRemaining

**Update Frequency**: 1 Hz (field statistics calculation)

#### 5. GuidanceData
**Purpose**: AB line and guidance state

**Members**: isABLineSet, abHeading, currentXTE, targetXTE, distanceToABLine, guidanceMode, isAutoSteerEngaged

**Update Frequency**: 10 Hz (guidance calculation loop)

#### 6. HardwareStatus
**Purpose**: Hardware module status and diagnostics

**Members**: moduleConnected, lastPGNTimestamp, serialPortStatus, ntripConnected, ntripAge, autosteerModuleVersion

**Update Frequency**: 1 Hz (status polling)

---

## Alternatives Considered

### Alternative 1: Keep Current Flat Structure
**Pros**: No migration required, stable existing code
**Cons**: Poor organization, Qt Design Studio incompatibility, high maintenance burden

**Verdict**: **Rejected** - Does not solve core organizational problems

### Alternative 2: Nested Q_OBJECT Classes
**Pros**: Full Q_OBJECT features (signals/slots per member)
**Cons**: High memory overhead (40+ bytes per instance), complex lifecycle management

**Example**:
```cpp
class RawGPSData : public QObject {  // 40+ bytes overhead
    Q_OBJECT
    Q_PROPERTY(double latitude READ latitude WRITE setLatitude NOTIFY latitudeChanged)
    // ... individual signals for each property
};
```

**Verdict**: **Rejected** - Unnecessary overhead for simple data containers

### Alternative 3: Q_GADGET without BINDABLE (Michael's Initial Approach)
**Pros**: Simple, lightweight
**Cons**: No Qt 6.8 automatic binding, manual signal emissions required

```cpp
class Backend : public QObject {
    Q_PROPERTY(RawGPSData rawGPS READ rawGPS NOTIFY rawGPSChanged)  // No BINDABLE
private:
    RawGPSData m_rawGPS;  // Simple member, not QProperty
};
```

**Verdict**: **Partially Accepted** - Good starting point, but missing Qt 6.8 benefits

### Alternative 4: Q_GADGET + BINDABLE (Proposed)
**Pros**: Lightweight Q_GADGET + Qt 6.8 automatic binding
**Cons**: Slightly more verbose than Alternative 3

```cpp
class Backend : public QObject {
    Q_PROPERTY(RawGPSData rawGPS READ rawGPS WRITE setRawGPS
               NOTIFY rawGPSChanged BINDABLE bindableRawGPS)  // BINDABLE!
private:
    QProperty<RawGPSData> m_rawGPS;  // Qt 6.8 binding support
};
```

**Verdict**: **ACCEPTED** - Best balance of organization, performance, and Qt 6.8 features

---

## Implementation Plan

### Phase 1: Backend Infrastructure (Week 1)

**Tasks**:
1. Create `backend/` subdirectory in project root
2. Implement Backend singleton with QML_ELEMENT + QML_SINGLETON
3. Configure qt_add_qml_module() in CMakeLists.txt
4. Test Backend singleton accessible in QML

**Files**:
- `backend/backend.h`
- `backend/backend.cpp`
- `CMakeLists.txt` (modify)

**Testing**: Verify `Backend` object accessible in QML with `import AOG 1.0`

### Phase 2: Core Q_GADGET Containers (Week 1-2)

**Tasks**:
1. Create Q_GADGET structs for each data domain
2. Add Q_PROPERTY(MEMBER) declarations
3. Register metatypes with Q_DECLARE_METATYPE
4. Add to Backend as QProperty members

**Files**:
- `backend/rawgpsdata.h`
- `backend/blockagedata.h`
- `backend/vehiclestate.h`
- `backend/fielddata.h`
- `backend/guidancedata.h`
- `backend/hardwarestatus.h`

**Testing**: Verify Q_GADGET properties accessible via `Backend.rawGPS.latitude` in QML

### Phase 3: FormGPS Integration (Week 2-3)

**Tasks**:
1. Update FormGPS position calculation to populate Backend
2. Migrate GPS data flow: PGNParser → FormGPS → Backend
3. Update blockage data handling to use Backend.blockage
4. Test data flow with real GPS module

**Migration Pattern**:
```cpp
// BEFORE - Direct QML property
void FormGPS::updatePosition() {
    setLatitude(newLat);  // Updates FormGPS property
}

// AFTER - Populate Backend Q_GADGET
void FormGPS::updatePosition() {
    RawGPSData gps = Backend::instance()->rawGPS();
    gps.latitude = newLat;
    gps.longitude = newLon;
    Backend::instance()->setRawGPS(gps);  // One update, one signal
}
```

### Phase 4: QML Migration (Week 3-4)

**Tasks**:
1. Update MainWindow.qml to use Backend.* properties
2. Update all QML components to new property paths
3. Remove old FormGPS property bindings
4. Test UI updates with Backend data flow

**Migration Pattern**:
```qml
// BEFORE
Text { text: aog.latitude.toFixed(6) }

// AFTER
Text { text: Backend.rawGPS.latitude.toFixed(6) }
```

### Phase 5: FormGPS Refactoring (Week 4-6)

**Tasks**:
1. Create FormGPS2 as QObject (not QQmlApplicationEngine)
2. Move business logic to FormGPS2
3. Migrate remaining properties to Backend containers
4. Update main.cpp to use generic QQmlApplicationEngine

**Goal**: FormGPS becomes pure business logic, Backend becomes QML interface

### Phase 6: Qt Design Studio Integration (Week 6+)

**Tasks**:
1. Restructure QML to be Design Studio compatible
2. Create Main.qml as QML entry point
3. Test live preview in Qt Design Studio
4. Document Design Studio workflow

---

## Impact Assessment

### Performance

**Memory Impact**:
- Q_GADGET: 0 bytes overhead (vs ~40 bytes for Q_OBJECT)
- Backend: ~6 QProperty containers = ~480 bytes total
- Net savings: ~240 bytes vs nested Q_OBJECT approach

**CPU Impact**:
- **Atomic Updates**: Update entire Q_GADGET in one operation → One signal emission
- **Before**: 8 individual property updates = 8 signals = 8 QML evaluations
- **After**: 1 Q_GADGET update = 1 signal = 1 QML evaluation
- **Improvement**: ~8x reduction in QML binding re-evaluations for grouped data

**Example**:
```cpp
// BEFORE - 8 signal emissions
setLatitude(45.0);   // emit latitudeChanged()
setLongitude(-93.0); // emit longitudeChanged()
setAltitude(250.0);  // emit altitudeChanged()
setSpeed(15.0);      // emit speedChanged()
setHeading(90.0);    // emit headingChanged()
setFixQuality(4);    // emit fixQualityChanged()
setSatCount(12);     // emit satCountChanged()
setHDOP(1.2);        // emit hdopChanged()

// AFTER - 1 signal emission
RawGPSData gps;
gps.latitude = 45.0; gps.longitude = -93.0; gps.altitude = 250.0;
gps.speed = 15.0; gps.heading = 90.0; gps.fixQuality = 4;
gps.satelliteCount = 12; gps.hdop = 1.2;
Backend::instance()->setRawGPS(gps);  // emit rawGPSChanged() ONCE
```

### Maintainability

**Improved Organization**:
- Clear data domain separation (GPS, vehicle, field, guidance, hardware)
- Natural namespacing in QML
- Single source of truth for each data domain

**Reduced Coupling**:
- FormGPS focuses on business logic, not QML exposure
- Backend provides clean interface layer
- Q_GADGET containers are reusable across C++ classes

**Code Reduction**:
- FormGPS: 67 flat properties → ~10 business logic methods
- Backend: 6 structured containers (one property each)
- Net reduction: ~50 property declarations eliminated

### Compatibility

**Breaking Changes**:
- All QML files must update property paths
- FormGPS Q_PROPERTY access from C++ must migrate

**Migration Path**:
1. Backend runs in parallel with FormGPS properties (Phase 2-4)
2. Gradual QML migration with find/replace
3. FormGPS properties deprecated after migration complete
4. No changes to external Arduino modules or GPS protocols

**Backward Compatibility**:
- Field file format unchanged
- Settings .ini format unchanged
- PGN/NMEA protocols unchanged

### Testing

**Unit Testing**:
```cpp
// Q_GADGET value semantics enable easy testing
TEST(RawGPSData, DefaultConstruction) {
    RawGPSData gps;
    EXPECT_EQ(gps.latitude, 0.0);
    EXPECT_EQ(gps.fixQuality, 0);
}

TEST(Backend, GPSDataUpdate) {
    RawGPSData gps;
    gps.latitude = 45.0;
    Backend::instance()->setRawGPS(gps);
    EXPECT_EQ(Backend::instance()->rawGPS().latitude, 45.0);
}
```

**Integration Testing**:
- Verify QML bindings update correctly
- Test GPS data flow: PGNParser → FormGPS → Backend → QML
- Validate blockage data accumulation across UDP packets
- Confirm Qt Design Studio live preview works

---

## Open Questions

### 1. FormGPS Lifecycle
**Question**: Should FormGPS remain QQmlApplicationEngine or become QObject immediately?

**Options**:
- **A**: Keep FormGPS as QQmlApplicationEngine, migrate properties progressively
- **B**: Create FormGPS2 as QObject, run in parallel during transition
- **C**: Full refactor to QObject in Phase 5

**Recommendation**: **Option A** for Phase 1-4, then **Option B** for Phase 5 to minimize risk

### 2. Q_INVOKABLE Write Methods
**Question**: How should QML write to Q_GADGET members (read-only from QML)?

**Options**:
- **A**: Q_INVOKABLE methods on Backend (`Backend.updateGPSLatitude(45.0)`)
- **B**: Expose individual writable properties alongside Q_GADGET
- **C**: QML read-only, all writes via C++ business logic

**Recommendation**: **Option C** - QML should be read-only display, C++ handles business logic

### 3. CMake Plugin Structure
**Question**: Should Backend be a separate CMake plugin or integrated into main executable?

**Options**:
- **A**: Integrated into main executable (simpler)
- **B**: Separate plugin library (better Design Studio compatibility)

**Recommendation**: **Option A** for Phase 1-4, consider **Option B** for Phase 6 (Design Studio)

### 4. Property Granularity
**Question**: Are the proposed 6 Q_GADGET containers the right granularity, or should we split further?

**Examples**:
- Split `VehicleState` into `VehicleGeometry` + `VehicleStatus`?
- Merge `GuidanceData` + `FieldData` into `OperationState`?

**Recommendation**: Start with 6 containers, refine based on usage patterns in Phase 2-3

---

## Decision Log

### 2025-12-13: Proposal Created
- **Author**: Michael Torrie, Mehdi Jaber
- **Status**: PENDING team review
- **Next Steps**:
  - Present to team for discussion
  - Address open questions
  - Create GitHub issue for tracking
  - Schedule implementation timeline

---

**Related Documentation**:
- [Qt 6.8 Property Migration Guide](../architecture/migration-qt68-properties.md)
- [QML Integration Architecture](../architecture/qml-integration.md)
- [System Architecture Overview](../architecture/system-architecture.md)
- [Q_GADGET Qt Documentation](https://doc.qt.io/qt-6/qobject.html#Q_GADGET)
- [QProperty Qt Documentation](https://doc.qt.io/qt-6/qproperty.html)

---

**Implementation Tracking**: TBD (create GitHub issue)
