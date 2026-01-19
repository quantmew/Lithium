/**
 * Direct2D Resource Implementation
 */

#include "d2d_backend.hpp"
#include <iostream>

namespace lithium::mica::direct2d {

// ============================================================================
// Texture (Direct2D Bitmap)
// ============================================================================>

class D2DTexture : public Texture {
public:
    D2DTexture(ComPtr<ID2D1Bitmap1> bitmap, i32 width, i32 height, ImageFormat format)
        : m_bitmap(std::move(bitmap))
        , m_width(width)
        , m_height(height)
        , m_format(format)
    {}

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
        return 1;  // TODO: Support mipmaps
    }

    [[nodiscard]] void* native_handle() noexcept override {
        return m_bitmap.Get();
    }

    [[nodiscard]] const void* data() const noexcept override {
        return nullptr;  // Direct2D bitmaps don't provide direct CPU access
    }

    [[nodiscard]] usize data_size() const noexcept override {
        return 0;  // Direct2D bitmaps are GPU resources
    }

    void update(const void* data, usize size) override {
        update_data(data, size, 0);
    }

    void update_data(const void* data, usize size, i32 mip_level) override {
        if (!m_bitmap || !data) return;

        D2D1_RECT_U rect = D2D1::RectU(0, 0, m_width, m_height);
        HRESULT hr = m_bitmap->CopyFromMemory(
            &rect,
            data,
            static_cast<UINT>(bytes_per_pixel(m_format))
        );

        if (FAILED(hr)) {
            std::cerr << "D2DTexture: Failed to update bitmap data" << std::endl;
        }
    }

    void generate_mipmaps() override {
        // TODO: Implement mipmap generation
    }

    void set_filter_mode(FilterMode min_filter, FilterMode mag_filter) override {
        // Direct2D bitmaps don't have filter mode
        // This is set when drawing
    }

    void set_wrap_mode(WrapMode wrap_u, WrapMode wrap_v) override {
        // Direct2D bitmaps don't have wrap mode
        // This is set when drawing
    }

private:
    ComPtr<ID2D1Bitmap1> m_bitmap;
    i32 m_width;
    i32 m_height;
    ImageFormat m_format;
};

// ============================================================================
// D2DBackend Resource Creation Methods
// ============================================================================

std::unique_ptr<Texture> D2DBackend::create_texture(
    i32 width,
    i32 height,
    ImageFormat format,
    const void* data,
    i32 stride) {

    // TODO: Implement texture creation
    std::cerr << "D2DBackend: Texture creation not yet implemented" << std::endl;
    return nullptr;
}

} // namespace lithium::mica::direct2d
