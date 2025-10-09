/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS configuration for Modbus RTU Client
 * 
 * This configuration is optimized for real-time Modbus communication on
 * Cortex-M4 with 128KB RAM. Adjust to your hardware capabilities.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ===========================================================================
 * HARDWARE CONFIGURATION
 * =========================================================================== */

/**
 * CPU Clock Frequency (Hz)
 * MUST match your system clock configuration
 */
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 80000000 )

/**
 * SysTick Frequency (Hz)
 * 1000 Hz = 1ms tick for Modbus timing requirements
 */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )

/**
 * Cortex-M4 with FPU
 * Set to 1 if your chip has hardware FPU (STM32F4, STM32G4, etc.)
 */
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0

/* ===========================================================================
 * KERNEL CONFIGURATION
 * =========================================================================== */

/**
 * Preemption: REQUIRED for real-time Modbus
 * High-priority RX task must preempt lower priority tasks
 */
#define configUSE_PREEMPTION                    1

/**
 * Time Slicing: DISABLED for predictable timing
 * Tasks run until they block or higher priority task preempts
 */
#define configUSE_TIME_SLICING                  0

/**
 * Idle Hook: ENABLED for power management
 * See vApplicationIdleHook() in main.c
 */
#define configUSE_IDLE_HOOK                     1

/**
 * Tick Hook: DISABLED (not needed for Modbus)
 */
#define configUSE_TICK_HOOK                     0

/**
 * Stack Overflow Detection: METHOD 2 (recommended)
 * Checks stack watermark on context switch
 * See vApplicationStackOverflowHook() in main.c
 */
#define configCHECK_FOR_STACK_OVERFLOW          2

/**
 * Malloc Failed Hook: ENABLED for debugging
 * See vApplicationMallocFailedHook() in main.c
 */
#define configUSE_MALLOC_FAILED_HOOK            1

/* ===========================================================================
 * MEMORY MANAGEMENT
 * =========================================================================== */

/**
 * Total Heap Size (bytes)
 * Breakdown for this example:
 * - RX task stack (256 * 4 bytes) = 1 KB
 * - TX task stack (256 * 4 bytes) = 1 KB
 * - App task stack (512 * 4 bytes) = 2 KB
 * - TCBs (3 * ~200 bytes) = 0.6 KB
 * - Stream buffers (512 bytes) = 0.5 KB
 * - Semaphores, mutexes = 0.5 KB
 * - Total ≈ 5.6 KB + margin = 8 KB
 */
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 8 * 1024 ) )

/**
 * Minimal Stack Size (words)
 * 128 words * 4 bytes = 512 bytes minimum per task
 */
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )

/**
 * Max Task Name Length
 * Shorter = less memory usage
 */
#define configMAX_TASK_NAME_LEN                 ( 16 )

/* ===========================================================================
 * TASK PRIORITIES
 * =========================================================================== */

/**
 * Number of Priority Levels
 * We use 4: Idle(0), App(1), TX(2), RX(3)
 */
#define configMAX_PRIORITIES                    ( 4 )

/* Priority constants (for reference) */
/* #define PRIORITY_MODBUS_RX (tskIDLE_PRIORITY + 3) // Highest */
/* #define PRIORITY_MODBUS_TX (tskIDLE_PRIORITY + 2) // High    */
/* #define PRIORITY_APP       (tskIDLE_PRIORITY + 1) // Normal  */

/* ===========================================================================
 * QUEUE AND SEMAPHORE CONFIGURATION
 * =========================================================================== */

#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0

/**
 * Task Notifications: ENABLED (lightweight alternative to binary semaphores)
 * Used for ISR -> task signaling in uart_idle_callback()
 */
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1

/* ===========================================================================
 * SOFTWARE TIMERS
 * =========================================================================== */

/**
 * Software Timers: DISABLED (not needed for Modbus)
 * Modbus uses vTaskDelay for periodic requests
 */
#define configUSE_TIMERS                        0
#define configTIMER_TASK_PRIORITY               ( 2 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/* ===========================================================================
 * CO-ROUTINES (deprecated)
 * =========================================================================== */

#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )

/* ===========================================================================
 * KERNEL FEATURES
 * =========================================================================== */

/**
 * 16-bit Ticks: DISABLED (use 32-bit for longer wait times)
 * Max delay = 0xFFFFFFFF ticks = ~49 days @ 1ms tick
 */
#define configUSE_16_BIT_TICKS                  0

/**
 * API Functions to Include
 */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1

/* ===========================================================================
 * STATS AND TRACING
 * =========================================================================== */

/**
 * Runtime Stats: DISABLED (enable for profiling)
 * When enabled, use vTaskGetRunTimeStats() to see CPU usage per task
 */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ===========================================================================
 * ASSERT AND DEBUG
 * =========================================================================== */

/**
 * configASSERT: ENABLED for development, OPTIONAL for production
 * Catches kernel errors like:
 * - Creating tasks with insufficient stack
 * - Calling kernel functions from invalid context
 * - Mutex/semaphore errors
 */
#ifdef DEBUG
    extern void vAssertCalled(const char *file, int line);
    #define configASSERT(x) if((x) == 0) vAssertCalled(__FILE__, __LINE__)
#else
    #define configASSERT(x) ((void)0)
#endif

/* ===========================================================================
 * CORTEX-M SPECIFIC CONFIGURATION
 * =========================================================================== */

/**
 * Interrupt Priority Levels
 * 
 * ARM Cortex-M uses lower numeric value = higher priority
 * STM32 typically has 4 bits = 16 priority levels (0-15)
 * 
 * Priority scheme for this example:
 * - 0-4:  NVIC priorities ABOVE FreeRTOS (can't call FreeRTOS APIs)
 *         Use for ultra-critical interrupts only
 * - 5:    configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
 *         FreeRTOS boundary - interrupts at this or lower priority
 *         can call FromISR() functions safely
 * - 6-15: NVIC priorities managed by FreeRTOS
 * 
 * UART IDLE ISR should be at priority 6 or lower to use xTaskNotifyFromISR()
 */

/**
 * Kernel Interrupt Priority (lowest priority)
 * Used by SysTick and PendSV
 */
#define configKERNEL_INTERRUPT_PRIORITY         ( 15 << 4 )  /* Priority 15 */

/**
 * Max Syscall Priority (highest priority that can call FreeRTOS APIs)
 * Interrupts with priority 0-4 cannot call FreeRTOS functions
 * Interrupts with priority 5-15 can call FromISR() functions
 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( 5 << 4 )   /* Priority 5 */

/**
 * Left-shift by 4 because STM32 uses upper 4 bits of priority byte
 * (configPRIO_BITS = 4 on most STM32, see CMSIS core_cm4.h)
 */

/* ===========================================================================
 * INTERRUPT SERVICE ROUTINE HELPERS
 * =========================================================================== */

/**
 * Context switch macros for ISRs
 * Used in uart_idle_callback() when calling xTaskNotifyFromISR()
 */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

/* Cortex-M4 specific */
#define configPRE_SLEEP_PROCESSING(x)           /* Optional: prepare for sleep */
#define configPOST_SLEEP_PROCESSING(x)          /* Optional: restore after wake */

/* ===========================================================================
 * RUNTIME ASSERTIONS (optional)
 * =========================================================================== */

/**
 * Queue Registry: DISABLED (for debugging with IAR/Keil debugger)
 */
#define configQUEUE_REGISTRY_SIZE               0

/**
 * Thread Local Storage Pointers per task: DISABLED (not needed)
 */
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0

/* ===========================================================================
 * NOTES FOR PORTING TO YOUR HARDWARE
 * =========================================================================== */

/*
 * 1. Adjust configCPU_CLOCK_HZ to match your SystemCoreClock
 * 
 * 2. If your chip has NO FPU (Cortex-M0/M3):
 *    - Set configENABLE_FPU = 0
 *    - Use -mfloat-abi=soft in compiler flags
 * 
 * 3. If you have limited RAM (<64KB):
 *    - Reduce configTOTAL_HEAP_SIZE (minimum ~4KB for this example)
 *    - Reduce task stack sizes in main.c
 *    - Disable stats: configUSE_TRACE_FACILITY = 0
 * 
 * 4. If you need software timers (for periodic Modbus polls):
 *    - Set configUSE_TIMERS = 1
 *    - Adjust configTIMER_TASK_PRIORITY (should be < PRIORITY_MODBUS_RX)
 * 
 * 5. For ultra-low-power applications:
 *    - Implement configPRE_SLEEP_PROCESSING to enter STOP/STANDBY mode
 *    - Use tickless idle mode (see FreeRTOS docs)
 *    - Wake on UART activity (UART interrupt with priority ≤ 5)
 * 
 * 6. For debugging stack overflows:
 *    - Enable configCHECK_FOR_STACK_OVERFLOW = 2
 *    - Use uxTaskGetStackHighWaterMark() to measure peak usage
 *    - Increase stack sizes if watermark is <10% of total
 * 
 * 7. For real-time performance analysis:
 *    - Enable configGENERATE_RUN_TIME_STATS = 1
 *    - Provide a high-resolution timer (e.g., DWT cycle counter)
 *    - Call vTaskGetRunTimeStats() to see CPU usage per task
 */

#endif /* FREERTOS_CONFIG_H */
