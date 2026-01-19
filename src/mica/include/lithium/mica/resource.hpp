#pragma once

#include "lithium/mica/types.hpp"
#include "lithium/core/memory.hpp"
#include <memory>
#include <vector>

namespace lithium::mica {

// ============================================================================
// Texture
// ============================================================================

// Note: ImageFormat is defined in types.hpp

/// Texture resource for image data
class Texture {
public:
    virtual ~Texture() = default;

    /// Get texture dimensions (individual)
    [[nodiscard]] virtual i32 width() const noexcept = 0;
    [[nodiscard]] virtual i32 height() const noexcept = 0;

    /// Get texture size (combined)
    [[nodiscard]] virtual Size size() const noexcept = 0;

    /// Get image format
    [[nodiscard]] virtual ImageFormat format() const noexcept = 0;

    /// Get number of mip levels
    [[nodiscard]] virtual i32 mip_levels() const noexcept = 0;

    /// Get native handle for platform-specific operations
    [[nodiscard]] virtual void* native_handle() noexcept = 0;

    /// Get raw pixel data
    [[nodiscard]] virtual const void* data() const noexcept = 0;
    [[nodiscard]] virtual usize data_size() const noexcept = 0;

    /// Update texture with new data
    virtual void update(const void* data, usize size) = 0;
    virtual void update_data(const void* data, usize size, i32 mip_level) = 0;

    /// Mipmap generation
    virtual void generate_mipmaps() = 0;

    /// Texture sampling parameters
    virtual void set_filter_mode(FilterMode min_filter, FilterMode mag_filter) = 0;
    virtual void set_wrap_mode(WrapMode wrap_u, WrapMode wrap_v) = 0;

protected:
    Texture() = default;
};

// ============================================================================
// Buffer
// ============================================================================

// Note: BufferType and BufferUsage are defined in types.hpp

class Buffer {
public:
    virtual ~Buffer() = default;

    /// Get buffer size in bytes
    [[nodiscard]] virtual usize size() const noexcept = 0;

    /// Get buffer type
    [[nodiscard]] virtual BufferType type() const noexcept = 0;

    /// Map buffer for CPU access
    [[nodiscard]] virtual void* map() = 0;
    virtual void unmap() = 0;

    /// Update buffer with new data
    virtual void update(const void* data, usize size, usize offset = 0) = 0;

protected:
    Buffer() = default;
};

// ============================================================================
// Shader
// ============================================================================

class Shader {
public:
    virtual ~Shader() = default;

    /// Get vertex shader source
    [[nodiscard]] virtual const String& vertex_source() const noexcept = 0;
    [[nodiscard]] virtual const String& fragment_source() const noexcept = 0;

protected:
    Shader() = default;
};

// ============================================================================
// Render Target
// ============================================================================

class RenderTarget {
public:
    virtual ~RenderTarget() = default;

    /// Get render target dimensions
    [[nodiscard]] virtual i32 width() const noexcept = 0;
    [[nodiscard]] virtual i32 height() const noexcept = 0;

    /// Get image format
    [[nodiscard]] virtual ImageFormat format() const noexcept = 0;

    /// Get texture (if backed by texture)
    [[nodiscard]] virtual Texture* texture() noexcept = 0;

protected:
    RenderTarget() = default;
};

} // namespace lithium::mica
