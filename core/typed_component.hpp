#pragma once

#include "state_function_tags.hpp"
#include "scalar.hpp"
#include <array>
#include <vector>
#include <span>
#include <type_traits>
#include <concepts>
#include <stdexcept>
#include <string_view>

namespace sopot {

// Forward declarations
template<size_t N, Scalar T> class TypedComponent;

//=============================================================================
// Component Concepts - All compile-time verification
//=============================================================================

// Basic component structure requirements
template<typename C>
concept TypedComponentConcept = requires {
    typename C::scalar_type;
    typename C::LocalState;
    typename C::LocalDerivative;
    { C::state_size } -> std::convertible_to<size_t>;
};

// Check if component has derivatives method (CRTP-style, non-virtual)
// This is the required interface for components with state
template<typename Component, typename T, typename Registry>
concept HasDerivativesMethod = requires(
    const Component& c,
    T t,
    std::span<const T> local,
    std::span<const T> global,
    const Registry& registry
) {
    { c.derivatives(t, local, global, registry) } -> std::same_as<typename Component::LocalDerivative>;
};

// Check if component has getInitialLocalState (non-virtual)
template<typename Component>
concept HasInitialState = requires(const Component& c) {
    { c.getInitialLocalState() } -> std::same_as<typename Component::LocalState>;
};

// Check if component has identification methods (non-virtual)
template<typename Component>
concept HasIdentification = requires(const Component& c) {
    { c.getComponentType() } -> std::convertible_to<std::string_view>;
    { c.getComponentName() } -> std::convertible_to<std::string_view>;
};

// Complete component concept - all required interfaces
template<typename C, typename T, typename Registry>
concept CompleteTypedComponent =
    TypedComponentConcept<C> &&
    HasInitialState<C> &&
    HasIdentification<C> &&
    (C::state_size == 0 || HasDerivativesMethod<C, T, Registry>);

// Check if component provides a specific state function with span (preferred)
template<typename Component, typename Tag, typename T>
concept TypedProvidesStateFunctionSpan = TypedComponentConcept<Component> &&
    StateTagConcept<Tag> &&
    requires(const Component& c, std::span<const T> state) {
        { c.compute(Tag{}, state) };
    };

// Check if component provides registry-aware state function (span)
template<typename Component, typename Tag, typename T, typename Registry>
concept TypedProvidesRegistryAwareStateFunctionSpan = TypedComponentConcept<Component> &&
    StateTagConcept<Tag> &&
    requires(const Component& c, std::span<const T> state, const Registry& reg) {
        { c.compute(Tag{}, state, reg) };
    };

// Combined: component provides state function (simple or registry-aware)
template<typename Component, typename Tag, typename T, typename Registry>
concept TypedProvidesStateFunction =
    TypedProvidesStateFunctionSpan<Component, Tag, T> ||
    TypedProvidesRegistryAwareStateFunctionSpan<Component, Tag, T, Registry>;

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
// All dispatch is resolved at compile time through concepts and templates.
// No virtual functions - components must provide required methods directly.
//
// Required methods for components:
//   - derivatives(t, local_span, global_span, registry) -> LocalDerivative
//     (only required if state_size > 0)
//   - getInitialLocalState() -> LocalState
//   - getComponentType() -> std::string_view
//   - getComponentName() -> std::string_view
//   - compute(Tag{}, state) or compute(Tag{}, state, registry) for state functions
//
// This base class provides:
//   - Type aliases (scalar_type, LocalState, LocalDerivative)
//   - State offset management
//   - Helper functions for state access
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
// All state function resolution happens at compile time.
// Registry-aware compute() methods take precedence over simple compute().
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

    // Zero-overhead function dispatch (span interface only)
    // Registry-aware compute() takes precedence over simple compute()
    template<StateTagConcept Tag>
    auto computeFunction(std::span<const T> state) const {
        static_assert(hasFunction<Tag>(), "No component provides this state function");
        const auto& provider = findProvider<Tag>();
        using ProviderType = std::decay_t<decltype(provider)>;

        // Prefer registry-aware compute over simple compute
        if constexpr (TypedProvidesRegistryAwareStateFunctionSpan<ProviderType, Tag, T, Self>) {
            return provider.compute(Tag{}, state, *this);
        } else {
            return provider.compute(Tag{}, state);
        }
    }

    // Convenience overload for vector - converts to span
    template<StateTagConcept Tag>
    auto computeFunction(const std::vector<T>& state) const {
        return computeFunction<Tag>(std::span<const T>(state));
    }

    static constexpr size_t component_count() {
        return sizeof...(Components);
    }

    template<size_t I>
    constexpr const auto& getComponent() const {
        return std::get<I>(m_components);
    }
};

//=============================================================================
// TypedODESystem - Compile-time ODE system composition
//=============================================================================
// Composes multiple components into an ODE system.
// All dispatch is resolved at compile time - no virtual functions.
//
// Components must provide:
//   - derivatives(t, local_span, global_span, registry) -> LocalDerivative
//     (only required if state_size > 0)
//   - getInitialLocalState() -> LocalState
//=============================================================================

template<typename T, TypedComponentConcept... Components>
class TypedODESystem {
private:
    std::tuple<Components...> m_components;
    TypedRegistry<T, Components...> m_registry;

    static constexpr size_t m_total_state_size = (Components::state_size + ...);
    static constexpr size_t m_component_count = sizeof...(Components);
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

            // Create spans for local and global state
            std::span<const T> local_span(state.data() + off, local_size);
            std::span<const T> global_span(state);

            // Compile-time requirement: component must have derivatives method
            static_assert(
                HasDerivativesMethod<ComponentType, T, RegistryType>,
                "Component with state_size > 0 must provide derivatives(t, local, global, registry)"
            );

            auto local_derivs = component.derivatives(t, local_span, global_span, m_registry);
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

    template<StateTagConcept Tag>
    auto computeStateFunction(const std::vector<T>& state) const {
        return m_registry.template computeFunction<Tag>(state);
    }

    template<StateTagConcept Tag>
    auto computeStateFunction(std::span<const T> state) const {
        return m_registry.template computeFunction<Tag>(state);
    }

    // Batch function evaluation
    template<StateTagConcept... Tags>
    auto computeStateFunctions(const std::vector<T>& state) const {
        static_assert(sizeof...(Tags) > 0, "Must specify at least one function");
        static_assert((hasFunction<Tags>() && ...),
                     "All requested functions must be available");
        return std::tuple{m_registry.template computeFunction<Tags>(state)...};
    }

    // Component access
    template<size_t I>
    requires (I < sizeof...(Components))
    constexpr const auto& getComponent() const {
        return std::get<I>(m_components);
    }

    static constexpr size_t getComponentCount() noexcept {
        return m_component_count;
    }

    const auto& getRegistry() const {
        return m_registry;
    }

    // Convert state to values (for output)
    std::vector<double> stateValues(const std::vector<T>& state) const {
        std::vector<double> values(state.size());
        for (size_t i = 0; i < state.size(); ++i) {
            values[i] = value_of(state[i]);
        }
        return values;
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
