#ifndef __TIMER_H
#define __TIMER_H

#include <stdint.h>

void Timer_Init(void);
void Timer_SetTotal(uint8_t h, uint8_t m, uint8_t s);
void Timer_Start(void);
void Timer_Pause(void);
void Timer_Resume(void);
uint8_t Timer_IsRunning(void);
uint8_t Timer_IsPaused(void);
uint8_t Timer_IsTimeout(void);
void Timer_GetRemaining(uint8_t *h, uint8_t *m, uint8_t *s);

#endif
