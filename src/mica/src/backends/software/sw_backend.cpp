/**
 * Software Backend Implementation
 */

#include "sw_backend.hpp"
#include "lithium/mica/backend.hpp"
#include <iostream>
#include <cstring>

namespace lithium::mica::software {

// ============================================================================
// SoftwareBackend
// ============================================================================

SoftwareBackend::SoftwareBackend() {
    // Software rendering has limited capabilities
    m_capabilities.supports_multisampling = false;
    m_capabilities.supports_shaders = false;
    m_capabilities.supports_compute = false;
    m_capabilities.supports_framebuffer_srgb = false;
    m_capabilities.max_texture_size = 8192;  // Limited by memory
    m_capabilities.max_texture_units = 1;
    m_capabilities.max_render_targets = 1;
    m_capabilities.max_vertex_attributes = 0;
    m_capabilities.max_uniform_buffer_bindings = 0;
    m_capabilities.max_anisotropy = 1.0f;
    m_capabilities.sample_counts = {1};  // No MSAA
}

SoftwareBackend::~SoftwareBackend() = default;

bool SoftwareBackend::initialize() {
    std::cout << "SoftwareBackend: Initialized" << std::endl;
    return true;
}

std::unique_ptr<Context> SoftwareBackend::create_context(
    NativeWindowHandle window,
    const SwapChainConfig& config) {

    auto context = std::make_unique<SoftwareContext>(*this, window, config);
    if (!context || !context->is_valid()) {
        std::cerr << "SoftwareBackend: Failed to create context" << std::endl;
        return nullptr;
    }

    return context;
}

std::unique_ptr<Shader> SoftwareBackend::create_shader(
    const String& vertex_source,
    const String& fragment_source) {

    // Software rendering doesn't use shaders
    std::cerr << "SoftwareBackend: Shaders not supported in software mode" << std::endl;
    return nullptr;
}

void SoftwareBackend::flush() {
    // Nothing to flush for software rendering
}

void SoftwareBackend::finish() {
    // Nothing to finish for software rendering
}

void register_software_backend_factory() {
    mica::register_backend_factory(BackendType::Software, []() -> std::unique_ptr<IBackend> {
        auto backend = std::make_unique<SoftwareBackend>();
        if (backend && backend->initialize()) {
            return backend;
        }
        return nullptr;
    });
}

} // namespace lithium::mica::software
