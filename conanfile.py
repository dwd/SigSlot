from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, CMake
from conan.tools.files import copy

class Siglot(ConanFile):
    name = "st-sigslot"
    license = "MIT"
    author = "Dave Cridland <dave@cridland.net>"
    url = "https://github.com/dwd/SigSlot"
    description = "A simple header-only Signal/Slot C++ library"
    topics = ("signal", "slot")
    exports_sources = "sigslot/*", "CMakeLists.txt", "test/*"
    no_copy_source = True
    options = {
        "tests": [True, False]
    }
    default_options = {
        "tests": False
    }
    settings = "os", "compiler", "build_type", "arch"

    def configure(self):
        if not self.options.get_safe("tests"):
            self.settings.clear()
        else:
            self.options["sentry-native"].backend = "inproc"

    def requirements(self):
        if self.options.get_safe("tests"):
            self.requires("gtest/1.12.1")
            self.requires("sentry-native/0.7.15")

    def layout(self):
        self.folders.source = "."

    def generate(self):
        if self.options.get_safe("tests"):
            deps = CMakeDeps(self)
            deps.generate()
            cmake = CMakeToolchain(self)
            cmake.generate()

    def build(self):
        if self.options.get_safe("tests"):
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def package(self):
        copy(self, "*.h", self.source_folder, self.package_folder + '/include')

    def package_info(self):
        self.cpp_info.includedirs = ["include"]
