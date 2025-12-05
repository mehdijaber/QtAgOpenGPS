# QtAgOpenGPS Architecture Refactoring Proposal

**Date**: 2025-01-05
**Context**: Discussion about FormGPS architectural improvements
**Status**: Proposal for discussion - awaiting architectural decisions

---

## Executive Summary

This document analyzes the current QtAgOpenGPS architecture and proposes refactoring recommendations to address:
1. FormGPS "unwieldy" God Object problem
2. Dependency cycles between classes
3. Deprecated `setContextProperty` usage
4. Mixed responsibilities (vehicle physics vs implement operations)
5. Scene Graph migration preparation (QSGGeometryNode path)

All recommendations are aligned with **Qt 6 official best practices** and modern patterns.

---

## Current Architectural Problems

### Problem 1: FormGPS is "Unwieldy" (God Object)

**Michael's observation**: "FormGPS is unwieldy right now"

**Current state**:
- FormGPS contains 1000+ lines of mixed responsibilities
- Rendering logic (oglMainPaint) + business logic + state management
- Tight coupling with all other classes
- Difficult to test, maintain, refactor

**Impact on Scene Graph migration**:
- Cannot migrate to QSGGeometryNode cleanly
- updatePaintNode() requires pure rendering logic (no FormGPS dependencies)
- Risk of copying all problems into new architecture

### Problem 2: Dependency Cycles

**Michael's observation**: "I need a way to break the cycle of the classes back to FormGPS"

**Current cycles**:
```
FormGPS ↔ CVehicle  (both reference each other)
FormGPS ↔ CTrack    (both reference each other)
FormGPS ↔ CBoundary (both reference each other)
```

**Root cause**:
```cpp
// cvehicle.h - PROBLEM: Pointer to FormGPS
class CVehicle {
    FormGPS *mf;  // Creates cycle!

    void updatePosition() {
        mf->setProperty("vehicleX", x);  // Direct access
    }
};
```

**Historical context**:
- Before AI rewrite: Used signals/slots (no cycles) ✅
- After AI rewrite: Direct pointers to FormGPS (cycles restored) ❌

### Problem 3: SettingsManager Shortcuts

**Michael's observation**: "Remove all member variables that are just shortcuts to the SettingsManager"

**Current pattern**:
```cpp
class FormGPS {
    // Dozens of shortcuts like this:
    double &vehicleWidth = SettingsManager::instance()->vehicle_width;
    double &toolWidth = SettingsManager::instance()->tool_width;
    // ... many more
};
```

**Michael's proposed solution**:
```cpp
// Direct access (compiler optimizes anyway)
void someFunction() {
    double width = SettingsManager::instance()->vehicle_width();
    // No performance loss, cleaner code
}
```

**Benefits**:
- Fewer FormGPS members
- Less coupling
- Faster compilation
- Compiler optimizes calls (no performance impact)

### Problem 4: setContextProperty is DEPRECATED

**Current usage** (main.cpp):
```cpp
engine.rootContext()->setContextProperty("aog", formgps);  // DEPRECATED!
```

**Qt official documentation** ([Context Properties](https://doc.qt.io/qt-6/qtqml-cppintegration-contextproperties.html)):
> "Context properties should be avoided in favour of Singletons."

**Performance impact** ([benchmark data](https://somcosoftware.com/en/blog/how-to-integrate-c-and-qml-registering-c-class-as-singleton-to-qml)):
- Singleton: 440ms ✅
- registerType: 470ms (6% slower)
- contextProperty: 570ms (30% slower) ❌

**Required change**:
```cpp
// formgps.h
class FormGPS : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
};

// QML access
import QtAgOpenGPS 1.0
Item {
    Component.onCompleted: {
        FormGPS.someMethod()  // Singleton access
    }
}
```

### Problem 5: Mixed Responsibilities (Vehicle vs Implement)

**Observation**: CVehicle contains both vehicle physics AND implement operations

**CVehicle currently contains**:

✅ **Legitimate vehicle properties**:
- Dimensions: wheelbase, trackWidth, antennaHeight, antennaPivot
- Type: vehicleType (0=tractor, 1=harvester, 2=4WD)
- Position: fixHeading, fixEasting, fixNorthing, pivotAxlePos, steerAxlePos
- Speed: avgSpeed, slowSpeedCutoff, panicStopSpeed
- Autosteer: goalPointLookAhead, stanleyGains, maxSteerAngle

❌ **Implement/tool properties (should be separated)**:
- `CSection sections[MAXSECTIONS]` - sections array (line 141)
- `toolPivotPos, toolPos` - tool positions (lines 121-122)
- `sectionTriggerDistance` - section triggering (line 138)
- `totalSquareMeters, totalUserSquareMeters` - work metrics (line 144)
- `isHydLiftOn, hydLiftDown, hydLiftLookAheadTime` - hydraulic lift (lines 35-36, 84)
- `leftTramState, rightTramState` - tram indicators (lines 38-39)

**Problem**: Violates Single Responsibility Principle
- CVehicle does 2 jobs: "what drives" (vehicle) AND "what works" (implement)
- Cannot test vehicle without implement
- Cannot have multiple implements for one vehicle
- Conceptual confusion

### Problem 6: Static Functions Opportunity

**Daniel's discussion**: "Most of the program only has a single instance - like a PGN is a readonly static object. As are most functions. Faster as well."

**Current pattern**:
```cpp
// Instance-based (indirect call)
class Utils {
public:
    double degreesToRadians(double deg);
};
Utils *utils = new Utils();
double rad = utils->degreesToRadians(45);  // Pointer indirection
```

**Potential optimization**:
```cpp
// Static (direct call, inlinable)
namespace MathUtils {
    static inline double degreesToRadians(double deg) {
        return deg * M_PI / 180.0;
    }
}
double rad = MathUtils::degreesToRadians(45);  // Direct, fast
```

**Benefits**:
- No pointer indirection
- Compiler can inline
- Less memory allocations
- Thread-safe (if stateless)

**When to use static**:
- ✅ Pure utility functions (no state)
- ✅ Readonly objects (PGN parser)
- ✅ Math helpers
- ❌ NOT for objects with state (CVehicle, CTrack)

---

## Michael's Scene Graph Strategy (Current Direction)

### Decision Made: QSGGeometryNode Approach

**Source**: [Michael's Scenegraph Wiki](https://github.com/torriem/QtAgOpenGPS/wiki/Scenegraph) and [Claude Conversation](https://github.com/torriem/QtAgOpenGPS/wiki/claude-conversation-about-scenegraph)

**Michael's chosen approach**: "Single QQuickItem that rebuilds scene tree on each update"

### Technical Strategy

**Philosophy**: Return to oglMainPaint() simplicity but via Scene Graph native rendering

**Implementation approach**:
```cpp
class FieldRenderer : public QQuickItem {
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override {
        // COMPLETE REBUILD each frame (mirrors oglMainPaint philosophy)
        delete oldNode;
        QSGNode *rootNode = new QSGNode();

        // For each section
        for (section : triStrip) {
            // Manual culling (kept from oglMainPaint)
            if (!isVisible(section)) continue;

            // Create QSGGeometryNode
            // ... geometry creation
        }

        return rootNode;
    }
};
```

### Key Technical Decisions

**1. Rebuild vs Smart Caching**:
- Michael chose: **Complete rebuild** each frame
- Rationale: Simplicity > optimization (can optimize later if needed)
- No state management needed (don't track which nodes changed)

**2. Custom Matrices**:
- QtAgOpenGPS uses custom projection matrices (orthographic 2D on 3D world)
- Solution: Combine viewport + projection + modelview in QSGTransformNode
- **Z-values preserved**: Scene Graph uses Z for depth ordering (painters algorithm)

**3. Culling Strategy**:
- **Problem discovered**: Scene Graph culling is per-item (based on untransformed boundingRect), not per-node
- Nodes off-screen after transform are still processed
- **Solution**: Keep manual culling in updatePaintNode() (like oglMainPaint)

**4. Thread Safety** (Critical):
- Scene graph nodes created/modified ONLY in updatePaintNode()
- NEVER from FormGPS callbacks or external threads
- Pattern: Queue data on main thread → build nodes in updatePaintNode()

### Three Methodologies Compared

| Approach | Simplicity | Performance | State Management | Michael's Verdict |
|----------|------------|-------------|------------------|-------------------|
| Single QQuickItem (rebuild) | ⭐⭐⭐ | ⭐⭐ | Minimal | ✅ **Recommended** |
| Smart Custom Nodes (cache) | ⭐ | ⭐⭐⭐ | Complex | Maybe later |
| Multiple Layered Items | ⭐⭐ | ⭐⭐⭐ | Synchronization | Too complex |

### Implications for Refactoring

**Critical insight**: Michael's QSGGeometryNode strategy DEPENDS on FormGPS refactoring first:

1. **oglMainPaint() extraction** (Recommendation #1) becomes urgent
2. Rendering logic must be **pure and stateless** for rebuild pattern
3. No FormGPS dependencies in updatePaintNode() (thread safety)
4. Data must be prepared on main thread, nodes built in render thread

**Timeline**: FormGPS refactoring should happen BEFORE QSGGeometryNode migration

---

## Architectural Recommendations

### Recommendation 1: Extract Rendering Logic from FormGPS

**Problem**: oglMainPaint() mixes business logic + rendering (1000+ lines)

**CRITICAL**: This refactoring is now URGENT - Michael's QSGGeometryNode strategy depends on it!

**Proposed architecture (aligned with Michael's rebuild pattern)**:

```cpp
// NEW FILE: fieldrendererdata.h
// Simpler than initially proposed - Michael wants stateless rebuild
struct PatchData {
    QVector<QVector3D> vertices;
    QColor color;
    bool visible;  // Pre-calculated culling
};

class FieldRendererData {
public:
    QList<PatchData> patches;
    QMatrix4x4 combinedMatrix;  // viewport * projection * modelview

    // NO pointers to FormGPS!
    // Pure data for thread-safe transfer
};
```

**FormGPS produces data** (main thread):
```cpp
class FormGPS {
    FieldRendererData prepareRenderData() {
        FieldRendererData data;

        // Business logic (culling, sorting, matrix calc)
        for (section : triStrip) {
            for (patch : section.patchList) {
                PatchData patchData;

                // Manual culling (kept from oglMainPaint)
                patchData.visible = isVisible(patch);

                // Copy vertex data
                patchData.vertices = patch.vertices;
                patchData.color = patch.color;

                data.patches.append(patchData);
            }
        }

        // Pre-calculate combined matrix
        data.combinedMatrix = calculateCombinedMatrix();

        return data;
    }
};
```

**FieldRenderer consumes data** (render thread):
```cpp
class FieldRenderer : public QQuickItem {
    Q_PROPERTY(FieldRendererData* renderData READ renderData WRITE setRenderData)

    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override {
        // Michael's rebuild pattern: delete old, rebuild fresh
        delete oldNode;
        QSGNode *rootNode = new QSGNode();

        if (!m_renderData) return rootNode;

        // Pure rendering - NO FormGPS dependency!
        for (const auto &patch : m_renderData->patches) {
            if (!patch.visible) continue;  // Pre-calculated culling

            // Create geometry
            QSGGeometry *geometry = new QSGGeometry(
                QSGGeometry::defaultAttributes_Point2D(),
                patch.vertices.size()
            );

            auto *vertices = geometry->vertexDataAsPoint2D();
            for (int i = 0; i < patch.vertices.size(); i++) {
                vertices[i].set(patch.vertices[i].x(),
                               patch.vertices[i].y());
            }

            // Transform node with combined matrix
            QSGTransformNode *transformNode = new QSGTransformNode();
            transformNode->setMatrix(m_renderData->combinedMatrix);

            // Geometry node
            QSGGeometryNode *geoNode = new QSGGeometryNode();
            geoNode->setGeometry(geometry);
            geoNode->setFlag(QSGNode::OwnsGeometry);

            // Material (simple flat color for now)
            QSGFlatColorMaterial *material = new QSGFlatColorMaterial();
            material->setColor(patch.color);
            geoNode->setMaterial(material);
            geoNode->setFlag(QSGNode::OwnsMaterial);

            transformNode->appendChildNode(geoNode);
            rootNode->appendChildNode(transformNode);
        }

        return rootNode;
    }

private:
    FieldRendererData *m_renderData = nullptr;
};
```

**Benefits**:
- ✅ Separation of concerns (MVC pattern)
- ✅ FormGPS = Controller (business logic, main thread)
- ✅ FieldRenderer = View (pure rendering, render thread)
- ✅ **Thread-safe**: Data prepared on main thread, nodes built on render thread
- ✅ **Stateless**: Complete rebuild each frame (Michael's pattern)
- ✅ **Clean migration path**: oglMainPaint logic preserved, just Qt Scene Graph output
- ✅ Testable rendering without FormGPS

**Alignment with Michael's strategy**:
- ✅ Rebuild pattern (not caching/smart nodes)
- ✅ Manual culling preserved
- ✅ Z-values preserved via QSGTransformNode
- ✅ Simplicity over optimization (can optimize later)

**Qt Scene Graph best practices** ([official docs](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html)):
> "Custom nodes are added to the scene graph by subclassing QQuickItem::updatePaintNode()"
> "All classes with QSG prefix should be used solely on the scene graph's rendering thread"

### Recommendation 2: Break Dependency Cycles with Qt 6 QProperty Bindings

**Problem**: Classes have `FormGPS *mf` pointers creating cycles

**INCORRECT approach** (interfaces):
```cpp
// ❌ NOT recommended by Qt
class IVehicleListener {
    virtual void onVehiclePositionChanged(...) = 0;
};
```

**CORRECT approach** (Qt 6 QProperty bindings):

**Source (CVehicle) - No pointer to FormGPS**:
```cpp
// cvehicle.h
class CVehicle : public QObject {
    Q_OBJECT
    Q_PROPERTY(double x BINDABLE bindableX)
    Q_PROPERTY(double y BINDABLE bindableY)
    Q_PROPERTY(double heading BINDABLE bindableHeading)

    // NO pointer to FormGPS!

private:
    QProperty<double> m_x;
    QProperty<double> m_y;
    QProperty<double> m_heading;
};
```

**Consumer (FormGPS) - Bindings (unidirectional)**:
```cpp
// formgps.h
class FormGPS : public QObject {
    Q_OBJECT
    Q_PROPERTY(double vehicleX BINDABLE bindableVehicleX)

public:
    FormGPS() {
        vehicle = new CVehicle(this);

        // Automatic binding (Qt 6 magic!)
        m_vehicleX.setBinding([this]() {
            return vehicle->x();
        });
        // Now m_vehicleX auto-updates when vehicle->x() changes!
    }

private:
    CVehicle *vehicle;   // FormGPS → CVehicle (ownership)
    QProperty<double> m_vehicleX;  // Binding (unidirectional)
};
```

**Flow (no cycle)**:
```
CVehicle (source) → QProperty<x>
                        ↓ (unidirectional binding)
FormGPS (consumer) ← QProperty<vehicleX>
```

**Benefits**:
- ✅ No cycles: Qt detects and prevents cycles
- ✅ Performance: No signal/slot overhead
- ✅ Automatic: No manual slots
- ✅ QML native: Direct binding to QML
- ✅ Thread-safe: Qt manages synchronization

**When to use bindings vs signals**:

Use **bindings** (Qt 6 recommended):
- ✅ State synchronization (x, y, width, enabled, etc.)
- ✅ Derived calculations (displayX = x + offset)
- ✅ Related properties (enabled = isConnected && isReady)

Use **signals** (still valid):
- ✅ One-time events (clicked, finished, error)
- ✅ Actions (save(), load(), process())
- ✅ Notifications (logMessage, statusUpdate)

**QtAgOpenGPS already uses this pattern**:
```cpp
// settingsmanager.h - ALREADY CORRECT!
Q_OBJECT_BINDABLE_PROPERTY(SettingsManager, double, vehicle_width, ...)

// formgps.h - ALREADY CORRECT!
Q_PROPERTY(double latitude BINDABLE bindableLatitude)
private:
    QProperty<double> m_latitude{0.0};
```

### Recommendation 3: Separate Vehicle and Implement

**Problem**: CVehicle mixes vehicle physics + implement operations

**Proposed architecture**:

```
CVehicle (QObject, QML_SINGLETON)
├─ Vehicle dimensions & physics
├─ Position & heading & speed
├─ Autosteer parameters
└─ QProperty bindings (no FormGPS pointers)

CImplement (QObject, QML_SINGLETON) - NEW CLASS
├─ Sections runtime data (sections[])
├─ Hydraulic lift control
├─ Tool positioning (toolPivotPos, toolPos)
├─ Area tracking metrics
├─ Tram indicators
└─ QProperty bindings to CVehicle (for position)

CTool (Plain C++ class)
├─ Tool configuration (geometry)
├─ Section configuration (colors, zones)
├─ Attachment type settings
└─ NOT QObject (static configuration)
```

**Example: CImplement binds to CVehicle**:
```cpp
class CImplement : public QObject {
    Q_OBJECT

    Q_PROPERTY(double toolPivotX BINDABLE bindableToolPivotX)
    Q_PROPERTY(bool isHydLiftOn BINDABLE bindableIsHydLiftOn)

public:
    CImplement() {
        // Automatic binding to vehicle position
        m_toolPivotX.setBinding([this]() {
            return CVehicle::instance()->fixEasting() + calculateToolOffset();
        });
    }

    CSection sections[MAXSECTIONS];
    double totalSquareMeters = 0.0;
    int leftTramState = 0;

private:
    Q_OBJECT_BINDABLE_PROPERTY(CImplement, double, m_toolPivotX, &CImplement::toolPivotXChanged)
    Q_OBJECT_BINDABLE_PROPERTY(CImplement, bool, m_isHydLiftOn, &CImplement::isHydLiftOnChanged)
};
```

**Benefits**:
- Clear separation: "what drives" (vehicle) vs "what works" (implement)
- No cycles: CImplement reads CVehicle via bindings (unidirectional)
- Flexibility: Multiple implements for one vehicle possible
- Testability: Test vehicle without implement
- Alignment with Michael's goals: "break the dependency cycles" ✅

### Recommendation 4: Static Functions for Utilities

**Candidates for static functions**:

```cpp
// utils/mathutils.h
namespace MathUtils {
    static inline double degreesToRadians(double deg) {
        return deg * M_PI / 180.0;
    }

    static inline QVector3D rotatePoint(const QVector3D &p, double angle) {
        // Pure calculation, no state
    }
}

// pgnparser.h (readonly, stateless)
class PGNParser {
public:
    static ParsedPGN parse(const QByteArray &data);
    // No instance needed!
};
```

**NOT candidates** (have state):
- CVehicle (position, speed, state)
- CTrack (AB line state)
- FormGPS (application state)

### Recommendation 5: Replace setContextProperty with QML_SINGLETON

**Current** (deprecated):
```cpp
// main.cpp
engine.rootContext()->setContextProperty("aog", formgps);  // ❌
```

**Recommended** (Qt 6):
```cpp
// formgps.h
class FormGPS : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static FormGPS *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine) {
        return instance();
    }
};

// QML
import QtAgOpenGPS 1.0
Item {
    Component.onCompleted: {
        FormGPS.latitude = 45.0  // Direct singleton access
    }
}
```

**Benefits**:
- 30% faster than contextProperty
- Type-safe (tooling support)
- QML Language Server works
- Aligned with Qt 6 best practices

### Recommendation 6: Remove SettingsManager Shortcuts

**Current**:
```cpp
class FormGPS {
    double &vehicleWidth = SettingsManager::instance()->vehicle_width;
    // ... dozens more
};
```

**Recommended**:
```cpp
class FormGPS {
    // Direct access (compiler optimizes)
    void someMethod() {
        double width = SettingsManager::instance()->vehicle_width();
    }
};
```

**Benefits**:
- Fewer FormGPS members
- Cleaner header
- Faster compilation
- No performance loss (compiler optimizes)

---

## Synthesis: Recommendations vs Qt Official Position

| Aspect | Initial Recommendation | Qt Official Position | Alignment |
|--------|------------------------|---------------------|-----------|
| setContextProperty | Avoid | DEPRECATED | ✅ Aligned |
| Singletons for globals | Yes | Yes (but don't overuse) | ✅ Aligned |
| Everything in QML | Yes | No - mix singleton/registerType | ❌ Incorrect |
| Interfaces for cycles | Yes | No - use QProperty bindings | ❌ Incorrect |
| Thread safety Scene Graph | Yes | Yes (critical) | ✅ Aligned |
| Rendering/logic separation | Yes | Yes | ✅ Aligned |
| Static functions | Yes | No official position | ⚠️ Neutral |
| updatePaintNode() only place | Yes | Yes (mandatory) | ✅ Aligned |

**Final recommendations aligned with Qt**:
- ✅ Keep singletons for SettingsManager, FormGPS, AgIOService
- ✅ Replace setContextProperty with QML_SINGLETON (urgent)
- ✅ Use QProperty bindings to break cycles (NOT interfaces)
- ✅ Separate data/rendering with updatePaintNode()
- ⚠️ Do NOT put everything in QML (keep C++ singletons)
- ✅ Thread safety: nodes created ONLY in updatePaintNode()

---

## Architectural Decisions Needed

### Decision 1: Instantiation Strategy

**Question**: Where should objects be instantiated?

**Options**:
A. **Current approach**: Everything in C++ (main.cpp) with qmlRegisterSingleton
B. **QML approach**: Instantiate from QML with dependency injection
C. **Hybrid approach**: Singletons in C++, registerType for multiples

**Qt recommendation**: Hybrid approach
- Singleton: For truly global state (SettingsManager, FormGPS, AgIOService)
- registerType: For objects that could have multiple instances
- QML instantiation: For simple logic, not performance-critical

**Current QtAgOpenGPS**:
```cpp
// Singleton candidates (keep as-is)
✅ SettingsManager  (global config)
✅ FormGPS          (global coordinator)
✅ AgIOService      (global I/O)

// Questionable (single instance now, but logically multiple)
⚠️ CVehicle        (could have multiple vehicles)
⚠️ CTrack          (could have multiple tracks)
⚠️ CBoundary       (could have multiple fields)
```

### Decision 2: Breaking Cycles

**Question**: How to break FormGPS dependency cycles?

**Options**:
A. Qt 6 QProperty bindings (recommended)
B. Traditional signals/slots (Qt 5 style)
C. Custom interfaces (IVehicleListener, etc.)

**Recommendation**: Option A (QProperty bindings)
- Modern Qt 6 pattern
- Automatic synchronization
- Better performance
- Already used in SettingsManager

### Decision 3: Vehicle/Implement Separation

**Question**: Should we separate CVehicle and create CImplement?

**Impact**:
- 8 hours development time
- Many files to modify (~20-30)
- Breaking change for QML
- Better architecture long-term

**Trade-off**: Short-term pain for long-term gain

### Decision 4: Rendering Logic Extraction

**Question**: Extract rendering logic before QSGGeometryNode migration?

**Options**:
A. Refactor now → Clean QSGGeometryNode migration
B. Migrate first → Refactor later (risk carrying problems forward)

**Recommendation**: Option A (refactor first)
- Michael's inclination: "Gonna think about this for a while"
- Cleaner migration path
- Less technical debt

### Decision 5: Static Functions

**Question**: Convert utility functions to static?

**Impact**: Minor performance gain, cleaner code

**Candidates**:
- Math utilities (degreesToRadians, rotatePoint, etc.)
- PGN parser (readonly, stateless)
- Geometry calculations

**NOT candidates**: CVehicle, CTrack, FormGPS (have state)

---

## Timeline Considerations

**Quick wins** (1-2 days):
1. Replace setContextProperty with QML_SINGLETON
2. Remove SettingsManager shortcuts from FormGPS
3. Convert utility functions to static

**Medium effort** (1 week):
1. Break dependency cycles with QProperty bindings
2. Extract rendering logic (FieldRendererData)

**Large refactoring** (2-3 weeks):
1. Separate CVehicle/CImplement
2. QSGGeometryNode migration

---

## Open Questions for Discussion

1. **Instantiation**: Keep current C++ singletons or move some to QML?
2. **Timeline**: Refactor before or after QSGGeometryNode migration?
3. **Vehicle/Implement**: Worth the effort to separate now?
4. **Static functions**: Convert utilities or keep as-is?
5. **Incremental vs Big Bang**: Gradual refactoring or complete rewrite?

---

## References

**Qt Official Documentation**:
- [Singletons in QML](https://doc.qt.io/qt-6/qml-singleton.html)
- [QML and C++ Integration](https://doc.qt.io/qt-6/qtqml-cppintegration-overview.html)
- [Context Properties (Deprecated)](https://doc.qt.io/qt-6/qtqml-cppintegration-contextproperties.html)
- [Qt Quick Scene Graph](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html)
- [QSGGeometryNode Class](https://doc.qt.io/qt-6/qsggeometrynode.html)

**Community Resources**:
- [Why setContextProperty is bad](https://raymii.org/s/articles/Qt_QML_Integrate_Cpp_with_QML_and_why_ContextProperties_are_bad.html)
- [Somco Software: C++ QML Integration](https://somcosoftware.com/en/blog/how-to-integrate-c-and-qml-registering-c-class-as-singleton-to-qml)
- [QML Guide: Singletons](https://qml.guide/singletons/)

---

**Next Steps**: Review this proposal with Michael and make architectural decisions before proceeding with implementation planning.
