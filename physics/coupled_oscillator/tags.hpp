#pragma once

namespace sopot::physics::coupled {

//=============================================================================
// State-function tags for the coupled oscillator
//=============================================================================
// Each tag is an empty type used purely as a compile-time key. mass1 and mass2
// get their own tag sets so the Spring can tell the two masses apart by type:
// a Spring<mass1, mass2> provides mass1::Force and mass2::Force, and there is
// no way to accidentally cross the wires.
//=============================================================================

struct mass1 {
    struct Position {};
    struct Velocity {};
    struct Force {};
    struct Mass {};
};

struct mass2 {
    struct Position {};
    struct Velocity {};
    struct Force {};
    struct Mass {};
};

namespace spring {
    struct Extension {};
    struct PotentialEnergy {};
    struct Stiffness {};
}

namespace system {
    struct TotalEnergy {};
    struct CenterOfMass {};
    struct Momentum {};
}

} // namespace sopot::physics::coupled
