#pragma once

#include <vector>
#include <span>
#include <chrono>
#include <concepts>

namespace sopot {

// State types
using StateVector = std::vector<double>;
using StateDerivative = std::vector<double>;
using StateView = std::span<const double>;

// Solution data structure
struct SolutionResult {
    std::vector<double> times;
    std::vector<StateVector> states;
    double total_time{0.0};

    size_t size() const noexcept { return times.size(); }
    bool empty() const noexcept { return times.empty(); }

    void reserve(size_t capacity) {
        times.reserve(capacity);
        states.reserve(capacity);
    }
};

// A derivative function maps (t, state) to dy/dt. The solver is generic over
// any callable that satisfies this - the coupled oscillator's RHS is just one.
template<typename F>
concept DerivativeFunctionConcept = requires(F f, double t, StateView state) {
    { f(t, state) } -> std::convertible_to<StateDerivative>;
};

// Classic fixed-step Runge-Kutta 4 integrator.
class RK4Solver {
private:
    mutable StateVector m_k1, m_k2, m_k3, m_k4;
    mutable StateVector m_temp_state;

    void ensureCapacity(size_t state_dim) const {
        if (m_k1.size() != state_dim) {
            m_k1.resize(state_dim);
            m_k2.resize(state_dim);
            m_k3.resize(state_dim);
            m_k4.resize(state_dim);
            m_temp_state.resize(state_dim);
        }
    }

public:
    // Generic solve with a derivatives function
    template<DerivativeFunctionConcept DerivFunc>
    SolutionResult solve(
        DerivFunc&& derivs,
        size_t state_dim,
        double t_start,
        double t_end,
        double dt,
        const StateVector& initial_state
    ) const {
        auto start_time = std::chrono::high_resolution_clock::now();

        const size_t num_steps = static_cast<size_t>((t_end - t_start) / dt) + 1;

        // Pre-allocate workspace
        ensureCapacity(state_dim);

        SolutionResult result;
        result.reserve(num_steps);

        // Initialize
        StateVector current_state = initial_state;
        double current_time = t_start;

        // Store initial conditions
        result.times.push_back(current_time);
        result.states.push_back(current_state);

        // Integration loop
        while (current_time < t_end - dt * 0.5) {
            StateView state_view(current_state);

            // k1 = dt * f(t, y)
            m_k1 = derivs(current_time, state_view);
            for (size_t i = 0; i < state_dim; ++i) {
                m_k1[i] *= dt;
                m_temp_state[i] = current_state[i] + 0.5 * m_k1[i];
            }

            // k2 = dt * f(t + dt/2, y + k1/2)
            StateView temp_view(m_temp_state);
            m_k2 = derivs(current_time + 0.5 * dt, temp_view);
            for (size_t i = 0; i < state_dim; ++i) {
                m_k2[i] *= dt;
                m_temp_state[i] = current_state[i] + 0.5 * m_k2[i];
            }

            // k3 = dt * f(t + dt/2, y + k2/2)
            m_k3 = derivs(current_time + 0.5 * dt, temp_view);
            for (size_t i = 0; i < state_dim; ++i) {
                m_k3[i] *= dt;
                m_temp_state[i] = current_state[i] + m_k3[i];
            }

            // k4 = dt * f(t + dt, y + k3)
            m_k4 = derivs(current_time + dt, temp_view);
            for (size_t i = 0; i < state_dim; ++i) {
                m_k4[i] *= dt;
            }

            // Update state: y_{n+1} = y_n + (k1 + 2*k2 + 2*k3 + k4)/6
            for (size_t i = 0; i < state_dim; ++i) {
                current_state[i] += (m_k1[i] + 2.0 * m_k2[i] + 2.0 * m_k3[i] + m_k4[i]) / 6.0;
            }

            current_time += dt;

            // Store result
            result.times.push_back(current_time);
            result.states.push_back(current_state);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        result.total_time = duration.count() / 1000.0; // Convert to milliseconds

        return result;
    }
};

} // namespace sopot
