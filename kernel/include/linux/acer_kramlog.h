#ifndef ACER_KRAMLOG_H
#define ACER_KRAMLOG_H

#include <mach/msm_iomap.h>

enum kramlog_index {
	KRAMLOG_KERNEL = 0,
	KRAMLOG_LOGCAT,
	KRAMLOG_SYSTEM,
	KRAMLOG_RADIO,
	KRAMLOG_MAX_NUM
};

#define	NAME_SIZE	32
#define	BUF_SIZE	(ACER_KRAMLOG_SIZE / KRAMLOG_MAX_NUM - NAME_SIZE - 4)

#define CHECK_INDEX(i) \
	do { \
		if( i < KRAMLOG_KERNEL || i >= KRAMLOG_MAX_NUM ) { \
			pr_err("[KRAMLOG] [%s] index is out of range.\r\n", __FUNCTION__); \
			return ; \
		} \
	} while(0)

struct log_struct {
	unsigned char name[NAME_SIZE];
	unsigned char buf[BUF_SIZE];
	unsigned int tail;
};

#if defined(CONFIG_BOARD_KRAMLOG_BASE) && defined(CONFIG_BOARD_KRAMLOG_SIZE)
void kramlog_memory_ready(void);
void kramlog_append_kernel_raw_char(unsigned char c);
void kramlog_append_time(unsigned int index);
void kramlog_append_newline(unsigned int index);
void kramlog_append_android_log(unsigned int index,
				const unsigned char *priority,
				const char * const tag,
				const int tag_bytes,
				const char * const msg,
				const int msg_bytes);
void kramlog_append_android2_log(unsigned int index,
				const char * const msg,
				const int msg_bytes);
#endif  // defined(CONFIG_BOARD_KRAMLOG_BASE) && defined(CONFIG_BOARD_KRAMLOG_SIZE)

#endif  // ACER_KRAMLOG_H
