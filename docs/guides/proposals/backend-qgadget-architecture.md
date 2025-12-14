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

#### 3. Unified C++/QML Singleton Pattern

**Critical Pattern**: Backend (and future CoreGPS) must be accessible from both C++ and QML using a **single shared instance**.

### Problem: Two-Instance Singleton Anti-Pattern

**Without this pattern**, you can accidentally create **two separate instances**:

```cpp
// ❌ WRONG - Two different instances!
class Backend : public QObject {
    static Backend* instance();  // C++ creates one instance
};

// main.cpp
qmlRegisterSingletonType<Backend>(..., [](...) {
    return new Backend();  // QML creates ANOTHER instance!
});

// Result: C++ sees one Backend, QML sees a different Backend
// GPS data updated in C++ won't appear in QML!
```

### Solution: instance() + create() Pattern (Michael)

**Pattern Requirements** (per Michael):
> "if a Q_OBJECT class is intended to be a QML_SINGLETON, and if it needs to be accessible to c++ code, it should follow the pattern in backend.h. Needs an instance() method for C++, and a create() method for the QML engine. Makes a singleton that is shared between C++ and QML."

**Implementation**:

```cpp
// backend.h
class Backend : public QObject
{
    Q_OBJECT
    QML_ELEMENT       // Qt 6 auto-registration
    QML_SINGLETON     // Tells QML this is a singleton

public:
    // For C++ - Standard singleton pattern
    static Backend* instance() {
        if (!m_instance) {
            m_instance = new Backend();
        }
        return m_instance;
    }

    // For QML - Called by QML engine during initialization
    static Backend* create(QQmlEngine* engine, QJSEngine* scriptEngine) {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)

        // KEY: Return the SAME instance as C++
        return instance();
    }

private:
    explicit Backend(QObject* parent = nullptr);
    static Backend* m_instance;
};

// backend.cpp
Backend* Backend::m_instance = nullptr;

Backend::Backend(QObject* parent)
    : QObject(parent)
{
    // Singleton initialization
}
```

### Benefits

1. **Single Shared Instance**: C++ and QML see exactly the same object
2. **No Manual Registration**: QML_ELEMENT + QML_SINGLETON = auto-registration (no qmlRegisterSingletonType needed)
3. **Zero main.cpp Boilerplate**: Qt 6 handles registration automatically
4. **Type Safety**: QML engine validates singleton at load time
5. **Guaranteed Initialization**: QML engine calls `create()` once during startup

### Comparison: Old vs New Pattern

#### ❌ Old Pattern (QtAgOpenGPS current state for some classes)

```cpp
// Singleton without unified pattern
class FormGPS : public QQmlApplicationEngine {
    static FormGPS* instance();
};

// main.cpp - Manual registration required
qmlRegisterSingletonType<FormGPS>("AOG", 1, 0, "FormGPS",
    [](QQmlEngine *engine, QJSEngine *jsEngine) -> QObject* {
        return FormGPS::instance();
    });
```

**Problems**:
- Manual registration code in main.cpp (10+ lines per singleton)
- Easy to forget registration (runtime errors)
- No compile-time checks
- Verbose and error-prone

#### ✅ New Pattern (Michael's approach - SettingsManager, AgIOService, Backend)

```cpp
// Unified singleton pattern
class Backend : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    static Backend* instance();  // C++
    static Backend* create(QQmlEngine*, QJSEngine*);  // QML
};

// main.cpp - NOTHING NEEDED!
// Qt 6 auto-registers via QML_ELEMENT + QML_SINGLETON
```

**Benefits**:
- Zero main.cpp code
- Compile-time checks (QML_ELEMENT validates class)
- Single source of truth (class definition only)
- Impossible to forget registration

### Real-World Examples (Already Implemented)

**Michael has already implemented this pattern** in these classes:

**SettingsManager** (already using unified pattern):
```cpp
// classes/settingsmanager.h
class SettingsManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static SettingsManager* instance();
    static SettingsManager* create(QQmlEngine*, QJSEngine*);
};

// QML usage - works automatically
Text { text: SettingsManager.vehicle_width }
```

**AgIOService** (already using unified pattern):
```cpp
// classes/agioservice.h
class AgIOService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static AgIOService* instance();
    static AgIOService* create(QQmlEngine*, QJSEngine*);
};

// QML usage - works automatically
Text { text: AgIOService.gpsFixQuality }
```

### Impact on main.cpp

**Before** (old FormGPS pattern - ~50 lines of registration):
```cpp
// main.cpp - LOTS of boilerplate
qmlRegisterSingletonType<Backend>(...);
qmlRegisterSingletonType<SettingsManager>(...);
qmlRegisterSingletonType<AgIOService>(...);
qmlRegisterSingletonType<CVehicle>(...);
qmlRegisterSingletonType<CTrack>(...);
// ... more registrations
```

**After** (unified pattern - 0 lines):
```cpp
// main.cpp - NOTHING!
// Qt 6 auto-registers all QML_ELEMENT + QML_SINGLETON classes
```

### Future: CoreGPS Singleton

**Michael's Plan**:
> "Eventually FormGPS (or if we rename to CoreGPS) will probably want to be such a singleton so that the QML engine can start it for us."

**Proposed CoreGPS** (FormGPS refactored):
```cpp
// coregps.h (future)
class CoreGPS : public QObject  // No longer QQmlApplicationEngine
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static CoreGPS* instance();  // C++ access
    static CoreGPS* create(QQmlEngine*, QJSEngine*);  // QML auto-start

    // Business logic methods
    Q_INVOKABLE void startFieldOperation();
    Q_INVOKABLE void saveField(const QString& name);

private:
    explicit CoreGPS(QObject* parent = nullptr);
    static CoreGPS* m_instance;
};
```

**Benefits of CoreGPS as unified singleton**:
- QML engine can initialize CoreGPS automatically
- No special main.cpp initialization needed
- Backend can use `setCore(CoreGPS::instance())`
- Clean separation: CoreGPS = business logic, Backend = UI data

### Implementation Checklist

For any new singleton that needs both C++ and QML access:

- [ ] Add `QML_ELEMENT` macro
- [ ] Add `QML_SINGLETON` macro
- [ ] Implement `static T* instance()` for C++
- [ ] Implement `static T* create(QQmlEngine*, QJSEngine*)` for QML
- [ ] `create()` must call `instance()` internally
- [ ] Private constructor
- [ ] Static `m_instance` member
- [ ] Remove any `qmlRegisterSingletonType` from main.cpp

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

## Business Logic Integration Pattern

### Problem: Circular Dependencies

Backend needs to call business logic methods in FormGPS, but we cannot include `formgps.h` in `backend.h` without creating circular dependencies and long compile times.

### Solution: QMetaObject::invokeMethod (Recommended by Michael)

Backend holds a **generic QObject pointer** to the core business logic (FormGPS) and uses **dynamic method invocation** via Qt's meta-object system.

#### Pattern Implementation

**Step 1: Add Core Pointer to Backend**
```cpp
// backend.h
class Backend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* core READ core WRITE setCore NOTIFY coreChanged)

public:
    QObject* core() const { return m_core; }
    void setCore(QObject* core) { m_core = core; emit coreChanged(); }

    // Q_INVOKABLE methods call into core without knowing its type
    Q_INVOKABLE void startFieldOperation() {
        if (m_core) {
            QMetaObject::invokeMethod(m_core, "startFieldOperation");
        }
    }

    Q_INVOKABLE void saveField(const QString& name) {
        if (m_core) {
            QMetaObject::invokeMethod(m_core, "saveField",
                                     Q_ARG(QString, name));
        }
    }

signals:
    void coreChanged();

private:
    QObject* m_core = nullptr;  // Points to FormGPS instance
};
```

**Step 2: Configure in main.cpp**
```cpp
// main.cpp
int main() {
    // Create singletons
    FormGPS* formGPS = FormGPS::instance();
    Backend* backend = Backend::instance();

    // Connect Backend to FormGPS (no header inclusion needed)
    backend->setCore(formGPS);

    // Now QML can call: Backend.startFieldOperation()
    // which dynamically calls: FormGPS::startFieldOperation()
}
```

**Step 3: Implement Well-Documented Methods in FormGPS**
```cpp
// formgps.h
class FormGPS : public QObject
{
    Q_OBJECT

public:
    // Well-documented Q_INVOKABLE methods for Backend to call
    Q_INVOKABLE void startFieldOperation();
    Q_INVOKABLE void saveField(const QString& name);
    Q_INVOKABLE void loadField(const QString& filename);

    // Internal C++ state (NOT exposed to Backend)
private:
    std::vector<Vec3> m_fieldPoints;      // Pure C++ calculation
    QByteArray m_pgnBuffer;               // Parsing buffer
    ComplexCalculationEngine m_engine;    // Business logic
};
```

### Benefits of This Pattern

1. **No Circular Dependencies**: Backend.h doesn't include formgps.h
2. **Fast Compile Times**: Changing FormGPS internals doesn't trigger Backend recompile
3. **Type Safety**: Qt meta-object system validates method signatures at runtime
4. **Clear Interface**: Q_INVOKABLE methods document the Backend↔Core contract
5. **Testability**: Easy to mock core object for Backend unit tests

### Performance Cost

- **Overhead**: ~10-50 nanoseconds per `invokeMethod()` call
- **Impact**: Negligible for UI interactions (user clicks, navigation)
- **Trade-off**: Massive compile-time savings worth minimal runtime cost

### Alternative: Signal-Based Approach (Not Recommended)

```cpp
// Alternative approach - more verbose, less direct
class Backend : public QObject {
    Q_INVOKABLE void startFieldOperation() {
        emit fieldOperationRequested();  // Emit signal
    }
signals:
    void fieldOperationRequested();
};

// Requires connection in main.cpp
connect(Backend::instance(), &Backend::fieldOperationRequested,
        FormGPS::instance(), &FormGPS::startFieldOperation);
```

**Verdict**: QMetaObject::invokeMethod is cleaner and more maintainable for this use case.

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
3. Add `core` property (QObject*) for FormGPS integration
4. Implement basic Q_INVOKABLE methods using QMetaObject::invokeMethod
5. Configure qt_add_qml_module() in CMakeLists.txt
6. Wire Backend→FormGPS connection in main.cpp
7. Test Backend singleton accessible in QML

**Files**:
- `backend/backend.h` (minimal, UI data only)
- `backend/backend.cpp`
- `CMakeLists.txt` (modify)
- `main.cpp` (add backend->setCore(formGPS))

**Directory Structure**:
```
backend/
├── backend.h          (Backend singleton - UI interface)
├── backend.cpp
├── rawgpsdata.h       (Q_GADGET - will be added in Phase 2)
├── blockagedata.h     (Q_GADGET - will be added in Phase 2)
└── ...                (All Q_OBJECT and Q_GADGET UI-related code)
```

**Important**: ALL Q_OBJECT and Q_GADGET code related to QML goes in `backend/` directory. FormGPS and other business logic remain in project root.

**Testing**:
- Verify `Backend` object accessible in QML with `import AOG 1.0`
- Test Q_INVOKABLE method call chain: QML → Backend → FormGPS

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

### 2. Q_INVOKABLE Methods Implementation Pattern
**Question**: How should Backend call FormGPS business logic without circular dependencies?

**Options**:
- **A**: Signal-based approach (Backend emits signals, FormGPS connects)
- **B**: QMetaObject::invokeMethod with core pointer (dynamic invocation)
- **C**: Direct inclusion of formgps.h (creates circular dependency)

**Decision (Michael)**: **Option B** - QMetaObject::invokeMethod is the best approach
- No circular dependencies
- Fast compile times (backend.h changes don't trigger FormGPS recompile)
- Clear Q_INVOKABLE interface contract
- ~10-50ns overhead is negligible for UI interactions

**Status**: ✅ **RESOLVED** - QMetaObject::invokeMethod pattern documented in "Business Logic Integration Pattern" section

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

### 5. Backend Scope and Compile-Time Optimization
**Question**: How do we prevent Backend from becoming like FormGPS (changing it triggers full project recompile)?

**Michael's Concern**:
> "My only concern [...] any C++ class that needs to access the Backend singleton needs to include literally everything (much like now with formgps.h), so compile times remain long when any little part of this is changed."

**Mitigation Strategy (Michael)**:
> "But if we limit Backend and friends to only holding data structures required by the UI, and use a different method for sharing pure C++ state between the classes that can mitigate this problem."

**Guidelines**:
- ✅ **Backend should contain**: UI-required data (Q_GADGET containers), Q_INVOKABLE UI methods
- ❌ **Backend should NOT contain**: C++ calculation buffers, internal algorithms, parsing state
- ✅ **Pure C++ state sharing**: Use existing patterns (FormGPS, CVehicle, CTrack singletons)
- ✅ **Directory isolation**: backend/ contains only QML interface code

**Example - What Goes Where**:

**✅ Backend (UI Data)**:
```cpp
// backend/backend.h - MINIMAL
class Backend : public QObject {
    Q_PROPERTY(RawGPSData rawGPS ...)     // UI needs this
    Q_PROPERTY(FieldData field ...)       // UI needs this
    Q_INVOKABLE void startField();        // UI action
};
```

**✅ FormGPS (C++ Business Logic)**:
```cpp
// formgps.h - C++ internals (not in backend/)
class FormGPS : public QObject {
    std::vector<Vec3> m_fieldPoints;      // C++ calculation
    QByteArray m_pgnBuffer;               // Parsing buffer
    ComplexCalculationEngine m_engine;    // Business logic

    void updatePosition();                // Internal method
    void calculateGuidance();             // Internal method
};
```

**Impact on Compile Time**:
- Backend.h changes: Only backend/ directory recompiles (~5-10 files)
- FormGPS.h changes: Most of project recompiles (~50+ files)
- **Goal**: Keep Backend.h stable and minimal

**Recommendation**: Strictly enforce Backend scope discipline - reject PRs that add C++ internal state to Backend

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

### 2025-12-13: Michael's Approval and Architectural Guidance
- **Decision**: ✅ **APPROVED**
- **Key Decisions**:
  1. **Directory Structure**: Create `backend/` directory for all Q_OBJECT and Q_GADGET QML-related code
  2. **Business Logic Integration**: Use QMetaObject::invokeMethod pattern (preferred over signals)
  3. **Backend Scope**: Backend contains ONLY UI data, NOT C++ internal state
  4. **Compile-Time Mitigation**: Keep Backend.h minimal to avoid formgps.h-style recompile issues

- **Implementation Pattern** (Michael):
  - Backend has `QObject* core` property pointing to FormGPS
  - Q_INVOKABLE methods use `QMetaObject::invokeMethod(m_core, "methodName")` for business logic calls
  - No header inclusion between Backend and FormGPS (no circular dependencies)
  - FormGPS implements well-documented Q_INVOKABLE methods as the Backend↔Core contract

- **Scope Guidelines** (Michael):
  - ✅ Backend: UI-required data (Q_GADGET containers), Q_INVOKABLE UI methods
  - ❌ Backend: C++ calculation buffers, internal algorithms, parsing state
  - ✅ Pure C++ state: Use existing singleton patterns (FormGPS, CVehicle, CTrack)

- **Status**: Ready for Phase 1 implementation
- **Next Action**: Begin Phase 1 - Backend Infrastructure (Week 1)

### 2025-12-14: Unified C++/QML Singleton Pattern Clarification
- **Source**: Michael Torrie (branches: refactorattempt, dev)
- **Key Guidance**:
  > "if a Q_OBJECT class is intended to be a QML_SINGLETON, and if it needs to be accessible to c++ code, it should follow the pattern in backend.h. Needs an instance() method for C++, and a create() method for the QML engine."

- **Pattern Requirements**:
  1. **instance()** method for C++ singleton access
  2. **create()** method for QML engine auto-registration
  3. **Single shared instance** between C++ and QML (create() calls instance())
  4. **QML_ELEMENT + QML_SINGLETON** macros for auto-registration
  5. **Zero main.cpp boilerplate** - Qt 6 handles registration

- **Already Implemented** (by Michael):
  - ✅ SettingsManager: Unified singleton pattern
  - ✅ AgIOService: Unified singleton pattern
  - ✅ No more qmlRegisterSingletonType calls needed

- **Future Plan**:
  - FormGPS → CoreGPS refactoring
  - CoreGPS will adopt unified singleton pattern
  - QML engine can auto-start CoreGPS

- **Documentation Added**: Section 2.3 "Unified C++/QML Singleton Pattern" with:
  - Problem statement (two-instance anti-pattern)
  - Complete implementation example
  - Comparison of old vs new patterns
  - Real-world examples (SettingsManager, AgIOService)
  - Implementation checklist

- **Status**: Pattern clarified and documented
- **Impact**: Backend implementation must follow this exact pattern

---

**Related Documentation**:
- [Qt 6.8 Property Migration Guide](../architecture/migration-qt68-properties.md)
- [QML Integration Architecture](../architecture/qml-integration.md)
- [System Architecture Overview](../architecture/system-architecture.md)
- [Q_GADGET Qt Documentation](https://doc.qt.io/qt-6/qobject.html#Q_GADGET)
- [QProperty Qt Documentation](https://doc.qt.io/qt-6/qproperty.html)

---

**Implementation Tracking**: TBD (create GitHub issue)
