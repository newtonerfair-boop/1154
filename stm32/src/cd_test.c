// 极简 C/D 双轮测试 — 不用编码器，纯粹 PWM+方向
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

    // GPIOA/B/C + TIM1
    RCC_APB2ENR |= (1<<2)|(1<<3)|(1<<4)|(1<<11);

    // PA10/PA11: AF推挽 (仅C/D PWM)
    GPIOA->CRH = (GPIOA->CRH & 0xFFFF0000) | (0xB<<8) | (0xB<<12);

    // PC4/PC5: 推挽 (C轮方向)
    GPIOC->CRL = (GPIOC->CRL & 0xFF000000) | (0x3<<16) | (0x3<<20);
    // PB0/PB1: 推挽 (D轮方向)
    GPIOB->CRL = (GPIOB->CRL & 0xFFFFFF00) | (0x3<<0) | (0x3<<4);

    // ---- 方向设为正转 ----
    GPIOC->BSRR = (1<<4); GPIOC->BRR = (1<<5);   // C: IN1高 IN2低
    GPIOB->BSRR = (1<<0); GPIOB->BRR = (1<<1);   // D: IN1高 IN2低

    // ---- TIM1 PWM: 10kHz ----
    TIM1_PSC   = 3;
    TIM1_ARR   = 1799;
    TIM1_CCR1  = 0;
    TIM1_CCR2  = 0;
    TIM1_CCR3  = 540;  // C轮 30%
    TIM1_CCR4  = 540;  // D轮 30%

    TIM1_CCMR1 = (6<<4)|(1<<3)|(6<<12)|(1<<11);
    TIM1_CCMR2 = (6<<4)|(1<<3)|(6<<12)|(1<<11);
    TIM1_CCER  = (1<<8)|(1<<12);  // 仅CH3+CH4
    TIM1_BDTR  = (1<<15);
    TIM1_CR1   = (1<<7);
    TIM1_EGR   = (1<<0);
    TIM1_CR1  |= (1<<0);

    while(1);
}
