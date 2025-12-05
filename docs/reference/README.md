# Reference Documentation

Technical reference material for QtAgOpenGPS protocols and APIs.

## Protocol References

- [NMEA Sentences](nmea-sentences.md) - NMEA 0183 sentence format and parsing
- [PGN Sentences](pgn-sentences.md) - PGN (Parameter Group Number) protocol reference

## Protocol Overview

QtAgOpenGPS uses two main communication protocols:

**NMEA 0183**: GPS position, heading, and motion data
- Standard sentences: GGA, VTG, HDT
- Custom sentences: PANDA (GPS+IMU), PAOGI (dual antenna+IMU)
- Manufacturer proprietary: AVR (Trimble), KSXT (Unicore)

**PGN Binary Protocol**: Hardware module communication
- AutoSteer control and status
- Section control (machine module)
- IMU data (roll, pitch, yaw)
- GPS dual antenna systems

## API Documentation

API documentation will be added covering:
- FormGPS public interface
- SettingsManager properties
- AgIOService communication
- QML component APIs

## See Also

For architecture and implementation details, see:
- [Getting Started](../getting-started/) - Installation and setup
- [Development](../development/) - Building and contributing
- [Guides](../guides/) - Technical deep-dives
