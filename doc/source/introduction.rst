Introduction
============

Welcome to the documentation of the **Modbus Master/Slave Library in C**. This library offers efficient and lightweight implementations of both Master (Client) and Slave (Server) functionalities for the Modbus protocol, making it ideal for embedded systems with limited resources.

**Key Features:**

- **Comprehensive Master and Slave Support:**
  - Implement both Master (Client) and Slave (Server) roles of the Modbus protocol seamlessly.
  
- **Robust State Management with FSM:**
  - Utilize a Finite State Machine (FSM) to efficiently manage communication states, ensuring reliable and predictable behavior.
  
- **Easy Integration with Custom Callbacks:**
  - Easily integrate custom read and write callbacks to handle Modbus registers, allowing for flexible and customizable data management.
  
- **No Dynamic Memory Allocation:**
  - Designed for resource-constrained environments by avoiding dynamic memory usage, ensuring consistent performance and reducing memory footprint.
  
- **Advanced Error Handling and CRC Verification:**
  - Implements thorough error detection mechanisms, including Cyclic Redundancy Check (CRC) validations, to ensure data integrity and reliable communication.
  
- **Device Information Support (Function Code 0x2B):**
  - Supports reading detailed device information, enabling comprehensive metadata retrieval for better device management and monitoring.
  
- **Static Resource Allocation:**
  - All memory and resources are statically allocated, providing predictable performance and simplifying memory management in embedded applications.
  
- **Modular and Extensible Design:**
  - The library's modular architecture allows for easy extension and customization, facilitating integration into a wide range of projects and applications.
  
- **Documentation and Examples:**
  - Comprehensive documentation and practical examples are provided to assist developers in quickly integrating and utilizing the library in their projects.

**Overview:**

The **Modbus Master/Slave Library in C** is tailored for developers seeking a dependable and efficient solution to implement Modbus communication in their embedded systems. Whether you're building industrial controllers, automation systems, or any application requiring robust Modbus support, this library provides the necessary tools and functionalities to achieve your goals with ease.

**Why Choose This Library?**

- **Efficiency:** Optimized for performance, ensuring minimal resource consumption without compromising functionality.
- **Reliability:** Robust error handling and state management guarantee stable and consistent communication.
- **Flexibility:** Customizable through callbacks and a modular design to fit diverse application needs.
- **Ease of Use:** Clear documentation and example implementations streamline the integration process.

Explore the following sections to learn how to install, configure, and effectively use the **Modbus Master/Slave Library in C** in your projects.

