// AWD 差速驱动固件 v1.0
// C/D 双轮差速 + 编码器闭环控制
// 编码器读取: TIM4 (C轮) + TIM8 (D轮)
#include <stdint.h>

// ===== RCC =====
#define RCC_BASE      0x40021000
#define RCC_CR        (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR      (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_APB2ENR   (*(volatile uint32_t*)(RCC_BASE + 0x18))
#define RCC_APB1ENR   (*(volatile uint32_t*)(RCC_BASE + 0x1C))

// ===== GPIO =====
typedef struct { volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; } GPIO_T;
#define GPIOA ((GPIO_T*)0x40010800)
#define GPIOB ((GPIO_T*)0x40010C00)
#define GPIOC ((GPIO_T*)0x40011000)

// ===== TIM1 (PWM, APB2) =====
#define TIM1_BASE     0x40012C00
#define TIM1_CR1      (*(volatile uint32_t*)(TIM1_BASE + 0x00))
#define TIM1_CCMR1    (*(volatile uint32_t*)(TIM1_BASE + 0x18))
#define TIM1_CCMR2    (*(volatile uint32_t*)(TIM1_BASE + 0x1C))
#define TIM1_CCER     (*(volatile uint32_t*)(TIM1_BASE + 0x20))
#define TIM1_PSC      (*(volatile uint32_t*)(TIM1_BASE + 0x28))
#define TIM1_ARR      (*(volatile uint32_t*)(TIM1_BASE + 0x2C))
#define TIM1_CCR1     (*(volatile uint32_t*)(TIM1_BASE + 0x34))
#define TIM1_CCR2     (*(volatile uint32_t*)(TIM1_BASE + 0x38))
#define TIM1_CCR3     (*(volatile uint32_t*)(TIM1_BASE + 0x3C))
#define TIM1_CCR4     (*(volatile uint32_t*)(TIM1_BASE + 0x40))
#define TIM1_BDTR     (*(volatile uint32_t*)(TIM1_BASE + 0x44))
#define TIM1_EGR      (*(volatile uint32_t*)(TIM1_BASE + 0x14))

// ===== TIM4 (编码器C轮, APB1) =====
#define TIM4_BASE     0x40000800
#define TIM4_CR1      (*(volatile uint32_t*)(TIM4_BASE + 0x00))
#define TIM4_SMCR     (*(volatile uint32_t*)(TIM4_BASE + 0x08))
#define TIM4_CCER     (*(volatile uint32_t*)(TIM4_BASE + 0x20))
#define TIM4_CCMR1    (*(volatile uint32_t*)(TIM4_BASE + 0x18))
#define TIM4_CNT      (*(volatile uint32_t*)(TIM4_BASE + 0x24))
#define TIM4_PSC      (*(volatile uint32_t*)(TIM4_BASE + 0x28))
#define TIM4_ARR      (*(volatile uint32_t*)(TIM4_BASE + 0x2C))

// ===== TIM8 (编码器D轮, APB2) =====
#define TIM8_BASE     0x40013400
#define TIM8_CR1      (*(volatile uint32_t*)(TIM8_BASE + 0x00))
#define TIM8_SMCR     (*(volatile uint32_t*)(TIM8_BASE + 0x08))
#define TIM8_CCER     (*(volatile uint32_t*)(TIM8_BASE + 0x20))
#define TIM8_CCMR1    (*(volatile uint32_t*)(TIM8_BASE + 0x18))
#define TIM8_CNT      (*(volatile uint32_t*)(TIM8_BASE + 0x24))
#define TIM8_PSC      (*(volatile uint32_t*)(TIM8_BASE + 0x28))
#define TIM8_ARR      (*(volatile uint32_t*)(TIM8_BASE + 0x2C))

void _start(void);
__attribute__((section(".vectors")))
const uint32_t vector_table[] = {
    [0] = 0x2000C000,
    [1] = (uint32_t)&_start,
};

static void delay_ms(uint32_t ms) {
    for (volatile uint32_t i=0; i<ms*9000; i++) __asm__("nop");
}

// 左驱动轮 (原Motor C): dir=1正转
static void motor_left(int dir, uint16_t duty) {
    if (dir) { GPIOC->BSRR = (1<<4); GPIOC->BRR = (1<<5); }  // PC4高 PC5低
    else     { GPIOC->BRR  = (1<<4); GPIOC->BSRR = (1<<5); } // 反转
    TIM1_CCR3 = duty > 1799 ? 1799 : duty;
}

// 右驱动轮 (原Motor D)
static void motor_right(int dir, uint16_t duty) {
    if (dir) { GPIOB->BSRR = (1<<0); GPIOB->BRR = (1<<1); }  // PB0高 PB1低
    else     { GPIOB->BRR  = (1<<0); GPIOB->BSRR = (1<<1); }
    TIM1_CCR4 = duty > 1799 ? 1799 : duty;
}

// 读取编码器计数值（硬件4倍频）
static int32_t encoder_left(void) {
    uint16_t raw = TIM4_CNT;
    TIM4_CNT = 0;  // 清零计数器
    return (int16_t)raw;  // 符号扩展处理正反转
}

static int32_t encoder_right(void) {
    uint16_t raw = TIM8_CNT;
    TIM8_CNT = 0;
    return (int16_t)raw;
}

void _start(void) {
    // ===== 时钟 72MHz =====
    RCC_CR |= (1<<16); while (!(RCC_CR&(1<<17)));
    RCC_CFGR |= (7<<18)|(1<<16);
    RCC_CR |= (1<<24); while (!(RCC_CR&(1<<25)));
    RCC_CFGR |= (2<<0); while ((RCC_CFGR&0x0C)!=0x08);

    // ===== 使能外设时钟 =====
    RCC_APB2ENR |= (1<<2)  // GPIOA
                |  (1<<3)  // GPIOB
                |  (1<<4)  // GPIOC
                |  (1<<11) // TIM1
                |  (1<<13); // TIM8 (APB2)

    RCC_APB1ENR |= (1<<2);  // TIM4 (APB1)

    // ===== PWM引脚: 仅 PA10 (C轮) + PA11 (D轮) =====
    // PA8/PA9 停用，保持默认浮空输入
    GPIOA->CRH = (GPIOA->CRH & 0xFFFF0000)
               | (0xB << 8) | (0xB << 12);

    // ===== 方向引脚 =====
    // GPIOC: 仅 PC4/PC5 (C轮方向)，PC0~3 保持浮空
    GPIOC->CRL = (GPIOC->CRL & 0xFF000000)
               | (0x3 << 16) | (0x3 << 20);
    // GPIOB: PB0/PB1 (D轮方向)，PB2~7 保持默认 (含编码器PB6/7)
    GPIOB->CRL = (GPIOB->CRL & 0xFFFFFF00)
               | (0x3 << 0) | (0x3 << 4);

    // ===== TIM1 PWM: 仅 CH3+CH4, 10kHz =====
    TIM1_PSC   = 3;
    TIM1_ARR   = 1799;
    TIM1_CCR1  = 0;    // A轮停用
    TIM1_CCR2  = 0;    // B轮停用
    TIM1_CCR3  = 0;    // C轮初始停转
    TIM1_CCR4  = 0;    // D轮初始停转

    // CCMR1: CH1/CH2 配置保留但不启用输出
    TIM1_CCMR1 = (6<<4)|(1<<3)|(6<<12)|(1<<11);
    // CCMR2: 仅 CH3/CH4 PWM模式1
    TIM1_CCMR2 = (6<<4)|(1<<3)|(6<<12)|(1<<11);
    // CCER: 仅使能 CH3 + CH4
    TIM1_CCER  = (1<<8)|(1<<12);
    TIM1_BDTR  = (1<<15);
    TIM1_CR1   = (1<<7);
    TIM1_EGR   = (1<<0);
    TIM1_CR1  |= (1<<0);

    // ===== TIM4 编码器模式 (C轮: PB6=A相, PB7=B相) =====
    // 先确保引脚为浮空输入（默认就是0x4，无需改动）
    TIM4_PSC   = 0;           // 不分频
    TIM4_ARR   = 0xFFFF;      // 最大计数范围
    TIM4_CCMR1 = (1<<0)       // CC1S=01 (TI1: PB6作为CH1输入)
               | (1<<8);      // CC2S=01 (TI2: PB7作为CH2输入)
    TIM4_CCER  = (1<<0)       // CC1E 使能CH1捕获
               | (1<<4);      // CC2E 使能CH2捕获
    TIM4_SMCR  = (3<<0);      // SMS=011 编码器模式3 (4倍频)
    TIM4_CNT   = 0;
    TIM4_CR1   = (1<<0);      // CEN 启动

    // ===== TIM8 编码器模式 (D轮: PC6=A相, PC7=B相) =====
    TIM8_PSC   = 0;
    TIM8_ARR   = 0xFFFF;
    TIM8_CCMR1 = (1<<0) | (1<<8);
    TIM8_CCER  = (1<<0) | (1<<4);
    TIM8_SMCR  = (3<<0);
    TIM8_CNT   = 0;
    TIM8_CR1   = (1<<0);

    // ===== 主循环: 两驱差速前进 + 编码器监控 =====
    uint16_t speed = 540;  // 30% 占空比
    uint32_t tick = 0;

    while (1) {
        tick++;

        // 每 100ms 读取一次编码器
        if (tick % 100 == 0) {
            int32_t encL = encoder_left();
            int32_t encR = encoder_right();

            // 简易闭环: 堵转加大输出 (左右独立)
            uint16_t spdL = speed, spdR = speed;
            if (encL < 5 && encL > -5) spdL = (speed < 1000) ? speed + 20 : 1000;
            if (encR < 5 && encR > -5) spdR = (speed < 1000) ? speed + 20 : 1000;

            motor_left(1, spdL);
            motor_right(1, spdR);
        }

        delay_ms(1);
    }
}
