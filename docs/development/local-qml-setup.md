# Local QML Development Setup

Configure QtAgOpenGPS to load QML files from disk instead of compiling them into the executable, enabling instant UI updates without recompilation.

## Overview

By default, QtAgOpenGPS compiles all QML files into the application binary using Qt's resource system. This is optimal for deployment but slow for development.

**Local QML mode** loads QML files directly from the filesystem, providing:
- **Instant updates**: Changes visible immediately without rebuilding
- **Fast iteration**: Edit-save-reload workflow (< 1 second)
- **Debug convenience**: Easier to test UI changes

**Trade-offs:**
- Development only (not for production builds)
- Requires source tree access at runtime
- Slightly slower application startup

## How It Works

### Resource Search Paths

When `LOCAL_QML` is enabled, QtAgOpenGPS searches for QML resources in these locations (relative to working directory or executable):

```
../
../qtaog/
../QtAgOpenGPS/
```

The search stops when it finds `qml/AOGInterface.qml`.

**Console output** shows which directory is being used:
```
Looking for resources in: ../QtAgOpenGPS/
Found resources at: ../QtAgOpenGPS/qml/AOGInterface.qml
```

## Configuration

### CMake Projects (Recommended)

QtAgOpenGPS uses CMake, making LOCAL_QML configuration straightforward.

#### Enable LOCAL_QML in Qt Creator

1. Open project in Qt Creator
2. Go to **Projects** tab (left sidebar)
3. Select your active kit
4. Select **Build** configuration
5. Ensure **Debug** is selected (LOCAL_QML should only be used for debug builds)
6. Under "CMake" section, find **LOCAL_QML** option
7. Check the **LOCAL_QML** checkbox
8. Click **"Apply Configuration Changes"** or **"Run CMake"**

#### Create Dedicated Build Configuration

For convenience, create a separate build configuration:

1. In **Projects > Build**, ensure Debug is active
2. Click **Add** (or **Clone**) to create new configuration
3. Name it "Debug Local QML" or similar
4. Enable **LOCAL_QML** checkbox for this configuration
5. Keep **LOCAL_QML** disabled for standard Debug configuration

You can now switch between configurations using the build configuration selector (icon above Run button).

#### Command Line Configuration

```bash
# Configure with LOCAL_QML enabled
cmake -B build -S . -DLOCAL_QML=ON -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build
```

### QMake Projects (Legacy)

If using an older QMake-based build:

```bash
qmake DEFINES+=LOCAL_QML
make
```

Or in Qt Creator:
1. Projects > Build
2. qMake section, expand **Build Steps**
3. Add to "Additional arguments": `DEFINES+=LOCAL_QML`

## Usage Workflow

### Typical Development Cycle

**Without LOCAL_QML:**
```
1. Edit QML file (30 seconds)
2. Rebuild project (60-180 seconds) ← SLOW
3. Run application (5 seconds)
4. Test changes
Total: ~2-4 minutes per iteration
```

**With LOCAL_QML:**
```
1. Edit QML file (30 seconds)
2. Reload QML (Ctrl+R or restart app: <1 second) ← FAST
3. Test changes
Total: ~30 seconds per iteration
```

### Example Workflow

**Step 1: Configure LOCAL_QML**
```bash
cmake -B build -S . -DLOCAL_QML=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**Step 2: Run Application**
```bash
./build/QtAgOpenGPS
```

**Step 3: Edit QML**
```qml
// Edit qml/components/CustomButton.qml
Button {
    text: "Click Me"
    // Change to:
    text: "Updated Button"
}
```

**Step 4: Reload**
- Restart application (or use Qt Quick Scene Graph reload if available)
- Changes visible immediately

**Repeat steps 3-4** for rapid UI development.

## Project Structure Requirements

### Directory Layout

LOCAL_QML requires the source tree to be accessible:

```
QtAgOpenGPS/
├─ build/
│  └─ QtAgOpenGPS          ← Executable location
├─ qml/
│  ├─ AOGInterface.qml     ← Searched for validation
│  ├─ MainWindow.qml
│  ├─ components/
│  └─ ...
├─ assets/                 ← Images, icons (if using LOCAL_QML for assets)
└─ CMakeLists.txt
```

### Working Directory

Qt Creator automatically sets the working directory correctly.

**If running from command line**, ensure you're in the right directory:

```bash
# Run from build directory
cd build
./QtAgOpenGPS

# Or run from project root
./build/QtAgOpenGPS
```

## Platform-Specific Notes

### Windows

LOCAL_QML works on Windows with proper path resolution:

```
Looking for resources in: ../QtAgOpenGPS/
Found: /path/to/QtAgOpenGPS/qml/AOGInterface.qml
```

### Linux

Standard Unix path resolution:

```
Looking for resources in: ../QtAgOpenGPS/
Found: /home/user/QtAgOpenGPS/qml/AOGInterface.qml
```

### Android

**LOCAL_QML is NOT available on Android.**

Android builds automatically disable LOCAL_QML and compile QML into resources. This is configured in [CMakeLists.txt](../../CMakeLists.txt):

```cmake
if(ANDROID)
    set(LOCAL_QML OFF CACHE BOOL "Disable LOCAL_QML on Android" FORCE)
endif()
```

## Troubleshooting

### QML Files Not Found

**Symptoms:**
- Application shows blank screen
- Console errors: "Cannot load QML file"

**Solutions:**

1. **Check console output** for resource search path:
```
Looking for resources in: ../
Looking for resources in: ../qtaog/
Looking for resources in: ../QtAgOpenGPS/  ← Should find here
```

2. **Verify file exists:**
```bash
ls ../QtAgOpenGPS/qml/AOGInterface.qml
# or
dir ..\QtAgOpenGPS\qml\AOGInterface.qml
```

3. **Check working directory:**
```bash
# In Qt Creator: Projects > Run > Working directory
# Should be: %{buildDir} or project root
```

### Changes Not Reflected

**Symptoms:**
- QML changes don't appear after restart

**Solutions:**

1. **Verify LOCAL_QML is enabled:**
```bash
# Check CMake cache
grep LOCAL_QML build/CMakeCache.txt
# Should show: LOCAL_QML:BOOL=ON
```

2. **Check console output on startup:**
```
Loading QML from: ../QtAgOpenGPS/qml/
```

3. **Rebuild with LOCAL_QML:**
```bash
cmake -B build -S . -DLOCAL_QML=ON
cmake --build build --clean-first
```

### QML Errors After Changes

**Symptoms:**
- Application crashes or shows QML errors
- Errors weren't present before

**Solutions:**

1. **Check QML syntax:**
```bash
qmllint qml/YourFile.qml
```

2. **Revert changes** to last working state:
```bash
git checkout qml/YourFile.qml
```

3. **Test in isolation:**
Create minimal test QML to verify syntax:
```qml
import QtQuick

Rectangle {
    width: 400
    height: 300
    color: "blue"  // Should see blue rectangle
}
```

## Best Practices

### Use for Development Only

**Enable LOCAL_QML:**
- Debug builds during active development
- QML-focused development tasks
- UI iteration and testing

**Disable LOCAL_QML:**
- Release builds
- Performance testing
- Distribution packages
- Android builds (automatically disabled)

### Build Configuration Workflow

Maintain two debug configurations:

1. **"Debug"** - Standard debug, QML compiled (slower build, faster runtime)
2. **"Debug Local QML"** - LOCAL_QML enabled (fast iteration)

Switch between them based on task:
- QML changes → Use "Debug Local QML"
- C++ changes → Use either (rebuild required anyway)
- Testing → Use "Debug" (matches release behavior)

### Git Workflow

LOCAL_QML doesn't affect source files, only build configuration.

**Safe to:**
- Edit QML files in LOCAL_QML mode
- Commit QML changes
- Share changes with team

**Not affected:**
- `CMakeLists.txt` - LOCAL_QML is a build option
- `.gitignore` - Build artifacts already ignored

### Performance Testing

**Always test performance with LOCAL_QML disabled** (standard resource system):

```bash
# Build without LOCAL_QML for performance testing
cmake -B build-release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/QtAgOpenGPS
```

LOCAL_QML adds minor filesystem overhead not present in production.

## Implementation Details

### CMakeLists.txt Configuration

The LOCAL_QML option is defined in [CMakeLists.txt](../../CMakeLists.txt):

```cmake
option(LOCAL_QML "Load QML from filesystem instead of resources" OFF)

if(LOCAL_QML AND NOT ANDROID)
    target_compile_definitions(QtAgOpenGPS PRIVATE LOCAL_QML)
    message(STATUS "LOCAL_QML enabled - loading QML from filesystem")
else()
    # Compile QML into resources
    qt_add_qml_module(QtAgOpenGPS
        URI AOG
        QML_FILES ${QML_FILES}
    )
endif()
```

### Runtime Detection

In [main.cpp](../../main.cpp):

```cpp
#ifdef LOCAL_QML
    // Set search paths for local QML loading
    engine.addImportPath("../");
    engine.addImportPath("../qtaog/");
    engine.addImportPath("../QtAgOpenGPS/");

    qDebug() << "LOCAL_QML mode: Loading from filesystem";
#else
    qDebug() << "Resource mode: QML compiled into binary";
#endif
```

## See Also

- [Building QtAgOpenGPS](building.md) - Build system and CMake configuration
- [Contributing Guidelines](contributing.md) - Development workflow
- [QML Style Guide](contributing.md#qml-style) - QML coding standards
