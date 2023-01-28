#ifndef __KSU_H_EMBED_KSUD
#define __KSU_H_EMBED_KSUD

#include "embed_ksud.lock.h"

// If you see `embed_ksud.lock.h` not found, please 
// embed ksud using bin2c.py and run
// `touch embed_ksud.lock.h`

extern unsigned int __ksud_size;
extern const char __ksud[];

#endif