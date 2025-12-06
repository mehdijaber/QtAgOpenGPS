# Settings Properties System

QtAgOpenGPS uses a code generation system to manage 389 configuration properties with automatic Qt binding support and .ini file persistence.

## Overview

The SettingsManager class provides a centralized configuration system with:
- **389 properties** covering all application settings
- **Automatic QML binding** via Q_OBJECT_BINDABLE_PROPERTY
- **Persistent storage** using QSettings (.ini files)
- **Code generation** from `settings_config.txt`
- **Type safety** with C++ strong typing

## Architecture

### SettingsManager Structure

```
SettingsManager (Singleton)
├─ Q_OBJECT_BINDABLE_PROPERTY declarations
├─ Automatic change notifications
├─ QSettings persistence
└─ QML accessibility
```

### Generated Files

The code generation system creates three files:

1. **settingsmanager_properties.h** - Q_PROPERTY declarations
2. **settingsmanager_members.h** - QProperty member variables
3. **settingsmanager_implementations.cpp** - Getter/setter implementations

These files are included in [classes/settingsmanager.h](../../classes/settingsmanager.h) and [classes/settingsmanager.cpp](../../classes/settingsmanager.cpp).

## Property Definition Format

Properties are defined in [settings_config.txt](../../settings_config.txt):

```
name|iniKey|defaultValue|type
```

### Fields

- **name**: Property name (camelCase, without type prefix)
- **iniKey**: Key path in .ini file (section/key format)
- **defaultValue**: Default value (type-appropriate)
- **type**: Data type (int, double, bool, QString, QColor)

### Examples

```
# Integer property
maxSections|sections/maxCount|16|int

# Double property
vehicle_width|vehicle/width|2.5|double

# Boolean property
isAutoSteerEnabled|steer/enabled|false|bool

# String property
vehicle_name|vehicle/name|Tractor|QString

# Color property
fieldColor|display/fieldColor|#00FF00|QColor
```

## Adding New Properties

### Step 1: Edit settings_config.txt

Add your property definition:

```
steering_sensitivity|steer/sensitivity|1.0|double
```

### Step 2: Run Code Generator

```bash
python3 generate_settings.py
```

This generates/updates:
- `classes/settingsmanager_properties.h`
- `classes/settingsmanager_members.h`
- `classes/settingsmanager_implementations.cpp`

### Step 3: Rebuild Project

```bash
cmake --build build
```

### Step 4: Use in Code

**C++:**
```cpp
// Get value
double sensitivity = SettingsManager::instance()->steering_sensitivity();

// Set value
SettingsManager::instance()->set_steering_sensitivity(2.0);

// Bind to property
auto binding = SettingsManager::instance()->bindable_steering_sensitivity();
```

**QML:**
```qml
import QtQuick

Item {
    // Read property
    Text {
        text: "Sensitivity: " + SettingsManager.steering_sensitivity
    }

    // Bind to property (automatic updates)
    Slider {
        value: SettingsManager.steering_sensitivity
        onValueChanged: SettingsManager.steering_sensitivity = value
    }
}
```

## Generated Property Pattern

For each property, the generator creates:

### In settingsmanager_properties.h

```cpp
Q_OBJECT_BINDABLE_PROPERTY(
    SettingsManager,
    double,
    steering_sensitivity,
    &SettingsManager::steering_sensitivityChanged
)
```

### In settingsmanager_members.h

```cpp
// Members are generated as part of Q_OBJECT_BINDABLE_PROPERTY
```

### In settingsmanager_implementations.cpp

```cpp
double SettingsManager::steering_sensitivity() const {
    return m_steering_sensitivity;
}

void SettingsManager::set_steering_sensitivity(double value) {
    m_steering_sensitivity = value;
}

QBindable<double> SettingsManager::bindable_steering_sensitivity() {
    return &m_steering_sensitivity;
}
```

## Property Types

### Supported Types

| Type | .ini Format | Example Default |
|------|-------------|----------------|
| `int` | Integer | `16` |
| `double` | Decimal | `2.5` |
| `bool` | true/false | `false` |
| `QString` | String | `"Default"` |
| `QColor` | Hex color | `#FF0000` |

### Type-Specific Considerations

**Integer (int):**
- Range: -2,147,483,648 to 2,147,483,647
- Use for counts, indices, enums represented as integers

**Double:**
- Floating-point precision
- Use for measurements, coordinates, calculations
- Format in .ini: `2.5` (decimal notation)

**Boolean (bool):**
- true/false values
- Stored in .ini as "true" or "false"
- Use for flags, toggles, enable/disable states

**QString:**
- Text strings
- Stored in .ini with quotes if containing spaces
- Use for names, paths, descriptions

**QColor:**
- Color values
- Stored in .ini as hex: `#RRGGBB` or `#AARRGGBB`
- Automatically converted to/from QColor objects

## Persistence

### Automatic Saving

SettingsManager automatically saves changed properties to disk:

**Location:**
- Windows: `C:\Users\<username>\Documents\QtAgOpenGPS\settings.ini`
- Linux: `~/.config/QtAgOpenGPS/settings.ini`
- Android: Application-specific storage

**Timing:**
- Properties saved immediately on change
- Uses QSettings atomic write operations
- Thread-safe (main thread only)

### .ini File Format

```ini
[vehicle]
width=2.5
name=Tractor
wheelbase=3.0

[steer]
enabled=true
sensitivity=1.0
maxSteerAngle=35.0

[display]
fieldColor=#00FF00
backgroundColor=#000000
```

### Manual Save/Load

```cpp
// Force save all settings
SettingsManager::instance()->sync();

// Reload settings from disk
SettingsManager::instance()->reload();
```

## Best Practices

### Property Naming

**Conventions:**
- Use camelCase without type prefix
- Descriptive, not abbreviated: `vehicleWidth` not `vehW`
- Group related properties with common prefixes:
  - `vehicle_*` - Vehicle configuration
  - `steer_*` - Steering settings
  - `display_*` - Display options
  - `field_*` - Field settings

**Examples:**
```
vehicle_width
vehicle_wheelbase
vehicle_antennaOffset
steer_maxAngle
steer_proportionalGain
display_fieldColor
display_gridSize
```

### .ini Key Organization

Use hierarchical sections:

```
vehicle/width          # Vehicle section
vehicle/wheelbase
steer/enabled          # Steer section
steer/sensitivity
display/fieldColor     # Display section
```

### Default Values

- **Sensible defaults**: Choose defaults that work for most users
- **Safe defaults**: Prefer conservative values (e.g., `false` for experimental features)
- **Type-appropriate**: Match default to data type

### Documentation

Add comments in settings_config.txt:

```
# Vehicle configuration
vehicle_width|vehicle/width|2.5|double  # Meters
vehicle_wheelbase|vehicle/wheelbase|3.0|double  # Meters

# Steering parameters
steer_maxAngle|steer/maxAngle|35.0|double  # Degrees
```

## Thread Safety

**Main Thread Only:**
SettingsManager must be accessed only from the main thread:
- QML bindings are always main thread
- C++ access from main thread only
- Worker threads must use signals to request changes

**Example (Worker Thread Communication):**
```cpp
// In worker thread - WRONG
SettingsManager::instance()->set_value(newValue);  // CRASH!

// In worker thread - CORRECT
emit requestSettingChange(newValue);

// In main thread (connected with Qt::QueuedConnection)
void onRequestSettingChange(double value) {
    SettingsManager::instance()->set_value(value);
}
```

## Property Binding

### Automatic QML Updates

QML components automatically update when properties change:

```qml
Text {
    text: "Width: " + SettingsManager.vehicle_width + "m"
    // Updates automatically when vehicle_width changes
}
```

### C++ Property Binding

Use QProperty binding for reactive updates:

```cpp
class CVehicle {
    QProperty<double> m_displayWidth;

    void setupBindings() {
        // Bind to SettingsManager property
        m_displayWidth.setBinding([this]() {
            return SettingsManager::instance()->vehicle_width() * 100.0;
        });
    }
};
```

## Migration from Old System

If migrating from property_* naming:

### Old System (Manual Properties)

```cpp
// Old style - manual property
Q_PROPERTY(double propertyVehicleWidth READ getVehicleWidth WRITE setVehicleWidth)

double getVehicleWidth() const { return vehicleWidth; }
void setVehicleWidth(double value) {
    vehicleWidth = value;
    emit vehicleWidthChanged();
}
```

### New System (Generated)

```cpp
// New style - generated from settings_config.txt
// Just add to settings_config.txt:
vehicle_width|vehicle/width|2.5|double

// No manual code needed!
```

## Debugging

### View Generated Code

Check generated files to verify property implementation:

```bash
# View property declarations
cat classes/settingsmanager_properties.h

# View member variables
cat classes/settingsmanager_members.h

# View implementations
cat classes/settingsmanager_implementations.cpp
```

### Common Issues

**Property not found in QML:**
```
Solution:
1. Verify property added to settings_config.txt
2. Run generate_settings.py
3. Rebuild project
4. Restart application
```

**Property not persisting:**
```
Solution:
1. Check .ini file location
2. Verify write permissions
3. Check QSettings error messages
4. Call sync() to force write
```

**Build fails after adding property:**
```
Solution:
1. Verify settings_config.txt syntax (4 fields, pipe-separated)
2. Check default value matches type
3. Run generate_settings.py again
4. Clean build: cmake --build build --clean-first
```

## Performance Considerations

### Property Access

- **Read**: O(1) - Direct member access
- **Write**: O(1) + disk I/O (asynchronous)
- **Binding evaluation**: Lazy, only when property changes

### Memory Usage

- 389 properties ≈ 3-4 KB base overhead
- Each property: ~8-32 bytes depending on type
- Total: ~15-20 KB for all settings

### .ini File Size

Typical settings.ini: 10-20 KB
- Human-readable format
- Atomic writes prevent corruption
- Minimal I/O impact

## Advanced Usage

### Programmatic Property Access

```cpp
// Get property by name (dynamic)
QVariant value = SettingsManager::instance()->property("vehicle_width");

// Set property by name
SettingsManager::instance()->setProperty("vehicle_width", 3.0);
```

### Property Change Notifications

```cpp
// Connect to change signal
connect(SettingsManager::instance(),
        &SettingsManager::vehicle_widthChanged,
        this, &MyClass::onWidthChanged);

void MyClass::onWidthChanged() {
    double width = SettingsManager::instance()->vehicle_width();
    qDebug() << "Width changed to:" << width;
}
```

### Batch Updates

```cpp
// Multiple property changes
SettingsManager::instance()->set_vehicle_width(3.0);
SettingsManager::instance()->set_vehicle_wheelbase(2.8);
SettingsManager::instance()->set_vehicle_antennaOffset(0.5);

// All saved to .ini file automatically
```

## Limitations

**Not applicable to QtAgIO:**
The settings properties system is specific to QtAgOpenGPS. QtAgIO (hardware I/O service) manages its configuration separately.

**Manual process for QtAgIO configuration** - settings_config.txt does not affect QtAgIO.

## See Also

- [Contributing Guidelines](contributing.md) - Code contribution workflow
- [Building QtAgOpenGPS](building.md) - Build system details
- [Local QML Setup](local-qml-setup.md) - Fast QML development
