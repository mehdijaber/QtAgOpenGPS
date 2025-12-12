# Android Development for QtAgOpenGPS

Detailed guide for Android development, platform-specific considerations, and troubleshooting.

## Overview

QtAgOpenGPS supports Android 8.0 (API level 26) and newer. The application uses OpenGL ES 2.0+ for rendering and integrates with Android GPS services.

## Windows Setup

Complete setup guide for Android development on Windows.

### Prerequisites

**Java JDK 17+**
1. Download Java SE from [Oracle JDK Downloads](https://www.oracle.com/java/technologies/downloads/)
2. Install using Windows installer
3. Verify installation:
```cmd
java -version
```

**Hyper-V Enablement**
Required for Android Emulator hardware acceleration:
1. Search "features" in Windows Start menu
2. Select "Turn Windows features on or off"
3. Check "Hyper-V"
4. Reboot when prompted

**Visual Studio Build Tools (Optional)**
For building native C++ components:
1. Download [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/)
2. Select "C++ build tools" workload
3. Requires approximately 6.8 GB disk space

**Git (Recommended)**
1. Download from [https://git-scm.com/download/win](https://git-scm.com/download/win)
2. Installation options:
   - Select "Add a Git Bash Profile to Windows Terminal"
   - Set Notepad or your preferred editor for commit messages

### Qt Installation for Android

1. Run Qt Online Installer
2. Select Qt 6.8.0 or newer
3. Expand the Qt version and select:
   - MSVC 2022 64-bit (for desktop testing)
   - **Android** (all architectures or specific ones):
     - arm64-v8a (modern 64-bit devices)
     - armeabi-v7a (older 32-bit devices)
     - x86_64 (emulator)
4. Additional Libraries (check these):
   - Qt 5 Compatibility Module
   - Qt Image Formats
   - Qt Multimedia
   - Qt Positioning
   - Qt Serial Port

### Android SDK Configuration

1. Launch Qt Creator
2. Go to **Edit > Preferences > Devices > Android**
3. Click **"Setup SDK"**
4. Accept Android SDK license agreements
5. Qt Creator downloads and configures:
   - Android SDK
   - Android NDK
   - Android SDK Build Tools
   - Android SDK Platform Tools

### Android Emulator Setup

**Install Emulator:**
1. In Android preferences, click **"Manage SDK"**
2. Go to **SDK Tools** tab
3. Check "Android Emulator"
4. Click **Apply** to install

**Create Virtual Device:**
1. In Devices tab, click **"Create"**
2. Select device definition (e.g., Pixel 6)
3. Select system image:
   - **x86_64** for best performance
   - Android 13 (API 33) or newer recommended
4. Configure device:
   - RAM: 2-4 GB
   - Internal storage: 4-8 GB
   - Enable hardware acceleration
5. Click **Finish**

**Launch Emulator:**
- Select emulator from device dropdown in Qt Creator
- Click Run button
- First launch takes 2-3 minutes

## Linux Setup

Complete setup guide for Android development on Linux.

### Prerequisites

**OpenJDK 17+**

**Fedora:**
```bash
sudo dnf install java-17-openjdk-devel
```

**Ubuntu/Debian:**
```bash
sudo apt install openjdk-17-jdk
```

**Verify installation:**
```bash
java -version
```

### Qt Installation for Android

Follow [Linux Installation](../getting-started/installation-linux.md) guide, ensuring Android components are selected during Qt installation.

### Android SDK Configuration

1. Launch Qt Creator
2. Go to **Edit > Preferences > Devices > Android**
3. Click **"Set Up SDK"**

**Configure JDK Location:**
1. Browse to Java installation (typically `/usr/lib/jvm`)
2. Select OpenJDK directory (e.g., `java-17-openjdk-amd64`)

**Download OpenSSL:**
Click "Download OpenSSL" button to install Android OpenSSL support.

**Install Missing Packages:**
Qt Creator will report missing packages. Install as prompted:

**Fedora:**
```bash
sudo dnf install android-tools
```

**Ubuntu:**
```bash
sudo apt install android-sdk-platform-tools adb
```

### Device Connection

**Enable USB Debugging on Device:**
1. Go to Settings > About Phone
2. Tap Build Number 7 times
3. Go to Settings > Developer Options
4. Enable "USB Debugging"

**Connect Device:**
1. Connect Android device via USB
2. Accept USB debugging authorization on device
3. Verify connection:
```bash
adb devices
```

**Configure udev Rules (Linux):**
Create `/etc/udev/rules.d/51-android.rules`:
```
SUBSYSTEM=="usb", ATTR{idVendor}=="18d1", MODE="0666", GROUP="plugdev"
```
Reload udev rules:
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

**Device Detection in Qt Creator:**
1. Go to **Devices** tab in Android preferences
2. Your device should appear in the list
3. If not visible, close and reopen the Devices window

## Building for Android

### Build Configuration

**Automatic Settings:**
- `LOCAL_QML` is automatically disabled for Android
- QML files are compiled into application resources
- Screen stays on during application runtime

**Build Types:**
- **Debug**: Development builds with debugging symbols
- **Release**: Optimized builds for distribution

### Architecture Selection

**arm64-v8a (Recommended):**
- Modern 64-bit ARM devices
- Best performance on newer devices
- Required for Google Play (64-bit mandate)

**armeabi-v7a:**
- 32-bit ARM devices
- Older devices (pre-2015)
- Smaller APK size

**x86_64:**
- Android emulators
- Some Intel-based tablets (rare)

### Build Process

1. Open project in Qt Creator
2. Select Android kit (e.g., Android Qt 6.8.0 Clang arm64-v8a)
3. Select build configuration (Debug or Release)
4. Build project (Ctrl+B)

**Build Output:**
- Debug: `build/android-build/build/outputs/apk/debug/`
- Release: `build/android-build/build/outputs/apk/release/`

## Platform-Specific Considerations

### Screen Management

QtAgOpenGPS keeps the screen on during operation. Implementation in [main.cpp:34-43](../../main.cpp#L34-L43):

```cpp
#ifdef Q_OS_ANDROID
QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    QJniObject window = activity.callObjectMethod("getWindow",
        "()Landroid/view/Window;");
    window.callMethod<void>("addFlags", "(I)V", 0x00000080);
});
#endif
```

### Permissions

Required Android permissions (configured in AndroidManifest.xml):
- `ACCESS_FINE_LOCATION` - GPS access
- `ACCESS_COARSE_LOCATION` - Network location
- `WRITE_EXTERNAL_STORAGE` - Save field data (Android <10)
- `INTERNET` - NTRIP corrections

### OpenGL ES

QtAgOpenGPS uses OpenGL ES 2.0+ for rendering:
- Compatible with all Android devices since API 11
- Hardware acceleration enabled by default
- Shaders located in `shaders/` directory

### GPS Integration

Android GPS data flows through Qt Positioning module:
- 10 Hz update rate (when GPS supports it)
- Integrates with device GPS/GNSS hardware
- NMEA sentence processing identical to desktop

## Troubleshooting

### Common Build Issues

**Problem: Gradle Build Fails**
```
Solution:
1. Check Android SDK installation in Qt Creator preferences
2. Verify internet connection (Gradle downloads dependencies)
3. Clear Gradle cache: rm -rf ~/.gradle/caches
4. Rebuild project
```

**Problem: NDK Not Found**
```
Solution:
1. Go to Preferences > Devices > Android
2. Click "Setup SDK"
3. Install NDK from SDK Manager
4. Restart Qt Creator
```

**Problem: Java Version Mismatch**
```
Solution:
1. Verify JDK version: java -version
2. Install OpenJDK 17 if needed
3. Update JDK path in Qt Creator Android preferences
```

### Common Runtime Issues

**Problem: App Crashes on Launch**
```
Solution:
1. Build in Debug mode
2. Connect device and run with debugger attached
3. Check Application Output pane for crash logs
4. Verify all Qt dependencies are deployed:
   - Go to Projects > Run > Android deployment
   - Check "Ministro service" or "Bundle Qt libraries"
```

**Problem: GPS Not Working**
```
Solution:
1. Grant location permissions in Android app settings
2. Enable location services on device
3. Test outdoors or near window for GPS signal
4. Check Application Output for NMEA sentences
```

**Problem: Black Screen / OpenGL Issues**
```
Solution:
1. Verify device supports OpenGL ES 2.0+
2. Check OpenGL initialization in logs
3. Try different device or emulator
4. Update device graphics drivers if available
```

**Problem: Serial Port Not Working**
```
Solution:
Android does not support serial ports through Qt SerialPort.
For Arduino/hardware communication on Android:
1. Use USB OTG adapter
2. Use Android USB Host API (requires Java/Kotlin wrapper)
3. Or use network/Bluetooth communication instead
```

### Emulator Issues

**Problem: Emulator Slow**
```
Solution:
1. Use x86_64 image (not ARM)
2. Enable hardware acceleration:
   - Windows: Hyper-V or HAXM
   - Linux: KVM
3. Allocate more RAM (4 GB recommended)
4. Close other applications
```

**Problem: Emulator Won't Start**
```
Solution:
1. Check virtualization enabled in BIOS
2. Verify Hyper-V (Windows) or KVM (Linux) is installed
3. Try deleting and recreating virtual device
4. Check disk space (need 4-8 GB free)
```

## Deployment and Distribution

### Debug APK

For testing:
```bash
adb install build/android-build/build/outputs/apk/debug/android-build-debug.apk
```

### Release APK

For distribution, create signed APK:

1. Generate keystore (first time only):
```bash
keytool -genkey -v -keystore qtagopengps.keystore -alias qtagopengps \
  -keyalg RSA -keysize 2048 -validity 10000
```

2. In Qt Creator:
   - Go to **Projects > Build > Build Android APK**
   - Select "Sign package"
   - Browse to keystore
   - Enter keystore and key passwords
   - Build

3. APK location: `build/android-build/build/outputs/apk/release/`

### Google Play Considerations

- Requires 64-bit support (arm64-v8a)
- Target API level 33 or newer
- Privacy policy required for location permissions
- App signing by Google Play (recommended)

## Performance Optimization

### Reduce APK Size

1. Build Release configuration
2. Enable APK splitting by architecture:
```cmake
# In CMakeLists.txt
set(QT_ANDROID_ABIS "arm64-v8a;armeabi-v7a")
```

3. Use ProGuard for code shrinking (advanced)

### Improve Runtime Performance

1. **OpenGL**:
   - Minimize draw calls
   - Use VBO (Vertex Buffer Objects)
   - Batch rendering where possible

2. **QML**:
   - Avoid complex bindings in loops
   - Use Loaders for heavy components
   - Profile with QML Profiler

3. **GPS Processing**:
   - Already optimized for 10 Hz updates
   - NMEA parsing is lightweight

## Testing

### Testing Strategy

1. **Emulator Testing** (x86_64):
   - UI layout and responsiveness
   - Basic functionality
   - Quick iteration

2. **Device Testing** (arm64-v8a):
   - GPS functionality
   - Performance validation
   - Final verification

### Test Checklist

- [ ] App launches without crash
- [ ] UI scales correctly on device
- [ ] Location permissions granted
- [ ] GPS data received (outdoors)
- [ ] Field operations work (boundaries, AB lines)
- [ ] Settings persist across app restarts
- [ ] Screen stays on during operation
- [ ] No excessive battery drain

## Additional Resources

- [Qt for Android Documentation](https://doc.qt.io/qt-6/android.html)
- [Android Developers Guide](https://developer.android.com/guide)
- [Qt Android Examples](https://doc.qt.io/qt-6/examples-android.html)
- [Android Studio](https://developer.android.com/studio) - For advanced debugging
