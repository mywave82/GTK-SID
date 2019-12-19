#ifndef _INSTANCE_H
#define _INSTANCE_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void sid_instance_init(void);

extern void sid_instance_done(void);

extern int sid_instance_clock(int16_t *buf, int buflen);

extern void sid_instance_write (uint_least8_t addr, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif
