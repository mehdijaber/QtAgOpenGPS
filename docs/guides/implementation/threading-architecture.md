# Threading Architecture

**Status**: IMPLEMENTED (Phase 6.0.24+)
**Last Validated**: 2025-09-15
**Objective**: Document Qt AgOpenGPS threading model and worker thread management

---

## Threading Model

### Architecture Overview

**Main Thread (Qt Event Loop)**:
- FormGPS (QQmlApplicationEngine)
- SettingsManager (singleton)
- AgIOService (singleton - thread coordinator)
- CTrack, CVehicle (singletons)
- AOGRenderer (OpenGL - dedicated render thread via Qt Scene Graph)
- All QML UI components
- **QUdpSocket** (event-driven, Phase 6.0.24+)

**Worker Threads** (managed by AgIOService):
- **NTRIPWorker**: RTK correction data (network I/O)
- **SerialWorker**: Arduino module communication

**Qt Scene Graph Render Thread** (automatic):
- OpenGL rendering (30 Hz, independent of main thread)

### Thread Coordinator Pattern

File: [classes/agioservice.h](../../../classes/agioservice.h)

```cpp
class AgIOService : public QObject {
    Q_OBJECT

private:
    // Worker threads
    QThread* m_ntripThread;
    NTRIPWorker* m_ntripWorker;

    QThread* m_serialThread;
    SerialWorker* m_serialWorker;

    // Main thread UDP (Phase 6.0.24)
    QUdpSocket* m_udpSocket;  // Event-driven, no thread needed

public:
    // Thread lifecycle
    void startWorkers();
    void stopWorkers();
};
```

---

## Worker Thread Management

### NTRIP Worker (RTK Corrections)

**Purpose**: Download RTK correction data from NTRIP caster

**Justification**: Network I/O may block (slow/unreliable internet)

**Implementation**:

```cpp
// Initialization
m_ntripThread = new QThread(this);
m_ntripWorker = new NTRIPWorker();
m_ntripWorker->moveToThread(m_ntripThread);

// Connections (Qt::QueuedConnection for thread safety)
connect(m_ntripThread, &QThread::started,
        m_ntripWorker, &NTRIPWorker::startConnection,
        Qt::QueuedConnection);

connect(m_ntripWorker, &NTRIPWorker::rtcmDataReceived,
        this, &AgIOService::onRtcmDataReceived,
        Qt::QueuedConnection);

// Start thread
m_ntripThread->start();
```

**Cleanup**:

```cpp
// Stop worker gracefully
m_ntripWorker->stop();
m_ntripThread->quit();
m_ntripThread->wait(5000);  // Wait max 5 seconds

// Delete worker and thread
m_ntripWorker->deleteLater();
m_ntripThread->deleteLater();
```

### Serial Worker (Arduino Communication)

**Purpose**: Communicate with Arduino modules (AutoSteer, sections)

**Justification**: Serial port I/O may block

**Implementation**:

```cpp
// Initialization
m_serialThread = new QThread(this);
m_serialWorker = new SerialWorker();
m_serialWorker->moveToThread(m_serialThread);

// Connections
connect(m_serialThread, &QThread::started,
        m_serialWorker, &SerialWorker::initialize,
        Qt::QueuedConnection);

connect(m_serialWorker, &SerialWorker::dataReceived,
        this, &AgIOService::onSerialDataReceived,
        Qt::QueuedConnection);

// Start thread
m_serialThread->start();
```

---

## Main Thread Event-Driven I/O (Phase 6.0.24)

### UDP Socket

**Before Phase 6.0.24**: UDPWorker thread (2535 lines, removed)

**After Phase 6.0.24**: Event-driven main thread

**Rationale**: QUdpSocket is non-blocking by design, worker thread unnecessary

File: [classes/agioservice_udp.cpp](../../../classes/agioservice_udp.cpp)

```cpp
// Main thread initialization
m_udpSocket = new QUdpSocket(this);

// Event-driven connection (Qt event loop)
connect(m_udpSocket, &QUdpSocket::readyRead,
        this, &AgIOService::onUdpDataReady,
        Qt::DirectConnection);  // Same thread, zero latency

// Event handler (< 2ms latency)
void AgIOService::onUdpDataReady() {
    while (m_udpSocket->hasPendingDatagrams()) {
        // Process datagram (non-blocking)
        // Parse and emit signals
    }
}
```

**Benefits**:
- 50× latency improvement (100ms → 2ms)
- Zero thread affinity violations
- -2535 lines of code

[See: agioservice-refactoring.md](agioservice-refactoring.md)

---

## Thread Safety Patterns

### Connection Types

**Qt::DirectConnection** (same thread):
```cpp
// Use when sender and receiver in same thread
connect(m_udpSocket, &QUdpSocket::readyRead,
        this, &AgIOService::onUdpDataReady,
        Qt::DirectConnection);

// Benefits: Zero latency, no queue overhead
// Requirements: Both objects in same thread
```

**Qt::QueuedConnection** (cross-thread):
```cpp
// Use when sender and receiver in different threads
connect(m_ntripWorker, &NTRIPWorker::rtcmDataReceived,
        this, &AgIOService::onRtcmDataReceived,
        Qt::QueuedConnection);

// Benefits: Thread-safe, automatic marshalling
// Cost: Latency (queue processing), memory (event queue)
```

**Qt::AutoConnection** (automatic):
```cpp
// Qt selects DirectConnection or QueuedConnection based on thread affinity
// Recommended for most cases
connect(sender, &Sender::signal, receiver, &Receiver::slot);
```

### BINDABLE Property Thread Affinity

**Critical Rule**: QProperty/BINDABLE properties can only be modified from their owner thread

**WRONG** (crash):
```cpp
// Worker thread updating FormGPS property
void UDPWorker::processGPS(double lat) {
    formGPS->setLatitude(lat);  // CRASH: Thread affinity violation
}
```

**CORRECT** (Phase 6.0.24 solution):
```cpp
// Main thread event-driven processing
void AgIOService::onUdpDataReady() {
    // Parse data (main thread)
    ParsedData data = m_parser.parse(datagram);

    // Emit signal (Qt::DirectConnection, same thread)
    emit parsedDataReady(data);
}

// FormGPS handler (main thread, same as QProperty)
void FormGPS::onParsedDataReady(const ParsedData& data) {
    m_latitude.setValue(data.latitude);  // Safe: Same thread ✓
}
```

---

## Thread Lifecycle

### Startup Sequence

```
1. Main thread creates AgIOService singleton
2. AgIOService::startWorkers()
   ├─ Create NTRIP thread + worker
   ├─ Create Serial thread + worker
   └─ Initialize UDP socket (main thread)
3. QThread::started() signals trigger worker initialization
4. Workers begin processing in their respective threads
```

### Shutdown Sequence

```
1. AgIOService::stopWorkers()
   ├─ Stop NTRIP worker
   │  ├─ Call worker->stop()
   │  ├─ QThread::quit()
   │  └─ QThread::wait(5000ms)
   ├─ Stop Serial worker
   │  ├─ Call worker->stop()
   │  ├─ QThread::quit()
   │  └─ QThread::wait(5000ms)
   └─ Close UDP socket (main thread)
2. Worker deleteLater() - Qt manages cleanup
3. Thread deleteLater() - Qt manages cleanup
```

File: [formgps.cpp:~FormGPS()](../../../formgps.cpp)

```cpp
~FormGPS() {
    // Stop workers before destruction
    AgIOService::instance()->stopWorkers();

    // Clear QML engine
    if (qmlEngine(this)) {
        qmlEngine(this)->clearComponentCache();
        qmlEngine(this)->collectGarbage();
    }

    // Process pending deletions
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
```

---

## Performance Characteristics

### Thread Count

```
Total Threads: 4
├─ Main Thread: 1 (Qt event loop, all UI, UDP)
├─ Worker Threads: 2 (NTRIP, Serial)
└─ Qt Scene Graph: 1 (OpenGL rendering)
```

**Optimization**: Minimal thread count reduces context switching overhead.

### Context Switching

**Measurement** (Windows Performance Toolkit):

```
Context Switches: ~102/second
├─ Main Thread: 62/s (timer-driven)
├─ NTRIP Worker: 8/s (network I/O wait)
├─ Serial Worker: 12/s (serial I/O wait)
└─ Render Thread: 20/s (30 Hz rendering)

Average Switch Duration: 0.8-2ms
```

[See: profiling-windows.md](../development/profiling-windows.md#step-3-analyze-context-switches)

### CPU Usage

```
CPU Distribution (4-core system):
├─ Main Thread: 15-20% (event processing, GPS calculations)
├─ NTRIP Worker: 1-2% (network wait)
├─ Serial Worker: 1-2% (serial wait)
└─ Render Thread: 5-8% (OpenGL 30 Hz)

Total: 22-32% CPU (efficient multi-threading)
```

---

## Best Practices

### When to Use Worker Threads

**Use worker thread for**:
- Blocking I/O (serial ports, slow file systems)
- CPU-intensive computation (>100ms)
- External library requiring blocking calls

**Do NOT use worker thread for**:
- Non-blocking Qt I/O (QUdpSocket, QTcpSocket, QFile on fast storage)
- Short computations (<10ms)
- Operations requiring main thread access (QML, QProperty)

### Thread Safety Checklist

```
✓ Use Qt::QueuedConnection for cross-thread signals
✓ Never call QProperty::setValue() from different thread
✓ Never access QML from worker threads
✓ Use mutex for shared data access
✓ Prefer moveToThread() over QThread subclassing
✓ Always wait() for threads before deletion
✓ Process DeferredDelete events in destructor
```

---

## References

**Related Documentation**:
- [System Architecture](../architecture/system-architecture.md) - AgIOService overview
- [Threading & Timers](../architecture/threading-timers.md) - Timer frequencies
- [AgIOService Refactoring](agioservice-refactoring.md) - Event-driven migration

**Qt Documentation**:
- QThread: https://doc.qt.io/qt-6/qthread.html
- Thread Support: https://doc.qt.io/qt-6/threads.html
- Signals & Slots: https://doc.qt.io/qt-6/signalsandslots.html

**Source Files**:
- [classes/agioservice.h](../../../classes/agioservice.h) - Thread coordinator
- [classes/ntripworker.h](../../../classes/ntripworker.h) - NTRIP worker
- [classes/serialworker.h](../../../classes/serialworker.h) - Serial worker

---

**Status**: IMPLEMENTED (Phase 6.0.24+)
**Validation**: Tested with real hardware, 4+ hours stability
**Last Validated**: 2025-09-15
**Thread Count**: 4 (main + 2 workers + render)
