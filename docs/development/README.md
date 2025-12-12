# Development Documentation

Documentation for developers contributing to QtAgOpenGPS or building custom features.

## Core Development

- [Building QtAgOpenGPS](building.md) - Build system, CMake configuration, and compilation options
- [Contributing Guidelines](contributing.md) - Code style, PR workflow, and contribution process
- [Settings Properties](settings-properties.md) - SettingsManager architecture and property system
- [Local QML Setup](local-qml-setup.md) - Fast QML development without recompilation

## Platform-Specific Development

- [Android Development](android-development.md) - Android build process and platform-specific considerations

## Development Workflow

1. Create feature branch from `new_dev`
2. Make changes following code style guidelines
3. Test on target platforms
4. Submit pull request with description
5. Address review feedback

## Code Standards

- Language: C++17
- Framework: Qt 6.8+
- Build System: CMake 3.22+
- QML: Qt Quick 6.8
- Property System: Q_OBJECT_BINDABLE_PROPERTY
- Threading: Main thread + worker threads for I/O

## Getting Help

- Check existing documentation in this docs/ folder
- Open an issue for questions or bugs
- Join discussions for design questions
