# -*- coding: utf-8 -*-
"""Canonical code fragments shown on the deck's code slides.

This is the source of truth for the *code* on the slides. Each key matches a
code box tagged "code:<key>" in the .pptx (see slides/SLIDES.md). To change a
code slide, edit the snippet here and run scripts/update_code_slides.py — it
rewrites only those boxes in place, leaving the rest of your hand-edited deck
untouched.

`highlight_lines` are 1-based line numbers drawn bold/yellow (the "look here"
emphasis). Keep snippets in sync with the real headers in core/ and
physics/coupled_oscillator/ — they are curated excerpts, not verbatim copies
(some elide e.g. the spring damping term for space).
"""

SNIPPETS = {
    # ---------------------------------------------------------------- tags
    "tags": {
        "highlight_lines": (),
        "code": """\
struct mass1 {
    struct Position {};      // empty types — pure compile-time keys
    struct Velocity {};
    struct Force    {};
    struct Mass     {};
};

struct mass2 { /* same shape, different types */ };

namespace spring {
    struct Extension       {};
    struct PotentialEnergy {};
};""",
    },

    # ------------------------------------------------------------- pointmass
    "pointmass": {
        "highlight_lines": (8,),
        "code": """\
template<typename TagSet, Scalar T = double>
class PointMass final : public Component<2, T> {   // 2 states: [x, v]
    double m_mass;
public:
    template<typename Registry>
    LocalDerivative derivatives(T t, std::span<const T> state, const Registry& registry) const {
        T velocity = this->localState(state, 1);          // dx/dt = v
        T force = query<typename TagSet::Force>(registry, state);
        return {velocity, force / T(m_mass)};             // dv/dt = F/m
    }
    template<typename Registry>            // Velocity: same, localState(s, 1)
    T compute(typename TagSet::Position, std::span<const T> s,
              const Registry&) const { return this->localState(s, 0); }
};""",
    },

    # ---------------------------------------------------------------- spring
    "spring": {
        "highlight_lines": (7, 8),
        "code": """\
template<typename TagSet1, typename TagSet2, Scalar T = double>
class Spring final : public Component<0, T> {        // 0 states!
    double m_stiffness, m_rest_length;
public:
    template<typename Registry>
    T compute(typename TagSet1::Force, std::span<const T> state, const Registry& reg) const {
        T x1 = query<typename TagSet1::Position>(reg, state);
        T x2 = query<typename TagSet2::Position>(reg, state);
        T extension = x1 - x2 - T(m_rest_length);
        return -T(m_stiffness) * extension;               // Hooke's law
    }
    template<typename Registry>
    T compute(typename TagSet2::Force, std::span<const T> state, const Registry& reg) const {
        return -query<typename TagSet1::Force>(reg, state);       // Newton's 3rd
    }
};""",
    },

    # ---------------------------------------------------------------- energy
    "energy": {
        "highlight_lines": (),
        "code": """\
template<Scalar T = double>
class EnergyMonitor final : public Component<0, T> {
public:
    template<typename Registry>
    T compute(system::TotalEnergy,
              std::span<const T> state, const Registry& registry) const {
        T m1 = query<mass1::Mass>(registry, state);
        T v1 = query<mass1::Velocity>(registry, state);
        T m2 = query<mass2::Mass>(registry, state);
        T v2 = query<mass2::Velocity>(registry, state);
        T pe = query<spring::PotentialEnergy>(registry, state);
        return T(0.5)*m1*v1*v1 + T(0.5)*m2*v2*v2 + pe;
    }
};""",
    },

    # ---------------------------------------------------------------- wiring
    "wiring": {
        "highlight_lines": (8, 9),
        "code": """\
auto sys = makeODESystem<double>(
    Mass1<double>(1.0, /*x0*/ 0.0, /*v0*/ 0.0),
    Mass2<double>(1.0, /*x0*/ 2.5, /*v0*/ 0.0),
    Spring12<double>(/*k*/ 4.0, /*L0*/ 1.0),
    EnergyMonitor<double>()
);

// the wiring is CHECKED at compile time:
static_assert(decltype(sys)::hasFunction<system::TotalEnergy>());

RK4Solver solver;
auto sol = solver.solve(derivs(sys), sys.getStateDimension(),
                        0.0, 20.0, /*dt*/ 0.001, sys.getInitialState());

double E = sys.query<system::TotalEnergy>(sol.states.back());""",
    },

    # -------------------------------------------------------------- registry
    "registry": {
        "highlight_lines": (7, 12, 13),
        "code": """\
template<typename T, ComponentConcept... Components>
class Registry {
    std::tuple<const Components&...> m_components;

    template<StateTagConcept Tag>
    static constexpr bool hasFunction() {
        return (ProvidesStateFunction<Components, Tag, T, Registry> || ...);
    }                                     //        fold over all components

    template<StateTagConcept Tag>
    auto computeFunction(std::span<const T> state) const {
        static_assert(hasFunction<Tag>(),
                      "No component provides this state function");
        const auto& provider = findProvider<Tag>();   // index found constexpr
        return provider.compute(Tag{}, state, *this);
    }
};""",
    },

    # -------------------------------------------------------------- concepts
    "concepts": {
        "highlight_lines": (),
        "code": """\
// what a component IS:
template<typename C>
concept ComponentConcept = requires(const C& c) {
    { C::state_size } -> std::convertible_to<size_t>;
    { c.getInitialLocalState() } -> std::same_as<typename C::LocalState>;
};

// what it PROVIDES (a quantity, selected by tag):
template<typename C, typename Tag, typename T, typename Registry>
concept ProvidesStateFunction =
    ComponentConcept<C> && StateTagConcept<Tag> &&
    requires(const C& c, std::span<const T> s, const Registry& reg)
    { c.compute(Tag{}, s, reg); };""",
    },

    # -------------------------------------------------------------- autodiff
    "autodiff": {
        "highlight_lines": (5, 6),
        "code": """\
// simulate:
auto sim  = makeODESystem<double>(comps...);

// differentiate — SAME components, different scalar:
using D = Dual<double, 14>;                  // forward-mode autodiff
auto grad = makeODESystem<D>(comps...);
auto J    = computeJacobian(grad, t, state); // exact, to machine precision

// linearize -> LQR gain straight from the nonlinear model
auto K = lqr(J.A, J.B, Q, R);""",
    },
}
