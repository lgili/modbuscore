from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
import os


class ModbusCoreConan(ConanFile):
    name = "modbuscore"
    version = "1.0.0"
    license = "MIT"
    author = "Lucas Gili <lgili@example.com>"
    url = "https://github.com/lgili/modbuscore"
    description = "Next-generation Modbus runtime with dependency injection"
    topics = ("modbus", "iot", "industrial", "embedded", "automation")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_testing": [True, False],
        "with_fuzzing": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_testing": False,
        "with_fuzzing": False,
    }
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "src/*",
        "tests/*",
        "examples/*",
        "LICENSE",
        "README.md",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        # ModbusCore is a C library
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTING"] = self.options.with_testing
        tc.variables["ENABLE_FUZZING"] = self.options.with_fuzzing
        if self.options.shared:
            tc.variables["BUILD_SHARED_LIBS"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if self.options.with_testing:
            cmake.test()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["modbuscore"]
        self.cpp_info.set_property("cmake_file_name", "ModbusCore")
        self.cpp_info.set_property("cmake_target_name", "ModbusCore::modbuscore")
        self.cpp_info.set_property("pkg_config_name", "modbuscore")
        
        # Platform-specific libraries
        if self.settings.os == "Windows":
            self.cpp_info.system_libs.append("ws2_32")
        
        # Include directories
        self.cpp_info.includedirs = ["include"]
