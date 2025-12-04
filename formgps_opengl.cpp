// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
// Main loop OpenGL stuff
//#include <QtOpenGL>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <assert.h>
#include "formgps.h"
#include "csection.h"
#include "cvehicle.h"
#include "cworldgrid.h"
#include "ccontour.h"
#include "cabline.h"
#include "cboundary.h"
#include "aogrenderer.h"
#include "cnmea.h"
#include "qmlutil.h"
#include "glm.h"
#include "glutils.h"
#include "qmlutil.h"
#include "classes/agioservice.h"  // For zero-latency GPS access
#include "classes/settingsmanager.h"
#include "cpgn.h"

#include <assert.h>
#include <QElapsedTimer>  // For latency profiling

#include <QLabel>

Q_LOGGING_CATEGORY (qgl, "formgps_opengl.qtagopengps")

extern QLabel *overlapPixelsWindow;
extern QOpenGLShaderProgram *interpColorShader; //pull from glutils.h

// Helper function to safely get OpenGL control properties with defaults
struct OpenGLViewport {
    QOpenGLContext *context = nullptr;
    int width = 800;
    int height = 600;
    double shiftX = 0.0;
    double shiftY = 0.0;
};

struct DrawElementsIndirectCommand {
    GLuint count;         // Number of indices
    GLuint instanceCount; // Number of instances
    GLuint firstIndex;    // Offset in index buffer
    GLuint baseVertex;    // Added to each index
    GLuint baseInstance;  // Base instance for instanced attributes
};

#define PATCHBUFFER_LENGTH 16 * 1024 * 1024 //16 MB
#define VERTEX_SIZE sizeof(ColorVertex) //combined vertex and color, 7 floats

QOpenGLContext *getGLContext(QQuickWindow *window) {
    auto *ri = window->rendererInterface();
    if (ri->graphicsApi() == QSGRendererInterface::OpenGL) {
        return static_cast<QOpenGLContext *>(
            ri->getResource(window, QSGRendererInterface::OpenGLContextResource));
    }
    return nullptr; // Not using OpenGL
}

OpenGLViewport getOpenGLViewport(QObject* mainWindow) {
    OpenGLViewport viewport;
    QObject *openglControl = qmlItem(mainWindow, "openglcontrol");


    if (openglControl) {
        viewport.width = openglControl->property("width").toReal();
        viewport.height = openglControl->property("height").toReal();
        // ✅ PHASE 6.3.0 FIX: shiftX/shiftY are now Q_PROPERTY in AOGRendererInSG
        viewport.shiftX = openglControl->property("shiftX").toDouble();
        viewport.shiftY = openglControl->property("shiftY").toDouble();
        viewport.context = getGLContext(qobject_cast<QQuickItem *>(openglControl)->window());
    } else {
        qWarning() << "⚠️ OpenGL control not found - using default viewport settings";
        // Defaults already set in struct definition
    }

    return viewport;
}
/*
*/
// Latency profiler for real-time validation
class LatencyProfiler {
public:
    static void measure(const QString& operation) {
        static QElapsedTimer timer;
        static bool initialized = false;
        if (!initialized) {
            timer.start();
            initialized = true;
            return;
        }
        qint64 elapsed = timer.nsecsElapsed();
        if (elapsed > 1000) { // >1μs warning for AutoSteer safety
            qWarning() << "⚠️ LATENCY:" << operation << elapsed << "ns";
        }
        timer.restart();
    }
};

QVector3D FormGPS::mouseClickToPan(int mouseX, int mouseY)
{
    /* returns easting and northing relative to the tractor's hitch position,
     * useful for drag to pan
     */

    QMatrix4x4 modelview;
    QMatrix4x4 projection;

    // Safe access to OpenGL viewport properties
    OpenGLViewport viewport = getOpenGLViewport(mainWindow);
    int width = viewport.width;
    int height = viewport.height;
    double shiftX = viewport.shiftX;
    double shiftY = viewport.shiftY;

    projection.setToIdentity();

    //to shift, translate projection here. -1,0,0 is far left, 1,0,0 is far right.
    projection.translate(shiftX,shiftY,0);

    //  Create a perspective transformation.
    projection.perspective(glm::toDegrees(fovy), width / (double)height, 1.0f, camDistanceFactor * camera.camSetDistance);
    modelview.setToIdentity();

    //camera does translations and rotations
    camera.SetWorldCam(modelview, CVehicle::instance()->pivotAxlePos.easting, CVehicle::instance()->pivotAxlePos.northing, camera.camHeading);
    modelview.translate(CVehicle::instance()->hitchPos.easting, CVehicle::instance()->hitchPos.northing, 0);
    //modelview.translate(sin(CVehicle::instance()->fixHeading) * tool.hitchLength,
    //                        cos(CVehicle::instance()->fixHeading) * tool.hitchLength, 0);
    if (camera.camFollowing)
        modelview.rotate(glm::toDegrees(-CVehicle::instance()->fixHeading), 0.0, 0.0, 1.0);

    float x,y;
    x = mouseX;
    y = height - mouseY;

    //get point on the near plane
    QVector3D worldpoint_near = QVector3D( { x, y, 0} ).unproject(modelview,projection,QRect(0,0,width, height));
    //get point on the far plane
    QVector3D worldpoint_far = QVector3D( { x, y, 1} ).unproject(modelview, projection,QRect(0,0,width, height));
    //get direction vector from near to far
    QVector3D direction = worldpoint_far - worldpoint_near;
    //determine intercept with z=0 plane, and calculate easting and northing
    double lambda = -(worldpoint_near.z()) / direction.z();

    mouseEasting = worldpoint_near.x() + lambda * direction.x();
    mouseNorthing = worldpoint_near.y() + lambda * direction.y();

    QMatrix4x4 m;
    m.rotate(-CVehicle::instance()->fixHeading, 0,0,1);

    QVector3D relative = QVector3D( { (float)mouseEasting, (float)mouseNorthing, 0 } );
    return relative;
}

QVector3D FormGPS::mouseClickToField(int mouseX, int mouseY)
{
    /* returns the field easting and northing position of a
     * mouse click
     */

    QMatrix4x4 modelview;
    QMatrix4x4 projection;

    // Safe access to OpenGL viewport properties
    OpenGLViewport viewport = getOpenGLViewport(mainWindow);
    int width = viewport.width;
    int height = viewport.height;
    double shiftX = viewport.shiftX;
    double shiftY = viewport.shiftY;

    projection.setToIdentity();

    //to shift, translate projection here. -1,0,0 is far left, 1,0,0 is far right.
    projection.translate(shiftX,shiftY,0);

    //  Create a perspective transformation.
    projection.perspective(glm::toDegrees(fovy), width / (double)height, 1.0f, camDistanceFactor * camera.camSetDistance);
    modelview.setToIdentity();

    //camera does translations and rotations
    camera.SetWorldCam(modelview, CVehicle::instance()->pivotAxlePos.easting, CVehicle::instance()->pivotAxlePos.northing, camera.camHeading);
    //modelview.translate(CVehicle::instance()->pivotAxlePos.easting, CVehicle::instance()->pivotAxlePos.northing, 0);
    //modelview.translate(sin(CVehicle::instance()->fixHeading) * tool.hitchLength,
    //                        cos(CVehicle::instance()->fixHeading) * tool.hitchLength, 0);
    //if (camera.camFollowing)
    //    modelview.rotate(glm::toDegrees(-CVehicle::instance()->fixHeading), 0.0, 0.0, 1.0);

    float x,y;
    x = mouseX;
    y = height - mouseY;

    //get point on the near plane
    QVector3D worldpoint_near = QVector3D( { x, y, 0} ).unproject(modelview,projection,QRect(0,0,width, height));
    //get point on the far plane
    QVector3D worldpoint_far = QVector3D( { x, y, 1} ).unproject(modelview, projection,QRect(0,0,width, height));
    //get direction vector from near to far
    QVector3D direction = worldpoint_far - worldpoint_near;
    //determine intercept with z=0 plane, and calculate easting and northing
    double lambda = -(worldpoint_near.z()) / direction.z();

    mouseEasting = worldpoint_near.x() + lambda * direction.x();
    mouseNorthing = worldpoint_near.y() + lambda * direction.y();

    QMatrix4x4 m;
    m.rotate(-CVehicle::instance()->fixHeading, 0,0,1);

    QVector3D fieldCoord = QVector3D( { (float)mouseEasting, (float)mouseNorthing, 0 } );
    return fieldCoord;
}

void FormGPS::render_main_fbo()
{
    QOpenGLContext *glContext = QOpenGLContext::currentContext();
    QOpenGLFunctions *gl = glContext->functions();
    //int width = glContext->surface()->size().width();
    //int height = glContext->surface()->size().height();
    QMatrix4x4 projection;
    QMatrix4x4 modelview;
    QColor color;
    GLHelperTextureBack gldrawtex;
    //GLHelperOneColorBack gldrawtex;

    initializeBackShader();

    // Set The Blending Function For Translucency
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl->glCullFace(GL_BACK);

    gl->glEnable(GL_BLEND);
    gl->glClearColor(0.25122f, 0.258f, 0.275f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl->glDisable(GL_DEPTH_TEST);

    modelview.setToIdentity();
    projection.setToIdentity();

    // Safe access to OpenGL viewport properties
    OpenGLViewport viewport = getOpenGLViewport(mainWindow);
    int width = viewport.width;
    int height = viewport.height;
    double shiftX = viewport.shiftX;
    double shiftY = viewport.shiftY;

    //gl->glViewport(0,0,width,height);
    //qDebug(qgl) << "viewport is " << width << height;

    if (active_fbo >= 0) {

        gl->glActiveTexture(GL_TEXTURE0);
        gl->glBindTexture(GL_TEXTURE_2D, mainFBO[active_fbo]->texture());

        //qDebug(qgl) << "Texture is " << overPix.size();
        //QOpenGLTexture texture = QOpenGLTexture(grnPix.mirrored(false, true));
        //texture.bind();


        gldrawtex.append( { QVector3D(1, 1, 0), QVector2D(1,1) } ); //Top Right
        gldrawtex.append( { QVector3D(-1, 1, 0), QVector2D(0,1) } ); //Top Left
        gldrawtex.append( { QVector3D(1, -1, 0), QVector2D(1,0) } ); //Bottom Right
        gldrawtex.append( { QVector3D(-1, -1, 0), QVector2D(0,0) } ); //Bottom Left
        /*
        gldrawtex.append( QVector3D(0.75, 0.75, 0)); //Top Right
        gldrawtex.append( QVector3D(-0.75, 0.75, 0)); //Top Left
        gldrawtex.append( QVector3D(0.75, -0.75, 0)); //Bottom Right
        gldrawtex.append( QVector3D(-0.75, -0.75, 0)); //Bottom Left
        */

        gldrawtex.draw(gl, projection * modelview, GL_TRIANGLE_STRIP, false);
        //gldrawtex.draw(gl, projection * modelview, QColor::fromRgb(255,0,0), GL_LINE_STRIP, 1.0f );
        //texture.release();
        gl->glBindTexture(GL_TEXTURE_2D, 0); //unbind the texture
        //texture.destroy();
    }
    gl->glFlush();
}

void FormGPS::oglMain_Paint()
{
    OpenGLViewport viewport = getOpenGLViewport(mainWindow);
#if !defined(Q_OS_WINDOWS) && !defined(Q_OS_ANDROID)
    //if there's no context we need to create one because
    //the qml renderer is in a different thread.
    if (!mainOpenGLContext.isValid()) {
        if (viewport.context)
            mainOpenGLContext.setShareContext(viewport.context);
        mainOpenGLContext.create();
    }
#endif
    QMatrix4x4 projection;
    QMatrix4x4 modelview;
    QColor color;
    GLHelperTexture gldrawtex;
    GLHelperColors gldrawcolors;
    GLHelperOneColor gldraw1;

    float lineWidth = SettingsManager::instance()->display_lineWidth();
    
    // Safe access to OpenGL viewport properties
    int width = viewport.width;
    int height = viewport.height;
    double shiftX = viewport.shiftX;
    double shiftY = viewport.shiftY;
    //gl->glViewport(oglX,oglY,width,height);
    //qDebug(qgl) << "viewport is " << width << height;

#if !defined(Q_OS_WINDOWS) && !defined(Q_OS_ANDROID)
    if (!mainSurface.isValid()) {
        QSurfaceFormat format = mainOpenGLContext.format();
        mainSurface.setFormat(format);
        mainSurface.create();
        auto r = mainSurface.isValid();
        qDebug(qgl) << "main surface creation: " << r;
    }

    auto result = mainOpenGLContext.makeCurrent(&mainSurface);

    QOpenGLFunctions *gl = mainOpenGLContext.functions();
#else
    QOpenGLContext *glContext = QOpenGLContext::currentContext();
    QOpenGLFunctions *gl = glContext->functions();
#endif

    initializeTextures();
    initializeShaders();

#if !defined(Q_OS_WINDOWS) && !defined(Q_OS_ANDROID)
    //we will work on the unused texture in case QML is rendering on
    //another core
    int working_fbo = (active_fbo < 0 ? 0 : active_fbo + 1 % 1);


    if (!mainFBO[working_fbo] || mainFBO[working_fbo]->size() != QSize(width,height)) {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        // ✅ C++17 RAII: automatic memory management, no manual delete needed
        mainFBO[working_fbo].reset(new QOpenGLFramebufferObject(QSize(width,height), format));
    }

    mainFBO[working_fbo]->bind();

    mainOpenGLContext.functions()->glViewport(0,0,width,height);
#endif

    // Set The Blending Function For Translucency
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl->glCullFace(GL_BACK);

    //gl->glDisable(GL_CULL_FACE);

    //set the camera to right distance
    camera.SetZoom();

    //now move the "camera" to the calculated zoom settings
    //I had to move these functions here because if setZoom is called
    //from elsewhere in the GUI (say a button press), there's no GL
    //context to work with.
    projection.setToIdentity();

    //to shift, translate projection here. -1,0,0 is far left, 1,0,0 is far right.

    //warning.  Moving in the Y direction alters the way the field tilts in 3D view.
    //would need to adjust the camera.setWorldCam stuff.
    //But can move in X direction without issue
    projection.translate(shiftX,shiftY,0);


    //  Create a perspective transformation.
    projection.perspective(glm::toDegrees(fovy), width / (double)height, 1.0f, camDistanceFactor * camera.camSetDistance);

    //oglMain rendering, Draw

    int deadCam = 0;

    gl->glEnable(GL_BLEND);
    gl->glClearColor(0.25122f, 0.258f, 0.275f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl->glDisable(GL_DEPTH_TEST);
    //gl->glDisable(GL_TEXTURE_2D);

    int currentPatchBuffer = 0;

    if(this->sentenceCounter() < 299)
    {
        if (isGPSPositionInitialized)
        {

            //  Clear the color and depth buffer.
            gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (isDay) gl->glClearColor(0.27f, 0.4f, 0.7f, 1.0f);
            else gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

            modelview.setToIdentity();

            //camera does translations and rotations
            camera.SetWorldCam(modelview, CVehicle::instance()->pivotAxlePos.easting, CVehicle::instance()->pivotAxlePos.northing, camera.camHeading);

            //calculate the frustum planes for culling
            CalcFrustum(projection*modelview);

            QColor fieldcolor;
            if(isDay) {
                fieldcolor = fieldColorDay;
            } else {
                fieldcolor = fieldColorNight;
            }
            //draw the field ground images
            worldGrid.DrawFieldSurface(gl, projection *modelview, isTextureOn, fieldcolor, camera);

            ////if grid is on draw it
            if (isGridOn)
                worldGrid.DrawWorldGrid(gl, modelview, projection, camera.gridZoom, QColor::fromRgbF(0,0,0,1));

            //OpenGL ES does not support wireframe in this way. If we want wireframe,
            //we'll have to do it with LINES
            //if (isDrawPolygons) gl->glPolygonMode(GL_FRONT, GL_LINE);

            gl->glEnable(GL_BLEND);
            //draw patches of sections

            if (patchesBufferDirty) {
                //destroy all GPU patches buffers
                patchBuffer.clear();
                patchesInBuffer.clear();

                for (int j = 0; j < triStrip.count(); j++) {
                    patchesInBuffer.append(QVector<PatchInBuffer>());
                    for (int k=0; k < triStrip[j].patchList.size()-1 ; k++) {
                        patchesInBuffer[j].append({ -1, -1, -1});
                    }
                }

                patchBuffer.append( { QOpenGLBuffer(), 0} );
                patchBuffer[0].patchBuffer.create();
                patchBuffer[0].patchBuffer.bind();
                patchBuffer[0].patchBuffer.allocate(PATCHBUFFER_LENGTH); //16 MB
                patchBuffer[0].patchBuffer.release();
                if (!patchesInBuffer.count()) {
                    patchesInBuffer.append(QVector<PatchInBuffer>());
                    patchesInBuffer[0].append({ -1, -1, -1});
                }
                currentPatchBuffer = 0;

                patchesBufferDirty = false;
            } else {
                currentPatchBuffer = patchBuffer.count() - 1;
            }

            bool draw_patch = false;
            //int total_vertices = 0;

            //initialize the steps for mipmap of triangles (skipping detail while zooming out)
            int mipmap = 0;
            if (camera.camSetDistance < -800) mipmap = 2;
            if (camera.camSetDistance < -1500) mipmap = 4;
            if (camera.camSetDistance < -2400) mipmap = 8;
            if (camera.camSetDistance < -5000) mipmap = 16;

            if (mipmap > 1)
                qDebug(qgl) << "mipmap is" << mipmap;

            //QVector<GLuint> indices;
            //indices.reserve(PATCHBUFFER_LENGTH / 28 * 3);  //enough to index 16 MB worth of vertices
            QVector<QVector<GLuint>> indices2;
            for (int i=0; i < patchBuffer.size(); i++) {
                indices2.append(QVector<GLuint>());
                indices2[i].reserve(PATCHBUFFER_LENGTH / 28 * 3);
            }

            bool enough_indices = false;

            //draw patches j= # of sections
            for (int j = 0; j < triStrip.count(); j++)
            {
                //every time the section turns off and on is a new patch
                int patchCount = triStrip[j].patchList.size();

                for (int k=0; k < patchCount; k++) {
                    QSharedPointer<PatchTriangleList> triList = triStrip[j].patchList[k];
                    QVector3D *triListRaw = triList->data();
                    int count2 = triList->size();
                    //total_vertices += count2;
                    draw_patch = false;

                    draw_patch = false;
                    for (int i = 1; i < count2; i += 3) //first vertice is color
                    {
                        //determine if point is in frustum or not, if < 0, its outside so abort, z always is 0
                        //x is easting, y is northing
                        if (frustum[0] * triListRaw[i].x() + frustum[1] * triListRaw[i].y() + frustum[3] <= 0)
                            continue;//right
                        if (frustum[4] * triListRaw[i].x() + frustum[5] * triListRaw[i].y() + frustum[7] <= 0)
                            continue;//left
                        if (frustum[16] * triListRaw[i].x() + frustum[17] * triListRaw[i].y() + frustum[19] <= 0)
                            continue;//bottom
                        if (frustum[20] * triListRaw[i].x() + frustum[21] * triListRaw[i].y() + frustum[23] <= 0)
                            continue;//top
                        if (frustum[8] * triListRaw[i].x() + frustum[9] * triListRaw[i].y() + frustum[11] <= 0)
                            continue;//far
                        if (frustum[12] * triListRaw[i].x() + frustum[13] * triListRaw[i].y() + frustum[15] <= 0)
                            continue;//near

                        //point is in frustum so draw the entire patch. The downside of triangle strips.
                        draw_patch = true;
                        break;
                    }

                    if (!draw_patch) continue;
                    color.setRgbF((*triList)[0].x(), (*triList)[0].y(), (*triList)[0].z(), 0.596 );

                    if (k == patchCount - 1) {
                        //If this is the last patch in the list, it's currently being worked on
                        //so we don't save this one.
                        QOpenGLBuffer triBuffer;

                        triBuffer.create();
                        triBuffer.bind();

                        //triangle lists are now using QVector3D, so we can allocate buffers
                        //directly from list data.

                        //first vertice is color, so we should skip it
                        triBuffer.allocate(triList->data() + 1, (count2-1) * sizeof(QVector3D));
                        //triBuffer.allocate(triList->data(), count2 * sizeof(QVector3D));
                        triBuffer.release();

                        //draw the triangles in each triangle strip
                        glDrawArraysColor(gl,projection*modelview,
                                          GL_TRIANGLE_STRIP, color,
                                          triBuffer,GL_FLOAT,count2-1);

                        triBuffer.destroy();
                        //qDebug(qgl) << "Last patch, not cached.";
                        continue;
                    } else {
                        if (j >= patchesInBuffer.size())
                            patchesInBuffer.append(QVector<PatchInBuffer>());
                        if (k >= patchesInBuffer[j].size())
                            patchesInBuffer[j].append({ -1, -1, -1});

                        if (patchesInBuffer[j][k].which == -1) {
                            //patch is not in one of the big buffers yet, so allocate it.
                            if ((patchBuffer[currentPatchBuffer].length + (count2-1) * VERTEX_SIZE) >= PATCHBUFFER_LENGTH ) {
                                //add a new buffer because the current one is full.
                                currentPatchBuffer ++;
                                patchBuffer.append( { QOpenGLBuffer(), 0 });
                                patchBuffer[currentPatchBuffer].patchBuffer.create();
                                patchBuffer[currentPatchBuffer].patchBuffer.bind();
                                patchBuffer[currentPatchBuffer].patchBuffer.allocate(PATCHBUFFER_LENGTH); //4MB
                                patchBuffer[currentPatchBuffer].patchBuffer.release();
                                indices2.append(QVector<GLuint>());
                                indices2[currentPatchBuffer].reserve(PATCHBUFFER_LENGTH / 28 * 3);
                            }

                            //there's room for it in the current patch buffer
                            patchBuffer[currentPatchBuffer].patchBuffer.bind();
                            QVector<ColorVertex> temp_patch;
                            temp_patch.reserve(count2-1);
                            for (int i=1; i < count2; i++) {
                                temp_patch.append( { triListRaw[i], QVector4D(triListRaw[0], 0.596) } );
                            }
                            patchBuffer[currentPatchBuffer].patchBuffer.write(patchBuffer[currentPatchBuffer].length,
                                                                              temp_patch.data(),
                                                                              (count2-1) * VERTEX_SIZE);
                            patchesInBuffer[j][k].which = currentPatchBuffer;
                            patchesInBuffer[j][k].offset = patchBuffer[currentPatchBuffer].length / VERTEX_SIZE;
                            patchesInBuffer[j][k].length = count2 - 1;
                            patchBuffer[currentPatchBuffer].length += (count2 - 1) * VERTEX_SIZE;
                            qDebug(qgl) << "buffering" << j << k << patchesInBuffer[j][k].which << ", " << patchBuffer[currentPatchBuffer].length;
                            patchBuffer[currentPatchBuffer].patchBuffer.release();
                        }
                        //generate list of indices for this patch
                        int index_offset = patchesInBuffer[j][k].offset;
                        int which_buffer = patchesInBuffer[j][k].which;

                        int step = mipmap;
                        if (count2 - 1 < mipmap + 2) {
                            for (int i = 1; i < count2 - 2 ; i ++)
                            {
                                if (i % 2) {  //preserve winding order
                                    indices2[which_buffer].append(i-1 + index_offset);
                                    indices2[which_buffer].append(i   + index_offset);
                                    indices2[which_buffer].append(i+1 + index_offset);
                                } else {
                                    indices2[which_buffer].append(i-1 + index_offset);
                                    indices2[which_buffer].append(i+1   + index_offset);
                                    indices2[which_buffer].append(i + index_offset);
                                }

                            }
                        } else {
                            //use mipmap to make fewer triangles
                            int last_index2 = indices2[which_buffer].count();

                            int vertex_count = 0;
                            for (int i=1; i < count2; i += step) {
                                //convert triangle strip to triangles
                                if (vertex_count > 2 ) { //even, normal winding
                                    indices2[which_buffer].append(indices2[which_buffer][last_index2 - 1]);
                                    indices2[which_buffer].append(indices2[which_buffer][last_index2 - 2]);
                                    last_index2+=3;
                                } else {
                                    last_index2 ++;
                                }
                                indices2[which_buffer].append(i-1 + index_offset);

                                i++;
                                vertex_count++;

                                if (vertex_count > 2) { //odd, reverse winding
                                    indices2[which_buffer].append(indices2[which_buffer][last_index2 - 2]);
                                }
                                indices2[which_buffer].append(i-1 + index_offset);

                                if (vertex_count > 2) {
                                    indices2[which_buffer].append(indices2[which_buffer][last_index2 - 1 ]);
                                    last_index2 += 3;
                                } else {
                                    last_index2 ++;
                                }
                                i++;
                                vertex_count++;

                                if (count2 - i <= (mipmap + 2))
                                    //too small to mipmap, so add each one
                                    //individually.
                                    step = 0;
                            }
                        }
                        if (indices2[which_buffer].count() > 2)
                            enough_indices = true;
                    }
                }

                qDebug(qgl) << "time after preparing patches for drawing" << swFrame.nsecsElapsed() / 1000000;

                if (enough_indices) {
                    interpColorShader->bind();
                    interpColorShader->setUniformValue("mvpMatrix", projection*modelview);
                    interpColorShader->setUniformValue("pointSize", 0.0f);

                    //glDrawElements needs a vertex array object to hold state
                    QOpenGLVertexArrayObject vao;
                    vao.create();
                    vao.bind();

                    //create ibo
                    QOpenGLBuffer ibo{QOpenGLBuffer::IndexBuffer};
                    ibo.create();

                    for (int i=0; i < indices2.count(); i++) {
                        if (indices2[i].count() > 2) {
                            patchBuffer[i].patchBuffer.bind();

                            //set up vertex positions in buffer for the shader
                            gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr); //3D vector
                            gl->glEnableVertexAttribArray(0);

                            gl->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float))); //color
                            gl->glEnableVertexAttribArray(1);


                            ibo.bind();
                            ibo.allocate(indices2[i].data(), indices2[i].size() * sizeof(GLuint));

                            gl->glDrawElements(GL_TRIANGLES, indices2[i].count(), GL_UNSIGNED_INT, nullptr);
                            patchBuffer[i].patchBuffer.release();

                            ibo.release();
                        }
                    }
                    ibo.destroy();
                    vao.release();
                    vao.destroy();
                    interpColorShader->release();
                }
            }

            // the follow up to sections patches
            int patchCount = 0;

            if (patchCounter > 0)
            {
                color = sectionColorDay;
                if (isDay) color.setAlpha(152);
                else color.setAlpha(76);

                for (int j = 0; j < triStrip.count(); j++)
                {
                    if (triStrip[j].isDrawing)
                    {
                        if (tool.isMultiColoredSections)
                        {
                            color = tool.secColors[j];
                            color.setAlpha(152);
                        }
                        patchCount = triStrip[j].patchList.count();

                       //draw the triangle in each triangle strip
                        gldraw1.clear();

                        //left side of triangle
                        QVector3D pt((CVehicle::instance()->cosSectionHeading * tool.section[triStrip[j].currentStartSectionNum].positionLeft) + CVehicle::instance()->toolPos.easting,
                                (CVehicle::instance()->sinSectionHeading * tool.section[triStrip[j].currentStartSectionNum].positionLeft) + CVehicle::instance()->toolPos.northing, 0);
                        gldraw1.append(pt);

                        //Right side of triangle
                        pt = QVector3D((CVehicle::instance()->cosSectionHeading * tool.section[triStrip[j].currentEndSectionNum].positionRight) + CVehicle::instance()->toolPos.easting,
                           (CVehicle::instance()->sinSectionHeading * tool.section[triStrip[j].currentEndSectionNum].positionRight) + CVehicle::instance()->toolPos.northing, 0);
                        gldraw1.append(pt);

                        int last = triStrip[j].patchList[patchCount -1]->count();
                        //antenna
                        gldraw1.append(QVector3D((*triStrip[j].patchList[patchCount-1])[last-2].x(), (*triStrip[j].patchList[patchCount-1])[last-2].y(),0));
                        gldraw1.append(QVector3D((*triStrip[j].patchList[patchCount-1])[last-1].x(), (*triStrip[j].patchList[patchCount-1])[last-1].y(),0));

                        gldraw1.draw(gl, projection*modelview, color, GL_TRIANGLE_STRIP, 1.0f);
                    }
                }
            }

            //qDebug(qgl) << "total vertices is "<< total_vertices;

            qDebug(qgl) << "time after painting patches " << (float)swFrame.nsecsElapsed() / 1000000;

            if (tram.displayMode != 0) tram.DrawTram(gl,projection*modelview,camera);

            //draw contour line if button on
            if (this->isContourBtnOn())
            {
                ct.DrawContourLine(gl, projection*modelview, mainWindow, swFrame);
            }
            else// draw the current and reference AB Lines or CurveAB Ref and line
            {
                //when switching lines, draw the ghost
                track.DrawTrack(gl, projection*modelview, isFontOn, worldGrid.isRateMap, yt, camera, gyd);
            }

            track.DrawTrackNew(gl, projection*modelview, camera, *CVehicle::instance());

            if (recPath.isRecordOn) {
                recPath.DrawRecordedLine(gl, projection*modelview);
                recPath.DrawDubins(gl, projection*modelview);
            }

            if (bnd.bndList.count() > 0 || bnd.isBndBeingMade == true)
            {
                //draw Boundaries
                bnd.DrawFenceLines(*CVehicle::instance(), mc, gl, projection*modelview, mainWindow);

                //draw the turnLines
                if (this->isYouTurnBtnOn() && ! this->isContourBtnOn())
                {
                    bnd.DrawFenceLines(*CVehicle::instance(),mc,gl,projection*modelview, mainWindow);

                    color.setRgbF(0.3555f, 0.6232f, 0.20f); //TODO: not sure what color turnLines should actually be

                    for (int i = 0; i < bnd.bndList.count(); i++)
                    {
                        DrawPolygon(gl,projection*modelview,bnd.bndList[i].turnLine,lineWidth,color);
                    }
                }

                //Draw headland
                if (this->isHeadlandOn())
                {
                    color.setRgbF(0.960f, 0.96232f, 0.30f);
                    DrawPolygon(gl,projection*modelview,bnd.bndList[0].hdLine,lineWidth,color);
                }
            }
            if (flagPts.count()>0) DrawFlags(gl, projection*modelview);
            //Direct line to flag if flag selected
            if (flagNumberPicked > 0)
            {
                if (flagPts.count() >= flagNumberPicked) {
                    gldraw1.clear();
                    gl->glLineWidth(2);
                    //TODO: implement with shader: GL.LineStipple(1, 0x0707);
                    gldraw1.append(QVector3D(CVehicle::instance()->pivotAxlePos.easting, CVehicle::instance()->pivotAxlePos.northing, 0));
                    gldraw1.append(QVector3D(flagPts[flagNumberPicked-1].easting, flagPts[flagNumberPicked-1].northing, 0));
                    gldraw1.draw(gl, projection*modelview,
                                QColor::fromRgbF(0.930f, 0.72f, 0.32f),
                                GL_LINES, lineWidth);
                    gl->glLineWidth(1);
                } else {
                    flagNumberPicked = 0;//reset
                }
            }

            //draw the vehicle/implement
            QMatrix4x4 mv = modelview; //push matrix
            // ✅ PHASE 6.3.0: InterfaceProperty guaranteed to be initialized before rendering
            tool.DrawTool(gl,modelview, projection,isJobStarted(),*CVehicle::instance(), camera,tram);
            double steerangle;
            if(timerSim.isActive()) steerangle = sim.steerangleAve;
            else steerangle = mc.actualSteerAngleDegrees;
            CVehicle::instance()->DrawVehicle(gl, modelview, projection, steerangle, isFirstHeadingSet,
                                QRect(0,0,width,height),camera,tool,bnd,mainWindow);
            modelview = mv; //pop matrix

            if (camera.camSetDistance > -150)
            {
                track.DrawTrackGoalPoint(gl, projection * modelview);
            }

            // 2D Ortho --------------------------
            //no need to "push" matrix since it will be regenerated next time
            projection.setToIdentity();
            //negative and positive on width, 0 at top to bottom ortho view
            projection.ortho(-(double)width / 2, (double)width / 2, (double)height, 0, -1, 1);

            //  Create the appropriate modelview matrix.
            modelview.setToIdentity();

            //lightbar, button status, compasstext, steer circle, Hyd Lift, RTK
            // alarm, etc are all done in QML based on properties in Interface.qml

            //uturn buttons drawn in QML

            if (tool.isDisplayTramControl && tram.displayMode != 0) { DrawTramMarkers(); }

            //if this is on, VehicleInterface.isHydLiftOn is true
            if (p_239.pgn[p_239.hydLift] == 2)
            {
                CVehicle::instance()->setHydLiftDown(false); //VehicleInterface.hydLiftDown in QML
            }
            else
            {
                CVehicle::instance()->setHydLiftDown(true);
            }

            //Reverse indicator in QML
            //RTK alarm implemented in QML
            //RTK age implemented in QML
            //guidance line text implemented in QML

            //draw the zoom window
            // ⚡ PHASE 6.3.0 SAFETY: Verify InterfaceProperty before OpenGL access
            bool jobStartedSafe = false;
            try {
                jobStartedSafe = this->isJobStarted();
            } catch (...) {
                // InterfaceProperty not ready, skip this rendering cycle
                jobStartedSafe = false;
            }

            if (jobStartedSafe)
            {
                /*TODO implement floating zoom windo
                if (threeSeconds != zoomUpdateCounter)
                {
                    zoomUpdateCounter = threeSeconds;
                    oglZoom.Refresh();
                }
                */
            }
            if (leftMouseDownOnOpenGL) MakeFlagMark(gl); //TODO: not working, fix!
        }
        else
        {
            gl->glClear(GL_COLOR_BUFFER_BIT);
        }

    }
    else
    {
        modelview.setToIdentity();
        modelview.translate(0,0.3,-10);
        //rotate the camera down to look at fix
        //modelview.rotate(-60, 1.0, 0.0, 0.0);
        modelview.rotate(deadCam, 0.0, 1.0, 0.0);
        deadCam += 5;

        //TODO: replace with QML widget

        //draw with NoGPS texture 21
       /* color.setRgbF(1.25f, 1.25f, 1.275f, 0.75);
        gldrawtex.append( { QVector3D(2.5, 2.5, 0), QVector2D(1,0) } ); //Top Right
        gldrawtex.append( { QVector3D(-2.5, 2.5, 0), QVector2D(0,0) } ); //Top Left
        gldrawtex.append( { QVector3D(2.5, -2.5, 0), QVector2D(1,1) } ); //Bottom Right
        gldrawtex.append( { QVector3D(-2.5, -2.5, 0), QVector2D(0,1) } ); //Bottom Left

        gldrawtex.draw(gl, projection * modelview, Textures::NOGPS, GL_TRIANGLE_STRIP, true,color);
*/

        // 2D Ortho ---------------------------------------////////-------------------------------------------------

        //we don't need to save the matrix since it's regenerated every time through this method
        projection.setToIdentity();

        //negative and positive on width, 0 at top to bottom ortho view
        projection.ortho(-(double)width / 2, (double)width / 2, (double)height, 0, -1, 1);

        //  Create the appropriate modelview matrix.
        modelview.setToIdentity();

        color.setRgbF(0.98f, 0.98f, 0.70f);
        int edge = -(double)width / 2 + 10;

        drawText(gl,projection * modelview,edge,height - 240, "<-- AgIO ?",1.0,true,color);


        //GUI widgets have to be updated elsewhere
    }

    gl->glFlush();
    qDebug() << "End of all main rendering" << swFrame.elapsed();

    /*
    if (SettingsManager::instance()->display_showBack()) {
        overPix = mainFBO[working_fbo]->toImage().convertToFormat(QImage::Format_RGBX8888);
        qDebug(qgl) << "image size is: " << overPix.size();
        overlapPixelsWindow->setPixmap(QPixmap::fromImage(overPix));
    }
    */
#if !defined(Q_OS_WINDOWS) && !defined(Q_OS_ANDROID)

    mainFBO[working_fbo]->bindDefault();

    //tell GUI to swich to new texture;
    active_fbo = working_fbo;
#endif
}

/// Handles the OpenGLInitialized event of the openGLControl control.
void FormGPS::openGLControl_Initialized()
{
    //QOpenGLContext *glContext = QOpenGLContext::currentContext();

    //qmlview->resetOpenGLState();

    //Load all the textures
    //qDebug(qgl) << "initializing Open GL.";
    //initializeTextures();
    //qDebug(qgl) << "textures loaded.";
    //initializeShaders();
    //qDebug(qgl) << "shaders loaded.";

}

void FormGPS::openGLControl_Shutdown()
{

    //qDebug(qgl) << "OpenGL shutting down... destroying buffers and shaders";
    //qDebug(qgl) << QOpenGLContext::currentContext();
    //We should have a valid OpenGL context here so we can clean up shaders and buffers
    destroyShaders();
    destroyTextures();
    //destroy any openGL buffers.
    worldGrid.destroyGLBuffers();
}

//back buffer openGL draw function
void FormGPS::oglBack_Paint()
{

    //QOpenGLContext *glContext = QOpenGLContext::currentContext();
    QMatrix4x4 projection;
    QMatrix4x4 modelview;

    GLHelperOneColorBack gldraw;

    GLint oldviewport[4];

    //if there's no context we need to create one because
    //the qml renderer is in a different thread.
    if (!backOpenGLContext.isValid()) {
        backOpenGLContext.create();
    }

    if (!backSurface.isValid()) {
        QSurfaceFormat format = backOpenGLContext.format();
        backSurface.setFormat(format);
        backSurface.create();
        auto r = backSurface.isValid();
        qDebug(qgl) << "back surface creation: " << r;
    }

    auto result = backOpenGLContext.makeCurrent(&backSurface);

    initializeBackShader(); //compiler the shader we need if necessary

    /* use the QML context with an offscreen surface to draw
     * the lookahead triangles
     */
    if (!backFBO) {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        // ✅ C++17 RAII: automatic memory management, no manual delete needed
        backFBO = std::make_unique<QOpenGLFramebufferObject>(QSize(500,300), format);
    }

    backFBO->bind();

    //save current viewport settings in case it conflicts with QML
    GLint origview[4];
    backOpenGLContext.functions()->glGetIntegerv(GL_VIEWPORT, origview);


    backOpenGLContext.functions()->glViewport(0,0,500,300);
    QOpenGLFunctions *gl = backOpenGLContext.functions();

    //int width = glContext->surface()->size().width();
    //int height = glContext->surface()->size().height();

    //  Load the identity.
    projection.setToIdentity();

    //projection.perspective(6.0f,1,1,6000);
    projection.perspective(glm::toDegrees((double)0.06f), 1.666666666666f, 50.0f, 520.0f);

    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear The Screen And The Depth Buffer
    //gl->glLoadIdentity();					// Reset The View
    modelview.setToIdentity();

    //back the camera up
    modelview.translate(0, 0, -500);

    //rotate camera so heading matched fix heading in the world
    //gl->glRotated(toDegrees(CVehicle::instance()->fixHeadingSection), 0, 0, 1);
    modelview.rotate(glm::toDegrees(CVehicle::instance()->toolPos.heading), 0, 0, 1);

    modelview.translate(-CVehicle::instance()->toolPos.easting - sin(CVehicle::instance()->toolPos.heading) * 15,
                        -CVehicle::instance()->toolPos.northing - cos(CVehicle::instance()->toolPos.heading) * 15,
                        0);

    //patch color
    QColor patchColor = QColor::fromRgbF(0.0f, 0.5f, 0.0f);

    //to draw or not the triangle patch
    bool isDraw;

    double pivEplus = CVehicle::instance()->pivotAxlePos.easting + 50;
    double pivEminus = CVehicle::instance()->pivotAxlePos.easting - 50;
    double pivNplus = CVehicle::instance()->pivotAxlePos.northing + 50;
    double pivNminus = CVehicle::instance()->pivotAxlePos.northing - 50;

    //draw patches j= # of sections
    for (int j = 0; j < triStrip.count(); j++)
    {
        //every time the section turns off and on is a new patch
        int patchCount = triStrip[j].patchList.size();

        if (patchCount > 0)
        {
            //for every new chunk of patch
            for (QSharedPointer<QVector<QVector3D>> &triList: triStrip[j].patchList)
            {
                isDraw = false;
                int count2 = triList->size();
                for (int i = 1; i < count2; i+=3)
                {
                    //determine if point is in frustum or not
                    if ((*triList)[i].x() > pivEplus)
                        continue;
                    if ((*triList)[i].x() < pivEminus)
                        continue;
                    if ((*triList)[i].y() > pivNplus)
                        continue;
                    if ((*triList)[i].y() < pivNminus)
                        continue;

                    //point is in frustum so draw the entire patch
                    isDraw = true;
                    break;
                }

                if (isDraw)
                {
                    QOpenGLBuffer triBuffer;

                    triBuffer.create();
                    triBuffer.bind();

                    //triangle lists are now using QVector3D, so we can allocate buffers
                    //directly from list data.

                    //first vertice is color, so we should skip it
                    triBuffer.allocate(triList->data() + 1, (count2-1) * sizeof(QVector3D));
                    //triBuffer.allocate(triList->data(), count2 * sizeof(QVector3D));
                    triBuffer.release();

                    //draw the triangles in each triangle strip
                    glDrawArraysColorBack(gl,projection*modelview,
                                      GL_TRIANGLE_STRIP, patchColor,
                                      triBuffer,GL_FLOAT,count2-1);

                    triBuffer.destroy();
                }
            }
        }
    }

    //draw tool bar for debugging
    //gldraw.clear();
    //gldraw.append(QVector3D(tool.section[0].leftPoint.easting, tool.section[0].leftPoint.northing,0.5));
    //gldraw.append(QVector3D(tool.section[tool.numOfSections-1].rightPoint.easting, tool.section[tool.numOfSections-1].rightPoint.northing,0.5));
    //gldraw.draw(gl,projection*modelview,QColor::fromRgb(255,0,0),GL_LINE_STRIP,1);

    //draw 245 green for the tram tracks

    if (tram.displayMode !=0 && tram.displayMode !=0 && (track.idx() > -1))
    {
        if ((tram.displayMode == 1 || tram.displayMode == 2))
        {
            for (int i = 0; i < tram.tramList.count(); i++)
            {
                gldraw.clear();
                for (int h = 0; h < tram.tramList[i]->count(); h++)
                    gldraw.append(QVector3D((*tram.tramList[i])[h].easting, (*tram.tramList[i])[h].northing, 0));
                gldraw.draw(gl,projection*modelview,QColor::fromRgb(0,245,0),GL_LINE_STRIP,8);
            }
        }

        if (tram.displayMode == 1 || tram.displayMode == 3)
        {
            gldraw.clear();
            for (int h = 0; h < tram.tramBndOuterArr.count(); h++)
                gldraw.append(QVector3D(tram.tramBndOuterArr[h].easting, tram.tramBndOuterArr[h].northing, 0));
            for (int h = 0; h < tram.tramBndInnerArr.count(); h++)
                gldraw.append(QVector3D(tram.tramBndInnerArr[h].easting, tram.tramBndInnerArr[h].northing, 0));
            gldraw.draw(gl,projection*modelview,QColor::fromRgb(0,245,0),GL_LINE_STRIP,8);
        }
    }

    //draw 240 green for boundary
    if (bnd.bndList.count() > 0)
    {
        ////draw the bnd line
        if (bnd.bndList[0].fenceLine.count() > 3)
        {
            DrawPolygonBack(gl,projection*modelview,bnd.bndList[0].fenceLine,3,QColor::fromRgb(0,240,0));
        }


        //draw 250 green for the headland
        if (this->isHeadlandOn() && bnd.isSectionControlledByHeadland)
        {
            DrawPolygonBack(gl,projection*modelview,bnd.bndList[0].hdLine,3,QColor::fromRgb(0,250,0));
        }
    }

    //finish it up - we need to read the ram of video card
    gl->glFlush();

    qDebug(qgl) << "Time after drawing back buffer: " << swFrame.elapsed();

    //read the whole block of pixels up to max lookahead, one read only
    //we'll use Qt's QImage function to grab it.
    grnPix = backFBO->toImage().mirrored().convertToFormat(QImage::Format_RGBX8888);
    qDebug(qgl) << "Time after glReadPixels: " << swFrame.elapsed();
    //qDebug(qgl) << grnPix.size();
    //QImage temp = grnPix.copy(tool.rpXPosition, 250, tool.rpWidth, 290 /*(int)rpHeight*/);
    //TODO: is thisn right?
    QImage temp = grnPix.copy(tool.rpXPosition, 0, tool.rpWidth, 290 /*(int)rpHeight*/);
    temp.setPixelColor(0,0,QColor::fromRgb(255,128,0));
    //grnPix = temp; //only show clipped image
    memcpy(grnPixels, temp.constBits(), temp.size().width() * temp.size().height() * 4);

    //first channel
    if (worldGrid.numRateChannels > 0)
    {
        /*
        GL.Enable(EnableCap.Texture2D);

        GL.BindTexture(TextureTarget.Texture2D, texture[(int)textures.RateMap1]);
        GL.Begin(PrimitiveType.TriangleStrip);
        GL.Color3(1.0f, 1.0f, 1.0f);
        GL.TexCoord2(0, 0);
        GL.Vertex3(worldGrid.eastingMinRate, worldGrid.northingMaxRate, 0.10);
        GL.TexCoord2(1, 0.0);
        GL.Vertex3(worldGrid.eastingMaxRate, worldGrid.northingMaxRate, 0.10);
        GL.TexCoord2(0.0, 1);
        GL.Vertex3(worldGrid.eastingMinRate, worldGrid.northingMinRate, 0.10);
        GL.TexCoord2(1, 1);
        GL.Vertex3(worldGrid.eastingMaxRate, worldGrid.northingMinRate, 0.0);
        GL.End();

        GL.Flush();

        //read the whole block of pixels up to max lookahead, one read only
        GL.ReadPixels(250, 1, 1, 1, OpenTK.Graphics.OpenGL.PixelFormat.Red, PixelType.UnsignedByte, rateRed);
        */
    }


    //The remaining code from the original method in the C# code is
    //broken out into a callback in formgps.cpp called
    //processSectionLookahead().

    backOpenGLContext.functions()->glFlush();

    //restore viewport
    backOpenGLContext.functions()->glViewport(origview[0], origview[1], origview[2], origview[3]);

    //restore QML's context
    backFBO->bindDefault();
    //resetOpenGLState();
}

//Draw section OpenGL window, not visible
void FormGPS::openGLControlBack_Initialized()
{
}

//back buffer openGL draw function
void FormGPS::oglZoom_Paint()
{
    //Because this is potentially running in another thread, we cannot
    //safely make any GUI calls to set buttons, etc.  So instead, we
    //do the GL drawing here and get the lookahead pixmap from GL here.
    //After this, this widget will emit a finished signal, where the main
    //thread can then run the second part of this function, which I've
    //split out into its own function.
    QOpenGLContext *glContext = QOpenGLContext::currentContext();
    QMatrix4x4 projection;
    QMatrix4x4 modelview;

    GLHelperOneColorBack gldraw;

    initializeBackShader(); //make sure shader is loaded

    /* use the QML context with an offscreen surface to draw
     * the lookahead triangles
     */
    if (!zoomFBO) {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        // ✅ C++17 RAII: automatic memory management, no manual delete needed
        zoomFBO = std::make_unique<QOpenGLFramebufferObject>(QSize(400,400), format);
    }

    zoomFBO->bind();
    glContext->functions()->glViewport(0,0,400,400);
    QOpenGLFunctions *gl = glContext->functions();

    //int width = glContext->surface()->size().width();
    //int height = glContext->surface()->size().height();

    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl->glCullFace(GL_BACK);
    gl->glClearColor(0, 0, 0, 1.0f);

    if (isJobStarted())
    {
        gl->glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        projection.setToIdentity(); //Reset the view
        projection.perspective(glm::toDegrees((double)1.0f), 1.0f, 100.0f, 5000.0f);
        modelview.setToIdentity();

        calculateMinMax();
        //back the camera up

        modelview.translate(0, 0, -maxFieldDistance);

        //translate to that spot in the world
        modelview.translate(-fieldCenterX, -fieldCenterY, 0);

        //draw patches
        int count2;

        for (int j = 0; j < triStrip.count(); j++)
        {
            //every time the section turns off and on is a new patch
            int patchCount = triStrip[j].patchList.count();

            if (patchCount > 0)
            {
                //for every new chunk of patch
                for (auto &triList: triStrip[j].patchList)
                {
                    //draw the triangle in each triangle strip
                    gldraw.clear();

                    count2 = triList->count();
                    int mipmap = 2;

                    //if large enough patch and camera zoomed out, fake mipmap the patches, skip triangles
                    if (count2 >= (mipmap))
                    {
                        int step = mipmap;
                        for (int i = 1; i < count2; i += step)
                        {
                            gldraw.append(QVector3D((*triList)[i].x(), (*triList)[i].y(), 0)); i++;
                            gldraw.append(QVector3D((*triList)[i].x(), (*triList)[i].y(), 0)); i++;

                            //too small to mipmap it
                            if (count2 - i <= (mipmap))
                                break;
                        }
                    }

                    else
                    {
                        for (int i = 1; i < count2; i++) gldraw.append(QVector3D((*triList)[i].x(), (*triList)[i].y(), 0));
                    }

                    gldraw.draw(gl,projection*modelview,QColor::fromRgbF(0.5, 0.5, 0.5, 0.5),GL_TRIANGLE_STRIP,1.0f);
                }
            }
        } //end of section patches

        gl->glFlush();

        //oglZoom.SwapBuffers();

        //QImage overPix;
        overPix = zoomFBO->toImage().mirrored().convertToFormat(QImage::Format_RGBX8888);
        memcpy(overPixels, overPix.constBits(), overPix.size().width() * overPix.size().height() * 4);
    }

    glContext->functions()->glFlush();

    //restore QML's context
    zoomFBO->bindDefault();
}

void FormGPS::MakeFlagMark(QOpenGLFunctions *gl)
{
    leftMouseDownOnOpenGL = false;
    uchar data1[768];
    memset(data1,0,768);

    qDebug(qgl) << "mouse down at " << mouseX << ", " << mouseY;
    //scan the center of click and a set of square points around
    gl->glReadPixels(mouseX - 8, mouseY - 8, 16, 16, GL_RGB, GL_UNSIGNED_BYTE, data1);

    //made it here so no flag found
    flagNumberPicked = 0;

    for (int ctr = 0; ctr < 768; ctr += 3)
    {
        if ((data1[ctr] == 255) || (data1[ctr + 1] == 255))
        {
            int candidateFlag = data1[ctr + 2];
            // Validate flag number is within bounds
            if (candidateFlag > 0 && candidateFlag <= flagPts.size()) {
                flagNumberPicked = candidateFlag;
                qDebug(qgl) << "Valid flag picked:" << flagNumberPicked;
            } else {
                flagNumberPicked = 0; // Invalid flag, reset to no selection
                qDebug(qgl) << "Invalid flag number" << candidateFlag << "- flagPts.size():" << flagPts.size();
            }
            break;
        }
    }

    /*TODO: popup flag menu*/
    //have to set a flag for the main loop

}

//DrawTramMarkers moved to QML
void FormGPS::DrawTramMarkers()
{
    /*3 states. On, off, manual on
     *  0 and 1 are automatic
     *  0 off
     *  1 on
     *  2 manual
     *  "flash" happens in qml.
     *  I just changed dot_color to have the 3 states
     */
    int dot_color;

    if (((tram.controlByte) & 2) == 2) dot_color = 1;// set to green #49FD49
    else dot_color = 0;//

    if ((bool)tram.isLeftManualOn())
    {
        dot_color = 2;
    }
    CVehicle::instance()->setLeftTramIndicator(dot_color);
    //done with left

    if (((tram.controlByte) & 1) == 1) dot_color = 1;
    else dot_color = 0;

    if ((bool)tram.isRightManualOn())
    {
        dot_color = 2;
    }
    CVehicle::instance()->setRightTramIndicator(dot_color);
}

void FormGPS::DrawFlags(QOpenGLFunctions *gl, QMatrix4x4 mvp)
{
    GLHelperOneColor gldraw;

    int flagCnt = flagPts.count();
    for (int f = 0; f < flagCnt; f++)
    {
        QColor color;
        QString flagColor = "&";

        if (flagPts[f].color == 0) {
            color = QColor::fromRgb(255, 0, flagPts[f].ID);
        }
        if (flagPts[f].color == 1) {
            color = QColor::fromRgb(0, 255, flagPts[f].ID);
            flagColor = "|";
        }
        if (flagPts[f].color == 2) {
            color = QColor::fromRgb(255, 255, flagPts[f].ID);
            flagColor = "~";
        }

        gldraw.append(QVector3D(flagPts[f].easting, flagPts[f].northing, 0));
        gldraw.draw(gl, mvp, color, GL_POINTS, 8.0f);
        flagColor += flagPts[f].notes;

        drawText3D(camera, gl, mvp, flagPts[f].easting, flagPts[f].northing, flagColor,1,true,color);
    }

    if (flagNumberPicked != 0)
    {
        ////draw the box around flag
        gldraw.clear();

        double offSet = (camera.zoomValue * camera.zoomValue * 0.01);
        gl->glLineWidth(4);
        gldraw.append(QVector3D(flagPts[flagNumberPicked - 1].easting, flagPts[flagNumberPicked - 1].northing + offSet, 0));
        gldraw.append(QVector3D(flagPts[flagNumberPicked - 1].easting - offSet, flagPts[flagNumberPicked - 1].northing, 0));
        gldraw.append(QVector3D(flagPts[flagNumberPicked - 1].easting, flagPts[flagNumberPicked - 1].northing - offSet, 0));
        gldraw.append(QVector3D(flagPts[flagNumberPicked - 1].easting + offSet, flagPts[flagNumberPicked - 1].northing, 0));
        gldraw.append(QVector3D(flagPts[flagNumberPicked - 1].easting, flagPts[flagNumberPicked - 1].northing + offSet, 0));
        gldraw.draw(gl, mvp, QColor::fromRgbF(0.980f, 0.0f, 0.980f),
                    GL_LINE_STRIP, 4.0);
        gl->glLineWidth(1);
    }
}

void FormGPS::CalcFrustum(const QMatrix4x4 &mvp)
{
    //const float *clip = mvp.constData(); //column major order

    // Extract the RIGHT clipping plane
    frustum[0] = mvp(3,0) - mvp(0,0);
    frustum[1] = mvp(3,1) - mvp(0,1);
    frustum[2] = mvp(3,2) - mvp(0,2);
    frustum[3] = mvp(3,3) - mvp(0,3);
    //frustum[0] = clip[3] - clip[0];
    //frustum[1] = clip[7] - clip[4];
    //frustum[2] = clip[11] - clip[8];
    //frustum[3] = clip[15] - clip[12];

    // Extract the LEFT clipping plane
    frustum[4] = mvp(0,0) + mvp(3,0);
    frustum[5] = mvp(0,1) + mvp(3,1);
    frustum[6] = mvp(0,2) + mvp(3,2);
    frustum[7] = mvp(0,3) + mvp(3,3);
    //frustum[4] = clip[3] + clip[0];
    //frustum[5] = clip[7] + clip[4];
    //frustum[6] = clip[11] + clip[8];
    //frustum[7] = clip[15] + clip[12];

    // Extract the FAR clipping plane
    frustum[8] = mvp(3,0) - mvp(2,0);
    frustum[9] = mvp(3,1) - mvp(2,1);
    frustum[10] = mvp(3,2) - mvp(2,2);
    frustum[11] = mvp(3,3) - mvp(2,3);
    //frustum[8] = clip[3] - clip[2];
    //frustum[9] = clip[7] - clip[6];
    //frustum[10] = clip[11] - clip[10];
    //frustum[11] = clip[15] - clip[14];

    // Extract the NEAR clipping plane.  This is last on purpose (see pointinfrustum() for reason)
    frustum[12] = mvp(2,0) + mvp(3,0);
    frustum[13] = mvp(2,1) + mvp(3,1);
    frustum[14] = mvp(2,2) + mvp(3,2);
    frustum[15] = mvp(2,3) + mvp(3,3);
    //frustum[12] = clip[3] + clip[2];
    //frustum[13] = clip[7] + clip[6];
    //frustum[14] = clip[11] + clip[10];
    //frustum[15] = clip[15] + clip[14];

    // Extract the BOTTOM clipping plane
    frustum[16] = mvp(1,0) + mvp(3,0);
    frustum[17] = mvp(1,1) + mvp(3,1);
    frustum[18] = mvp(1,2) + mvp(3,2);
    frustum[19] = mvp(1,3) + mvp(3,3);
    //frustum[16] = clip[3] + clip[1];
    //frustum[17] = clip[7] + clip[5];
    //frustum[18] = clip[11] + clip[9];
    //frustum[19] = clip[15] + clip[13];

    // Extract the TOP clipping plane
    frustum[20] = mvp(3,0) - mvp(1,0);
    frustum[21] = mvp(3,1) - mvp(1,1);
    frustum[22] = mvp(3,2) - mvp(1,2);
    frustum[23] = mvp(3,3) - mvp(1,3);
    //frustum[20] = clip[3] - clip[1];
    //frustum[21] = clip[7] - clip[5];
    //frustum[22] = clip[11] - clip[9];
    //frustum[23] = clip[15] - clip[13];
}

//determine mins maxs of patches and whole field.
void FormGPS::calculateMinMax()
{

    minFieldX = 9999999; minFieldY = 9999999;
    maxFieldX = -9999999; maxFieldY = -9999999;


    //min max of the boundary
    //min max of the boundary
    if (bnd.bndList.count() > 0)
    {
        int bndCnt = bnd.bndList[0].fenceLine.count();
        for (int i = 0; i < bndCnt; i++)
        {
            double x = bnd.bndList[0].fenceLine[i].easting;
            double y = bnd.bndList[0].fenceLine[i].northing;

            //also tally the max/min of field x and z
            if (minFieldX > x) minFieldX = x;
            if (maxFieldX < x) maxFieldX = x;
            if (minFieldY > y) minFieldY = y;
            if (maxFieldY < y) maxFieldY = y;
        }

    }
    else
    {
        //draw patches j= # of sections
        for (int j = 0; j < triStrip.count(); j++)
        {
            //every time the section turns off and on is a new patch
            int patchCount = triStrip[j].patchList.count();

            if (patchCount > 0)
            {
                //for every new chunk of patch
                for (QSharedPointer<PatchTriangleList> &triList: triStrip[j].patchList)
                {
                    int count2 = triList->count();
                    for (int i = 0; i < count2; i += 3)
                    {
                        double x = (*triList)[i].x();
                        double y = (*triList)[i].y();

                        //also tally the max/min of field x and z
                        if (minFieldX > x) minFieldX = x;
                        if (maxFieldX < x) maxFieldX = x;
                        if (minFieldY > y) minFieldY = y;
                        if (maxFieldY < y) maxFieldY = y;
                    }
                }
            }
        }
    }


    if (maxFieldX == -9999999 || minFieldX == 9999999 || maxFieldY == -9999999 || minFieldY == 9999999)
    {
        maxFieldX = 0; minFieldX = 0; maxFieldY = 0; minFieldY = 0;
    }
    else
    {
        //the largest distancew across field
        double dist = fabs(minFieldX - maxFieldX);
        double dist2 = fabs(minFieldY - maxFieldY);

        if (dist > dist2) maxFieldDistance = (dist);
        else maxFieldDistance = (dist2);

        if (maxFieldDistance < 100) maxFieldDistance = 100;
        if (maxFieldDistance > 19900) maxFieldDistance = 19900;
        //lblMax.Text = ((int)maxFieldDistance).ToString();

        fieldCenterX = (maxFieldX + minFieldX) / 2.0;
        fieldCenterY = (maxFieldY + minFieldY) / 2.0;
    }

    headland_form.setFieldInfo(maxFieldDistance,fieldCenterX,fieldCenterY);
    headache_form.setFieldInfo(maxFieldDistance,fieldCenterX,fieldCenterY);
}
