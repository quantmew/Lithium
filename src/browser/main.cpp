/**
 * Lithium Browser - A lightweight browser engine
 *
 * This is the main entry point for the Lithium browser application.
 */

#include "lithium/browser/engine.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/core/logger.hpp"
#include <iostream>

using namespace lithium;

int main(int argc, char* argv[]) {
    // Initialize logging
    logging::init();
    logging::set_level(LogLevel::Info);

    LITHIUM_LOG_INFO("Lithium Browser v0.1.0");

    // Initialize platform
    if (!platform::platform::init()) {
        LITHIUM_LOG_ERROR("Failed to initialize platform");
        return 1;
    }

    // Create window
    platform::WindowConfig config;
    config.title = "Lithium Browser";
    config.width = 1280;
    config.height = 720;

    auto window = platform::Window::create(config);
    if (!window) {
        LITHIUM_LOG_ERROR("Failed to create window");
        platform::platform::shutdown();
        return 1;
    }

    // Create graphics context
    auto graphics = platform::GraphicsContext::create(window.get());
    if (!graphics) {
        LITHIUM_LOG_ERROR("Failed to create graphics context");
        platform::platform::shutdown();
        return 1;
    }

    // Create browser engine
    browser::Engine engine;
    if (!engine.init()) {
        LITHIUM_LOG_ERROR("Failed to initialize browser engine");
        platform::platform::shutdown();
        return 1;
    }

    // Set up callbacks
    engine.set_title_changed_callback([&window](const String& title) {
        window->set_title("Lithium - "_s + title);
    });

    // Load initial URL or default page
    String initial_url = "about:blank";
    if (argc > 1) {
        initial_url = argv[1];
    }

    if (initial_url == "about:blank") {
        // Load a simple welcome page
        engine.load_html(R"(
<!DOCTYPE html>
<html>
<head>
    <title>Welcome to Lithium</title>
    <style>
        body {
            font-family: sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
            background: #f5f5f5;
        }
        h1 {
            color: #333;
        }
        p {
            color: #666;
            line-height: 1.6;
        }
    </style>
</head>
<body>
    <h1>Welcome to Lithium Browser</h1>
    <p>Lithium is a lightweight browser engine implemented from scratch in C++.</p>
    <p>Features:</p>
    <ul>
        <li>HTML5 parsing</li>
        <li>CSS styling</li>
        <li>JavaScript execution</li>
        <li>Layout and rendering</li>
    </ul>
</body>
</html>
        )");
    } else {
        engine.load_url(initial_url);
    }

    // Handle window resize
    auto size = window->size();
    engine.resize(size.width, size.height);

    // Event callback
    window->set_event_callback([&engine, &window](const platform::Event& event) {
        platform::EventDispatcher dispatcher(event);

        dispatcher.dispatch<platform::WindowCloseEvent>([&window](const auto&) {
            window->set_should_close(true);
            return true;
        });

        dispatcher.dispatch<platform::WindowResizeEvent>([&engine](const auto& e) {
            engine.resize(e.width, e.height);
            return true;
        });

        dispatcher.dispatch<platform::KeyEvent>([&engine](const auto& e) {
            engine.handle_event(platform::Event{e});
            return true;
        });

        dispatcher.dispatch<platform::MouseButtonEvent>([&engine](const auto& e) {
            engine.handle_event(platform::Event{e});
            return true;
        });

        dispatcher.dispatch<platform::MouseMoveEvent>([&engine](const auto& e) {
            engine.handle_event(platform::Event{e});
            return true;
        });

        dispatcher.dispatch<platform::MouseScrollEvent>([&engine](const auto& e) {
            engine.handle_event(platform::Event{e});
            return true;
        });
    });

    // Main loop
    while (!window->should_close()) {
        // Poll events
        window->poll_events();

        // Process pending tasks
        engine.process_tasks();

        // Render
        graphics->begin_frame();
        graphics->clear(Color::white());
        engine.render(*graphics);
        graphics->end_frame();

        // Swap buffers
        graphics->swap_buffers();
    }

    // Cleanup
    logging::shutdown();
    platform::platform::shutdown();

    return 0;
}
