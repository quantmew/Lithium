/**
 * Software Resource Implementation
 */

#include "sw_backend.hpp"
#include <iostream>

namespace lithium::mica::software {

// ============================================================================
// SoftwareTexture
// ============================================================================

class SoftwareTexture : public Texture {
public:
    SoftwareTexture(i32 width, i32 height, ImageFormat format)
        : m_width(width)
        , m_height(height)
        , m_format(format)
    {
        usize size = static_cast<usize>(width * height) * bytes_per_pixel(format);
        m_data.resize(size, 0);
    }

    [[nodiscard]] i32 width() const noexcept override {
        return m_width;
    }

    [[nodiscard]] i32 height() const noexcept override {
        return m_height;
    }

    [[nodiscard]] Size size() const noexcept override {
        return {static_cast<f32>(m_width), static_cast<f32>(m_height)};
    }

    [[nodiscard]] ImageFormat format() const noexcept override {
        return m_format;
    }

    [[nodiscard]] i32 mip_levels() const noexcept override {
        return 1;  // Software renderer doesn't support mipmaps
    }

    [[nodiscard]] void* native_handle() noexcept override {
        return m_data.data();
    }

    [[nodiscard]] const void* data() const noexcept override {
        return m_data.data();
    }

    [[nodiscard]] usize data_size() const noexcept override {
        return m_data.size();
    }

    void update(const void* data, usize size) override {
        usize copy_size = std::min(size, m_data.size());
        std::memcpy(m_data.data(), data, copy_size);
    }

    void update_data(const void* data, usize size, i32 mip_level) override {
        if (mip_level != 0) {
            std::cerr << "SoftwareTexture: Mipmaps not supported" << std::endl;
            return;
        }
        update(data, size);
    }

    void generate_mipmaps() override {
        // Not supported in software renderer
    }

    void set_filter_mode(FilterMode min_filter, FilterMode mag_filter) override {
        // Not supported in software renderer
    }

    void set_wrap_mode(WrapMode wrap_u, WrapMode wrap_v) override {
        // Not supported in software renderer
    }

private:
    i32 m_width;
    i32 m_height;
    ImageFormat m_format;
    std::vector<u8> m_data;
};

// ============================================================================
// SoftwareBuffer
// ============================================================================

class SoftwareBuffer : public Buffer {
public:
    SoftwareBuffer(usize size, BufferType type, BufferUsage usage)
        : m_size(size)
        , m_type(type)
        , m_usage(usage)
    {
        m_data.resize(size, 0);
    }

    [[nodiscard]] usize size() const noexcept override {
        return m_size;
    }

    [[nodiscard]] BufferType type() const noexcept override {
        return m_type;
    }

    [[nodiscard]] void* map() override {
        return m_data.data();
    }

    void unmap() override {
        // Nothing to do
    }

    void update(const void* data, usize size, usize offset) override {
        if (offset + size > m_data.size()) {
            std::cerr << "SoftwareBuffer: Update out of bounds" << std::endl;
            return;
        }
        std::memcpy(m_data.data() + offset, data, size);
    }

private:
    usize m_size;
    BufferType m_type;
    BufferUsage m_usage;
    std::vector<u8> m_data;
};

// ============================================================================
// SoftwareRenderTarget
// ============================================================================

class SoftwareRenderTarget : public RenderTarget {
public:
    SoftwareRenderTarget(i32 width, i32 height, ImageFormat format)
        : m_width(width)
        , m_height(height)
        , m_format(format)
    {
        m_texture = std::make_unique<SoftwareTexture>(width, height, format);
    }

    [[nodiscard]] i32 width() const noexcept override {
        return m_width;
    }

    [[nodiscard]] i32 height() const noexcept override {
        return m_height;
    }

    [[nodiscard]] ImageFormat format() const noexcept override {
        return m_format;
    }

    [[nodiscard]] Texture* texture() noexcept override {
        return m_texture.get();
    }

private:
    i32 m_width;
    i32 m_height;
    ImageFormat m_format;
    std::unique_ptr<SoftwareTexture> m_texture;
};

// ============================================================================
// SoftwareBackend Resource Creation Methods
// ============================================================================

std::unique_ptr<Texture> SoftwareBackend::create_texture(
    i32 width,
    i32 height,
    ImageFormat format,
    const void* data,
    i32 stride) {

    auto texture = std::make_unique<SoftwareTexture>(width, height, format);
    if (data) {
        texture->update(data, static_cast<usize>(width * height) * bytes_per_pixel(format));
    }
    return texture;
}

std::unique_ptr<Buffer> SoftwareBackend::create_buffer(
    BufferType type,
    BufferUsage usage,
    usize size,
    const void* data) {

    auto buffer = std::make_unique<SoftwareBuffer>(size, type, usage);
    if (data) {
        buffer->update(data, size, 0);
    }
    return buffer;
}

std::unique_ptr<RenderTarget> SoftwareBackend::create_render_target(
    const RenderTargetDesc& desc) {

    return std::make_unique<SoftwareRenderTarget>(desc.width, desc.height, desc.format);
}

} // namespace lithium::mica::software
