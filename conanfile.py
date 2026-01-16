from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMakeDeps


class LithiumRecipe(ConanFile):
    name = "lithium"
    version = "0.1.0"
    description = "A lightweight browser engine implemented from scratch"
    license = "MIT"

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "with_freetype": [True, False],
        "with_curl": [True, False],
    }
    default_options = {
        "shared": False,
        "with_freetype": True,
        "with_curl": True,
    }

    def requirements(self):
        # Font loading (optional)
        if self.options.with_freetype:
            self.requires("freetype/2.13.2")

        # HTTP client (optional)
        if self.options.with_curl:
            self.requires("libcurl/8.4.0")

    def build_requirements(self):
        # Testing framework
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()
