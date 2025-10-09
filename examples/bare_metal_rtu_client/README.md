# Bare-Metal Modbus RTU Client Example

A minimal, copy-paste ready example of a Modbus RTU client running in bare-metal (no RTOS) environment.

## 📋 Features

- ✅ **Non-blocking**: Uses `poll_with_budget()` for cooperative multitasking
- ✅ **Portable**: Minimal HAL layer - adapt to any MCU
- ✅ **Production-ready**: Timeout handling, retries, error recovery
- ✅ **Educational**: Heavily commented code explaining each step

## 🎯 What It Does

1. Initializes UART for RS-485 communication (19200 baud, 8E1)
2. Sends FC03 (Read Holding Registers) request every second
3. Reads 10 registers starting from address 0x0000
4. Blinks LED on successful communication
5. Polls Modbus stack 4 times per main loop iteration

## 🛠️ Hardware Requirements

- MCU with UART (STM32, ESP32, nRF52, etc.)
- RS-485 transceiver (MAX485, ADM2587E, etc.)
- LED on GPIO (for status indication)
- 1ms system tick timer (SysTick or hardware timer)

## 🔌 Wiring

```
MCU UART TX  →  RS-485 DI (Driver Input)
MCU UART RX  ←  RS-485 RO (Receiver Output)
MCU GPIO     →  RS-485 DE/RE (Direction control - optional)
```

## 📁 Files

- `main.c` - Main application code
- `system_config.h` - Hardware abstraction layer (HAL)
- `Makefile` - Build configuration (adapt to your toolchain)
- `README.md` - This file

## 🚀 Getting Started

### Step 1: Adapt HAL Functions

Edit `system_config.h` and implement these functions for your platform:

```c
void system_clock_init(void);    // Configure PLL/clocks
void timer_init(void);           // Setup 1ms tick
uint32_t millis(void);           // Return millisecond counter
void uart_init(...);             // Configure UART (19200 8E1)
size_t uart_send(...);           // Send bytes via UART
size_t uart_recv_available(...); // Check RX buffer
size_t uart_recv(...);           // Read from RX buffer
void led_init/on/off(void);      // LED control
```

**Tip**: See `embedded/ports/stm32-ll-dma-idle/` for reference UART implementation with DMA.

### Step 2: Configure Modbus Parameters

Edit `main.c` and adjust:

```c
#define SERVER_ADDRESS      1       // Your server unit ID
#define REGISTER_START      0x0000  // Starting register address
#define REGISTER_COUNT      10      // Number of registers
#define POLL_BUDGET         4       // Steps per main loop
#define REQUEST_INTERVAL_MS 1000    // Request interval
```

### Step 3: Build

#### Using Make (GCC ARM):
```bash
make PLATFORM=STM32F4
```

#### Using CMake:
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-none-eabi.cmake ..
cmake --build .
```

#### Using STM32CubeIDE:
1. Import project
2. Add `modbus` library to include paths
3. Link against `libmodbus.a`
4. Build and flash

### Step 4: Flash & Test

```bash
# Flash to target (adjust for your programmer)
st-flash write build/modbus_client.bin 0x08000000

# Or using OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "program build/modbus_client.elf verify reset exit"
```

## 📊 Expected Behavior

1. **LED Off**: Waiting for first request
2. **LED Blinks**: Successful communication
3. **LED Off**: Communication timeout/error
4. **Rapid Blink**: Init failure (check UART connections)

## 🔍 Debugging

### Check UART Output

Use a logic analyzer or oscilloscope to verify:
- Baud rate: 19200 bps
- Format: 8E1 (8 data bits, even parity, 1 stop bit)
- Frame structure: `[Unit ID][FC03][Start][Count][CRC]`

### Enable Debug Logging

In `system_config.h`, enable logging:

```c
#define MB_LOG_ENABLE 1
#define MB_LOG_LEVEL MB_LOG_DEBUG
```

Then implement `mb_log()` to output via UART or SWO.

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| No communication | Wrong baud rate | Verify 19200 baud |
| CRC errors | Noisy line | Add termination resistors (120Ω) |
| Timeouts | Wrong server address | Check SERVER_ADDRESS matches device |
| LED rapid blink | Init failure | Verify UART peripheral enabled |

## 🎓 Understanding the Code

### Main Loop Structure

```c
while (1) {
    // 1. Check if it's time to send new request
    if (now - last_request >= REQUEST_INTERVAL_MS) {
        send_read_request();
    }
    
    // 2. Poll Modbus stack (non-blocking, budgeted)
    mb_client_poll_with_budget(&client, POLL_BUDGET);
    
    // 3. Your application logic here
    process_register_values();
}
```

### Budget Control

`POLL_BUDGET = 4` means the stack processes up to 4 internal steps per iteration:
- Step 1: Check RX buffer
- Step 2: Parse incoming frame
- Step 3: Validate CRC
- Step 4: Invoke callback

This prevents the Modbus stack from blocking your main loop for too long.

### Callback Pattern

```c
void modbus_read_callback(mb_client_t *client,
                          const mb_client_txn_t *txn,
                          mb_err_t status,
                          const mb_adu_view_t *response,
                          void *user_ctx)
{
    if (status == MB_OK) {
        // Parse response and extract register values
        mb_pdu_parse_read_holding_response(...);
    } else {
        // Handle error (timeout, CRC error, exception)
    }
}
```

## 📈 Performance

On STM32F4 @ 168 MHz:
- CPU usage: < 1% (idle, 1 req/sec)
- RAM usage: ~512 bytes (client + pool)
- ROM usage: ~8 KB (LEAN profile with FC03 only)

## 🔗 See Also

- [Modbus Documentation](../../docs/)
- [RTU Timing Guide](../../docs/rtu_timing.md)
- [Port Wizard](../../embedded/porting-wizard.md)
- [STM32 DMA Example](../../embedded/ports/stm32-ll-dma-idle/)

## 📝 License

Same as main library (MIT).

---

**Need help?** Check [TROUBLESHOOTING.md](../../docs/TROUBLESHOOTING.md) or open an issue.
