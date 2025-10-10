#ifndef MODBUS_COMPAT_ERRNO_H
#define MODBUS_COMPAT_ERRNO_H

/**
 * @file modbus-errno.h
 * @brief Subset of libmodbus errno constants for compatibility builds.
 *
 * These values intentionally match libmodbus so that existing applications can
 * keep their error handling logic. They are used together with the public
 * ``modbus_errno`` symbol and the standard ``errno`` value.
 */

/* Protocol exception aliases ------------------------------------------------ */
#define EMBXILFUN   0x01
#define EMBXILADR   0x02
#define EMBXILVAL   0x03
#define EMBXSFAIL   0x04
#define EMBXACK     0x05
#define EMBXSBUSY   0x06
#define EMBXNACK    0x07
#define EMBXMEMPAR  0x08
#define EMBXGPATH   0x0A
#define EMBXGTAR    0x0B

/* Transport / client side errors ------------------------------------------- */
#define EMBBADCRC       0xE0
#define EMBBADDATA      0xE1
#define EMBADSLAVE      0xE2
#define EMBMDATA        0xE3
#define EMBEDATA        0xE4
#define EMBEBUSY        0xE5
#define EMBETIMEDOUT    0xE6
#define EMBECONNRESET   0xE7
#define EMBEILLSTATE    0xE8

#endif /* MODBUS_COMPAT_ERRNO_H */
