/************************************************************************
  memutil.c
    2019/09/17  yuxia228      development start
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 2 as
 published by the Free Software Foundation.
************************************************************************/
#include <asm/io.h>	// for ioremap
#include <asm/uaccess.h>   // for copy_to/from
#include <linux/cdev.h>    // for char dev
#include <linux/device.h>  // for makeing /dev
#include <linux/fs.h>      // for char dev
#include <linux/init.h>    // kernel-module
#include <linux/module.h>  // kernel-module
#include <linux/types.h>   // for ssize_t
#include <linux/uaccess.h> // for copy_to/from
MODULE_LICENSE("GPL");

/***************************************************************
 * local header
 * ************************************************************/
#include "memutil_debug.h" // for PDEBUG

/***************************************************************
 * prototype function
 * ************************************************************/
static int memdev_open(struct inode *inode, struct file *filp);
static int memdev_release(struct inode *inode, struct file *filp);
static ssize_t memdev_read(struct file *filp, char __user *buff, size_t count,
			   loff_t *offp);
static ssize_t memdev_write(struct file *filp, const char __user *buff,
			    size_t count, loff_t *offp);
static void cmd_write(unsigned long addr, unsigned long value, int length);
static void cmd_read(unsigned long addr, int length, int read_cycle);

/***************************************************************
 * Variables
 * ************************************************************/
#define DEV_NAME "memd"
static unsigned int major_num = 0;
static unsigned int minor_num = 0;
static struct cdev memdev_cdev;

/***************************************************************
 * Char Device
 * ************************************************************/
static struct class *memdev_class =
    NULL; // デバイスドライバのクラスオブジェクト
enum MODE
{
	M_NONE,
	M_READ,
	M_WRITE,
	M_MONITOR,
};

static unsigned long g_addr;
static int g_mode = M_NONE;
static char g_buf[1024];

static int cmdparse(char *k_buf)
{
	char cmd[32];
	unsigned long addr, value;
	int length, ret;
	int l_mode = M_NONE;
	ret = sscanf(k_buf, "%31s %d %lx %lx", cmd, &length, &addr, &value);
	cmd[31] = '\0';
	PDEBUG("ret:    %d\n", ret);
	PDEBUG("cmd:    %s\n", cmd);
	PDEBUG("length: %d\n", length);
	PDEBUG("addr:   %08lx\n", addr);
	PDEBUG("value:  %08lx\n", value);
	switch (cmd[0])
	{
	case 'w':
		l_mode = M_WRITE;
		if (ret == 4)
			cmd_write(addr, value, length);
		else
			printk("invalid argument\n");
		break;
	case 'r':
		l_mode = M_READ;
		if (ret == 3)
			cmd_read(addr, length, 4);
		else if (ret == 4)
			cmd_read(addr, length, value);
		else
			printk("invalid argument\n");
		break;
	case 'm':
		l_mode = M_MONITOR;
		break;
	default:
		l_mode = M_NONE;
		printk("invalid argument\n");
		break;
	}
	return l_mode;
}

static void cmd_write(unsigned long addr, unsigned long value, int length)
{
	// int loop;
	int write_size = 0;
	unsigned long old_value = 0, current_value = 0;
	int loop = 0;
	char *l_reg;
	unsigned int var;
	char *reg;

	// request_mem_region(addr, length + 1, DEV_NAME);
	reg = (char *)ioremap_nocache(addr, length);
	// if length not equal 1, 2, 4.
	if (0 == length || 1 < ((length & 0x01) | (length >> 1 & 0x01) | (length >> 2 & 0x01)))
	{
		printk("invalid argument: length is only 1,2,4,or 8\n");
		return;
	}
	else
		write_size = length;

	PDEBUG("Write [%08lx] %0*lx write_size:%d[byte]\n", addr, length / 2,
	       value, write_size);
	if (1 == write_size)
	{
		old_value = ioread8(reg);
		iowrite8((unsigned char)(value & 0xff), reg);
		current_value = ioread8((volatile void *)reg);
	}
	else if (2 == write_size)
	{
		old_value = ioread16(reg);
		iowrite16((unsigned short)(value & 0xffff), reg);
		current_value = ioread16(reg);
	}
	else if (4 == write_size)
	{
		old_value = ioread32(reg);
		iowrite32((unsigned int)(value & 0xffffffff), reg);
		current_value = ioread32(reg);
	}
	PDEBUG("[%08lx] %0*lx -> %0*lx\n", addr, length * 2, old_value,
	       length * 2, current_value);
	iounmap((void *)addr);
	// release_mem_region(addr, length + 1);
}

static void cmd_read(unsigned long addr, int length, int _read_cycle)
{
	int loop;
	int read_cycle;
	unsigned long val = 0;
	unsigned long l_addr;
	char *reg;
	g_addr = addr;
	// if _read_cycle not equal 1, 2, 4.
	if (0 == _read_cycle || 1 < ((_read_cycle >> 0 & 0x01) + (_read_cycle >> 1 & 0x01) + (_read_cycle >> 2 & 0x01)))
		read_cycle = 4;
	else
		read_cycle = _read_cycle;

	if (0 != length % read_cycle)
		length += read_cycle - (length % read_cycle);
	// request_mem_region(addr, length + 1, DEV_NAME);
	reg = (char *)ioremap_nocache(addr, length);
	PDEBUG("Read [%08lx] %d byte\n", addr, length);
	for (loop = 0; loop < length; loop++)
	{
		char str[100];
		val |= (unsigned long)ioread8(reg++) << 8 * (loop % read_cycle);
		if ((loop % read_cycle) == (read_cycle - 1))
		{
			l_addr = addr + read_cycle * (loop / read_cycle);
			sprintf(str, "[0x%08lx] 0x%0*lx\n", l_addr,
				read_cycle * 2, val);
			PDEBUG("%s", str);
			strcat(g_buf, str);
			val = 0;
		}
	}
	iounmap((void *)addr);
	// release_mem_region(addr, length + 1);
}

static int memdev_open(struct inode *inode, struct file *filp)
{
	PDEBUG("%s_open\n", DEV_NAME);
	return 0; // success
}

static int memdev_release(struct inode *inode, struct file *filp)
{
	PDEBUG("%s_release\n", DEV_NAME);
	return 0; // success
}

static ssize_t memdev_read(struct file *filp, char __user *buff, size_t count,
			   loff_t *offp)
{
	int ret;
	int i;
	PDEBUG("%s_read  : count: %d\n", DEV_NAME, (int)count);
	switch (g_mode)
	{
	case M_NONE:
		return 0;
	case M_READ:
		PDEBUG("M_READ\n");
		ret = copy_to_user(buff, g_buf, sizeof(g_buf));
		if (ret != 0)
			return -EFAULT;
		for (i = 0; i < 1024; ++i)
			g_buf[i] = '\0';
		g_mode = M_NONE;
		return sizeof(g_buf);
	case M_WRITE:
		return 0;
	case M_MONITOR:
		return 0;
	}
	return 0;
}

static ssize_t memdev_write(struct file *filp, const char __user *buff,
			    size_t count, loff_t *offp)
{
	static int ret;
	static char k_buf[1024];

	PDEBUG("%s_write : count: %d\n", DEV_NAME, (int)count);
	ret = copy_from_user(k_buf, buff, count);
	if (ret > 0)
	{
		PDEBUG("Data is Remain: %d\n", ret);
	}
	PDEBUG("Recv: %s\n", k_buf);
	g_mode = cmdparse(k_buf);

	return count;
}

struct file_operations memdev_fops = {
    .owner = THIS_MODULE,
    .read = memdev_read,
    .write = memdev_write,
    .open = memdev_open,
    .release = memdev_release,
};

/***************************************************************
 * Driver Initilize / Close
 * ************************************************************/
static int memutil_init(void)
{
	int ret = 0;			   // For error handling
	dev_t dev = MKDEV(0, 0);	   // dev_t構造体の作成
	static unsigned int dev_count = 1; // デバイス番号の数
	printk("init memutil\n");
	PDEBUG("Debug is ON\n");

	// デバイス番号の動的な割り当て
	ret = alloc_chrdev_region(&dev, 0, 1, DEV_NAME);
	if (ret != 0)
	{
		printk("  Error Init\n");
		return -1;
	}
	major_num = MAJOR(dev); // メジャー番号の格納
	minor_num = MINOR(dev); // マイナー番号の格納

	// devにcdevを埋め込む
	cdev_init(&memdev_cdev, &memdev_fops);
	memdev_cdev.owner = THIS_MODULE;
	ret = cdev_add(&memdev_cdev, MKDEV(major_num, minor_num), dev_count);
	if (ret != 0)
	{
		printk(KERN_ALERT "error cdev_add\n");
		unregister_chrdev_region(dev, dev_count);
		return -1;
	}

	// このデバイスのclassの登録
	memdev_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(memdev_class))
	{
		printk(KERN_ERR "class_create\n");
		cdev_del(&memdev_cdev);
		unregister_chrdev_region(dev, dev_count);
		return -1;
	}
	device_create(memdev_class, NULL, MKDEV(major_num, minor_num), NULL,
		      "%s", DEV_NAME);

	return 0;
}

static void memutil_exit(void)
{
	static unsigned int dev_count = 1; // デバイス番号の数
	dev_t dev = MKDEV(major_num, 0);

	// remove /dev/DEV_NAME
	device_destroy(memdev_class, MKDEV(major_num, minor_num));
	class_destroy(memdev_class);
	// remove cdev
	cdev_del(&memdev_cdev);
	// remove dev
	unregister_chrdev_region(dev, dev_count);

	printk("end memutil\n");
}

module_init(memutil_init);
module_exit(memutil_exit);