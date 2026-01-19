#pragma once

/// Mica Graphics Engine
/// A high-performance, cross-platform graphics rendering engine for Lithium Browser
///
/// Named after the lithium-bearing mineral Mica (KAl2(AlSi3O10)(F,OH)2)
/// This engine provides:
/// - Platform-agnostic graphics APIs
/// - Multiple rendering backends (OpenGL, Direct2D, Software, etc.)
/// - Integration with Beryl text engine for high-quality text rendering
///
/// Architecture:
/// ┌─────────────────────────────────────┐
/// │   High-Level Drawing API            │
/// │   (Painter, Canvas, Path, etc.)     │
/// └─────────────────────────────────────┘
///                ↓
/// ┌─────────────────────────────────────┐
/// │   Beryl Text Engine                 │
/// │   (Text shaping & rendering)        │
/// └─────────────────────────────────────┘
///                ↓
/// ┌─────────────────────────────────────┐
/// │   Rendering Backend Interface       │
/// └────────────────���────────────────────┘
///                ↓
/// ┌──────────┬──────────┬──────────────┐
/// │ OpenGL   │ Direct2D │  Software    │
/// └──────────┴──────────┴──────────────┘

#include "lithium/mica/types.hpp"
#include "lithium/mica/context.hpp"
#include "lithium/mica/painter.hpp"
#include "lithium/mica/canvas.hpp"
#include "lithium/mica/resource.hpp"
#include "lithium/mica/backend.hpp"

// Forward declaration for Beryl TextEngine
namespace lithium::beryl {
class TextEngine;
}

namespace lithium::mica {

/// Engine initialization and configuration
class Engine {
public:
    Engine();
    ~Engine();

    // Prevent copying
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Allow moving
    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    /// Initialize the engine with a specific backend
    [[nodiscard]] bool initialize(BackendType type = BackendType::Auto);

    /// Get the current backend type
    [[nodiscard]] BackendType backend_type() const noexcept;

    /// Create a graphics context for rendering
    [[nodiscard]] std::unique_ptr<Context> create_context(void* native_window);

    /// Create a painter for drawing operations
    [[nodiscard]] std::unique_ptr<Painter> create_painter(Context& context);

    /// Access to the text engine (Beryl)
    /// Note: Returns nullptr for now, text engine integration is optional
    [[nodiscard]] beryl::TextEngine* text_engine() noexcept { return nullptr; }

    /// Engine capabilities
    struct Capabilities {
        bool supports_multisampling;
        bool supports_shaders;
        bool supports_compute;
        i32 max_texture_size;
        i32 max_texture_units;
    };

    [[nodiscard]] Capabilities capabilities() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    // Note: Text engine integration is optional for now
    // std::unique_ptr<beryl::TextEngine> m_text_engine;
};

/// Get the global engine instance (if initialized)
[[nodiscard]] Engine* engine() noexcept;

} // namespace lithium::mica
