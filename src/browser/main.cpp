/**
 * Lithium Browser - A lightweight browser engine
 *
 * This is the main entry point for the Lithium browser application.
 */

#include "lithium/browser/engine.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/mica/mica.hpp"
#include "lithium/core/logger.hpp"
#include <iostream>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <thread>
#include <chrono>

using namespace lithium;

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] [URL]\n"
              << "\n"
              << "Options:\n"
              << "  --backend=TYPE    Graphics backend to use\n"
              << "                    Available: auto, software, direct2d, opengl\n"
              << "                    Default: auto\n"
              << "  --no-vsync        Disable vertical synchronization\n"
              << "  --msaa=N          Enable MSAA with N samples (2, 4, 8)\n"
              << "  --list-backends   List available graphics backends\n"
              << "  --help            Show this help message\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " --backend=direct2d https://example.com\n"
              << "  " << program_name << " --backend=opengl --no-vsync\n"
              << "  " << program_name << " --list-backends\n";
}

mica::BackendType parse_backend_type(const std::string& backend_str) {
    if (backend_str == "auto" || backend_str == "Auto") {
        return mica::BackendType::Auto;
    } else if (backend_str == "software") {
        return mica::BackendType::Software;
    } else if (backend_str == "direct2d") {
        return mica::BackendType::Direct2D;
    } else if (backend_str == "opengl") {
        return mica::BackendType::OpenGL;
    }
    return mica::BackendType::Auto;
}

std::string get_backend_name(mica::BackendType type) {
    switch (type) {
        case mica::BackendType::Auto: return "Auto";
        case mica::BackendType::Software: return "Software";
        case mica::BackendType::Direct2D: return "Direct2D";
        case mica::BackendType::OpenGL: return "OpenGL";
    }
    return "Unknown";
}

int main(int argc, char* argv[])
{
    try
    {
        // Initialize logging
        logging::init();
        logging::set_level(LogLevel::Info);

        LITHIUM_LOG_INFO("Lithium Browser v0.1.0");
        LITHIUM_LOG_INFO("Starting browser initialization...");

        // Parse command line arguments
        mica::BackendType backend_type = mica::BackendType::Auto;
        String initial_url = "about:blank";
        bool show_help = false;
        bool list_backends = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help") {
                show_help = true;
            } else if (arg == "--list-backends") {
                list_backends = true;
            } else if (arg.find("--backend=") == 0) {
                std::string backend_str = arg.substr(10);
                backend_type = parse_backend_type(backend_str);
            } else if (arg == "--no-vsync") {
                // TODO: Handle vsync in mica
            } else if (arg.find("--msaa=") == 0) {
                // TODO: Handle MSAA in mica
            } else if (arg.find("--") != 0) {
                // This is not an option, treat it as URL
                initial_url = arg.c_str();
                break;
            }
        }

        if (show_help) {
            print_usage(argv[0]);
            logging::shutdown();
            return 0;
        }

        if (list_backends) {
            std::cout << "Available graphics backends:\n";
            std::cout << "  - Auto: Automatically detect best backend\n";
            std::cout << "  - Software: CPU software rendering (always available)\n";

            #ifdef _WIN32
            std::cout << "  - Direct2D: Hardware-accelerated (Windows)\n";
            #endif

            #if defined(__linux__) || defined(__ANDROID__)
            std::cout << "  - OpenGL: Hardware-accelerated (Linux/Android)\n";
            #endif

            #ifdef __APPLE__
            std::cout << "  - OpenGL: Hardware-accelerated (macOS/iOS)\n";
            #endif

            logging::shutdown();
            return 0;
        }

        // Initialize platform subsystem
        LITHIUM_LOG_INFO("Initializing platform subsystem...");
        if (!platform::platform::init()) {
            LITHIUM_LOG_ERROR("Failed to initialize platform subsystem");
            logging::shutdown();
            return 1;
        }
        LITHIUM_LOG_INFO("Platform initialized successfully");

        // Create window
        LITHIUM_LOG_INFO("Creating window...");
        platform::WindowConfig window_config;
        window_config.title = "Lithium Browser";
        window_config.width = 1280;
        window_config.height = 720;

        auto window = platform::Window::create(window_config);
        if (!window) {
            LITHIUM_LOG_ERROR("Failed to create window");
            platform::platform::shutdown();
            logging::shutdown();
            return 1;
        }

        LITHIUM_LOG_INFO("Window created successfully");
        LITHIUM_LOG_INFO_FMT("Window size: {}x{}", window->size().width, window->size().height);

        // Ensure window is shown
        if (!window->is_visible()) {
            window->show();
        }

        // Initialize mica graphics engine
        std::string backend_name = get_backend_name(backend_type);
        LITHIUM_LOG_INFO_FMT("Initializing mica graphics engine with backend: {}", backend_name.c_str());

        auto graphics_engine = std::make_unique<mica::Engine>();
        if (!graphics_engine->initialize(backend_type)) {
            LITHIUM_LOG_ERROR("Failed to initialize mica graphics engine");
            window->set_should_close(true);
        } else {
            LITHIUM_LOG_INFO("Mica graphics engine initialized successfully");

            // Query and display backend capabilities
            auto caps = graphics_engine->capabilities();
            LITHIUM_LOG_INFO_FMT("Hardware Accelerated: {}", caps.supports_multisampling ? "Yes" : "No");
        }

        // Create graphics context from native window handle
        std::unique_ptr<mica::Context> graphics_context;
        std::unique_ptr<mica::Painter> painter;

        if (!window->should_close()) {
            void* native_window = window->native_handle();
            graphics_context = graphics_engine->create_context(native_window);
            if (!graphics_context) {
                LITHIUM_LOG_ERROR("Failed to create graphics context");
                window->set_should_close(true);
            } else {
                LITHIUM_LOG_INFO("Graphics context created successfully");

                // Create painter for drawing
                painter = graphics_engine->create_painter(*graphics_context);
                if (!painter) {
                    LITHIUM_LOG_ERROR("Failed to create painter");
                    window->set_should_close(true);
                } else {
                    LITHIUM_LOG_INFO("Painter created successfully");
                }
            }
        }

        // Create browser engine
        LITHIUM_LOG_INFO("Creating browser engine...");
        browser::Engine engine;
        if (!engine.init()) {
            LITHIUM_LOG_ERROR("Failed to initialize browser engine");
            window->set_should_close(true);
        } else {
            LITHIUM_LOG_INFO("Browser engine initialized successfully");

            // Set graphics context and painter in engine
            if (graphics_context && painter) {
                engine.set_graphics_context(std::move(graphics_context), std::move(painter));
                LITHIUM_LOG_INFO("Graphics context and painter passed to engine");
            } else {
                LITHIUM_LOG_WARN("Graphics context or painter not available, rendering will be disabled");
            }

            // Set up callbacks
            engine.set_title_changed_callback([&window](const String& title) {
                String full_title = String("Lithium - ") + title.c_str();
                window->set_title(full_title);
            });

            // Load initial page
            LITHIUM_LOG_INFO_FMT("Loading page: {}", initial_url.c_str());

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
        .feature-list {
            margin: 20px 0;
            padding-left: 20px;
        }
        .feature-list li {
            margin: 5px 0;
        }
    </style>
</head>
<body>
    <h1>Welcome to Lithium Browser</h1>
    <p>Lithium is a lightweight browser engine implemented from scratch in C++.</p>
    <p>Features:</p>
    <ul class="feature-list">
        <li>HTML5 parsing</li>
        <li>CSS styling</li>
        <li>JavaScript execution</li>
        <li>Layout and rendering with Mica graphics engine</li>
        <li>Text rendering with Beryl text engine</li>
        <li>Multiple graphics backends (Direct2D, OpenGL, Software)</li>
    </ul>
</body>
</html>
                )");
            } else {
                engine.load_url(initial_url);
            }
        }

        // Handle window resize
        auto size = window->size();
        engine.resize(size.width, size.height);
        LITHIUM_LOG_INFO_FMT("Engine resized to: {}x{}", size.width, size.height);

        // Event callback
        window->set_event_callback([&engine, &window](const platform::Event& event) {
            platform::EventDispatcher dispatcher(event);

            dispatcher.dispatch<platform::WindowCloseEvent>([&window](const auto&) {
                LITHIUM_LOG_INFO("Window close event received");
                window->set_should_close(true);
                return true;
            });

            dispatcher.dispatch<platform::WindowResizeEvent>([&engine](const auto& e) {
                LITHIUM_LOG_INFO_FMT("Window resize event: {}x{}", e.width, e.height);
                engine.resize(e.width, e.height);
                return true;
            });

            dispatcher.dispatch<platform::KeyEvent>([&engine, &event](const auto& e) {
                engine.handle_event(event);
                return true;
            });

            dispatcher.dispatch<platform::MouseButtonEvent>([&engine, &event](const auto& e) {
                engine.handle_event(event);
                return true;
            });
        });

        // Main loop
        LITHIUM_LOG_INFO("Starting main loop...");
        std::cout << "Starting main loop..." << std::endl;
        int frame_count = 0;
        while (!window->should_close()) {
            // Poll events
            window->poll_events();

            // Process pending tasks
            engine.process_tasks();

            // Render frame
            engine.render();

            // Debug output every 60 frames
            if (++frame_count % 60 == 0) {
                LITHIUM_LOG_INFO_FMT("Frame: {}", frame_count);
                std::cout << "Frame: " << frame_count << std::endl;
            }

            // Small delay to reduce CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        LITHIUM_LOG_INFO_FMT("Main loop ended. Frames rendered: {}", frame_count);
        std::cout << "Main loop ended. Frames rendered: " << frame_count << std::endl;

        // Cleanup
        LITHIUM_LOG_INFO("Cleaning up resources...");
        try {
            LITHIUM_LOG_INFO("Shutting down logging system...");
            logging::shutdown();
            LITHIUM_LOG_INFO("Logging system shut down successfully");

            LITHIUM_LOG_INFO("Shutting down platform subsystem...");
            platform::platform::shutdown();
            LITHIUM_LOG_INFO("Platform subsystem shut down successfully");

            std::cout << "Cleanup completed successfully" << std::endl;
            return 0;
        }
        catch (const std::exception& e) {
            LITHIUM_LOG_FATAL_FMT("Fatal error during cleanup: {}", e.what());
            std::cerr << "Fatal error during cleanup: " << e.what() << std::endl;

            // Try to continue cleanup
            try {
                platform::platform::shutdown();
                logging::shutdown();
            }
            catch (...) {
                // Ignore cleanup errors during exception handling
            }

            return 1;
        }
        catch (...) {
            LITHIUM_LOG_FATAL("Unknown fatal error occurred");
            std::cerr << "Unknown fatal error occurred" << std::endl;

            // Try to cleanup
            try {
                platform::platform::shutdown();
                logging::shutdown();
            }
            catch (...) {
                // Ignore cleanup errors during exception handling
            }

            return 1;
        }
    }
    catch (const std::exception& e) {
        LITHIUM_LOG_FATAL_FMT("Fatal error: {}", e.what());
        std::cerr << "Fatal error: " << e.what() << std::endl;

        // Try to cleanup
        try {
            logging::shutdown();
            platform::platform::shutdown();
        }
        catch (...) {
            // Ignore cleanup errors during exception handling
        }

        return 1;
    }
    catch (...) {
        LITHIUM_LOG_FATAL("Unknown fatal error occurred");
        std::cerr << "Unknown fatal error occurred" << std::endl;

        // Try to cleanup
        try {
            logging::shutdown();
            platform::platform::shutdown();
        }
        catch (...) {
            // Ignore cleanup errors during exception handling
        }

        return 1;
    }
}
