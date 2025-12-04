// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// OpenGL renderer. Displayed in QML
#ifndef OPENGLCONTROL_H
#define OPENGLCONTROL_H

#include <QQuickWindow>
//#include <QOpenGLContext>
#include <QQuickFramebufferObject>
#include <QtQml/qqmlregistration.h>
#include <QProperty>
#include <QBindable>
#include <QOpenGLFunctions>
#include <QSGRenderNode>

#include <functional>
Q_DECLARE_METATYPE(std::function<void (void)>)

class AOGRendererItem;

class AOGRenderer : public QQuickFramebufferObject::Renderer
{

protected:
    virtual void synchronize(QQuickFramebufferObject *);

    bool isInitialized;

public:
    AOGRenderer();
    ~AOGRenderer();

    void render();

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size);

    /*
    void registerPaintCallback(std::function<void ()> callback);
    void registerInitCallback(std::function<void ()> callback);
    void registerCleanupCallback(std::function<void ()> callback);
    */

private:
    QQuickWindow *win;
    bool calledInit;
    //FormGPS *mf;
    int samples;

    //callback in main form to do actual rendering
    void *callback_object;
    std::function<void (void)> paintCallback;
    std::function<void (void)> initCallback;
    std::function<void (void)> cleanupCallback;
};

class AOGRendererInSG : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AOGRenderer)

    // ===== QML PROPERTIES - Qt 6.8 Rectangle Pattern =====
    Q_PROPERTY(double shiftX READ shiftX WRITE setShiftX NOTIFY shiftXChanged BINDABLE bindableShiftX)
    Q_PROPERTY(double shiftY READ shiftY WRITE setShiftY NOTIFY shiftYChanged BINDABLE bindableShiftY)

public:
    AOGRenderer *theRenderer;

    AOGRendererInSG(QQuickItem* parent = nullptr);

    AOGRenderer *createRenderer() const;

    // ===== Qt 6.8 Rectangle Pattern READ/WRITE/BINDABLE Methods =====
    double shiftX() const;
    void setShiftX(double value);
    QBindable<double> bindableShiftX();

    double shiftY() const;
    void setShiftY(double value);
    QBindable<double> bindableShiftY();

signals:
    // Qt 6.8 Rectangle Pattern NOTIFY signals
    void shiftXChanged();
    void shiftYChanged();

    // NOTE: clicked, dragged, zoomOut, zoomIn signals are defined in QML
    // and work via Qt's meta-object system - no C++ declaration needed

private:
    // ===== Qt 6.8 Q_OBJECT_BINDABLE_PROPERTY Private Members =====
    Q_OBJECT_BINDABLE_PROPERTY(AOGRendererInSG, double, m_shiftX, &AOGRendererInSG::shiftXChanged)
    Q_OBJECT_BINDABLE_PROPERTY(AOGRendererInSG, double, m_shiftY, &AOGRendererInSG::shiftYChanged)
};

class AOGRendererItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(AOGRendererItem)

    Q_PROPERTY(std::function<void (void)> paintCallback READ getPaintCallback WRITE setPaintCallback NOTIFY paintCallbackChanged)
    Q_PROPERTY(std::function<void (void)> initCallback READ getInitCallback WRITE setInitCallback NOTIFY initCallbackChanged)
    Q_PROPERTY(std::function<void (void)> cleanupCallback READ getCleanupCallback WRITE setCleanupCallback NOTIFY cleanupCallbackChanged)
    Q_PROPERTY(void* callbackObject READ getCallbackObject WRITE setCallbackObject NOTIFY callbackObjectChanged)
    Q_PROPERTY(int samples READ getSamples WRITE setSamples NOTIFY samplesChanged)

public:
    AOGRendererItem();
    std::function<void (void)> getPaintCallback() { return paintCallback; }
    std::function<void (void)> getInitCallback() { return initCallback; }
    std::function<void (void)> getCleanupCallback() { return cleanupCallback; }
    void *getCallbackObject() { return callback_object; }
    int getSamples() { return samples;}

    void setInitCallback(std::function<void (void)> callback);
    void setPaintCallback(std::function<void (void)> callback);
    void setCleanupCallback(std::function<void (void)> callback);
    void setSamples(int samples);
    void setCallbackObject(void *object);

    void *callback_object;
    std::function<void (void)> paintCallback;
    std::function<void (void)> initCallback;
    std::function<void (void)> cleanupCallback;
    int samples;

    // ===== Qt 6.8 Rectangle Pattern READ/WRITE/BINDABLE Methods =====
    double shiftX() const;
    void setShiftX(double value);
    QBindable<double> bindableShiftX();

    double shiftY() const;
    void setShiftY(double value);
    QBindable<double> bindableShiftY();

protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *) override;

signals:
    void paintCallbackChanged();
    void initCallbackChanged();
    void cleanupCallbackChanged();
    void callbackObjectChanged();
    void samplesChanged();
    // Qt 6.8 Rectangle Pattern NOTIFY signals
    void shiftXChanged();
    void shiftYChanged();

    // NOTE: clicked, dragged, zoomOut, zoomIn signals are defined in QML
    // and work via Qt's meta-object system - no C++ declaration needed

private:
    // ===== Qt 6.8 Q_OBJECT_BINDABLE_PROPERTY Private Members =====
    Q_OBJECT_BINDABLE_PROPERTY(AOGRendererItem, double, m_shiftX, &AOGRendererItem::shiftXChanged)
    Q_OBJECT_BINDABLE_PROPERTY(AOGRendererItem, double, m_shiftY, &AOGRendererItem::shiftYChanged)

public slots:
    //void sync();
    //void cleanup();
};

class AOGRendererNode : public QSGRenderNode
{
public:
    //void setViewportSize(const QSize &size) { m_viewportSize = size; }
    //void setWindow(QQuickWindow *window) { m_window = window; }
    //void setItem(AOGRendererItem *item) {this->item = item; }
    AOGRendererNode(QQuickWindow *window);
    ~AOGRendererNode();
    void render(const RenderState *state) override;
    void releaseResources() override;
    QSGRenderNode::RenderingFlags flags() const override;
    QRectF rect() const override;

    void sync(QQuickItem *item);

private:
    QRectF our_rect;
    bool initialized = false;
    QQuickWindow *m_window = nullptr;


public:
    AOGRendererItem *item;

    //int samples;
    //QSize m_viewportSize;
    //QQuickWindow *m_window = nullptr;
};

#endif // OPENGLCONTROL_H
