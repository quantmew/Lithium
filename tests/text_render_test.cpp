/**
 * Simple Text Rendering Test
 *
 * Tests the text rendering functionality directly
 */

#include "lithium/platform/window.hpp"
#include "lithium/platform/graphics_context.hpp"
#include "lithium/platform/graphics_config.hpp"
#include "lithium/core/logger.hpp"
#include <iostream>

using namespace lithium;

int main() {
    // Initialize logging
    logging::init();
    logging::set_level(LogLevel::Info);

    LITHIUM_LOG_INFO("Text Rendering Test");
    LITHIUM_LOG_INFO("====================");

    // Initialize platform
    if (!platform::platform::init()) {
        LITHIUM_LOG_ERROR("Failed to initialize platform");
        return 1;
    }

    // Create window
    platform::WindowConfig window_config;
    window_config.title = "Text Rendering Test";
    window_config.width = 800;
    window_config.height = 600;

    auto window = platform::Window::create(window_config);
    if (!window) {
        LITHIUM_LOG_ERROR("Failed to create window");
        return 1;
    }

    window->show();

    // Create graphics context
    platform::GraphicsConfig config;
    config.preferred_backend = platform::GraphicsConfig::BackendType::Direct2D;

    auto graphics = platform::GraphicsContext::create(window.get(), config);
    if (!graphics) {
        LITHIUM_LOG_ERROR("Failed to create graphics context");
        return 1;
    }

    LITHIUM_LOG_INFO("Graphics context created successfully");

    // Main loop
    LITHIUM_LOG_INFO("Starting main loop...");
    int frame_count = 0;

    while (!window->should_close() && frame_count < 300) {
        window->poll_events();

        // Render
        graphics->begin_frame();
        graphics->clear(Color{240, 240, 240, 255});

        // Test text rendering at different positions and sizes
        Color text_color = {20, 20, 20, 255};

        // Title
        graphics->draw_text({20, 20}, "Lithium Browser - Text Rendering Test", text_color, 24);

        // Different sizes
        graphics->draw_text({20, 60}, "Hello, World! (16px)", text_color, 16);
        graphics->draw_text({20, 90}, "Hello, World! (20px)", text_color, 20);
        graphics->draw_text({20, 120}, "Hello, World! (32px)", text_color, 32);

        // Different colors
        graphics->draw_text({20, 170}, "Red Text", {255, 0, 0, 255}, 24);
        graphics->draw_text({20, 200}, "Green Text", {0, 255, 0, 255}, 24);
        graphics->draw_text({20, 230}, "Blue Text", {0, 0, 255, 255}, 24);

        // Text measurement test
        String test_text = "This is a test of text measurement";
        f32 width = graphics->measure_text(test_text, 16);
        auto size = graphics->measure_text_size(test_text, 16);

        graphics->draw_text({20, 280}, test_text, text_color, 16);
        graphics->draw_text({20, 310},
            "Text width: " + std::to_string(static_cast<int>(width)) + "px",
            {100, 100, 100, 255}, 14);
        graphics->draw_text({20, 330},
            "Text size: " + std::to_string(static_cast<int>(size.width)) + "x" + std::to_string(static_cast<int>(size.height)) + "px",
            {100, 100, 100, 255}, 14);

        // Draw a box around the text
        graphics->stroke_rect({18, 278, size.width + 4, size.height + 4}, {200, 200, 200, 255}, 1.0f);

        graphics->end_frame();
        graphics->swap_buffers();

        frame_count++;

        // Log every 60 frames
        if (frame_count % 60 == 0) {
            LITHIUM_LOG_INFO("Frame: {}", frame_count);
        }
    }

    LITHIUM_LOG_INFO("Test completed. Frames rendered: {}", frame_count);

    // Cleanup
    platform::platform::shutdown();
    logging::shutdown();

    return 0;
}
