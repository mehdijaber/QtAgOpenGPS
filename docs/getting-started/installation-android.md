# Building QtAgOpenGPS for Android

This guide covers building QtAgOpenGPS for Android devices from Windows or Linux.

## Prerequisites

**Critical:** You MUST be able to build QtAgOpenGPS for desktop before attempting Android builds. If desktop compilation fails, resolve those issues first.

**Requirements:**
- Qt 6.8.0+ with Android support installed
- Android SDK and NDK (can be installed via Qt)
- Java JDK 17+
- At least 15 GB free disk space
- USB debugging enabled Android device or emulator

## Part 1: Test Build with Qt Example

Before building QtAgOpenGPS for Android, verify your environment by building Qt's Calqlatr example.

### Step 1: Open Qt Example

1. Launch Qt Creator
2. Go to **Welcome** screen
3. Search for "Calqlatr" in examples
4. Open the Calqlatr calculator example

### Step 2: Configure for Android

1. Enable Android targets by checking the appropriate boxes:
   - **Android Qt 6.8.0 Clang x86_64** - For emulator testing
   - **Android Qt 6.8.0 Clang arm64-v8a** - For modern devices (64-bit)
   - **Android Qt 6.8.0 Clang armeabi-v7a** - For older devices (32-bit)

2. Select build target using the icon above Run/Debug buttons

3. Choose appropriate Kit based on your target:
   - x86_64 for emulator
   - arm64-v8a for most modern Android devices
   - armeabi-v7a for older devices

### Step 3: Deploy and Run

1. Click the **Run** button (green play icon)
2. Emulator launches automatically if x86_64 is selected
3. Qt Creator builds, deploys APK, and launches the app
4. Verify the calculator app runs correctly

If Calqlatr builds and runs successfully, your Android environment is configured correctly.

## Part 2: Platform-Specific Setup

Choose your development platform:

### Windows Setup

See [Android Development on Windows](../development/android-development.md#windows-setup) for detailed instructions including:
- Java JDK installation
- Hyper-V enablement
- Android SDK configuration
- Emulator setup

### Linux Setup

See [Android Development on Linux](../development/android-development.md#linux-setup) for detailed instructions including:
- OpenJDK installation
- Android SDK configuration via Qt Creator
- Device connection and USB debugging

## Part 3: Build QtAgOpenGPS for Android

Once your environment is verified with the Calqlatr example:

### Step 1: Open Project

1. In Qt Creator, open `CMakeLists.txt` from QtAgOpenGPS repository
2. Select Android kit (arm64-v8a for most devices)
3. Click **Configure Project**

### Step 2: Build Configuration

The project automatically disables LOCAL_QML for Android builds (QML files are compiled into resources).

Build configurations:
- **Debug**: For development with debugging enabled
- **Release**: Optimized for distribution

### Step 3: Build

1. Select **Android Qt 6.8.0 Clang arm64-v8a** kit
2. Click **Build** (Ctrl+B)
3. Compilation takes 5-15 minutes depending on your system
4. Watch for any build errors in the Issues pane

### Step 4: Deploy to Device

**Connect Device:**
1. Enable USB debugging on your Android device:
   - Go to Settings > About Phone
   - Tap "Build Number" 7 times to enable Developer Options
   - Go to Settings > Developer Options
   - Enable "USB Debugging"

2. Connect device via USB
3. Accept USB debugging authorization on device

**Deploy:**
1. Qt Creator should detect your device in the device dropdown
2. Select your device
3. Click **Run** (Ctrl+R)
4. Qt Creator installs and launches QtAgOpenGPS on your device

### Step 5: Verify Installation

On your Android device:
1. QtAgOpenGPS should launch automatically
2. Check app permissions (location, storage) if prompted
3. Verify GPS functionality if your device has GPS hardware

## Troubleshooting

### Build Fails with SDK/NDK Errors

1. Go to **Tools > Options > Devices > Android**
2. Click "Setup SDK" to download/update Android SDK
3. Verify SDK, NDK, and JDK paths are correct

### Device Not Detected

**Linux:**
- Add udev rules for your device
- Check device is in Developer Mode with USB debugging enabled
- Try different USB cable (some cables are charge-only)

**Windows:**
- Install device-specific USB drivers if needed
- Check Device Manager for unrecognized devices

**Both platforms:**
- Close and reopen Qt Creator's Devices page
- Run `adb devices` in terminal to verify device detection

### App Crashes on Launch

1. Build in Debug mode
2. Connect device and run with debugger
3. Check **Application Output** pane for crash logs
4. Verify all required Qt modules are included in deployment

### Emulator Performance Issues

- Use x86_64 image (fastest)
- Enable hardware acceleration (HAXM on Windows, KVM on Linux)
- Allocate sufficient RAM (2-4 GB) to emulator

### Build Size Too Large

Release builds should be 20-40 MB. If larger:
1. Verify Release build configuration is selected
2. Check CMake configuration for debug symbols
3. Consider using APK Analyzer to identify large resources

## APK Distribution

### Generate Signed APK

For distribution outside Google Play:

1. Build > Generate Signed Bundle / APK
2. Create or use existing keystore
3. Select release variant
4. APK location: `build/android-build/build/outputs/apk/release/`

### Install APK Manually

```bash
adb install path/to/QtAgOpenGPS.apk
```

## Performance Considerations

- OpenGL ES 2.0+ is required
- GPS updates run at 10 Hz
- Screen stays on automatically during operation
- Recommend Android 8.0 (API level 26) or newer

## Next Steps

- [Android Development Guide](../development/android-development.md) - Detailed Android platform information
- [Contributing Guidelines](../development/contributing.md) - Contribute Android improvements
- [Local QML Setup](../development/local-qml-setup.md) - Note: Not available for Android

## Additional Resources

- [Qt for Android](https://doc.qt.io/qt-6/android.html)
- [Android Studio](https://developer.android.com/studio)
- [Qt Android Examples](https://doc.qt.io/qt-6/examples-android.html)
