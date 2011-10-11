#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include "diagfwd.h"

static int proc_qxdm_log_read(struct file *file, char *buf, size_t size, loff_t *ppos);
static int proc_qxdm_log_write(struct file *file, const char *buf, size_t size, loff_t *ppos);

struct file_operations proc_qxdm_log_fops = {
	read:proc_qxdm_log_read,
	write:proc_qxdm_log_write,
};

static int proc_qxdm_log_read(struct file *file, char *buf, size_t size, loff_t *ppos)
{
	int end_p_now;
	int read_to_p;
	int buf_read = 0;
	int buf_available;

	end_p_now = end_p;
	if (start_p == end_p_now) {
		return buf_read;
	} else {
		buf_available = (end_p_now - start_p + CIR_BUF_LEN) % CIR_BUF_LEN;
		if (size < buf_available) {
			read_to_p = (start_p + size + CIR_BUF_LEN) % CIR_BUF_LEN;
		} else {
			read_to_p = end_p_now;
		}
		if (read_to_p > start_p) {
			if (copy_to_user(buf, circular_buf + start_p, read_to_p - start_p)) {
				goto copy_err;
			}
		} else {
			if (copy_to_user(buf, circular_buf + start_p, CIR_BUF_LEN - start_p)) {
				goto copy_err;
			}
			if (copy_to_user(buf + (CIR_BUF_LEN - start_p), circular_buf, read_to_p)) {
				goto copy_err;
			}
		}
		buf_read = ((read_to_p - start_p) + CIR_BUF_LEN) % CIR_BUF_LEN;
		start_p = read_to_p;
		return buf_read;
	}

copy_err:
	pr_err("proc_qxdm_log:copy_to_user error \n");
	return -1;
}

static int proc_qxdm_log_write(struct file *file, const char *buf, size_t size, loff_t *ppos)
{
	return 0;
}

static int __init init_qxdm_log(void)
{
	struct proc_dir_entry *p;
	p = create_proc_entry ("qxdm_log", S_IFREG | S_IRUGO | S_IWUGO, NULL);
	if (p)
		p->proc_fops = &proc_qxdm_log_fops;
	return 0;
}

static void __exit cleanup_qxdm_log(void)
{
	pr_debug("cleanup qxdm log\n");
	remove_proc_entry ("qxdm_log", NULL);
}

module_init(init_qxdm_log);
module_exit(cleanup_qxdm_log);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luke Lai (luke_lai@acer.com.tw)");
MODULE_DESCRIPTION("For reading QXDM log");
