# Makefile for Bare-Metal Modbus RTU Client
# Adapt this for your specific toolchain and target

# ==============================================================================
# PLATFORM SELECTION
# ==============================================================================
# Set PLATFORM to your target: STM32F4, STM32G0, ESP32, NRF52, etc.
PLATFORM ?= STM32F4

# ==============================================================================
# TOOLCHAIN CONFIGURATION
# ==============================================================================

# ARM GCC Toolchain (adjust path if needed)
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AR = $(PREFIX)ar
OBJCOPY = $(PREFIX)objcopy
SIZE = $(PREFIX)size

# ==============================================================================
# PROJECT PATHS
# ==============================================================================

PROJECT_NAME = modbus_rtu_client
BUILD_DIR = build
SRC_DIR = .
MODBUS_DIR = ../../modbus

# ==============================================================================
# SOURCE FILES
# ==============================================================================

SOURCES = \
	$(SRC_DIR)/main.c

# Add platform-specific startup and system files here
ifeq ($(PLATFORM),STM32F4)
SOURCES += \
	$(SRC_DIR)/startup_stm32f4xx.s \
	$(SRC_DIR)/system_stm32f4xx.c
endif

# ==============================================================================
# INCLUDE PATHS
# ==============================================================================

INCLUDES = \
	-I$(SRC_DIR) \
	-I$(MODBUS_DIR)/include

# Add platform-specific includes
ifeq ($(PLATFORM),STM32F4)
INCLUDES += \
	-I$(SRC_DIR)/CMSIS/Include \
	-I$(SRC_DIR)/CMSIS/Device/ST/STM32F4xx/Include
endif

# ==============================================================================
# COMPILER FLAGS
# ==============================================================================

# Common flags
CFLAGS = \
	-std=c11 \
	-Wall \
	-Wextra \
	-Werror \
	-O2 \
	-g3 \
	-ffunction-sections \
	-fdata-sections \
	$(INCLUDES)

# Platform-specific flags
ifeq ($(PLATFORM),STM32F4)
CFLAGS += \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfpu=fpv4-sp-d16 \
	-mfloat-abi=hard \
	-DSTM32F4 \
	-DSTM32F407xx \
	-DUSE_HAL_DRIVER
endif

# Modbus configuration
CFLAGS += \
	-DMB_CONF_PROFILE=MB_PROFILE_LEAN \
	-DMB_CONF_ENABLE_FC03=1 \
	-DMB_CONF_TRANSPORT_RTU=1 \
	-DMB_CONF_BUILD_CLIENT=1

# ==============================================================================
# LINKER FLAGS
# ==============================================================================

LDFLAGS = \
	-Wl,--gc-sections \
	-Wl,-Map=$(BUILD_DIR)/$(PROJECT_NAME).map

# Platform-specific linker flags
ifeq ($(PLATFORM),STM32F4)
LDFLAGS += \
	-mcpu=cortex-m4 \
	-mthumb \
	-mfpu=fpv4-sp-d16 \
	-mfloat-abi=hard \
	-T$(SRC_DIR)/STM32F407VGTx_FLASH.ld \
	-specs=nosys.specs \
	-specs=nano.specs
endif

# Link against Modbus library
LDFLAGS += -L$(MODBUS_DIR)/build -lmodbus

# ==============================================================================
# BUILD RULES
# ==============================================================================

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(SOURCES:.c=.o)))
OBJECTS := $(OBJECTS:.s=.o)

all: $(BUILD_DIR)/$(PROJECT_NAME).elf $(BUILD_DIR)/$(PROJECT_NAME).hex $(BUILD_DIR)/$(PROJECT_NAME).bin
	@echo "Build complete!"
	@$(SIZE) $(BUILD_DIR)/$(PROJECT_NAME).elf

$(BUILD_DIR)/$(PROJECT_NAME).elf: $(OBJECTS)
	@echo "Linking $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	@echo "Assembling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	@echo "Creating $@"
	@$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	@echo "Creating $@"
	@$(OBJCOPY) -O binary -S $< $@

$(BUILD_DIR):
	@mkdir -p $@

# ==============================================================================
# UTILITY TARGETS
# ==============================================================================

.PHONY: clean flash debug size help

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

flash: all
	@echo "Flashing $(BUILD_DIR)/$(PROJECT_NAME).bin to target..."
	st-flash write $(BUILD_DIR)/$(PROJECT_NAME).bin 0x08000000

debug:
	@echo "Starting GDB server (OpenOCD)..."
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

size: all
	@echo "Memory usage:"
	@$(SIZE) $(BUILD_DIR)/$(PROJECT_NAME).elf

help:
	@echo "Available targets:"
	@echo "  all      - Build project (default)"
	@echo "  clean    - Remove build directory"
	@echo "  flash    - Flash binary to target via ST-Link"
	@echo "  debug    - Start OpenOCD debug server"
	@echo "  size     - Show memory usage"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Platform selection:"
	@echo "  make PLATFORM=STM32F4 (default)"
	@echo "  make PLATFORM=STM32G0"
	@echo ""
	@echo "Note: You may need to adjust paths and linker scripts"

# ==============================================================================
# DEPENDENCIES
# ==============================================================================

-include $(OBJECTS:.o=.d)
