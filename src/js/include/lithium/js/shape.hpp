/**
 * Hidden Classes (Shapes) for efficient property storage
 *
 * Shapes describe the layout of object properties:
 * - Maps property names to slot indices
 * - Objects with the same Shape share the same property layout
 * - Property values are stored in a dense vector indexed by slot number
 * - Shape transitions form a tree for efficient property addition
 */

#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <atomic>

namespace lithium::js {

class Shape;
using ShapePtr = std::shared_ptr<Shape>;

// Property descriptor (for future extensibility: writable, enumerable, etc.)
struct PropertyDescriptor {
    u32 slot;           // Index in object's slot array
    bool writable{true};
    bool enumerable{true};
    bool configurable{true};
};

class Shape : public std::enable_shared_from_this<Shape> {
public:
    // Create the root (empty) shape
    static ShapePtr create_root();

    // Get or create a transition to a new shape with an added property
    ShapePtr add_property(const String& name);

    // Lookup property slot (-1 if not found)
    [[nodiscard]] i32 find_slot(const String& name) const;

    // Get property descriptor (nullptr if not found)
    [[nodiscard]] const PropertyDescriptor* get_descriptor(const String& name) const;

    // Number of slots needed for objects with this shape
    [[nodiscard]] u32 slot_count() const { return m_slot_count; }

    // Unique shape ID for inline cache matching
    [[nodiscard]] u32 id() const { return m_id; }

    // Get all property names in order
    [[nodiscard]] std::vector<String> property_names() const;

    // Check if shape matches (for IC)
    [[nodiscard]] bool matches(u32 shape_id) const { return m_id == shape_id; }

private:
    Shape();  // Private constructor, use create_root()

    // Property name -> descriptor (built up through transition chain)
    std::unordered_map<String, PropertyDescriptor> m_properties;

    // Transitions to child shapes (property name -> child shape)
    std::unordered_map<String, std::weak_ptr<Shape>> m_transitions;

    // Parent shape (for walking the transition chain)
    std::weak_ptr<Shape> m_parent;

    // Number of property slots
    u32 m_slot_count{0};

    // Unique ID for this shape (for fast IC comparison)
    u32 m_id;

    // Global ID counter
    static std::atomic<u32> s_next_id;
};

// Shape registry - manages root shapes and caches common shapes
class ShapeRegistry {
public:
    static ShapeRegistry& instance();

    // Get the root (empty) shape
    [[nodiscard]] ShapePtr root_shape() const { return m_root; }

private:
    ShapeRegistry();
    ShapePtr m_root;
};

} // namespace lithium::js
