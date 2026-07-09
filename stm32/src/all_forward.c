// AWD 四轮前进 — 极简恒定30%占空比
#include <stdint.h>

#define RCC_BASE      0x40021000
#define RCC_CR        (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR      (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_APB2ENR   (*(volatile uint32_t*)(RCC_BASE + 0x18))

typedef struct { volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; } GPIO_T;
#define GPIOA ((GPIO_T*)0x40010800)
#define GPIOB ((GPIO_T*)0x40010C00)
#define GPIOC ((GPIO_T*)0x40011000)

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

void _start(void);
__attribute__((section(".vectors")))
const uint32_t vector_table[] = {
    [0] = 0x2000C000,
    [1] = (uint32_t)&_start,
};

void _start(void) {
    // 72MHz
    RCC_CR |= (1<<16); while (!(RCC_CR&(1<<17)));
    RCC_CFGR |= (7<<18)|(1<<16);
    RCC_CR |= (1<<24); while (!(RCC_CR&(1<<25)));
    RCC_CFGR |= (2<<0); while ((RCC_CFGR&0x0C)!=0x08);

    // 时钟: GPIOA/B/C + TIM1
    RCC_APB2ENR |= (1<<2)|(1<<3)|(1<<4)|(1<<11);

    // PWM 脚: PA8/9/10/11 复用推挽 — 保留高16位(含SWD调试口)
    GPIOA->CRH = (GPIOA->CRH & 0xFFFF0000)
               | (0xB<<0) | (0xB<<4) | (0xB<<8) | (0xB<<12);

    // 方向脚: PC0~5 推挽 — 保留高8位(PC6/7编码器D)
    GPIOC->CRL = (GPIOC->CRL & 0xFF000000)
               | (0x3<<0)|(0x3<<4)|(0x3<<8)|(0x3<<12)|(0x3<<16)|(0x3<<20);
    // PB0~1 推挽 — 先清零低8位再写入
    GPIOB->CRL = (GPIOB->CRL & 0xFFFFFF00) | (0x3<<0)|(0x3<<4);

    // ===== 先设方向再开TIM1，避免方向-使能竞争 =====
    // A(PC0/1): IN1高 IN2低 → 正转
    GPIOC->BSRR = (1<<0);  GPIOC->BRR = (1<<1);
    // B(PC2/3):
    GPIOC->BSRR = (1<<2);  GPIOC->BRR = (1<<3);
    // C(PC4/5):
    GPIOC->BSRR = (1<<4);  GPIOC->BRR = (1<<5);
    // D(PB0/1):
    GPIOB->BSRR = (1<<0);  GPIOB->BRR = (1<<1);

    // ===== TIM1 PWM: 10kHz, 30%占空比 =====
    TIM1_PSC = 3;
    TIM1_ARR = 1799;
    TIM1_CCR1 = 540;  // 30%
    TIM1_CCR2 = 540;
    TIM1_CCR3 = 540;
    TIM1_CCR4 = 540;

    TIM1_CCMR1 = (6<<4)|(1<<3)|(6<<12)|(1<<11);  // CH1/2 PWM mode1
    TIM1_CCMR2 = (6<<4)|(1<<3)|(6<<12)|(1<<11);  // CH3/4
    TIM1_CCER  = (1<<0)|(1<<4)|(1<<8)|(1<<12);   // 使能四路
    TIM1_BDTR  = (1<<15);  // MOE 主输出使能
    TIM1_CR1   = (1<<7);   // ARPE
    TIM1_EGR   = (1<<0);   // 更新
    TIM1_CR1  |= (1<<0);   // 启动

    // ===== 永久运行 =====
    while(1);
}
