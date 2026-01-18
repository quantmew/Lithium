/**
 * Unit Tests for Hardware-Accelerated Graphics Backends
 *
 * This file contains comprehensive tests for the graphics backend system,
 * including backend factory, configuration, and basic rendering tests.
 */

#include "lithium/platform/graphics_config.hpp"
#include "lithium/platform/graphics_backend.hpp"
#include "lithium/platform/graphics_context.hpp"
#include "lithium/platform/window.hpp"
#include "lithium/core/logger.hpp"
#include <gtest/gtest.h>

using namespace lithium;

// ============================================================================
// Backend Factory Tests
// ============================================================================

TEST(BackendFactoryTest, AvailableBackends) {
    auto backends = platform::GraphicsBackendFactory::available_backends();

    // Software backend should always be available
    bool has_software = false;
    for (const auto& backend : backends) {
        if (backend == platform::GraphicsConfig::BackendType::Software) {
            has_software = true;
            break;
        }
    }
    EXPECT_TRUE(has_software);
}

TEST(BackendFactoryTest, DefaultBackend) {
    auto backend = platform::GraphicsBackendFactory::default_backend();

    // Default backend should be valid
    bool is_valid =
        backend == platform::GraphicsConfig::BackendType::Auto ||
        backend == platform::GraphicsConfig::BackendType::OpenGL ||
        backend == platform::GraphicsConfig::BackendType::Direct2D ||
        backend == platform::GraphicsConfig::BackendType::Software;
    EXPECT_TRUE(is_valid);
}

TEST(BackendFactoryTest, QuerySoftwareCapabilities) {
    auto result = platform::GraphicsBackendFactory::query_capabilities(
        platform::GraphicsConfig::BackendType::Software
    );

    ASSERT_TRUE(result.is_ok());
    auto& caps = result.value();

    EXPECT_FALSE(caps.hardware_accelerated);
    EXPECT_EQ(caps.backend_name, "Software");
    EXPECT_TRUE(caps.max_texture_size > 0);
}

TEST(BackendFactoryTest, QueryOpenGLCapabilities) {
    #ifdef LITHIUM_HAS_OPENGL
    auto result = platform::GraphicsBackendFactory::query_capabilities(
        platform::GraphicsConfig::BackendType::OpenGL
    );

    // OpenGL might not be available on all systems
    if (result.is_ok()) {
        auto& caps = result.value();
        EXPECT_EQ(caps.backend_name, "OpenGL");
        EXPECT_TRUE(caps.supports_shaders);
        EXPECT_TRUE(caps.max_texture_size > 0);
    }
    #endif
}

TEST(BackendFactoryTest, QueryDirect2DCapabilities) {
    #ifdef LITHIUM_HAS_DIRECT2D
    auto result = platform::GraphicsBackendFactory::query_capabilities(
        platform::GraphicsConfig::BackendType::Direct2D
    );

    // Direct2D is Windows-only
    if (result.is_ok()) {
        auto& caps = result.value();
        EXPECT_EQ(caps.backend_name, "Direct2D");
        EXPECT_TRUE(caps.hardware_accelerated);
    }
    #endif
}

// ============================================================================
// GraphicsConfig Tests
// ============================================================================

TEST(GraphicsConfigTest, DefaultValues) {
    platform::GraphicsConfig config;

    EXPECT_EQ(config.preferred_backend, platform::GraphicsConfig::BackendType::Auto);
    EXPECT_TRUE(config.enable_vsync);
    EXPECT_EQ(config.msaa_samples, 0);
    EXPECT_TRUE(config.allow_fallback);
    EXPECT_TRUE(config.enable_hardware_acceleration);
    EXPECT_EQ(config.min_opengl_version, 0x00030003); // OpenGL 3.3
    EXPECT_FALSE(config.enable_debug);
}

TEST(GraphicsConfigTest, BackendTypes) {
    platform::GraphicsConfig config;

    config.preferred_backend = platform::GraphicsConfig::BackendType::OpenGL;
    EXPECT_EQ(config.preferred_backend, platform::GraphicsConfig::BackendType::OpenGL);

    config.preferred_backend = platform::GraphicsConfig::BackendType::Direct2D;
    EXPECT_EQ(config.preferred_backend, platform::GraphicsConfig::BackendType::Direct2D);

    config.preferred_backend = platform::GraphicsConfig::BackendType::Software;
    EXPECT_EQ(config.preferred_backend, platform::GraphicsConfig::BackendType::Software);
}

TEST(GraphicsConfigTest, VSyncConfiguration) {
    platform::GraphicsConfig config;

    config.enable_vsync = false;
    EXPECT_FALSE(config.enable_vsync);

    config.enable_vsync = true;
    EXPECT_TRUE(config.enable_vsync);
}

TEST(GraphicsConfigTest, MSAAConfiguration) {
    platform::GraphicsConfig config;

    config.msaa_samples = 4;
    EXPECT_EQ(config.msaa_samples, 4);

    config.msaa_samples = 8;
    EXPECT_EQ(config.msaa_samples, 8);
}

// ============================================================================
// BackendError Tests
// ============================================================================

TEST(BackendErrorTest, StringConversion) {
    EXPECT_STREQ(to_string(platform::BackendError::None), "None");
    EXPECT_STREQ(to_string(platform::BackendError::InitializationFailed), "InitializationFailed");
    EXPECT_STREQ(to_string(platform::BackendError::NotSupported), "NotSupported");
    EXPECT_STREQ(to_string(platform::BackendError::OutOfMemory), "OutOfMemory");
    EXPECT_STREQ(to_string(platform::BackendError::InvalidConfig), "InvalidConfig");
    EXPECT_STREQ(to_string(platform::BackendError::DeviceLost), "DeviceLost");
    EXPECT_STREQ(to_string(platform::BackendError::CompilationFailed), "CompilationFailed");
    EXPECT_STREQ(to_string(platform::BackendError::LinkingFailed), "LinkingFailed");
    EXPECT_STREQ(to_string(platform::BackendError::Unknown), "Unknown");
}

// ============================================================================
// GraphicsCapabilities Tests
// ============================================================================

TEST(GraphicsCapabilitiesTest, DefaultConstruction) {
    platform::GraphicsCapabilities caps;

    EXPECT_EQ(caps.backend_name, "Unknown");
    EXPECT_EQ(caps.renderer_name, "Unknown");
    EXPECT_EQ(caps.vendor_name, "Unknown");
    EXPECT_EQ(caps.version_string, "0.0.0");
    EXPECT_FALSE(caps.hardware_accelerated);
    EXPECT_FALSE(caps.supports_vsync);
    EXPECT_FALSE(caps.supports_msaa);
    EXPECT_FALSE(caps.supports_shaders);
    EXPECT_EQ(caps.max_texture_size, 0);
    EXPECT_EQ(caps.max_texture_units, 0);
    EXPECT_EQ(caps.max_color_attachments, 0);
    EXPECT_EQ(caps.max_viewport_width, 0);
    EXPECT_EQ(caps.max_viewport_height, 0);
    EXPECT_EQ(caps.max_msaa_samples, 0);
    EXPECT_EQ(caps.max_anisotropy, 0.0f);
}

// ============================================================================
// Context Creation Tests
// ============================================================================

TEST(GraphicsContextTest, CreateWithoutInit) {
    // Creating a graphics context without platform init should fail gracefully
    // This test verifies error handling

    // Note: This test is informational - actual behavior depends on platform
    // In production, you would initialize the platform first

    // We'll skip actual context creation to avoid side effects
    // But we can verify the factory signature is correct
    platform::Window* null_window = nullptr;
    platform::GraphicsConfig config;
    auto result = platform::GraphicsBackendFactory::create(null_window, config);

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), platform::BackendError::InvalidConfig);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(IntegrationTest, BackendSelectionFallback) {
    platform::GraphicsConfig config;

    // Test auto backend selection
    config.preferred_backend = platform::GraphicsConfig::BackendType::Auto;
    config.allow_fallback = true;

    auto backends = platform::GraphicsBackendFactory::available_backends();
    EXPECT_GE(backends.size(), 1); // At least software should be available
}

TEST(IntegrationTest, BackendSelectionNoFallback) {
    platform::GraphicsConfig config;

    // Test that we can configure to prevent fallback
    config.preferred_backend = platform::GraphicsConfig::BackendType::OpenGL;
    config.allow_fallback = false;

    // Without a valid window, this should fail
    platform::Window* null_window = nullptr;
    auto result = platform::GraphicsBackendFactory::create(null_window, config);

    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(PerformanceTest, BackendQuerySpeed) {
    // Backend capability queries should be fast
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        platform::GraphicsBackendFactory::query_capabilities(
            platform::GraphicsConfig::BackendType::Software
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 queries in less than 100ms
    EXPECT_LT(duration.count(), 100);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    // Initialize logging for tests
    logging::init();
    logging::set_level(LogLevel::Warn); // Reduce noise during tests

    // Run GoogleTest
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    // Cleanup (ignore errors from shutdown)
    try {
        logging::shutdown();
    } catch (...) {
        // Ignore shutdown errors
    }

    return result;
}
