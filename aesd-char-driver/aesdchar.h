#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include <linux/cdev.h>
#include <linux/mutex.h>
#include "aesd-circular-buffer.h"

#define AESD_DEBUG 1

#undef PDEBUG
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
#    define PDEBUG(fmt, args...) printk(KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)
#endif

struct aesd_dev {
    struct cdev cdev;
    struct aesd_circular_buffer buffer;
    struct mutex lock;
    char *partial_write;
    size_t partial_size;
};

int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
int aesd_init_module(void);
void aesd_cleanup_module(void);
static long aesd_ioctl(struct file *file, unsigned int ioctl_cmd, unsigned long ioctl_arg);
static loff_t aesd_llseek(struct file *file, loff_t user_offset, int origin);

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */

