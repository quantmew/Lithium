/**
 * Lithium Browser - A lightweight browser engine
 *
 * This is the main entry point for the Lithium browser application.
 */

#include "lithium/browser/engine.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/platform/graphics_config.hpp"
#include "lithium/platform/graphics_backend.hpp"
#include "lithium/core/logger.hpp"
#include <iostream>
#include <sstream>

using namespace lithium;

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] [URL]\n"
              << "\n"
              << "Options:\n"
              << "  --backend=TYPE    Graphics backend to use\n"
              << "                    Available: auto, software, opengl, direct2d\n"
              << "                    Default: auto\n"
              << "  --no-vsync        Disable vertical synchronization\n"
              << "  --msaa=N          Enable MSAA with N samples (2, 4, 8)\n"
              << "  --no-fallback     Disable fallback to software rendering\n"
              << "  --list-backends   List available graphics backends\n"
              << "  --help            Show this help message\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " --backend=opengl https://example.com\n"
              << "  " << program_name << " --backend=direct2d --no-vsync\n"
              << "  " << program_name << " --list-backends\n";
}

platform::GraphicsConfig parse_graphics_config(int argc, char* argv[]) {
    platform::GraphicsConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--backend=auto") {
            config.preferred_backend = platform::GraphicsConfig::BackendType::Auto;
        } else if (arg == "--backend=software") {
            config.preferred_backend = platform::GraphicsConfig::BackendType::Software;
        } else if (arg == "--backend=opengl") {
            config.preferred_backend = platform::GraphicsConfig::BackendType::OpenGL;
        } else if (arg == "--backend=direct2d") {
            config.preferred_backend = platform::GraphicsConfig::BackendType::Direct2D;
        } else if (arg == "--no-vsync") {
            config.enable_vsync = false;
        } else if (arg.find("--msaa=") == 0) {
            int samples = std::stoi(arg.substr(7));
            config.msaa_samples = samples;
        } else if (arg == "--no-fallback") {
            config.allow_fallback = false;
        }
    }

    return config;
}

std::string get_backend_name(platform::GraphicsConfig::BackendType type) {
    switch (type) {
        case platform::GraphicsConfig::BackendType::Auto: return "Auto";
        case platform::GraphicsConfig::BackendType::OpenGL: return "OpenGL";
        case platform::GraphicsConfig::BackendType::Direct2D: return "Direct2D";
        case platform::GraphicsConfig::BackendType::Software: return "Software";
    }
    return "Unknown";
}

int main(int argc, char* argv[]) {
    // Initialize logging
    logging::init();
    logging::set_level(LogLevel::Info);

    LITHIUM_LOG_INFO("Lithium Browser v0.1.0");

    // Check for help flag
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    // Check for list backends flag
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--list-backends") {
            std::cout << "Available graphics backends:\n";

            auto backends = platform::GraphicsBackendFactory::available_backends();
            for (const auto& backend : backends) {
                std::string name = get_backend_name(backend);
                std::cout << "  - " << name;

                // Query capabilities
                auto caps_result = platform::GraphicsBackendFactory::query_capabilities(backend);
                if (caps_result.is_ok()) {
                    auto& caps = caps_result.value();
                    std::cout << " (" << caps.backend_name;
                    if (caps.hardware_accelerated) {
                        std::cout << ", Hardware Accelerated";
                    }
                    std::cout << ")\n";
                } else {
                    std::cout << "\n";
                }
            }

            return 0;
        }
    }

    // Parse graphics configuration
    auto graphics_config = parse_graphics_config(argc, argv);

    // Initialize platform
    if (!platform::platform::init()) {
        LITHIUM_LOG_ERROR("Failed to initialize platform");
        return 1;
    }

    // Create window
    platform::WindowConfig window_config;
    window_config.title = "Lithium Browser";
    window_config.width = 1280;
    window_config.height = 720;

    auto window = platform::Window::create(window_config);
    if (!window) {
        LITHIUM_LOG_ERROR("Failed to create window");
        platform::platform::shutdown();
        return 1;
    }

    // Create graphics context with backend selection
    LITHIUM_LOG_INFO("Creating graphics context with backend: {}",
                     get_backend_name(graphics_config.preferred_backend));

    auto graphics = platform::GraphicsContext::create(window.get(), graphics_config);
    if (!graphics) {
        LITHIUM_LOG_ERROR("Failed to create graphics context");
        platform::platform::shutdown();
        return 1;
    }

    // Query and display backend capabilities
    auto caps_result = platform::GraphicsBackendFactory::query_capabilities(
        graphics_config.preferred_backend
    );

    if (caps_result.is_ok()) {
        auto& caps = caps_result.value();
        LITHIUM_LOG_INFO("Graphics Backend: {} {}", caps.backend_name, caps.version_string);
        LITHIUM_LOG_INFO("Renderer: {}", caps.renderer_name);
        LITHIUM_LOG_INFO("Vendor: {}", caps.vendor_name);
        LITHIUM_LOG_INFO("Hardware Accelerated: {}", caps.hardware_accelerated ? "Yes" : "No");
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
    // URL is the first argument that's not an option (doesn't start with --)
    String initial_url = "about:blank";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--") != 0) {
            // This is not an option, treat it as URL
            initial_url = arg.c_str();
            break;
        }
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
