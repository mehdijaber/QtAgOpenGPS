# Contributing to QtAgOpenGPS

Thank you for your interest in contributing to QtAgOpenGPS! This guide covers code style, development workflow, and best practices.

## Before You Start

### Communication

- **Questions**: Open a [GitHub Discussion](https://github.com/mehdijaber/QtAgOpenGPS/discussions)
- **Bug Reports**: Create an [issue](https://github.com/mehdijaber/QtAgOpenGPS/issues)
- **Feature Proposals**: Start with a discussion before implementing

We encourage asking questions before implementing extensive code changes. It's better to clarify requirements early than to rework large contributions later.

### Development Environment

Ensure you can successfully build and run QtAgOpenGPS on your development platform:
- [Windows Installation](../getting-started/installation-windows.md)
- [Linux Installation](../getting-started/installation-linux.md)

## Development Workflow

### 1. Fork and Clone

```bash
# Fork the repository on GitHub, then:
git clone https://github.com/YOUR-USERNAME/QtAgOpenGPS.git
cd QtAgOpenGPS
git remote add upstream https://github.com/mehdijaber/QtAgOpenGPS.git
```

### 2. Create Feature Branch

```bash
# Create branch from new_dev (default development branch)
git checkout new_dev
git pull upstream new_dev
git checkout -b feature/your-feature-name
```

**Branch Naming:**
- `feature/` - New features
- `fix/` - Bug fixes
- `docs/` - Documentation improvements
- `refactor/` - Code refactoring

### 3. Make Changes

Follow the code standards and testing guidelines below.

### 4. Commit Changes

```bash
git add .
git commit -m "Brief description of changes"
```

**Commit Message Format:**
```
<type>: <subject>

<body>

<footer>
```

**Types:**
- `feat` - New feature
- `fix` - Bug fix
- `docs` - Documentation changes
- `refactor` - Code refactoring
- `test` - Adding tests
- `chore` - Build/config changes

**Example:**
```
feat: Add dual antenna IMU support for PAOGI sentences

- Parse PAOGI NMEA sentence for dual antenna + IMU data
- Add heading calculation from dual antenna baseline
- Integrate roll compensation from IMU

Closes #42
```

### 5. Push and Create Pull Request

```bash
git push origin feature/your-feature-name
```

Create a Pull Request on GitHub targeting the `new_dev` branch.

## Code Standards

### Language and Framework

- **Language**: C++17
- **Framework**: Qt 6.8+
- **Build System**: CMake 3.22+
- **QML**: Qt Quick 6.8
- **Property System**: Q_OBJECT_BINDABLE_PROPERTY (SettingsManager), QProperty (other classes)
- **Threading**: Main thread for UI/Qt, worker threads for I/O only

### C++ Style

**Naming Conventions:**

The codebase follows C# AgOpenGPS naming conventions:

```cpp
// Classes: PascalCase
class FormGPS { };
class CVehicle { };

// Methods: PascalCase (C# style preserved)
void UpdatePosition();
void CalculateHeading();

// Member variables: camelCase with m_ prefix
double m_latitude;
QProperty<double> m_heading;

// Constants: UPPER_CASE
const int MAX_SECTIONS = 16;

// Enums: PascalCase
enum class FixQuality {
    NoFix,
    GPS,
    DGPS,
    RTK
};
```

**Formatting:**
- Indent: 4 spaces (no tabs)
- Braces: Opening brace on same line for methods, next line for classes
- Line length: Aim for 100 characters, max 120

**Example:**
```cpp
void FormGPS::UpdatePosition() {
    if (m_isPositionValid) {
        double distance = CalculateDistance(m_latitude, m_longitude);
        emit positionChanged();
    }
}
```

### Qt 6.8 Property System

**Use Q_OBJECT_BINDABLE_PROPERTY for SettingsManager:**

Settings are auto-generated. To add a property:
1. Edit `settings_config.txt`
2. Run `python3 generate_settings.py`
3. Rebuild project

**Use QProperty for other classes:**

```cpp
class CVehicle : public QObject {
    Q_OBJECT
    Q_PROPERTY(double wheelbase READ wheelbase WRITE setWheelbase
               NOTIFY wheelbaseChanged BINDABLE bindableWheelbase)

public:
    double wheelbase() const { return m_wheelbase; }
    void setWheelbase(double value) { m_wheelbase = value; }
    QBindable<double> bindableWheelbase() { return &m_wheelbase; }

signals:
    void wheelbaseChanged();

private:
    QProperty<double> m_wheelbase{2.5};
};
```

**Important:** Never manually emit change signals for BINDABLE properties. Qt handles this automatically.

### QML Style

**Responsive Design:**

All UIs must be scalable for multi-platform support (desktop, tablet, phone).

**Use scale factors:**
```qml
Rectangle {
    width: 500 * theme.scaleWidth   // NOT width: 500
    height: 300 * theme.scaleHeight
}
```

**Individual margin specification:**

Never use `anchors.margins`. Specify each margin individually:

```qml
// WRONG
anchors.margins: 10

// CORRECT
anchors.leftMargin: 10 * theme.scaleWidth
anchors.topMargin: 10 * theme.scaleHeight
anchors.rightMargin: 10 * theme.scaleWidth
anchors.bottomMargin: 10 * theme.scaleHeight
```

**File Organization:**
- One component per file
- Filename matches component name
- Group related components in directories

**Example:**
```
qml/
├─ components/
│  ├─ Button.qml
│  └─ TextField.qml
├─ config/
│  └─ VehicleSettings.qml
```

## Porting from C# AgOpenGPS

If porting code from the original AgOpenGPS:

### Data Type Conversions

| C# | Qt/C++ |
|---|---|
| `byte` | `quint8` |
| `char` | `QByteArray` |
| `string` | `QString` |
| `Byte[]` | `QByteArray` |
| `List<T>` | `QList<T>` |
| `Dictionary<K,V>` | `QMap<K,V>` or `QHash<K,V>` |

### Memory Management

**C# (Garbage Collection):**
```csharp
Form myForm = new Form();
// Automatic cleanup
```

**Qt (Parent-Child Ownership):**
```cpp
auto* myWidget = new QWidget(parent);
// parent deletes myWidget automatically
```

**QML Objects:**
```cpp
// Created from QML - QML engine owns it
// Use deleteLater() if deleting from C++
qmlObject->deleteLater();
```

### String Operations

**C#:**
```csharp
string result = string.Format("{0:F2}", value);
```

**Qt:**
```cpp
QString result = QString::number(value, 'f', 2);
// or
QString result = QString("%1").arg(value, 0, 'f', 2);
```

## Testing

### Before Submitting PR

1. **Build succeeds** on your platform (Windows/Linux)
2. **No compiler warnings** (treat warnings as errors)
3. **Application runs** without crashes
4. **Manual testing** of your changes
5. **Existing features** still work (regression testing)

### Testing Checklist

- [ ] GPS simulator works (built-in test mode)
- [ ] Field operations functional (boundaries, AB lines)
- [ ] Settings save and load correctly
- [ ] UI scales correctly at different window sizes
- [ ] No memory leaks (run with heob on Windows or valgrind on Linux)

### GPS Simulator

QtAgOpenGPS includes a built-in simulator for testing without GPS hardware:

```cpp
// Automatically activates in formgps_sim.cpp
// Provides:
// - 10 Hz simulated GPS position updates
// - 5-degree AB line demonstration
// - Simulated vehicle movement
```

## Code Review Process

### Pull Request Requirements

Your PR should include:

1. **Clear description** of changes
2. **Issue reference** if fixing a bug (`Closes #123`)
3. **Testing performed** and results
4. **Screenshots/videos** for UI changes
5. **Documentation updates** if adding features

### Review Checklist

Reviewers will check:
- Code follows style guidelines
- No unnecessary complexity
- Thread safety maintained
- Memory management correct
- QML responsive design principles followed
- No Qt warnings in build
- Commit messages are clear

### Addressing Feedback

```bash
# Make requested changes
git add .
git commit -m "Address review feedback"
git push origin feature/your-feature-name
```

The PR updates automatically with your new commits.

## Project Structure

Understanding the codebase organization:

```
QtAgOpenGPS/
├─ classes/              # C++ classes
│  ├─ settingsmanager.h  # Auto-generated properties
│  ├─ agioservice.h      # Hardware I/O coordinator
│  ├─ cnmea.h            # NMEA parser
│  └─ cpgn.h             # PGN protocol
├─ qml/                  # QML UI components
│  ├─ MainWindow.qml     # Main application window
│  ├─ AOG/               # Core GPS UI
│  ├─ config/            # Configuration dialogs
│  └─ components/        # Reusable components
├─ formgps*.cpp          # Main application logic
├─ main.cpp              # Application entry point
├─ settings_config.txt   # Settings definition
├─ generate_settings.py  # Settings code generator
└─ CMakeLists.txt        # Build configuration
```

### Key Files

- [formgps.h](../../formgps.h) / [formgps.cpp](../../formgps.cpp) - Main application engine
- [formgps_position.cpp](../../formgps_position.cpp) - Position calculations (10Hz)
- [formgps_opengl.cpp](../../formgps_opengl.cpp) - OpenGL rendering
- [classes/agioservice.cpp](../../classes/agioservice.cpp) - I/O coordinator
- [classes/pgnparser.cpp](../../classes/pgnparser.cpp) - PGN sentence parsing

## Documentation

### When to Update Documentation

Update documentation when:
- Adding new features
- Changing build process
- Modifying installation requirements
- Changing configuration options

### Documentation Files

Documentation is in `docs/` directory:
```
docs/
├─ README.md                    # Documentation hub
├─ getting-started/             # Installation guides
├─ development/                 # Developer guides
├─ guides/                      # Technical deep-dives
└─ reference/                   # Protocol references
```

## Communication Standards

### Language

All code, comments, documentation, commit messages, issues, and PRs MUST be in English.

### Professional Writing

- No emojis in code, comments, or documentation
- Concise, technical language
- Developer-focused content
- No AI signatures or attributions

## Getting Help

### Resources

- [Qt 6.8 Documentation](https://doc.qt.io/qt-6/)
- [CMake Documentation](https://cmake.org/documentation/)
- [C++17 Reference](https://en.cppreference.com/)
- [AgOpenGPS Discourse](https://discourse.agopengps.com/)

### Community

- [GitHub Discussions](https://github.com/mehdijaber/QtAgOpenGPS/discussions) - Questions and design discussions
- [GitHub Issues](https://github.com/mehdijaber/QtAgOpenGPS/issues) - Bug reports and feature requests

## Acknowledgments

QtAgOpenGPS builds on the work of many contributors:

- Brian Tischler for the original AgOpenGPS
- Michael Torrie (torriem) for the initial Qt port
- Muhktimar (tamirscn@gmail.com) for Qt port contributions
- David Wedel (Davidwedel) for contributions to the Qt port
- Artem (vrartem) for contributions to the Qt port
- GruniUdm for contributions to the Qt port
- AgOpenGPS community for ongoing support

Thank you for contributing to QtAgOpenGPS!
