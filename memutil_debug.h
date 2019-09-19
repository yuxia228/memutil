#ifndef __MEMUTIL_DEBUG_H__
#define __MEMUTIL_DEBUG_H__ // include guard

#ifndef DRV_NAME
#define DRV_NAME "TEST"
#endif

// for switch output printk
#undef PDEBUG
#ifdef MEMUTIL_DEBUG
#ifdef __KERNEL__
// in kernel space
// #define PDEBUG(fmt, args...) printk(KERN_DEBUG DRV_NAME ": " fmt, ##args)
#define PDEBUG(fmt, args...) printk(KERN_INFO DRV_NAME ": " fmt, ##args)
#else
#define PDEBUG(fmt, args...) printf(stderr, DRV_NAME ": " fmt, ##args)
#endif // __KERNEL__
#else
#define PDEBUG(fmt, args...)
#endif // MEMUTIL_DEBUG

#undef PDEBUGG
#define PDEBUGG(fmt, args...)

#endif // __MEMUTIL_DEBUG_H__