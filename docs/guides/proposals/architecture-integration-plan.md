# Architecture Integration Plan - QtAgOpenGPS

**Date**: 2024-12-14
**Status**: PENDING TEAM VALIDATION
**Authors**: Mehdi Jaber, Michael Torrie

## Executive Summary

This document integrates three architectural proposals for QtAgOpenGPS:
- **backend-qgadget-architecture.md** (Data Organization)
- **rendering-architecture.md** (Rendering Technology)
- **architecture-refactoring.md** (Code Organization)

**Key Decision**: These proposals are **complementary**, not competing. They address different architectural layers and must be implemented in sequence.

**Implementation Order**: Backend Infrastructure → FormGPS Refactoring → Qt Design Studio Conventions → Scene Graph Migration

---

## Overview of Existing Proposals

### 1. backend-qgadget-architecture.md (Dec 13-14, 2024)
**Focus**: UI data organization in Backend singleton

**Key Proposals**:
- Backend singleton with 6 Q_GADGET containers (RawGPSData, BlockageData, VehicleState, FieldData, GuidanceData, HardwareStatus)
- QMetaObject::invokeMethod pattern to call FormGPS without circular dependencies
- Unified C++/QML singleton pattern (instance() + create())
- backend/ directory for all QML-related code

### 2. rendering-architecture.md (Nov 29, 2024)
**Focus**: Technical migration from OpenGL to Qt Scene Graph

**Key Proposals**:
- Analysis of qpback branch (buffer caching, bounding boxes, frustum culling)
- Migration path: QSGRenderNode (phase 1) then QSGGeometryNode (phase 2)
- Appendix C: comprehensive comparison of Qt rendering options
- Automatic batching by material (100+ draw calls → 3-10)

### 3. architecture-refactoring.md (Jan 5, 2025)
**Focus**: FormGPS God Object refactoring

**Key Proposals**:
- Extract FieldRendererData (pure data, thread-safe)
- Break dependency cycles with QProperty bindings
- Separate CVehicle / CImplement (NEW class)
- setContextProperty → QML_SINGLETON
- Remove SettingsManager shortcuts
- Static functions for utilities

---

## Analysis of Overlaps and Redundancies

### ❌ REDUNDANCY #1: UI Data Organization

**backend-qgadget** proposes:
```cpp
// Backend with Q_GADGET containers
Backend::instance()->rawGPS.latitude
Backend::instance()->vehicle.speed
Backend::instance()->field.workedArea
```

**architecture-refactoring** proposes:
```cpp
// FieldRendererData (for Scene Graph)
class FieldRendererData {
    QList<PatchData> patches;
    QMatrix4x4 combinedMatrix;
};
```

**Verdict**: These two approaches do **the same thing** - separate UI data from business logic!

**Resolution**: Backend Q_GADGET containers **REPLACE** FieldRendererData
- Backend covers 6 domains (GPS, vehicle, field, guidance, blockage, hardware)
- FieldRendererData only covered patches + matrices
- Backend is QML-friendly (Q_GADGET auto-accessible)
- FieldRendererData was only for Scene Graph, Backend is global

### ❌ REDUNDANCY #2: Unified C++/QML Singleton Pattern

**backend-qgadget** documents:
```cpp
class Backend {
    static Backend* instance();  // C++
    static Backend* create(QQmlEngine*, QJSEngine*);  // QML
};
```

**architecture-refactoring** uses (Recommendation #5):
```cpp
class FormGPS {
    QML_SINGLETON
    static FormGPS* create(...) { return instance(); }
};
```

**Verdict**: **SAME pattern**, just applied to different classes

**Resolution**: Apply this pattern to ALL singleton classes (Backend, FormGPS, CVehicle, CTrack)

### ❌ REDUNDANCY #3: Breaking Dependency Cycles

**backend-qgadget** proposes:
```cpp
// Backend has NO pointer to FormGPS
// Uses QMetaObject::invokeMethod(m_core, "method")
```

**architecture-refactoring** proposes (Recommendation #2):
```cpp
// CVehicle has NO pointer to FormGPS
// FormGPS uses QProperty bindings to read CVehicle
```

**Verdict**: **SAME objective**, different but complementary strategies
- Backend → FormGPS: QMetaObject::invokeMethod (method calls)
- FormGPS → CVehicle: QProperty bindings (state reading)

### ✅ COMPLEMENTARITY #1: Backend + FormGPS Refactoring

**backend-qgadget** organizes **UI data**
**architecture-refactoring** breaks **God Object FormGPS**

**Synergy**:
```
BEFORE:
FormGPS (67 properties) + business logic + rendering
           ↓
          Chaos

AFTER (combined):
Backend (6 Q_GADGET containers) ← Pure UI data
    ↓ (QMetaObject::invokeMethod)
CoreGPS (pure business logic) ← Business logic
    ↓ (prepareRenderData)
FieldRenderer (Scene Graph) ← Pure rendering
```

### ✅ COMPLEMENTARITY #2: Backend + Scene Graph

**backend-qgadget** provides organized UI data
**rendering-architecture** uses this data for Scene Graph

**Proposed Flow**:
```cpp
// 1. CoreGPS produces business data
CoreGPS::updatePosition() {
    // GPS calculations, guidance, etc.

    // 2. Updates Backend (UI data)
    RawGPSData gps;
    gps.latitude = calculatedLat;
    Backend::instance()->setRawGPS(gps);
}

// 3. FieldRenderer reads Backend for Scene Graph
QSGNode* FieldRenderer::updatePaintNode() {
    auto gps = Backend::instance()->rawGPS();
    auto field = Backend::instance()->field();

    // Uses this data to build nodes
    // ...
}
```

---

## Recommended Implementation Order

### PHASE 0: Quick Wins ⚡
**Source**: architecture-refactoring.md Recommendations #5, #6

**Actions**:
1. Replace setContextProperty → QML_SINGLETON (FormGPS, CVehicle, CTrack)
2. Remove SettingsManager shortcuts from FormGPS
3. Convert utility functions to static

**Why First**: No dependencies, immediate gains

**Dependencies**: None

---

### PHASE 1: Backend Infrastructure 🏗️
**Source**: backend-qgadget-architecture.md Phase 1-2

**Actions**:
1. Create backend/ directory
2. Implement Backend singleton (instance() + create())
3. Create 6 Q_GADGET containers (RawGPSData, BlockageData, VehicleState, FieldData, GuidanceData, HardwareStatus)
4. Wire Backend → FormGPS with QMetaObject::invokeMethod
5. Migrate FormGPS properties → Backend containers (parallel run)

**Why Now**:
- Provides clean structure for Phase 2
- Backend becomes "single source of truth" for UI data
- Allows beginning to break FormGPS God Object

**Dependencies**: None (can start immediately after Phase 0)

**Validation Criteria**:
- Backend singleton accessible from both C++ and QML
- All 6 Q_GADGET containers functional
- QML bindings update automatically when Backend data changes
- Thread-safe data access verified

---

### PHASE 2: FormGPS Refactoring 🔨
**Source**: architecture-refactoring.md Recommendations #1, #2, #3, #4

**Actions**:
1. Extract rendering logic from FormGPS (use Backend data)
2. Break dependency cycles:
   - FormGPS → CVehicle: QProperty bindings
   - Backend → FormGPS: QMetaObject::invokeMethod (already done Phase 1)
3. **Separate CVehicle / CImplement** (NEW class) ✅ **USER VALIDATED: Do NOW**
4. FormGPS → CoreGPS rename (pure business logic)

**Why Now**:
- Backend already in place (Phase 1) to store UI data
- Break FormGPS God Object BEFORE Scene Graph migration
- Avoids copying problems into new architecture

**Dependencies**: Backend infrastructure (Phase 1)

**FieldRendererData Replacement**:
```cpp
// ❌ BEFORE (architecture-refactoring proposed):
FieldRendererData data = formGPS->prepareRenderData();

// ✅ AFTER (uses Backend from Phase 1):
// Backend already filled by CoreGPS via setters
auto patches = Backend::instance()->field().patches;
auto gps = Backend::instance()->rawGPS();
```

**Validation Criteria**:
- FormGPS God Object broken into separate responsibilities
- CVehicle and CImplement are separate classes
- Dependency cycles eliminated
- Code organization is clean and maintainable

---

### PHASE 2.5: Qt Design Studio + QML Conventions 🎨
**Source**: User requirement + Qt 6.8/Qt 10 best practices

**Actions**:
1. **QML File Structure Reorganization**:
   - Adopt Qt Design Studio conventions for file naming (PascalCase for components)
   - Directory structure: `ui/`, `components/`, `pages/`, `assets/`
   - Proper `qmldir` files for each module
   - CMakeLists.txt: qt_add_qml_module() for each QML module

2. **Qt 6.8/Qt 10 Conventions**:
   - Use `required property` for mandatory properties (Qt 6.2+)
   - Prefer `component` inline components over separate files when appropriate
   - Use Qt Quick Controls 2 (not Controls 1)
   - Binding optimization: use `Binding { when: }` for conditional bindings
   - Use `pragma ComponentBehavior: Bound` for better type safety (Qt 6.8+)

3. **Design System Integration**:
   - Create `Theme.qml` singleton for colors, fonts, spacing
   - Standardize component property naming (lowercase camelCase)
   - Document components for Qt Design Studio metadata
   - Use Qt Quick Designer annotations (`@DesignerSupported`, `@StudioComponent`)

4. **QML Module Structure**:
```qml
# qml/QtAgOpenGPS/CMakeLists.txt
qt_add_qml_module(qtagopengps
    URI QtAgOpenGPS
    VERSION 1.0
    QML_FILES
        MainWindow.qml
        # ... other files
    SOURCES
        # C++ types for this module
)
```

5. **Component Best Practices**:
   - Each custom component in separate file
   - Use Qt Quick Layouts (RowLayout, ColumnLayout) over anchors when possible
   - Proper signal/property naming (onXChanged, not onXChange)
   - Use `Item {}` as root for custom components (not Rectangle unless needed)

**Why Now**:
- C++ architecture already refactored (Phase 2 complete)
- Backend provides clean data for QML
- BEFORE creating new Scene Graph components
- Establishes conventions for Phase 3 (FieldRenderer will be compliant from the start)

**Dependencies**: Phase 2 (FormGPS refactoring)

**Impact**:
- Qt Design Studio can be used for visual development
- QML code compatible Qt 6.8 → Qt 10
- Better maintainability and organization
- Performance: compiled QML modules (qt_add_qml_module)

**Example Transformation**:
```qml
// ❌ BEFORE (old style)
import QtQuick 2.15
import "." as AOG

Rectangle {
    property var formGPS  // setContextProperty
    width: formGPS.latitude
}

// ✅ AFTER (Qt 6.8 conventions)
pragma ComponentBehavior: Bound
import QtQuick
import QtAgOpenGPS

Item {  // Item root instead of Rectangle
    required property FormGPS formGPS  // required property

    implicitWidth: 100
    implicitHeight: 50

    // Backend access via singleton
    readonly property real latitude: Backend.rawGPS.latitude
}
```

**Validation Criteria**:
- Qt Design Studio can open and edit project
- All QML files follow Qt 6.8+ conventions
- QML modules properly registered and accessible
- Theme system implemented and used consistently

---

### PHASE 3: Scene Graph Migration with QSGGeometryNode 🎨
**Source**: rendering-architecture.md + architecture-refactoring.md (Michael's strategy) + User validation

**Decision**: Going **directly to QSGGeometryNode** ✅ **USER VALIDATED** (not QSGRenderNode as intermediate step)

**Actions**:
1. **Create FieldRenderer QQuickItem** (QML-instantiable):
   - Implement updatePaintNode() with Michael's rebuild pattern
   - Read data from Backend singleton (thread-safe)
   - Manual culling preserved from current oglMainPaint
   - Z-ordering via QSGTransformNode

2. **Implement QSGGeometryNode-based rendering**:
   - Convert OpenGL code to QSGGeometry
   - Use QSGFlatColorMaterial for simple geometry
   - Custom QSGMaterialShader for projection matrices (if needed)
   - Automatic batching by material type

3. **Optimize Scene Graph**:
   - Batch similar geometry (100+ draw calls → 3-10)
   - Preserve bounding box culling from qpback
   - Frustum culling for off-screen geometry
   - Buffer caching for static geometry

4. **QRhi benefits** (automatic with QSGGeometryNode):
   - Vulkan backend on Linux/Windows
   - Metal backend on macOS
   - DirectX backend on Windows
   - OpenGL fallback

**Why Last**:
- **CRITICAL**: Michael says "refactor FormGPS FIRST, THEN migrate Scene Graph"
- Clean data (Backend) already available
- FormGPS God Object already broken (Phase 2)
- QML conventions already established (Phase 2.5)
- FieldRenderer can read Backend directly (thread-safe)
- New Qt Design Studio conventions already in place

**Dependencies**:
- Backend (Phase 1) - provides thread-safe data
- CoreGPS refactoring (Phase 2) - clean architecture
- QML conventions (Phase 2.5) - modern structure for FieldRenderer

**Final Flow**:
```cpp
// QML
FieldRenderer {
    anchors.fill: parent
    // No property! Reads Backend directly
}

// C++ - FieldRenderer
QSGNode* updatePaintNode() {
    // Thread-safe: Backend is thread-safe
    auto field = Backend::instance()->field();
    auto gps = Backend::instance()->rawGPS();
    auto vehicle = Backend::instance()->vehicle();

    // Michael's rebuild pattern
    delete oldNode;
    QSGNode *root = buildSceneGraph(field, gps, vehicle);
    return root;
}
```

**Validation Criteria**:
- Performance ≥ qpback branch
- Multi-backend support verified (Vulkan, Metal, DirectX, OpenGL)
- Automatic batching reduces draw calls to 3-10
- Visual output identical to current OpenGL renderer

---

## Summary: How Proposals Complement Each Other

```
┌─────────────────────────────────────────────────────────────┐
│                    INTEGRATION OF 3 PROPOSALS                │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  backend-qgadget          architecture-refactoring          │
│  (Data Organization)      (Code Organization)               │
│         │                           │                        │
│         ├─ Backend Q_GADGET         ├─ FormGPS → CoreGPS    │
│         │  containers               │   (God Object split)  │
│         │                           │                        │
│         ├─ QMetaObject::invoke ────┼─ Break cycles          │
│         │  (Backend→CoreGPS)        │   (QProperty bindings)│
│         │                           │                        │
│         ├─ Singleton pattern ───────┼─ Replace setContext   │
│         │  (instance()+create())    │   (QML_SINGLETON)     │
│         │                           │                        │
│         └────────┬──────────────────┘                        │
│                  │                                           │
│                  ▼                                           │
│         rendering-architecture                              │
│         (Rendering Technology)                              │
│                  │                                           │
│                  ├─ QSGGeometryNode (direct)                │
│                  ├─ Uses Backend data                       │
│                  └─ Michael's rebuild pattern               │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Complementarity Table

| Aspect | backend-qgadget | architecture-refactoring | rendering-architecture |
|--------|----------------|-------------------------|----------------------|
| **Data organization** | ✅ PRIMARY | ⚠️ Partial (FieldRendererData) | ❌ N/A |
| **Code organization** | ⚠️ Partial (Backend only) | ✅ PRIMARY (FormGPS split) | ❌ N/A |
| **Rendering tech** | ❌ N/A | ⚠️ Partial (extract logic) | ✅ PRIMARY (Scene Graph) |
| **Break cycles** | ✅ Backend→CoreGPS | ✅ CoreGPS→CVehicle | ❌ N/A |
| **Singleton pattern** | ✅ PRIMARY (docs) | ✅ Uses pattern | ❌ N/A |
| **QML integration** | ✅ PRIMARY | ✅ Uses Backend | ✅ Reads Backend |

---

## Critical Decisions

### DECISION #1: FieldRendererData vs Backend Q_GADGET
**Choice**: **Backend Q_GADGET REPLACES FieldRendererData**

**Rationale**:
- Backend covers 6 domains (GPS, vehicle, field, guidance, blockage, hardware)
- FieldRendererData only covered patches + matrices
- Backend already QML-accessible (Q_GADGET)
- Less code duplication

### DECISION #2: Implementation Order
**Choice**: **Backend BEFORE FormGPS refactoring BEFORE Scene Graph**

**Rationale**:
- Backend provides structure to store UI data
- FormGPS refactoring uses Backend to separate UI data
- Scene Graph reads Backend (clean data, thread-safe)
- Avoids doing the same work 3 times

### DECISION #3: CVehicle / CImplement Separation
**Choice**: **Do during Phase 2 (FormGPS refactoring)** ✅ **USER VALIDATED**

**Rationale**:
- Logically related to FormGPS God Object split
- Breaks another God Object (CVehicle mixes 2 responsibilities)
- Backend.vehicle can point to clean CVehicle
- CImplement can be added to Backend if needed

### DECISION #4: Qt Design Studio Timing
**Choice**: **Phase 2.5 (between FormGPS refactoring and Scene Graph)** ✅ **USER VALIDATED**

**Rationale**:
- C++ architecture is clean (Backend, CoreGPS, CVehicle, CImplement separated)
- QML conventions established BEFORE creating FieldRenderer
- New Scene Graph architecture follows conventions from the start
- No QML refactoring needed after the fact
- Qt Design Studio can be used immediately
- Compatible Qt 6.8 → Qt 10 (future-proof)

### DECISION #5: Scene Graph Target
**Choice**: **QSGGeometryNode directly** ✅ **USER VALIDATED** (skip QSGRenderNode intermediate step)

**Rationale**:
- Better performance through automatic batching
- Multi-backend support (Vulkan, Metal, DirectX, OpenGL)
- More maintainable in long term
- Team confident in direct migration

---

## Redundancies to Eliminate

### ❌ Do NOT Create FieldRendererData
**Reason**: Backend Q_GADGET containers do the same job

**Replacement**:
```cpp
// ❌ BEFORE (architecture-refactoring proposed)
FieldRendererData data = formGPS->prepareRenderData();

// ✅ AFTER (uses Backend)
// Backend already filled by CoreGPS
auto field = Backend::instance()->field();
auto gps = Backend::instance()->rawGPS();
```

### ❌ Do NOT Duplicate Singleton Pattern Docs
**Reason**: backend-qgadget.md already has complete documentation

**Action**: architecture-refactoring.md Recommendation #5 → Reference backend-qgadget.md Section 2.3

### ❌ Do NOT Duplicate QMetaObject::invokeMethod Docs
**Reason**: backend-qgadget.md already has "Business Logic Integration Pattern" section

**Action**: architecture-refactoring.md Recommendation #2 → Partial (QProperty bindings only)

---

## User Validations (2024-12-14)

✅ **Backend Q_GADGET containers (6 proposed)**: **VALIDATED**
- RawGPSData, BlockageData, VehicleState, FieldData, GuidanceData, HardwareStatus

✅ **CVehicle / CImplement separation**: **NOW** (Phase 2)
- Don't wait, do during FormGPS refactoring

✅ **Scene Graph target**: **QSGGeometryNode** (Phase 3)
- Going directly to QSGGeometryNode (not QSGRenderNode first)

✅ **Qt Design Studio conventions**: **REQUIRED** (New Phase 2.5)
- Qt Design Studio programming conventions
- Qt 6.8 file structure and Qt 10 compatibility
- Modern QML structure BEFORE Scene Graph implementation

---

## Next Steps

1. ✅ Validations received (Backend, CVehicle/CImplement, Scene Graph, Qt Design Studio)
2. **Team review and approval of this integration plan**
3. Create GitHub issues for each phase
4. Begin Phase 0 (quick wins)
5. Implement Backend architecture (Phase 1)

---

## Related Documentation

- [backend-qgadget-architecture.md](backend-qgadget-architecture.md) - Complete Backend proposal with singleton pattern
- [rendering-architecture.md](rendering-architecture.md) - Scene Graph technical analysis
- [architecture-refactoring.md](architecture-refactoring.md) - FormGPS God Object refactoring
- [Qt 6.8 Property Migration Guide](../architecture/migration-qt68-properties.md)
- [QML Integration Architecture](../architecture/qml-integration.md)
- [System Architecture Overview](../architecture/system-architecture.md)

---

**Implementation Tracking**: TBD (create GitHub issues after team approval)
