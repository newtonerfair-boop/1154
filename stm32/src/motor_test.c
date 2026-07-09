// AWD 底盘固件 v0.2 — 四轮前进测试
// TIM1 四路 PWM (PA8~PA11) + GPIO 方向控制
#include <stdint.h>

// ===== 寄存器 =====
#define RCC_BASE      0x40021000
#define RCC_CR        (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR      (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_APB2ENR   (*(volatile uint32_t*)(RCC_BASE + 0x18))
#define RCC_APB1ENR   (*(volatile uint32_t*)(RCC_BASE + 0x1C))

// GPIO
typedef struct { volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; } GPIO_T;
#define GPIOA ((GPIO_T*)0x40010800)
#define GPIOB ((GPIO_T*)0x40010C00)
#define GPIOC ((GPIO_T*)0x40011000)

// TIM1 (高级定时器)
#define TIM1_BASE     0x40012C00
#define TIM1_CR1      (*(volatile uint32_t*)(TIM1_BASE + 0x00))
#define TIM1_CR2      (*(volatile uint32_t*)(TIM1_BASE + 0x04))
#define TIM1_SMCR     (*(volatile uint32_t*)(TIM1_BASE + 0x08))
#define TIM1_DIER     (*(volatile uint32_t*)(TIM1_BASE + 0x0C))
#define TIM1_SR       (*(volatile uint32_t*)(TIM1_BASE + 0x10))
#define TIM1_EGR      (*(volatile uint32_t*)(TIM1_BASE + 0x14))
#define TIM1_CCMR1    (*(volatile uint32_t*)(TIM1_BASE + 0x18))
#define TIM1_CCMR2    (*(volatile uint32_t*)(TIM1_BASE + 0x1C))
#define TIM1_CCER     (*(volatile uint32_t*)(TIM1_BASE + 0x20))
#define TIM1_CNT      (*(volatile uint32_t*)(TIM1_BASE + 0x24))
#define TIM1_PSC      (*(volatile uint32_t*)(TIM1_BASE + 0x28))
#define TIM1_ARR      (*(volatile uint32_t*)(TIM1_BASE + 0x2C))
#define TIM1_RCR      (*(volatile uint32_t*)(TIM1_BASE + 0x30))
#define TIM1_CCR1     (*(volatile uint32_t*)(TIM1_BASE + 0x34))
#define TIM1_CCR2     (*(volatile uint32_t*)(TIM1_BASE + 0x38))
#define TIM1_CCR3     (*(volatile uint32_t*)(TIM1_BASE + 0x3C))
#define TIM1_CCR4     (*(volatile uint32_t*)(TIM1_BASE + 0x40))
#define TIM1_BDTR     (*(volatile uint32_t*)(TIM1_BASE + 0x44))
#define TIM1_DCR      (*(volatile uint32_t*)(TIM1_BASE + 0x48))
#define TIM1_DMAR     (*(volatile uint32_t*)(TIM1_BASE + 0x4C))

void _start(void);
__attribute__((section(".vectors")))
const uint32_t vector_table[] = {
    [0] = 0x2000C000,
    [1] = (uint32_t)&_start,
};

static void delay_ms(uint32_t ms) {
    for (volatile uint32_t i=0; i<ms*9000; i++) __asm__("nop");
}

// 设置电机方向: mot=0..3, 1=正转 0=反转
static void motor_dir(int mot, int forward) {
    // Motor A (左前): PC0=IN1, PC1=IN2
    // Motor B (右前): PC2=IN1, PC3=IN2
    // Motor C (左后): PC4=IN1, PC5=IN2
    // Motor D (右后): PB0=IN1, PB1=IN2
    uint32_t in1_mask[] = {0, 0, 0, 0, 0, 0, 1}; // bit offset
    GPIO_T *ports[] = {GPIOC, GPIOC, GPIOC, GPIOB};
    int in1[] = {0, 2, 4, 0};
    int in2[] = {1, 3, 5, 1};

    if (forward) {
        ports[mot]->BSRR = (1 << in1[mot]);          // IN1 = HIGH
        ports[mot]->BRR  = (1 << in2[mot]);          // IN2 = LOW
    } else {
        ports[mot]->BRR  = (1 << in1[mot]);          // IN1 = LOW
        ports[mot]->BSRR = (1 << in2[mot]);          // IN2 = HIGH
    }
}

// 设置单个电机 PWM 占空比 (0~1799)
static void motor_pwm(int mot, uint16_t duty) {
    volatile uint32_t *ccr[] = {&TIM1_CCR1, &TIM1_CCR2, &TIM1_CCR3, &TIM1_CCR4};
    *ccr[mot] = duty > 1799 ? 1799 : duty;
}

void _start(void) {
    // ----- 时钟 72MHz -----
    RCC_CR |= (1<<16); while (!(RCC_CR&(1<<17)));
    RCC_CFGR |= (7<<18)|(1<<16);  // PLL x9, src=HSE
    RCC_CR |= (1<<24); while (!(RCC_CR&(1<<25)));
    RCC_CFGR |= (2<<0); while ((RCC_CFGR&0x0C)!=0x08);

    // ----- 使能外设时钟 -----
    RCC_APB2ENR |= (1<<2)  // GPIOA
                |  (1<<3)  // GPIOB
                |  (1<<4)  // GPIOC
                |  (1<<11); // TIM1

    // ----- PWM 引脚: PA8~PA11 复用推挽输出 -----
    // PA8: CRH bits 3-0   = 1011 (AF push-pull, 50MHz)
    // PA9: CRH bits 7-4   = 1011
    // PA10: CRH bits 11-8  = 1011
    // PA11: CRH bits 15-12 = 1011
    GPIOA->CRH = (GPIOA->CRH & 0xFFFF0000)
               | (0xB << 0) | (0xB << 4) | (0xB << 8) | (0xB << 12);

    // ----- 方向引脚: PC0~5, PB0~1 推挽输出 -----
    GPIOC->CRL = (GPIOC->CRL & 0xFF000000)
               | (0x3<<0)|(0x3<<4)|(0x3<<8)|(0x3<<12)|(0x3<<16)|(0x3<<20);

    GPIOB->CRL = (GPIOB->CRL & 0xFFFFFF00) | (0x3<<0)|(0x3<<4);

    // ----- TIM1 PWM 配置 -----
    // 10kHz: 72MHz / (PSC+1) / (ARR+1)
    // PSC=3, ARR=1799 → 72M/4/1800=10kHz
    TIM1_PSC = 3;
    TIM1_ARR = 1799;
    TIM1_RCR = 0;

    // CCMR1: CH1/CH2 PWM模式1, 预装载使能
    // CH1: OC1M=110 (bits6-4), OC1PE=1 (bit3)
    // CH2: OC2M=110 (bits14-12), OC2PE=1 (bit11)
    TIM1_CCMR1 = (6<<4) | (1<<3)   // CH1
               | (6<<12)| (1<<11);  // CH2

    // CCMR2: CH3/CH4 PWM模式1, 预装载使能
    TIM1_CCMR2 = (6<<4) | (1<<3)   // CH3
               | (6<<12)| (1<<11);  // CH4

    // CCER: 使能四路输出, 高电平有效
    TIM1_CCER = (1<<0)   // CC1E
              | (1<<4)   // CC2E
              | (1<<8)   // CC3E
              | (1<<12); // CC4E

    // BDTR: MOE=1 (主输出使能) — TIM1高级定时器必须设置
    TIM1_BDTR = (1<<15);

    // CR1: ARPE=1(预装载), 边沿对齐, 向上计数
    TIM1_CR1 = (1<<7);

    // 生成更新事件，装载影子寄存器
    TIM1_EGR = (1<<0);

    // 使能计数器
    TIM1_CR1 |= (1<<0);

    // ----- 初始占空比 0（电机静止） -----
    motor_pwm(0, 0);  // A 左前
    motor_pwm(1, 0);  // B 右前
    motor_pwm(2, 0);  // C 左后
    motor_pwm(3, 0);  // D 右后

    // ----- 所有电机正转方向 -----
    for (int i=0; i<4; i++) motor_dir(i, 1);

    // ===== 运行：缓慢启动到 30%，跑 3 秒，再停止 =====
    uint16_t duty = 0;

    // 加速阶段 (0→30% = 0→540)
    for (duty=0; duty<=540; duty+=10) {
        for (int i=0; i<4; i++) motor_pwm(i, duty);
        delay_ms(10);
    }

    // 匀速 3 秒
    delay_ms(3000);

    // 减速停止
    for (duty=540; duty>0; duty-=10) {
        for (int i=0; i<4; i++) motor_pwm(i, duty);
        delay_ms(5);
    }

    // 全停
    for (int i=0; i<4; i++) motor_pwm(i, 0);

    // 无限循环 — 每 5 秒重复一次
    while (1) {
        delay_ms(2000);

        for (duty=0; duty<=540; duty+=10) {
            for (int i=0; i<4; i++) motor_pwm(i, duty);
            delay_ms(10);
        }
        delay_ms(3000);

        for (duty=540; duty>0; duty-=10) {
            for (int i=0; i<4; i++) motor_pwm(i, duty);
            delay_ms(5);
        }
        for (int i=0; i<4; i++) motor_pwm(i, 0);
    }
}
