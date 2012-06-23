/* kernel/power/earlysuspend.c
 *
 * Copyright (C) 2005-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/syscalls.h> /* sys_sync */
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include "power.h"

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 2,
};
#if defined(CONFIG_MACH_ACER_A4) || defined(CONFIG_MACH_ACER_A5)
static int debug_mask = DEBUG_USER_STATE | DEBUG_SUSPEND;
#else
static int debug_mask = DEBUG_USER_STATE;
#endif  /* CONFIG_MACH_ACER_A4 || CONFIG_MACH_ACER_A5 */
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static DEFINE_MUTEX(early_suspend_lock);
static LIST_HEAD(early_suspend_handlers);
static void early_suspend(struct work_struct *work);
static void late_resume(struct work_struct *work);
static DECLARE_WORK(early_suspend_work, early_suspend);
static DECLARE_WORK(late_resume_work, late_resume);
static DEFINE_SPINLOCK(state_lock);
enum {
	SUSPEND_REQUESTED = 0x1,
	SUSPENDED = 0x2,
	SUSPEND_REQUESTED_AND_SUSPENDED = SUSPEND_REQUESTED | SUSPENDED,
};
static int state;

// create a work queue to monitor the "suspend" thread while DUT enter suspend
#if defined(CONFIG_MACH_ACER_A4) || defined(CONFIG_MACH_ACER_A5)
static int early_suspend_is_working = 0;

struct task_struct *suspend_thread_task = NULL;
EXPORT_SYMBOL(suspend_thread_task);

static void monitor_suspend_work_queue(struct work_struct *work);
static DECLARE_WORK(Monitor_Work_Queue, monitor_suspend_work_queue);
struct workqueue_struct *suspend_work_queue_monitored = NULL;

int is_suspend_mode(void)
{
	return !!(state & SUSPENDED);
}
EXPORT_SYMBOL(is_suspend_mode);

static void monitor_suspend_work_queue(struct work_struct *work)
{
	while (early_suspend_is_working) {
		msleep(10000);
		if (suspend_thread_task && !has_wake_lock(WAKE_LOCK_SUSPEND)) {
			pr_info("Suspend Thread is blocked.\r\n");
			read_lock(&tasklist_lock);
			sched_show_task(suspend_thread_task);
			read_unlock(&tasklist_lock);
		}
	}
}
#endif  // CONFIG_MACH_ACER_A4 || CONFIG_MACH_ACER_A5

void register_early_suspend(struct early_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&early_suspend_lock);
	list_for_each(pos, &early_suspend_handlers) {
		struct early_suspend *e;
		e = list_entry(pos, struct early_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	if ((state & SUSPENDED) && handler->suspend)
		handler->suspend(handler);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(register_early_suspend);

void unregister_early_suspend(struct early_suspend *handler)
{
	mutex_lock(&early_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(unregister_early_suspend);

static void early_suspend(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;

	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED)
		state |= SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("early_suspend: abort, state %d\n", state);
		mutex_unlock(&early_suspend_lock);
		goto abort;
	}

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("early_suspend: call handlers\n");
#if defined(CONFIG_MACH_ACER_A4) || defined(CONFIG_MACH_ACER_A5)
	set_user_nice(current, -5);
	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->suspend != NULL) {
			if (debug_mask & DEBUG_SUSPEND)
				pr_info("[SUSPEND_DEBUG] early suspend ... [0x%8x]\r\n", (unsigned int) pos->suspend);
			pos->suspend(pos);
		}
	}
#else  // CONFIG_MACH_ACER_A4 || CONFIG_MACH_ACER_A5
	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->suspend != NULL)
			pos->suspend(pos);
	}
#endif  // CONFIG_MACH_ACER_A4 || CONFIG_MACH_ACER_A5
	mutex_unlock(&early_suspend_lock);

	suspend_sys_sync_queue();
abort:
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED_AND_SUSPENDED)
		wake_unlock(&main_wake_lock);
	spin_unlock_irqrestore(&state_lock, irqflags);
}

static void late_resume(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;

	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPENDED)
		state &= ~SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("late_resume: abort, state %d\n", state);
		goto abort;
	}
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: call handlers\n");
#if defined(CONFIG_MACH_ACER_A4) || defined(CONFIG_MACH_ACER_A5)
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link) {
		if (pos->resume != NULL) {
			if (debug_mask & DEBUG_SUSPEND)
				pr_info("[SUSPEND_DEBUG] late resume ... [0x%8x]\r\n", (unsigned int) pos->resume);
			pos->resume(pos);
		}
	}
	set_user_nice(current, 0);
#else  // CONFIG_MACH_ACER_A4 || CONFIG_MACH_ACER_A5
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link)
		if (pos->resume != NULL)
			pos->resume(pos);
#endif  // CONFIG_MACH_ACER_A4 || CONFIG_MACH_ACER_A5
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: done\n");

#if defined(CONFIG_MACH_ACER_A4) || defined(CONFIG_MACH_ACER_A5)
	early_suspend_is_working = 0;
#endif

abort:
	mutex_unlock(&early_suspend_lock);
}

void request_suspend_state(suspend_state_t new_state)
{
	unsigned long irqflags;
	int old_sleep;

#if defined(CONFIG_MACH_ACER_A4) || defined(CONFIG_MACH_ACER_A5)
	if (suspend_work_queue_monitored == NULL)
		suspend_work_queue_monitored = create_singlethread_workqueue("monitor_suspend");
#endif

	spin_lock_irqsave(&state_lock, irqflags);
	old_sleep = state & SUSPEND_REQUESTED;
	if (debug_mask & DEBUG_USER_STATE) {
		struct timespec ts;
		struct rtc_time tm;
		getnstimeofday(&ts);
		rtc_time_to_tm(ts.tv_sec, &tm);
#if defined(CONFIG_MACH_ACER_A4) || defined(CONFIG_MACH_ACER_A5)
		pr_info("request_suspend_state: %s (%d->%d) at %lld "
			"(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC) pid=%d\n",
			new_state != PM_SUSPEND_ON ? "sleep" : "wakeup",
			requested_suspend_state, new_state,
			ktime_to_ns(ktime_get()),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec, current->pid);
#else
		pr_info("request_suspend_state: %s (%d->%d) at %lld "
			"(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			new_state != PM_SUSPEND_ON ? "sleep" : "wakeup",
			requested_suspend_state, new_state,
			ktime_to_ns(ktime_get()),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
#endif
	}
	if (!old_sleep && new_state != PM_SUSPEND_ON) {
		state |= SUSPEND_REQUESTED;
#if defined(CONFIG_MACH_ACER_A4) || defined(CONFIG_MACH_ACER_A5)
		early_suspend_is_working = 1;
		queue_work(suspend_work_queue_monitored, &Monitor_Work_Queue);
#endif
		queue_work(suspend_work_queue, &early_suspend_work);
	} else if (old_sleep && new_state == PM_SUSPEND_ON) {
		state &= ~SUSPEND_REQUESTED;
		wake_lock(&main_wake_lock);
		queue_work(suspend_work_queue, &late_resume_work);
	}
	requested_suspend_state = new_state;
	spin_unlock_irqrestore(&state_lock, irqflags);
}

suspend_state_t get_suspend_state(void)
{
	return requested_suspend_state;
}
