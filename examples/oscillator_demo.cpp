// Driver for the ACCU talk: build the coupled oscillator, integrate it with
// RK4, and dump positions + total energy so we can check energy is conserved.
#include "physics/coupled_oscillator/coupled_system.hpp"
#include "core/solver.hpp"

#include <cmath>
#include <format>
#include <fstream>
#include <iostream>

using namespace sopot;
using namespace sopot::physics::coupled;

int main() {
    // Two equal masses joined by a spring, started with the spring stretched.
    auto sys = createCoupledOscillator<double>(
        1.0, 0.0, 0.0,   // m1, x1_0, v1_0
        1.0, 2.5, 0.0,   // m2, x2_0, v2_0  (spring stretched 1.5 m past L0)
        4.0, 1.0         // k, L0
    );

    // Compile-time completeness check: the assembled system really does know
    // how to compute TotalEnergy. If no component provided it, this would be a
    // build error - not a runtime surprise.
    static_assert(decltype(sys)::hasFunction<system::TotalEnergy>());

    // The solver only needs a (t, state) -> dy/dt callable; the system supplies it.
    RK4Solver solver;
    auto deriv = [&](double t, StateView s) {
        return sys.computeDerivatives(t, std::vector<double>(s.begin(), s.end()));
    };
    auto sol = solver.solve(deriv, sys.getStateDimension(), 0.0, 20.0, 0.001,
                            sys.getInitialState());

    const double E0 = sys.query<system::TotalEnergy>(sol.states.front());
    double max_drift = 0.0;

    std::ofstream csv("oscillator_data.csv");
    csv << "t,x1,x2,E,drift\n";
    for (size_t i = 0; i < sol.size(); i += 10) {
        const auto& s = sol.states[i];
        const double E     = sys.query<system::TotalEnergy>(s);
        const double drift = (E - E0) / E0;
        const double x1    = sys.query<mass1::Position>(s);
        const double x2    = sys.query<mass2::Position>(s);
        max_drift = std::max(max_drift, std::abs(drift));
        csv << std::format("{:.6f},{:.9f},{:.9f},{:.15e},{:.3e}\n",
                           sol.times[i], x1, x2, E, drift);
    }

    std::cout << std::format(
        "steps={}  wall={:.2f} ms  E0={:.15f}  max_rel_drift={:.3e}\n",
        sol.size(), sol.total_time, E0, max_drift);
    return 0;
}
