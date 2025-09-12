#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QVector>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include "glutils.h"
#include "ctrack.h"
#include "cvehicle.h"
#include "glm.h"
#include "cabcurve.h"
#include "cabline.h"
#include "newsettings.h"
#include "cyouturn.h"
#include "cboundary.h"
#include "ctram.h"
#include "ccamera.h"
#include "cahrs.h"
#include "cguidance.h"
#include "cworldgrid.h"

CTrk::CTrk()
{
    heading = 3;
}

CTrk::CTrk(const CTrk &orig)
{
    curvePts = orig.curvePts;
    heading = orig.heading;
    name = orig.name;
    isVisible = orig.isVisible;
    ptA = orig.ptA;
    ptB = orig.ptB;
    endPtA = orig.endPtA;
    endPtB = orig.endPtB;
    mode = orig.mode;
    nudgeDistance = orig.nudgeDistance;
}

CTrack::CTrack(QObject* parent) : QAbstractListModel(parent)
{
    // Initialize role names
    m_roleNames[index] = "index";
    m_roleNames[NameRole] = "name";
    m_roleNames[IsVisibleRole] = "isVisible";
    m_roleNames[ModeRole] = "mode";
    m_roleNames[ptA] = "ptA";
    m_roleNames[ptB] = "ptB";
    m_roleNames[endPtA] = "endPtA";
    m_roleNames[endPtB] = "endPtB";
    m_roleNames[nudgeDistance] = "nudgeDistance";

    setIdx(-1);
}

CTrack::~CTrack()
{
    idx = -1;
    gArr.clear();
    reloadModel();
}

int CTrack::FindClosestRefTrack(Vec3 pivot, const CVehicle &vehicle)
{
    if (idx < 0 || gArr.count() == 0) return -1;

    //only 1 track
    if (gArr.count() == 1) return idx;

    int trak = -1;
    int cntr = 0;

    //Count visible
    for (int i = 0; i < gArr.count(); i++)
    {
        if (gArr[i].isVisible)
        {
            cntr++;
            trak = i;
        }
    }

    //only 1 track visible of the group
    if (cntr == 1) return trak;

    //no visible tracks
    if (cntr == 0) return -1;

    //determine if any aligned reasonably close
    bool isAlignedArr[gArr.count()];
    for (int i = 0; i < gArr.count(); i++)
    {
        if (gArr[i].mode == (int)TrackMode::Curve) isAlignedArr[i] = true;
        else
        {
            double diff = M_PI - fabs(fabs(pivot.heading - gArr[i].heading) - M_PI);
            if ((diff < 1) || (diff > 2.14))
                isAlignedArr[i] = true;
            else
                isAlignedArr[i] = false;
        }
    }

    double minDistA = glm::DOUBLE_MAX;
    double dist;

    Vec2 endPtA, endPtB;

    for (int i = 0; i < gArr.count(); i++)
    {
        if (!isAlignedArr[i]) continue;
        if (!gArr[i].isVisible) continue;

        if (gArr[i].mode == (int)TrackMode::AB)
        {
            double abHeading = gArr[i].heading;

            endPtA.easting = gArr[i].ptA.easting - (sin(abHeading) * 2000);
            endPtA.northing = gArr[i].ptA.northing - (cos(abHeading) * 2000);

            endPtB.easting = gArr[i].ptB.easting + (sin(abHeading) * 2000);
            endPtB.northing = gArr[i].ptB.northing + (cos(abHeading) * 2000);

            //x2-x1
            double dx = endPtB.easting - endPtA.easting;
            //z2-z1
            double dy = endPtB.northing - endPtA.northing;

            dist = ((dy * vehicle.steerAxlePos.easting) - (dx * vehicle.steerAxlePos.northing) +
                    (endPtB.easting * endPtA.northing) - (endPtB.northing * endPtA.easting))
                   / sqrt((dy * dy) + (dx * dx));

            dist *= dist;

            if (dist < minDistA)
            {
                minDistA = dist;
                trak = i;
            }
        }
        else
        {
            for (int j = 0; j < gArr[i].curvePts.count(); j ++)
            {

                dist = glm::DistanceSquared(gArr[i].curvePts[j], pivot);

                if (dist < minDistA)
                {
                    minDistA = dist;
                    trak = i;
                }
            }
        }
    }

    return trak;
}

void CTrack::SwitchToClosestRefTrack(Vec3 pivot, const CVehicle &vehicle)
{
    int new_idx;

    new_idx = FindClosestRefTrack(pivot, vehicle);
    if (new_idx >= 0 && new_idx != idx) {
        setIdx(new_idx);
        curve.isCurveValid = false;
        ABLine.isABValid = false;
    }
}

void CTrack::NudgeTrack(double dist)
{
    if (idx > -1)
    {
        if (gArr[idx].mode == (int)TrackMode::AB)
        {
            ABLine.isABValid = false;
            ABLine.lastSecond = 0;
            gArr[idx].nudgeDistance += ABLine.isHeadingSameWay ? dist : -dist;
        }
        else
        {
            curve.isCurveValid = false;
            curve.lastHowManyPathsAway = 98888;
            curve.lastSecond = 0;
            gArr[idx].nudgeDistance += curve.isHeadingSameWay ? dist : -dist;
        }

        //if (gArr[idx].nudgeDistance > 0.5 * mf.tool.width) gArr[idx].nudgeDistance -= mf.tool.width;
        //else if (gArr[idx].nudgeDistance < -0.5 * mf.tool.width) gArr[idx].nudgeDistance += mf.tool.width;
    }
}

void CTrack::NudgeDistanceReset()
{
    if (idx > -1 && gArr.count() > 0)
    {
        if (gArr[idx].mode == (int)TrackMode::AB)
        {
            ABLine.isABValid = false;
            ABLine.lastSecond = 0;
        }
        else
        {
            curve.isCurveValid = false;
            curve.lastHowManyPathsAway = 98888;
            curve.lastSecond = 0;
        }

        gArr[idx].nudgeDistance = 0;
    }
}

void CTrack::SnapToPivot()
{
    //if (isBtnGuidanceOn)

    if (idx > -1)
    {
        if (gArr[idx].mode == (int)(TrackMode::AB))
        {
            NudgeTrack(ABLine.distanceFromCurrentLinePivot);

        }
        else
        {
            NudgeTrack(curve.distanceFromCurrentLinePivot);
        }

    }
}

void CTrack::NudgeRefTrack(double dist)
{
    if (idx > -1)
    {
        if (gArr[idx].mode == (int)TrackMode::AB)
        {
            ABLine.isABValid = false;
            ABLine.lastSecond = 0;
            NudgeRefABLine( gArr[idx], ABLine.isHeadingSameWay ? dist : -dist);
        }
        else
        {
            curve.isCurveValid = false;
            curve.lastHowManyPathsAway = 98888;
            curve.lastSecond = 0;
            NudgeRefCurve( gArr[idx], curve.isHeadingSameWay ? dist : -dist);
        }
    }
}

void CTrack::NudgeRefABLine(CTrk &track, double dist)
{
    double head = track.heading;

    track.ptA.easting += (sin(head+glm::PIBy2) * (dist));
    track.ptA.northing += (cos(head + glm::PIBy2) * (dist));

    track.ptB.easting += (sin(head + glm::PIBy2) * (dist));
    track.ptB.northing += (cos(head + glm::PIBy2) * (dist));
}

void CTrack::NudgeRefCurve(CTrk &track, double distAway)
{
    curve.isCurveValid = false;
    curve.lastHowManyPathsAway = 98888;
    curve.lastSecond = 0;

    QVector<Vec3> curList;

    double distSqAway = (distAway * distAway) - 0.01;
    Vec3 point;

    for (int i = 0; i < track.curvePts.count(); i++)
    {
        point = Vec3(
            track.curvePts[i].easting + (sin(glm::PIBy2 + track.curvePts[i].heading) * distAway),
            track.curvePts[i].northing + (cos(glm::PIBy2 + track.curvePts[i].heading) * distAway),
            track.curvePts[i].heading);
        bool Add = true;

        for (int t = 0; t < track.curvePts.count(); t++)
        {
            double dist = ((point.easting - track.curvePts[t].easting) * (point.easting - track.curvePts[t].easting))
            + ((point.northing - track.curvePts[t].northing) * (point.northing - track.curvePts[t].northing));
            if (dist < distSqAway)
            {
                Add = false;
                break;
            }
        }

        if (Add)
        {
            if (curList.count() > 0)
            {
                double dist = ((point.easting - curList[curList.count() - 1].easting) * (point.easting - curList[curList.count() - 1].easting))
                + ((point.northing - curList[curList.count() - 1].northing) * (point.northing - curList[curList.count() - 1].northing));
                if (dist > 1.0)
                    curList.append(point);
            }
            else curList.append(point);
        }
    }

    int cnt = curList.count();
    if (cnt > 6)
    {
        QVector<Vec3> arr = curList;
        curList.clear();

        for (int i = 0; i < (arr.count() - 1); i++)
        {
            arr[i].heading = atan2(arr[i + 1].easting - arr[i].easting, arr[i + 1].northing - arr[i].northing);
            if (arr[i].heading < 0) arr[i].heading += glm::twoPI;
            if (arr[i].heading >= glm::twoPI) arr[i].heading -= glm::twoPI;
        }

        arr[arr.count() - 1].heading = arr[arr.count() - 2].heading;

        //replace the array
        cnt = arr.count();
        double distance;
        double spacing = 1.2;

        //add the first point of loop - it will be p1
        curList.append(arr[0]);

        for (int i = 0; i < cnt - 3; i++)
        {
            // add p2
            curList.append(arr[i + 1]);

            distance = glm::Distance(arr[i + 1], arr[i + 2]);

            if (distance > spacing)
            {
                int loopTimes = (int)(distance / spacing + 1);
                for (int j = 1; j < loopTimes; j++)
                {
                    Vec3 pos(glm::Catmull(j / (double)(loopTimes), arr[i], arr[i + 1], arr[i + 2], arr[i + 3]));
                    curList.append(pos);
                }
            }
        }

        curList.append(arr[cnt - 2]);
        curList.append(arr[cnt - 1]);

        curve.CalculateHeadings(curList);

        track.curvePts = curList;
        //track.curvePts.clear();

        //for (auto item: curList)
        //{
        //    track.curvePts.append(new vec3(item));
        //}

        //for (int i = 0; i < cnt; i++)
        //{
        //    arr[i].easting += cos(arr[i].heading) * (dist);
        //    arr[i].northing -= sin(arr[i].heading) * (dist);
        //    track.curvePts.append(arr[i]);
        //}
    }
}

void CTrack::DrawTrackNew(QOpenGLFunctions *gl, const QMatrix4x4 &mvp, const CCamera &camera, const CVehicle &vehicle)
{
    GLHelperOneColor gldraw;
    QColor color;
    double lineWidth = settings->value(SETTINGS_display_lineWidth).value<double>();

    if (ABLine.isMakingABLine && getNewMode() == TrackMode::AB) {
        ABLine.DrawABLineNew(gl, mvp, camera);

        gldraw.append(QVector3D(designRefLine[0].easting, designRefLine[0].northing, 0.0));
        gldraw.append(QVector3D(designRefLine[1].easting, designRefLine[1].northing, 0.0));
        color.setRgbF(0.930f, 0.4f, 0.4f);
        gldraw.draw(gl, mvp, color, GL_LINES, lineWidth);

    } else if (curve.isMakingCurve && getNewMode() == TrackMode::Curve) {
        curve.DrawCurveNew(gl, mvp);
    }
}

void CTrack::DrawTrack(QOpenGLFunctions *gl,
                       const QMatrix4x4 &mvp,
                       bool isFontOn, bool isRateMapOn,
                       CYouTurn &yt,
                       const CCamera &camera,
                       const CGuidance &gyd)
{
    if (idx >= 0) {
        if (gArr[idx].mode == TrackMode::AB)
            ABLine.DrawABLines(gl, mvp, isFontOn, isRateMapOn, gArr[idx], yt, camera, gyd);
        else if (gArr[idx].mode == TrackMode::Curve)
            curve.DrawCurve(gl, mvp, isFontOn, gArr[idx], yt, camera);
    }
}

void CTrack::DrawTrackGoalPoint(QOpenGLFunctions *gl,
                                const QMatrix4x4 &mvp)
{
    GLHelperOneColor gldraw1;
    QColor color;

    if (idx >= 0) {
        color.setRgbF(0.98, 0.98, 0.098);
        if (gArr[idx].mode == TrackMode::AB) {
            gldraw1.append(QVector3D(ABLine.goalPointAB.easting, ABLine.goalPointAB.northing, 0));
            gldraw1.draw(gl,mvp,QColor::fromRgbF(0,0,0),GL_POINTS,16);
            gldraw1.draw(gl,mvp,color,GL_POINTS,10);
        } else if (gArr[idx].mode == TrackMode::Curve) {
            gldraw1.append(QVector3D(curve.goalPointCu.easting, curve.goalPointCu.northing, 0));
            gldraw1.draw(gl,mvp,QColor::fromRgbF(0,0,0),GL_POINTS,16);
            gldraw1.draw(gl,mvp,color,GL_POINTS,10);
        }
    }
}

void CTrack::BuildCurrentLine(Vec3 pivot, double secondsSinceStart,
                              bool isBtnAutoSteerOn,
                              CYouTurn &yt,
                              CVehicle &vehicle,
                              const CBoundary &bnd,
                              const CAHRS &ahrs,
                              CGuidance &gyd,
                              CNMEA &pn)
{
    if (gArr.count() > 0 && idx > -1)
    {
        if (gArr[idx].mode == TrackMode::AB)
        {
            ABLine.BuildCurrentABLineList(pivot,secondsSinceStart,gArr[idx],yt,vehicle);

            ABLine.GetCurrentABLine(pivot, vehicle.steerAxlePos,isBtnAutoSteerOn,vehicle,yt,ahrs,gyd,pn);
        }
        else
        {
            //build new current ref line if required
            curve.BuildCurveCurrentList(pivot, secondsSinceStart,vehicle,gArr[idx],bnd,yt);

            curve.GetCurrentCurveLine(pivot, vehicle.steerAxlePos,isBtnAutoSteerOn,vehicle,gArr[idx],yt,ahrs,gyd,pn);
        }
    }
    emit howManyPathsAwayChanged(); //notify QML property is changed
}

void CTrack::ResetCurveLine()
{
    if (idx >=0 && gArr[idx].mode == TrackMode::Curve) {
        curve.curList.clear();
        setIdx(-1);
    }
}

void CTrack::AddPathPoint(Vec3 point)
{
    if (curve.isMakingCurve) {
        curve.desList.append(point);
    } else if (ABLine.isMakingABLine && !ABLine.isDesPtBSet) {
        //set the B point to current so we can draw a preview line
        ABLine.desPtB.easting = point.easting;
        ABLine.desPtB.northing = point.northing;

        update_ab_refline();
    }
}

int CTrack::getHowManyPathsAway()
{
    if (idx >= 0) {
        if (gArr[idx].mode == TrackMode::AB)
            return ABLine.howManyPathsAway;
        else
            return curve.howManyPathsAway;
    }

    return 0;
}

void CTrack::setIdx(int new_idx)
{
    if (new_idx < gArr.count()) {
        idx = new_idx;
        emit idxChanged();
        emit modeChanged();
        emit currentNameChanged();
    }
}

int CTrack::getNewMode(void)
{
    return newTrack.mode;
}

void CTrack::setNewMode(TrackMode new_mode)
{
    newTrack.mode = new_mode;
    emit newModeChanged();
}

QString CTrack::getNewName(void)
{
    if (getNewMode() == TrackMode::AB)
        return ABLine.desName;
    else
        return curve.desName;
}

void CTrack::setNewName(QString new_name)
{
    if (getNewMode() == TrackMode::AB)
        ABLine.desName = new_name;
    else
        curve.desName= new_name;

    emit newNameChanged();
}

int CTrack::getNewRefSide()
{
    return newRefSide;
}

void CTrack::setNewRefSide(int which_side)
{
    if (newRefSide != which_side) {
        newRefSide = which_side;
        if (getNewMode() == TrackMode::AB)
            update_ab_refline();

        emit newRefSideChanged();
    }
}

double CTrack::getNewHeading()
{
    if (getNewMode() == TrackMode::AB)
        return ABLine.desHeading;
    return 0;
}

void CTrack::setNewHeading(double new_heading)
{
    QLocale locale;
    if (getNewMode() == TrackMode::AB) {
        if (new_heading != ABLine.desHeading) {
            //calculate the B point 10 meters from the A point

            mark_end(newRefSide, ABLine.desPtA.easting + sin(new_heading) * 10,
                     ABLine.desPtA.northing + cos(new_heading) * 10);

            //update the new line heading as well as recalculate the ref line
            setNewName("A+ " + locale.toString(glm::toDegrees(ABLine.desHeading), 'f', 1) + QChar(0x00B0));
            emit newHeadingChanged();
        }
    }
}

QString CTrack::getCurrentName(void)
{
    if (idx > -1) {
        return gArr[idx].name;
    } else {
        return "";
    }
}

int CTrack::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return gArr.size();
}

QVariant CTrack::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if(row < 0 || row >= gArr.size()) {
        return QVariant();
    }

    const CTrk &trk = gArr.at(row);
    switch(role) {
    case RoleNames::index:
        return row;
    case RoleNames::NameRole:
        return trk.name;
    case RoleNames::ModeRole:
        return trk.mode;
    case RoleNames::IsVisibleRole:
        return trk.isVisible;
    case RoleNames::ptA:
        return QVector2D(trk.ptA.easting, trk.ptA.northing);
    case RoleNames::ptB:
        return QVector2D(trk.ptB.easting, trk.ptB.northing);
    case RoleNames::endPtA:
        return QVector2D(trk.endPtA.easting, trk.endPtA.northing);
    case RoleNames::endPtB:
        return QVector2D(trk.endPtB.easting, trk.endPtB.northing);
    case RoleNames::nudgeDistance:
        return trk.nudgeDistance;
    }

    return QVariant();
}

QHash<int, QByteArray> CTrack::roleNames() const
{
    return m_roleNames;
}

QString CTrack::getTrackName(int index)
{
    if (index < 0) return "";
    return gArr[index].name;
}

bool CTrack::getTrackVisible(int index)
{
    if (index < 0) return false;

    return gArr[index].isVisible;
}

double CTrack::getTrackNudge(int index)
{
    if (index < 0) return 0;

    return gArr[index].nudgeDistance;
}

void CTrack::update_ab_refline()
{
    double dist;
    double heading90;
    double vehicle_toolWidth = settings->value(SETTINGS_vehicle_toolWidth).value<double>();
    double vehicle_toolOffset = settings->value(SETTINGS_vehicle_toolOffset).value<double>();
    double vehicle_toolOverlap = settings->value(SETTINGS_vehicle_toolOverlap).value<double>();

    ABLine.desHeading = atan2(ABLine.desPtB.easting - ABLine.desPtA.easting,
                              ABLine.desPtB.northing - ABLine.desPtA.northing);
    if (ABLine.desHeading < 0) ABLine.desHeading += glm::twoPI;

    heading90 = ABLine.desHeading + glm::PIBy2;

    // update the ABLine desLineA and B and the design reference lines

    if (newRefSide > 0) {
        // right side
        dist = (vehicle_toolWidth - vehicle_toolOverlap) * 0.5 + vehicle_toolOffset;
    } else if (newRefSide < 0) {
        // left side
        dist = (vehicle_toolWidth - vehicle_toolOverlap) * -0.5 + vehicle_toolOffset;
    }

    ABLine.desLineEndA.easting = ABLine.desPtA.easting - (sin(ABLine.desHeading) * 1000);
    ABLine.desLineEndA.northing = ABLine.desPtA.northing - (cos(ABLine.desHeading) * 1000);

    designRefLine[0].easting = ABLine.desLineEndA.easting + sin(heading90) * dist;
    designRefLine[0].northing = ABLine.desLineEndA.northing + cos(heading90) * dist;

    ABLine.desLineEndB.easting = ABLine.desPtA.easting + (sin(ABLine.desHeading) * 1000);
    ABLine.desLineEndB.northing = ABLine.desPtA.northing + (cos(ABLine.desHeading) * 1000);

    designRefLine[1].easting = ABLine.desLineEndB.easting + sin(heading90) * dist;
    designRefLine[1].northing = ABLine.desLineEndB.northing + cos(heading90) * dist;
}

void CTrack::select(int index)
{
    //reset to generate new reference
    curve.isCurveValid = false;
    curve.lastHowManyPathsAway = 98888;
    ABLine.isABValid = false;
    curve.desList.clear();

    emit saveTracks(); //Do we really need to do this here?

    //We assume that QML will always pass us a valid index that is
    //visible, or -1
    setIdx(index);
    emit resetCreatedYouTurn();
    //yt.ResetCreatedYouTurn();
}

void CTrack::next()
{
    if (idx < 0) return;

    int visible_count = 0;
    for(CTrk &track : gArr) {
        if (track.isVisible) visible_count++;
    }

    if (visible_count == 0) return; //no visible tracks to choose

    idx = (idx + 1) % gArr.count();
    while (!gArr[idx].isVisible)
        idx = (idx + 1) % gArr.count();
}

void CTrack::prev()
{
    if (idx < 0) return;

    int visible_count = 0;
    for(CTrk &track : gArr) {
        if (track.isVisible) visible_count++;
    }

    if (visible_count == 0) return; //no visible tracks to choose

    if (--idx < 0) idx = gArr.count() - 1;
    while (!gArr[idx].isVisible) {
        if (--idx < 0) idx = gArr.count() - 1;
    }
}

void CTrack::start_new(int mode)
{
    newTrack = CTrk();
    newTrack.nudgeDistance = 0;
    setNewMode((TrackMode)mode);
    setNewName("");
    designRefLine.clear();
    designRefLine.append(Vec2());
    designRefLine.append(Vec2());
}

void CTrack::mark_start(double easting, double northing, double heading)
{
    //mark "A" location for AB Line or AB curve, or center for waterPivot
    switch(getNewMode()) {
    case TrackMode::AB:
        curve.desList.clear();
        newTrack.ptA.easting = easting;
        newTrack.ptA.northing = northing;
        ABLine.isMakingABLine = true;
        ABLine.desPtA.easting = easting;
        ABLine.desPtA.northing = northing;
        //temporarily set the B point based on current heading
        ABLine.desPtB.easting = easting + sin(heading) * 1000;
        ABLine.desPtB.northing = easting + cos(heading) * 1000;

        ABLine.isDesPtBSet = false;

        update_ab_refline();

        break;

    case TrackMode::Curve:
        curve.desList.clear();
        curve.isMakingCurve = true;
        break;

    case TrackMode::waterPivot:
        //record center
        newTrack.ptA.easting = easting;
        newTrack.ptA.northing = northing;
        setNewName("Piv");

    default:
        return;
    }
}

void CTrack::mark_end(int refSide, double easting, double northing)
{
    QLocale locale;
    double vehicle_toolWidth = settings->value(SETTINGS_vehicle_toolWidth).value<double>();
    double vehicle_toolOffset = settings->value(SETTINGS_vehicle_toolOffset).value<double>();
    double vehicle_toolOverlap = settings->value(SETTINGS_vehicle_toolOverlap).value<double>();


    //mark "B" location for AB Line or AB curve, or NOP for waterPivot
    int cnt;
    double aveLineHeading = 0;

    newRefSide = refSide;

    switch(getNewMode()) {
    case TrackMode::AB:
        newTrack.ptB.easting = easting;
        newTrack.ptB.northing = northing;

        //set desPtB in ABLine just so display updates.
        ABLine.desPtB.easting = easting;
        ABLine.desPtB.northing = northing;
        ABLine.isDesPtBSet = true;

        update_ab_refline();

        newTrack.heading = ABLine.desHeading;

        setNewName("AB " + locale.toString(glm::toDegrees(ABLine.desHeading), 'f', 1) + QChar(0x00B0));

        //after we're sure we want this, we'll shift it over
        break;

    case TrackMode::Curve:
        newTrack.curvePts.clear();

        cnt = curve.desList.count();
        if (cnt > 3)
        {
            //make sure point distance isn't too big
            curve.MakePointMinimumSpacing(curve.desList, 1.6);
            curve.CalculateHeadings(curve.desList);

            newTrack.ptA = Vec2(curve.desList[0].easting,
                                curve.desList[0].northing);
            newTrack.ptB = Vec2(curve.desList[curve.desList.count() - 1].easting,
                                curve.desList[curve.desList.count() - 1].northing);

            //calculate average heading of line
            double x = 0, y = 0;
            for (Vec3 &pt : curve.desList)
            {
                x += cos(pt.heading);
                y += sin(pt.heading);
            }
            x /= curve.desList.count();
            y /= curve.desList.count();
            aveLineHeading = atan2(y, x);
            if (aveLineHeading < 0) aveLineHeading += glm::twoPI;

            newTrack.heading = aveLineHeading;

            //build the tail extensions
            curve.AddFirstLastPoints(curve.desList);
            curve.SmoothABDesList(4);
            curve.CalculateHeadings(curve.desList);

            //write out the Curve Points
            for (Vec3 &item : curve.desList)
            {
                newTrack.curvePts.append(item);
            }

            setNewName("Cu " + locale.toString(glm::toDegrees(aveLineHeading), 'g', 1) + QChar(0x00B0));

            double dist;

            if (newRefSide > 0)
            {
                dist = (vehicle_toolWidth - vehicle_toolOverlap) * 0.5 + vehicle_toolOffset;
                NudgeRefCurve(newTrack, dist);
            }
            else if (newRefSide < 0)
            {
                dist = (vehicle_toolWidth - vehicle_toolOverlap) * -0.5 + vehicle_toolOffset;
                NudgeRefCurve(newTrack, dist);
            }
            //else no nudge, center ref line

        }
        else
        {
            curve.isMakingCurve = false;
            curve.desList.clear();
        }
        break;

    case TrackMode::waterPivot:
        //Do nothing here.  pivot point is already established.
        break;

    default:
        return;
    }

}

void CTrack::finish_new(QString name)
{
    double vehicle_toolWidth = settings->value(SETTINGS_vehicle_toolWidth).value<double>();
    double vehicle_toolOffset = settings->value(SETTINGS_vehicle_toolOffset).value<double>();
    double vehicle_toolOverlap = settings->value(SETTINGS_vehicle_toolOverlap).value<double>();

    double dist;
    newTrack.name = name;

    switch(getNewMode()) {
    case TrackMode::AB:
        if (!ABLine.isMakingABLine) return; //do not add line if it stopped

        if (newRefSide > 0)
        {
            dist = (vehicle_toolWidth - vehicle_toolOverlap) * 0.5 + vehicle_toolOffset;
            NudgeRefABLine(newTrack, dist);

        }
        else if (newRefSide < 0)
        {
            dist = (vehicle_toolWidth - vehicle_toolOverlap) * -0.5 + vehicle_toolOffset;
            NudgeRefABLine(newTrack, dist);
        }

        ABLine.isMakingABLine = false;
        break;

    case TrackMode::Curve:
        if (!curve.isMakingCurve) return; //do not add line if it failed.
        curve.isMakingCurve = false;
        break;

    case TrackMode::waterPivot:
        break;

    default:
        return;

    }

    newTrack.isVisible = true;
    gArr.append(newTrack);

    //emit saveTracks();

    //save tracks and activate the latest one
    select(gArr.count() - 1);
    reloadModel();

}

void CTrack::cancel_new()
{
    ABLine.isMakingABLine = false;
    curve.isMakingCurve = false;
    if(newTrack.mode == TrackMode::Curve) {
        curve.desList.clear();
    }
    newTrack.mode = 0;

    //don't need to do anything else
    }

void CTrack::pause(bool pause)
{
    if (newTrack.mode == TrackMode::Curve) {
        //turn off isMakingCurve when paused, or turn it on
        //when unpausing
        curve.isMakingCurve = !pause;
    }
}

void CTrack::add_point(double easting, double northing, double heading)
{
    AddPathPoint(Vec3(easting, northing, heading));
}

void CTrack::ref_nudge(double dist_m)
{
    NudgeRefTrack(dist_m);
}

void CTrack::nudge_zero()
{
    NudgeDistanceReset();
}

void CTrack::nudge_center()
{
    SnapToPivot();
}

void CTrack::nudge(double dist_m)
{
    NudgeTrack(dist_m);
}

void CTrack::delete_track(int index)
{
    //if we are using the track we are deleting, cancel
    //autosteer
    if (idx == index) idx = -1;

    //otherwise we'll have to adjust the current index after
    //deleting this track.
    if (idx > index) idx--;

    gArr.removeAt(index);
    reloadModel();
}

void CTrack::swapAB(int idx)
{
    if (idx >= 0 && idx < gArr.count()) {
        if (gArr[idx].mode == TrackMode::AB)
        {
            Vec2 bob = gArr[idx].ptA;
            gArr[idx].ptA = gArr[idx].ptB;
            gArr[idx].ptB = bob;

            gArr[idx].heading += M_PI;
            if (gArr[idx].heading < 0) gArr[idx].heading += glm::twoPI;
            if (gArr[idx].heading > glm::twoPI) gArr[idx].heading -= glm::twoPI;
        }
        else
        {
            int cnt = gArr[idx].curvePts.count();
            if (cnt > 0)
            {
                QVector<Vec3> arr;
                arr.reserve(gArr[idx].curvePts.count());
                std::reverse_copy(gArr[idx].curvePts.begin(),
                                  gArr[idx].curvePts.end(), std::back_inserter(arr));

                gArr[idx].curvePts.clear();

                gArr[idx].heading += M_PI;
                if (gArr[idx].heading < 0) gArr[idx].heading += glm::twoPI;
                if (gArr[idx].heading > glm::twoPI) gArr[idx].heading -= glm::twoPI;

                for (int i = 1; i < cnt; i++)
                {
                    Vec3 pt3 = arr[i];
                    pt3.heading += M_PI;
                    if (pt3.heading > glm::twoPI) pt3.heading -= glm::twoPI;
                    if (pt3.heading < 0) pt3.heading += glm::twoPI;
                    gArr[idx].curvePts.append(pt3);
                }

                Vec2 temp = gArr[idx].ptA;

                gArr[idx].ptA =gArr[idx].ptB;
                gArr[idx].ptB = temp;
            }
        }
    }
    reloadModel();
}

void CTrack::changeName(int index, QString new_name)
{
    if (index >=0 && index < gArr.count() ) {
        gArr[index].name = new_name;
    }
    reloadModel();
}

void CTrack::setVisible(int index, bool isVisible)
{
    if (index >=0 && index <= gArr.count() ) {
        gArr[index].isVisible = isVisible;
    }
    reloadModel();
}

void CTrack::copy(int index, QString new_name)
{
    CTrk new_track = CTrk(gArr[index]);
    new_track.name = new_name;

    gArr.append(new_track);
    reloadModel();
}

