# Qt SceneGraph Architecture and OpenGL Rendering

Deep dive into QtAgOpenGPS rendering architecture, OpenGL implementation, and potential migration to Qt SceneGraph.

## Overview

QtAgOpenGPS currently uses a custom OpenGL rendering system with a "painter's algorithm" approach. This document explores the current implementation and discusses transitioning to Qt's SceneGraph for improved performance and maintainability.

## Current Rendering Architecture

### Painter's Algorithm

The current implementation renders display elements sequentially at z-depth 0, despite working with 3D coordinates:

**Rendering Order:**
1. Field background and grid
2. Coverage patches (field coverage tracking)
3. Boundaries and headlands
4. Guidance lines (AB lines, curves)
5. Implement visualization
6. Tractor/vehicle
7. Flags and markers

This order ensures proper visual layering without true 3D depth testing.

### OpenGL Implementation

**Current rendering flow:**
```
QML (Qt Quick Scene) → AOGRenderer → OpenGL ES 2.0 calls → GPU
```

**Key components:**
- [aogrenderer.h](../../aogrenderer.h) / [aogrenderer.cpp](../../aogrenderer.cpp) - QML-instantiated OpenGL renderer
- [formgps_opengl.cpp](../../formgps_opengl.cpp) - OpenGL drawing logic
- [glutils.cpp](../../glutils.cpp) - OpenGL helper functions
- [shaders/](../../shaders/) - GLSL vertex and fragment shaders

### Rendering Frequency

- **OpenGL rendering**: 30 Hz (dedicated render thread via Qt Scene Graph)
- **GPS updates**: 10 Hz (position data)
- **QML updates**: Variable (UI interactions)

The rendering thread is independent of GPS data updates, ensuring smooth frame rates.

### Coordinate System

**World coordinates (3D):**
- X: East-West (meters)
- Y: Altitude/elevation (meters)
- Z: North-South (meters)

**Rendering coordinates (2D projection):**
- Flattened to z=0 for painter's algorithm
- Ortho projection transforms world to screen coordinates
- Camera position and zoom control viewing area

## Qt SceneGraph Overview

Qt SceneGraph is Qt Quick's hardware-accelerated rendering system built on OpenGL/Vulkan/Metal.

### Benefits of SceneGraph

**Performance:**
- Optimized scene graph traversal
- Automatic batching of draw calls
- Hardware-accelerated rendering
- GPU-resident geometry (VBOs)

**Maintainability:**
- Higher-level API than raw OpenGL
- Cross-platform abstraction (OpenGL/Vulkan/Metal/Direct3D)
- Better Qt integration
- Automatic resource management

**Flexibility:**
- Node-based hierarchy
- Easy to add/remove/modify elements
- Built-in transformations
- Support for effects and shaders

### SceneGraph Node Types

**QSGNode** (Base Container)
- Organizes hierarchical structure
- Groups related nodes
- No rendering itself

**QSGTransformNode** (Transformations)
- Applies 4x4 matrix transformations
- Handles modelview/projection matrices
- Transforms all child nodes
- Used for camera positioning and zoom

**QSGGeometryNode** (Geometry)
- Contains actual renderable geometry
- 2D vertices: triangles, triangle strips, lines, points
- Color and texture support
- Material system for rendering properties

**QSGSimpleTextureNode** (Textures)
- Renders textured quads
- Used for images, icons
- GPU-resident textures

## Migration Strategies

### Option 1: Single QQuickItem (Simplest)

**Architecture:**
```
QQuickItem (FieldRenderer)
└─ updatePaintNode() → Rebuild entire scene graph each frame
    ├─ Field background nodes
    ├─ Coverage nodes
    ├─ Boundary nodes
    ├─ Guidance line nodes
    ├─ Vehicle nodes
    └─ Flag nodes
```

**Advantages:**
- Simplest implementation
- Easy to understand and maintain
- Single source of truth for rendering
- Automatic synchronization

**Disadvantages:**
- Potentially slower (full rebuild each frame)
- No intelligent caching
- Higher CPU usage for geometry generation

**Best for:**
- Initial migration
- Prototyping
- Applications with simple scenes

**Implementation example:**
```cpp
class FieldRenderer : public QQuickItem {
    Q_OBJECT
public:
    FieldRenderer(QQuickItem *parent = nullptr)
        : QQuickItem(parent) {
        setFlag(ItemHasContents, true);
    }

    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override {
        // Create root node
        QSGNode *root = oldNode ? oldNode : new QSGNode();

        // Clear and rebuild (simple approach)
        root->removeAllChildNodes();

        // Add field background
        root->appendChildNode(createFieldNode());

        // Add coverage
        root->appendChildNode(createCoverageNode());

        // Add boundaries
        root->appendChildNode(createBoundaryNode());

        // ... more layers

        return root;
    }

private:
    QSGGeometryNode *createFieldNode();
    QSGGeometryNode *createCoverageNode();
    QSGGeometryNode *createBoundaryNode();
};
```

### Option 2: Custom Node Classes (Optimized)

**Architecture:**
```
QQuickItem (FieldRenderer)
└─ updatePaintNode()
    ├─ FieldNode (custom QSGNode)
    │   └─ Caches geometry, only updates on field changes
    ├─ CoverageNode (custom QSGNode)
    │   └─ Partial updates for coverage patches
    ├─ BoundaryNode (custom QSGNode)
    │   └─ Static geometry, rarely changes
    └─ VehicleNode (custom QSGNode)
        └─ Updates every frame (position, heading)
```

**Advantages:**
- Intelligent caching (only rebuild what changes)
- Better performance (reduced CPU usage)
- Granular control over updates
- Optimized for specific use cases

**Disadvantages:**
- More complex implementation
- Additional boilerplate code
- Requires careful cache invalidation
- Harder to debug

**Best for:**
- Performance-critical applications
- Complex scenes with many elements
- Production implementations

**Implementation example:**
```cpp
class FieldNode : public QSGNode {
public:
    void update(const FieldData &data) {
        if (m_cachedData == data) {
            return;  // No change, skip rebuild
        }

        m_cachedData = data;
        rebuildGeometry();
    }

private:
    FieldData m_cachedData;
    QSGGeometryNode *m_geometryNode;

    void rebuildGeometry() {
        // Rebuild only field geometry
    }
};

class FieldRenderer : public QQuickItem {
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override {
        FieldNode *fieldNode = static_cast<FieldNode*>(oldNode);
        if (!fieldNode) {
            fieldNode = new FieldNode();
        }

        // Update only if data changed
        fieldNode->update(m_fieldData);

        return fieldNode;
    }

private:
    FieldData m_fieldData;
};
```

### Option 3: Multiple QQuickItems (Modular)

**Architecture:**
```
QQuickItem (FieldContainer)
├─ QQuickItem (FieldBackground)
├─ QQuickItem (CoverageLayer)
├─ QQuickItem (BoundaryLayer)
├─ QQuickItem (GuidanceLayer)
├─ QQuickItem (VehicleLayer)
└─ QQuickItem (FlagLayer)
```

**Advantages:**
- Modular design (separation of concerns)
- Easy to add/remove layers
- Independent update frequencies
- Simplifies QML integration

**Disadvantages:**
- More QQuickItems (higher memory overhead)
- Coordination between items needed
- Potential z-ordering issues

**Best for:**
- Large applications
- Extensibility requirements
- Layered visualization systems

**Implementation example:**
```qml
Item {
    id: fieldContainer

    FieldBackground {
        anchors.fill: parent
        z: 0
    }

    CoverageLayer {
        anchors.fill: parent
        z: 1
        visible: showCoverage
    }

    BoundaryLayer {
        anchors.fill: parent
        z: 2
    }

    GuidanceLayer {
        anchors.fill: parent
        z: 3
    }

    VehicleLayer {
        anchors.fill: parent
        z: 4
        position: vehiclePosition
        heading: vehicleHeading
    }

    FlagLayer {
        anchors.fill: parent
        z: 5
    }
}
```

## Implementation Considerations

### Geometry Generation

**Current approach (OpenGL):**
```cpp
// Generate vertices for AB line
QVector<GLfloat> vertices;
vertices << startX << startY << 0.0f;
vertices << endX << endY << 0.0f;

// Draw
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices.data());
glDrawArrays(GL_LINES, 0, 2);
```

**SceneGraph approach:**
```cpp
// Create geometry
QSGGeometry *geometry = new QSGGeometry(
    QSGGeometry::defaultAttributes_Point2D(), 2);
geometry->setDrawingMode(QSGGeometry::DrawLines);

// Set vertices
QSGGeometry::Point2D *vertices = geometry->vertexDataAsPoint2D();
vertices[0].set(startX, startY);
vertices[1].set(endX, endY);

// Create node
QSGGeometryNode *node = new QSGGeometryNode();
node->setGeometry(geometry);
node->setFlag(QSGNode::OwnsGeometry);

// Set material (color, etc.)
QSGFlatColorMaterial *material = new QSGFlatColorMaterial();
material->setColor(QColor(255, 0, 0));
node->setMaterial(material);
node->setFlag(QSGNode::OwnsMaterial);
```

### Coordinate Transformation

**Current transformation (OpenGL matrices):**
```cpp
QMatrix4x4 projection;
projection.ortho(left, right, bottom, top, -1.0, 1.0);

QMatrix4x4 modelview;
modelview.translate(cameraX, cameraY, 0);
modelview.scale(zoomFactor, zoomFactor, 1.0);

glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, projection.data());
glUniformMatrix4fv(modelviewLoc, 1, GL_FALSE, modelview.data());
```

**SceneGraph transformation (QSGTransformNode):**
```cpp
QSGTransformNode *transformNode = new QSGTransformNode();

QMatrix4x4 matrix;
matrix.ortho(left, right, bottom, top, -1.0, 1.0);
matrix.translate(cameraX, cameraY, 0);
matrix.scale(zoomFactor, zoomFactor, 1.0);

transformNode->setMatrix(matrix);

// Add geometry as child
transformNode->appendChildNode(geometryNode);
```

### Material System

**Basic materials:**
- `QSGFlatColorMaterial` - Solid colors
- `QSGVertexColorMaterial` - Per-vertex colors
- `QSGOpaqueTextureMaterial` - Opaque textures
- `QSGTextureMaterial` - Textures with alpha

**Custom materials:**
```cpp
class CustomMaterial : public QSGMaterial {
public:
    QSGMaterialType *type() const override {
        static QSGMaterialType type;
        return &type;
    }

    QSGMaterialShader *createShader() const override {
        return new CustomMaterialShader();
    }
};
```

## Performance Optimization

### Batching

SceneGraph automatically batches draw calls with identical materials:

**Poor batching:**
```cpp
// 100 draw calls (different materials)
for (int i = 0; i < 100; ++i) {
    QSGGeometryNode *node = createLine(i);
    QSGFlatColorMaterial *material = new QSGFlatColorMaterial();
    material->setColor(QColor(i, 0, 0));  // Different color each
    node->setMaterial(material);
}
```

**Good batching:**
```cpp
// 1 draw call (same material, vertex colors)
QSGGeometry *geometry = new QSGGeometry(
    QSGGeometry::defaultAttributes_ColoredPoint2D(), 200);

QSGGeometry::ColoredPoint2D *vertices = geometry->vertexDataAsColoredPoint2D();
for (int i = 0; i < 100; ++i) {
    vertices[i*2].set(x1, y1, r, g, b, 255);
    vertices[i*2+1].set(x2, y2, r, g, b, 255);
}

QSGGeometryNode *node = new QSGGeometryNode();
node->setGeometry(geometry);
node->setMaterial(new QSGVertexColorMaterial());
```

### VBO (Vertex Buffer Objects)

SceneGraph uses VBOs automatically, keeping geometry on GPU:

```cpp
// Geometry stays on GPU
QSGGeometry *geometry = new QSGGeometry(...);
geometry->allocate(vertexCount);
// Fill vertices once

// Later frames: No CPU→GPU transfer needed
// Just update transform or material properties
```

### Culling

Implement frustum culling for off-screen elements:

```cpp
QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override {
    QRectF viewport = boundingRect();

    for (auto &element : m_elements) {
        if (viewport.intersects(element.bounds())) {
            // Element visible, add to scene graph
            addElement(element);
        } else {
            // Element off-screen, skip
        }
    }

    return oldNode;
}
```

## Migration Path

### Phase 1: Proof of Concept

1. Implement single layer (e.g., field background) using SceneGraph
2. Compare performance with current OpenGL implementation
3. Validate coordinate transformation accuracy
4. Test on target hardware (desktop, Android, Raspberry Pi)

### Phase 2: Core Layers

1. Migrate field background and grid
2. Migrate boundaries and headlands
3. Migrate coverage patches
4. Validate visual accuracy and performance

### Phase 3: Dynamic Elements

1. Migrate guidance lines (AB lines, curves)
2. Migrate vehicle and implement
3. Migrate flags and markers
4. Implement update optimization

### Phase 4: Optimization

1. Implement intelligent caching (Option 2)
2. Batch similar geometry
3. Optimize VBO usage
4. Profile and optimize hotspots

### Phase 5: Polish

1. Add visual effects (shadows, anti-aliasing)
2. Improve rendering quality
3. Optimize for mobile (Android)
4. Performance validation across platforms

## Current OpenGL Code Structure

### Key Files

**Rendering:**
- [aogrenderer.cpp](../../aogrenderer.cpp) - Main OpenGL renderer
- [formgps_opengl.cpp](../../formgps_opengl.cpp) - Drawing logic
- [glutils.cpp](../../glutils.cpp) - OpenGL utilities

**Shaders:**
- `shaders/vertex.glsl` - Vertex shader
- `shaders/fragment.glsl` - Fragment shader

**Rendering frequency:**
- [ARCHITECTURE_FREQUENCES_TIMERS.md](../../ARCHITECTURE_FREQUENCES_TIMERS.md) - Timer documentation

### Example OpenGL Code

**Drawing AB line (current):**
```cpp
void FormGPS::DrawABLine() {
    // Set shader
    m_program->bind();

    // Set matrices
    m_program->setUniformValue("projectionMatrix", projectionMatrix);
    m_program->setUniformValue("modelviewMatrix", modelviewMatrix);

    // Prepare vertices
    QVector<QVector3D> vertices;
    vertices << QVector3D(abLine.startX, abLine.startY, 0);
    vertices << QVector3D(abLine.endX, abLine.endY, 0);

    // Draw
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices.data());
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, 2);
    glDisableVertexAttribArray(0);

    m_program->release();
}
```

## Testing Strategy

### Visual Validation

Compare screenshots between OpenGL and SceneGraph implementations:

```cpp
// Capture framebuffer
QImage screenshot = m_window->grabWindow();
screenshot.save("scenegraph_test.png");

// Compare with reference
QImage reference("opengl_reference.png");
if (screenshot != reference) {
    qWarning() << "Visual differences detected";
}
```

### Performance Profiling

**Qt Creator Profiler:**
1. Debug > Profiler > QML Profiler
2. Record rendering session
3. Analyze frame times and GPU usage

**Console metrics:**
```cpp
QElapsedTimer timer;
timer.start();

// Render
updatePaintNode(...);

qint64 elapsed = timer.nsecsElapsed();
qDebug() << "Frame time:" << elapsed / 1000000.0 << "ms";
```

### Regression Testing

Test on all target platforms:
- Windows (OpenGL, DirectX)
- Linux (OpenGL)
- Android (OpenGL ES)
- Raspberry Pi (OpenGL ES)

Verify:
- Visual accuracy
- Performance (30 Hz minimum)
- Memory usage
- Stability (no leaks, no crashes)

## Additional Resources

- [Qt SceneGraph Documentation](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html)
- [Qt SceneGraph Example](https://doc.qt.io/qt-6/qtquick-scenegraph-customgeometry-example.html)
- [OpenGL to SceneGraph Migration Guide](https://doc.qt.io/qt-6/qtquick-visualcanvas-adaptations.html)
- [Qt Quick Scene Graph Renderer](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html)

## See Also

- [Building QtAgOpenGPS](../development/building.md) - Build system
- [Contributing Guidelines](../development/contributing.md) - Development workflow
- [Architecture Documentation](../../ARCHITECTURE_COMPLETE_QTAOG.md) - Complete system architecture
