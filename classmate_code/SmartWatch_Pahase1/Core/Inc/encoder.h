#ifndef __ENCODER_H
#define __ENCODER_H

#include <stdint.h>

void Encoder_Init(void);
int8_t Encoder_GetDelta(void);
uint8_t Encoder_ButtonPressed(void);

#endif
