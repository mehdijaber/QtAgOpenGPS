# QtAgOpenGPS Rendering Architecture: Analysis and Proposal

## Document Purpose

This document analyzes the current rendering architecture in QtAgOpenGPS, evaluates recent optimizations on the `qpback` branch, and proposes a potential migration path to Qt's native Scene Graph for improved cross-platform performance, particularly on Android.

---

## Executive Summary

| Aspect | Current (qpback) | Proposed (QSGGeometryNode) |
|--------|------------------|---------------------------|
| Draw calls per frame | 50-200 | 1-10 |
| Buffer management | Manual (cached) | Automatic (Qt managed) |
| Batching | None | Automatic by material |
| Backend | OpenGL ES only | Vulkan/Metal/D3D/OpenGL via QRhi |
| Android performance | Improved | Optimal |

---

## Part 1: Current Architecture Analysis

### 1.1 Rendering Pipeline Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                    CURRENT ARCHITECTURE                              │
│                (QQuickFramebufferObject + OpenGL)                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   QML Layer                        C++ Rendering Layer               │
│   ┌──────────────────┐             ┌────────────────────────────┐   │
│   │ AOGRendererInSG  │             │  FormGPS::oglMain_Paint()  │   │
│   │ (FBO wrapper)    │──callback──▶│                            │   │
│   └──────────────────┘             │  Manual OpenGL calls:      │   │
│           │                        │  • glBindBuffer()          │   │
│           ▼                        │  • glDrawArrays()          │   │
│   ┌──────────────────┐             │  • glFlush()               │   │
│   │  Framebuffer     │             └────────────────────────────┘   │
│   │  Object (FBO)    │                                              │
│   └──────────────────┘                                              │
│           │                                                          │
│           ▼                                                          │
│   ┌──────────────────┐                                              │
│   │    OpenGL ES     │  ← Single backend, no Vulkan/Metal           │
│   └──────────────────┘                                              │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.2 Current Patch Rendering Flow

```cpp
// formgps_opengl.cpp - Current implementation
void FormGPS::oglMain_Paint() {
    // For each section
    for (int j = 0; j < triStrip.count(); j++) {
        // For each patch in section
        for (auto &triList : triStrip[j].patchList) {

            // Manual frustum culling (per-vertex)
            bool isDraw = false;
            for (int i = 1; i < count2; i += 3) {
                if (frustum[0] * (*triList)[i].x() + ... <= 0) continue;
                // ... 5 more plane tests
                isDraw = true;
                break;
            }

            if (isDraw) {
                QOpenGLBuffer triBuffer;
                triBuffer.create();
                triBuffer.allocate(triList->data(), ...);
                glDrawArraysColor(...);  // ← Individual draw call
                triBuffer.destroy();
            }
        }
    }
}
```

### 1.3 Performance Bottlenecks Identified

| Bottleneck | Impact | Description |
|------------|--------|-------------|
| Buffer create/destroy per frame | HIGH | GPU memory allocation is expensive |
| Individual draw calls | HIGH | 100+ glDrawArrays() calls per frame |
| No batching | HIGH | Each patch = separate draw call |
| Per-vertex frustum culling | MEDIUM | CPU overhead for visibility tests |
| FBO overhead | MEDIUM | Extra render target copy |
| OpenGL ES only | MEDIUM | No Vulkan benefits on Android |

---

## Part 2: Evaluation of qpback Optimizations

### 2.1 What Was Implemented

The `qpback` branch introduced several valuable optimizations:

#### 2.1.1 GPU Buffer Caching

```cpp
// BEFORE (new_dev) - Buffer created/destroyed every frame
QOpenGLBuffer triBuffer;
triBuffer.create();           // GPU allocation
triBuffer.allocate(...);      // GPU upload
glDrawArraysColor(...);
triBuffer.destroy();          // GPU deallocation

// AFTER (qpback) - Buffer cached in VRAM
if (!patchesBuffer[j][k].isCreated()) {
    patchesBuffer[j][k].create();
    patchesBuffer[j][k].allocate(...);  // Only once
}
patchesBuffer[j][k].bind();
glDrawArraysColor(...);                  // Reuse cached buffer
```

**Impact**: Eliminates per-frame GPU memory allocation overhead.

#### 2.1.2 Bounding Box Precalculation

```cpp
// Bounding boxes computed during field loading, not per-frame
// Enables faster frustum culling at patch level vs vertex level
```

**Impact**: Reduces CPU overhead for visibility determination.

#### 2.1.3 Threaded Back Buffer with QPainter

```cpp
// Back buffer rendering moved to separate thread
// Uses QPainter instead of glReadPixels (which caused Android stalls)
```

**Impact**: Prevents GPU stalls on Android, improves frame consistency.

#### 2.1.4 Frustum-Based Patch Culling

```cpp
// Early rejection of patches outside view frustum
// Uses precalculated bounding boxes for faster tests
```

**Impact**: Reduces number of patches sent to GPU.

### 2.2 What Was Attempted But Abandoned

#### glDrawMultiElementsIndirect

```cpp
// Attempted: Single draw call for all patches
glDrawMultiElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
                            indirectBuffer, drawCount, stride);
```

**Why abandoned**: Not supported on OpenGL ES (Android). Requires OpenGL 4.3+ or Vulkan.

#### glReadPixels for Back Buffer

**Why abandoned**: Caused GPU stalls on Android devices. The GPU-to-CPU data transfer blocked the rendering pipeline.

### 2.3 Remaining Limitations

Despite excellent optimizations, fundamental limitations remain:

| Issue | Status | Root Cause |
|-------|--------|------------|
| 50-200 draw calls per frame | Not solved | OpenGL cannot batch automatically |
| State changes between draws | Not solved | Each patch = bind + draw |
| OpenGL ES only | Not solved | No path to Vulkan/Metal |
| FBO separate from scene graph | Not solved | Architectural limitation |

```
CURRENT FRAME EXECUTION (qpback)
════════════════════════════════

glBindBuffer(patch_1)  → glDrawArrays()  ← Draw call #1
glBindBuffer(patch_2)  → glDrawArrays()  ← Draw call #2
glBindBuffer(patch_3)  → glDrawArrays()  ← Draw call #3
...
glBindBuffer(patch_99) → glDrawArrays()  ← Draw call #99
glBindBuffer(patch_100)→ glDrawArrays()  ← Draw call #100

Total: 100 draw calls, 100 state changes
GPU utilization: Suboptimal (command processing overhead)
```

---

## Part 3: Proposed Architecture - Qt Scene Graph

### 3.1 Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                    PROPOSED ARCHITECTURE                             │
│                    (Qt Quick Scene Graph)                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   QML Layer                        C++ Scene Graph Nodes             │
│   ┌──────────────────┐             ┌────────────────────────────┐   │
│   │ FieldRenderer    │             │  updatePaintNode()         │   │
│   │ (QQuickItem)     │─────────────▶                            │   │
│   │                  │             │  Returns node tree:        │   │
│   └──────────────────┘             │  ├─ QSGTransformNode       │   │
│                                    │  │   ├─ QSGGeometryNode    │   │
│   No FBO needed!                   │  │   │   (all green patches)│  │
│   Native scene graph               │  │   ├─ QSGGeometryNode    │   │
│   integration                      │  │   │   (all red patches) │   │
│                                    │  │   └─ QSGGeometryNode    │   │
│                                    │  │       (boundaries)      │   │
│                                    └────────────────────────────┘   │
│                                              │                       │
│                                              ▼                       │
│                                    ┌────────────────────────────┐   │
│                                    │  Qt Scene Graph Renderer   │   │
│                                    │                            │   │
│                                    │  Automatic:                │   │
│                                    │  • Material-based sorting  │   │
│                                    │  • Draw call batching      │   │
│                                    │  • Frustum culling         │   │
│                                    │  • Buffer management       │   │
│                                    └────────────────────────────┘   │
│                                              │                       │
│                                              ▼                       │
│                                    ┌────────────────────────────┐   │
│                                    │          QRhi              │   │
│                                    │   (Abstraction Layer)      │   │
│                                    └────────────────────────────┘   │
│                                              │                       │
│              ┌───────────────┬───────────────┼───────────────┐      │
│              ▼               ▼               ▼               ▼      │
│        ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐   │
│        │  Vulkan  │   │  Metal   │   │  D3D11   │   │  OpenGL  │   │
│        │ (Android)│   │  (iOS)   │   │ (Windows)│   │ (Legacy) │   │
│        └──────────┘   └──────────┘   └──────────┘   └──────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 Key Concept: Batching by Material

The Qt Scene Graph automatically batches draw calls for nodes with the same material (color/shader):

```
PROPOSED FRAME EXECUTION (QSGGeometryNode)
══════════════════════════════════════════

Qt Scene Graph Renderer:
  1. Collect all QSGGeometryNodes
  2. Sort by material (color)
  3. Merge compatible geometries into mega-buffers
  4. Issue minimal draw calls

glBindBuffer(all_green_patches) → glDrawArrays()  ← Draw call #1
glBindBuffer(all_red_patches)   → glDrawArrays()  ← Draw call #2
glBindBuffer(boundaries)        → glDrawArrays()  ← Draw call #3

Total: 3-5 draw calls (vs 100+)
GPU utilization: Optimal
```

### 3.3 Implementation Example

#### 3.3.1 Node Classes

```cpp
// patchgroupnode.h
#pragma once
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>

class PatchGroupNode : public QSGGeometryNode {
public:
    PatchGroupNode(const QColor &color) {
        // Create geometry for triangle strips
        auto *geometry = new QSGGeometry(
            QSGGeometry::defaultAttributes_Point2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        setGeometry(geometry);
        setFlag(QSGNode::OwnsGeometry);

        // Create flat color material
        auto *material = new QSGFlatColorMaterial();
        material->setColor(color);
        setMaterial(material);
        setFlag(QSGNode::OwnsMaterial);
    }

    void updateVertices(const QVector<QPointF> &vertices) {
        auto *geo = geometry();

        if (geo->vertexCount() != vertices.size()) {
            geo->allocate(vertices.size());
        }

        auto *v = geo->vertexDataAsPoint2D();
        for (int i = 0; i < vertices.size(); ++i) {
            v[i].set(vertices[i].x(), vertices[i].y());
        }

        markDirty(QSGNode::DirtyGeometry);
    }
};

class FieldRootNode : public QSGTransformNode {
public:
    QHash<QRgb, PatchGroupNode*> patchGroups;

    PatchGroupNode* getOrCreateGroup(const QColor &color) {
        QRgb key = color.rgba();
        if (!patchGroups.contains(key)) {
            auto *node = new PatchGroupNode(color);
            appendChildNode(node);
            patchGroups[key] = node;
        }
        return patchGroups[key];
    }

    void removeUnusedGroups(const QSet<QRgb> &activeColors) {
        for (auto it = patchGroups.begin(); it != patchGroups.end(); ) {
            if (!activeColors.contains(it.key())) {
                removeChildNode(it.value());
                delete it.value();
                it = patchGroups.erase(it);
            } else {
                ++it;
            }
        }
    }
};
```

#### 3.3.2 QQuickItem Implementation

```cpp
// fieldrenderer.h
#pragma once
#include <QQuickItem>

class FormGPS;

class FieldRenderer : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(FormGPS* formGPS READ formGPS WRITE setFormGPS NOTIFY formGPSChanged)

public:
    explicit FieldRenderer(QQuickItem *parent = nullptr);

    FormGPS* formGPS() const { return m_formGPS; }
    void setFormGPS(FormGPS *gps);

signals:
    void formGPSChanged();

protected:
    QSGNode* updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private:
    FormGPS *m_formGPS = nullptr;

    void updatePatches(FieldRootNode *root);
    void updateBoundaries(FieldRootNode *root);
    void updateCamera(FieldRootNode *root);
};

// fieldrenderer.cpp
#include "fieldrenderer.h"
#include "formgps.h"
#include "patchgroupnode.h"

FieldRenderer::FieldRenderer(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void FieldRenderer::setFormGPS(FormGPS *gps) {
    if (m_formGPS == gps) return;

    if (m_formGPS) {
        disconnect(m_formGPS, nullptr, this, nullptr);
    }

    m_formGPS = gps;

    if (m_formGPS) {
        // Trigger repaint when data changes
        connect(m_formGPS, &FormGPS::patchesChanged,
                this, &QQuickItem::update);
        connect(m_formGPS, &FormGPS::positionChanged,
                this, &QQuickItem::update);
    }

    emit formGPSChanged();
    update();
}

QSGNode* FieldRenderer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    FieldRootNode *root = static_cast<FieldRootNode*>(oldNode);

    if (!root) {
        root = new FieldRootNode();
    }

    if (!m_formGPS || !m_formGPS->isGPSPositionInitialized()) {
        return root;
    }

    // Update camera transform
    updateCamera(root);

    // Update geometry if patches changed
    if (m_formGPS->patchesDirty()) {
        updatePatches(root);
    }

    // Update boundaries if needed
    if (m_formGPS->boundariesDirty()) {
        updateBoundaries(root);
    }

    return root;
}

void FieldRenderer::updatePatches(FieldRootNode *root) {
    // Group all patches by color
    QHash<QRgb, QVector<QPointF>> patchesByColor;
    QSet<QRgb> activeColors;

    for (int j = 0; j < m_formGPS->triStrip.count(); ++j) {
        QColor sectionColor = m_formGPS->tool.secColors[j];
        sectionColor.setAlpha(150);
        QRgb colorKey = sectionColor.rgba();
        activeColors.insert(colorKey);

        for (const auto &triList : m_formGPS->triStrip[j].patchList) {
            // Skip first vertex (color info in old format)
            for (int i = 1; i < triList->size(); ++i) {
                const QVector3D &v = (*triList)[i];
                patchesByColor[colorKey].append(QPointF(v.x(), v.y()));
            }
        }
    }

    // Update scene graph nodes
    for (auto it = patchesByColor.begin(); it != patchesByColor.end(); ++it) {
        QColor color = QColor::fromRgba(it.key());
        PatchGroupNode *node = root->getOrCreateGroup(color);
        node->updateVertices(it.value());
    }

    // Remove nodes for colors no longer in use
    root->removeUnusedGroups(activeColors);
}

void FieldRenderer::updateCamera(FieldRootNode *root) {
    QMatrix4x4 transform;

    // Apply projection
    transform.perspective(
        glm::toDegrees(m_formGPS->fovy),
        width() / height(),
        1.0f,
        m_formGPS->camDistanceFactor * m_formGPS->camera.camSetDistance
    );

    // Apply camera position
    m_formGPS->camera.SetWorldCam(
        transform,
        CVehicle::instance()->pivotAxlePos.easting,
        CVehicle::instance()->pivotAxlePos.northing,
        m_formGPS->camera.camHeading
    );

    root->setMatrix(transform);
}
```

#### 3.3.3 QML Usage

```qml
// MainWindow.qml
import QtQuick
import QtAgOpenGPS  // Your module

Item {
    id: root

    // New scene graph based renderer
    FieldRenderer {
        id: fieldView
        anchors.fill: parent
        formGPS: aog  // Direct binding to FormGPS instance
    }

    // UI overlays remain unchanged
    LightBar { ... }
    SpeedDisplay { ... }
    // etc.
}
```

### 3.4 Migration Strategy

#### Phase 1: Parallel Implementation

Create new `FieldRenderer` component alongside existing `AOGRendererInSG`:

```qml
Item {
    // Toggle between implementations for testing
    property bool useSceneGraph: true

    AOGRendererInSG {
        visible: !useSceneGraph
        // ... existing implementation
    }

    FieldRenderer {
        visible: useSceneGraph
        formGPS: aog
    }
}
```

#### Phase 2: Component Migration

Migrate components one by one:
1. Field patches (highest impact)
2. Boundaries and turn lines
3. AB lines and tracks
4. Vehicle rendering

#### Phase 3: Back Buffer Solution

For section lookahead, use `QSGRenderNode` for direct command injection:

```cpp
class LookaheadRenderNode : public QSGRenderNode {
public:
    void render(const RenderState *state) override {
        // Direct OpenGL/Vulkan commands for lookahead buffer
        // This runs within the scene graph render pass
    }
};
```

#### Phase 4: Cleanup and Optimization

- Remove old FBO-based renderer
- Fine-tune batching strategies
- Add LOD support at node level

---

## Part 4: Comparison Summary

### 4.1 Performance Comparison

| Metric | new_dev | qpback | QSGGeometryNode |
|--------|---------|--------|-----------------|
| Draw calls / frame | 100-200 | 100-200 | 3-10 |
| Buffer allocations / frame | 100-200 | 0 (cached) | 0 (Qt managed) |
| Frustum culling | Per-vertex | Per-patch (bbox) | Per-node (auto) |
| Batching | None | None | Automatic |
| State changes | 100-200 | 100-200 | 3-10 |
| Android backend | OpenGL ES | OpenGL ES | Vulkan (auto) |

### 4.2 Code Complexity

| Aspect | Current (OpenGL) | Proposed (Scene Graph) |
|--------|------------------|------------------------|
| Rendering code lines | ~500 | ~150 |
| Buffer management | Manual | Automatic |
| Threading concerns | Manual sync | Qt managed |
| Platform support | Manual per-platform | Automatic via QRhi |

### 4.3 Compatibility

| Platform | Current | Proposed |
|----------|---------|----------|
| Android | OpenGL ES 3.0 | Vulkan 1.0 (auto fallback to ES) |
| iOS | OpenGL ES (deprecated) | Metal |
| Windows | OpenGL | D3D11/D3D12 or Vulkan |
| Linux | OpenGL | Vulkan or OpenGL |
| macOS | OpenGL (deprecated) | Metal |

---

## Part 5: Risks and Considerations

### 5.1 Migration Risks

| Risk | Mitigation |
|------|------------|
| Scene graph learning curve | Extensive Qt documentation available |
| Behavior differences | Parallel implementation for A/B testing |
| Back buffer complexity | QSGRenderNode provides escape hatch |
| Performance regression | Benchmark both approaches before commit |

### 5.2 What We Keep from qpback

The optimizations on `qpback` are not wasted. Several concepts transfer directly:

| qpback Optimization | Scene Graph Equivalent |
|--------------------|------------------------|
| Buffer caching | Automatic (QSGGeometry persists) |
| Bounding box precalc | Use for dirty-checking, node updates |
| Frustum culling logic | Can still apply for early node updates |
| QPainter back buffer | May still be needed for lookahead |

---

## Part 6: Recommendation

### Short Term (Recommended)

1. **Merge qpback optimizations** into `new_dev`
   - Buffer caching is valuable
   - Bounding box precalculation helps
   - QPainter back buffer is more stable

2. **Begin QSGGeometryNode prototype** in parallel branch
   - Start with patches only
   - Benchmark against qpback

### Medium Term

3. **Migrate to Scene Graph** if benchmarks are favorable
   - Expected 2-5x performance improvement on Android
   - Automatic Vulkan support
   - Reduced maintenance burden

### Long Term

4. **Evaluate Qt Quick 3D** for future features
   - If true 3D visualization becomes needed
   - Terrain rendering, 3D vehicle models, etc.

---

## Appendix A: Qt Scene Graph Resources

- [Qt Scene Graph Documentation](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html)
- [Custom Geometry Example](https://doc.qt.io/qt-6/qtquick-scenegraph-customgeometry-example.html)
- [QSGGeometryNode Class](https://doc.qt.io/qt-6/qsggeometrynode.html)
- [QRhi Overview](https://doc.qt.io/qt-6/qrhi.html)

---

## Appendix B: Benchmark Methodology

To compare approaches, measure the following on target devices:

```cpp
// Add to rendering code
QElapsedTimer frameTimer;
frameTimer.start();

// ... rendering code ...

qint64 frameTime = frameTimer.nsecsElapsed();
qDebug() << "Frame time:" << frameTime / 1000000.0 << "ms";
```

Key metrics:
- Frame time (target: <16.6ms for 60fps)
- Draw call count (use RenderDoc or similar)
- GPU memory usage
- CPU usage during render

---

## Appendix C: Complete Qt Rendering Options Comparison

This appendix provides a comprehensive comparison of all Qt rendering options considered for QtAgOpenGPS.

### C.1 Options Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    QT RENDERING OPTIONS FOR QtAgOpenGPS                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐              │
│  │   Qt Quick      │  │   Qt Quick      │  │   Qt Quick      │              │
│  │   Canvas        │  │   Shapes        │  │   3D            │              │
│  │   (JavaScript)  │  │   (QML)         │  │   (QML)         │              │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘              │
│           │                    │                    │                        │
│           ▼                    ▼                    ▼                        │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐              │
│  │  Texture Upload │  │ CPU Triangula-  │  │ Full 3D Engine  │              │
│  │  Every Frame    │  │ tion + Scene    │  │ PBR, Lighting   │              │
│  │  (SLOW!)        │  │ Graph           │  │ (OVERKILL!)     │              │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘              │
│                                                                              │
│  ┌─────────────────┐  ┌─────────────────┐                                   │
│  │  QSGGeometry    │  │  QSGRender      │                                   │
│  │  Node           │  │  Node           │                                   │
│  │  (C++)          │  │  (C++)          │                                   │
│  └────────┬────────┘  └────────┬────────┘                                   │
│           │                    │                                             │
│           ▼                    ▼                                             │
│  ┌─────────────────┐  ┌─────────────────┐                                   │
│  │ Scene Graph     │  │ Direct OpenGL   │                                   │
│  │ + Custom Mat.   │  │ Injection       │                                   │
│  │ (OPTIMAL)       │  │ (RECOMMENDED)   │                                   │
│  └─────────────────┘  └─────────────────┘                                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### C.2 Qt Quick Canvas - NOT SUITABLE

```qml
Canvas {
    width: 800; height: 600
    onPaint: {
        var ctx = getContext("2d")
        for (var patch of patches) {
            ctx.beginPath()
            ctx.moveTo(patch[0].x, patch[0].y)
            // ... draw patch
            ctx.fill()
        }
    }
}
```

| Aspect | Assessment |
|--------|------------|
| **How it works** | JavaScript draws to texture, uploaded to GPU each frame |
| **Custom projection matrix** | Not supported |
| **Performance** | Poor |
| **Android** | Bad - texture upload overhead |
| **Qt Documentation Warning** | "large canvases, frequent updates, and animation should be avoided... each update will lead to a texture upload" |
| **Verdict** | **NOT SUITABLE** for dynamic rendering |

### C.3 Qt Quick Shapes - NOT SUITABLE

```qml
Shape {
    ShapePath {
        strokeWidth: 2
        strokeColor: "green"
        fillColor: "lightgreen"
        PathPolyline { path: patchVertices }
    }
}
```

| Aspect | Assessment |
|--------|------------|
| **How it works** | QML declarative paths, CPU triangulation, scene graph rendering |
| **Custom projection matrix** | Not supported - uses Qt's coordinate system |
| **Performance** | Medium - CPU triangulation on every geometry change |
| **Android** | Medium |
| **Batching** | Partial (within same Shape item only) |
| **Verdict** | **NOT SUITABLE** - no custom matrices, CPU overhead |

### C.4 Qt Quick 3D - OVERKILL

```qml
import QtQuick3D

View3D {
    PerspectiveCamera {
        position: Qt.vector3d(0, 200, 0)
    }
    DirectionalLight { }

    Repeater3D {
        model: patchesModel
        Model {
            geometry: CustomGeometry { }
            materials: PrincipledMaterial {
                baseColor: patchColor
                lighting: PrincipledMaterial.NoLighting
            }
        }
    }
}
```

| Aspect | Assessment |
|--------|------------|
| **How it works** | Full 3D scene graph with PBR materials, lighting, shadows |
| **Custom projection matrix** | Via Camera API - different from our current system |
| **Performance** | Good but heavy footprint |
| **Android** | Good but +20-50 MB RAM overhead |
| **Verdict** | **OVERKILL** - we would disable 90% of features |

**What Qt Quick 3D provides (unused by QtAgOpenGPS)**:
- PBR Materials (metalness, roughness)
- Dynamic lighting and shadows
- 3D model loading (.gltf, .obj)
- Environment maps and skyboxes
- Screen Space Ambient Occlusion
- Reflections and refractions

**What QtAgOpenGPS actually needs**:
- Flat colored triangles
- Custom projection/modelview matrices
- No lighting, no shadows
- Depth testing disabled

### C.5 QSGGeometryNode + Custom Material - OPTIMAL (Complex)

```cpp
class FieldMaterial : public QSGMaterial {
public:
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_modelViewMatrix;
    QColor m_color;

    QSGMaterialShader *createShader() const override {
        return new FieldMaterialShader();
    }
};

// In updatePaintNode()
QSGGeometryNode *node = new QSGGeometryNode();
node->setGeometry(patchGeometry);
node->setMaterial(new FieldMaterial(proj, mv, color));
```

| Aspect | Assessment |
|--------|------------|
| **How it works** | Native scene graph with custom material and shaders |
| **Custom projection matrix** | Yes - via custom QSGMaterial uniforms |
| **Performance** | Optimal - automatic batching by material |
| **Android** | Excellent - Vulkan via QRhi |
| **Migration effort** | High - requires .qsb shader compilation |
| **Verdict** | **OPTIMAL** but requires custom shaders |

**Challenge**: Custom projection matrices require:
1. Custom `QSGMaterial` subclass
2. Custom `QSGMaterialShader` subclass
3. GLSL shaders compiled to .qsb format via Qt shader tools

### C.6 QSGRenderNode - RECOMMENDED FIRST STEP

```cpp
class FieldRenderNode : public QSGRenderNode {
public:
    void render(const RenderState *state) override {
        QOpenGLFunctions *gl = QOpenGLContext::currentContext()->functions();

        // Existing OpenGL code works directly
        gl->glDisable(GL_DEPTH_TEST);
        gl->glEnable(GL_BLEND);

        // Custom matrices - no problem
        QMatrix4x4 projection, modelview;
        projection.perspective(fovy, aspect, near, far);
        camera.SetWorldCam(modelview, easting, northing, heading);

        // Draw patches with existing code
        for (auto &strip : triStrip) {
            for (auto &patch : strip.patchList) {
                // ... existing drawing code
            }
        }
    }

    StateFlags changedStates() const override {
        return BlendState | DepthState;
    }
};
```

| Aspect | Assessment |
|--------|------------|
| **How it works** | Injects raw OpenGL commands into scene graph render pass |
| **Custom projection matrix** | Yes - full control with existing code |
| **Performance** | Very good - no FBO overhead |
| **Android** | Good - still OpenGL ES |
| **Migration effort** | Low - reuse existing oglMain_Paint() code |
| **Automatic batching** | No (manual) |
| **Verdict** | **RECOMMENDED** as first migration step |

### C.7 Complete Comparison Matrix

| Option | Custom MVP | Dynamic Geometry | Performance | Android | Effort | Suitable |
|--------|-----------|------------------|-------------|---------|--------|----------|
| Qt Quick Canvas | No | Poor | Poor | Bad | Low | No |
| Qt Quick Shapes | No | Medium | Medium | Medium | Low | No |
| Qt Quick 3D | Via API | Good | Good | Heavy | Medium | No (overkill) |
| QSGGeometryNode | Yes (shaders) | Excellent | Optimal | Excellent | High | Yes |
| **QSGRenderNode** | **Yes (direct)** | **Very Good** | **Very Good** | **Good** | **Low** | **Yes** |

### C.8 Suitability Matrix for QtAgOpenGPS Requirements

```
                    ┌─────────────────────────────────────────────────────────┐
                    │           SUITABILITY FOR QtAgOpenGPS                   │
                    ├─────────────────────────────────────────────────────────┤
                    │                                                          │
Feature Needed      │ Canvas  Shapes  Qt3D   QSGGeoNode  QSGRenderNode        │
────────────────────┼──────────────────────────────────────────────────────────
Custom MVP matrix   │   No      No     Partial   Yes         Yes              │
Dynamic triangles   │   No      Partial  Yes     Yes         Yes              │
Flat colors only    │   Yes     Yes     Yes      Yes         Yes              │
No depth test       │   Yes     Yes     Partial  Yes         Yes              │
Low overhead        │   No      Partial  No      Yes         Yes              │
Existing code reuse │   No      No      No       Partial     Yes              │
Auto batching       │   No      Partial  Partial Yes         No               │
Vulkan support      │   Partial Yes     Yes      Yes         No               │
────────────────────┼──────────────────────────────────────────────────────────
SCORE               │  2/8    3/8     4/8      7/8         6/8               │
                    │                                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### C.9 Recommended Migration Path

```
┌─────────────────────────────────────────────────────────────────────┐
│                    RECOMMENDED MIGRATION PATH                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  PHASE 1: QSGRenderNode (Quick Win)                                 │
│  ──────────────────────────────────                                 │
│  • Move oglMain_Paint() logic to QSGRenderNode::render()            │
│  • Keep existing OpenGL code and custom matrices                    │
│  • Eliminate FBO overhead                                           │
│  • Validate approach, benchmark performance                         │
│                                                                      │
│  DECISION POINT: Is Phase 1 performance sufficient?                 │
│  ─────────────────────────────────────────────────                  │
│  If YES → Maintain QSGRenderNode approach                           │
│  If NO  → Proceed to Phase 2                                        │
│                                                                      │
│  PHASE 2: QSGGeometryNode + Custom Material (Full Optimization)     │
│  ──────────────────────────────────────────────────────────────     │
│  • Implement custom QSGMaterial with projection uniforms            │
│  • Write and compile shaders to .qsb format                         │
│  • Benefit from automatic draw call batching                        │
│  • Get automatic Vulkan support on Android                          │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### C.10 References

- [Qt Quick Canvas](https://doc.qt.io/qt-6/qml-qtquick-canvas.html)
- [Qt Quick Shapes](https://doc.qt.io/qt-6/qml-qtquick-shapes-shape.html)
- [Qt Quick 3D](https://doc.qt.io/qt-6/qtquick3d-index.html)
- [QSGGeometryNode](https://doc.qt.io/qt-6/qsggeometrynode.html)
- [QSGRenderNode](https://doc.qt.io/qt-6/qsgrendernode.html)
- [QSGMaterial](https://doc.qt.io/qt-6/qsgmaterial.html)
- [Qt Shader Tools](https://doc.qt.io/qt-6/qtshadertools-index.html)

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-11-29 | - | Initial analysis and proposal |
| 1.1 | 2025-12-01 | - | Added Appendix C: Complete Qt rendering options comparison |

---

*This document is intended for technical discussion among QtAgOpenGPS developers.*
