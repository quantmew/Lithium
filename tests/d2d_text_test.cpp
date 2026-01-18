/**
 * Direct2D Text Rendering Test
 *
 * Direct test of Direct2D text rendering functionality
 */

#include "lithium/platform/window.hpp"
#include "lithium/platform/graphics_context.hpp"
#include "lithium/platform/graphics_config.hpp"
#include "lithium/core/logger.hpp"
#include <iostream>

using namespace lithium;

int main() {
    logging::init();
    logging::set_level(LogLevel::Info);

    LITHIUM_LOG_INFO("=== Direct2D Text Rendering Test ===");

    // Initialize platform
    if (!platform::platform::init()) {
        LITHIUM_LOG_ERROR("Failed to initialize platform");
        return 1;
    }

    // Create window
    platform::WindowConfig window_config;
    window_config.title = "Direct2D Text Test";
    window_config.width = 800;
    window_config.height = 600;

    auto window = platform::Window::create(window_config);
    if (!window) {
        LITHIUM_LOG_ERROR("Failed to create window");
        return 1;
    }

    LITHIUM_LOG_INFO("Window created: {}x{}", window->size().width, window->size().height);
    window->show();

    // Create Direct2D graphics context
    platform::GraphicsConfig config;
    config.preferred_backend = platform::GraphicsConfig::BackendType::Direct2D;
    config.enable_vsync = true;

    LITHIUM_LOG_INFO("Creating Direct2D graphics context...");
    auto graphics = platform::GraphicsContext::create(window.get(), config);

    if (!graphics) {
        LITHIUM_LOG_ERROR("Failed to create Direct2D graphics context!");
        LITHIUM_LOG_INFO("Trying software fallback...");

        // Try software rendering
        config.preferred_backend = platform::GraphicsConfig::BackendType::Software;
        graphics = platform::GraphicsContext::create(window.get(), config);

        if (!graphics) {
            LITHIUM_LOG_ERROR("Failed to create software graphics context too!");
            return 1;
        }

        LITHIUM_LOG_INFO("Using software rendering");
    } else {
        LITHIUM_LOG_INFO("Direct2D graphics context created successfully!");
        LITHIUM_LOG_INFO("Hardware Accelerated: YES");
    }

    // Main loop
    LITHIUM_LOG_INFO("Starting render loop...");
    int frame_count = 0;

    while (!window->should_close() && frame_count < 180) {
        window->poll_events();

        // Render
        graphics->begin_frame();
        graphics->clear(Color{245, 245, 250, 255});

        // Draw title bar background
        graphics->fill_rect({0, 0, 800, 60}, Color{70, 130, 180, 255});

        // Draw various text samples
        Color white = {255, 255, 255, 255};
        Color black = {20, 20, 20, 255};
        Color gray = {100, 100, 100, 255};

        // Title
        graphics->draw_text({20, 15}, "Direct2D Text Rendering Test", white, 28);

        // Test different font sizes
        graphics->draw_text({20, 80}, "Large Text (24px)", black, 24);
        graphics->draw_text({20, 115}, "Medium Text (18px)", black, 18);
        graphics->draw_text({20, 145}, "Normal Text (14px)", black, 14);
        graphics->draw_text({20, 170}, "Small Text (12px)", black, 12);

        // Test different colors
        graphics->draw_text({20, 210}, "Red Color", {220, 50, 50, 255}, 20);
        graphics->draw_text({150, 210}, "Green Color", {50, 180, 50, 255}, 20);
        graphics->draw_text({290, 210}, "Blue Color", {50, 100, 220, 255}, 20);

        // Draw a box around some text
        String box_text = "Text in a box";
        f32 text_width = graphics->measure_text(box_text, 16);
        graphics->draw_text({420, 210}, box_text, black, 16);
        graphics->stroke_rect({415, 205, text_width + 10, 30}, {200, 100, 50, 255}, 2.0f);

        // Test text measurement
        String long_text = "This is a longer text to measure and display";
        auto text_size = graphics->measure_text_size(long_text, 14);

        graphics->draw_text({20, 260}, long_text, gray, 14);
        graphics->fill_rect({20, 290, text_size.width, 2}, {70, 130, 180, 255});

        String info = "Width: " + std::to_string(static_cast<int>(text_size.width)) +
                     "px  Height: " + std::to_string(static_cast<int>(text_size.height)) + "px";
        graphics->draw_text({20, 300}, info, {100, 100, 150, 255}, 12);

        // Draw frame counter
        String frame_str = "Frame: " + std::to_string(frame_count);
        graphics->draw_text({650, 15}, frame_str, white, 16);

        graphics->end_frame();
        graphics->swap_buffers();

        frame_count++;

        // Log every 60 frames
        if (frame_count % 60 == 0) {
            LITHIUM_LOG_INFO("Rendered {} frames", frame_count);
        }
    }

    LITHIUM_LOG_INFO("Test completed. Total frames: {}", frame_count);

    // Cleanup
    platform::platform::shutdown();
    logging::shutdown();

    return 0;
}
