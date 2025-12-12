# QtAgOpenGPS

![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![Qt Version](https://img.shields.io/badge/Qt-6.8%2B-brightgreen)
![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20Android-lightgrey)

> Agricultural precision mapping and section control software

## Overview

QtAgOpenGPS is a Qt 6.8/C++17 port of AgOpenGPS, an agricultural precision mapping and guidance system for field operations. This fork focuses on modernizing the codebase with Qt 6.8 architecture patterns, including the QProperty/BINDABLE system for automatic QML property binding.

The project provides GPS-based field guidance, section control (up to 8 sections), AB line following, and auto-steer hardware integration. The architecture has been refactored for improved thread safety, memory efficiency, and real-time performance. The AgIOService component handles all hardware I/O coordination, including GPS modules, RTK corrections via NTRIP, and section/auto-steer control modules.

This is a work in progress. Core functionality is operational with the built-in simulator, and hardware integration is complete. UI refinement and some C# AOG features remain in development.

## Documentation

Complete documentation is available in the [docs/](docs/) directory:

- [Installation Guides](docs/getting-started/) - Windows, Linux, and Android installation
- [Development Guides](docs/development/) - Building, contributing, and development workflow
- [Technical Guides](docs/guides/) - In-depth technical documentation
- [Protocol References](docs/reference/) - NMEA and PGN protocol specifications

Quick Start:
- [Windows Installation](docs/getting-started/installation-windows.md)
- [Linux Installation](docs/getting-started/installation-linux.md)
- [Contributing Guidelines](docs/development/contributing.md)

## Project Status

**Current Phase**: 6.0.45+ (Qt 6.8 Migration & Modernization)

**Working**:
- GPS position tracking (NMEA/PGN protocols)
- Field management (boundaries, AB lines, coverage tracking)
- Section control (up to 8 sections for product application)
- Auto-steer hardware integration (serial communication)
- RTK corrections (NTRIP client)
- Built-in simulator for testing

**Known Limitations**:
- UI refinement in progress (functional but not polished)
- QtAgIO connection recovery requires manual restart on connection failure
- Some features from C# AgOpenGPS not yet ported

**Recent Achievements**:
- **Phase 6.0.45**: 91% memory leak reduction (32.96 MB → 2.93 MB)
- **Phase 6.0.24**: Thread architecture refactoring (main thread + specialized workers)
- **Phase 6.0.19**: SettingsManager modernization (389 bindable properties)

See [docs/guides/proposals/](docs/guides/proposals/) for pending architectural decisions.

## Requirements

- Qt 6.8 or newer
- CMake 3.22+ (3.27 recommended)
- C++17 compiler (MSVC 2022, GCC 11+, or Clang 11+)
- OpenGL ES 2.0+ or DirectX (Windows)

Platform-specific requirements: [Installation Guides](docs/getting-started/)

## Quick Build

```bash
# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Development with live QML editing
cmake -B build -S . -DLOCAL_QML=ON
```

See [Building Guide](docs/development/building.md) for detailed instructions.

## History

QtAgOpenGPS is a Qt port of the agricultural precision mapping software AgOpenGPS:

1. **Original**: [AgOpenGPS](https://github.com/AgOpenGPS-Official/AgOpenGPS) by Brian Tischler (C#)
2. **Qt Port**: [QtAgOpenGPS](https://github.com/torriem/QtAgOpenGPS) by Michael Torrie (torriem)
3. **Current Fork**: QtAgOpenGPS by Mehdi Jaber
   - Focus: Qt 6.8 modernization
   - Architecture: QProperty/BINDABLE system
   - Ongoing improvements and refactoring

## Copyright & License

**License**: GNU General Public License v3 (GPLv3)

**Copyright**:
- Original AgOpenGPS: Brian Tischler (2016-2017)
- Qt Port: Michael Torrie ([torriem](https://github.com/torriem))
- Qt Port contributions: Muhktimar, David Wedel ([Davidwedel](https://github.com/Davidwedel)), Artem ([vrartem](https://github.com/vrartem)), [GruniUdm](https://github.com/GruniUdm)
- Current Fork: Mehdi Jaber

See [LICENSE](LICENSE) for full license text.

## Contributing

Contributions welcome! See [Contributing Guidelines](docs/development/contributing.md).

**Communication**:
- Issues: [GitHub Issues](https://github.com/mehdijaber/QtAgOpenGPS/issues)
- Discussions: [GitHub Discussions](https://github.com/mehdijaber/QtAgOpenGPS/discussions)

**Language Policy**: All code, comments, documentation, and communication in English.

## Acknowledgments

- Brian Tischler for the original AgOpenGPS
- Michael Torrie ([torriem](https://github.com/torriem)) for the initial Qt port
- Muhktimar for contributions to the Qt port
- David Wedel ([Davidwedel](https://github.com/Davidwedel)) for contributions to the Qt port
- Artem ([vrartem](https://github.com/vrartem)) for contributions to the Qt port
- [GruniUdm](https://github.com/GruniUdm) for contributions to the Qt port
- AgOpenGPS community for ongoing support
