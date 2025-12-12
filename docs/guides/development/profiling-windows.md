# Windows Performance Profiling Guide

**Status**: IMPLEMENTED (Phase 6.0.43+)
**Last Validated**: 2025-12-06
**Platform**: Windows 11 with Visual Studio 2022, Qt 6.8.2
**Objective**: Measure 6 runtime performance metrics impossible to determine from code analysis

---

## Table of Contents

1. [Overview](#overview)
2. [Method 1: Qt Creator Profiler](#method-1-qt-creator-profiler)
3. [Method 2: Visual Studio Profiler](#method-2-visual-studio-profiler)
4. [Method 3: Windows Performance Toolkit](#method-3-windows-performance-toolkit)
5. [Method 4: Very Sleepy](#method-4-very-sleepy)
6. [Method 5: Android Device Profiling](#method-5-android-device-profiling)
7. [Tool Comparison Matrix](#tool-comparison-matrix)
8. [Results Analysis](#results-analysis)

---

## Overview

### 6 Performance Metrics to Measure

| # | Metric | Recommended Tool | Difficulty | Time Required |
|---|--------|------------------|------------|---------------|
| 1 | Real-time function execution | Qt Creator CPU Profiler | Moderate | 30 min |
| 2 | QML binding triggers | Qt Creator QML Profiler | Moderate | 15 min |
| 3 | Memory allocation hotspots | Visual Studio Memory Profiler | Moderate | 1 hour |
| 4 | Cache miss rates | Windows Performance Toolkit | Advanced | 2 hours |
| 5 | GPU wait time | Visual Studio GPU Usage | Moderate | 45 min |
| 6 | System call overhead | Windows Performance Toolkit | Advanced | 1.5 hours |

### Recommended Quick Start (2 hours)

For 80% of critical performance data:

1. Qt Creator CPU Profiler (30 min) - Metric #1
2. Qt Creator QML Profiler (15 min) - Metric #2
3. Visual Studio Memory (30 min) - Metric #3
4. Visual Studio GPU (30 min) - Metric #5
5. Analysis (15 min)

**Advanced** (3.5 hours additional):
- Windows Performance Toolkit for syscalls (1.5 hours) - Metric #6
- Windows Performance Toolkit for cache misses (2 hours) - Metric #4

---

## Method 1: Qt Creator Profiler

**Objectives**:
- Measure CPU time per function (Metric #1)
- Count QML binding evaluations (Metric #2)
- Identify performance hotspots

**Prerequisites**:
- Qt Creator (included with Qt 6.8.2)
- Visual Studio 2022 with "Desktop development with C++" workload
- Debug symbols enabled (configuration below)

### Step 1: Configure Debug Symbols

**Why**: Without debug symbols, profiler shows only memory addresses instead of function names.

**Enable in CMakeLists.txt**:

File: [CMakeLists.txt:29-32](../../../CMakeLists.txt#L29-L32)

```cmake
# Uncomment these lines for profiling builds:
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
message("MSVC: Debug symbols enabled in Release for profiling")
```

**Qt Creator Configuration**:

1. Open project: Qt Creator - File - Open File or Project - [CMakeLists.txt](../../../CMakeLists.txt)
2. Select Build Type: Projects (left sidebar) - Build Settings - Build configuration: **RelWithDebInfo**
3. Rebuild: Build - Rebuild All Projects
4. Verify .pdb created: `dir build\Desktop_Qt_6_8_2_MSVC2022_64bit-RelWithDebInfo\*.pdb`

### Step 2: CPU Profiler (Metric #1)

**Launch Profiling Session**:

1. Open Qt Creator
2. Configure Profiler:
   - Analyze - Profiler Settings
   - Profiler: Performance (ETW on Windows)
   - Sample frequency: 1000 Hz (1ms)
3. Start Profiling: Analyze - Profiling - Start Performance Analyzer (or Ctrl+Shift+F11)
4. Execute Test Scenario:
   - Start GPS or Simulation
   - Open a field
   - Activate autosteer
   - Run for 60 seconds
   - Close application
5. Qt Creator captures automatically on application exit

**Analyze Results**:

**Timeline View**:
```
Analyze - Profiler - Timeline View
└─ CPU usage graph by thread
   ├─ Main Thread (QtThread)
   ├─ Render Thread
   └─ QML Engine
```

**Flame Graph**:
```
Analyze - Profiler - Flame Graph
└─ Hierarchical visualization
   ├─ Width = CPU time
   └─ Height = call stack depth
```

**Top-Down Statistics**:
```
Analyze - Profiler - Statistics (Top-Down)

Function                       Total CPU   Self CPU   Calls
─────────────────────────────────────────────────────────────
FormGPS::UpdateFixPosition     840ms (42%) 120ms (6%) 100
  ├─ oglBack_Paint             280ms (14%) 280ms (14%) 100
  ├─ Q_PROPERTY updates         180ms (9%)  180ms (9%)  5400
  └─ CalculatePositionHeading   150ms (7.5%) 150ms (7.5%) 100
```

**Export Data**: Right-click in Statistics - Export to CSV

**Critical Information to Capture**:

| Metric | Location | Purpose |
|--------|----------|---------|
| UpdateFixPosition() total time | Search "UpdateFixPosition" | Validate ≤80ms measured in code |
| oglBack_Paint() time | Search "oglBack" | OpenGL rendering cost |
| Q_PROPERTY update overhead | Flame Graph - "QObject::setProperty" | Qt binding system overhead |
| CalculatePositionHeading | Search "CalculatePosition" | Navigation calculation cost |
| Main thread CPU % | Timeline - Main Thread line | Confirm 69% baseline |

### Step 3: QML Profiler (Metric #2)

**Enable QML Debugging**:

```cmake
# CMakeLists.txt - Add before qt_add_executable
add_definitions(-DQT_QML_DEBUG)
```

**Launch QML Profiling**:

1. Rebuild project with QML debugging enabled
2. Start QML Profiler: Analyze - QML Profiler
3. Execute test scenario (30 seconds)
4. Stop and analyze results

**Analyze Binding Evaluations**:

```
QML Profiler - Bindings tab

Property                     Evaluations  Total Time  Avg Time
──────────────────────────────────────────────────────────────
aog.latitude                 540          180ms       0.33ms
aog.longitude                540          175ms       0.32ms
vehicle_xy (cascade)         540          280ms       0.52ms  <- Cascade!
mapTransform                 540          420ms       0.78ms  <- Cascade 2!
```

**Timeline View (QML Perspective)**:
- JavaScript execution (yellow)
- Binding updates (blue)
- Signal handling (green)
- Rendering (red)

**Critical Metrics**:

| Metric | Location | Purpose |
|--------|----------|---------|
| Total bindings triggered/frame | Bindings tab - Count column | Confirm cascade effect |
| Binding cascade depth | Flame Graph | Identify chains A-B-C-D |
| Expensive bindings | Bindings tab - Sort by Total Time | Top 10 costly bindings |
| JavaScript execution time | Timeline - JavaScript section | QML logic overhead |

**Troubleshooting**:

**Problem**: "No debug symbols found"
- Solution: Verify `.pdb` files exist in build directory
- Force generation: `cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..`

**Problem**: "Profiler cannot connect"
- Solution: Tools - Options - Analyzer - Check "Automatically start profiler"

**Problem**: QML Profiler empty
- Solution: Verify `QT_QML_DEBUG` is defined in CMakeLists.txt

---

## Method 2: Visual Studio Profiler

**Objectives**:
- Memory allocation hotspots (Metric #3)
- GPU wait time (Metric #5)
- Alternative CPU profiling

**Prerequisites**:
- Visual Studio 2022 Professional/Enterprise (Community works for CPU/Memory)
- CMake project opened in Visual Studio

### Step 1: Open Project in Visual Studio

```
1. Launch Visual Studio 2022
2. File - Open - CMake...
3. Select: CMakeLists.txt (in project root directory)
4. Wait for CMake configuration
5. Build - Build All (F7)
```

### Step 2: CPU Profiler

**Configure**:
```
Debug - Profiler - Performance Profiler... (Alt+F2)

Enable:
☑ CPU Usage
☑ GPU Usage (for OpenGL analysis)
☐ Memory Usage (for now)

Target: Release / RelWithDebInfo
```

**Launch**:
1. Click "Start" in Performance Profiler
2. Application starts automatically
3. Execute test scenario (60 seconds)
4. Close application
5. Visual Studio analyzes automatically

**Analyze Results - Hot Path**:

```
CPU Usage - Hot Path tab

Call Tree                       Total CPU  Self CPU
───────────────────────────────────────────────────
[External Code]                 85%        15%
├─ QtAgOpenGPS.exe!WinMain      70%        2%
│  └─ QApplication::exec        68%        5%
│     └─ QEventLoop::exec       63%        3%
│        └─ FormGPS::UpdateFixPosition  55%  8%
│           ├─ oglBack_Paint    20%        20%  <- Hotspot!
│           ├─ QObject::setProperty 12%   12%  <- Hotspot!
│           └─ CalculateSectionLookAhead 10% 10%
```

**Caller/Callee View**:
```
CPU Usage - Select function - Caller/Callee

UpdateFixPosition (55% total):
Called by:
  ├─ QTimer::timeout           (54.5%)
  └─ DirectConnection signals  (0.5%)

Calls to:
  ├─ oglBack_Paint             (20%)
  ├─ QObject::setProperty      (12%)
  └─ CalculateSectionLookAhead (10%)
```

### Step 3: Memory Profiler (Metric #3)

**Configure**:
```
Debug - Profiler - Performance Profiler... (Alt+F2)

Enable:
☐ CPU Usage
☑ .NET/C++ Memory (Native Memory)
☐ GPU Usage

Target: Release / RelWithDebInfo
```

**Launch**:
1. Click "Start"
2. Application starts
3. Execute test scenario (2 minutes)
4. Click "Take Snapshot" (camera icon) - Snapshot #1
5. Continue usage (2 minutes)
6. Click "Take Snapshot" - Snapshot #2
7. Close application

**Compare Snapshots**:

```
Memory Usage - Compare Snapshots (#2 - #1)

Type                   Size Diff  Count Diff  Avg Size
─────────────────────────────────────────────────────
QString                +25 MB     +500,000    50 bytes  <- Leak suspect!
QByteArray             +12 MB     +200,000    60 bytes
QVector<CContourPt>    +8 MB      +100,000    80 bytes
QOpenGLBuffer          +5 MB      +50         100 KB    <- OpenGL buffers!
```

**Allocation View**:

```
Memory Usage - Snapshot #2 - Allocation tab

Function                     Live Bytes  Live Objects
───────────────────────────────────────────────────────
QString::operator+           45 MB       900,000      <- Hotspot!
  Called by:
    └─ FormGPS::UpdateFixPosition
       └─ QString::number().toUtf8() + ","

QVector::append              20 MB       500,000
  Called by:
    └─ CContour::AddPoint
```

**Detect Memory Leaks**:

```
Memory Usage - Snapshot #2 - "Show objects with no references"

Type             Size    Count  Allocation Site
─────────────────────────────────────────────────────
QTimer*          4.5 KB  150    agioservice.cpp:62  <- Potential leak!
QOpenGLTexture*  8 MB    20     oglMain.cpp:340     <- GPU leak!
```

**Critical Metrics**:

| Metric | Location | Action if Problem |
|--------|----------|-------------------|
| Memory growth rate | Snapshot diff | If >10 MB/min - leak probable |
| QString allocations | Allocation tab - Search "QString" | Optimize concatenations |
| QOpenGLBuffer count | Search "QOpenGL" | Verify glDeleteBuffers() |
| Objects with no references | "Show objects with no references" | Confirmed memory leaks |

### Step 4: GPU Usage Profiler (Metric #5)

**Configure**:
```
Debug - Profiler - Performance Profiler... (Alt+F2)

Enable:
☐ CPU Usage
☐ Memory Usage
☑ GPU Usage

Target: Release / RelWithDebInfo
```

**Launch**:
1. Click "Start"
2. Execute scenario with OpenGL rendering active (field opened)
3. Run for 60 seconds
4. Close application

**Analyze GPU Timeline**:

```
GPU Usage - Timeline

[CPU Thread Timeline]
████ CPU Busy ████░░░░ CPU Wait for GPU ░░░░████
     ↓ glDrawArrays         ↓ glFinish

[GPU Timeline]
░░░░████████ GPU Processing ████████░░░░

Metrics:
  CPU wait for GPU: 25%  <- If >10% = problem!
  GPU utilization:  45%
```

**GPU Frame Analysis**:

```
GPU Usage - Click on frame in timeline

Frame #1234 (16.6ms target):
├─ CPU time:     12ms
├─ GPU time:     28ms  <- Exceeds budget!
├─ Sync wait:    8ms   <- CPU-GPU sync wait
└─ Total:        40ms  <- 25 FPS instead of 60 FPS

Draw calls:      124   <- If >100 = too many!
State changes:   45    <- If >20 = too many!
Texture uploads: 8 MB  <- If >1 MB/frame = problem
```

**GPU Pipeline Stages**:

```
GPU Usage - Pipeline Stages

Stage              Time   % of GPU
─────────────────────────────────────
Vertex Shader      4ms    14%
Fragment Shader    18ms   64%  <- Bottleneck!
Rasterization      3ms    11%
Texture Sampling   3ms    11%
```

**Critical Thresholds**:

| Metric | Location | Threshold | Action |
|--------|----------|-----------|--------|
| CPU wait for GPU % | Timeline metrics | >10% | Async rendering, remove glFinish |
| GPU time per frame | Frame Analysis | >16ms (60fps) | Reduce draw calls, optimize shaders |
| Draw calls per frame | Frame Analysis | >100 | Batch geometry |
| Texture upload size | Frame Analysis | >1 MB/frame | Cache textures, reduce resolution |

**Troubleshooting**:

**Problem**: GPU Usage unavailable
- Cause: Windows 10 version < 1903
- Solution: Update Windows or use RenderDoc (free, better for OpenGL)

**Problem**: Memory Profiler crashes
- Solution: Visual Studio - Tools - Options - Debugging - Uncheck "Enable Edit and Continue"

---

## Method 3: Windows Performance Toolkit

**Objectives**:
- System call overhead (Metric #6)
- Cache miss rates (Metric #4) - Intel CPU only
- Detailed context switches

**Prerequisites**:

**Installation**:
1. Download Windows SDK: https://developer.microsoft.com/windows/downloads/windows-sdk/
2. Install only "Windows Performance Toolkit" (uncheck other components)
3. Verify installation: `wpr -help` and `wpa -help`

### Step 1: Capture ETW Trace (System Calls)

**Start Recording** (PowerShell as Administrator):

```powershell
cd "C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit"

# Optimized profile for syscalls + context switches
.\wpr.exe -start CPU -start FileIO -start DiskIO

# Alternative: General profile
.\wpr.exe -start GeneralProfile
```

**Execute Application**:
1. Launch QtAgOpenGPS from Qt Creator (F5)
2. Execute test scenario (2 minutes)
3. Close application

**Stop Recording**:

```powershell
.\wpr.exe -stop D:\traces\qtagio_syscalls.etl

# Typical file size: 500 MB - 2 GB
```

### Step 2: Analyze Trace (Metric #6)

**Open in Windows Performance Analyzer**:

```powershell
.\wpa.exe D:\traces\qtagio_syscalls.etl
```

**Analyze System Calls**:

```
Graph: CPU Usage (Sampled) by Process, Thread

1. Drag "CPU Usage (Sampled)" to Analysis pane
2. Gold bar - View - Load Symbols (wait 2-5 min)
3. Filter by Process - QtAgOpenGPS.exe
4. Expand call stacks

Call Stack                          CPU Time  % Total
───────────────────────────────────────────────────────
ntoskrnl.exe!KiSystemCall64         850ms     35%     <- Kernel!
├─ ntdll.dll!NtWriteFile            320ms     13%
│  └─ kernelbase.dll!WriteFile      315ms     13%
│     └─ Qt6Core.dll!QFile::write   310ms     13%    <- File I/O
│
├─ ntdll.dll!NtDeviceIoControlFile  180ms     7.5%
│  └─ ws2_32.dll!sendto             175ms     7.2%
│     └─ AgIOService::sendPgn       170ms     7%     <- UDP packets
│
└─ ntdll.dll!NtWaitForSingleObject  250ms     10%
   └─ kernel32.dll!WaitForSingleObject 245ms  10%
      └─ QTimer::timeout handling   240ms     10%    <- Timer overhead
```

**System Calls by Process**:

```
Graph: System Calls

Filter: QtAgOpenGPS.exe

System Call              Count    Total Time  Avg Time
─────────────────────────────────────────────────────────
NtWaitForMultipleObjects 5,400    850ms       0.15ms   <- 95 timers/s × 60s!
NtDeviceIoControlFile    2,800    320ms       0.11ms   <- UDP sendto
NtWriteFile              120      180ms       1.5ms    <- File writes
NtAllocateVirtualMemory  8,900    140ms       0.016ms  <- Memory allocs
```

**Critical Metrics**:

| Metric | Location | Interpretation |
|--------|----------|----------------|
| Syscall count total | System Calls graph | If >10,000/s = high overhead |
| NtWaitForMultipleObjects % | System Calls - Sort by Count | Confirms 33 timer impact |
| NtDeviceIoControlFile count | System Calls - Search "Device" | UDP send rate |
| Kernel time % | CPU Usage - ntoskrnl.exe | If >20% = syscall heavy |

### Step 3: Analyze Context Switches

```
Graph: Context Switch Count by Process, Thread

Filter: QtAgOpenGPS.exe

Thread               Switches  Reason              Avg Duration
───────────────────────────────────────────────────────────────
QtThread (Main)      3,200     WaitForSingleObject 0.8ms
qtMainLoopThrea      1,800     WaitForSingleObject 1.2ms
RenderThread         950       WaitForSingleObject 2.1ms
NTRIP Worker         80        WaitForSingleObject 15ms
Serial Worker        120       WaitForSingleObject 8ms

TOTAL: 6,150 context switches in 60s = 102 switches/s
```

**Critical Metrics**:

| Metric | Measured Value | Interpretation |
|--------|----------------|----------------|
| Context switches/second | ~102/s | Normal for 33 timers |
| Wait reason | WaitForSingleObject (95%) | Timer-driven application |
| Avg switch duration | 0.8-2ms | Acceptable if <5ms |

### Step 4: Cache Misses (Metric #4) - Intel CPU Only

**WARNING**: Cache profiling requires:
- Intel CPU with Performance Monitoring Unit (PMU)
- Special WPR profile with PMC (Performance Monitor Counters)

**Verify PMU Support**:
- Download Intel Processor Identification Utility
- Or check Task Manager - Performance - CPU - "Hardware Counters: Available"

**Capture Trace with PMC**:

```powershell
# PowerShell as Administrator
wpr -start CPU -start PMC

# Execute application (30s)

wpr -stop D:\traces\qtagio_pmc.etl
```

**Analyze Cache Misses**:

```powershell
wpa D:\traces\qtagio_pmc.etl

# Graph: PMC Metrics
1. Drag "PMC Metrics" to Analysis pane
2. Filter: QtAgOpenGPS.exe
3. Metrics:
   ├─ L1 Cache Misses
   ├─ L2 Cache Misses
   ├─ L3 Cache Misses (Last Level Cache)
   └─ Instructions Retired
```

**Example Results**:

```
Function                     L3 Misses  IPC   CPI
──────────────────────────────────────────────────
FormGPS::UpdateFixPosition   2.5M       0.8   1.25  <- Memory bound!
  ├─ CalculateSectionLookAhead 1.2M     0.6   1.67  <- High cache miss!
  └─ oglBack_Paint            800K      1.2   0.83  <- OK

IPC (Instructions Per Cycle):
  - >2.0 = Good (CPU-bound, few cache misses)
  - 1.0-2.0 = Medium
  - <1.0 = Bad (memory-bound, many cache misses)
```

**Critical Thresholds**:

| Metric | Threshold | Action |
|--------|-----------|--------|
| IPC < 1.0 | Memory-bound | Improve data locality, reduce pointer chasing |
| L3 miss rate > 5% | Cache thrashing | Reduce working set size, improve data layout |
| CPI > 2.0 | Pipeline stalls | Optimize branches, reduce dependencies |

**Troubleshooting**:

**Problem**: "Access Denied"
- Solution: Run PowerShell and Qt Creator as Administrator

**Problem**: Symbols not resolved
- Solution: WPA - Trace - Load Symbols
- Configure Symbol Path: `SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols`

**Problem**: Trace too large (>5 GB)
- Solution: Limit capture duration to 30 seconds instead of 2 minutes

---

## Method 4: Very Sleepy

**Objectives**:
- Basic CPU profiling without complex setup
- Alternative if Qt Creator/Visual Studio unavailable

**Installation**:

1. Download Very Sleepy CS: http://www.codersnotes.com/sleepy/
2. Install (5 MB, 2 minutes)
3. No configuration required

**Usage**:

1. Open Very Sleepy
2. File - Launch Application
3. Select: `build\QtAgOpenGPS.exe`
4. Click "Run"
5. Execute test scenario (60 seconds)
6. Close application
7. Very Sleepy analyzes automatically

**Analyze Results - Functions View**:

```
Function                          Inclusive  Exclusive  Calls
──────────────────────────────────────────────────────────────
FormGPS::UpdateFixPosition        42.5%      6.8%       600
├─ oglBack_Paint                  14.2%      14.2%      600
├─ QObject::setProperty           9.3%       9.3%       32,400  <- 54×600!
└─ CalculateSectionLookAhead      7.8%       7.8%       600

ntdll.dll!NtWaitForSingleObject   10.2%      10.2%      5,700  <- Timers!
```

**Advantages**:
- Zero configuration
- Results in 2 minutes
- Lightweight sampling profiler

**Limitations**:
- No QML profiling
- No memory profiling
- No GPU analysis
- Precision ~10ms (vs 1ms Qt Creator)

---

## Method 5: Android Device Profiling

**Objectives**:
- Profile on real Android device
- Measure ARM CPU performance
- Mobile GPU profiling

**Prerequisites**:
- Android device connected via USB
- Android Debug Bridge (adb) installed
- Application compiled in Debug or RelWithDebInfo

### Android Studio Profiler

**Installation**:
1. Download Android Studio: https://developer.android.com/studio
2. Install only "Android SDK Platform-Tools"
3. Verify adb: `adb devices`

**Launch**:

```bash
# Install application on device (adjust path to your build directory)
adb install -r build/android/QtAgOpenGPS.apk

# Identify package name
adb shell pm list packages | findstr qtagio
# Output: package:com.agopengps.qtagio

# Launch Android Profiler
# Android Studio - View - Tool Windows - Profiler
```

**CPU Profiling**:

```
Android Studio - Profiler - CPU tab
1. Click "Record" button
2. Execute scenario (60s)
3. Click "Stop"
4. Analyze call chart

Call Chart (Flame Graph):
UpdateFixPosition ████████████████████████ 850ms (42%)
├─ oglBack_Paint ██████ 280ms (14%)
└─ CalculatePositionHeading ████ 150ms (7.5%)
```

**Memory Profiling**:

```
Android Profiler - Memory tab

[Real-time graph]
Native Memory:  ████████ 180 MB
Java Heap:      ███ 16 MB
Graphics:       ████ 120 MB  <- OpenGL buffers
Stack:          █ 8 MB

Click "Dump Java heap" - Analyze allocations
```

### systrace (System Details)

**Capture Trace**:

```bash
cd %ANDROID_HOME%\platform-tools\systrace

# Capture 10 seconds of trace
python systrace.py -t 10 -o D:\traces\qtagio_systrace.html gfx view sched freq idle

# Open in Chrome
chrome.exe D:\traces\qtagio_systrace.html
```

**Analyze Timeline**:

```
[Chrome Trace Viewer]

Processes:
├─ com.agopengps.qtagio
│  ├─ UI Thread (QtThread)          ████████░░░░░██████  69% busy
│  │  └─ UpdateFixPosition          ████ 80ms @ 10 Hz
│  ├─ RenderThread                  ████░░░░░░░░░░░░░░░  7% busy
│  └─ GPU                           ░░░░████░░░░░░░░░░░  45% busy
│
└─ SurfaceFlinger (Android Compositor)
   └─ Vsync events                   ▼ ▼ ▼ ▼ (16.6ms / 60 FPS)
```

**Critical Metrics**:

| Metric | Location | Problem if... |
|--------|----------|---------------|
| Frame rendering time | UI Thread - red frames | >16ms = dropped frames |
| GPU wait time | GPU row - gaps | >30% idle = CPU bottleneck |
| Context switches | Sched section | >200/s = thread thrashing |
| CPU frequency | Freq section | <50% max = thermal throttling |

---

## Tool Comparison Matrix

| Tool | Metric #1 CPU | Metric #2 QML | Metric #3 Memory | Metric #4 Cache | Metric #5 GPU | Metric #6 Syscalls | Difficulty | Setup Time |
|------|---------------|---------------|------------------|-----------------|---------------|--------------------|------------|------------|
| **Qt Creator Profiler** | Excellent | Excellent | Basic | No | Basic | No | Moderate | 30 min |
| **Visual Studio** | Excellent | No | Excellent | No | Good | Basic | Moderate | 15 min |
| **WPT (ETW)** | Good | No | Basic | Good (Intel) | No | Excellent | Advanced | 2 hours |
| **Very Sleepy** | Basic | No | No | No | No | No | Easy | 2 min |
| **Android Profiler** | Good | No | Good | No | Excellent | Good | Moderate | 1 hour |

### Recommended Scenarios

**Scenario 1: Quick Analysis (2 hours)**
1. Qt Creator CPU Profiler (30 min) - Metric #1
2. Qt Creator QML Profiler (15 min) - Metric #2
3. Visual Studio Memory (30 min) - Metric #3
4. Visual Studio GPU (30 min) - Metric #5
5. Analyze results (15 min)

Result: Covers 4 of 6 metrics, 80% of useful information

**Scenario 2: Deep Dive (1 day)**
1. Qt Creator (1 hour) - Metrics #1, #2
2. Visual Studio (2 hours) - Metrics #3, #5
3. Windows Performance Toolkit (3 hours) - Metrics #4, #6
4. Comparative analysis (2 hours)

Result: Covers all 6 metrics, comprehensive data

**Scenario 3: Android Production (3 hours)**
1. Android Profiler (1.5 hours) - Metrics #1, #3, #5
2. systrace (30 min) - Metric #6
3. Snapdragon Profiler (1 hour) - Metric #5 detailed

Result: Real device performance, mobile GPU analysis

---

## Results Analysis

### Correlation with Code Analysis

**Metric #1: Real-Time Function Execution**

**Code Hypothesis**:
- UpdateFixPosition() ≤80ms (measured in code line 1374)
- OpenGL ~10ms (estimation)
- Q_PROPERTY ~18ms (estimation: 54 × 0.33ms)

**Profiler Validation**:
```
If Profiler shows:
  UpdateFixPosition = 75ms → Confirms code ✓
  oglBack_Paint = 28ms → 3× higher than estimated!
  Q_PROPERTY = 12ms → Order of magnitude OK ✓
```

**Action**: If discrepancies >50%, investigate why estimation was incorrect.

**Metric #2: QML Bindings Triggered**

**Code Hypothesis**:
- 888 properties declared
- 54 C++ updates × 10 Hz = 540 changes/second
- Cascade unknown (not visible in code)

**Profiler Validation**:
```
If QML Profiler shows:
  1,200 bindings triggered/frame → Cascade factor = 1200/54 = 22×
  Top binding: mapTransform (50ms) → Hotspot identified!
```

**Action**:
1. Document actual cascade (e.g., latitude - vehicle_xy - mapTransform - displayCoords)
2. Optimize top 10 expensive bindings
3. Consider Qt.callLater() for throttling

**Metric #3: Memory Allocation Hotspots**

**Code Hypothesis**:
- 94% RAM used
- No File I/O in loop - no obvious leaks

**Profiler Validation**:
```
If Memory Profiler shows:
  QString growth +25 MB/min → Leak confirmed!
    └─ FormGPS::UpdateFixPosition
       └─ QString::number().toUtf8() + ","
```

**Fix Example**:

```cpp
// BEFORE (leak)
sbGrid.append(QString::number(lat, 'f', 7).toUtf8() + ",");

// AFTER (optimized)
QByteArray buffer;
buffer.reserve(50); // Pre-allocate
buffer.append(QByteArray::number(lat, 'f', 7));
buffer.append(',');
sbGrid.append(buffer);
```

**Metric #4: Cache Miss Rates**

**Code Hypothesis**:
- UpdateFixPosition() = 723 lines - potentially large working set
- Accesses CVehicle, pn, ahrs, bnd, yt - fragmented data

**Profiler Validation**:
```
If WPT PMC shows:
  IPC = 0.6 (UpdateFixPosition) → Memory-bound!
  L3 miss rate = 8% → Cache thrashing
```

**Fix Example**:

```cpp
// BEFORE (poor locality)
m_easting = CVehicle::instance()->pivotAxlePos.easting;
m_northing = CVehicle::instance()->pivotAxlePos.northing;
// ... repeated 54×

// AFTER (better locality)
auto& vehicle = *CVehicle::instance(); // Cache pointer
const auto& pivotPos = vehicle.pivotAxlePos; // Cache reference
m_easting = pivotPos.easting;
m_northing = pivotPos.northing;
```

**Metric #5: GPU Wait Time**

**Code Hypothesis**:
- OpenGL in main thread - synchronous
- No Qt Scene Graph offload

**Profiler Validation**:
```
If VS GPU Usage shows:
  CPU wait for GPU = 30% → Major problem!
  Draw calls/frame = 124 → Too many!
```

**Action**:
1. Use Qt Scene Graph instead of raw OpenGL
2. Batch draw calls (combine geometry)
3. Remove glFinish()/glFlush() synchronous calls

**Metric #6: System Call Overhead**

**Code Hypothesis**:
- 33 timers - ~95 NtWaitForSingleObject/second
- UDP packets - NtDeviceIoControlFile

**Profiler Validation**:
```
If WPT ETW shows:
  NtWaitForSingleObject = 5,400 calls/min (90/s) → Confirms ✓
  Kernel time = 35% → Confirms Android profiler ✓
```

**Action**:
1. Merge timers (12 AgIO timers - 3 master timers)
2. Batch UDP sends (group PGN packets)

### Recommended Profiling Report

After profiling, create a structured report:

```markdown
# PROFILING REPORT - QtAgOpenGPS

## Methodology
- Tool: Qt Creator CPU + QML Profiler
- Duration: 60 seconds
- Scenario: GPS mode, field opened, autosteer active

## Results Metric #1: Real-Time Functions

| Function | Total Time | Self Time | Calls | Notes |
|----------|------------|-----------|-------|-------|
| UpdateFixPosition | 75ms (42%) | 10ms (5.6%) | 600 | Confirms ≤80ms code ✓ |
| oglBack_Paint | 28ms (15.7%) | 28ms (15.7%) | 600 | 3× initial estimation |
| QObject::setProperty | 12ms (6.7%) | 12ms (6.7%) | 32,400 | 54 props × 600 frames ✓ |
| CalculateSectionLookAhead | 10ms (5.6%) | 10ms (5.6%) | 600 | Order of magnitude OK ✓ |

**Total CPU**: 68.9% → Matches Android profiler (69%) ✓

## Results Metric #2: QML Bindings

| Binding | Evaluations | Total Time | Avg Time | Cascade |
|---------|-------------|------------|----------|---------|
| aog.latitude | 600 | 18ms | 0.03ms | - 5 bindings |
| vehicle_xy | 600 | 35ms | 0.058ms | - 12 bindings |
| mapTransform | 600 | 50ms | 0.083ms | Hotspot! |

**Total binding time**: 18% CPU
**Cascade factor**: 22× (1,200 bindings triggered / 54 C++ updates)

## Results Metric #3: Memory

| Allocation Type | Rate | Location |
|-----------------|------|----------|
| QString | +18 MB/min | QString::number in UpdateFixPosition |
| QByteArray | +8 MB/min | NMEA parsing |
| QOpenGLBuffer | Stable | No leak detected |

**Action**: Optimize QString concatenations

## Priority Recommendations

1. **P0**: Optimize oglBack_Paint (28ms - target <10ms)
   - Batch draw calls
   - Cache geometry buffers

2. **P0**: Fix QString allocation leak
   - Use QByteArray::reserve()
   - Pre-allocate buffers

3. **P1**: Optimize mapTransform binding cascade
   - Break cascade with intermediate variables
   - Use Qt.callLater() for throttling

4. **P1**: Reduce Q_PROPERTY update frequency
   - Batch updates
   - Conditional updates (only if changed)
```

---

## References

**Official Documentation**:
- Qt Creator Profiler: https://doc.qt.io/qtcreator/creator-qml-performance-monitor.html
- Visual Studio Profiler: https://learn.microsoft.com/visualstudio/profiling/
- Windows Performance Toolkit: https://learn.microsoft.com/windows-hardware/test/wpt/
- Android Profiler: https://developer.android.com/studio/profile

**Related Documentation**:
- [Memory Debugging Guide](memory-debugging.md) - Heob memory leak analysis
- [Phase 6.0.45 Validation](phase-6-0-45-validation.md) - Memory leak fix validation
- [Performance Baseline](../../analysis/performance-baseline.md) - Performance analysis results

---

**Status**: IMPLEMENTED (Phase 6.0.43+)
**Validation**: Tested with Qt Creator 12, Visual Studio 2022, Windows 11
**Last Validated**: 2025-12-06
