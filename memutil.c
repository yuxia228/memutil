/************************************************************************
  memutil.c : driver for read/write memory.
    2019/09/17  yuxia228      development start
 licensed under GNU General Public License 3.0 or later.
************************************************************************/
#include <asm/io.h>	// for ioremap
#include <asm/uaccess.h>   // for copy_to/from
#include <linux/cdev.h>    // for char dev
#include <linux/device.h>  // for making /dev
#include <linux/fs.h>      // for char dev
#include <linux/init.h>    // kernel-module
#include <linux/module.h>  // kernel-module
#include <linux/types.h>   // for ssize_t
#include <linux/uaccess.h> // for copy_to/from
#include <linux/delay.h>   // msleep function
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
static void cmd_monitor(unsigned long addr, int length, int read_cycle);
static void cmd_polling(unsigned long addr, int length, int interval,
			int read_cycle);
static int monitor_proc(void);
static void polling_proc(void);

/***************************************************************
 * Variables
 * ************************************************************/
#define DEV_NAME "memd"
#define BUFF_SIZE 1024
static unsigned int major_num = 0;
static unsigned int minor_num = 0;
static struct cdev memdev_cdev;

/***************************************************************
 * Char Device
 * ************************************************************/
static struct class *memdev_class =
	NULL; // デバイスドライバのクラスオブジェクト
enum MODE {
	M_NONE,
	M_READ,
	M_WRITE,
	M_MONITOR,
	M_POLLING,
};

static unsigned long g_addr;
static int g_mode = M_NONE;
static char g_buf[BUFF_SIZE];
static unsigned int g_readLength;
static unsigned int g_readCycle;
static unsigned int g_msleepCount = 1000;

static int cmdparse(char *k_buf)
{
	char cmd[32];
	unsigned long addr, value = 0, option = 0;
	int length, ret;
	int l_mode = M_NONE;
	ret = sscanf(k_buf, "%31s %d %lx %lx %lx", cmd, &length, &addr, &value,
		     &option);
	cmd[31] = '\0';
	PDEBUG("ret:    %d\n", ret);
	PDEBUG("cmd:    %s\n", cmd);
	PDEBUG("length: %d\n", length);
	PDEBUG("addr:   %08lx\n", addr);
	PDEBUG("value:  %lx\n", value);
	PDEBUG("option: %lx\n", option);
	switch (cmd[0]) {
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
		if (ret == 3)
			cmd_monitor(addr, length, 4);
		else if (ret == 4)
			cmd_monitor(addr, length, value);
		break;
	case 'p':
		l_mode = M_POLLING;
		if (ret == 3)
			cmd_polling(addr, length, 1000, 4); // 1000ms wait
		else if (ret == 4)
			cmd_polling(addr, length, 1000, value); // 1000ms wait
		else if (ret == 5)
			cmd_polling(addr, length, option, value);
		break;
	default:
		l_mode = M_NONE;
		printk("invalid argument\n");
		break;
	}
	return l_mode;
}

static void cmd_monitor(unsigned long addr, int value, int read_cycle)
{
	g_addr = addr;
	g_readLength = value;
	g_readCycle = read_cycle;
}

static int monitor_proc()
{
	char *reg;
	unsigned long addr_offset = 0; // for address alignment
	int loop;
	unsigned long val = 0;
	unsigned char l_buf[BUFF_SIZE];
	unsigned int l_length = 0;
	PDEBUG("%x %d %d\n", g_addr, g_readLength, g_readCycle);

	for (loop = 0; loop < BUFF_SIZE; ++loop)
		l_buf[loop] = '\0';

	addr_offset = g_addr % 4;
	if (addr_offset)
		l_length = g_readLength + 4;
	else
		l_length = g_readLength;
	reg = (char *)ioremap_nocache(g_addr - addr_offset, l_length);

	sprintf(l_buf, "[Address] value\n[%08x] ", g_addr);
	for (loop = 0; loop < l_length; loop++) {
		char str[100];
		val |= (unsigned long)ioread8(reg++)
		       << 8 * (loop % g_readCycle);
		if ((loop % g_readCycle) == (g_readCycle - 1)) {
			sprintf(str, "0x%0*lx ", g_readCycle * 2, val);
			strcat(l_buf, str);
			val = 0;
		}
		if (15 == (loop % 16)) {
			sprintf(str, "\n[%08x] ", g_addr + loop + 1);
			strcat(l_buf, str);
		}
	}
	strcat(l_buf, "\n");

	iounmap((void *)(g_addr - addr_offset));

	for (loop = 0; loop < BUFF_SIZE; ++loop) {
		if (g_buf[loop] != l_buf[loop]) {
			memcpy(g_buf, l_buf, BUFF_SIZE);
			return 1;
		}
		if (l_buf[loop] == '\0')
			break;
	}
	return 0;

	// if (2 != strcmp(g_buf, l_buf)) // not equal
	// {
	// 	// for (loop = 0; loop < BUFF_SIZE; ++loop)
	// 	// 	g_buf[loop] = l_buf[loop];
	// 	memcpy(g_buf, l_buf, BUFF_SIZE);
	// 	printk("detect\n");
	// 	return 1;
	// }
	// return 0;
}
static void cmd_polling(unsigned long addr, int value, int interval,
			int read_cycle)
{
	g_addr = addr;
	g_readLength = value;
	g_msleepCount = interval;
	g_readCycle = read_cycle;
}

static void polling_proc()
{
	char *reg;
	unsigned long addr_offset = 0; // for address alignment
	int loop;
	unsigned long val = 0;
	unsigned char l_buf[BUFF_SIZE];
	unsigned int l_length = 0;
	switch (g_readCycle) {
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
		break;
	default:
		g_readCycle = 4;
		break;
	}
	for (loop = 0; loop < BUFF_SIZE; ++loop)
		l_buf[loop] = '\0';

	addr_offset = g_addr % 4;
	if (addr_offset)
		l_length = g_readLength + 4;
	else
		l_length = g_readLength;
	reg = (char *)ioremap_nocache(g_addr - addr_offset, l_length);
	//
	sprintf(l_buf, "            ");
	for (loop = 0; loop < 16 / g_readCycle; ++loop) {
		char tmp[100];
		sprintf(tmp, " %*x", g_readCycle * 2 + 2, loop * g_readCycle);
		strcat(l_buf, tmp);
	}
	{
		char tmp[100];
		sprintf(tmp, "\n[0x%08lx] ", g_addr);
		strcat(l_buf, tmp);
	}

	for (loop = 0; loop < l_length; loop++) {
		char str[100];
		val |= (unsigned long)ioread8(reg++)
		       << 8 * (loop % g_readCycle);
		if ((loop % g_readCycle) == (g_readCycle - 1)) {
			sprintf(str, "0x%0*lx ", g_readCycle * 2, val);
			strcat(l_buf, str);
			val = 0;
		}
		if (loop == (l_length - 1))
			break;

		if (15 == (loop % 16)) {
			sprintf(str, "\n[0x%08lx] ", g_addr + loop + 1);
			strcat(l_buf, str);
		}
	}
	strcat(l_buf, "\n");

	iounmap((void *)(g_addr - addr_offset));
	memcpy(g_buf, l_buf, BUFF_SIZE);
}

static void cmd_write(unsigned long addr, unsigned long value, int length)
{
	// int loop;
	int write_size = 0;
	unsigned int old_value = 0, current_value = 0;
	char *reg;

	unsigned long addr_offset = 0; // for address alignment
	unsigned int tmp_value[2];
	unsigned char *p;
	unsigned int loop;

	addr_offset = addr % 4;

	// request_mem_region(addr, length + 1, DEV_NAME);
	reg = (char *)ioremap_nocache(addr - addr_offset, length + 4);
	// if length not equal 1, 2, 4.
	if (0 == length
	    || 1 < ((length & 0x01) + (length >> 1 & 0x01)
		    + (length >> 2 & 0x01))) {
		printk("invalid argument: length is only 1,2,4,or 8\n");
		return;
	} else
		write_size = length;

	PDEBUG("Write [%08lx] %0*lx write_size:%d[byte]\n", addr, length / 2,
	       value, write_size);
	if (1 == write_size) {
		old_value = ioread8(reg + addr_offset);
		tmp_value[0] = ioread32(reg);
		p = (unsigned char *)tmp_value;
		p[addr_offset] = value & 0xff;
		iowrite32(tmp_value[0], reg);
		current_value = ioread8((reg + addr_offset));
	} else if (2 == write_size || 4 == write_size) {
		p = (unsigned char *)&old_value;
		for (loop = 0; loop < write_size; ++loop)
			p[loop] = ioread8(reg + addr_offset + loop) & 0xff;
		tmp_value[0] = ioread32(reg);
		tmp_value[1] = ioread32(reg);
		p = (unsigned char *)tmp_value;
		for (loop = 0; loop < write_size; ++loop)
			p[addr_offset + loop] = (value >> 8 * loop) & 0xff;
		iowrite32(tmp_value[0], reg);
		iowrite32(tmp_value[1], reg + 4);
		p = (unsigned char *)&current_value;
		for (loop = 0; loop < write_size; ++loop)
			p[loop] = ioread8(reg + addr_offset + loop) & 0xff;
	}

	PDEBUG("[%08lx] %0*x -> %0*x\n", addr, length * 2, old_value,
	       length * 2, current_value);
	iounmap((void *)(addr - addr_offset));
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
	switch (_read_cycle) {
	case 1:
	case 2:
	case 4:
		read_cycle = _read_cycle;
		break;
	default:
		read_cycle = 4;
		break;
	}
	if (0 != length % read_cycle)
		length += read_cycle - (length % read_cycle);
	// request_mem_region(addr, length + 1, DEV_NAME);
	reg = (char *)ioremap_nocache(addr, length);
	PDEBUG("Read [%08lx] %d byte\n", addr, length);
	for (loop = 0; loop < length; loop++) {
		char str[100];
		val |= (unsigned long)ioread8(reg++) << 8 * (loop % read_cycle);
		if ((loop % read_cycle) == (read_cycle - 1)) {
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
	unsigned char dummy[BUFF_SIZE];
	for (i = 0; i < BUFF_SIZE; ++i)
		dummy[i] = '\0';
	PDEBUG("%s_read  : count: %d\n", DEV_NAME, (int)count);
	switch (g_mode) {
	case M_NONE:
		return 0;
	case M_READ:
		PDEBUG("M_READ\n");
		ret = copy_to_user(buff, g_buf, sizeof(g_buf));
		if (ret != 0)
			return -EFAULT;
		for (i = 0; i < BUFF_SIZE; ++i)
			g_buf[i] = '\0';
		g_mode = M_NONE;
		return sizeof(g_buf);
	case M_WRITE:
		return 0;
	case M_MONITOR:
		PDEBUG("M_MONITOR\n");
		ret = monitor_proc();
		if (ret == 1)
			ret = copy_to_user(buff, g_buf, sizeof(g_buf));
		else
			ret = copy_to_user(buff, dummy, sizeof(dummy));
		if (ret != 0)
			printk("EFAULT\n");
		if (ret != 0)
			return -EFAULT;
		msleep(1);
		return BUFF_SIZE;
	case M_POLLING:
		PDEBUG("M_POLLING\n");
		polling_proc();
		ret = copy_to_user(buff, g_buf, sizeof(g_buf));
		if (ret != 0)
			return -EFAULT;
		msleep(g_msleepCount);
		return BUFF_SIZE;
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
	if (ret > 0) {
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
 * Driver Initialize / Close
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
	if (ret != 0) {
		printk("  Error Init\n");
		return -1;
	}
	major_num = MAJOR(dev); // メジャー番号の格納
	minor_num = MINOR(dev); // マイナー番号の格納

	// devにcdevを埋め込む
	cdev_init(&memdev_cdev, &memdev_fops);
	memdev_cdev.owner = THIS_MODULE;
	ret = cdev_add(&memdev_cdev, MKDEV(major_num, minor_num), dev_count);
	if (ret != 0) {
		printk(KERN_ALERT "error cdev_add\n");
		unregister_chrdev_region(dev, dev_count);
		return -1;
	}

	// このデバイスのclassの登録
	memdev_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(memdev_class)) {
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
