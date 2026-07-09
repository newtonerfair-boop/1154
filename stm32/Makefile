# STM32F103RCT6 AWD 底盘固件 Makefile
# 编译环境: arm-none-eabi-gcc

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
CFLAGS  = -mcpu=cortex-m3 -mthumb -O2 -Wall -nostartfiles -ffreestanding -fno-common
LDFLAGS = -T stm32f103.ld

SRC_DIR  = src
BUILD_DIR = build

# 按需选择目标固件
SRCS = $(SRC_DIR)/diff_drive.c   # 当前固件: 2WD 差速驱动+编码器
# SRCS = $(SRC_DIR)/motor_test.c # 四轮加减速测试
# SRCS = $(SRC_DIR)/all_forward.c # 四轮前进测试
# SRCS = $(SRC_DIR)/cd_test.c     # C/D 双轮极简 PWM 测试

all: $(BUILD_DIR) firmware.bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

firmware.elf: $(SRCS) stm32f103.ld | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BUILD_DIR)/firmware.elf $(SRCS)

firmware.bin: firmware.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/firmware.elf $(BUILD_DIR)/firmware.bin
	@echo "✅ firmware.bin ready ($(BUILD_DIR)/firmware.bin)"

# 烧录 (WSL2 + usbipd)
flash:
	st-flash write $(BUILD_DIR)/firmware.bin 0x08000000

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all flash clean
