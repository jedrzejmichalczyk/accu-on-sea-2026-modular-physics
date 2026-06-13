#pragma once

#include "../../core/typed_component.hpp"
#include "../../core/scalar.hpp"
#include "tags.hpp"
#include <span>
#include <stdexcept>

namespace sopot::physics::coupled {

//=============================================================================
// PointMass - A single point mass with position and velocity
//=============================================================================
// Template parameter TagSet must provide:
//   - TagSet::Position - tag for this mass's position
//   - TagSet::Velocity - tag for this mass's velocity
//   - TagSet::Force    - tag for force acting on this mass
//   - TagSet::Mass     - tag for this mass's mass value
//
// State (2 elements): [position, velocity]
//
// Provides: TagSet::Position, TagSet::Velocity, TagSet::Mass
// Requires: TagSet::Force (for derivatives)
//=============================================================================

template<typename TagSet, Scalar T = double>
class PointMass final : public TypedComponent<2, T> {
public:
    using Base = TypedComponent<2, T>;
    using typename Base::LocalState;
    using typename Base::LocalDerivative;

private:
    double m_mass;
    double m_initial_position;
    double m_initial_velocity;

public:
    PointMass(
        double mass,
        double initial_position = 0.0,
        double initial_velocity = 0.0
    ) : m_mass(mass)
      , m_initial_position(initial_position)
      , m_initial_velocity(initial_velocity) {
        if (mass <= 0.0) {
            throw std::invalid_argument("Mass must be positive");
        }
    }

    //=========================================================================
    // Required Component Interface
    //=========================================================================

    LocalState getInitialLocalState() const {
        return {T(m_initial_position), T(m_initial_velocity)};
    }

    //=========================================================================
    // Derivatives - dx/dt = v, dv/dt = F/m
    //=========================================================================

    template<typename Registry>
    LocalDerivative derivatives(
        [[maybe_unused]] T t,
        std::span<const T> state,
        const Registry& registry
    ) const {
        // dx/dt = v        — velocity is our own second state variable
        T velocity = this->localState(state, 1);

        // dv/dt = F/m      — ask the system for the force on this mass
        T force = query<typename TagSet::Force>(registry, state);
        T acceleration = force / T(m_mass);

        return {velocity, acceleration};
    }

    //=========================================================================
    // State Functions - Position, Velocity, Mass
    //=========================================================================

    // Position and Velocity are our own state; Mass is a constant. None of
    // these need the registry, but every compute() takes it for uniformity.
    template<typename Registry>
    T compute(typename TagSet::Position, std::span<const T> state, const Registry&) const {
        return this->localState(state, 0);   // our state slice: [position, velocity]
    }

    template<typename Registry>
    T compute(typename TagSet::Velocity, std::span<const T> state, const Registry&) const {
        return this->localState(state, 1);
    }

    template<typename Registry>
    T compute(typename TagSet::Mass, std::span<const T>, const Registry&) const {
        return T(m_mass);
    }

    //=========================================================================
    // Parameter Access
    //=========================================================================

    double getMass() const noexcept { return m_mass; }
    double getInitialPosition() const noexcept { return m_initial_position; }
    double getInitialVelocity() const noexcept { return m_initial_velocity; }
};

//=============================================================================
// Pre-defined Mass Types using the tag namespaces
//=============================================================================

template<Scalar T = double>
using Mass1 = PointMass<mass1, T>;

template<Scalar T = double>
using Mass2 = PointMass<mass2, T>;

} // namespace sopot::physics::coupled
