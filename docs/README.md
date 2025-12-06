# QtAgOpenGPS Documentation

Welcome to the QtAgOpenGPS documentation hub. This documentation covers installation, development, and architecture of QtAgOpenGPS, a Qt 6.8/C++17 port of AgOpenGPS for agricultural precision mapping and section control.

## Getting Started

New to QtAgOpenGPS? Start here:

- [Windows Installation](getting-started/installation-windows.md) - Install and compile on Windows
- [Linux Installation](getting-started/installation-linux.md) - Install and compile on Linux
- [Android Installation](getting-started/installation-android.md) - Build for Android devices

## Development Guides

Contributing to QtAgOpenGPS or building custom features:

- [Building QtAgOpenGPS](development/building.md) - Build system and compilation
- [Contributing Guidelines](development/contributing.md) - How to contribute to the project
- [Settings Properties](development/settings-properties.md) - SettingsManager architecture
- [Local QML Setup](development/local-qml-setup.md) - Fast QML development workflow
- [Android Development](development/android-development.md) - Android-specific development

## Technical Guides

In-depth technical documentation organized by domain:

- **[Architecture Guides](guides/architecture/)** - System architecture, threading, QML integration, Qt 6.8 migration
- **[Protocol References](guides/protocols/)** - NMEA/PGN architecture and integration
- **[Development Guides](guides/development/)** - Profiling, memory debugging, QML optimization
- **[Implementation References](guides/implementation/)** - Case studies of major refactorings (AgIOService, threading, memory leaks)
- **[Specialized Analysis](guides/analysis/)** - Performance baselines, protocol analysis, theoretical foundations
- **[Architectural Proposals](guides/proposals/)** - Pending architectural decisions (status tracking)

For complete guide index, see [guides/README.md](guides/README.md)

## Reference

Technical reference documentation:

- [NMEA Sentences](reference/nmea-sentences.md) - NMEA protocol reference
- [PGN Sentences](reference/pgn-sentences.md) - PGN protocol reference

## Project Information

- **Repository**: [github.com/mehdijaber/QtAgOpenGPS](https://github.com/mehdijaber/QtAgOpenGPS)
- **Original Project**: [AgOpenGPS Official](https://github.com/AgOpenGPS-Official/AgOpenGPS)
- **Upstream Qt Port**: [QtAgOpenGPS by torriem](https://github.com/torriem/QtAgOpenGPS)
- **Qt Version**: 6.8+
- **Language**: C++17
- **License**: See [LICENSE](../LICENSE)

## Contributing

We welcome contributions! Please read our [Contributing Guidelines](development/contributing.md) before submitting pull requests.

## Support

- **Issues**: [GitHub Issues](https://github.com/mehdijaber/QtAgOpenGPS/issues)
- **Discussions**: [GitHub Discussions](https://github.com/mehdijaber/QtAgOpenGPS/discussions)
