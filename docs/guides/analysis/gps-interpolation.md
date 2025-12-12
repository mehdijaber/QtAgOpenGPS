# GPS/IMU Interpolation Theory for 50 Hz Position Updates

**Status**: TO BE DECIDED - Future Feature
**Last Validated**: 2025-10-02
**Prerequisites**: Complete Qt 6.8 migration and stable codebase
**Objective**: Theoretical framework for 3.3× position update frequency improvement

---

## Executive Summary

### Current System Limitations

**GPS Updates**: 15 Hz (66ms interval) - Real position but slow
**IMU Updates**: 50 Hz (20ms interval) - Fast orientation but no position
**Problem**: 70% of IMU data unused, position frozen for 46ms out of every 66ms

### Proposed Solution

GPS/IMU sensor fusion using **Dead Reckoning** with Kalman filtering to achieve 50 Hz position updates while maintaining GPS accuracy.

### Expected Benefits

- Position updated **3.3× more frequently** (15Hz → 50Hz)
- AutoSteer latency reduced from **66ms to 20ms**
- **100% IMU data utilization** (vs. 30% currently)

**IMPORTANT**: This is a theoretical design document. Implementation should only occur after Qt 6.8 migration is complete and the codebase is fully stable.

---

## Theoretical Foundations

### Dead Reckoning Principle

**Definition**: Estimating current position based on a known position and integrating velocity over time.

**Fundamental Equation**:
```
Position(t) = Position(t₀) + ∫[t₀→t] Velocity(τ) × Direction(τ) dτ
```

**2D Vectorial Form**:
```
P(t) = P(t₀) + ∫[t₀→t] v(τ) × [cos(θ(τ)), sin(θ(τ))]ᵀ dτ

Where:
- P(t) = [Easting(t), Northing(t)]ᵀ  (UTM coordinates)
- v(τ) = scalar velocity at time τ (m/s)
- θ(τ) = heading at time τ (radians)
```

### Numerical Discretization

**Euler Approximation** (Order 1):
```
P(t + Δt) = P(t) + v(t) × Δt × [cos(θ(t)), sin(θ(t))]ᵀ

Where Δt = 20ms for 50 Hz updates
```

**Runge-Kutta 2** (Order 2 - Improved Accuracy):
```
k₁ = v(t) × [cos(θ(t)), sin(θ(t))]ᵀ
k₂ = v(t + Δt/2) × [cos(θ(t + Δt/2)), sin(θ(t + Δt/2))]ᵀ
P(t + Δt) = P(t) + Δt × k₂
```

### Error Accumulation and Drift

**Cumulative Error**:
```
ε(t) = ε₀ + ∫[0→t] (δv(τ) × Direction + v(τ) × δθ(τ)) dτ

Where:
- δv(τ) = velocity error at time τ
- δθ(τ) = heading error at time τ
```

**Linear Error Growth Approximation**:
```
|ε(t)| ≈ |ε₀| + t × (|δv| + v × |δθ|)

Numerical Example (vehicle at 10 km/h):
- Velocity error: 0.1 km/h → drift 0.028 m/s
- Heading error: 1° → drift 0.048 m/s at 10 km/h
- Total after 1 second: ~7.6 cm drift
```

---

## GPS/IMU Sensor Fusion

### Kalman Filter Formulation

**State Vector** (position + velocity):
```
State X = [px, py, vx, vy, θ]ᵀ

Prediction Step:
X̂(k|k-1) = F × X(k-1|k-1) + B × u(k)

State Transition Matrix F:
F = | 1  0  Δt  0   0  |
    | 0  1  0   Δt  0  |
    | 0  0  1   0   0  |
    | 0  0  0   1   0  |
    | 0  0  0   0   1  |

Control Input u = [ax, ay, ω]ᵀ (accelerations, angular velocity)
```

**Update Step** (GPS correction):
```
Innovation: y(k) = z(k) - H × X̂(k|k-1)
Kalman Gain: K(k) = P(k|k-1) × Hᵀ × (H × P(k|k-1) × Hᵀ + R)⁻¹
Corrected State: X̂(k|k) = X̂(k|k-1) + K(k) × y(k)
Covariance: P(k|k) = (I - K(k) × H) × P(k|k-1)
```

### Simplified Complementary Filter (Recommended)

**Alternative Implementation** - simpler than full Kalman filter:

```cpp
// GPS/IMU heading fusion
θ_fused = α × θ_IMU + (1-α) × θ_GPS

// Adaptive weight based on GPS age
α = min(1.0, time_since_GPS / 200ms)

// Position with progressive correction
P_corrected = P_dead_reckoning + β × (P_GPS - P_dead_reckoning)
β = 0.02  // 2% correction per frame
```

---

## Implementation Architecture

### Data Structure

```cpp
struct InterpolationState {
    // Primary state
    Vec2 lastGPSPosition;        // Last real GPS position
    Vec2 interpolatedPosition;    // Current interpolated position
    Vec2 velocity;               // Velocity vector [vx, vy]

    // Heading and orientation
    double lastGPSHeading;       // Last GPS heading (radians)
    double lastIMUHeading;       // Last IMU heading (radians)
    double fusedHeading;         // Fused heading (radians)

    // Timing
    QElapsedTimer timeSinceLastGPS;
    int missedGPSFrames;         // Counter for frames without GPS

    // Drift metrics
    double driftDistance;        // Drift distance (meters)
    double driftAngle;          // Drift angle (radians)
    Vec2 driftVector;           // Drift vector [dx, dy]

    // Filtering
    double velocitySmoothed;     // Smoothed velocity (m/s)
    Mat2x2 covarianceMatrix;    // Position covariance matrix
};
```

### Main Algorithm (50 Hz Loop)

```cpp
void onIMUInterpolationTick() {
    const double dt = 0.02; // 50 Hz = 20ms

    // === Phase 1: Data acquisition ===
    double imuHeading = glm::toRadians(ahrs.imuHeading);
    double gpsSpeed = CVehicle::instance()->avgSpeed * 0.277777; // km/h → m/s

    // === Phase 2: Heading fusion ===
    double timeSinceGPS = state.timeSinceLastGPS.elapsed() / 1000.0;
    double alpha = min(1.0, timeSinceGPS / 0.2); // 200ms max
    state.fusedHeading = alpha * imuHeading + (1-alpha) * state.lastGPSHeading;

    // === Phase 3: Dead reckoning ===
    double vx = gpsSpeed * sin(state.fusedHeading);
    double vy = gpsSpeed * cos(state.fusedHeading);

    state.interpolatedPosition.easting += vx * dt;
    state.interpolatedPosition.northing += vy * dt;

    // === Phase 4: Drift correction (if GPS recent) ===
    if (timeSinceGPS < 0.1) { // GPS < 100ms
        const double beta = 0.02; // 2% correction per frame
        double dx = state.lastGPSPosition.easting - state.interpolatedPosition.easting;
        double dy = state.lastGPSPosition.northing - state.interpolatedPosition.northing;

        state.interpolatedPosition.easting += beta * dx;
        state.interpolatedPosition.northing += beta * dy;
    }

    // === Phase 5: Safety limits ===
    double drift = glm::Distance(state.interpolatedPosition, state.lastGPSPosition);
    if (drift > MAX_ALLOWED_DRIFT) { // 2m default
        // Reset to GPS
        state.interpolatedPosition = state.lastGPSPosition;
        qWarning() << "Interpolation reset: drift" << drift << "m";
    }

    // === Phase 6: Apply position ===
    pn.fix = state.interpolatedPosition;
    UpdateFixPosition();
}
```

### GPS Resynchronization (15 Hz)

```cpp
void onGPSUpdate(Vec2 gpsPosition, double gpsHeading) {
    // === Measure drift ===
    double drift = glm::Distance(state.interpolatedPosition, gpsPosition);
    state.driftDistance = drift;

    // === Log if significant ===
    if (drift > 0.5) { // 50cm
        qDebug() << "GPS Resync - Drift:" << drift << "m"
                 << "Frames:" << state.missedGPSFrames;
    }

    // === Reset state ===
    state.lastGPSPosition = gpsPosition;
    state.lastGPSHeading = gpsHeading;
    state.timeSinceLastGPS.restart();
    state.missedGPSFrames = 0;

    // === Soft or hard correction ===
    if (drift < 1.0) {
        // Progressive correction over 1 second
        // (stored in state for gradual application)
        state.driftVector = gpsPosition - state.interpolatedPosition;
    } else {
        // Hard reset if excessive drift
        state.interpolatedPosition = gpsPosition;
        state.driftVector = Vec2(0, 0);
    }
}
```

---

## Performance Analysis

### Computational Complexity

| Operation | Complexity | Time (ARM Cortex-A72) | Time (x86-64) |
|-----------|------------|------------------------|---------------|
| Sin/Cos | O(1) | ~20ns | ~5ns |
| Multiplication | O(1) | ~2ns | ~1ns |
| Vec2 Addition | O(1) | ~4ns | ~2ns |
| Vec2 Distance | O(1) | ~30ns | ~10ns |
| **Total/frame** | O(1) | **~100ns** | **~30ns** |

**CPU Load at 50 Hz**:
- ARM: 100ns × 50 = 5 μs/s = **0.0005%**
- x86: 30ns × 50 = 1.5 μs/s = **0.00015%**

### Expected Accuracy

**Test Scenarios**:

| Scenario | Speed | Duration without GPS | Theoretical Drift | Expected Measured |
|----------|-------|---------------------|-------------------|-------------------|
| Straight line | 10 km/h | 200ms | 2.8 cm | ~3 cm |
| 90° turn | 10 km/h | 200ms | 5.6 cm | ~7 cm |
| U-turn | 5 km/h | 500ms | 14 cm | ~20 cm |
| Stationary | 0 km/h | 1000ms | 0 cm | ~1 cm (noise) |

### Before/After Comparison

| Metric | Without Interpolation | With Interpolation | Improvement |
|--------|----------------------|-------------------|-------------|
| Position frequency | 15 Hz | 50 Hz | **3.3×** |
| Maximum latency | 66 ms | 20 ms | **3.3×** |
| IMU data utilization | 30% | 100% | **3.3×** |
| Maximum drift/sec | 0 (frozen) | ~35 cm @ 10km/h | N/A |
| AutoSteer responsiveness | Choppy | Smooth | **Qualitative** |

---

## Implementation Phases

### Phase 1: Infrastructure (2-3 days)

1. Create `InterpolationEngine` class
2. Add 50 Hz IMU timer
3. Implement basic dead reckoning
4. Unit tests

### Phase 2: GPS/IMU Fusion (3-4 days)

1. Implement complementary filter
2. Add GPS resynchronization
3. Drift management and safety limits
4. Integration tests

### Phase 3: Optimization (2-3 days)

1. Cache sin/cos (lookup tables)
2. SIMD for vector calculations
3. Profiling and optimization
4. Performance tests

### Phase 4: UI & Configuration (1-2 days)

1. Settings in .ini file
2. Drift display in UI
3. Diagnostic graphs
4. User documentation

**Total Estimated Effort**: 10-15 developer days

---

## Recommended Configuration

### Settings File (QtAgOpenGPS.ini)

```ini
[Interpolation]
enabled=true                    # Enable interpolation
frequency=50                    # Hz (20, 50, 100)
maxDrift=2.0                    # Max drift before reset (meters)
correctionRate=0.02             # Correction rate per frame
fusionAlpha=0.8                 # IMU vs GPS weight
enableLogging=true              # Log drift events
enableVisualisation=false       # Debug display

# Advanced parameters
kalmanProcessNoise=0.01         # Kalman process noise
kalmanMeasurementNoise=0.1      # Kalman measurement noise
deadReckoningMethod=euler       # euler|rk2|rk4
headingFusionMethod=complementary # complementary|kalman|mahony
```

### Validation Tests

**Test 1: Static Drift**
```cpp
// Vehicle stationary, measure drift over 1 minute
EXPECT_LT(driftAfter60s, 0.1); // < 10cm
```

**Test 2: Straight Line**
```cpp
// 100m straight line at 10 km/h
EXPECT_LT(crossTrackError, 0.5); // < 50cm
```

**Test 3: Turn**
```cpp
// 180° turn at 5 km/h
EXPECT_LT(headingError, 5.0); // < 5°
```

**Test 4: GPS Loss**
```cpp
// Simulate 5 second GPS loss
EXPECT_LT(positionError, 5.0); // < 5m after 5s
```

---

## Future Enhancements

### Version 2.0 - Extended Kalman Filter (EKF)

Complete multi-sensor integration:
- GPS (position)
- IMU (orientation + accelerations)
- WAS (wheel angle sensor)
- Odometry (wheel speed)

### Version 3.0 - Agricultural SLAM

Simultaneous Localization And Mapping:
- Field boundary detection
- Obstacle recognition
- Camera fusion

### Version 4.0 - Trajectory Prediction

Machine learning for predictive guidance:
- Operator movement anticipation
- Trajectory optimization
- Predictive collision avoidance

---

## Implementation Checklist

### Technical Prerequisites

- [ ] Qt 6.8 migration complete and stable
- [ ] AgIOService architecture finalized
- [ ] Unit test coverage > 80%
- [ ] Code documentation up to date

### Hardware Prerequisites

- [ ] BNO08x IMU module functional
- [ ] F9P RTK GPS configured
- [ ] Stable UDP communication
- [ ] AutoSteer module compatible

### Team Validation

- [ ] Architecture review by team
- [ ] Agreement on default parameters
- [ ] Test plan accepted
- [ ] Rollback strategy defined

---

## References

### Scientific Literature

1. Kalman, R.E. (1960). "A New Approach to Linear Filtering and Prediction Problems"
2. Mahony, R. et al. (2008). "Nonlinear Complementary Filters on the Special Orthogonal Group"
3. Groves, P.D. (2013). "Principles of GNSS, Inertial, and Multisensor Integrated Navigation Systems"

### Open Source Implementations

1. ArduPilot - EKF2/EKF3 implementation
2. PX4 - ECL EKF
3. Robot Localization (ROS) - EKF/UKF nodes

### Agricultural Standards

1. ISO 11783 - Tractors and machinery for agriculture (ISOBUS)
2. NMEA 0183 - Marine/GPS communication
3. SAE J1939 - Vehicle bus standard

---

## Related Documentation

**Architecture**:
- [System Architecture](../architecture/system-architecture.md) - FormGPS component overview
- [Threading & Timers](../architecture/threading-timers.md) - Current timer frequencies
- [AgIOService Architecture](../architecture/agioservice-architecture.md) - I/O coordination

**Protocols**:
- [NMEA/PGN Architecture](../protocols/nmea-pgn-architecture.md) - GPS data parsing
- [PGN Protocol Reference](../protocols/nmea-pgn-architecture.md) - Binary GPS data (PGN 214)

**Performance**:
- [Performance Baseline](performance-baseline.md) - Current 10 Hz system measurements
- [Profiling Guide](../development/profiling-windows.md) - Performance measurement tools

---

## Summary

**Complexity**: Medium-High (4/5 stars)
**Estimated Effort**: 10-15 developer days
**ROI**: 3.3× smoother position updates, more responsive AutoSteer
**Risks**: Drift with poor tuning, debugging complexity

**Recommendation**: Implement AFTER complete Qt 6.8 stabilization. Start with simple complementary filter, evolve to Kalman if necessary.

**CRITICAL**: Do not implement before Qt 6.8 migration is finalized and codebase is fully stable.

---

**Status**: TO BE DECIDED - Future Feature
**Prerequisites**: Complete Qt 6.8 migration and stable codebase
**Implementation Priority**: LOW - Deferred until after core system stabilization
