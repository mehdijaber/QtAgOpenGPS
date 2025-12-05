# NMEA Sentences Reference - AgOpenGPS Qt Port

Complete technical documentation for all NMEA sentence formats used in AgOpenGPS Qt implementation.

**Document Version**: 1.0
**Last Updated**: 2025-10-04
**Phase**: 6.0.21

---

## Table of Contents

1. [Overview](#overview)
2. [Standard NMEA Sentences](#standard-nmea-sentences)
   - [GGA - GPS Fix Data](#gga---gps-fix-data)
   - [VTG - Track and Speed](#vtg---track-and-speed)
   - [HDT - Heading True](#hdt---heading-true)
3. [AgOpenGPS Custom Sentences](#agopengps-custom-sentences)
   - [PANDA - Single Antenna + IMU](#panda---single-antenna--imu)
   - [PAOGI - Dual Antenna + IMU](#paogi---dual-antenna--imu)
4. [Manufacturer Proprietary Sentences](#manufacturer-proprietary-sentences)
   - [AVR - Trimble Dual Antenna Attitude](#avr---trimble-dual-antenna-attitude)
   - [HPD - High Precision Distance](#hpd---high-precision-distance)
   - [KSXT - Integrated GNSS/IMU](#ksxt---integrated-gnssimu)
5. [Code Variable Mapping](#code-variable-mapping)
6. [Source References](#source-references)

---

## Overview

AgOpenGPS Qt implementation supports 8 NMEA sentence types for GPS positioning, heading, and IMU data. These sentences are parsed by the PGNParser class and mapped to FormGPS properties for real-time display and control.

### Sentence Categories

**Standard NMEA-0183 (Fully Documented)**:

- GGA - Global Positioning System Fix Data
- VTG - Track Made Good and Ground Speed
- HDT - Heading True

**AgOpenGPS Custom (Proprietary)**:

- PANDA - Single antenna GPS + IMU fusion
- PAOGI - Dual antenna GPS + IMU fusion

**Manufacturer Proprietary**:

- AVR - Trimble dual antenna attitude (PTNL,AVR format)
- HPD - High precision distance component
- KSXT - Unicore integrated GNSS/IMU data

### Parser Architecture

```
GPS Module → NMEA Sentence → PGNParser → ParsedData → FormGPS Properties → QML UI
```

**Primary Files**:

- `classes/pgnparser.h` - ParsedData structure definitions
- `classes/pgnparser.cpp` - NMEA parsing implementation (lines 180-290)
- `formgps_position.cpp` - Property updates from parsed data (lines 1900-1930)
- `classes/agioservice.h` - QML property exposure (lines 115-141)

---

## Standard NMEA Sentences

### GGA - GPS Fix Data

**Standard NMEA-0183 sentence for position and quality data.**

#### Format

```
$GPGGA,hhmmss.ss,ddmm.mmmmmmm,N/S,dddmm.mmmmmmm,E/W,q,ss,h.h,alt.t,M,geoid.t,M,age,stn*CS
Field: 0      1          2         3     4           5   6 7  8   9     10 11    12 13  14 15
```

#### Field Definitions

| Field | Name           | Type          | Description                                        | Code Variable       |
| ----- | -------------- | ------------- | -------------------------------------------------- | ------------------- |
| 0     | Sentence ID    | String        | "GPGGA" or "GNGGA"                                 | -                   |
| 1     | UTC Time       | hhmmss.ss     | Time of position fix                               | `data.utcTime`    |
| 2     | Latitude       | ddmm.mmmmmmm  | Latitude in degrees and minutes                    | `data.latitude`   |
| 3     | N/S Indicator  | Char          | 'N' = North, 'S' = South                           | -                   |
| 4     | Longitude      | dddmm.mmmmmmm | Longitude in degrees and minutes                   | `data.longitude`  |
| 5     | E/W Indicator  | Char          | 'E' = East, 'W' = West                             | -                   |
| 6     | Fix Quality    | Int           | 0=Invalid, 1=GPS, 2=DGPS, 4=RTK Fixed, 5=RTK Float | `data.fixQuality` |
| 7     | Satellites     | Int           | Number of satellites used                          | `data.satellites` |
| 8     | HDOP           | Float         | Horizontal Dilution of Precision                   | `data.hdop`       |
| 9     | Altitude       | Float         | Altitude above mean sea level                      | `data.altitude`   |
| 10    | Altitude Units | Char          | 'M' = Meters                                       | -                   |
| 11    | Geoid Height   | Float         | Height of geoid above WGS84 ellipsoid              | -                   |
| 12    | Geoid Units    | Char          | 'M' = Meters                                       | -                   |
| 13    | Age            | Float         | Time since last DGPS update (seconds)              | `data.age`        |
| 14    | Station ID     | String        | DGPS station ID                                    | -                   |
| 15    | Checksum       | Hex           | *CS where CS is 2-digit hex checksum               | -                   |

#### Example

```
$GPGGA,055129.00,5326.1729618,N,11109.6028200,W,4,12,0.9,300.0,M,46.9,M,1.2,*47
```

**Decoded**:

- Time: 05:51:29.00 UTC
- Position: 53°26.1729618'N, 111°09.6028200'W
- Fix Quality: 4 (RTK Fixed)
- Satellites: 12
- HDOP: 0.9
- Altitude: 300.0 meters
- Age of correction: 1.2 seconds

#### Parser Implementation

**Source**: `classes/pgnparser.cpp` lines 180-200

```cpp
void PGNParser::parseGGA(const QStringList& fields, ParsedData& data) {
    if (fields.size() < 15) return;

    data.latitude = convertNMEAToDecimal(fields[2], fields[3]);
    data.longitude = convertNMEAToDecimal(fields[4], fields[5]);
    data.fixQuality = fields[6].toInt();
    data.satellites = fields[7].toInt();
    data.hdop = fields[8].toDouble();
    data.altitude = fields[9].toDouble();
    data.age = fields[13].toDouble();
}
```

#### FormGPS Property Mapping

| ParsedData          | FormGPS Property | QML Property               |
| ------------------- | ---------------- | -------------------------- |
| `data.latitude`   | `m_latitude`   | `aog.latitude`           |
| `data.longitude`  | `m_longitude`  | `aog.longitude`          |
| `data.fixQuality` | `m_fixQuality` | `aog.fixQuality`         |
| `data.satellites` | `m_satellites` | `AgIOService.satellites` |
| `data.hdop`       | `m_hdop`       | `aog.hdop`               |
| `data.altitude`   | `m_altitude`   | `aog.altitude`           |
| `data.age`        | `m_age`        | `aog.age`                |

---

### VTG - Track and Speed

**Standard NMEA-0183 sentence for course and speed over ground.**

#### Format

```
$GPVTG,cogt,T,cogm,M,sog_kn,N,sog_kph,K,mode*CS
Field: 0      1    2  3    4  5      6  7       8  9
```

#### Field Definitions

| Field | Name               | Type   | Description                               | Code Variable    |
| ----- | ------------------ | ------ | ----------------------------------------- | ---------------- |
| 0     | Sentence ID        | String | "GPVTG" or "GNVTG"                        | -                |
| 1     | COG True           | Float  | Course over ground (degrees true)         | `data.heading` |
| 2     | True Indicator     | Char   | 'T' = True                                | -                |
| 3     | COG Magnetic       | Float  | Course over ground (degrees magnetic)     | -                |
| 4     | Magnetic Indicator | Char   | 'M' = Magnetic                            | -                |
| 5     | SOG Knots          | Float  | Speed over ground in knots                | -                |
| 6     | Knots Indicator    | Char   | 'N' = Knots                               | -                |
| 7     | SOG km/h           | Float  | Speed over ground in km/h                 | `data.speed`   |
| 8     | km/h Indicator     | Char   | 'K' = Kilometers per hour                 | -                |
| 9     | Mode               | Char   | A=Autonomous, D=Differential, E=Estimated | -                |

#### Example

```
$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48
```

**Decoded**:

- True heading: 54.7°
- Magnetic heading: 34.4°
- Speed: 5.5 knots = 10.2 km/h

#### Parser Implementation

**Source**: `classes/pgnparser.cpp` lines 250-260

```cpp
void PGNParser::parseVTG(const QStringList& fields, ParsedData& data) {
    if (fields.size() < 9) return;

    if (!fields[1].isEmpty()) {
        data.heading = fields[1].toDouble();
    }
    if (!fields[7].isEmpty()) {
        data.speed = fields[7].toDouble();
    }
}
```

#### FormGPS Property Mapping

| ParsedData       | FormGPS Property  | QML Property        |
| ---------------- | ----------------- | ------------------- |
| `data.heading` | `m_headingTrue` | `aog.headingTrue` |
| `data.speed`   | `m_speedKph`    | `aog.speedKph`    |

---

### HDT - Heading True

**Standard NMEA-0183 sentence for true heading from dual antenna systems.**

#### Format

```
$GPHDT,heading,T*CS
Field: 0      1       2
```

#### Field Definitions

| Field | Name           | Type   | Description                     | Code Variable       |
| ----- | -------------- | ------ | ------------------------------- | ------------------- |
| 0     | Sentence ID    | String | "GPHDT" or "GNHDT"              | -                   |
| 1     | Heading        | Float  | True heading in degrees (0-360) | `data.headingHDT` |
| 2     | True Indicator | Char   | 'T' = True heading              | -                   |

#### Example

```
$GPHDT,123.456,T*00
```

**Decoded**:

- True heading: 123.456° (from dual antenna moving baseline)

#### Description

The HDT sentence provides true heading computed from a dual antenna GPS system's moving baseline vector. This is more accurate than GPS course over ground at low speeds and provides instant heading when stationary.

**Requirements**:

- Two GPS antennas mounted with known baseline distance
- RTK or DGPS quality fix on both antennas
- Sufficient baseline length (typically >0.5m for agricultural use)

#### Usage in AgOpenGPS

HDT heading is used when:

1. Vehicle speed < 0.5 km/h (stationary or very slow)
2. Dual antenna system is configured and available
3. HDT data quality is good (both antennas have RTK fix)

At higher speeds, VTG course over ground may be used instead.

#### FormGPS Property Mapping

| ParsedData          | FormGPS Property | QML Property       |
| ------------------- | ---------------- | ------------------ |
| `data.headingHDT` | `m_headingHDT` | `aog.headingHDT` |

---

## AgOpenGPS Custom Sentences

### PANDA - Single Antenna + IMU

**AgOpenGPS proprietary sentence combining GPS data with IMU measurements.**

#### Format

```
$PANDA,time,lat,N/S,lon,E/W,q,ss,hdop,alt,age,spd,hdg,roll,pitch,yaw*CS
Field: 0     1    2   3   4   5   6 7  8    9   10  11  12  13   14    15
```

#### Field Definitions

| Field | Name        | Type          | Description               | Code Variable       | Unit       |
| ----- | ----------- | ------------- | ------------------------- | ------------------- | ---------- |
| 0     | Sentence ID | String        | "PANDA"                   | -                   | -          |
| 1     | UTC Time    | hhmmss.ss     | Time of position fix      | `data.utcTime`    | -          |
| 2     | Latitude    | ddmm.mmmmmmm  | Latitude degrees/minutes  | `data.latitude`   | degrees    |
| 3     | N/S         | Char          | 'N' or 'S'                | -                   | -          |
| 4     | Longitude   | dddmm.mmmmmmm | Longitude degrees/minutes | `data.longitude`  | degrees    |
| 5     | E/W         | Char          | 'E' or 'W'                | -                   | -          |
| 6     | Fix Quality | Int           | 0-8 (same as GGA)         | `data.fixQuality` | -          |
| 7     | Satellites  | Int           | Number of satellites      | `data.satellites` | -          |
| 8     | HDOP        | Float         | Horizontal DOP            | `data.hdop`       | -          |
| 9     | Altitude    | Float         | Altitude MSL              | `data.altitude`   | meters     |
| 10    | Age         | Float         | Differential age          | `data.age`        | seconds    |
| 11    | Speed       | Float         | Speed over ground         | `data.speed`      | km/h       |
| 12    | IMU Heading | Int           | Heading from IMU/compass  | `data.imuHeading` | degrees    |
| 13    | IMU Roll    | Int           | Roll angle from IMU       | `data.imuRoll`    | degrees    |
| 14    | IMU Pitch   | Int           | Pitch angle from IMU      | `data.imuPitch`   | degrees    |
| 15    | Yaw Rate    | Int           | Angular velocity x10      | `data.yawRate`    | deg/s * 10 |

#### Example

```
$PANDA,145331.50,5326.1729618,N,11109.6028200,W,4,12,0.9,300,3.2,10.5,67,2,5,45*7E
```

**Decoded**:

- Time: 14:53:31.50 UTC
- Position: 53°26.1729618'N, 111°09.6028200'W
- Fix: RTK Fixed with 12 satellites
- Speed: 10.5 km/h
- IMU Heading: 67° (heading from compass/IMU)
- Roll: 2° (right roll)
- Pitch: 5° (nose up)
- Yaw Rate: 4.5°/s (45/10)

#### Firmware Source

**Reference**: AIO firmware `zHandlers.ino` lines 320-333

```cpp
void BuildNmea(void) {
    // Fields 12-15: IMU data
    strcat(nmea, imuHeading);  // Field 12: Heading in degrees
    strcat(nmea, ",");
    strcat(nmea, imuRoll);     // Field 13: Roll in degrees
    strcat(nmea, ",");
    strcat(nmea, imuPitch);    // Field 14: Pitch in degrees
    strcat(nmea, ",");
    strcat(nmea, imuYawRate);  // Field 15: Yaw rate x10
}
```

#### Parser Implementation

**Source**: `classes/pgnparser.cpp` lines 226-240

```cpp
// Fields 12-15: IMU data (from firmware zHandlers.ino BuildNmea)
// Field 12: IMU Heading in degrees
if (fields.size() >= 13) data.imuHeading = fields[12].toInt();

// Field 13: Roll angle in degrees
if (fields.size() >= 14) data.imuRoll = fields[13].toInt();

// Field 14: Pitch angle in degrees
if (fields.size() >= 15) data.imuPitch = fields[14].toInt();

// Field 15: Yaw Rate in degrees/sec (INTEGER x10)
if (fields.size() >= 16) {
    int yawRateRaw = fields[15].toInt();
    data.yawRate = yawRateRaw / 10.0;  // Convert x10 integer to degrees/sec
}
```

#### IMU Data Detection

```cpp
// Mark as having IMU data if any IMU field is non-zero
if (data.imuRoll != 0 || data.imuPitch != 0 || data.imuHeading != 0) {
    data.hasIMU = true;
}
```

#### FormGPS Property Mapping

| ParsedData          | FormGPS Property  | QML Property       |
| ------------------- | ----------------- | ------------------ |
| GPS fields          | (same as GGA/VTG) | (see above)        |
| `data.imuHeading` | `m_imuHeading`  | `aog.imuHeading` |
| `data.imuRoll`    | `m_imuRoll`     | `aog.imuRoll`    |
| `data.imuPitch`   | `m_imuPitch`    | `aog.imuPitch`   |
| `data.yawRate`    | `m_yawRate`     | `aog.yawRate`    |

**Property Update**: `formgps_position.cpp` lines 1915-1922

```cpp
// Store IMU data if present
if (data.hasIMU) {
    setImuRoll(data.imuRoll);
    setImuPitch(data.imuPitch);
    if (data.imuHeading != 0.0) {
        setImuHeading(data.imuHeading);
    }
}
```

---

### PAOGI - Dual Antenna + IMU

**AgOpenGPS proprietary sentence for dual antenna GPS with fused IMU data.**

#### Format

```
$PAOGI,time,lat,N/S,lon,E/W,q,ss,hdop,alt,geoid,spd,hdg,roll,pitch,yaw,T*CS
Field: 0      1    2   3   4   5   6 7  8    9   10    11  12  13   14    15  16
```

#### Field Definitions

Similar to PANDA but with dual antenna heading fusion.

| Field | Name           | Type  | Description                        | Note                       |
| ----- | -------------- | ----- | ---------------------------------- | -------------------------- |
| 0-11  | GPS Data       | -     | Same as PANDA/GGA                  | -                          |
| 12    | Heading        | Float | Fused heading (dual antenna + IMU) | Higher precision           |
| 13    | Roll           | Float | Roll angle                         | From dual antenna baseline |
| 14    | Pitch          | Float | Pitch angle                        | From IMU                   |
| 15    | Yaw Rate       | Float | Angular velocity                   | From IMU gyro              |
| 16    | True Indicator | Char  | 'T' = True heading                 | -                          |

#### Key Difference from PANDA

**PANDA**: Single antenna GPS + standalone IMU heading
**PAOGI**: Dual antenna GPS heading + IMU fusion for improved accuracy

The dual antenna system provides:

- True heading at all speeds (including stationary)
- Roll angle from antenna baseline geometry
- Yaw rate from IMU gyroscope
- Fused heading combining GPS and IMU for optimal performance

#### Example

```
$PAOGI,055129.00,5326.1729618,N,11109.6028200,W,4,12,0.9,300,M,46.9,M,10.5,123.456,2.1,0.12,359.9,T*5A
```

**Decoded**:

- Fused heading: 123.456° (dual antenna + IMU kalman filter)
- Roll: 2.1° (from dual antenna baseline)
- Pitch: 0.12° (from IMU)
- Yaw rate: 359.9°/s (very fast turn)

#### Usage Priority

When both PANDA and PAOGI are available:

1. Use PAOGI heading (more accurate, works at all speeds)
2. Use PAOGI roll (from dual antenna geometry)
3. Use PANDA pitch and yaw rate (from IMU sensors)

#### FormGPS Property Mapping

Same as PANDA but with higher priority for heading and roll values.

---

## Manufacturer Proprietary Sentences

### AVR - Trimble Dual Antenna Attitude

**Trimble proprietary NMEA sentence for dual antenna attitude data.**

#### Format

```
$PTNL,AVR,time,yaw,Yaw,tilt,Tilt,roll,Roll,baseline,q,pdop,sat*CS
Field: 0    1   2    3   4   5    6    7    8    9        10 11   12
```

#### Field Definitions

| Field | Name        | Type      | Description                                     | Unit    |
| ----- | ----------- | --------- | ----------------------------------------------- | ------- |
| 0     | Sentence ID | String    | "PTNL" (Proprietary Trimble Navigation Limited) | -       |
| 1     | Subsystem   | String    | "AVR" (Attitude Vector Record)                  | -       |
| 2     | UTC Time    | hhmmss.ss | Time of measurement                             | -       |
| 3     | Yaw         | Float     | Heading/yaw angle                               | degrees |
| 4     | Yaw Label   | String    | "Yaw"                                           | -       |
| 5     | Tilt        | Float     | Tilt/pitch angle                                | degrees |
| 6     | Tilt Label  | String    | "Tilt"                                          | -       |
| 7     | Roll        | Float     | Roll angle                                      | degrees |
| 8     | Roll Label  | String    | "Roll"                                          | -       |
| 9     | Baseline    | Float     | Distance between antennas                       | meters  |
| 10    | Quality     | Int       | Solution quality (3 = RTK fixed)                | -       |
| 11    | PDOP        | Float     | Position DOP                                    | -       |
| 12    | Satellites  | Int       | Number of satellites                            | -       |

#### Example

```
$PTNL,AVR,212405.20,+52.1531,Yaw,-0.0806,Tilt,2.3456,Roll,1.575,3,1.4,16*39
```

**Decoded**:

- Time: 21:24:05.20 UTC
- Yaw (Heading): 52.1531°
- Tilt (Pitch): -0.0806° (nose down)
- Roll: 2.3456° (right roll)
- Baseline: 1.575 meters
- Quality: 3 (RTK fixed)
- PDOP: 1.4
- Satellites: 16

#### Trimble Baseline Configuration

The baseline field indicates the physical distance between the two GPS antennas. For agricultural applications:

- Minimum recommended: 0.5 meters
- Typical: 1.0 - 2.0 meters
- Longer baseline = better heading accuracy
- Baseline must be configured in receiver settings

#### AgOpenGPS Usage

AVR sentence is used when:

1. Trimble dual antenna receiver is configured
2. Both antennas have RTK or DGPS fix
3. Baseline solution quality is good (typically quality >= 3)

The yaw field provides true heading independent of vehicle motion.

#### Simulator Implementation

**Source**: ModSim `Controls.Designer.cs` lines 327-343

```csharp
private void BuildAVR() {
    sbAVR.Clear();
    sbAVR.Append("$PTNL,AVR,");
    sbAVR.Append(TimeNow);
    sbAVR.Append(degrees.ToString("N5")); // Yaw
    sbAVR.Append(",Yaw,-2.1,Tilt,");
    sbAVR.Append(roll.ToString() + ",Roll,");
    sbAVR.Append("444.232,3,1.2,17*"); // baseline, quality, pdop, sats
    // ... checksum
}
```

---

### HPD - High Precision Distance

**Sentence for high-precision distance component in dual antenna systems.**

#### Format

```
[Format not publicly documented - proprietary implementation]
```

#### Description

Based on firmware source code (`zRelPos.ino` line 24), `relPosHPD` is the high-precision distance component extracted from UBX-NAV-RELPOSNED messages:

```cpp
double relPosHPD = (signed char)ackPacket[40];
relPosHPD *= 0.01;
relPosD += relPosHPD;
```

This appears to be a high-precision correction factor (in centimeters) added to the relative position distance between dual antennas.

#### Implementation Status

**Code Reference**: `classes/agioservice.cpp` line 2580

```cpp
if (data.contains("$GPHPD")) {
    return true;  // Recognized as GPS data
}
```

**Simulator**: NOT implemented in ModSim (placeholder only)

**Parser**: No dedicated parsing function found

#### Likely Usage

HPD data is probably transmitted internally within the dual antenna receiver system (e.g., from secondary antenna to primary antenna) but may not be output as a standalone NMEA sentence. The high-precision component is likely embedded in other proprietary messages.

---

### KSXT - Integrated GNSS/IMU

**Unicore proprietary sentence for integrated GNSS/INS data.**

#### Format

```
$KSXT,time,lon,lat,alt,yaw,pitch,spd_ang,spd,roll,pos_stat,hdg_stat,hdg_sat,pos_sat,base_e,base_n,base_u,vel_e,vel_n,vel_u,age,base_sat,,,chk
Field: 0    1    2   3   4   5   6     7       8   9    10       11       12      13      14     15     16     17    18    19    20  21       22 23
```

#### Field Definitions

| Field | Name            | Type              | Description                           | Unit    |
| ----- | --------------- | ----------------- | ------------------------------------- | ------- |
| 0     | Sentence ID     | String            | "KSXT"                                | -       |
| 1     | Time            | YYYYMMDDhhmmss.ss | Satellite time                        | -       |
| 2     | Longitude       | Float             | Longitude                             | degrees |
| 3     | Latitude        | Float             | Latitude                              | degrees |
| 4     | Altitude        | Float             | Height above ellipsoid                | meters  |
| 5     | Yaw             | Float             | True heading (0-360°)                | degrees |
| 6     | Pitch           | Float             | Pitch angle (-90 to 90°)             | degrees |
| 7     | Speed Angle     | Float             | Course over ground (0-360°)          | degrees |
| 8     | Speed           | Float             | Ground speed                          | km/h    |
| 9     | Roll            | Float             | Roll angle (-90 to 90°)              | degrees |
| 10    | Position Status | Int               | 0=invalid, 1=single, 2=float, 3=fixed | -       |
| 11    | Heading Status  | Int               | 0=invalid, 1=single, 2=float, 3=fixed | -       |
| 12    | Heading Sats    | Int               | Satellites used for heading           | -       |
| 13    | Position Sats   | Int               | Satellites used for positioning       | -       |
| 14    | Base East       | Float             | East position relative to base        | meters  |
| 15    | Base North      | Float             | North position relative to base       | meters  |
| 16    | Base Up         | Float             | Up position relative to base          | meters  |
| 17    | Velocity East   | Float             | East velocity component               | km/h    |
| 18    | Velocity North  | Float             | North velocity component              | km/h    |
| 19    | Velocity Up     | Float             | Up velocity component                 | km/h    |
| 20    | Age             | Float             | Age of differential                   | seconds |
| 21    | Base Satellites | Int               | Satellites tracked by base            | -       |
| 22-23 | Reserved        | -                 | Reserved fields                       | -       |

#### Example

```
$KSXT,20191219093115.00,112.87713062,28.23315515,65.5618,45.67,0.00,336.65,0.010,2.3,3,3,13,23,-1075.146,-98.462,-8.618,-0.004,0.009,0.004,1.0,30,,,*3FCF0C9B
```

**Decoded**:

- Time: 2019-12-19 09:31:15.00
- Position: 112.87713062°E, 28.23315515°N, 65.5618m altitude
- Yaw (Heading): 45.67° true
- Pitch: 0.00°
- Speed angle (COG): 336.65°
- Speed: 0.010 km/h
- Roll: 2.3°
- Position status: 3 (RTK fixed)
- Heading status: 3 (RTK fixed - good dual antenna solution)
- Heading satellites: 13
- Position satellites: 23

#### Key Features

**Integrated Solution**:

- Combined GNSS positioning and INS (Inertial Navigation System)
- Provides heading even when stationary
- 6-axis motion data (position + orientation)

**Solution Quality**:

- Field 10 (position status): indicates RTK solution quality
- Field 11 (heading status): indicates dual antenna heading quality
- Both should be 3 (RTK fixed) for best performance

**Base Station Data**:

- Fields 14-16 provide vector to base station
- Fields 17-19 provide velocity components
- Useful for RTK network diagnostics

#### Simulator Implementation

**Source**: ModSim `Controls.Designer.cs` lines 391-476 (with complete documentation)

```csharp
private void BuildKSXT() {
    sbKSXT.Clear();
    sbKSXT.Append("$KSXT,");
    sbKSXT.Append(TimeNow);  // YYYYMMDD format
    sbKSXT.Append(longitude.ToString("0000.0000000")).Append(',');
    sbKSXT.Append(latitude.ToString("0000.0000000")).Append(',');
    sbKSXT.Append(altitude.ToString()).Append(',');
    sbKSXT.Append(degrees.ToString("N5"));  // Yaw/heading
    sbKSXT.Append(",22,35,");  // Pitch, speed angle
    sbKSXT.Append(speed.ToString()).Append(',');
    sbKSXT.Append(roll.ToString()).Append(",3,3,13,-1075,-98,-8,,,,37,13,,");
    sbKSXT.Append("*3FCF0C9B");  // Fixed checksum
}
```

#### AgOpenGPS Usage

KSXT is detected and processed as valid GPS data:

**Source**: `classes/agioservice.cpp` line 2580

```cpp
if (data.contains("$KSXT")) {
    return true;  // Recognized as GNSS data
}
```

However, full parsing implementation for extracting all KSXT fields is not yet implemented in PGNParser. Current implementation relies on ArduPilot integration which extracts:

- Velocity components (fields 17-18) for speed calculation
- Fix status (field 10) for quality indication
- Heading status (field 11) for dual antenna validation

---

## Code Variable Mapping

### ParsedData Structure

**Definition**: `classes/pgnparser.h` lines 40-70

```cpp
struct ParsedData {
    // GPS Position Data (from GGA, PANDA, PAOGI)
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    QString utcTime;

    // GPS Quality (from GGA, PANDA, PAOGI)
    int fixQuality = 0;
    int satellites = 0;
    double hdop = 0.0;
    double age = 0.0;

    // Motion Data (from VTG, PANDA, PAOGI)
    double speed = 0.0;          // km/h
    double heading = 0.0;        // degrees from VTG
    double headingHDT = 0.0;     // degrees from HDT

    // IMU Data (from PANDA field 12-15 OR PGN 129)
    // Reference: AIO firmware zHandlers.ino BuildNmea() lines 320-333
    double imuHeading = 0.0;     // degrees (PANDA field 12 - heading/yaw)
    double imuRoll = 0.0;        // degrees (PANDA field 13 - roll)
    double imuPitch = 0.0;       // degrees (PANDA field 14 - pitch)
    double yawRate = 0.0;        // degrees/sec (PANDA field 15 - yaw rate x10)

    // Dual Antenna Data (from PAOGI, AVR, HDT)
    double rollGPS = 0.0;        // degrees (from dual antenna baseline)

    // Status Flags
    bool hasIMU = false;
    bool hasDualAntenna = false;
};
```

### FormGPS Property Storage

**Core Properties**: `formgps.h`

```cpp
// GPS Position (Qt 6.8 QProperty + BINDABLE)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_latitude)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_longitude)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_altitude)

// GPS Quality
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_fixQuality)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, int, m_satellites)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_hdop)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_age)

// Motion
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_speedKph)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_headingTrue)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_headingHDT)

// IMU Data
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_imuHeading)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_imuRoll)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_imuPitch)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_yawRate)
Q_OBJECT_BINDABLE_PROPERTY(FormGPS, double, m_rollGPS)
```

### QML Property Exposure

**Main Properties**: Available as `aog.propertyName` in QML

**Additional Properties**: `classes/agioservice.h` lines 115-141

```cpp
// NMEA Sentence Storage (full sentence strings)
Q_PROPERTY(QString ggaSentence READ ggaSentence WRITE setGgaSentence ...)
Q_PROPERTY(QString vtgSentence READ vtgSentence WRITE setVtgSentence ...)
Q_PROPERTY(QString pandaSentence READ pandaSentence WRITE setPandaSentence ...)
Q_PROPERTY(QString paogiSentence READ paogiSentence WRITE setPaogiSentence ...)
Q_PROPERTY(QString hdtSentence READ hdtSentence WRITE setHdtSentence ...)
Q_PROPERTY(QString avrSentence READ avrSentence WRITE setAvrSentence ...)
Q_PROPERTY(QString hpdSentence READ hpdSentence WRITE setHpdSentence ...)
Q_PROPERTY(QString sxtSentence READ sxtSentence WRITE setSxtSentence ...)

// NMEA Sentence Aliases (short names for QML - Phase 6.0.21 - READ-ONLY)
Q_PROPERTY(QString gga READ gga NOTIFY ggaSentenceChanged BINDABLE bindableGga)
Q_PROPERTY(QString vtg READ vtg NOTIFY vtgSentenceChanged BINDABLE bindableVtg)
Q_PROPERTY(QString panda READ panda NOTIFY pandaSentenceChanged BINDABLE bindablePanda)
Q_PROPERTY(QString paogi READ paogi NOTIFY paogiSentenceChanged BINDABLE bindablePaogi)
Q_PROPERTY(QString hdt READ hdt NOTIFY hdtSentenceChanged BINDABLE bindableHdt)
Q_PROPERTY(QString avr READ avr NOTIFY avrSentenceChanged BINDABLE bindableAvr)
Q_PROPERTY(QString hpd READ hpd NOTIFY hpdSentenceChanged BINDABLE bindableHpd)
Q_PROPERTY(QString sxt READ sxt NOTIFY sxtSentenceChanged BINDABLE bindableSxt)
```

**QML Usage Example**: `qml/agio/GPSInfo.qml`

```qml
Comp.Text {
    text: qsTr("Lat: ") + Number(aog.latitude).toLocaleString(Qt.locale(), 'f', 7)
}
Comp.Text {
    text: qsTr("# Sats: ") + AgIOService.satellites
}
Comp.Text {
    text: "GGA: " + AgIOService.gga  // Full NMEA sentence
}
```

### Data Flow Summary

```
NMEA Sentence String
    ↓
PGNParser::parse(sentence)
    ↓
parseGGA/parseVTG/parsePANDA/etc.
    ↓
ParsedData structure filled
    ↓
emit parsedData(data) signal
    ↓
FormGPS::updateFromParsedData(data)
    ↓
setLatitude/setLongitude/setImuHeading/etc.
    ↓
Qt 6.8 QProperty auto-notification
    ↓
QML UI auto-updates (aog.latitude, etc.)
```

---

## Source References

### Firmware Sources

**AIO v4 Firmware** - AgOpenGPS hardware module firmware

- `zHandlers.ino` lines 320-333: PANDA sentence generation with IMU fields
- `zHandlers.ino` lines 413-463: GGA and VTG format documentation
- `zRelPos.ino` line 24: High-precision distance component (relPosHPD)

### Parser Implementation

**Qt AgOpenGPS Parser** - NMEA parsing and data extraction

- `classes/pgnparser.h` lines 40-70: ParsedData structure definition
- `classes/pgnparser.cpp` lines 180-290: NMEA sentence parsing functions
- `classes/pgnparser.cpp` lines 226-240: PANDA IMU field parsing
- `classes/pgnparser.cpp` line 282: IMU data detection logic

### Property Updates

**FormGPS Integration** - Parsed data to application properties

- `formgps_position.cpp` lines 1900-1930: GPS position update logic
- `formgps_position.cpp` lines 1915-1922: IMU data storage

### QML Exposure

**AgIOService** - NMEA sentence storage and QML interface

- `classes/agioservice.h` lines 115-141: Q_PROPERTY declarations for sentences
- `classes/agioservice.cpp` lines 192-222: Getter/setter implementations
- `classes/agioservice.cpp` line 2580: NMEA sentence detection (KSXT, HPD)

### Simulator Reference

**ModSim** - AgOpenGPS hardware simulator (C# .NET)

- `Forms/Controls.Designer.cs` lines 284-297: BuildGGA() implementation
- `Forms/Controls.Designer.cs` lines 299-313: BuildVTG() implementation
- `Forms/Controls.Designer.cs` lines 315-325: BuildHDT() implementation
- `Forms/Controls.Designer.cs` lines 327-343: BuildAVR() implementation
- `Forms/Controls.Designer.cs` lines 345-366: BuildOGI() (PAOGI) implementation
- `Forms/Controls.Designer.cs` lines 368-389: BuildNDA() (PANDA) implementation
- `Forms/Controls.Designer.cs` lines 391-476: BuildKSXT() with full field documentation
- `Forms/FormGPSData.Designer.cs` lines 385-519: NMEA sentence display examples

### External Documentation

**NMEA-0183 Standard**

- GGA: Global Positioning System Fix Data (standard sentence)
- VTG: Track Made Good and Ground Speed (standard sentence)
- HDT: Heading True (standard sentence)

**Manufacturer Documentation**

- Trimble PTNL,AVR: Proprietary dual antenna attitude format
- Unicore KSXT: Integrated GNSS/INS data format

---

## Implementation Status

### Fully Implemented

| Sentence | Parser  | FormGPS | QML | Simulator |
| -------- | ------- | ------- | --- | --------- |
| GGA      | Yes     | Yes     | Yes | Yes       |
| VTG      | Yes     | Yes     | Yes | Yes       |
| PANDA    | Yes     | Yes     | Yes | Yes       |
| HDT      | Partial | Yes     | Yes | Yes       |
| PAOGI    | Partial | Yes     | Yes | Yes       |

### Partially Implemented

| Sentence | Status         | Notes                                   |
| -------- | -------------- | --------------------------------------- |
| AVR      | Detection only | Recognized but no dedicated parser      |
| KSXT     | Detection only | Recognized but no full field extraction |

### Placeholder Only

| Sentence | Status         | Notes                                  |
| -------- | -------------- | -------------------------------------- |
| HPD      | Detection only | Referenced but no implementation found |

### Usage Priority

When multiple sentences are available:

1. **Position**: GGA > PANDA > PAOGI
2. **Speed**: VTG > PANDA > PAOGI
3. **Heading (stationary)**: HDT > PAOGI > AVR > PANDA
4. **Heading (moving)**: VTG > PAOGI > HDT > PANDA
5. **Roll**: PAOGI > AVR > PANDA
6. **Pitch**: PANDA > PAOGI
7. **Yaw Rate**: PANDA > PAOGI

---

**Document Maintained By**: AgOpenGPS Qt Development Team
**Contact**: Phase 6.0.21 Migration Project
**License**: Same as AgOpenGPS project

---

End of NMEA Sentences Reference
