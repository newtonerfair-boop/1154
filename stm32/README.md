# STM32 底盘控制固件

## 架构

```
RDK X5 (上位机)
  │ USART3 (PB10/PB11)
  ▼
STM32F103RCT6 (72MHz)
  │ TIM1_CH1~4 (PA8~PA11) → 4路 PWM
  │ GPIO PC0~5, PB0~1     → 方向控制
  │ TIM2/3/4/8 编码器接口  → 转速反馈
  ▼
电机驱动板 (AT8236 / TB6612FNG)
  ▼
4× 直流减速电机 (A/B/C/D)
```

## 固件文件

| 文件 | 说明 |
|------|------|
| `diff_drive.c` | 差速驱动 + 编码器闭环 + 堵转检测 |
| `motor_test.c` | 四轮加减速循环测试 |
| `all_forward.c` | 四轮恒定 30% 前进 |
| `cd_test.c` | 极简 PWM 测试 |

## 编译 & 烧录

```bash
# 编译
make

# 烧录 (WSL2 需先 usbipd attach ST-Link)
make flash
```

## 引脚映射

详见 [hardware-pin-spec.md](hardware-pin-spec.md)
