# Modbus Master/Slave Library in C

## Overview

The **Modbus Master/Slave Library in C** is an efficient and lightweight library that provides robust implementations for both Master (Client) and Slave (Server) functionalities of the Modbus protocol. Designed specifically for embedded systems with limited resources, this library ensures reliable communication without the need for dynamic memory allocation.

## Key Features

- **Comprehensive Master and Slave Support**
  - Seamlessly implement both Master (Client) and Slave (Server) roles of the Modbus protocol.
  
- **Robust State Management with FSM**
  - Utilize a Finite State Machine (FSM) to efficiently manage communication states, ensuring reliable and predictable behavior.
  
- **Easy Integration with Custom Callbacks**
  - Integrate custom read and write callbacks to handle Modbus registers, allowing for flexible and customizable data management.
  
- **No Dynamic Memory Allocation**
  - Designed for resource-constrained environments by avoiding dynamic memory usage, ensuring consistent performance and reducing memory footprint.
  
- **Advanced Error Handling and CRC Verification**
  - Implements thorough error detection mechanisms, including Cyclic Redundancy Check (CRC) validations, to ensure data integrity and reliable communication.
  
- **Device Information Support (Function Code 0x2B)**
  - Supports reading detailed device information, enabling comprehensive metadata retrieval for better device management and monitoring.
  
- **Static Resource Allocation**
  - All memory and resources are statically allocated, providing predictable performance and simplifying memory management in embedded applications.
  
- **Modular and Extensible Design**
  - The library's modular architecture allows for easy extension and customization, facilitating integration into a wide range of projects and applications.
  
- **Comprehensive Documentation and Examples**
  - Detailed documentation and practical examples are provided to assist developers in quickly integrating and utilizing the library in their projects.
- **Pluggable Transport Abstraction (Gate 3)**
  - A lightweight `mb_transport_if_t` interface decouples the Modbus core from the physical link, enabling UART, TCP, or custom back-ends with consistent timeout handling.
- **Reusable RTU Framing Helpers (Gate 3)**
  - Shared encode/decode logic via `mb_frame_rtu_encode`/`mb_frame_rtu_decode` keeps CRC enforcement and bounds checks centralized for both master and slave stacks.

## Build-time configuration

Key behaviours can be toggled through preprocessor definitions so each target
selects the right balance between visibility and footprint.  Add the desired
options via your build system (for example `target_compile_definitions` in
CMake):

| Macro | Description |
| --- | --- |
| `MB_LOG_ENABLE_STDIO` | Enable the default stdio logging sink (default `1`). |
| `MB_LOG_ENABLE_SEGGER_RTT` | Enable the SEGGER RTT sink when the SDK is available (default `0`). |
| `MB_LOG_DEFAULT_THRESHOLD` | Set the initial log severity threshold (default `MB_LOG_INFO_LEVEL`). |
| `MB_LOG_STDOUT_SYNC_FLUSH` | Flush the stdio stream after each message (default `1`). |
| `MB_LOG_RTT_CHANNEL` | Configure the RTT down-channel used for logs (default `0`). |
| `MODBUS_BUILD_FUZZERS` | Build libFuzzer harnesses (requires Clang with `-fsanitize=fuzzer`). |
| `MODBUS_ENABLE_COVERAGE` | Instrument builds with GCOV-style coverage (GCC/Clang). |

Example: enable RTT logging while keeping stdout silent on a release profile:

```cmake
target_compile_definitions(my_app PRIVATE
    MB_LOG_ENABLE_STDIO=0
    MB_LOG_ENABLE_SEGGER_RTT=1
    MB_LOG_DEFAULT_THRESHOLD=MB_LOG_WARNING_LEVEL)
```

To exercise the Gate 2 fuzz harness locally:

```bash
cmake -S . -B build/fuzz -DMODBUS_BUILD_FUZZERS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/fuzz --target modbus_pdu_fuzz
```

Generate a coverage report (requires `lcov`/`genhtml`):

```bash
cmake -S . -B build/coverage -DMODBUS_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/coverage
cmake --build build/coverage --target coverage
```

## Utilities (Gate 1)

Gate 1 introduces reusable, heap-free utilities that underpin future transport
and FSM work:

- **Single-producer/single-consumer ring buffer** – `modbus/ringbuf.h` offers a
  lock-free FIFO ideal for UART RX pipelines.  It requires a power-of-two
  capacity and uses caller-provided storage.
- **Fixed-size memory pool** – `modbus/mempool.h` supplies deterministic block
  allocation without `malloc`, supporting reset, containment checks and
  double-free detection.
- **PDU codec helpers** – `modbus/pdu.h` encodes/decodes the core register
  function codes (0x03/0x06/0x10) with strict bounds checking.
- **CRC helpers** – table-driven and bitwise implementations live in
  `modbus/utils.h` with accompanying tests.
- **Logging façade** – `modbus/mb_log.h` standardises the logging interface and
  optional sinks, keeping legacy `LOG_*` usage isolated.

Each helper ships with targeted GoogleTests so behaviour remains stable across
toolchains.  See `tests/test_ringbuf.cpp` and `tests/test_mempool.cpp` for usage
examples.

## Installation

### Prerequisites

Ensure you have the following installed on your system:

- **C Compiler**: Compatible with the C99 standard or later (e.g., GCC, Clang).
- **Python 3**: Required for generating documentation with Sphinx and Breathe.
- **Doxygen**: Used to generate documentation from the source code.
- **Sphinx**: A powerful documentation generator.
- **Breathe**: An extension that integrates Doxygen-generated XML with Sphinx.
- **CMake**: Build system generator.

### Step-by-Step Guide

1. **Clone the Repository**

    ```bash
    git clone https://github.com/yourusername/modbus-library.git
    cd modbus-library
    ```

2. **Set Up the Python Environment**

    It's recommended to use a Python virtual environment to manage dependencies:

    ```bash
    python3 -m venv venv
    source venv/bin/activate  # On Windows, use `venv\Scripts\activate`
    pip install --upgrade pip
    pip install sphinx breathe
    ```

3. **Install Doxygen**

    Ensure that Doxygen is installed on your system.

    - **Ubuntu/Debian:**

      ```bash
      sudo apt-get update
      sudo apt-get install doxygen
      ```

    - **macOS (using Homebrew):**

      ```bash
      brew install doxygen
      ```

    - **Windows:**

      Download the installer from the [official Doxygen website](http://www.doxygen.nl/download.html) and follow the installation instructions.

4. **Configure and Build the Project with CMake**

    The project uses CMake with presets to simplify the build process.

    ```bash
    cmake --preset release
    cmake --build --preset release
    ```

5. **Generate the Documentation**

    ```bash
    cmake --build . --preset doc
    ```

    The documentation will be generated in the `doc/build/html` directory.

6. **Build and run slave rtu example**
   Change the COM port on the code (line 59 on main.c) to use the correct one, before build!!!

  ```bash
    cmake --build . --preset ex_slave_uart
    .\build\debug\examples\win\server\uart\slave_uart_example.exe
  ```

7. **Build and run master rtu example**
  Change the COM port on the code (line 85 on main.c) to use the correct one, before build!!!
  ```bash
    cmake --build . --preset ex_master_uart
    .\build\debug\examples\win\server\uart\master_uart_example.exe
  ```

## Usage

For detailed instructions on initializing and configuring both Master and Slave functionalities, refer to the [Usage Documentation](doc/build/html/usage.html).

## Examples

Explore practical examples to understand how to implement Modbus Master and Slave functionalities:

- [Slave Example](doc/build/html/examples/slave_example.html)
- [Master Example](doc/build/html/examples/master_example.html)

## API Reference

Comprehensive details about all the functions, structures, and enumerations available in the library can be found in the [API Reference](doc/build/html/api_reference.html).

## Documentation

Full documentation is available [here](doc/build/html/index.html). It includes detailed guides on installation, usage, API reference, and examples.

## Contributing

Contributions are welcome! Please refer to the [Contributing Guide](doc/build/html/contributing.html) for detailed instructions on how to contribute to this project.

## License

## Proprietary License

The **Modbus Master/Slave Library in C** is proprietary software developed and owned by **Embraco**. All rights, including intellectual property rights, are reserved by Embraco.

You are granted a non-exclusive, non-transferable license to use, modify, and distribute this library solely for your internal projects and purposes. Any other use, including but not limited to commercial distribution, sublicensing, or sharing with third parties, is strictly prohibited without prior written consent from Embraco.

## Contact

For permissions beyond the scope of this license or to request authorization for specific uses, please contact:

**Embraco Licensing Team**

- **Email:** [licensing@embraco.com](mailto:licensing@embraco.com)
- **Website:** [www.embraco.com](https://www.embraco.com)

## Author

**Luiz Carlos Gili**

Luiz Carlos Gili is a seasoned software developer specializing in embedded systems and communication protocols. With a strong background in C programming and a passion for creating efficient and reliable software solutions, Luiz has contributed to numerous projects in the industrial and automation sectors.

- **Email**: [your.email@example.com](mailto:your.email@example.com)
- **GitHub**: [yourusername](https://github.com/yourusername)
- **LinkedIn**: [Your LinkedIn Profile](https://www.linkedin.com/in/yourprofile)

---
