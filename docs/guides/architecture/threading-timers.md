# Threading and Timer Architecture

**Status**: IMPLEMENTED ✅
**Phase**: 6.0.45+
**Last Validated**: 2025-09-15

Comprehensive guide to QtAgOpenGPS timer frequencies, threading model, and their impact on GPS processing, file recording, OpenGL rendering, and module communication.

## Overview

QtAgOpenGPS uses a timer-driven architecture to control:
- GPS position updates and simulation frequency
- Field data recording (Contour.txt, Sections.txt, Boundary.txt)
- OpenGL rendering frame rate
- PGN protocol transmission to hardware modules

**Core Design Principle**: **10 Hz (100ms) for optimal balance** between responsiveness, file size, network load, and CPU performance.

## Timer System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    TIMER SYSTEM                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐     ┌──────────────┐    ┌──────────────┐ │
│  │  timerGPS    │     │  timerSim    │    │ tmrWatchdog  │ │
│  │   100ms      │     │   100ms      │    │   250ms      │ │
│  │   (10 Hz)    │     │   (10 Hz)    │    │   (4 Hz)     │ │
│  └──────┬───────┘     └──────┬───────┘    └──────┬───────┘ │
│         │                    │                   │          │
│         └────────────────────┴───────────────────┘          │
│                              ↓                               │
│                  ┌────────────────────────┐                 │
│                  │ UpdateFixPosition()    │                 │
│                  │ - GPS data processing  │                 │
│                  │ - Point recording      │                 │
│                  │ - OpenGL trigger       │                 │
│                  │ - PGN 254 send         │                 │
│                  └───┬────────────────┬───┘                 │
│                      │                │                     │
│         ┌────────────┴─────┐    ┌─────┴──────────────┐     │
│         │   File Recording  │    │  OpenGL Rendering  │     │
│         │   - Contour.txt   │    │  - newframe = true │     │
│         │   - Sections.txt  │    │  - QMetaObject     │     │
│         │   - Boundary.txt  │    │  - AOGRenderer     │     │
│         └───────────────────┘    └────────────────────┘     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## Core Timers

### timerGPS - Real GPS Mode Timer

**Purpose**: Fixed-frequency position updates in real GPS mode

**Declaration**: [formgps.h:740](../../../formgps.h#L740)
```cpp
QTimer timerGPS;  // Phase 6.0.24: Fixed 10 Hz timer for real GPS mode
```

**Initialization**: [formgps_classcallbacks.cpp:17-21](../../../formgps_classcallbacks.cpp#L17-L21)
```cpp
// Phase 6.0.33: GPS timer for real GPS mode (10 Hz fixed rate)
// 10 Hz = 100ms interval for smooth rendering and PGN 254 AutoSteer commands
connect(&timerGPS, &QTimer::timeout, this, &FormGPS::onGPSTimerTimeout, Qt::UniqueConnection);
timerGPS.start(100);  // 100ms = 10 Hz
```

**Callback**: [formgps_position.cpp:2416-2427](../../../formgps_position.cpp#L2416-L2427)
```cpp
void FormGPS::onGPSTimerTimeout() {
    if (SettingsManager::instance()->menu_isSimulatorOn()) {
        return;  // Skip if simulation mode active
    }
    UpdateFixPosition();  // Timer-driven position updates at 10 Hz
}
```

**Characteristics**:
- **Frequency**: 100ms = 10 Hz
- **Mode**: Real GPS only (disabled during simulation)
- **Role**: Calls `UpdateFixPosition()` at fixed frequency
- **Stability**: Ensures constant timing independent of UDP reception

### timerSim - Simulation Mode Timer

**Purpose**: Fixed-frequency position updates in simulation mode

**Declaration**: [formgps.h:739](../../../formgps.h#L739)
```cpp
QTimer timerSim;
```

**Initialization**: [formgps_ui.cpp:2074](../../../formgps_ui.cpp#L2074) (initializeQMLInterfaces)
```cpp
// Phase 6.3.0: Start simulator timer AFTER InterfaceProperty initialization
if (SettingsManager::instance()->menu_isSimulatorOn()) {
    if (!timerSim.isActive()) {
        timerSim.start(100); // 10Hz - synchronized with timerGPS
    }
}
```

**Runtime Toggle**: [formgps.cpp:966-967](../../../formgps.cpp#L966-L967) (tmrWatchdog detection)
```cpp
} else if (isSimulatorOn && ! timerSim.isActive() ) {
    qDebug() << "Starting up simulator.";
    timerSim.start(100); // 10Hz - synchronized with timerGPS
    gpsHz = 10; // sync gpsHz to sim rate
}
```

**Callback**: [formgps_sim.cpp:132-161](../../../formgps_sim.cpp#L132-L161)
```cpp
void FormGPS::onSimTimerTimeout() {
    // Simulator tick logic
    sim.DoSimTick(steerAngle);
    // Emits signal newPosition
    // Triggers onSimNewPosition()
    // Calls UpdateFixPosition()
}
```

**Characteristics**:
- **Frequency**: 100ms = 10 Hz (synchronized with timerGPS)
- **Mode**: Simulation only (disabled in real GPS mode)
- **Role**: Generates simulated GPS data and triggers position updates
- **Consistency**: 10 Hz in all activation scenarios

### tmrWatchdog - System Watchdog Timer

**Purpose**: System monitoring and periodic PGN transmission

**Declaration**: [formgps.h:738](../../../formgps.h#L738)
```cpp
QTimer *tmrWatchdog;
```

**Initialization**: [formgps_ui.cpp:352-354](../../../formgps_ui.cpp#L352-L354)
```cpp
tmrWatchdog = new QTimer(this);
connect(tmrWatchdog, SIGNAL(timeout()), this, SLOT(tmrWatchdog_timeout()));
tmrWatchdog->start(250); // 250ms = 4 Hz
```

**Callback**: [formgps.cpp:944](../../../formgps.cpp#L944)
```cpp
void FormGPS::tmrWatchdog_timeout() {
    // Detect SIM/REAL mode changes
    if (wasSimulatorOn != isSimulatorOn) {
        ResetGPSState(isSimulatorOn);
    }

    // Toggle timerSim if necessary
    if (isSimulatorOn && !timerSim.isActive()) {
        timerSim.start(100); // 10 Hz
    }

    // Send PGN 239 (Machine) and PGN 229 (Sections)
    m_agioService->sendPgn(p_239.pgn);
    m_agioService->sendPgn(p_229.pgn);
}
```

**Characteristics**:
- **Frequency**: 250ms = 4 Hz (fixed)
- **Roles**:
  - System health monitoring
  - Simulation/real mode toggle detection
  - Periodic PGN 239 and 229 transmission
  - Sentence counter management (`sentenceCounter++`)

### gpsHz - Calculation Variable (Not a Timer!)

**Declaration**: [formgps.h:825](../../../formgps.h#L825)
```cpp
double gpsHz = 10;  // Just a variable for calculations
```

**Initialization**: [formgps.cpp:967](../../../formgps.cpp#L967), [formgps_sim.cpp:39](../../../formgps_sim.cpp#L39)
```cpp
gpsHz = 10;  // Synchronized with 100ms timer
```

**Usage Example**: [formgps_position.cpp:1804, 1838](../../../formgps_position.cpp#L1804)
```cpp
// Wheel speed calculation based on gpsHz
leftSpeed = left.getLength() * gpsHz * 10;
rightSpeed = right.getLength() * gpsHz * 10;
```

**Important Clarifications**:
- **Does NOT control** timer frequencies
- **Used for** speed and section timer calculations
- **Must be synchronized** with actual timer frequency
- **Current value**: `gpsHz = 10` (matches 10 Hz timers)

## Impact on Field File Recording

### Recording Mechanism

Points are recorded in `UpdateFixPosition()` based on:
1. **Distance traveled** (minimum threshold)
2. **Timer frequency** (maximum points per second limit)

**Code**: [formgps_position.cpp:1551-1584](../../../formgps_position.cpp#L1551-L1584)
```cpp
// Calculate distance since last point
sectionTriggerDistance = glm::Distance(pn.fix, prevSectionPos);
contourTriggerDistance = glm::Distance(pn.fix, prevContourPos);

// Record if distance > threshold
if (contourTriggerDistance > tool.contourWidth) {
    AddContourPoints();  // Writes to Contour.txt
    prevContourPos = pn.fix;
}

if (sectionTriggerDistance > sectionTriggerStepDistance) {
    AddSectionOrPathPoints();  // Writes to Sections.txt
    prevSectionPos = pn.fix;
}
```

### Distance Thresholds

| File | Threshold | Calculation |
|------|-----------|-------------|
| **Contour.txt** | `tool.contourWidth` | Tool width / 2 |
| **Sections.txt** | `sectionTriggerStepDistance` | `f.minHeadingStepDistance` |
| **Boundary.txt** | `tool.contourWidth` | Tool width / 2 |

### Frequency vs Distance Limitation

**Formula**: `Points/second = MIN(Velocity/Threshold, Timer Frequency)`

**Real-world Example** (5 km/h velocity):
- Distance traveled per frame at 10 Hz: `5000 m/h ÷ 3600 s ÷ 10 Hz = 0.139 m = 13.9 cm`
- If threshold = 2 cm < 13.9 cm → **LIMITED BY TIMER**, not by distance

### File Size Impact: 10 Hz vs 50 Hz

| Scenario | Velocity | Threshold | 10 Hz | 50 Hz | Difference |
|----------|----------|-----------|-------|-------|------------|
| **High speed** | 10 km/h | 10 cm | 2.78 pts/s | 2.78 pts/s | 1× (distance limited) |
| **Medium speed** | 5 km/h | 5 cm | 2.78 pts/s | 2.78 pts/s | 1× (distance limited) |
| **Low speed + small threshold** | 5 km/h | 2 cm | **10 pts/s** | **50 pts/s** | **5× (timer limited)** |

**When threshold < distance/frame**:
- **Contour.txt**: 5× smaller at 10 Hz
- **Sections.txt**: 5× smaller at 10 Hz
- **Boundary.txt**: 5× smaller at 10 Hz

**Benefits of 10 Hz**:
- ✅ Files 5× smaller
- ✅ Reduced disk I/O
- ✅ Faster field loading
- ✅ Lower memory usage

**Trade-offs at 10 Hz**:
- ⚠️ Less precision for contours at low speed
- ⚠️ Wider point spacing on curved lines

## Impact on OpenGL Rendering

### Rendering Synchronization

**Maximum OpenGL Frame Rate = Timer Frequency**

**Complete Flow**:
```
Timer (10 Hz)
    ↓
onGPSTimerTimeout() / onSimTimerTimeout()
    ↓
UpdateFixPosition()
    ↓
newframe = true  [formgps_position.cpp:1500]
    ↓
QMetaObject::invokeMethod(renderer, "update")  [line 1367]
    ↓
Qt Scene Graph → AOGRenderer::render()
    ↓
oglMain_Paint()
    ↓
lock.tryLockForRead()  [formgps_opengl.cpp:206]
    ↓ (if newframe=true)
OpenGL Rendering
```

### Synchronization Code

**Trigger Rendering**: [formgps_position.cpp:1500, 1364-1367](../../../formgps_position.cpp#L1500)
```cpp
newframe = true;  // Signal new position available

// Force OpenGL update in GUI thread
AOGRendererInSG *renderer = mainWindow->findChild<AOGRendererInSG *>("openglcontrol");
if (renderer) {
    QMetaObject::invokeMethod(renderer, "update", Qt::QueuedConnection);
}
```

**Lock Protection**: [formgps_opengl.cpp:206-213](../../../formgps_opengl.cpp#L206-L213)
```cpp
if (!lock.tryLockForRead())
    // If there's no new position to draw, just return so we don't
    // waste time redrawing. Frame rate is at most gpsHz.
    return;
```

### Rendering Performance

| Timer Frequency | Max OpenGL FPS | Frame Interval |
|-----------------|----------------|----------------|
| **10 Hz** | 10 FPS | 100ms |
| **50 Hz** | 50 FPS | 20ms |

**Note**: Actual rendering may be slower if OpenGL calculations exceed timer interval.

**Benefits of 10 Hz**:
- ✅ Reduced GPU load
- ✅ Lower CPU consumption
- ✅ Better performance on limited hardware

**Trade-offs at 10 Hz**:
- ⚠️ Less fluid animation (10 FPS vs 50 FPS)
- ⚠️ Perception of "choppy" movement

## Impact on PGN Module Communication

### PGN Transmission Architecture

**Phase 6.0+**: AgIOService manages all PGN transmission

**Code**: [formgps.cpp:1234-1236](../../../formgps.cpp#L1234-L1236)
```cpp
if (m_agioService) {
    m_agioService->sendPgn(p_239.pgn);  // Machine data
    m_agioService->sendPgn(p_229.pgn);  // Sections data
}
```

### PGN 254 - AutoSteer Data (0x7FFE)

**Purpose**: Real-time steering commands and status

**Structure**: [classes/cpgn.h:17-34](../../../classes/cpgn.h#L17-L34)
```cpp
class CPGN_FE {  // PGN 254 = 0xFE
    int speedLo = 5;      // Vehicle speed low byte
    int speedHi = 6;      // Vehicle speed high byte
    int status = 7;       // 0=OFF, 1=ON
    int steerAngleLo = 8; // Steering angle low byte
    int steerAngleHi = 9; // Steering angle high byte
    int lineDistance = 10; // Distance to line
    int sc1to8 = 11;      // Sections 1-8
    int sc9to16 = 12;     // Sections 9-16
}
```

**Transmission**: [formgps_position.cpp:1011-1014](../../../formgps_position.cpp#L1011-L1014) (inside UpdateFixPosition)
```cpp
// Phase 6.0.33: Send PGN 254 at 10 Hz (synchronized with timer frequency)
// Status byte controls module behavior: 0=OFF, 1=ON
// Managed by AgIOService
```

**Frequency**: **= Timer Frequency**
- **At 10 Hz**: 10 PGN 254/second
- **At 50 Hz**: 50 PGN 254/second

**Impact Analysis**:
- ✅ 10 Hz = Sufficient responsiveness for autosteer (firmware samples at 40 Hz)
- ✅ Reduced UDP network load
- ⚠️ 50 Hz = PGN spam (40+ per second), excessive network load

### PGN 239 - Machine Data (0x7FEF)

**Purpose**: Machine state and control signals

**Structure**: [classes/cpgn.h:90-104](../../../classes/cpgn.h#L90-L104)
```cpp
class CPGN_EF {  // PGN 239 = 0xEF
    int uturn = 5;
    int speed = 6;
    int hydLift = 7;
    int tram = 8;
    int geoStop = 9;    // Out of bounds flag
    int sc1to8 = 11;
    int sc9to16 = 12;
}
```

**Transmission**: [formgps.cpp:1235](../../../formgps.cpp#L1235) (inside tmrWatchdog_timeout)
```cpp
m_agioService->sendPgn(p_239.pgn);
```

**Frequency**: **4 Hz** (250ms via tmrWatchdog)

### PGN 229 - Sections Extended (0x7FE5)

**Purpose**: Extended section control (64 sections total)

**Structure**: [classes/cpgn.h:106-123](../../../classes/cpgn.h#L106-L123)
```cpp
class CPGN_E5 {  // PGN 229 = 0xE5
    int sc1to8 = 5;
    int sc9to16 = 6;
    int sc17to24 = 7;
    int sc25to32 = 8;
    int sc33to40 = 9;
    int sc41to48 = 10;
    int sc49to56 = 11;
    int sc57to64 = 12;
    int toolLSpeed = 13;
    int toolRSpeed = 14;
}
```

**Transmission**: [formgps.cpp:1236](../../../formgps.cpp#L1236) (inside tmrWatchdog_timeout)
```cpp
m_agioService->sendPgn(p_229.pgn);
```

**Frequency**: **4 Hz** (250ms via tmrWatchdog)

### Configuration PGNs

| PGN | Name | Transmission | Frequency |
|-----|------|--------------|-----------|
| **252 (0xFC)** | AutoSteer Settings | On-demand | Once |
| **251 (0xFB)** | AutoSteer Board Config | On-demand | Once |
| **238 (0xEE)** | Machine Config | On-demand | Once |
| **236 (0xEC)** | Relay Config | On-demand | Once |
| **235 (0xEB)** | Sections Config | On-demand | Once |
| **208 (0xD0)** | Latitude/Longitude | Rare | Once |

## Frequency Comparison: 10 Hz vs 50 Hz

| Parameter | 10 Hz (100ms) | 50 Hz (20ms) | Impact |
|-----------|--------------|--------------|--------|
| **UpdateFixPosition() per second** | 10 | 50 | 5× more calls |
| **Points recorded/s (2cm threshold, 5km/h)** | 10 | 50 | **5× more data** |
| **Contour.txt size (same path)** | 13K points | 65K points | **5× larger** |
| **OpenGL max frame rate** | 10 FPS | 50 FPS | 5× smoother animation |
| **PGN 254 per second** | 10 | 50 | **5× more UDP traffic** |
| **CPU load** | Low | High | 5× more iterations |
| **UDP network load** | ~1 KB/s | ~5 KB/s | 5× more bandwidth |
| **Autosteer responsiveness** | Excellent | Excellent | Firmware samples at 40 Hz |

## Design Rationale: Why 10 Hz is Optimal

**Technical Justifications**:
1. **Autosteer firmware** samples at 40 Hz → 10 Hz sufficient (Nyquist theorem: sample rate / 2)
2. **Field files** 5× smaller → faster loading and processing
3. **Network load** reduced → improved UDP stability
4. **CPU performance** optimized → works on lightweight hardware
5. **Rendering** 10 FPS = fluid for agricultural navigation

**Cases Where 50 Hz Might Be Justified**:
- ❌ **NONE** - Firmware module samples at 40 Hz, so 50 Hz = unnecessary spam

## Thread Safety and Coordination

### Main Thread Timers

All three timers (`timerGPS`, `timerSim`, `tmrWatchdog`) run in the **main thread**.

**Thread Safety**:
- ✅ No cross-thread synchronization needed for timer callbacks
- ✅ Direct access to FormGPS properties safe
- ✅ QML updates automatic via Qt 6.8 BINDABLE properties

### UpdateFixPosition() Execution

**Thread**: Main thread
**Call Frequency**: 10 Hz (from timerGPS or timerSim)
**Critical Sections**:
- File I/O (Contour.txt, Sections.txt, Boundary.txt)
- OpenGL state updates (`newframe = true`)
- PGN data preparation

**Performance**: Each call must complete within 100ms to avoid timer queue backlog.

## Timer Maintenance and Validation

### Critical Variables

| Variable | File | Line | Expected Value |
|----------|------|------|----------------|
| `timerGPS.start()` | formgps_classcallbacks.cpp | 21 | 100 |
| `timerSim.start()` | formgps_ui.cpp | 2074, 2080 | 100 |
| `timerSim.start()` | formgps.cpp | 966 | 100 |
| `gpsHz` | formgps.cpp | 967 | 10.0 |
| `gpsHz` | formgps_sim.cpp | 39 | 10.0 |

### Validation Tests

**After frequency changes**:
1. ✅ Verify `gpsHz` synchronized with timer frequency
2. ✅ Test Contour.txt recording (reasonable file size)
3. ✅ Verify fluid rendering (no stuttering)
4. ✅ Test simulation toggle (frequency consistency)
5. ✅ Monitor CPU/UDP load

### Common Issues

**Symptom**: File sizes explode (65K points for simple path)
**Cause**: Timer frequency > 10 Hz
**Fix**: Verify all `timerSim.start()` and `timerGPS.start()` use 100ms

**Symptom**: Choppy rendering or autosteer response
**Cause**: `gpsHz` out of sync with timer frequency
**Fix**: Ensure `gpsHz = 10` matches timer interval

**Symptom**: Inconsistent simulation behavior
**Cause**: Different `timerSim` frequencies in different code paths
**Fix**: Standardize all `timerSim.start()` to 100ms

## Timer Performance Characteristics

**Timer Accuracy**:
- QTimer uses system clock (millisecond precision)
- Actual interval may vary ±1-5ms depending on system load
- Average frequency stable over time (self-correcting)

**Callback Execution Time**:
- **UpdateFixPosition()**: 5-20ms typical (depends on complexity)
- **tmrWatchdog_timeout()**: <1ms (lightweight operations)
- **onSimTimerTimeout()**: 2-5ms (simulation calculations)

**Queue Behavior**:
- If callback exceeds interval, next timer event queued
- No timer events skipped (all execute eventually)
- Heavy load may cause timer drift (accumulated delay)

## References

### Code Files
- [formgps.h](../../../formgps.h) - Timer declarations and gpsHz variable
- [formgps_classcallbacks.cpp](../../../formgps_classcallbacks.cpp) - timerGPS initialization
- [formgps_ui.cpp](../../../formgps_ui.cpp) - timerSim and tmrWatchdog initialization
- [formgps.cpp](../../../formgps.cpp) - tmrWatchdog callback and simulation toggle
- [formgps_sim.cpp](../../../formgps_sim.cpp) - Simulation timer callback
- [formgps_position.cpp](../../../formgps_position.cpp) - UpdateFixPosition() core logic
- [formgps_opengl.cpp](../../../formgps_opengl.cpp) - OpenGL rendering coordination
- [classes/cpgn.h](../../../classes/cpgn.h) - PGN protocol structures

### Related Documentation
- [System Architecture](system-architecture.md) - Overall architecture and threading model
- [AgIOService Architecture](agioservice-architecture.md) - PGN transmission details
- [NMEA Protocol Reference](../../reference/nmea-sentences.md) - GPS data formats
- [PGN Protocol Reference](../../reference/pgn-sentences.md) - Module communication protocols

### External Resources
- [Qt QTimer Documentation](https://doc.qt.io/qt-6/qtimer.html)
- [AgOpenGPS Forum - Timer Architecture](https://discourse.agopengps.com)
