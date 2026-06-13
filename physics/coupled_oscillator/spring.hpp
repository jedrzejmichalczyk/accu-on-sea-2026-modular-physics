#pragma once

#include "../../core/typed_component.hpp"
#include "../../core/scalar.hpp"
#include "tags.hpp"
#include <span>
#include <stdexcept>

namespace sopot::physics::coupled {

//=============================================================================
// Spring - a linear spring connecting two masses
//=============================================================================
// Template parameters TagSet1, TagSet2 identify the two masses it connects.
//
// State (0 elements): the spring stores no state of its own - it computes
// forces from the positions (and velocities) it queries from the masses.
//
// Provides: TagSet1::Force, TagSet2::Force, spring::Extension,
//           spring::PotentialEnergy, spring::Stiffness
// Requires: TagSet1::Position, TagSet2::Position (+ Velocity if damped)
//
// Physics:
//   extension = x1 - x2 - rest_length
//   F1 = -k * extension - c * (v1 - v2)   (force on mass 1)
//   F2 = -F1                              (Newton's 3rd law)
//=============================================================================

template<typename TagSet1, typename TagSet2, Scalar T = double>
class Spring final : public TypedComponent<0, T> {
public:
    using Base = TypedComponent<0, T>;
    using typename Base::LocalState;
    using typename Base::LocalDerivative;

private:
    double m_stiffness;    // spring constant k   [N/m]
    double m_rest_length;  // natural length L0   [m]
    double m_damping;      // damping coefficient c [N·s/m]

public:
    Spring(
        double stiffness,
        double rest_length = 1.0,
        double damping = 0.0
    ) : m_stiffness(stiffness)
      , m_rest_length(rest_length)
      , m_damping(damping) {
        if (stiffness <= 0.0) {
            throw std::invalid_argument("Spring stiffness must be positive");
        }
        if (damping < 0.0) {
            throw std::invalid_argument("Damping must be non-negative");
        }
    }

    //=========================================================================
    // Required Component Interface
    //=========================================================================

    LocalState getInitialLocalState() const { return {}; }

    //=========================================================================
    // State Functions - Forces and Spring Properties
    //=========================================================================

    // Extension: how far the spring is stretched past its rest length.
    template<typename Registry>
    T compute(spring::Extension, std::span<const T> state, const Registry& registry) const {
        T x1 = query<typename TagSet1::Position>(registry, state);
        T x2 = query<typename TagSet2::Position>(registry, state);
        return x1 - x2 - T(m_rest_length);
    }

    // Force on mass 1: restoring spring force, plus damping if c > 0.
    template<typename Registry>
    T compute(typename TagSet1::Force, std::span<const T> state, const Registry& registry) const {
        T extension = query<spring::Extension>(registry, state);
        T force = -T(m_stiffness) * extension;

        if (m_damping > 0.0) {
            T v1 = query<typename TagSet1::Velocity>(registry, state);
            T v2 = query<typename TagSet2::Velocity>(registry, state);
            force += -T(m_damping) * (v1 - v2);
        }
        return force;
    }

    // Force on mass 2 is equal and opposite (Newton's 3rd law).
    template<typename Registry>
    T compute(typename TagSet2::Force, std::span<const T> state, const Registry& registry) const {
        return -query<typename TagSet1::Force>(registry, state);
    }

    // Potential energy stored in the spring: U = 0.5 * k * extension^2.
    template<typename Registry>
    T compute(spring::PotentialEnergy, std::span<const T> state, const Registry& registry) const {
        T ext = query<spring::Extension>(registry, state);
        return T(0.5 * m_stiffness) * ext * ext;
    }

    // Stiffness (a constant - needs neither state nor registry).
    template<typename Registry>
    T compute(spring::Stiffness, std::span<const T>, const Registry&) const {
        return T(m_stiffness);
    }

    //=========================================================================
    // Parameter Access
    //=========================================================================

    double getStiffness() const noexcept { return m_stiffness; }
    double getRestLength() const noexcept { return m_rest_length; }
    double getDamping() const noexcept { return m_damping; }
};

//=============================================================================
// Pre-defined Spring Type connecting mass1 and mass2
//=============================================================================

template<Scalar T = double>
using Spring12 = Spring<mass1, mass2, T>;

} // namespace sopot::physics::coupled
