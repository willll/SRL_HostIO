/**
 *  SD card access module for Saturn Gamer's Cartridge
 *  by cafe-alpha
 *
 *  See LICENSE file for details.
**/

#ifndef _SGCLIB_LOG_
#define _SGCLIB_LOG_

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define LOG_ENABLE 0


#if LOG_ENABLE == 1

#include "sclib_4b/sclib_4d.h"

void log_init(void);
#define log_out(...)     scd_logout(__VA_ARGS__)
#define msg_out(id, ...) scd_msgout(id, __VA_ARGS__)

#else

#define log_init()
#define log_out(...)
#define msg_out(id, ...)

#endif

#endif /* _SGCLIB_LOG_ */
