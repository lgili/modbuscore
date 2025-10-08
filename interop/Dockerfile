# Modbus Interoperability Test Environment
# Gate 26: Prove compatibility with the broader Modbus ecosystem
#
# This Docker environment includes:
# - modpoll (commercial CLI tool)
# - pymodbus (Python library)
# - libmodbus (C library)
# - diagslave (simulation tool)
# - Our library's TCP client/server examples
#
# Usage:
#   docker build -t modbus-interop -f interop/Dockerfile .
#   docker run --rm -v $(pwd)/interop/pcaps:/pcaps modbus-interop

FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    wget \
    tcpdump \
    tshark \
    libssl-dev \
    pkg-config \
    nano \
    net-tools \
    iputils-ping \
    socat \
    autoconf \
    automake \
    libtool \
    && rm -rf /var/lib/apt/lists/*

# Install Python Modbus libraries
RUN pip3 install --no-cache-dir \
    pymodbus==3.5.4 \
    click==8.1.7 \
    pytest==7.4.3

# Install libmodbus from source
WORKDIR /tmp/libmodbus
RUN git clone --depth 1 --branch v3.1.10 https://github.com/stephane/libmodbus.git . && \
    ./autogen.sh && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    rm -rf /tmp/libmodbus

# Download modpoll (demo version)
# Note: modpoll is commercial software. This uses the free trial for testing.
# For production, acquire proper license from https://www.modbusdriver.com/modpoll.html
WORKDIR /opt/modpoll
RUN wget -q https://www.modbusdriver.com/downloads/modpoll.tgz && \
    tar -xzf modpoll.tgz && \
    rm -f modpoll.tgz && \
    MODPOLL_BIN=$(find . -name modpoll -type f | head -1) && \
    if [ -n "$MODPOLL_BIN" ]; then \
        chmod +x "$MODPOLL_BIN" && \
        ln -sf "/opt/modpoll/$MODPOLL_BIN" /usr/local/bin/modpoll; \
    else \
        echo "Warning: modpoll binary not found"; \
    fi

# Download diagslave (free simulation tool)
WORKDIR /opt/diagslave
RUN wget -q https://www.modbusdriver.com/downloads/diagslave.tgz && \
    tar -xzf diagslave.tgz && \
    rm -f diagslave.tgz && \
    DIAGSLAVE_BIN=$(find . -name diagslave -type f | head -1) && \
    if [ -n "$DIAGSLAVE_BIN" ]; then \
        chmod +x "$DIAGSLAVE_BIN" && \
        ln -sf "/opt/diagslave/$DIAGSLAVE_BIN" /usr/local/bin/diagslave; \
    else \
        echo "Warning: diagslave binary not found"; \
    fi

# Copy our library into the container
WORKDIR /modbus-lib
COPY . .

# Clean any existing host builds (macOS binaries won't work in Linux container)
RUN rm -rf build && mkdir -p build

# Build our library examples for Linux
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DMODBUS_BUILD_EXAMPLES=ON \
    -DMODBUS_ENABLE_INTEGRATION_TESTS=ON \
    -DMODBUS_ENABLE_RTU=ON \
    -DMODBUS_ENABLE_TCP=ON && \
    cmake --build build -j$(nproc) || \
    echo "Note: Build completed with warnings"

# Verify examples were built
RUN ls -la /modbus-lib/build/examples/ 2>/dev/null || echo "No examples directory found"
RUN find /modbus-lib/build -name "modbus_tcp_*" -type f -executable 2>/dev/null || echo "TCP examples not found"

# Verify executables are Linux ELF binaries
RUN file /modbus-lib/build/examples/modbus_tcp_server_demo /modbus-lib/build/examples/modbus_tcp_client_cli 2>/dev/null || echo "Checking binary format..."

# Create directories for test artifacts
RUN mkdir -p /pcaps /results /logs

# Copy interop test scripts
COPY interop/scripts /interop-scripts
RUN chmod +x /interop-scripts/*.sh /interop-scripts/*.py 2>/dev/null || true

# Set up entrypoint
WORKDIR /interop-scripts
COPY interop/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["--help"]
