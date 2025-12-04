// Copyright (C) 2024 Michael Torrie and the QtAgOpenGPS Dev Team
// SPDX-License-Identifier: GNU General Public License v3.0 or later
//
#include "glutils.h"
#include <QThread>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include "ccamera.h"
#include <assert.h>
#include <math.h>
#include "classes/settingsmanager.h"
#include "glm.h"
#include <QPainter>

//module-level symbols
QOpenGLShaderProgram *simpleColorShader = 0;
QOpenGLShaderProgram *texShader = 0;
QOpenGLShaderProgram *interpColorShader = 0;
QOpenGLShaderProgram *simpleColorShaderBack = 0;
QOpenGLShaderProgram *texShaderBack = 0;

QVector<QOpenGLTexture *> texture;

static int GlyphsPerLine = 16;
//static int GlyphLineCount = 16;
static int GlyphWidth = 16;
static int GlyphHeight = 32;
static int CharXSpacing = 14;

int textureWidth;
int textureHeight;

#ifdef LOCAL_QML
#define PREFIX "local:"
#else
#define PREFIX ":/AOG"
#endif


bool isFontOn = true;

bool texturesLoaded = false;

void initializeBackShader() {
    //needs a valid GL context bound before calling.
    if (!simpleColorShaderBack) {
        simpleColorShaderBack = new QOpenGLShaderProgram(); //memory managed by Qt
        if (!simpleColorShaderBack->addShaderFromSourceFile(QOpenGLShader::Vertex, PREFIX "/shaders/color_vshader.vsh")) {
            qDebug() << "Vertex shader compile error:" << simpleColorShaderBack->log();
        }
        if (!simpleColorShaderBack->addShaderFromSourceFile(QOpenGLShader::Fragment, PREFIX "/shaders/color_fshader.fsh")) {
            qDebug() << "Fragment shader compile error:" << simpleColorShaderBack->log();
        }
        if (!simpleColorShaderBack->link()){
            qDebug() << "Shader link error::" << simpleColorShaderBack->log();
        }
    }
    if (!texShaderBack) {
        texShaderBack = new QOpenGLShaderProgram(); //memory managed by Qt
        if (!texShaderBack->addShaderFromSourceFile(QOpenGLShader::Vertex, PREFIX "/shaders/colortex_vshader.vsh")) {
            qDebug() << "Vertex shader compile error:" << texShader->log();
        }
        if (!texShaderBack->addShaderFromSourceFile(QOpenGLShader::Fragment, PREFIX "/shaders/colortex_fshader.fsh")) {
            qDebug() << "Fragment shader compile error:" << texShader->log();
        }
        texShaderBack->bindAttributeLocation("vertex", 0);
        texShaderBack->bindAttributeLocation("texcoord_src", 1);
        if (!texShaderBack->link()) {
            qDebug() << "Shader link error::" << texShader->log();
        }
    }
}

void initializeShaders() {
    QOpenGLContext *glContext = QOpenGLContext::currentContext();
    glContext->functions()->initializeOpenGLFunctions();

    //GL context must be bound by caller, and this must be called from
    //a QThread context

    //All shader memory will be managed by the current QThread
    if (!simpleColorShader) {
        simpleColorShader = new QOpenGLShaderProgram(); //memory managed by Qt
        if (!simpleColorShader->addShaderFromSourceFile(QOpenGLShader::Vertex, PREFIX "/shaders/color_vshader.vsh")) {
            qDebug() << "Vertex shader compile error:" << simpleColorShader->log();
        }
        if (!simpleColorShader->addShaderFromSourceFile(QOpenGLShader::Fragment, PREFIX "/shaders/color_fshader.fsh")) {
            qDebug() << "Fragment shader compile error:" << simpleColorShader->log();
        }
        simpleColorShader->bindAttributeLocation("vertex",0);
        if (!simpleColorShader->link()){
            qDebug() << "Shader link error::" << simpleColorShader->log();
        }
    }
    if (!texShader) {
        texShader = new QOpenGLShaderProgram(); //memory managed by Qt
        if (!texShader->addShaderFromSourceFile(QOpenGLShader::Vertex, PREFIX "/shaders/colortex_vshader.vsh")) {
            qDebug() << "Vertex shader compile error:" << texShader->log();
        }
        if (!texShader->addShaderFromSourceFile(QOpenGLShader::Fragment, PREFIX "/shaders/colortex_fshader.fsh")) {
            qDebug() << "Fragment shader compile error:" << texShader->log();
        }
        texShader->bindAttributeLocation("vertex", 0);
        texShader->bindAttributeLocation("texcoord_src", 1);
        if (!texShader->link()) {
            qDebug() << "Shader link error::" << texShader->log();
        }
    }
    if (!interpColorShader) {
        interpColorShader = new QOpenGLShaderProgram(); //memory managed by Qt
        if (!interpColorShader->addShaderFromSourceFile(QOpenGLShader::Vertex, PREFIX "/shaders/colors_vshader.vsh")){
            qDebug() << "Vertex shader compile error:" << interpColorShader->log();
        }
        if (!interpColorShader->addShaderFromSourceFile(QOpenGLShader::Fragment, PREFIX "/shaders/colors_fshader.fsh")) {
            qDebug() << "Fragment shader compile error:" << interpColorShader->log();
        }
        interpColorShader->bindAttributeLocation("vertex", 0);
        interpColorShader->bindAttributeLocation("color", 1);
        if (!interpColorShader->link()) {
            qDebug() << "Shader link error::" << interpColorShader->log();
        }
    }
}

void initializeTextures() {
    QOpenGLTexture *t;

    if (texture.size()) return; //textures already loaded.

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/floor.png"));
    t->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    texture.append(t); //FLOOR

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/Font.png"));
    textureWidth = t->width();
    textureHeight= t->height();
    texture.append(t); //FONT

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/Lift.png"));
    texture.append(t); //HYDIFT

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/z_Tractor.png"));
    texture.append(t); //TRACTOR

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/z_QuestionMark.png"));
    texture.append(t); //QUESTION_MARK

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/FrontWheels.png"));
    texture.append(t); //FRONT_WHEELS

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/Tractor4WDFront.png"));
    texture.append(t); //TRACTOR_4WD_FRONT

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/Tractor4WDRear.png"));
    texture.append(t); //TRACTOR_4WD_REAR

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/Harvester.png"));
    texture.append(t); //HARVESTER

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/z_Tool.png"));
    texture.append(t); //TOOLWHEELS

    t = new QOpenGLTexture(QImage(PREFIX "/images/textures/z_Tire.png"));
    texture.append(t); //TIRE

}

void destroyShaders() {
    //OpenGL context must be bound by caller.

    if(simpleColorShader) {
        delete simpleColorShader;
        simpleColorShader = 0;
    }

    if(texShader) {
        delete texShader;
        texShader = 0;
    }

    if(interpColorShader) {
        delete interpColorShader;
        interpColorShader = 0;
    }
}

void destroyTextures() {

    //TODO: compiler says this code is problematic.
    for(const QOpenGLTexture *t: texture) {
        delete t;
    }
    texture.clear();
}

void glDrawArraysColorBack(QOpenGLFunctions *gl,
                           QMatrix4x4 mvp,
                           GLenum operation,
                           QColor color,
                           QOpenGLBuffer &vertexBuffer,
                           GLenum GL_type,
                           int count,
                           float pointSize)
{
    //bind shader
    //assert(simpleColorShaderBack->bind());
     if (!simpleColorShaderBack->bind()) {
        qDebug() << "bind failed" << simpleColorShaderBack->log();
    }
    //set color
    simpleColorShaderBack->setUniformValue("color", color);
    //set mvp matrix
    simpleColorShaderBack->setUniformValue("mvpMatrix", mvp);

    simpleColorShaderBack->setUniformValue("pointSize", pointSize);


    vertexBuffer.bind();

    //TODO: these require a VBA to be bound; we need to create them I suppose.
    //enable the vertex attribute array in shader
    simpleColorShaderBack->enableAttributeArray("vertex");
    //use attribute array from buffer, using non-normalized vertices
    gl->glVertexAttribPointer(simpleColorShaderBack->attributeLocation("vertex"),
                              3, //3D vertices
                              GL_type, //type of data GL_FLAOT or GL_DOUBLE
                              GL_FALSE, //not normalized vertices!
                              0, //no spaceing between vertices in data
                              0 //start at offset 0 in buffer
                             );

    //draw primitive
    gl->glDrawArrays(operation,0,count);
    //release buffer
    vertexBuffer.release();
    //release shader
    simpleColorShaderBack->release();
}

void glDrawArraysColor(QOpenGLFunctions *gl,
                       QMatrix4x4 mvp,
                       GLenum operation,
                       QColor color,
                       QOpenGLBuffer &vertexBuffer,
                       GLenum GL_type,
                       int count,
                       float pointSize)
{
    //bind shader
    if (!simpleColorShader->bind()) {
        qDebug() << "bind failed" << simpleColorShader->log();
    }
    //set color
    simpleColorShader->setUniformValue("color", color);
    //set mvp matrix
    simpleColorShader->setUniformValue("mvpMatrix", mvp);

    simpleColorShader->setUniformValue("pointSize", pointSize);


    vertexBuffer.bind();

    //TODO: these require a VBA to be bound; we need to create them I suppose.
    //enable the vertex attribute array in shader
    simpleColorShader->enableAttributeArray("vertex");
    //use attribute array from buffer, using non-normalized vertices
    gl->glVertexAttribPointer(simpleColorShader->attributeLocation("vertex"),
                              3, //3D vertices
                              GL_type, //type of data GL_FLAOT or GL_DOUBLE
                              GL_FALSE, //not normalized vertices!
                              0, //no spaceing between vertices in data
                              0 //start at offset 0 in buffer
                             );

    //draw primitive
    gl->glDrawArrays(operation,0,count);
    //release buffer
    vertexBuffer.release();
    //release shader
    simpleColorShader->release();
}

//Buffer should be a list of 7D tuples.  3 values for x,y,z,
//and 4 values for color: r,g,b,a.
void glDrawArraysColors(QOpenGLFunctions *gl,
                        QMatrix4x4 mvp,
                        GLenum operation,
                        QOpenGLBuffer &vertexBuffer,
                        GLenum GL_type,
                        int count,
                        float pointSize)
{
    //bind shader
    //assert(interpColorShader->bind());
    if (!interpColorShader->bind()) {
        qDebug() << "bind failed" << interpColorShader->log();
    }
    //set mvp matrix
    interpColorShader->setUniformValue("mvpMatrix", mvp);

    interpColorShader->setUniformValue("pointSize", pointSize);


    vertexBuffer.bind();

    //enable the vertex attribute array in shader
    interpColorShader->enableAttributeArray("vertex");
    interpColorShader->enableAttributeArray("color");

    //use attribute array from buffer, using non-normalized vertices
    gl->glVertexAttribPointer(interpColorShader->attributeLocation("vertex"),
                              3, //3D vertices
                              GL_type, //type of data GL_FLAOT or GL_DOUBLE
                              GL_FALSE, //not normalized vertices!
                              7*sizeof(float), //vertex+color
                              0 //start at offset 0 in buffer
                             );

    gl->glVertexAttribPointer(interpColorShader->attributeLocation("color"),
                              4, //4D color
                              GL_type, //type of data GL_FLAOT or GL_DOUBLE
                              GL_FALSE, //not normalized vertices!
                              7*sizeof(float), //vertex+color
                              (void *)(sizeof(float) * 3) //start at 3rd float in buffer
                             );

    //draw primitive
    gl->glDrawArrays(operation,0,count);
    //release buffer
    vertexBuffer.release();
    //release shader
    interpColorShader->release();
}

//Buffer should consist of 5D values.  3D for x,y,z, and 2D for
//texture x,y coordinate.
void glDrawArraysTexture(QOpenGLFunctions *gl,
                         QMatrix4x4 mvp,
                         GLenum operation,
                         QOpenGLBuffer &vertexBuffer,
                         GLenum GL_type,
                         int count,
                         bool useColor = false,
                         QColor color = QColor::fromRgbF(1,1,1))
{
    //gl->glEnable(GL_TEXTURE_2D);
    //bind shader
    //assert(texShader->bind());
    if (!texShader->bind()) {
        qDebug() << "bind failed" << texShader->log();
    }
    //set mvp matrix
    texShader->setUniformValue("color", color);
    texShader->setUniformValue("texture", 0);
    texShader->setUniformValue("mvpMatrix", mvp);
    texShader->setUniformValue("useColor", useColor);


    vertexBuffer.bind();

    //enable the vertex attribute array in shader
    texShader->enableAttributeArray("vertex");
    texShader->enableAttributeArray("texcoord_src");

    //use attribute array from buffer, using non-normalized vertices
    gl->glVertexAttribPointer(texShader->attributeLocation("vertex"),
                              3, //3D vertices
                              GL_type, //type of data GL_FLAOT or GL_DOUBLE
                              GL_FALSE, //not normalized vertices!
                              5*sizeof(float), //vertex+color
                              0 //start at offset 0 in buffer
                             );

    gl->glVertexAttribPointer(texShader->attributeLocation("texcoord_src"),
                              2, //2D coordinate
                              GL_type, //type of data GL_FLAOT or GL_DOUBLE
                              GL_FALSE, //not normalized vertices!
                              5*sizeof(float), //vertex+color
                              (void *)(sizeof(float) * 3) //start at 3rd float in buffer
                             );

    //draw primitive
    gl->glDrawArrays(operation,0,count);
    //release buffer
    vertexBuffer.release();
    //release shader
    texShader->release();
    //gl->glDisable(GL_TEXTURE_2D);
}

//Buffer should consist of 5D values.  3D for x,y,z, and 2D for
//texture x,y coordinate.
void glDrawArraysTextureBack(QOpenGLFunctions *gl,
                         QMatrix4x4 mvp,
                         GLenum operation,
                         QOpenGLBuffer &vertexBuffer,
                         GLenum GL_type,
                         int count,
                         bool useColor = false,
                         QColor color = QColor::fromRgbF(1,1,1))
{
    //gl->glEnable(GL_TEXTURE_2D);
    //bind shader
    //assert(texShader->bind());
    if (!texShaderBack->bind()) {
        qDebug() << "bind failed" << texShaderBack->log();
    }
    //set mvp matrix
    texShaderBack->setUniformValue("color", color);
    texShaderBack->setUniformValue("texture", 0);
    texShaderBack->setUniformValue("mvpMatrix", mvp);
    texShaderBack->setUniformValue("useColor", useColor);


    vertexBuffer.bind();

    //enable the vertex attribute array in shader
    texShaderBack->enableAttributeArray("vertex");
    texShaderBack->enableAttributeArray("texcoord_src");

    //use attribute array from buffer, using non-normalized vertices
    gl->glVertexAttribPointer(texShaderBack->attributeLocation("vertex"),
                              3, //3D vertices
                              GL_type, //type of data GL_FLAOT or GL_DOUBLE
                              GL_FALSE, //not normalized vertices!
                              5*sizeof(float), //vertex+color
                              0 //start at offset 0 in buffer
                             );

    gl->glVertexAttribPointer(texShaderBack->attributeLocation("texcoord_src"),
                              2, //2D coordinate
                              GL_type, //type of data GL_FLAOT or GL_DOUBLE
                              GL_FALSE, //not normalized vertices!
                              5*sizeof(float), //vertex+color
                              (void *)(sizeof(float) * 3) //start at 3rd float in buffer
                             );

    //draw primitive
    gl->glDrawArrays(operation,0,count);
    //release buffer
    vertexBuffer.release();
    //release shader
    texShaderBack->release();
    //gl->glDisable(GL_TEXTURE_2D);
}

void DrawPolygon(QOpenGLFunctions *gl, QMatrix4x4 mvp, QVector<Vec2> &polygon, float size, QColor color)
{
    GLHelperOneColor gldraw;
    if (polygon.count() > 2)
    {
        for (int i = 0; i < polygon.count() ; i++)
        {
            gldraw.append(QVector3D(polygon[i].easting, polygon[i].northing, 0));
        }
        gldraw.draw(gl, mvp, color, GL_LINE_LOOP, size);
    }
}

void DrawPolygon(QOpenGLFunctions *gl, QMatrix4x4 mvp, QVector<Vec3> &polygon, float size, QColor color)
{
    GLHelperOneColor gldraw;
    if (polygon.count() > 2)
    {
        for (int i = 0; i < polygon.count() ; i++)
        {
            gldraw.append(QVector3D(polygon[i].easting, polygon[i].northing, 0));
        }
        gldraw.draw(gl, mvp, color, GL_LINE_LOOP, size);
    }
}

void DrawPolygonBack(QOpenGLFunctions *gl, QMatrix4x4 mvp, QVector<Vec2> &polygon, float size, QColor color)
{
    GLHelperOneColorBack gldraw;
    if (polygon.count() > 2)
    {
        for (int i = 0; i < polygon.count() ; i++)
        {
            gldraw.append(QVector3D(polygon[i].easting, polygon[i].northing, 0));
        }
        gldraw.draw(gl, mvp, color, GL_LINE_LOOP, size);
    }
}

void DrawPolygonBack(QOpenGLFunctions *gl, QMatrix4x4 mvp, QVector<Vec3> &polygon, float size, QColor color)
{
    GLHelperOneColorBack gldraw;
    if (polygon.count() > 2)
    {
        for (int i = 0; i < polygon.count() ; i++)
        {
            gldraw.append(QVector3D(polygon[i].easting, polygon[i].northing, 0));
        }
        gldraw.draw(gl, mvp, color, GL_LINE_LOOP, size);
    }
}

void DrawPolygonBack(QPainter &painter, QMatrix4x4 mvp, QVector<Vec2> &polygon, float size, QColor color)
{
    if (polygon.count() > 2)
    {
        QPolygonF p;
        for (int i = 0; i < polygon.count() ; i++)
        {
            p.append(glm::backbuffer_world_to_screen(mvp, polygon[i]));
        }
        QPen pen(color);
        pen.setWidth(size);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(p);
    }
}

void DrawPolygonBack(QPainter &painter, QMatrix4x4 mvp, QVector<Vec3> &polygon, float size, QColor color)
{
    if (polygon.count() > 2)
    {
        QPolygonF p;

        for (int i = 0; i < polygon.count() ; i++)
        {
            p.append(glm::backbuffer_world_to_screen(mvp, polygon[i]));
        }
        QPen pen(color);
        pen.setWidth(size);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(p);
    }
}

void drawText(QOpenGLFunctions *gl, QMatrix4x4 mvp, double x, double y, QString text, double size, bool colorize, QColor color)
{
    //GL.Color3(0.95f, 0.95f, 0.40f);

    GLHelperTexture gldraw;
    VertexTexcoord vt;

    double u_step = (double)GlyphWidth / (double)textureWidth;
    double v_step = (double)GlyphHeight / (double)textureHeight;

    for (int n = 0; n < text.length(); n++)
    {
        gldraw.clear();
        char idx = text.at(n).toLatin1();
        double u = (double)(idx % GlyphsPerLine) * u_step;
        double v = (double)(idx / GlyphsPerLine) * v_step;


        //1
        vt.texcoord = QVector2D(u, v + v_step);
        vt.vertex = QVector3D(x, y + GlyphHeight * size, 0);
        gldraw.append(vt);

        //2
        vt.texcoord = QVector2D(u + u_step, v + v_step);
        vt.vertex = QVector3D(x + GlyphWidth * size, y + GlyphHeight * size, 0);
        gldraw.append(vt);

        //4
        vt.texcoord = QVector2D(u, v);
        vt.vertex = QVector3D(x, y, 0);
        gldraw.append(vt);

        //4
        vt.texcoord = QVector2D(u, v);
        vt.vertex = QVector3D(x, y, 0);
        gldraw.append(vt);

        //2
        vt.texcoord = QVector2D(u + u_step, v + v_step);
        vt.vertex = QVector3D(x + GlyphWidth * size, y + GlyphHeight * size, 0);
        gldraw.append(vt);

        //3
        vt.texcoord = QVector2D(u + u_step, v);
        vt.vertex = QVector3D(x + GlyphWidth * size, y, 0);
        gldraw.append(vt);


        x += CharXSpacing * size;
    }

    gldraw.draw(gl, mvp, Textures::FONT, GL_TRIANGLE_STRIP, colorize, color);

}



void drawText3D(const CCamera &camera, QOpenGLFunctions *gl,
                QMatrix4x4 mvp, double x1, double y1, QString text,
                double size, bool colorize, QColor color)
{
    GLHelperTexture gldraw;
    VertexTexcoord vt;

    double x = 0, y = 0;

    mvp.translate(x1, y1, 0);

    if (SettingsManager::instance()->display_camPitch() < -45)
    {
        mvp.rotate(90, 1, 0, 0);
        if (camera.camFollowing) mvp.rotate(-camera.camHeading, 0, 1, 0);
        size = -camera.camSetDistance;
        size = pow(size, 0.8);
        size /= 300;
    }

    else
    {
        if (camera.camFollowing) mvp.rotate(-camera.camHeading, 0, 0, 1);
        size = -camera.camSetDistance;
        size = pow(size, 0.85);
        size /= 500;
    }

    double u_step = (double)GlyphWidth / (double)textureWidth;
    double v_step = (double)GlyphHeight / (double)textureHeight;


    for (int n = 0; n < text.length(); n++)
    {
        char idx = text.at(n).toLatin1();
        double u = (double)(idx % GlyphsPerLine) * u_step;
        double v = (double)(idx / GlyphsPerLine) * v_step;
        //gldraw.clear();

        //1
        vt.texcoord = QVector2D(u, v + v_step);
        vt.vertex = QVector3D(x, y, 0);
        gldraw.append(vt);

        //2
        vt.texcoord = QVector2D(u + u_step, v + v_step);
        vt.vertex = QVector3D(x + GlyphWidth * size, y, 0);
        gldraw.append(vt);

        //4
        vt.texcoord = QVector2D(u, v);
        vt.vertex = QVector3D(x, y + GlyphHeight * size, 0);
        gldraw.append(vt);

        //4
        vt.texcoord = QVector2D(u, v);
        vt.vertex = QVector3D(x, y + GlyphHeight * size, 0);
        gldraw.append(vt);

        //2
        vt.texcoord = QVector2D(u + u_step, v + v_step);
        vt.vertex = QVector3D(x + GlyphWidth * size, y, 0);
        gldraw.append(vt);

        //3
        vt.texcoord = QVector2D(u + u_step, v);
        vt.vertex = QVector3D(x + GlyphWidth * size, y + GlyphHeight * size, 0);
        gldraw.append(vt);


        x += CharXSpacing * size;
    }
    gldraw.draw(gl, mvp, Textures::FONT, GL_TRIANGLES, colorize, color);

}

void drawTextVehicle(const CCamera &camera, QOpenGLFunctions *gl, QMatrix4x4 mvp,
                     double x, double y, QString text, double size, bool colorize, QColor color)
{
    GLHelperTexture gldraw;
    VertexTexcoord vt;

    size *= -camera.camSetDistance;
    size = pow(size, 0.8)/800;

    //2d
    if (SettingsManager::instance()->display_camPitch() < -58)
    {
        if (!camera.camFollowing)
        {
            mvp.rotate(camera.camHeading, 0, 0, 1);
            y *= 1.2;
        }
        else
        {
            y *= 1.2;
            mvp.translate(x, y, 0);
            x = y = 0;
        }
    }
    //3d
    else
    {
        if (!camera.camFollowing)
        {
            mvp.rotate(90, 1, 0, 0);
            mvp.rotate(camera.camHeading, 0, 1, 0);
            y *= 0.3;
        }
        else
        {
            mvp.rotate(- SettingsManager::instance()->display_camPitch(), 1, 0, 0);
            y *= 0.3;
        }
    }

    double u_step = (double)GlyphWidth / (double)textureWidth;
    double v_step = (double)GlyphHeight / (double)textureHeight;


    for (int n = 0; n < text.length(); n++)
    {
        char idx = text.at(n).toLatin1();
        double u = (double)(idx % GlyphsPerLine) * u_step;
        double v = (double)(idx / GlyphsPerLine) * v_step;

        //1
        vt.texcoord = QVector2D(u, v + v_step);
        vt.vertex = QVector3D(x, y, 0);
        gldraw.append(vt);

        //2
        vt.texcoord = QVector2D(u + u_step, v + v_step);
        vt.vertex = QVector3D(x + GlyphWidth * size, y, 0);
        gldraw.append(vt);

        //4
        vt.texcoord = QVector2D(u, v);
        vt.vertex = QVector3D(x, y + GlyphHeight * size, 0);
        gldraw.append(vt);

        //4
        vt.texcoord = QVector2D(u, v);
        vt.vertex = QVector3D(x, y + GlyphHeight * size, 0);
        gldraw.append(vt);

        //2
        vt.texcoord = QVector2D(u + u_step, v + v_step);
        vt.vertex = QVector3D(x + GlyphWidth * size, y, 0);
        gldraw.append(vt);

        //3
        vt.texcoord = QVector2D(u + u_step, v);
        vt.vertex = QVector3D(x + GlyphWidth * size, y + GlyphHeight * size, 0);
        gldraw.append(vt);


        x += CharXSpacing * size;
    }
    gldraw.draw(gl, mvp, Textures::FONT, GL_TRIANGLES, colorize, color);
}

GLHelperOneColorBack::GLHelperOneColorBack() {
}

void GLHelperOneColorBack::draw(QOpenGLFunctions *gl, QMatrix4x4 mvp, QColor color, GLenum operation, float point_size) {
    QOpenGLBuffer vertexBuffer;

    vertexBuffer.create();
    vertexBuffer.bind();
    vertexBuffer.allocate(data(), size()*sizeof(QVector3D));
    vertexBuffer.release();


    glDrawArraysColorBack(gl, mvp,operation,
                      color, vertexBuffer, GL_FLOAT,
                      size(),point_size);

}

GLHelperOneColor::GLHelperOneColor() {
}

void GLHelperOneColor::draw(QOpenGLFunctions *gl, QMatrix4x4 mvp, QColor color, GLenum operation, float point_size) {
    QOpenGLBuffer vertexBuffer;

    vertexBuffer.create();
    vertexBuffer.bind();
    vertexBuffer.allocate(data(), size()*sizeof(QVector3D));
    vertexBuffer.release();


    glDrawArraysColor(gl, mvp,operation,
                      color, vertexBuffer, GL_FLOAT,
                      size(),point_size);

}

GLHelperColors::GLHelperColors() {
}

void GLHelperColors::draw(QOpenGLFunctions *gl, QMatrix4x4 mvp, GLenum operation, float point_size) {
    QOpenGLBuffer vertexBuffer;

    vertexBuffer.create();
    vertexBuffer.bind();
    vertexBuffer.allocate(data(), size()*sizeof(ColorVertex));
    vertexBuffer.release();


    glDrawArraysColors(gl, mvp,operation,
                       vertexBuffer, GL_FLOAT,
                       size(),point_size);

}

GLHelperTexture::GLHelperTexture() {

}

void GLHelperTexture::draw(QOpenGLFunctions *gl, QMatrix4x4 mvp, GLenum operation, bool colorize, QColor color)
{
    //assume texture is already bound
    QOpenGLBuffer vertexBuffer;
    vertexBuffer.create();
    vertexBuffer.bind();
    vertexBuffer.allocate(data(),size() * sizeof(VertexTexcoord));
    vertexBuffer.release();

    glDrawArraysTexture(gl, mvp, operation, vertexBuffer, GL_FLOAT,size(),
                        colorize, color);
}

void GLHelperTexture::draw(QOpenGLFunctions *gl, QMatrix4x4 mvp, Textures textureno, GLenum operation, bool colorize, QColor color)
{
    QOpenGLBuffer vertexBuffer;
    vertexBuffer.create();
    vertexBuffer.bind();
    vertexBuffer.allocate(data(),size() * sizeof(VertexTexcoord));
    vertexBuffer.release();

    texture[textureno]->bind();
    glDrawArraysTexture(gl, mvp, operation, vertexBuffer, GL_FLOAT,size(),
                        colorize, color);
    texture[textureno]->release();
}

GLHelperTextureBack::GLHelperTextureBack() {

}

void GLHelperTextureBack::draw(QOpenGLFunctions *gl, QMatrix4x4 mvp, GLenum operation, bool colorize, QColor color)
{
    //assume texture is already bound
    QOpenGLBuffer vertexBuffer;
    vertexBuffer.create();
    vertexBuffer.bind();
    vertexBuffer.allocate(data(),size() * sizeof(VertexTexcoord));
    vertexBuffer.release();

    glDrawArraysTextureBack(gl, mvp, operation, vertexBuffer, GL_FLOAT,size(),
                        colorize, color);
}
