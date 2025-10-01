Examples
========

This section provides practical examples demonstrating how to use the **Modbus Master/Slave Library in C** to implement both Master and Slave functionalities of the Modbus protocol. These examples are designed to help you quickly integrate and understand the library's capabilities in real-world scenarios.

.. toctree::
   :maxdepth: 2
   :caption: Example Contents

   slave_example
   master_example

## Overview

The examples are divided into two main categories:

- **Slave Example (`slave_example.rst`):** Demonstrates how to set up and run a Modbus Slave (Server) using the library. It covers initializing the slave context, configuring holding registers, and handling incoming requests.

- **Master Example (`master_example.rst`):** Shows how to configure and operate a Modbus Master (Client). It includes initializing the master context, sending read/write requests to the slave, and processing responses.

## How to Run the Examples

Follow these steps to compile and execute the examples:

1. **Build the Project with Examples:**

   Ensure that you have followed the [Installation](installation.rst) steps to set up the project and generate the necessary build files.

   ```bash
   cmake --preset release
   cmake --build --preset release

2. **Run the Example:**
    ```bash
    cd examples

Ensure that the slave is running before starting the master to establish successful communication.

## Example Descriptions
### Slave Example

The Slave Example sets up a Modbus Slave device with predefined holding registers. It listens for incoming requests from a Modbus Master and responds accordingly. This example demonstrates the initialization and configuration process for a Modbus Slave, including setting up transport functions and handling client requests.

### Master Example

The Master Example initializes a Modbus Master device that sends read requests to the Slave Example. It showcases how to configure the Master, send requests, handle responses, and process the received data. This example is useful for understanding how to implement Master-side operations using the library.