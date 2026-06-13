#pragma once

//=============================================================================
// Coupled Oscillator System - Two masses connected by a spring
//=============================================================================
//
// This example demonstrates SOPOT's modular architecture:
//
//   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
//   │   Mass1     │     │   Spring    │     │   Mass2     │
//   │             │     │             │     │             │
//   │ State: x1,v1│◄────│ Force1,Force2│────►│ State: x2,v2│
//   │             │     │             │     │             │
//   │ Provides:   │     │ Provides:   │     │ Provides:   │
//   │  Position1  │────►│  Force1     │◄────│  Position2  │
//   │  Velocity1  │     │  Force2     │     │  Velocity2  │
//   │  Mass1      │     │  Extension  │     │  Mass2      │
//   └─────────────┘     └─────────────┘     └─────────────┘
//                              │
//                              ▼
//                       ┌─────────────┐
//                       │EnergyMonitor│
//                       │             │
//                       │ Provides:   │
//                       │  TotalEnergy│
//                       │  CenterOfMass│
//                       │  Momentum   │
//                       └─────────────┘
//
// Key Design Principles:
//
// 1. SEPARATION OF CONCERNS
//    - Each component has a single responsibility
//    - Mass: position/velocity integration, mass property
//    - Spring: force computation, energy storage
//    - EnergyMonitor: system-level quantities
//
// 2. COMPILE-TIME DISPATCH
//    - All component interactions through typed tags
//    - Zero runtime overhead - no virtual functions
//    - Type errors caught at compile time
//
// 3. TAGGED STATE FUNCTIONS
//    - mass1::Position vs mass2::Position distinguished by type
//    - Spring knows which mass is which through template parameters
//    - Prevents accidental mixing of component states
//
// Usage:
//   auto system = makeODESystem<double>(
//       Mass1<double>(1.0, 0.0, 0.0),     // m=1, x0=0, v0=0
//       Mass2<double>(1.0, 2.0, 0.0),     // m=1, x0=2, v0=0
//       Spring12<double>(4.0, 1.0),       // k=4, L0=1
//       EnergyMonitor<double>()
//   );
//   // ...or just call createCoupledOscillator(...) below.
//
//   auto state  = system.getInitialState();
//   auto energy = system.query<system::TotalEnergy>(state);
//
//=============================================================================

#include "tags.hpp"
#include "point_mass.hpp"
#include "spring.hpp"
#include "energy_monitor.hpp"
#include "../../core/component.hpp"

namespace sopot::physics::coupled {

//=============================================================================
// Convenience Factory for Complete System
//=============================================================================

template<Scalar T = double>
auto createCoupledOscillator(
    double m1, double x1_0, double v1_0,
    double m2, double x2_0, double v2_0,
    double k, double L0 = 1.0, double damping = 0.0
) {
    return makeODESystem<T>(
        Mass1<T>(m1, x1_0, v1_0),
        Mass2<T>(m2, x2_0, v2_0),
        Spring12<T>(k, L0, damping),
        EnergyMonitor<T>()
    );
}

} // namespace sopot::physics::coupled
