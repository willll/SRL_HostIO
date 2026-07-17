/**
 *  SD card access module for Saturn Gamer's Cartridge
 *  by cafe-alpha
 *
 *  See LICENSE file for details.
**/

#include "log.h"

#if LOG_ENABLE == 1

#include "sclib_4b/sclib_4d.h"

/* The stub itself, wildly embedded here. */
#include "sclib_4b/sclib_4d_stub.h"
#include "sclib_4b/sclib_4d_stub.c"


void log_init(void)
{
    unsigned char* stub_ptr  = (unsigned char*)(sclib_4d_stub_bin_dat);
    unsigned long  stub_size = SCLIB_4D_STUB_BIN_SIZ;

    /* Load (copy) the stub to its execution area near end of LRAM.
     * After being loaded, data in &_sclib_4d_stub_dat array is no longer needed.
     */
    memcpy((void*)SCLIB_4D_LOAD_ADDRESS, (void*)stub_ptr, stub_size);

    /* Initialize sclib_4d. */
    sclib_4d_init();
}

#endif // LOG_ENABLE == 1

