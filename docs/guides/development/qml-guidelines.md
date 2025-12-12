# QML Development Guidelines

**Status**: IMPLEMENTED (Phase 6.0+)
**Last Validated**: 2025-12-06
**Objective**: QML best practices, security patterns, and performance optimization

---

## Table of Contents

1. [Security Patterns](#security-patterns)
2. [Performance Optimization](#performance-optimization)
3. [QML Object Lifecycle](#qml-object-lifecycle)
4. [Property Access Patterns](#property-access-patterns)
5. [Future Architecture](#future-architecture)

---

## Security Patterns

### Critical Protection: nullptr Checks

**Problem**: Accessing QML objects without nullptr checks causes segmentation faults.

**Achievement (Phase 6)**: 53 potential crash points eliminated

**Pattern 1: Standard QML Property Access**

```cpp
// WRONG - Crash if QML object not found
QObject *aog = qmlItem(mainWindow, "aog");
aog->setProperty("frameTime", frameTime);  // CRASH if aog == nullptr

// CORRECT - Safe with nullptr check
QObject *aog = qmlItem(mainWindow, "aog");
if (aog) {  // Verify object exists
    aog->setProperty("frameTime", frameTime);
}
```

**Pattern 2: OpenGL Context with Default Values**

File: [formgps_opengl.cpp](../../../formgps_opengl.cpp)

```cpp
// WRONG - Crash if OpenGL control not found
int width = qmlItem(mainWindow, "openglcontrol")->property("width").toReal();  // CRASH!

// CORRECT - Helper function with defaults
struct OpenGLViewport {
    int width = 800;   // Default fallback
    int height = 600;
    double shiftX = 0.0, shiftY = 0.0;
};

OpenGLViewport getOpenGLViewport(QQuickItem* mainWindow) {
    OpenGLViewport viewport;  // Start with defaults
    QObject *openglControl = qmlItem(mainWindow, "openglcontrol");

    if (openglControl) {  // Safe access
        viewport.width = openglControl->property("width").toReal();
        viewport.height = openglControl->property("height").toReal();
        viewport.shiftX = openglControl->property("shiftX").toReal();
        viewport.shiftY = openglControl->property("shiftY").toReal();
    } else {
        qWarning() << "OpenGL control not found - using defaults";
    }
    return viewport;
}
```

**Benefits**:
- Graceful degradation instead of crashes
- Default values allow application to continue
- Warning messages for diagnostics

### Security Checklist

**Before committing code**:

```cpp
// ❌ NEVER do this
qmlItem(mainWindow, "obj")->setProperty("prop", value);

// ✓ ALWAYS do this
QObject *obj = qmlItem(mainWindow, "obj");
if (obj) {
    obj->setProperty("prop", value);
}
```

**Exceptions**: Only skip nullptr check if QML object is **guaranteed** to exist (e.g., root window).

---

## Performance Optimization

### Class Member Pattern

**Problem**: Repeated `qmlItem()` calls cause unnecessary QML tree traversals.

**Achievement (Phase 6)**: 80-89% reduction in QML lookups

**Anti-Pattern: Local Variables**

```cpp
// INEFFICIENT - 9 QML tree searches for same object
void updateBoundary1() {
    QObject *boundaryInterface = qmlItem(mainWindow, "boundaryInterface");  // Search #1
    if (boundaryInterface) boundaryInterface->setProperty("count", 5);
}

void updateBoundary2() {
    QObject *boundaryInterface = qmlItem(mainWindow, "boundaryInterface");  // Search #2
    if (boundaryInterface) boundaryInterface->setProperty("area", 100);
}

// ... 7 more functions, each doing qmlItem() search
```

**Optimized Pattern: Class Members**

File: [formgps.h](../../../formgps.h)

```cpp
class FormGPS : public QObject {
    Q_OBJECT

private:
    // QML objects as class members
    QObject *contextFlag;
    QObject *boundaryInterface;        // Single instance
    QObject *fieldInterface;
    QObject *recordedPathInterface;
    AOGRendererInSG *openGLControl;
};
```

File: [formgps_ui.cpp](../../../formgps_ui.cpp)

```cpp
// Initialize once in constructor or setupGui()
contextFlag = qmlItem(mainWindow, "contextFlag");
boundaryInterface = qmlItem(mainWindow, "boundaryInterface");  // ONE search
fieldInterface = qmlItem(mainWindow, "fieldInterface");
recordedPathInterface = qmlItem(mainWindow, "recordedPathInterface");
```

File: [formgps_ui_boundary.cpp](../../../formgps_ui_boundary.cpp)

```cpp
// Reuse member in all functions - NO additional searches
void updateBoundary1() {
    if (boundaryInterface) {  // Use cached member
        boundaryInterface->setProperty("count", 5);
    }
}

void updateBoundary2() {
    if (boundaryInterface) {  // Use cached member
        boundaryInterface->setProperty("area", 100);
    }
}
```

**Performance Gains**:

| Object | Before | After | Reduction |
|--------|--------|-------|-----------|
| boundaryInterface | 9× qmlItem() | 1× member | 89% |
| fieldInterface | 6× qmlItem() | 1× member | 83% |
| recordedPathInterface | 5× qmlItem() | 1× member | 80% |

**Total**: 20 redundant QML tree traversals eliminated

### When to Use Class Members

**Use class members for**:
- QML objects accessed in 3+ functions
- Objects accessed frequently (e.g., every frame)
- Critical performance paths (e.g., OpenGL rendering)

**Use local variables for**:
- QML objects accessed once or rarely
- Occasional operations (e.g., initialization)
- Debug/diagnostic code

**Example**:

```cpp
// Class member (frequent access)
QObject *boundaryInterface;  // Used in 9 functions

// Local variable (rare access)
void showTimedMessage(QString msg) {
    QObject *timedMessage = qmlItem(mainWindow, "timedMessage");  // Used once
    if (timedMessage) {
        timedMessage->setProperty("message", msg);
    }
}
```

---

## QML Object Lifecycle

### Dynamic Component Creation

**Pattern: Component.createObject()**

```qml
// MyComponent.qml
Component {
    id: dynamicComponent
    Rectangle {
        width: 100
        height: 100
        Component.onDestruction: {
            console.log("Component destroyed");
        }
    }
}

// Usage
property var instance: null

function createComponent() {
    instance = dynamicComponent.createObject(parent);
}

// CRITICAL: Must explicitly destroy
function destroyComponent() {
    if (instance) {
        instance.destroy();  // Trigger QML deletion
        instance = null;
    }
}
```

**Memory Leak Prevention**: Always call `destroy()` on dynamically created components.

### Loader Component

**Automatic Lifecycle Management**:

```qml
Loader {
    id: loader
    source: "MyComponent.qml"
    active: false  // Component not loaded initially

    function load() {
        active = true;  // Creates component
    }

    function unload() {
        active = false;  // Destroys component automatically ✓
    }
}
```

**Recommendation**: Prefer `Loader` over manual `createObject()` for simpler lifecycle management.

### Repeater Component

**List Item Lifecycle**:

```qml
Repeater {
    model: myListModel
    delegate: ItemDelegate {
        text: modelData.name

        // Created/destroyed automatically with model changes
        Component.onDestruction: {
            console.log("Item destroyed:", modelData.name);
        }
    }
}
```

**Automatic Management**: Qt handles creation/destruction when model changes.

---

## Property Access Patterns

### C++ to QML: setProperty()

**Current Pattern** (Phase 6.0):

```cpp
QObject *aog = qmlItem(mainWindow, "aog");
if (aog) {
    aog->setProperty("latitude", latitude);
    aog->setProperty("longitude", longitude);
}
```

**Limitations**:
- No type safety
- String-based property names (typos not caught at compile time)
- Slow compared to direct Q_PROPERTY binding

**Use Case**: Temporary solution during Qt 6 migration. Works but not optimal.

### QML to C++: Q_PROPERTY

**Modern Pattern** (Phase 6.0.19+):

File: [formgps.h](../../../formgps.h)

```cpp
class FormGPS : public QObject {
    Q_OBJECT

    // Qt 6 BINDABLE property - automatic QML updates
    Q_PROPERTY(double latitude READ latitude WRITE setLatitude
               NOTIFY latitudeChanged BINDABLE bindableLatitude)

private:
    QProperty<double> m_latitude{0.0};

public:
    double latitude() const { return m_latitude.value(); }
    void setLatitude(double lat) { m_latitude.setValue(lat); }
    QBindable<double> bindableLatitude() { return &m_latitude; }

signals:
    void latitudeChanged();
};
```

**QML Binding** (automatic updates):

```qml
Text {
    text: aog.latitude.toFixed(7)  // Automatically updates when latitude changes
}
```

**Benefits**:
- 50× faster than setProperty() [See: profiling-windows.md](profiling-windows.md)
- Type safety at compile time
- Automatic QML updates
- No manual signal emissions

**Recommendation**: Use Q_PROPERTY + QProperty for all new code. Migrate setProperty() gradually.

### Property Binding Optimization

**Anti-Pattern: Binding Cascades**

```qml
// INEFFICIENT - Cascade triggers multiple re-evaluations
property real latitude: aog.latitude  // Binding 1
property real x: latitude * scale     // Binding 2 (depends on Binding 1)
property real screenX: x + offset     // Binding 3 (depends on Binding 2)

// aog.latitude change → 3 bindings triggered (cascade)
```

**Optimized Pattern: Direct Binding**

```qml
// EFFICIENT - Single binding
property real screenX: (aog.latitude * scale) + offset  // One binding

// aog.latitude change → 1 binding triggered ✓
```

**Measurement**: Use Qt Creator QML Profiler to detect binding cascades [See: profiling-windows.md](profiling-windows.md).

---

## Future Architecture

### Current State (Phase 6.0)

**Pattern**: C++ manipulates QML via `qmlItem()` + `setProperty()`

**Status**: ❌ Non-compliant with Qt 6 best practices

**Issues**:
- Tight C++/QML coupling
- No type safety
- Performance overhead
- Violates separation of concerns

### Qt 6 Recommended Architecture

**Pattern**: QML binds to C++ via Q_PROPERTY

**Migration Path**:

**Phase 1 (Current)**: Security hardening
- ✓ All qmlItem() calls protected with nullptr checks
- ✓ Class member optimization for frequent QML access

**Phase 2 (Next)**: Q_PROPERTY migration
- Replace setProperty() with Q_PROPERTY declarations
- QML binds directly to C++ properties
- Example: [SettingsManager](../architecture/system-architecture.md#settingsmanager) (389 Q_PROPERTY)

**Phase 3 (Future)**: Interface injection
- QML declares C++ type requirements
- Qt injects C++ instances automatically
- Zero qmlItem() calls needed

**Example Future Architecture**:

```cpp
// C++ - Pure business logic
class GPSController : public QObject {
    Q_OBJECT
    Q_PROPERTY(double latitude MEMBER m_latitude NOTIFY latitudeChanged)
    Q_PROPERTY(double longitude MEMBER m_longitude NOTIFY longitudeChanged)

public:
    void updatePosition(double lat, double lon) {
        m_latitude = lat;    // QML updates automatically
        m_longitude = lon;
    }

private:
    double m_latitude = 0.0;
    double m_longitude = 0.0;
};
```

```qml
// QML - Pure presentation
import QtQuick

Window {
    // Qt injects GPSController instance
    required property GPSController gpsController

    Text {
        text: gpsController.latitude.toFixed(7)  // Automatic binding
    }
}
```

**Benefits**:
- Zero qmlItem() calls
- Full type safety
- Clear separation of concerns
- Maximum performance

**Timeline**: After AgIO Phase completion (Phase 6.1+)

---

## Quick Reference

### Security Checklist

```cpp
✓ Always check if (obj) before obj->setProperty()
✓ Use default values for critical properties (OpenGL viewport)
✓ Log warnings when QML objects not found
✓ Test with missing/incomplete QML files
```

### Performance Checklist

```cpp
✓ Use class members for frequently accessed QML objects (3+ functions)
✓ Initialize QML members once in constructor/setupGui()
✓ Profile with Qt Creator QML Profiler to detect binding cascades
✓ Prefer Q_PROPERTY over setProperty() for new code
```

### Lifecycle Checklist

```qml
✓ Call destroy() on all createObject() instances
✓ Use Loader for automatic lifecycle management
✓ Implement Component.onDestruction for cleanup
✓ Clear QML engine cache in destructor: clearComponentCache()
```

---

## References

**Related Documentation**:
- [System Architecture](../architecture/system-architecture.md) - QML integration patterns
- [QML Integration](../architecture/qml-integration.md) - 6 primary QML services
- [Memory Debugging](memory-debugging.md) - QML memory management patterns
- [Phase 6.0.45 Validation](phase-6-0-45-validation.md) - QML cleanup effectiveness

**Qt Documentation**:
- QML Object Ownership: https://doc.qt.io/qt-6/qtqml-cppintegration-data.html#data-ownership
- Q_PROPERTY: https://doc.qt.io/qt-6/properties.html
- QProperty: https://doc.qt.io/qt-6/qproperty.html

**Case Studies**:
- Phase 6.0: 53 crash points eliminated with nullptr checks
- Phase 6.0: 80-89% QML lookup reduction with class member pattern
- Phase 6.0.45: 99.1% QQmlObjectCreator leak reduction with proper cleanup

---

**Status**: IMPLEMENTED (Phase 6.0+)
**Validation**: Tested with Qt 6.8.2, zero crashes in production
**Last Validated**: 2025-12-06
