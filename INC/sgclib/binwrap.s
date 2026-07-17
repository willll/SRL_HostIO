!
!  SD card access module for Saturn Gamer's Cartridge
!  by cafe-alpha
!
!  See LICENSE file for details.
!

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    .global ___sgclib_stub_dat
    .global _sgclib_stub_end
    .global __sgclib_stub_end
    .global ___sgclib_stub_end

    .align 4
_sgclib_stub_dat:
__sgclib_stub_dat:
___sgclib_stub_dat:
    .incbin "sgclib/sgclib.bin"

    .align 4
_sgclib_stub_end:
__sgclib_stub_end:
___sgclib_stub_end:
