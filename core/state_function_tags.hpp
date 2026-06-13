#pragma once

#include <type_traits>

namespace sopot {

// A state-function tag is just an empty type used as a compile-time key.
//
// There is no runtime metadata (no id, no name): a quantity is identified by
// its tag *type*, and the system dispatches on that type alone. A component
// "provides" a quantity by defining `compute(Tag, ...)` for that tag.
template<typename Tag>
concept StateTagConcept = std::is_class_v<Tag>;

} // namespace sopot
