#pragma once

#include "state_function_tags.hpp"
#include "scalar.hpp"
#include <array>
#include <vector>
#include <span>
#include <type_traits>
#include <concepts>

namespace sopot {

// Forward declarations
template<size_t N, Scalar T> class TypedComponent;

//=============================================================================
// Component Concepts - the contract a component meets, checked at compile time
//=============================================================================

// What every component is: the state-type aliases, a compile-time state_size,
// and an initial value for its own state slice.
template<typename C>
concept TypedComponentConcept = requires(const C& c) {
    typename C::scalar_type;
    typename C::LocalState;
    typename C::LocalDerivative;
    { C::state_size } -> std::convertible_to<size_t>;
    { c.getInitialLocalState() } -> std::same_as<typename C::LocalState>;
};

// Extra contract for a component that owns state (state_size > 0): it provides
// derivatives(t, state, registry) returning the time-derivative of its own
// slice, reading its own variables via localState() and others via query().
template<typename Component, typename T, typename Registry>
concept HasDerivativesMethod = requires(
    const Component& c, T t, std::span<const T> state, const Registry& registry
) {
    { c.derivatives(t, state, registry) } -> std::same_as<typename Component::LocalDerivative>;
};

// A component provides Tag's state function if it defines
// compute(Tag, state, registry). Every compute() takes the registry - even the
// ones that ignore it - so any state function may query others uniformly.
template<typename Component, typename Tag, typename T, typename Registry>
concept TypedProvidesStateFunction = TypedComponentConcept<Component> &&
    StateTagConcept<Tag> &&
    requires(const Component& c, std::span<const T> state, const Registry& reg) {
        { c.compute(Tag{}, state, reg) };
    };

//=============================================================================
// query<Tag>() - Ask the system for a quantity by its tag
//=============================================================================
// A component computes one quantity from another by querying the registry:
//
//     T force = query<mass1::Force>(registry, state);
//
// As a free function it needs no 'template' disambiguator at the call site,
// unlike the equivalent member call registry.template computeFunction<Tag>().
//=============================================================================

// The registry provides this tag's quantity.
template<typename Registry, typename Tag>
concept RegistryProvides = requires {
    { Registry::template hasFunction<Tag>() } -> std::convertible_to<bool>;
} && Registry::template hasFunction<Tag>();

template<StateTagConcept Tag, typename Registry, typename T>
    requires RegistryProvides<Registry, Tag>
inline auto query(const Registry& registry, std::span<const T> state) {
    return registry.template computeFunction<Tag>(state);
}

//=============================================================================
// TypedComponent - Non-virtual base class for components
//=============================================================================
// No virtual functions: a component just provides the methods the concepts
// above require, resolved at compile time.
//
// Required of every component:
//   - getInitialLocalState() -> LocalState
//   - compute(Tag{}, state, registry) for each state function it provides
// Required only if state_size > 0:
//   - derivatives(t, state, registry) -> LocalDerivative
//
// This base class supplies the type aliases (scalar_type, LocalState,
// LocalDerivative), tracks the component's offset in the global state vector,
// and provides localState() for reading the component's own variables.
//=============================================================================

template<size_t StateSize, Scalar T = double>
class TypedComponent {
public:
    using scalar_type = T;
    static constexpr size_t state_size = StateSize;
    using LocalState = ScalarState<T, StateSize>;
    using LocalDerivative = ScalarState<T, StateSize>;

    // No virtual destructor needed - no polymorphic deletion through base pointer
    ~TypedComponent() = default;

    // Where this component's slice begins in the global state vector.
    size_t getStateOffset() const noexcept { return m_state_offset; }
    void setStateOffset(size_t offset) noexcept { m_state_offset = offset; }

protected:
    size_t m_state_offset{0};

    // Read one variable of THIS component's own state. The argument is the
    // full global state vector; index is local (0-based within our slice).
    T localState(std::span<const T> global_state, size_t index) const {
        return global_state[m_state_offset + index];
    }
};

//=============================================================================
// TypedRegistry - Compile-time registry for state function dispatch
//=============================================================================
// Holds references to every component and, for a given tag, resolves which
// component provides it - entirely at compile time. The provider's compute()
// receives the registry, so it can query further state functions in turn.
//=============================================================================

template<typename T, TypedComponentConcept... Components>
class TypedRegistry {
    std::tuple<const Components&...> m_components;

    // Self type for concept checks
    using Self = TypedRegistry<T, Components...>;

    // Index of the first component whose compute() accepts this tag.
    // The whole search runs during template instantiation, so by the time the
    // program runs the provider is already a fixed, known index.
    template<StateTagConcept Tag, size_t Index = 0>
    static constexpr size_t findProviderIndex() {
        if constexpr (Index >= sizeof...(Components)) {
            return 0;  // not found; hasFunction()'s static_assert reports it
        } else {
            using ComponentType = std::tuple_element_t<Index, std::tuple<Components...>>;
            if constexpr (TypedProvidesStateFunction<ComponentType, Tag, T, Self>) {
                return Index;
            } else {
                return findProviderIndex<Tag, Index + 1>();
            }
        }
    }

    // Get component by index - returns reference
    template<size_t Index>
    constexpr decltype(auto) getComponentByIndex() const {
        return std::get<Index>(m_components);
    }

    // Find provider using compile-time index (avoids recursive findProvider)
    template<StateTagConcept Tag>
    constexpr decltype(auto) findProvider() const {
        constexpr size_t provider_index = findProviderIndex<Tag>();
        return getComponentByIndex<provider_index>();
    }

public:
    explicit constexpr TypedRegistry(const Components&... components)
        : m_components(components...) {}

    // Compile-time function availability check
    template<StateTagConcept Tag>
    static constexpr bool hasFunction() {
        return (TypedProvidesStateFunction<Components, Tag, T, Self> || ...);
    }

    // The dispatch mechanism: hand the chosen provider the state and the
    // registry, so its compute() can in turn query other state functions.
    // The public spelling is the free function query<Tag>(registry, state),
    // which simply forwards here.
    template<StateTagConcept Tag>
    auto computeFunction(std::span<const T> state) const {
        static_assert(hasFunction<Tag>(), "No component provides this state function");
        return findProvider<Tag>().compute(Tag{}, state, *this);
    }
};

//=============================================================================
// TypedODESystem - Compile-time ODE system composition
//=============================================================================
// Composes multiple components into an ODE system.
// All dispatch is resolved at compile time - no virtual functions.
//
// Components must provide:
//   - derivatives(t, state, registry) -> LocalDerivative
//     (only required if state_size > 0)
//   - getInitialLocalState() -> LocalState
//=============================================================================

template<typename T, TypedComponentConcept... Components>
class TypedODESystem {
private:
    std::tuple<Components...> m_components;
    TypedRegistry<T, Components...> m_registry;

    static constexpr size_t m_total_state_size = (Components::state_size + ...);
    using RegistryType = TypedRegistry<T, Components...>;

    // Each component owns a contiguous slice of the global state vector.
    // This precomputes where every slice starts (and the total size, last).
    static constexpr auto make_offset_array() {
        std::array<size_t, sizeof...(Components) + 1> offsets{};
        size_t offset = 0;
        size_t i = 0;
        // Fold expression: processes all Components in parallel
        ((offsets[i++] = offset, offset += Components::state_size), ...);
        offsets[sizeof...(Components)] = offset;  // Total size at end
        return offsets;
    }

    static constexpr auto offset_array = make_offset_array();

    // Tell each component where its slice begins.
    constexpr void initializeOffsets() {
        [this]<size_t... Is>(std::index_sequence<Is...>) {
            (std::get<Is>(m_components).setStateOffset(offset_array[Is]), ...);
        }(std::make_index_sequence<sizeof...(Components)>{});
    }

    // Gather every component's local derivatives into the global derivative
    // vector. The fold visits all components; each writes into its own slice.
    template<size_t I>
    void collectDerivativeForComponent(std::vector<T>& derivatives, T t,
                                       const std::vector<T>& state) const {
        using ComponentType = std::tuple_element_t<I, std::tuple<Components...>>;
        constexpr size_t local_size = ComponentType::state_size;

        if constexpr (local_size > 0) {
            const auto& component = std::get<I>(m_components);
            constexpr size_t off = offset<I>();

            // Compile-time requirement: component must have derivatives method
            static_assert(
                HasDerivativesMethod<ComponentType, T, RegistryType>,
                "Component with state_size > 0 must provide derivatives(t, state, registry)"
            );

            // Hand the component the full state; it picks out its own slice.
            auto local_derivs = component.derivatives(t, std::span<const T>(state), m_registry);
            for (size_t j = 0; j < local_size; ++j) {
                derivatives[off + j] = local_derivs[j];
            }
        }
    }

    void collectDerivatives(std::vector<T>& derivatives, T t, const std::vector<T>& state) const {
        [this, &derivatives, t, &state]<size_t... Is>(std::index_sequence<Is...>) {
            // Fold expression: processes all components in parallel
            (collectDerivativeForComponent<Is>(derivatives, t, state), ...);
        }(std::make_index_sequence<sizeof...(Components)>{});
    }

    template<size_t I>
    static constexpr size_t offset() {
        return offset_array[I];
    }

    // Gather every component's initial local state into the global vector.
    template<size_t I>
    void collectInitialStateForComponent(std::vector<T>& state) const {
        using ComponentType = std::tuple_element_t<I, std::tuple<Components...>>;
        constexpr size_t local_size = ComponentType::state_size;

        if constexpr (local_size > 0) {
            const auto& component = std::get<I>(m_components);
            auto local_state = component.getInitialLocalState();
            size_t off = component.getStateOffset();

            for (size_t j = 0; j < local_state.size; ++j) {
                state[off + j] = local_state[j];
            }
        }
    }

    void collectInitialStates(std::vector<T>& state) const {
        [this, &state]<size_t... Is>(std::index_sequence<Is...>) {
            // Fold expression: processes all components in parallel
            (collectInitialStateForComponent<Is>(state), ...);
        }(std::make_index_sequence<sizeof...(Components)>{});
    }

public:
    using scalar_type = T;

    explicit TypedODESystem(Components... components)
        : m_components(std::move(components)...)
        , m_registry(std::get<Components>(m_components)...) {
        initializeOffsets();
    }

    // Core ODE interface
    std::vector<T> computeDerivatives(T t, const std::vector<T>& state) const {
        std::vector<T> derivatives(m_total_state_size);
        collectDerivatives(derivatives, t, state);
        return derivatives;
    }

    static constexpr size_t getStateDimension() noexcept {
        return m_total_state_size;
    }

    std::vector<T> getInitialState() const {
        std::vector<T> state(m_total_state_size);
        collectInitialStates(state);
        return state;
    }

    // State function access
    template<StateTagConcept Tag>
    static constexpr bool hasFunction() {
        return RegistryType::template hasFunction<Tag>();
    }

    // Ask the system for a quantity by its tag - the driver-facing spelling of
    // the same query<>() components use internally.
    template<StateTagConcept Tag>
    auto query(const std::vector<T>& state) const {
        return query<Tag>(std::span<const T>(state));
    }

    template<StateTagConcept Tag>
    auto query(std::span<const T> state) const {
        return m_registry.template computeFunction<Tag>(state);
    }
};

// Factory function: deduces the component types so callers can write
// makeTypedODESystem<double>(Mass1{...}, Spring12{...}, ...).
template<typename T = double, TypedComponentConcept... Components>
auto makeTypedODESystem(Components&&... components) {
    return TypedODESystem<T, std::decay_t<Components>...>(
        std::forward<Components>(components)...
    );
}

} // namespace sopot
