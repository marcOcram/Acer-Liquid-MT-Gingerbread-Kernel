#include "fast_dormancy.h"
#include <net/tcp.h>

#define ACER_DBG(fmt, arg...) do {} while (0)

int fast_dormancy_enable(void)
{

	int rc = 0;
	static struct msm_rpc_endpoint *ipfilter_endpoint = NULL;
	dormancy_request req_packet;
	dormancy_reply  rep_packet;

	if (!ipfilter_endpoint) {
		ipfilter_endpoint = msm_rpc_connect(CMPROG, CMVERS, 0);
		if (IS_ERR(ipfilter_endpoint)) {
			printk(KERN_INFO "fast dormancy: cannot connect to rpc server!\n");
			ACER_DBG("fast dormancy: cannot connect to rpc server!\n");
			ipfilter_endpoint = NULL;
			return -EIO;
		}
	}

	ACER_DBG("Enable Fast_Domancy!!\n");

	memset(&req_packet, 0x00, sizeof(req_packet));

	req_packet.cb_id = MSM_RPC_CLIENT_NULL_CB_ID;
	req_packet.handle = QCRIL_TOKEN_ID_INTERNAL;
	req_packet.client_id = REQ_PACKET_CLIENT_ID;

	rc = msm_rpc_call_reply(ipfilter_endpoint,
		ONCRPC_CM_MM_CALL_CMD_PS_SIG_RELEASE_PROC,
		&req_packet, sizeof(req_packet), &rep_packet,
		sizeof(rep_packet), -1);

	if (rc < 0) {
		pr_info("enable Fast_Domancy rpc call error! rc=%d, reply=%d\n",
			rc, rep_packet.reply);
		ACER_DBG("enable Fast_Domancy rpc call error! rc=%d, reply=%d\n",
			rc, rep_packet.reply);
		return -EIO;
	}

	ACER_DBG("ENABLED! reply = %d\n", rep_packet.reply);
	return 0;
}
