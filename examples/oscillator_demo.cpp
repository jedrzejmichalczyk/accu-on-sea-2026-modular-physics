// Driver for the ACCU talk: run the coupled oscillator and dump energy data.
#include "physics/coupled_oscillator/coupled_system.hpp"
#include "core/solver.hpp"
#include <cstdio>

using namespace sopot;
using namespace sopot::physics::coupled;

int main() {
    auto sys = createCoupledOscillator<double>(
        1.0, 0.0, 0.0,   // m1, x1_0, v1_0
        1.0, 2.5, 0.0,   // m2, x2_0, v2_0  (spring stretched 1.5 m past L0)
        4.0, 1.0         // k, L0
    );

    static_assert(decltype(sys)::hasFunction<system::TotalEnergy>());

    RK4Solver solver;
    auto deriv = [&](double t, StateView s) {
        return sys.computeDerivatives(t, std::vector<double>(s.begin(), s.end()));
    };
    auto sol = solver.solve(deriv, sys.getStateDimension(), 0.0, 20.0, 0.001,
                            sys.getInitialState());

    const double E0 = sys.computeStateFunction<system::TotalEnergy>(sol.states.front());
    double max_drift = 0.0;

    FILE* f = fopen("oscillator_data.csv", "w");
    fprintf(f, "t,x1,x2,E,drift\n");
    for (size_t i = 0; i < sol.size(); i += 10) {
        const auto& s = sol.states[i];
        double E = sys.computeStateFunction<system::TotalEnergy>(s);
        double drift = (E - E0) / E0;
        if (std::abs(drift) > max_drift) max_drift = std::abs(drift);
        double x1 = sys.computeStateFunction<mass1::Position>(s);
        double x2 = sys.computeStateFunction<mass2::Position>(s);
        fprintf(f, "%.6f,%.9f,%.9f,%.15e,%.3e\n", sol.times[i], x1, x2, E, drift);
    }
    fclose(f);

    printf("steps=%zu  wall=%.2f ms  E0=%.15f  max_rel_drift=%.3e\n",
           sol.size(), sol.total_time, E0, max_drift);
    return 0;
}
