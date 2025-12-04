#ifndef CPATCHES_H
#define CPATCHES_H

#include <QSharedPointer>
#include <QVector>
#include <QVector3D>
#include <QObject>

typedef QVector<QVector3D> PatchTriangleList;

struct PatchBoundingBox {
    float minx;
    float miny;
    float maxx;
    float maxy;
};

class CFieldData;
class CTool;

class CPatches
{
public:
    //torriem: we use a QVector of QVector3D so that it's
    //more efficient to draw on openGL back buffer.

    //currently building list of patch data individual triangles
    QSharedPointer<PatchTriangleList> triangleList;
    QSharedPointer<PatchBoundingBox> triangleListBoundingBox;

    //list of the list of patch data individual triangles for that entire section activity
    QVector<QSharedPointer<PatchTriangleList>> patchList;
    QVector<QSharedPointer<PatchBoundingBox>> patchBoundingBoxList;

    //mapping
    bool isDrawing = false;

    //points in world space that start and end of section are in
    QVector3D leftPoint, rightPoint;

    int numTriangles = 0;
    int currentStartSectionNum, currentEndSectionNum;
    int newStartSectionNum, newEndSectionNum;

    CPatches();

    void TurnMappingOn(CTool &tool,
                       int j);
    void TurnMappingOff(CTool &tool,
                        CFieldData &fd,
                        QObject *mainWindow,
                        class FormGPS *formGPS);
    void AddMappingPoint(CTool &tool,
                         CFieldData &fd,
                         int j,
                         QObject *mainWindow,
                         class FormGPS *formGPS);





};

#endif // CPATCHES_H
