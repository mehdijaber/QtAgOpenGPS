# Technical Guides

Comprehensive technical documentation for QtAgOpenGPS architecture, protocols, development, implementation, and analysis.

---

## Architecture Guides

System design, architectural patterns, and component integration.

### [architecture/](architecture/)

- **[System Architecture](architecture/system-architecture.md)** - Complete QtAgOpenGPS Phase 6.0.45+ architecture overview
- **[Threading & Timers](architecture/threading-timers.md)** - Timer frequencies, threading model, real-time constraints
- **[AgIOService Architecture](architecture/agioservice-architecture.md)** - I/O coordinator, worker threads, real-time communication
- **[QML Integration](architecture/qml-integration.md)** - QML↔C++ interaction, property binding, singletons
- **[System Integration](architecture/system-integration.md)** - How all components work together
- **[Qt 6.8 Property Migration](architecture/migration-qt68-properties.md)** - QProperty and BINDABLE pattern reference
- **[Scenegraph Architecture](architecture/scenegraph.md)** - OpenGL rendering, Qt Scenegraph, graphics pipeline

---

## Protocol References

Communication protocols for NMEA, PGN, and UDP.

### [protocols/](protocols/)

- **[NMEA/PGN Architecture](protocols/nmea-pgn-architecture.md)** - Complete protocol integration and parser architecture

**See also**: [Protocol Specifications](../../reference/) - Detailed NMEA and PGN sentence references

---

## Development Guides

Developer workflows, debugging tools, and best practices.

### [development/](../development/)

- **[Profiling on Windows](../development/profiling-windows.md)** - Heob, Visual Studio profiler, memory leak detection
- **[Memory Debugging](../development/memory-debugging.md)** - Memory debugging methodology and profiling techniques
- **[Phase 6.0.45 Validation](../development/phase-6-0-45-validation.md)** - Validation results and baseline metrics
- **[QML Guidelines](../development/qml-guidelines.md)** - QML performance optimization and security best practices
- **[NMEA/PGN Parser Design](../development/nmea-pgn-parser-design.md)** - Parser architecture and design patterns

---

## Implementation References

Refactoring histories and implementation case studies.

### [implementation/](implementation/)

- **[AgIOService Refactoring](implementation/agioservice-refactoring.md)** - Phase 6.0.21+ architecture decisions (IMPLEMENTED)
- **[Threading Architecture](implementation/threading-architecture.md)** - Phase 6.0.24+ main thread unification (IMPLEMENTED)
- **[Field Persistence](implementation/field-persistence.md)** - Phase 6.0.39+ save/load optimization (IMPLEMENTED)
- **[Memory Leak Fixes](implementation/memory-leak-fixes.md)** - Phase 6.0.45 memory leak reduction (91.1% improvement)

---

## Specialized Analysis

Performance analysis, protocol investigations, and theoretical foundations.

### [analysis/](analysis/)

- **[Performance Baseline](analysis/performance-baseline.md)** - Phase 6.0.43 performance metrics and benchmarks
- **[UDP/PGN Protocol Analysis](analysis/udp-pgn-analysis.md)** - Critical protocol implementation errors and fixes
- **[GPS/IMU Interpolation Theory](analysis/gps-interpolation.md)** - 50 Hz sensor fusion theory (future feature)

---

## Architectural Proposals

Pending architectural decisions requiring team discussion and approval.

### [proposals/](proposals/)

- **[Architecture Refactoring](proposals/architecture-refactoring.md)** - Major FormGPS refactoring proposal (status: PENDING)
- **[Rendering Architecture](proposals/rendering-architecture.md)** - Scene Graph migration proposal (status: PENDING)
- **[Proposals README](proposals/README.md)** - Status tracking and lifecycle management

---

## Documentation Organization

**By Topic**:
- **Architecture**: System design and component interaction
- **Protocols**: Communication standards and parsing
- **Development**: Tools and debugging workflows
- **Implementation**: Real-world refactoring case studies
- **Analysis**: Performance studies and theoretical designs

**By Status**:
- **IMPLEMENTED** - Code exists in Phase 6.0.45+ codebase
- **ANALYSIS ONLY** - Theoretical designs, not implemented
- **TO BE DECIDED** - Future features requiring decision

---

## Related Documentation

- **[Getting Started](../getting-started/)** - Installation guides (Windows, Linux, Android)
- **[Development](../development/)** - Building, contributing, settings, QML setup
- **[Protocol References](../../reference/)** - NMEA and PGN sentence specifications
- **[Root Documentation](../../)** - Project README and main documentation hub

---

## Quick Navigation

**New to QtAgOpenGPS?**
1. Start with [System Architecture](architecture/system-architecture.md)
2. Read [Threading & Timers](architecture/threading-timers.md)
3. Understand [QML Integration](architecture/qml-integration.md)

**Debugging Issues?**
1. Check [Profiling Guide](../development/profiling-windows.md)
2. Review [Memory Debugging](../development/memory-debugging.md)
3. See [Performance Baseline](analysis/performance-baseline.md)

**Understanding Protocols?**
1. Read [NMEA/PGN Architecture](protocols/nmea-pgn-architecture.md)
2. Check [Protocol Specifications](../../reference/)
3. Review [UDP/PGN Analysis](analysis/udp-pgn-analysis.md)

**Studying Implementations?**
1. [AgIOService Refactoring](implementation/agioservice-refactoring.md) - Thread coordinator
2. [Threading Architecture](implementation/threading-architecture.md) - Main thread design
3. [Memory Leak Fixes](implementation/memory-leak-fixes.md) - 91.1% reduction case study

---

**Last Updated**: 2025-12-06
**Total Documents**: 23 professional technical guides (20 guides + 3 proposal files)
