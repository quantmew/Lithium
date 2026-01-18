/**
 * Shape implementation - Hidden Classes for efficient property storage
 */

#include "lithium/js/shape.hpp"

namespace lithium::js {

std::atomic<u32> Shape::s_next_id{0};

Shape::Shape() : m_id(s_next_id++) {}

ShapePtr Shape::create_root() {
    return ShapePtr(new Shape());
}

ShapePtr Shape::add_property(const String& name) {
    // Check if we already have a transition for this property
    auto it = m_transitions.find(name);
    if (it != m_transitions.end()) {
        if (auto existing = it->second.lock()) {
            return existing;
        }
    }

    // Create new shape with this property added
    auto new_shape = ShapePtr(new Shape());
    new_shape->m_parent = shared_from_this();
    new_shape->m_properties = m_properties;  // Copy existing properties
    new_shape->m_slot_count = m_slot_count + 1;

    // Add new property at the next slot
    PropertyDescriptor desc;
    desc.slot = m_slot_count;
    new_shape->m_properties[name] = desc;

    // Cache the transition
    m_transitions[name] = new_shape;

    return new_shape;
}

i32 Shape::find_slot(const String& name) const {
    auto it = m_properties.find(name);
    if (it != m_properties.end()) {
        return static_cast<i32>(it->second.slot);
    }
    return -1;
}

const PropertyDescriptor* Shape::get_descriptor(const String& name) const {
    auto it = m_properties.find(name);
    if (it != m_properties.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<String> Shape::property_names() const {
    std::vector<String> names;
    names.resize(m_slot_count);
    for (const auto& [name, desc] : m_properties) {
        if (desc.slot < names.size()) {
            names[desc.slot] = name;
        }
    }
    return names;
}

// ShapeRegistry singleton
ShapeRegistry& ShapeRegistry::instance() {
    static ShapeRegistry registry;
    return registry;
}

ShapeRegistry::ShapeRegistry() : m_root(Shape::create_root()) {}

} // namespace lithium::js
