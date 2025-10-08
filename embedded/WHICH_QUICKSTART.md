# Which Quickstart Should I Use?

**Goal:** Get the right setup guide for your platform in under 30 seconds.

Use this flowchart to navigate to the correct quickstart guide.

---

## 🎯 Flowchart

```
START: What are you building?
│
├─ 🖥️  Desktop App (Linux/macOS/Windows)
│   └─ Use: examples/level1_hello/
│      → 10-line examples with mb_host.h API
│      → No platform-specific setup needed
│
├─ 🔌 Embedded System
│   │
│   ├─ Which platform?
│   │
│   ├─ ESP32 (ESP-IDF)
│   │   └─ Use: embedded/quickstarts/esp32/
│   │      → CMake component integration
│   │      → FreeRTOS + WiFi TCP transport
│   │      → UART RTU with DMA
│   │
│   ├─ STM32 (Any IDE: STM32CubeIDE, IAR, Keil, bare Makefile)
│   │   └─ Use: embedded/quickstarts/stm32/
│   │      → STM32 HAL + LL drivers
│   │      → UART + DMA + IDLE interrupt
│   │      → Timer-based T1.5/T3.5 guards
│   │
│   ├─ Zephyr RTOS (Any chip: Nordic, STM32, ESP32, etc.)
│   │   └─ Use: embedded/quickstarts/zephyr/
│   │      → West module integration
│   │      → Socket API for TCP
│   │      → UART API for RTU
│   │
│   ├─ Renesas RL78 (bare-metal)
│   │   └─ Use: embedded/quickstarts/renesas-rl78/
│   │      → Code generator (CG) integration
│   │      → Serial Array Unit (SAU) UART
│   │      → Timer-based guards
│   │
│   ├─ Arduino (Uno, Mega, ESP32, etc.)
│   │   └─ Use: embedded/quickstarts/arduino/
│   │      → Arduino Library Manager
│   │      → Serial/SoftwareSerial RTU
│   │
│   └─ Other / Custom / Bare-Metal
│       └─ Use: embedded/porting-wizard.md
│          → Step-by-step porting guide
│          → Implement 4 HAL functions
│          → Takes ~2 hours
│
└─ 🧪 Testing / Learning
    └─ Use: examples/level1_hello/
       → Simplest examples (10 lines)
       → Use loopback or simulator
```

---

## 📋 Quick Decision Table

| Your Situation | Recommended Quickstart | Time to First Build |
|----------------|------------------------|---------------------|
| Desktop app (Linux/macOS/Windows) | [examples/level1_hello](../examples/level1_hello) | 2 min |
| ESP32 with ESP-IDF | [embedded/quickstarts/esp32](quickstarts/esp32) | 15 min |
| STM32 (any IDE) | [embedded/quickstarts/stm32](quickstarts/stm32) | 20 min |
| Zephyr RTOS | [embedded/quickstarts/zephyr](quickstarts/zephyr) | 10 min |
| Renesas RL78 | [embedded/quickstarts/renesas-rl78](quickstarts/renesas-rl78) | 25 min |
| Arduino | [embedded/quickstarts/arduino](quickstarts/arduino) | 5 min |
| Other / bare-metal | [embedded/porting-wizard.md](porting-wizard.md) | 2 hours |

---

## 🔍 Still Not Sure?

### Common Questions

**Q: I'm using FreeRTOS but not ESP32. Which quickstart?**  
A: Start with `embedded/porting-wizard.md` and refer to `modbus/ports/modbus_port_freertos.c` for FreeRTOS primitives.

**Q: I need RTU over RS-485. Which guide?**  
A: All embedded quickstarts cover RTU. For desktop, see `examples/level1_hello/hello_rtu.c`.

**Q: I want TCP over Ethernet. Which guide?**  
A: 
- Desktop: Use `examples/level1_hello/hello_tcp.c`
- ESP32: See `embedded/quickstarts/esp32` (WiFi + lwIP)
- Zephyr: See `embedded/quickstarts/zephyr` (native TCP sockets)
- Other: See `embedded/porting-wizard.md` and implement socket HAL

**Q: Can I use this with libmodbus?**  
A: Yes! See `examples/unit_test_loop_demo.c` for libmodbus-style API mapping.

**Q: Do I need RTOS?**  
A: No. The library works with bare-metal, RTOS, or desktop OS. The client/server FSMs are non-blocking and cooperative.

---

## 📚 Next Steps After Quickstart

1. ✅ Complete your platform quickstart
2. 📖 Read the [API documentation](https://lgili.github.io/modbus/docs/)
3. 🛠️ Check [TROUBLESHOOTING.md](../docs/TROUBLESHOOTING.md) if you hit issues
4. 🚀 Explore advanced examples in `examples/`
5. 🔧 Customize your port in `modbus/ports/`

---

## 🆘 Need Help?

- **Build errors?** See [TROUBLESHOOTING.md](../docs/TROUBLESHOOTING.md)
- **API questions?** See [API docs](https://lgili.github.io/modbus/docs/)
- **Porting issues?** See [porting-wizard.md](porting-wizard.md)
- **Bug reports:** [GitHub Issues](https://github.com/lgili/modbus/issues)
