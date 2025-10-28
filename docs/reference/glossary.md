# Glossary

Common terms and acronyms used in ModbusCore and Modbus protocol.

## A

**ADU (Application Data Unit)**
: The complete Modbus frame including addressing and error checking. For TCP, this is MBAP header + PDU. For RTU, this is slave address + PDU + CRC.

**ASCII Mode**
: Modbus serial transmission mode using ASCII characters (not supported by ModbusCore). Each byte is transmitted as two ASCII hex characters.

**Auto-Heal**
: ModbusCore feature for automatic error recovery, including frame repair and retry logic.

## B

**Baud Rate**
: Speed of serial communication in bits per second (bps). Common values: 9600, 19200, 38400, 115200.

**Big-Endian**
: Byte order where the most significant byte comes first. Modbus uses big-endian for multi-byte values.

**Broadcast**
: Modbus message sent to all devices (slave address 0). No response expected.

## C

**CAN Bus**
: Controller Area Network, a vehicle bus standard. Can be used as custom Modbus transport.

**Client**
: Modbus device that initiates requests (formerly called "master"). Synonym: Master.

**Coil**
: Single-bit read/write data (Boolean). Modbus functions 0x01 (read) and 0x05/0x0F (write).

**CRC (Cyclic Redundancy Check)**
: Error detection code used in Modbus RTU. ModbusCore uses CRC-16 (polynomial 0xA001).

## D

**Deterministic**
: Property where behavior is predictable and consistent. ModbusCore uses static memory for determinism.

**Diagnostics**
: ModbusCore feature for logging frames, errors, and performance statistics.

**Discrete Input**
: Single-bit read-only data (Boolean). Modbus function 0x02.

## E

**Embedded System**
: Computer system with dedicated function within larger system. ModbusCore targets embedded platforms.

**Exception Code**
: Error code returned by Modbus server when request cannot be processed. Common codes: 0x01 (illegal function), 0x02 (illegal address), 0x03 (illegal value).

**Exception Response**
: Modbus response indicating error. Function code is request function + 0x80.

## F

**Frame**
: Complete Modbus message ready for transmission. See ADU.

**FreeRTOS**
: Real-time operating system for embedded devices. ModbusCore supports FreeRTOS.

**Function Code**
: Byte indicating Modbus operation type. Examples: 0x03 (read holding), 0x06 (write single).

## H

**Half-Duplex**
: Communication mode where data flows in one direction at a time. RS-485 is half-duplex.

**Holding Register**
: 16-bit read/write data. Modbus functions 0x03 (read) and 0x06/0x10 (write).

## I

**Input Register**
: 16-bit read-only data. Modbus function 0x04.

**Inter-Frame Delay**
: Silent time between Modbus RTU frames. Must be at least 3.5 character times.

**ISR (Interrupt Service Routine)**
: Function called in response to hardware interrupt. ModbusCore has optional ISR-safe mode.

## L

**LibFuzzer**
: Fuzzing engine for finding bugs. ModbusCore tested with LibFuzzer.

**Little-Endian**
: Byte order where least significant byte comes first. CRC in RTU is little-endian.

## M

**Master**
: Legacy term for Modbus client. Modern term: Client.

**MBAP (Modbus Application Protocol)**
: Protocol header used in Modbus TCP. Contains transaction ID, protocol ID, length, and unit ID.

**Modbus**
: Industrial communication protocol for PLCs and sensors. Defines data model and message format.

**Modbus RTU**
: Serial variant of Modbus using binary encoding and CRC error checking.

**Modbus TCP**
: Ethernet variant of Modbus using TCP/IP with MBAP header.

## N

**Non-Blocking I/O**
: I/O operations that return immediately without waiting. Use with `select()` or `poll()`.

## P

**PDU (Protocol Data Unit)**
: Modbus message content: function code + data. Independent of transport layer.

**PLC (Programmable Logic Controller)**
: Industrial computer for automation. Common Modbus device.

**POSIX**
: Portable Operating System Interface. Standards for Unix-like systems. ModbusCore supports POSIX.

**Protocol ID**
: Field in MBAP header. Always 0 for Modbus TCP.

## Q

**QoS (Quality of Service)**
: Network traffic management. ModbusCore has optional QoS features.

## R

**Register**
: 16-bit Modbus data value. Types: holding (RW), input (RO).

**RS-232**
: Serial communication standard. Full-duplex, point-to-point, short distance (<15m).

**RS-485**
: Serial communication standard. Half-duplex, multi-drop, long distance (<1200m). Common for Modbus RTU.

**RTOS (Real-Time Operating System)**
: OS providing deterministic timing. Examples: FreeRTOS, Zephyr.

**RTU (Remote Terminal Unit)**
: Modbus serial protocol using binary encoding with CRC.

## S

**SCADA (Supervisory Control and Data Acquisition)**
: Industrial control system. Common user of Modbus.

**Server**
: Modbus device that responds to requests (formerly called "slave"). Synonym: Slave.

**Slave**
: Legacy term for Modbus server. Modern term: Server.

**Slave Address**
: Identifier for Modbus RTU device (1-247). Address 0 is broadcast.

**Socket**
: Network communication endpoint. Used for Modbus TCP.

**Static Allocation**
: Memory allocated at compile time. ModbusCore uses only static allocation.

## T

**TCP (Transmission Control Protocol)**
: Reliable, connection-oriented network protocol. Used by Modbus TCP.

**Timeout**
: Maximum time to wait for response. Configurable in ModbusCore.

**Transaction**
: Complete Modbus request-response cycle.

**Transaction ID**
: Identifier in MBAP header matching requests to responses. Incremented for each transaction.

**Transport Layer**
: Layer responsible for data transmission. ModbusCore supports TCP and RTU transports.

## U

**UART (Universal Asynchronous Receiver-Transmitter)**
: Hardware for serial communication. Used for Modbus RTU.

**UDP (User Datagram Protocol)**
: Connectionless network protocol. Can be used as custom Modbus transport.

**Unit ID**
: Field in MBAP header identifying device. Allows bridging to serial Modbus.

## V

**vcpkg**
: C/C++ package manager from Microsoft. Supports ModbusCore.

## W

**Winsock**
: Windows Sockets API. Required for Modbus TCP on Windows.

## Z

**Zephyr**
: Real-time operating system for embedded devices. ModbusCore supports Zephyr.

**Zero-Copy**
: Data transfer without copying. ModbusCore has optional zero-copy I/O.

---

## Modbus Function Codes

| Code | Name | Type | Data |
|------|------|------|------|
| 0x01 | Read Coils | Read | Bits (RW) |
| 0x02 | Read Discrete Inputs | Read | Bits (RO) |
| 0x03 | Read Holding Registers | Read | Words (RW) |
| 0x04 | Read Input Registers | Read | Words (RO) |
| 0x05 | Write Single Coil | Write | Bit (RW) |
| 0x06 | Write Single Register | Write | Word (RW) |
| 0x0F | Write Multiple Coils | Write | Bits (RW) |
| 0x10 | Write Multiple Registers | Write | Words (RW) |
| 0x17 | Read/Write Multiple Registers | Both | Words (RW) |

## Exception Codes

| Code | Name | Description |
|------|------|-------------|
| 0x01 | Illegal Function | Function code not supported |
| 0x02 | Illegal Data Address | Address out of range |
| 0x03 | Illegal Data Value | Value out of range |
| 0x04 | Server Device Failure | Unrecoverable error |
| 0x05 | Acknowledge | Long operation in progress |
| 0x06 | Server Device Busy | Server busy, retry later |

## Status Codes

ModbusCore status codes (`mbc_status_t`):

| Code | Value | Meaning |
|------|-------|---------|
| `MBC_STATUS_OK` | 0 | Success |
| `MBC_STATUS_INVALID_PARAMETER` | 1 | Invalid parameter |
| `MBC_STATUS_BUFFER_TOO_SMALL` | 2 | Output buffer too small |
| `MBC_STATUS_INVALID_FUNCTION` | 3 | Unsupported function code |
| `MBC_STATUS_INVALID_DATA_LENGTH` | 4 | Invalid data length |
| `MBC_STATUS_CRC_ERROR` | 5 | CRC mismatch (RTU) |
| `MBC_STATUS_TIMEOUT` | 6 | Operation timeout |
| `MBC_STATUS_IO_ERROR` | 7 | I/O error |
| `MBC_STATUS_NOT_CONNECTED` | 8 | Not connected |
| `MBC_STATUS_ENCODE_ERROR` | 9 | Encoding failed |
| `MBC_STATUS_DECODE_ERROR` | 10 | Decoding failed |

## Data Limits

| Item | Limit | Reason |
|------|-------|--------|
| **Max PDU size** | 253 bytes | Modbus spec |
| **Max MBAP frame** | 260 bytes | 7-byte header + 253 PDU |
| **Max RTU frame** | 256 bytes | 1 addr + 253 PDU + 2 CRC |
| **Max read coils** | 2000 | Modbus spec |
| **Max read registers** | 125 | Modbus spec |
| **Max write coils** | 1968 | Modbus spec |
| **Max write registers** | 123 | Modbus spec |
| **RTU slave address** | 1-247 | 0 = broadcast, 248-255 reserved |
| **Transaction ID** | 0-65535 | 16-bit unsigned |

## Serial Parameters

| Parameter | Common Values | Notes |
|-----------|--------------|-------|
| **Baud Rate** | 9600, 19200, 38400, 115200 | Must match all devices |
| **Data Bits** | 8 | Always 8 for Modbus |
| **Parity** | None, Even, Odd | Most common: None (8N1) |
| **Stop Bits** | 1, 2 | Most common: 1 |
| **Format** | 8N1, 8E1, 8O1 | 8 data, parity, 1 stop |

## Timing

| Timing | Value | Notes |
|--------|-------|-------|
| **Character time** | 11 bits / baudrate | 1 start + 8 data + 1 parity + 1 stop |
| **Inter-frame delay** | 3.5 characters | Minimum silence between frames |
| **Response timeout** | 1-10 seconds | Application-specific |
| **TCP connection timeout** | 5-30 seconds | Network-dependent |

---

## See Also

- [FAQ](faq.md) - Frequently asked questions
- [Troubleshooting](troubleshooting.md) - Common problems and solutions
- [API Reference](../api/README.md) - Complete API documentation

---

**Prev**: [Troubleshooting ←](troubleshooting.md) | **Home**: [Documentation Index →](../README.md)
