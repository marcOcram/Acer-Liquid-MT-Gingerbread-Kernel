/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#define DEBUG 1

#include <linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/acer_battery_framework.h>

#include <asm/atomic.h>

#include <mach/msm_rpcrouter.h>
#include <mach/msm_battery.h>

#ifdef CONFIG_MACH_ACER_A4
#include <linux/acer_a4_vol_table.h>
#include <linux/i2c/smb136.h>
#endif

#if DEBUG
#define db_msg(x...) do {pr_info(x); } while (0)
#endif

#define BATTERY_RPC_PROG	0x30000089
#define BATTERY_RPC_VER_1_1	0x00010001
#define BATTERY_RPC_VER_2_1	0x00020001
#define BATTERY_RPC_VER_4_1     0x00040001
#define BATTERY_RPC_VER_5_1     0x00050001

#define BATTERY_RPC_CB_PROG	(BATTERY_RPC_PROG | 0x01000000)

#define BATTERY_REGISTER_PROC				2
#define BATTERY_MODIFY_CLIENT_PROC			4
#define BATTERY_DEREGISTER_CLIENT_PROC			5
#define BATTERY_READ_MV_PROC				12
#define BATTERY_ENABLE_DISABLE_FILTER_PROC		14

#define VBATT_FILTER			2

#define BATTERY_CB_TYPE_PROC		1
#define BATTERY_CB_ID_ALL_ACTIV		1
#define BATTERY_CB_ID_LOW_VOL		2

#define BATT_RPC_TIMEOUT    5000	/* 5 sec */

#define INVALID_BATT_HANDLE    -1

#define RPC_TYPE_REQ     0
#define RPC_REQ_REPLY_COMMON_HEADER_SIZE   (3 * sizeof(uint32_t))

struct _msm_batt_rpc_info {
	u32 batt_api_version;
	u32 chg_api_version;
	s32 batt_handle;

	struct msm_rpc_client *batt_client;

	u32 vbatt_modify_reply_avail;
	u32 voltage_max_design;
	u32 voltage_min_design;
};

static struct _msm_batt_rpc_info msm_batt_rpc_info = {
	.batt_handle = INVALID_BATT_HANDLE,
	.vbatt_modify_reply_avail = 0,
	.voltage_max_design = BATTERY_HIGH,
	.voltage_min_design = BATTERY_LOW,
};

/* acer battery framework */
static struct _batt_func batt_funcs = {0};

enum {
	BATTERY_REGISTRATION_SUCCESSFUL = 0,
	BATTERY_DEREGISTRATION_SUCCESSFUL = BATTERY_REGISTRATION_SUCCESSFUL,
	BATTERY_MODIFICATION_SUCCESSFUL = BATTERY_REGISTRATION_SUCCESSFUL,
	BATTERY_INTERROGATION_SUCCESSFUL = BATTERY_REGISTRATION_SUCCESSFUL,
	BATTERY_CLIENT_TABLE_FULL = 1,
	BATTERY_REG_PARAMS_WRONG = 2,
	BATTERY_DEREGISTRATION_FAILED = 4,
	BATTERY_MODIFICATION_FAILED = 8,
	BATTERY_INTERROGATION_FAILED = 16,
	/* Client's filter could not be set because perhaps it does not exist */
	BATTERY_SET_FILTER_FAILED         = 32,
	/* Client's could not be found for enabling or disabling the individual
	 * client */
	BATTERY_ENABLE_DISABLE_INDIVIDUAL_CLIENT_FAILED  = 64,
	BATTERY_LAST_ERROR = 128,
};

enum {
	BATTERY_VOLTAGE_UP = 0,
	BATTERY_VOLTAGE_DOWN,
	BATTERY_VOLTAGE_ABOVE_THIS_LEVEL,
	BATTERY_VOLTAGE_BELOW_THIS_LEVEL,
	BATTERY_VOLTAGE_LEVEL,
	BATTERY_ALL_ACTIVITY,
	VBATT_CHG_EVENTS,
	BATTERY_VOLTAGE_UNKNOWN,
};

struct msm_batt_get_volt_ret_data {
	u32 battery_voltage;
};

static int msm_batt_get_volt_ret_func(struct msm_rpc_client *batt_client,
				       void *buf, void *data)
{
	struct msm_batt_get_volt_ret_data *data_ptr, *buf_ptr;

	data_ptr = (struct msm_batt_get_volt_ret_data *)data;
	buf_ptr = (struct msm_batt_get_volt_ret_data *)buf;

	data_ptr->battery_voltage = be32_to_cpu(buf_ptr->battery_voltage);

	return 0;
}

static u32 msm_batt_get_vbatt_voltage(void)
{
	int rc;

	struct msm_batt_get_volt_ret_data rep;

	rc = msm_rpc_client_req(msm_batt_rpc_info.batt_client,
			BATTERY_READ_MV_PROC,
			NULL, NULL,
			msm_batt_get_volt_ret_func, &rep,
			msecs_to_jiffies(BATT_RPC_TIMEOUT));

	if (rc < 0) {
		pr_err("%s: FAIL: vbatt get volt. rc=%d\n", __func__, rc);
		return 0;
	}

	return rep.battery_voltage;
}

struct batt_modify_client_req {

	u32 client_handle;

	/* The voltage at which callback (CB) should be called. */
	u32 desired_batt_voltage;

	/* The direction when the CB should be called. */
	u32 voltage_direction;

	/* The registered callback to be called when voltage and
	 * direction specs are met. */
	u32 batt_cb_id;

	/* The call back data */
	u32 cb_data;
};

struct batt_modify_client_rep {
	u32 result;
};

static int msm_batt_modify_client_arg_func(struct msm_rpc_client *batt_client,
				       void *buf, void *data)
{
	struct batt_modify_client_req *batt_modify_client_req =
		(struct batt_modify_client_req *)data;
	u32 *req = (u32 *)buf;
	int size = 0;

	*req = cpu_to_be32(batt_modify_client_req->client_handle);
	size += sizeof(u32);
	req++;

	*req = cpu_to_be32(batt_modify_client_req->desired_batt_voltage);
	size += sizeof(u32);
	req++;

	*req = cpu_to_be32(batt_modify_client_req->voltage_direction);
	size += sizeof(u32);
	req++;

	*req = cpu_to_be32(batt_modify_client_req->batt_cb_id);
	size += sizeof(u32);
	req++;

	*req = cpu_to_be32(batt_modify_client_req->cb_data);
	size += sizeof(u32);

	return size;
}

static int msm_batt_modify_client_ret_func(struct msm_rpc_client *batt_client,
				       void *buf, void *data)
{
	struct  batt_modify_client_rep *data_ptr, *buf_ptr;

	data_ptr = (struct batt_modify_client_rep *)data;
	buf_ptr = (struct batt_modify_client_rep *)buf;

	data_ptr->result = be32_to_cpu(buf_ptr->result);

	return 0;
}

static int msm_batt_modify_client(u32 client_handle, u32 desired_batt_voltage,
	     u32 voltage_direction, u32 batt_cb_id, u32 cb_data)
{
	int rc;

	struct batt_modify_client_req  req;
	struct batt_modify_client_rep rep;

	req.client_handle = client_handle;
	req.desired_batt_voltage = desired_batt_voltage;
	req.voltage_direction = voltage_direction;
	req.batt_cb_id = batt_cb_id;
	req.cb_data = cb_data;

	rc = msm_rpc_client_req(msm_batt_rpc_info.batt_client,
			BATTERY_MODIFY_CLIENT_PROC,
			msm_batt_modify_client_arg_func, &req,
			msm_batt_modify_client_ret_func, &rep,
			msecs_to_jiffies(BATT_RPC_TIMEOUT));

	if (rc < 0) {
		pr_err("%s: ERROR. failed to modify  Vbatt client\n",
		       __func__);
		return rc;
	}

	if (rep.result != BATTERY_MODIFICATION_SUCCESSFUL) {
		pr_err("%s: ERROR. modify client failed. result = %u\n",
		       __func__, rep.result);
		return -EIO;
	}

	return 0;
}

void batt_early_suspend(void)
{
	int rc;

	if (msm_batt_rpc_info.batt_handle != INVALID_BATT_HANDLE) {
		rc = msm_batt_modify_client(msm_batt_rpc_info.batt_handle,
				BATTERY_LOW, BATTERY_VOLTAGE_BELOW_THIS_LEVEL,
				BATTERY_CB_ID_LOW_VOL, BATTERY_LOW);

		if (rc < 0) {
			pr_err("%s: msm_batt_modify_client. rc=%d\n",
			       __func__, rc);
			return;
		}
	} else {
		pr_err("%s: ERROR. invalid batt_handle\n", __func__);
		return;
	}
}

void batt_late_resume(void)
{
	int rc;

	if (msm_batt_rpc_info.batt_handle != INVALID_BATT_HANDLE) {
		rc = msm_batt_modify_client(msm_batt_rpc_info.batt_handle,
				BATTERY_LOW, BATTERY_ALL_ACTIVITY,
			       BATTERY_CB_ID_ALL_ACTIV, BATTERY_ALL_ACTIVITY);
		if (rc < 0) {
			pr_err("%s: msm_batt_modify_client FAIL rc=%d\n",
			       __func__, rc);
			return;
		}
	} else {
		pr_err("%s: ERROR. invalid batt_handle\n", __func__);
		return;
	}
}

struct msm_batt_vbatt_filter_req {
	u32 batt_handle;
	u32 enable_filter;
	u32 vbatt_filter;
};

struct msm_batt_vbatt_filter_rep {
	u32 result;
};

static int msm_batt_filter_arg_func(struct msm_rpc_client *batt_client,

		void *buf, void *data)
{
	struct msm_batt_vbatt_filter_req *vbatt_filter_req =
		(struct msm_batt_vbatt_filter_req *)data;
	u32 *req = (u32 *)buf;
	int size = 0;

	*req = cpu_to_be32(vbatt_filter_req->batt_handle);
	size += sizeof(u32);
	req++;

	*req = cpu_to_be32(vbatt_filter_req->enable_filter);
	size += sizeof(u32);
	req++;

	*req = cpu_to_be32(vbatt_filter_req->vbatt_filter);
	size += sizeof(u32);
	return size;
}

static int msm_batt_filter_ret_func(struct msm_rpc_client *batt_client,
				       void *buf, void *data)
{

	struct msm_batt_vbatt_filter_rep *data_ptr, *buf_ptr;

	data_ptr = (struct msm_batt_vbatt_filter_rep *)data;
	buf_ptr = (struct msm_batt_vbatt_filter_rep *)buf;

	data_ptr->result = be32_to_cpu(buf_ptr->result);
	return 0;
}

static int msm_batt_enable_filter(u32 vbatt_filter)
{
	int rc;
	struct  msm_batt_vbatt_filter_req  vbatt_filter_req;
	struct  msm_batt_vbatt_filter_rep  vbatt_filter_rep;

	vbatt_filter_req.batt_handle = msm_batt_rpc_info.batt_handle;
	vbatt_filter_req.enable_filter = 1;
	vbatt_filter_req.vbatt_filter = vbatt_filter;

	rc = msm_rpc_client_req(msm_batt_rpc_info.batt_client,
			BATTERY_ENABLE_DISABLE_FILTER_PROC,
			msm_batt_filter_arg_func, &vbatt_filter_req,
			msm_batt_filter_ret_func, &vbatt_filter_rep,
			msecs_to_jiffies(BATT_RPC_TIMEOUT));

	if (rc < 0) {
		pr_err("%s: FAIL: enable vbatt filter. rc=%d\n",
		       __func__, rc);
		return rc;
	}

	if (vbatt_filter_rep.result != BATTERY_DEREGISTRATION_SUCCESSFUL) {
		pr_err("%s: FAIL: enable vbatt filter: result=%d\n",
		       __func__, vbatt_filter_rep.result);
		return -EIO;
	}
	return rc;
}

struct batt_client_registration_req {
	/* The voltage at which callback (CB) should be called. */
	u32 desired_batt_voltage;

	/* The direction when the CB should be called. */
	u32 voltage_direction;

	/* The registered callback to be called when voltage and
	 * direction specs are met. */
	u32 batt_cb_id;

	/* The call back data */
	u32 cb_data;
	u32 more_data;
	u32 batt_error;
};

struct batt_client_registration_req_4_1 {
	/* The voltage at which callback (CB) should be called. */
	u32 desired_batt_voltage;

	/* The direction when the CB should be called. */
	u32 voltage_direction;

	/* The registered callback to be called when voltage and
	 * direction specs are met. */
	u32 batt_cb_id;

	/* The call back data */
	u32 cb_data;
	u32 batt_error;
};

struct batt_client_registration_rep {
	u32 batt_handle;
};

struct batt_client_registration_rep_4_1 {
	u32 batt_handle;
	u32 more_data;
	u32 err;
};

static int msm_batt_register_arg_func(struct msm_rpc_client *batt_client,
				       void *buf, void *data)
{
	struct batt_client_registration_req *batt_reg_req =
		(struct batt_client_registration_req *)data;

	u32 *req = (u32 *)buf;
	int size = 0;


	if (msm_batt_rpc_info.batt_api_version == BATTERY_RPC_VER_4_1) {
		*req = cpu_to_be32(batt_reg_req->desired_batt_voltage);
		size += sizeof(u32);
		req++;

		*req = cpu_to_be32(batt_reg_req->voltage_direction);
		size += sizeof(u32);
		req++;

		*req = cpu_to_be32(batt_reg_req->batt_cb_id);
		size += sizeof(u32);
		req++;

		*req = cpu_to_be32(batt_reg_req->cb_data);
		size += sizeof(u32);
		req++;

		*req = cpu_to_be32(batt_reg_req->batt_error);
		size += sizeof(u32);

		return size;
	} else {
		*req = cpu_to_be32(batt_reg_req->desired_batt_voltage);
		size += sizeof(u32);
		req++;

		*req = cpu_to_be32(batt_reg_req->voltage_direction);
		size += sizeof(u32);
		req++;

		*req = cpu_to_be32(batt_reg_req->batt_cb_id);
		size += sizeof(u32);
		req++;

		*req = cpu_to_be32(batt_reg_req->cb_data);
		size += sizeof(u32);
		req++;

		*req = cpu_to_be32(batt_reg_req->more_data);
		size += sizeof(u32);
		req++;

		*req = cpu_to_be32(batt_reg_req->batt_error);
		size += sizeof(u32);

		return size;
	}

}

static int msm_batt_register_ret_func(struct msm_rpc_client *batt_client,
				       void *buf, void *data)
{
	struct batt_client_registration_rep *data_ptr, *buf_ptr;
	struct batt_client_registration_rep_4_1 *data_ptr_4_1, *buf_ptr_4_1;

	if (msm_batt_rpc_info.batt_api_version == BATTERY_RPC_VER_4_1) {
		data_ptr_4_1 = (struct batt_client_registration_rep_4_1 *)data;
		buf_ptr_4_1 = (struct batt_client_registration_rep_4_1 *)buf;

		data_ptr_4_1->batt_handle
			= be32_to_cpu(buf_ptr_4_1->batt_handle);
		data_ptr_4_1->more_data
			= be32_to_cpu(buf_ptr_4_1->more_data);
		data_ptr_4_1->err = be32_to_cpu(buf_ptr_4_1->err);
		return 0;
	} else {
		data_ptr = (struct batt_client_registration_rep *)data;
		buf_ptr = (struct batt_client_registration_rep *)buf;

		data_ptr->batt_handle = be32_to_cpu(buf_ptr->batt_handle);
		return 0;
	}
}

static int msm_batt_register(u32 desired_batt_voltage,
			     u32 voltage_direction, u32 batt_cb_id, u32 cb_data)
{
	struct batt_client_registration_req batt_reg_req;
	struct batt_client_registration_req_4_1 batt_reg_req_4_1;
	struct batt_client_registration_rep batt_reg_rep;
	struct batt_client_registration_rep_4_1 batt_reg_rep_4_1;
	void *request;
	void *reply;
	int rc;

	if (msm_batt_rpc_info.batt_api_version == BATTERY_RPC_VER_4_1) {
		batt_reg_req_4_1.desired_batt_voltage = desired_batt_voltage;
		batt_reg_req_4_1.voltage_direction = voltage_direction;
		batt_reg_req_4_1.batt_cb_id = batt_cb_id;
		batt_reg_req_4_1.cb_data = cb_data;
		batt_reg_req_4_1.batt_error = 1;
		request = &batt_reg_req_4_1;
	} else {
		batt_reg_req.desired_batt_voltage = desired_batt_voltage;
		batt_reg_req.voltage_direction = voltage_direction;
		batt_reg_req.batt_cb_id = batt_cb_id;
		batt_reg_req.cb_data = cb_data;
		batt_reg_req.more_data = 1;
		batt_reg_req.batt_error = 0;
		request = &batt_reg_req;
	}

	if (msm_batt_rpc_info.batt_api_version == BATTERY_RPC_VER_4_1)
		reply = &batt_reg_rep_4_1;
	else
		reply = &batt_reg_rep;

	rc = msm_rpc_client_req(msm_batt_rpc_info.batt_client,
			BATTERY_REGISTER_PROC,
			msm_batt_register_arg_func, request,
			msm_batt_register_ret_func, reply,
			msecs_to_jiffies(BATT_RPC_TIMEOUT));

	if (rc < 0) {
		pr_err("%s: FAIL: vbatt register. rc=%d\n", __func__, rc);
		return rc;
	}

	if (msm_batt_rpc_info.batt_api_version == BATTERY_RPC_VER_4_1) {
		if (batt_reg_rep_4_1.more_data != 0
			&& batt_reg_rep_4_1.err
				!= BATTERY_REGISTRATION_SUCCESSFUL) {
			pr_err("%s: vBatt Registration Failed proc_num=%d\n"
					, __func__, BATTERY_REGISTER_PROC);
			return -EIO;
		}
		msm_batt_rpc_info.batt_handle = batt_reg_rep_4_1.batt_handle;
	} else
		msm_batt_rpc_info.batt_handle = batt_reg_rep.batt_handle;

	return 0;
}

struct batt_client_deregister_req {
	u32 batt_handle;
};

struct batt_client_deregister_rep {
	u32 batt_error;
};

static int msm_batt_deregister_arg_func(struct msm_rpc_client *batt_client,
				       void *buf, void *data)
{
	struct batt_client_deregister_req *deregister_req =
		(struct  batt_client_deregister_req *)data;
	u32 *req = (u32 *)buf;
	int size = 0;

	*req = cpu_to_be32(deregister_req->batt_handle);
	size += sizeof(u32);

	return size;
}

static int msm_batt_deregister_ret_func(struct msm_rpc_client *batt_client,
				       void *buf, void *data)
{
	struct batt_client_deregister_rep *data_ptr, *buf_ptr;

	data_ptr = (struct batt_client_deregister_rep *)data;
	buf_ptr = (struct batt_client_deregister_rep *)buf;

	data_ptr->batt_error = be32_to_cpu(buf_ptr->batt_error);

	return 0;
}

static int msm_batt_deregister(u32 batt_handle)
{
	int rc;
	struct batt_client_deregister_req req;
	struct batt_client_deregister_rep rep;

	req.batt_handle = batt_handle;

	rc = msm_rpc_client_req(msm_batt_rpc_info.batt_client,
			BATTERY_DEREGISTER_CLIENT_PROC,
			msm_batt_deregister_arg_func, &req,
			msm_batt_deregister_ret_func, &rep,
			msecs_to_jiffies(BATT_RPC_TIMEOUT));

	if (rc < 0) {
		pr_err("%s: FAIL: vbatt deregister. rc=%d\n", __func__, rc);
		return rc;
	}

	if (rep.batt_error != BATTERY_DEREGISTRATION_SUCCESSFUL) {
		pr_err("%s: vbatt deregistration FAIL. error=%d, handle=%d\n",
		       __func__, rep.batt_error, batt_handle);
		return -EIO;
	}

	return 0;
}

static int msm_batt_cleanup(void)
{
	int rc = 0;

	if (msm_batt_rpc_info.batt_handle != INVALID_BATT_HANDLE) {
		rc = msm_batt_deregister(msm_batt_rpc_info.batt_handle);
		if (rc < 0)
			pr_err("%s: FAIL: msm_batt_deregister. rc=%d\n",
			       __func__, rc);
	}

	msm_batt_rpc_info.batt_handle = INVALID_BATT_HANDLE;

	if (msm_batt_rpc_info.batt_client)
		msm_rpc_unregister_client(msm_batt_rpc_info.batt_client);

	return rc;
}

#ifdef CONFIG_MACH_ACER_A4

#define SUSPEND_CHANGE_TIME  5400  /* 5400s */
#define AC_CHANGE_TIME  150  /* 150s */
#define NOT_AC_CHANGE_TIME  300  /* 300s */
#define GREATER_THAN_90_CHANGE_TIME  20 * 60 /* 20 minutes */

static u32 module_enable_flag; /* for batt_module_enable() */
static u32 first_capacity_after_boot;
static u32 base_battery_capacity;
static u32 pre_battery_capacity;
static u32 pre_chg_type = CHARGER_TYPE_NONE;
static struct timespec current_time;
static struct timespec change_time;
static void (*battery_isr_hander)(unsigned int flag);

void batt_module_enable(int module, bool state)
{
#if DEBUG
	db_msg("batt_module_enable:%d -> %d\n", module, state);
#endif
	if (state)
		module_enable_flag |= module;
	else
		module_enable_flag &= ~module;
	pr_info("batt:module_enable_flag:%d\n",module_enable_flag);
}

static int inline get_charger_type(void)
{
	if (!batt_funcs.get_charger_type) {
		pr_err("battery:: get charger type function error!!\n");
		pr_err("set charger type to USB!!\n");
		return	CHARGER_TYPE_USB_PC;
	} else
		return batt_funcs.get_charger_type();
}

static u32 acer_batt_calculate_capacity(u32 current_voltage, u32 current_case)
{
	u32 current_capacity = 0;
	static u32 shutdown_counter;
	acer_battery_table_type batt_lookup_table[MAX_VOLT_TABLE_SIZE] = { {0, 0} };

	int index;

	if (current_case == BATT_CHG_25C_900mA)
		memcpy(batt_lookup_table, acer_battery_chg_25C_900mA_table,
				MAX_VOLT_TABLE_SIZE*sizeof(acer_battery_table_type));
	else if (current_case == BATT_CHG_25C_500mA)
		memcpy(batt_lookup_table, acer_battery_chg_25C_500mA_table,
				MAX_VOLT_TABLE_SIZE*sizeof(acer_battery_table_type));
	else if (current_case == BATT_DSG_25C_500mA)
		memcpy(batt_lookup_table, acer_battery_dsg_25C_500mA_table,
				MAX_VOLT_TABLE_SIZE*sizeof(acer_battery_table_type));
	else if (current_case == BATT_DSG_25C_300mA)
		memcpy(batt_lookup_table, acer_battery_dsg_25C_300mA_table,
				MAX_VOLT_TABLE_SIZE*sizeof(acer_battery_table_type));
	else if (current_case == BATT_DSG_25C_100mA)
		memcpy(batt_lookup_table, acer_battery_dsg_25C_100mA_table,
				MAX_VOLT_TABLE_SIZE*sizeof(acer_battery_table_type));
	else if (current_case == BATT_DSG_25C_10mA)
		memcpy(batt_lookup_table, acer_battery_dsg_25C_10mA_table,
				MAX_VOLT_TABLE_SIZE*sizeof(acer_battery_table_type));

	if (current_voltage > 3500) {
		for (index = 0; index < MAX_VOLT_TABLE_SIZE-1; index++) {
			if (current_voltage >= batt_lookup_table[0].voltage) {
				current_capacity = 100;
				break;
			}

			if ((current_voltage < batt_lookup_table[index].voltage) &&
				(current_voltage >= batt_lookup_table[index+1].voltage)) {
				current_capacity = batt_lookup_table[index].capacity;
				break;
			}
		}

		shutdown_counter = 0;
	} else {
		shutdown_counter = shutdown_counter + 1;
		if (shutdown_counter >= 2)
			current_capacity = 0;
		else
			current_capacity = 5;
		pr_info("BATT:shutdown_counter:%d\n", shutdown_counter);
	}

	return current_capacity;
}

static u32 acer_batt_capacity_scheme(u32 pre_chg_type, u32 chg_type,
				u32 pre_batt_capacity, u32 batt_capacity, u32 batt_table)
{
	static int change_counter;
	u32 transfer_batt_capacity = 0;
	unsigned long int delta_time = 0, change_capacity_time = 0;
#if DEBUG
	db_msg("BATT:pre_chg_type:%d chg_type:%d\n", pre_chg_type, chg_type);
	db_msg("BATT:pre_batt_capacity:%d batt_capacity:%d\n",
		pre_batt_capacity, batt_capacity);
#endif

	/* if chager on and is_full_charging(from smb136), charging full)*/
	if ((chg_type != CHARGER_TYPE_NONE) && (pre_batt_capacity >= 90)) {
		if (smb136_control(RD_FULL_CHARGING, 0)) {
			transfer_batt_capacity = 100;
			base_battery_capacity = transfer_batt_capacity;
			pr_info("BATT: Charging full\n");
			goto done;
		} else {
			if (batt_capacity == 100) {
				pr_info("BATT:voltage >= 100%%.\n");
				batt_capacity = 95;
			}
		}
	}

	if (pre_chg_type != chg_type) {
		base_battery_capacity = pre_batt_capacity;
		transfer_batt_capacity = pre_batt_capacity;
		pr_info("BATT:base_battery_capacity:%d\n",
			base_battery_capacity);
		change_counter = 0;
		goto done;
	} else {
		current_time = current_kernel_time();
		delta_time = current_time.tv_sec - change_time.tv_sec;
#if DEBUG
		db_msg("delta_time: %ld\n", delta_time);
#endif

		if (delta_time > SUSPEND_CHANGE_TIME) {
			change_time = current_kernel_time();

			base_battery_capacity = batt_capacity;
			transfer_batt_capacity = batt_capacity;
			goto filter;
		} else {
			if (base_battery_capacity == batt_capacity) {
				transfer_batt_capacity = batt_capacity;
				goto filter;
			} else {
				/* for AC, increase 5%: 300s; for USB, increase 5%: 540s;
				get half time: 150s and 300s to avoid gapping */
				if (batt_table == BATT_CHG_25C_900mA)
#ifdef CONFIG_ENABLE_ONE_PERCENT_BATTERY_STEPS
					change_capacity_time = AC_CHANGE_TIME/5;
#else
					change_capacity_time = AC_CHANGE_TIME;
#endif
				else
#ifdef CONFIG_ENABLE_ONE_PERCENT_BATTERY_STEPS
					change_capacity_time = NOT_AC_CHANGE_TIME/5;
#else
					change_capacity_time = NOT_AC_CHANGE_TIME;
#endif

				if ((chg_type != CHARGER_TYPE_NONE) &&
					(base_battery_capacity >= 90))
#ifdef CONFIG_ENABLE_ONE_PERCENT_BATTERY_STEPS
						change_capacity_time = GREATER_THAN_90_CHANGE_TIME/5;
#else
						change_capacity_time = GREATER_THAN_90_CHANGE_TIME;
#endif

				if ((delta_time > change_capacity_time) &&
					((change_counter >= 2) || (change_counter <= -2))) {
					change_time = current_kernel_time();

					if (change_counter <= -2)
#ifdef CONFIG_ENABLE_ONE_PERCENT_BATTERY_STEPS
						transfer_batt_capacity = base_battery_capacity - 1;
#else
						transfer_batt_capacity = base_battery_capacity - 5;
#endif
					else
#ifdef CONFIG_ENABLE_ONE_PERCENT_BATTERY_STEPS
						transfer_batt_capacity = base_battery_capacity + 1;
#else
						transfer_batt_capacity = base_battery_capacity + 5;
#endif

					base_battery_capacity = transfer_batt_capacity;
					change_counter  = 0;
				} else {
					if (pre_batt_capacity > batt_capacity)
						change_counter--;
					else if (pre_batt_capacity < batt_capacity)
						change_counter++;

					transfer_batt_capacity = pre_batt_capacity;
				}
			}
		}
	}
#if DEBUG
	db_msg("BATT:change_count=%d, base_battery_capacity=%d\n",
		change_counter, base_battery_capacity);
	db_msg("transfer_batt_capacity = %d\n", transfer_batt_capacity);
#endif

filter:
	/* Avoid capacity increasing/decreasing when not charging/charging */
	if (smb136_control(RD_IS_CHARGING, 0)) {
		if (transfer_batt_capacity < pre_batt_capacity) {
			transfer_batt_capacity = pre_batt_capacity;
			base_battery_capacity = transfer_batt_capacity;
		}
	} else {
		if (transfer_batt_capacity > pre_batt_capacity) {
			transfer_batt_capacity = pre_batt_capacity;
			base_battery_capacity = transfer_batt_capacity;
		}
	}

done:
	return transfer_batt_capacity;
}

/* update battery information after usb driver is ready(wait 10s) */
static int batt_update_thread(void *ptr)
{
	pr_info("battery information updating thread start...\n");
	msleep(10000);

	if (battery_isr_hander)
		battery_isr_hander(FLAG_BATT_CAP_CHANGE);
	return 0;
}

static u32 msm_batt_capacity(u32 current_voltage)
{
	u32 cur_battery_capacity;
	u32 cal_battery_capacity;
	u32 chg_type;
	u32 batt_chg_case;

	chg_type = get_charger_type();

	if (chg_type == CHARGER_TYPE_WALL) {
		batt_chg_case = BATT_CHG_25C_900mA;
		pr_info("BATT:charger type:AC.\n");
	} else if (chg_type == CHARGER_TYPE_USB_PC){
		batt_chg_case = BATT_CHG_25C_500mA;
		pr_info("BATT:charger type:USB.\n");
	} else {
		/* not charging */
		/* if any module is enabled, use 300mA table */
		if (module_enable_flag)
			batt_chg_case = BATT_DSG_25C_300mA;
		else
			batt_chg_case = BATT_DSG_25C_100mA;
	}
#if DEBUG
	db_msg("\nbat voltage table index:%d\n", batt_chg_case);
#endif

	/* current_voltage > 10000, means the charger is connected
	 * when power button is pressed.*/
	if (current_voltage > 10000) {
		current_voltage -= 10000;

		/* if the charger is removed before booting voltage changing,
		 * we just guess the charger type is USB.*/
		if (chg_type == CHARGER_TYPE_NONE) {
			batt_chg_case = BATT_CHG_25C_500mA;
			pr_info("BATT: charger is removed, Using USB table.\n");
		} else
			pr_info("BATT: Booting with charger type:%d\n", chg_type);
	} else {
		/* if the charger is inserted after booting,
		 * we must change the charger type to Battery at first capacity reading.
		 */
		if (first_capacity_after_boot == 1 &&
			chg_type != CHARGER_TYPE_NONE) {
			pr_info("BATT:charger is inserted, Using Battery table.\n");
			batt_chg_case = BATT_DSG_25C_100mA;
		}
	}

	cur_battery_capacity =
		acer_batt_calculate_capacity(current_voltage, batt_chg_case);

	if (first_capacity_after_boot == 1) {
		change_time = current_kernel_time();

		base_battery_capacity = cur_battery_capacity;
		pre_battery_capacity = cur_battery_capacity;
		first_capacity_after_boot++;
	}

	if (cur_battery_capacity != 0) {
		cal_battery_capacity =
				acer_batt_capacity_scheme(pre_chg_type,	chg_type,
					pre_battery_capacity, cur_battery_capacity, batt_chg_case);
	} else
		cal_battery_capacity = 0;

	pre_chg_type = chg_type;
	pre_battery_capacity = cal_battery_capacity;

	pr_info("battery cap:%d%%(%d%%) voltage:%d mv\n",
		cal_battery_capacity, cur_battery_capacity, current_voltage);
	return cal_battery_capacity;
}
#else
static u32 msm_batt_capacity(u32 current_voltage)
{
	u32 low_voltage = msm_batt_rpc_info.voltage_min_design;
	u32 high_voltage = msm_batt_rpc_info.voltage_max_design;

	if (current_voltage <= low_voltage)
		return 0;
	else if (current_voltage >= high_voltage)
		return 100;
	else
		return (current_voltage - low_voltage) * 100
			/ (high_voltage - low_voltage);
}
#endif

static int msm_batt_cb_func(struct msm_rpc_client *client,
			    void *buffer, int in_size)
{
	int rc = 0;
	struct rpc_request_hdr *req;
	u32 procedure;
	u32 accept_status;

	req = (struct rpc_request_hdr *)buffer;
	procedure = be32_to_cpu(req->procedure);

	switch (procedure) {
	case BATTERY_CB_TYPE_PROC:
		accept_status = RPC_ACCEPTSTAT_SUCCESS;
		break;

	default:
		accept_status = RPC_ACCEPTSTAT_PROC_UNAVAIL;
		pr_err("%s: ERROR. procedure (%d) not supported\n",
		       __func__, procedure);
		break;
	}

	msm_rpc_start_accepted_reply(msm_batt_rpc_info.batt_client,
			be32_to_cpu(req->xid), accept_status);

	rc = msm_rpc_send_accepted_reply(msm_batt_rpc_info.batt_client, 0);
	if (rc)
		pr_err("%s: FAIL: sending reply. rc=%d\n", __func__, rc);

	return rc;
}

static int report_batt_cap(struct _batt_info *batt)
{
	static int pre_voltage;
	static int pre_capacity;

	if (!batt) {
		pr_err("get_batt_cap: Null pointer!!\n");
		return -EINVAL;
	}

#ifdef CONFIG_MACH_ACER_A4
	if (first_capacity_after_boot == 0) {
		kthread_run(batt_update_thread, NULL, "update_battery");
		batt->temperature = 0;
		batt->voltage = BATTERY_HIGH;
		batt->cap_percent = 100;
		first_capacity_after_boot++;
		return 0;
	}
#endif

	batt->temperature = 0; /*fake temperature*/
	batt->voltage = msm_batt_get_vbatt_voltage();
	if (pre_voltage != batt->voltage) {
		batt->cap_percent = msm_batt_capacity(batt->voltage);
		if (batt->voltage > 10000)
			batt->voltage -= 10000;
		pre_voltage = batt->voltage;
		pre_capacity = batt->cap_percent;
	} else
		batt->cap_percent = pre_capacity;

	return 0;
}

static int __devinit batt_init(void)
{
	int rc = 0;

	/* init rpc */
	msm_batt_rpc_info.batt_client =
		msm_rpc_register_client("battery", BATTERY_RPC_PROG,
					BATTERY_RPC_VER_4_1,
					1, msm_batt_cb_func);

	if (msm_batt_rpc_info.batt_client == NULL) {
		pr_err("%s: FAIL: rpc_register_client. batt_client=NULL\n",
		       __func__);
		return -ENODEV;
	}
	if (IS_ERR(msm_batt_rpc_info.batt_client)) {
		msm_batt_rpc_info.batt_client =
			msm_rpc_register_client("battery", BATTERY_RPC_PROG,
						BATTERY_RPC_VER_1_1,
						1, msm_batt_cb_func);
		msm_batt_rpc_info.batt_api_version =  BATTERY_RPC_VER_1_1;
	}
	if (IS_ERR(msm_batt_rpc_info.batt_client)) {
		msm_batt_rpc_info.batt_client =
			msm_rpc_register_client("battery", BATTERY_RPC_PROG,
						BATTERY_RPC_VER_2_1,
						1, msm_batt_cb_func);
		msm_batt_rpc_info.batt_api_version =  BATTERY_RPC_VER_2_1;
	}
	if (IS_ERR(msm_batt_rpc_info.batt_client)) {
		msm_batt_rpc_info.batt_client =
			msm_rpc_register_client("battery", BATTERY_RPC_PROG,
						BATTERY_RPC_VER_5_1,
						1, msm_batt_cb_func);
		msm_batt_rpc_info.batt_api_version =  BATTERY_RPC_VER_5_1;
	}
	if (IS_ERR(msm_batt_rpc_info.batt_client)) {
		rc = PTR_ERR(msm_batt_rpc_info.batt_client);
		pr_err("%s: ERROR: rpc_register_client: rc = %d\n ",
		       __func__, rc);
		msm_batt_rpc_info.batt_client = NULL;
		return rc;
	}

	rc = msm_batt_register(BATTERY_LOW, BATTERY_ALL_ACTIVITY,
			       BATTERY_CB_ID_ALL_ACTIV, BATTERY_ALL_ACTIVITY);
	if (rc < 0) {
		pr_err(
			"%s: msm_batt_register failed rc = %d\n", __func__, rc);
		msm_batt_cleanup();
		return rc;
	}

	rc =  msm_batt_enable_filter(VBATT_FILTER);

	if (rc < 0) {
		pr_err(
			"%s: msm_batt_enable_filter failed rc = %d\n",
			__func__, rc);
		msm_batt_cleanup();
		return rc;
	}

	/* connect with acer battery framework */
	batt_funcs.get_battery_info = report_batt_cap;
	batt_funcs.early_suspend = batt_early_suspend;
	batt_funcs.late_resume = batt_late_resume;
	register_bat_func(&batt_funcs);

#ifdef CONFIG_MACH_ACER_A4
	battery_isr_hander = batt_funcs.battery_isr_hander;
#endif
	return rc;
}

static int __init acer_batt_init(void)
{
	int rc;
	rc = batt_init();

	if (rc < 0) {
		pr_err("%s: FAIL: msm_batt_init_rpc.  rc=%d\n", __func__, rc);
		msm_batt_cleanup();
		return rc;
	}
	return 0;
}

static void __exit acer_batt_exit(void)
{
}

module_init(acer_batt_init);
module_exit(acer_batt_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jacob Lee");
MODULE_DESCRIPTION("Battery driver for Qualcomm MSM chipsets.");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:msm_battery");
