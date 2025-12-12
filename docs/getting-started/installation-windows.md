# Installing and Compiling QtAgOpenGPS on Windows

This guide covers installing Qt and building QtAgOpenGPS on Windows 11.

## Prerequisites

- Windows 11 (Windows 10 should also work)
- At least 10 GB free disk space
- Administrator access for installation

## Step 1: Install Qt

### Download Qt Online Installer

1. Visit [https://www.qt.io/download-open-source](https://www.qt.io/download-open-source)
2. Create a Qt Account if you don't have one
3. Download the Qt Online Installer for Windows

### Run the Installer

1. Launch the Qt Online Installer
2. Log in with your Qt Account credentials
3. Select **Custom Installation** (not default/typical)

### Select Qt Components

**Qt Version:**
- Qt 6.8.0 or newer recommended
- Qt 6.7.3 has been proven to work

**Required Components** (for Desktop Qt 6.8.0):
- MSVC 2022 64-bit OR MinGW 64-bit
- Qt 5 Compatibility Module
- Qt Image Formats
- Qt Multimedia
- Qt Positioning
- Qt Serial Port

**Optional Components:**
- Skip Qt Design Studio unless needed
- Skip Preview extensions unless needed
- Skip Android unless building for Android (see [Android Installation](installation-android.md))

### Complete Installation

1. Accept license agreements
2. Wait for installer to download and install components (this may take 30-60 minutes)
3. Launch Qt Creator when installation completes

## Step 2: Install Build Tools

### Option A: MSVC Compiler (Recommended)

1. Download **Build Tools for Visual Studio** from [Visual Studio Downloads](https://visualstudio.microsoft.com/downloads/)
2. Run the installer and select "C++ build tools"
3. Requires approximately 6.8 GB disk space

### Option B: MinGW Compiler

MinGW is included with Qt if you selected it during Qt installation.

## Step 3: Optional Tools

### Git (Recommended)

1. Download Git from [https://git-scm.com/download/win](https://git-scm.com/download/win)
2. During installation:
   - Select "Add a Git Bash Profile to Windows Terminal"
   - Set Notepad as default commit message editor (or choose your preferred editor)

## Step 4: Clone QtAgOpenGPS

Open a terminal (Command Prompt, PowerShell, or Git Bash):

```bash
git clone https://github.com/mehdijaber/QtAgOpenGPS.git
cd QtAgOpenGPS
```

## Step 5: Build QtAgOpenGPS

### Using Qt Creator (Recommended)

1. Launch Qt Creator
2. Select **File > Open File or Project**
3. Navigate to the cloned repository and open `CMakeLists.txt`
4. Select your kit (MSVC 2022 64-bit or MinGW 64-bit)
5. Click **Configure Project**
6. Click the **Build** button (hammer icon) or press `Ctrl+B`

### Using Command Line

```bash
# Configure with CMake
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

The executable will be located in `build/Release/QtAgOpenGPS.exe`

## Step 6: Run QtAgOpenGPS

### From Qt Creator

Click the **Run** button (green play icon) or press `Ctrl+R`

### From Command Line

```bash
cd build/Release
.\QtAgOpenGPS.exe
```

## Troubleshooting

### Missing DLL Errors

If you get errors about missing Qt DLLs when running the executable:

1. Add Qt bin directory to your PATH, or
2. Run from Qt Creator, or
3. Use `windeployqt` to copy required DLLs:

```bash
cd build/Release
C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe QtAgOpenGPS.exe
```

### Compiler Not Found

If Qt Creator cannot find your compiler:

1. Go to **Tools > Options > Kits**
2. Check **Compilers** tab for detected compilers
3. Check **Qt Versions** tab for detected Qt installations
4. Check **Kits** tab and ensure a kit is configured with both compiler and Qt version

### Build Fails with "sections exceeded"

This is normal for SettingsManager compilation. The project automatically adds the `/bigobj` flag for MSVC to handle the large number of properties (389 Q_OBJECT_BINDABLE_PROPERTY).

## Next Steps

- [Local QML Setup](../development/local-qml-setup.md) - Fast QML development without recompilation
- [Contributing Guidelines](../development/contributing.md) - How to contribute to the project
- [Building for Android](installation-android.md) - Build for Android devices

## Additional Resources

- [Qt Creator Documentation](https://doc.qt.io/qtcreator/)
- [CMake Documentation](https://cmake.org/documentation/)
- [Qt for Windows](https://doc.qt.io/qt-6/windows.html)
