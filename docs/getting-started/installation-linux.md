# Installing and Compiling QtAgOpenGPS on Linux

This guide covers installing Qt and building QtAgOpenGPS on Linux distributions.

## Prerequisites

- Linux distribution (Ubuntu 22.04+, Fedora 38+, or equivalent)
- At least 10 GB free disk space
- sudo/root access for package installation

## Step 1: Install System Dependencies

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake git
sudo apt install libgl1-mesa-dev libglu1-mesa-dev
sudo apt install libxcb-xinerama0 libxcb-cursor0
```

### Fedora/RHEL

```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake git gcc-c++
sudo dnf install mesa-libGL-devel mesa-libGLU-devel
sudo dnf install libxcb-devel xcb-util-cursor-devel
```

### Arch Linux

```bash
sudo pacman -S base-devel cmake git
sudo pacman -S mesa glu
sudo pacman -S xcb-util-cursor
```

## Step 2: Install Qt

### Option A: Qt Online Installer (Recommended)

1. Download the Qt Online Installer:
```bash
wget https://d13lb3tujbc8s0.cloudfront.net/onlineinstallers/qt-unified-linux-x64-online.run
chmod +x qt-unified-linux-x64-online.run
./qt-unified-linux-x64-online.run
```

2. Create Qt Account and log in
3. Select **Custom Installation**
4. Select Qt 6.8.0 or newer with these components:
   - Desktop gcc 64-bit
   - Qt 5 Compatibility Module
   - Qt Image Formats
   - Qt Multimedia
   - Qt Positioning
   - Qt Serial Port

### Option B: Distribution Packages

**Note:** Distribution packages may not include the latest Qt version (6.8+).

**Ubuntu/Debian:**
```bash
sudo apt install qt6-base-dev qt6-declarative-dev
sudo apt install qt6-serialport-dev qt6-multimedia-dev
sudo apt install qt6-positioning-dev qt6-tools-dev
sudo apt install qml6-module-qtquick-controls
```

**Fedora:**
```bash
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel
sudo dnf install qt6-qtserialport-devel qt6-qtmultimedia-devel
sudo dnf install qt6-qtpositioning-devel qt6-qttools-devel
```

## Step 3: Clone QtAgOpenGPS

```bash
git clone https://github.com/mehdijaber/QtAgOpenGPS.git
cd QtAgOpenGPS
```

## Step 4: Build QtAgOpenGPS

### Using Qt Creator (Recommended)

1. Launch Qt Creator:
```bash
qtcreator  # or ~/Qt/Tools/QtCreator/bin/qtcreator
```

2. Open `CMakeLists.txt` from the cloned repository
3. Select Desktop Qt 6.8.0 GCC 64bit kit
4. Click **Configure Project**
5. Build the project (Ctrl+B)

### Using Command Line

```bash
# Configure with CMake
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build (use -j for parallel compilation)
cmake --build build --config Release -j$(nproc)
```

The executable will be located at `build/QtAgOpenGPS`

## Step 5: Run QtAgOpenGPS

### From Qt Creator

Click the Run button or press Ctrl+R

### From Command Line

```bash
./build/QtAgOpenGPS
```

## Troubleshooting

### Missing Qt Libraries

If you get errors about missing Qt libraries:

```bash
# Check which libraries are missing
ldd build/QtAgOpenGPS

# Install missing Qt packages (Ubuntu example)
sudo apt install qt6-base-private-dev
```

### OpenGL Issues

If you encounter OpenGL errors:

```bash
# Install OpenGL development libraries
sudo apt install libgl1-mesa-dev libglu1-mesa-dev

# For NVIDIA GPU, ensure proper drivers are installed
sudo ubuntu-drivers autoinstall
```

### Serial Port Permission Denied

Add your user to the dialout group for serial port access:

```bash
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect
```

### Qt Creator Not Finding Compiler

1. Go to **Tools > Options > Kits**
2. Check **Compilers** tab - should show gcc/g++
3. If missing, manually add:
   - Path to gcc: `/usr/bin/gcc`
   - Path to g++: `/usr/bin/g++`

## Running on Raspberry Pi

QtAgOpenGPS runs on Raspberry Pi 4 and newer:

1. Use Raspberry Pi OS (64-bit recommended)
2. Install Qt from distribution packages
3. Build as described above
4. For better OpenGL performance, use:
```bash
export QT_QPA_EGLFS_FORCE888=1
```

## Next Steps

- [Local QML Setup](../development/local-qml-setup.md) - Fast QML development
- [Building for Android on Linux](installation-android.md) - Android builds from Linux
- [Contributing Guidelines](../development/contributing.md) - Contribute to the project

## Additional Resources

- [Qt for Linux](https://doc.qt.io/qt-6/linux.html)
- [Building Qt Applications](https://doc.qt.io/qt-6/build-sources.html)
- [Raspberry Pi Development](https://doc.qt.io/qt-6/raspberrypi.html)
