#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/ctype.h>
#include <linux/console.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/rtc.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>
#include <linux/acer_kramlog.h>

MODULE_AUTHOR("Jay Hsieh");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Acer Kernel Ram Log");

extern unsigned int kramlog_phys;
extern unsigned int kramlog_size;

struct log_struct *record_log;
static int mem_cleaned = 0;
static int mem_ready = 0;

void kramlog_memory_ready(void)
{
    mem_ready = 1;
}
EXPORT_SYMBOL(kramlog_memory_ready);

static __inline__ void __kramlog_append_char(unsigned int index, unsigned char c)
{
	CHECK_INDEX(index);

	if (!mem_ready)
		return ;

	if (!mem_cleaned) {
		record_log = (void *) ACER_KRAMLOG_BASE;
		memset(record_log, 0, sizeof(struct log_struct) * KRAMLOG_MAX_NUM);
		mem_cleaned = 1;
	}

	if (c == 0 || c >= 128)
		c = ' ';
	(record_log + index)->buf[(record_log + index)->tail] = c;
	(record_log + index)->tail = ((record_log + index)->tail + 1) % (unsigned int) (BUF_SIZE);
}

void kramlog_append_kernel_raw_char(unsigned char c)
{
	__kramlog_append_char(KRAMLOG_KERNEL, c);
}
EXPORT_SYMBOL(kramlog_append_kernel_raw_char);

static __inline__ void __kramlog_append_str(unsigned int index, unsigned char *str, size_t len)
{
	int i = 0;

	for(i = 0; i < len; i++) {
		if (isprint(*(str + i)))
			__kramlog_append_char(index, *(str + i));
		else
			__kramlog_append_char(index, ' ');
	}
}

static __inline__ void __kramlog_append_time(unsigned int index)
{
	unsigned char tbuf[128];
	unsigned tlen = 0;
	struct timespec now;
	struct rtc_time tm;

// old time format, still keep codes
/*
	now = current_kernel_time();
	tlen = snprintf(tbuf, sizeof(tbuf), "[%8lx.%08lx] ", now.tv_sec, now.tv_nsec);
*/

	now = current_kernel_time();
	rtc_time_to_tm(now.tv_sec, &tm);
	tlen = snprintf(tbuf, sizeof(tbuf), "[%d-%02d-%02d %02d:%02d:%02d.%09lu] ",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
						tm.tm_hour, tm.tm_min, tm.tm_sec, now.tv_nsec);

	__kramlog_append_str(index, tbuf, tlen);

	return;
}

void kramlog_append_time(unsigned int index)
{
	if (!mem_ready)
		return ;

	CHECK_INDEX(index);

	__kramlog_append_time(index);
}
EXPORT_SYMBOL(kramlog_append_time);

static __inline__ void __kramlog_append_newline(unsigned int index)
{
	__kramlog_append_char(index, '\n');
}

static __inline__ void __kramlog_append_separator(unsigned int index)
{
	unsigned char *sp = " | ";

	__kramlog_append_str(index, sp, strlen(sp));
}

void kramlog_append_newline(unsigned int index)
{
	if (!mem_ready)
		return ;

	CHECK_INDEX(index);

	__kramlog_append_char(index, '\n');
}
EXPORT_SYMBOL(kramlog_append_newline);

void kramlog_append_android_log(unsigned int index,
				const unsigned char *priority,
				const char * const tag,
				const int tag_bytes,
				const char * const msg,
				const int msg_bytes)
{
	int prilen = 0;
	unsigned char pribuf[8];

	CHECK_INDEX(index);

	__kramlog_append_time(index);

	prilen = snprintf(pribuf, sizeof(pribuf), "<%u> ", (unsigned int)*priority);
	__kramlog_append_str(index, pribuf, prilen);

	__kramlog_append_str(index, (unsigned char *)tag, (unsigned int)tag_bytes);
	__kramlog_append_separator(index);

	__kramlog_append_str(index, (unsigned char *)msg, (unsigned int)msg_bytes);
	__kramlog_append_newline(index);
}
EXPORT_SYMBOL(kramlog_append_android_log);

void kramlog_append_android2_log(unsigned int index,
				const char * const msg,
				const int msg_bytes)
{
	CHECK_INDEX(index);

	__kramlog_append_str(index, (unsigned char *)msg, (unsigned int)msg_bytes);
}
EXPORT_SYMBOL(kramlog_append_android2_log);

static int __init kramlog_init(void)
{
	int retval = 0;

	pr_info("Acer Kernel RamLog Init\n");
	record_log = (void *) ACER_KRAMLOG_BASE;

	if (kramlog_size == 0) {
		pr_err("Acer Kernel RamLog: Init failed.\n");
		pr_err("Acer Kernel RamLog: CONFIG_BOARD_KRAMLOG_BASE does not match with modem.\n");
		BUG_ON(kramlog_size == 0);
		return retval;
	}

	snprintf((record_log + KRAMLOG_KERNEL)->name, NAME_SIZE, "%-29s\r\n", "Kernel_Log");
	snprintf((record_log + KRAMLOG_LOGCAT)->name, NAME_SIZE, "\r\n%-27s\r\n", "Logcat_Log");
	snprintf((record_log + KRAMLOG_SYSTEM)->name, NAME_SIZE, "\r\n%-27s\r\n", "System_Log");
	snprintf((record_log + KRAMLOG_RADIO)->name, NAME_SIZE, "\r\n%-27s\r\n", "Radio_Log");

	return retval;
}

static void __exit kramlog_exit(void)
{
	pr_info("Acer Kernel RamLog Exit\n");
}

module_init(kramlog_init);
module_exit(kramlog_exit);
