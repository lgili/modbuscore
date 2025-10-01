Slave Example
=============

This example demonstrates how to set up and run a Modbus Slave (Server) using the **Modbus Master/Slave Library in C**. It includes initializing the slave context, configuring holding registers, and handling incoming requests.

```c
#include <modbus_slave.h>
#include <modbus_transport.h>
#include <modbus/utils.h>

// Define transport functions (e.g., UART)
void uart_write(uint8_t *data, uint16_t length) {
    // Implement UART write functionality
}

bool uart_read(uint8_t *data, uint16_t length) {
    // Implement UART read functionality
    return true;
}

uint16_t get_msec(void) {
    // Implement a function to get the current time in milliseconds
}

uint16_t measure_time(uint16_t start_time) {
    // Implement a function to measure elapsed time since start_time
    return get_msec() - start_time;
}

int main(void) {
    modbus_context_t ctx;
    modbus_transport_t transport = {
        .write = uart_write,
        .read = uart_read,
        .get_reference_msec = get_msec,
        .measure_time_msec = measure_time
    };
    
    uint16_t device_address = 1;
    uint16_t baudrate = 19200;
    
    // Initialize the Modbus Slave
    modbus_error_t error = modbus_server_create(&ctx, &transport, &device_address, &baudrate);
    if (error != MODBUS_ERROR_NONE) {
        // Handle initialization error
    }
    
    // Register holding registers
    int16_t reg1 = 100;
    int16_t reg2 = 200;
    modbus_set_holding_register(&ctx, 0x0000, &reg1, false, NULL, NULL);
    modbus_set_holding_register(&ctx, 0x0001, &reg2, false, NULL, NULL);
    
    // Add device information
    const char vendor_name[] = "ModbusMasterSlaveLibrary";
    modbus_server_add_device_info(&ctx, vendor_name, sizeof(vendor_name) - 1);
    
    // Main loop
    while (1) {
        modbus_server_poll(&ctx);
        // Other application tasks
    }
    
    return 0;
}


***************************************************************
Slave Example
=============
    
This example demonstrates how to set up and run a Modbus Slave (Server) on a Windows system using a COM port. It utilizes the **Modbus Master/Slave Library in C** to handle Modbus communication and the Windows API for serial port operations. The example includes comprehensive logging to provide real-time feedback on the slave's operations, aiding in understanding and debugging.

## Prerequisites

- **Windows Operating System**: This example is tailored for Windows environments.
- **COM Port**: Ensure that a COM port is available and properly configured on your system (e.g., COM3).
- **Modbus Master/Slave Library in C**: Ensure the library is correctly integrated into your project.
- **C Compiler**: Compatible with C99 or later (e.g., GCC via MinGW, Microsoft Visual Studio).

## Overview

The **Slave Example** sets up a Modbus Slave that listens on a specified COM port. It registers holding registers and handles incoming Modbus requests from a Master device. The example includes logging statements to display the slave's status, incoming requests, and responses, providing clear visibility into the communication process.

## Implementation Details

### 1. Setting Up the Serial COM Port

The example uses the Windows API to configure and manage the serial COM port. Functions such as `CreateFile`, `SetCommState`, `ReadFile`, and `WriteFile` are employed to handle serial communication.

### 2. Defining Transport Functions

The Modbus library requires defining transport functions for reading from and writing to the communication medium, as well as functions to obtain the current time and measure elapsed time.

- **Write Function (`serial_write`)**: Sends data over the COM port.
- **Read Function (`serial_read`)**: Receives data from the COM port.
- **Get Time Function (`get_current_time_ms`)**: Retrieves the current system time in milliseconds.
- **Measure Time Function (`measure_elapsed_time`)**: Calculates the time elapsed since a given start time.

### 3. Initializing the Modbus Slave

The Modbus Slave is initialized with the transport configurations, device address, and baud rate. Holding registers are registered, and device information is added to support Function Code 0x2B for device information queries.

### 4. Logging

Logging is implemented using `printf` statements to provide real-time feedback on:

- Successful initialization of the COM port and Modbus Slave.
- Incoming Modbus requests and their processing.
- Responses sent back to the Master device.
- Errors encountered during communication.

## Example Code

Below is the complete C code for the Modbus Slave example on Windows using a COM port:

```c
// slave_example.c

#include <stdio.h>
#include <windows.h>
#include <stdint.h>
#include <time.h>
#include "modbus_slave.h"
#include "modbus_transport.h"

// Define the COM port (e.g., "COM3")
#define COM_PORT "COM3"

// Function to write data to the COM port
int serial_write(modbus_transport_t *transport, uint8_t *data, uint16_t length) {
    HANDLE hSerial = (HANDLE)transport->user_data;
    DWORD bytesWritten;

    BOOL status = WriteFile(hSerial, data, length, &bytesWritten, NULL);
    if (!status) {
        printf("[Error] Failed to write to COM port.\n");
        return -1;
    }

    printf("[Info] Sent %d bytes to COM port.\n", bytesWritten);
    return bytesWritten;
}

// Function to read data from the COM port
int serial_read(modbus_transport_t *transport, uint8_t *buffer, uint16_t length) {
    HANDLE hSerial = (HANDLE)transport->user_data;
    DWORD bytesRead;

    BOOL status = ReadFile(hSerial, buffer, length, &bytesRead, NULL);
    if (!status) {
        printf("[Error] Failed to read from COM port.\n");
        return -1;
    }

    if (bytesRead > 0) {
        printf("[Info] Received %d bytes from COM port.\n", bytesRead);
    }

    return bytesRead;
}

// Function to get the current time in milliseconds
uint16_t get_current_time_ms(void) {
    return (uint16_t)(GetTickCount() & 0xFFFF);
}

// Function to measure elapsed time since start_time
uint16_t measure_elapsed_time(uint16_t start_time) {
    uint16_t current_time = get_current_time_ms();
    return (current_time - start_time) & 0xFFFF;
}

int main(void) {
    // Initialize logging
    printf("Initializing Modbus Slave on %s...\n", COM_PORT);

    // Open the COM port
    HANDLE hSerial = CreateFile(
        COM_PORT,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("[Error] Unable to open COM port %s.\n", COM_PORT);
        return 1;
    }

    // Configure the serial port
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        printf("[Error] Failed to get current COM state.\n");
        CloseHandle(hSerial);
        return 1;
    }

    dcbSerialParams.BaudRate = CBR_19200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        printf("[Error] Failed to set COM parameters.\n");
        CloseHandle(hSerial);
        return 1;
    }

    // Set timeouts (optional)
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 50;
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        printf("[Error] Failed to set COM timeouts.\n");
        CloseHandle(hSerial);
        return 1;
    }

    printf("[Info] COM port %s opened and configured successfully.\n", COM_PORT);

    // Define transport with user_data as the COM port handle
    modbus_transport_t transport = {
        .write = serial_write,
        .read  = serial_read,
        .get_reference_msec = get_current_time_ms,
        .measure_time_msec   = measure_elapsed_time,
        .user_data = hSerial
    };

    // Initialize Modbus context
    modbus_context_t ctx;
    uint16_t device_address = 1;
    uint16_t baudrate = 19200;

    modbus_error_t error = modbus_slave_create(&ctx, &transport, device_address, baudrate);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to initialize Modbus Slave. Error code: %d\n", error);
        CloseHandle(hSerial);
        return 1;
    }

    printf("[Info] Modbus Slave initialized successfully.\n");

    // Register holding registers
    int16_t reg1 = 100;
    int16_t reg2 = 200;

    error = modbus_set_holding_register(&ctx, 0x0000, &reg1, false, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to register holding register 0x0000. Error code: %d\n", error);
    }

    error = modbus_set_holding_register(&ctx, 0x0001, &reg2, false, NULL, NULL);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to register holding register 0x0001. Error code: %d\n", error);
    }

    printf("[Info] Holding registers registered successfully.\n");

    // Add device information
    const char vendor_name[] = "Embraco_Modbus_Slave";
    error = modbus_slave_add_device_info(&ctx, vendor_name, sizeof(vendor_name) - 1);
    if (error != MODBUS_ERROR_NONE) {
        printf("[Error] Failed to add device information. Error code: %d\n", error);
    }

    printf("[Info] Device information added successfully.\n");

    // Main polling loop
    printf("[Info] Entering main polling loop. Press Ctrl+C to exit.\n");
    while (1) {
        modbus_slave_poll(&ctx);
        // Additional application tasks can be performed here
    }

    // Cleanup (unreachable in this example)
    CloseHandle(hSerial);
    return 0;
}
