#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>

void init_timer(void);
uint16_t timer_take_ticks(void);
uint32_t timer_get_millis(void);

#endif