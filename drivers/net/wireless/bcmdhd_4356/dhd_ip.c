/*
 * IP Packet Parser Module.
 *
 * Copyright (C) 1999-2015, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_ip.c 502735 2014-09-16 00:53:02Z $
 */
#include <typedefs.h>
#include <osl.h>

#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/802.3.h>
#include <proto/bcmip.h>
#include <bcmendian.h>

#include <dhd_dbg.h>

#include <dhd_ip.h>

#ifdef DHDTCPACK_SUPPRESS
#include <dhd_bus.h>
#include <dhd_proto.h>
#include <proto/bcmtcp.h>
#endif 

static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

pkt_frag_t pkt_frag_info(osl_t *osh, void *p)
{
	uint8 *frame;
	int length;
	uint8 *pt;			
	uint16 ethertype;
	struct ipv4_hdr *iph;		
	int ipl;			
	uint16 iph_frag;

	ASSERT(osh && p);

	frame = PKTDATA(osh, p);
	length = PKTLEN(osh, p);

	
	if (length < ETHER_HDR_LEN) {
		DHD_INFO(("%s: short eth frame (%d)\n", __FUNCTION__, length));
		return DHD_PKT_FRAG_NONE;
	} else if (ntoh16(*(uint16 *)(frame + ETHER_TYPE_OFFSET)) >= ETHER_TYPE_MIN) {
		
		pt = frame + ETHER_TYPE_OFFSET;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
	} else {
		DHD_INFO(("%s: non-SNAP 802.3 frame\n", __FUNCTION__));
		return DHD_PKT_FRAG_NONE;
	}

	ethertype = ntoh16(*(uint16 *)pt);

	
	if (ethertype == ETHER_TYPE_8021Q) {
		pt += VLAN_TAG_LEN;

		if (pt + ETHER_TYPE_LEN > frame + length) {
			DHD_INFO(("%s: short VLAN frame (%d)\n", __FUNCTION__, length));
			return DHD_PKT_FRAG_NONE;
		}

		ethertype = ntoh16(*(uint16 *)pt);
	}

	if (ethertype != ETHER_TYPE_IP) {
		DHD_INFO(("%s: non-IP frame (ethertype 0x%x, length %d)\n",
			__FUNCTION__, ethertype, length));
		return DHD_PKT_FRAG_NONE;
	}

	iph = (struct ipv4_hdr *)(pt + ETHER_TYPE_LEN);
	ipl = (uint)(length - (pt + ETHER_TYPE_LEN - frame));

	
	if ((ipl < IPV4_OPTIONS_OFFSET) || (IP_VER(iph) != IP_VER_4)) {
		DHD_INFO(("%s: short frame (%d) or non-IPv4\n", __FUNCTION__, ipl));
		return DHD_PKT_FRAG_NONE;
	}

	iph_frag = ntoh16(iph->frag);

	if (iph_frag & IPV4_FRAG_DONT) {
		return DHD_PKT_FRAG_NONE;
	} else if ((iph_frag & IPV4_FRAG_MORE) == 0) {
		return DHD_PKT_FRAG_LAST;
	} else {
		return (iph_frag & IPV4_FRAG_OFFSET_MASK)? DHD_PKT_FRAG_CONT : DHD_PKT_FRAG_FIRST;
	}
}

bool pkt_is_dhcp(osl_t *osh, void *p)
{
	uint8 *frame;
	int length;
	uint8 *pt;			
	uint16 ethertype;
	struct ipv4_hdr *iph;		
	int ipl;			
	uint16 src_port;

	ASSERT(osh && p);

	frame = PKTDATA(osh, p);
	length = PKTLEN(osh, p);

	
	if (length < ETHER_HDR_LEN) {
		DHD_INFO(("%s: short eth frame (%d)\n", __FUNCTION__, length));
		return FALSE;
	} else if (ntoh16(*(uint16 *)(frame + ETHER_TYPE_OFFSET)) >= ETHER_TYPE_MIN) {
		
		pt = frame + ETHER_TYPE_OFFSET;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
	} else {
		DHD_INFO(("%s: non-SNAP 802.3 frame\n", __FUNCTION__));
		return FALSE;
	}

	ethertype = ntoh16(*(uint16 *)pt);

	
	if (ethertype == ETHER_TYPE_8021Q) {
		pt += VLAN_TAG_LEN;

		if (pt + ETHER_TYPE_LEN > frame + length) {
			DHD_INFO(("%s: short VLAN frame (%d)\n", __FUNCTION__, length));
			return FALSE;
		}

		ethertype = ntoh16(*(uint16 *)pt);
	}

	if (ethertype != ETHER_TYPE_IP) {
		DHD_INFO(("%s: non-IP frame (ethertype 0x%x, length %d)\n",
			__FUNCTION__, ethertype, length));
		return FALSE;
	}

	iph = (struct ipv4_hdr *)(pt + ETHER_TYPE_LEN);
	ipl = (uint)(length - (pt + ETHER_TYPE_LEN - frame));

	
	if ((ipl < (IPV4_OPTIONS_OFFSET + 2)) || (IP_VER(iph) != IP_VER_4)) {
		DHD_INFO(("%s: short frame (%d) or non-IPv4\n", __FUNCTION__, ipl));
		return FALSE;
	}

	src_port = ntoh16(*(uint16 *)(pt + ETHER_TYPE_LEN + IPV4_OPTIONS_OFFSET));

	return (src_port == 0x43 || src_port == 0x44);
}

#ifdef DHDTCPACK_SUPPRESS

typedef struct {
	void *pkt_in_q;		
	void *pkt_ether_hdr;	
	int ifidx;
	uint8 supp_cnt;
	dhd_pub_t *dhdp;
	struct timer_list timer;
} tcpack_info_t;

typedef struct _tdata_psh_info_t {
	uint32 end_seq;			
	struct _tdata_psh_info_t *next;	
} tdata_psh_info_t;

typedef struct {
	uint8 src_ip_addr[IPV4_ADDR_LEN];	
	uint8 dst_ip_addr[IPV4_ADDR_LEN];	
	uint8 src_tcp_port[TCP_PORT_LEN];	
	uint8 dst_tcp_port[TCP_PORT_LEN];	
	tdata_psh_info_t *tdata_psh_info_head;	
	tdata_psh_info_t *tdata_psh_info_tail;	
	uint32 last_used_time;	
} tcpdata_info_t;

typedef struct {
	int tcpack_info_cnt;
	tcpack_info_t tcpack_info_tbl[TCPACK_INFO_MAXNUM];	
	int tcpdata_info_cnt;
	tcpdata_info_t tcpdata_info_tbl[TCPDATA_INFO_MAXNUM];	
	tdata_psh_info_t *tdata_psh_info_pool;	
	tdata_psh_info_t *tdata_psh_info_free;	
#ifdef DHDTCPACK_SUP_DBG
	int psh_info_enq_num;	
#endif 
} tcpack_sup_module_t;

#if defined(DEBUG_COUNTER) && defined(DHDTCPACK_SUP_DBG)
counter_tbl_t tack_tbl = {"tcpACK", 0, 1000, 10, {0, }, 1};
#endif 

static void
_tdata_psh_info_pool_enq(tcpack_sup_module_t *tcpack_sup_mod,
	tdata_psh_info_t *tdata_psh_info)
{
	if ((tcpack_sup_mod == NULL) || (tdata_psh_info == NULL)) {
		DHD_ERROR(("%s %d: ERROR %p %p\n", __FUNCTION__, __LINE__,
			tcpack_sup_mod, tdata_psh_info));
		return;
	}

	ASSERT(tdata_psh_info->next == NULL);
	tdata_psh_info->next = tcpack_sup_mod->tdata_psh_info_free;
	tcpack_sup_mod->tdata_psh_info_free = tdata_psh_info;
#ifdef DHDTCPACK_SUP_DBG
	tcpack_sup_mod->psh_info_enq_num++;
#endif
}

static tdata_psh_info_t*
_tdata_psh_info_pool_deq(tcpack_sup_module_t *tcpack_sup_mod)
{
	tdata_psh_info_t *tdata_psh_info = NULL;

	if (tcpack_sup_mod == NULL) {
		DHD_ERROR(("%s %d: ERROR %p\n", __FUNCTION__, __LINE__,
			tcpack_sup_mod));
		return NULL;
	}

	tdata_psh_info = tcpack_sup_mod->tdata_psh_info_free;
	if (tdata_psh_info == NULL)
		DHD_ERROR(("%s %d: Out of tdata_disc_grp\n", __FUNCTION__, __LINE__));
	else {
		tcpack_sup_mod->tdata_psh_info_free = tdata_psh_info->next;
		tdata_psh_info->next = NULL;
#ifdef DHDTCPACK_SUP_DBG
		tcpack_sup_mod->psh_info_enq_num--;
#endif 
	}

	return tdata_psh_info;
}

static int _tdata_psh_info_pool_init(dhd_pub_t *dhdp,
	tcpack_sup_module_t *tcpack_sup_mod)
{
	tdata_psh_info_t *tdata_psh_info_pool = NULL;
	uint i;

	DHD_TRACE(("%s %d: Enter\n", __FUNCTION__, __LINE__));

	if (tcpack_sup_mod == NULL)
		return BCME_ERROR;

	ASSERT(tcpack_sup_mod->tdata_psh_info_pool == NULL);
	ASSERT(tcpack_sup_mod->tdata_psh_info_free == NULL);

	tdata_psh_info_pool =
		MALLOC(dhdp->osh, sizeof(tdata_psh_info_t) * TCPDATA_PSH_INFO_MAXNUM);

	if (tdata_psh_info_pool == NULL)
		return BCME_NOMEM;
	bzero(tdata_psh_info_pool, sizeof(tdata_psh_info_t) * TCPDATA_PSH_INFO_MAXNUM);
#ifdef DHDTCPACK_SUP_DBG
	tcpack_sup_mod->psh_info_enq_num = 0;
#endif 

	
	for (i = 0; i < TCPDATA_PSH_INFO_MAXNUM; i++)
		_tdata_psh_info_pool_enq(tcpack_sup_mod, &tdata_psh_info_pool[i]);

	ASSERT(tcpack_sup_mod->tdata_psh_info_free != NULL);
	tcpack_sup_mod->tdata_psh_info_pool = tdata_psh_info_pool;

	return BCME_OK;
}

static void _tdata_psh_info_pool_deinit(dhd_pub_t *dhdp,
	tcpack_sup_module_t *tcpack_sup_mod)
{
	uint i;
	tdata_psh_info_t *tdata_psh_info;

	DHD_TRACE(("%s %d: Enter\n", __FUNCTION__, __LINE__));

	if (tcpack_sup_mod == NULL) {
		DHD_ERROR(("%s %d: ERROR tcpack_sup_mod NULL!\n",
			__FUNCTION__, __LINE__));
		return;
	}

	for (i = 0; i < tcpack_sup_mod->tcpdata_info_cnt; i++) {
		tcpdata_info_t *tcpdata_info = &tcpack_sup_mod->tcpdata_info_tbl[i];
		
		while ((tdata_psh_info = tcpdata_info->tdata_psh_info_head)) {
			tcpdata_info->tdata_psh_info_head = tdata_psh_info->next;
			tdata_psh_info->next = NULL;
			_tdata_psh_info_pool_enq(tcpack_sup_mod, tdata_psh_info);
		}
		tcpdata_info->tdata_psh_info_tail = NULL;
	}
#ifdef DHDTCPACK_SUP_DBG
	DHD_ERROR(("%s %d: PSH INFO ENQ %d\n",
		__FUNCTION__, __LINE__, tcpack_sup_mod->psh_info_enq_num));
#endif 

	i = 0;
	
	while ((tdata_psh_info = tcpack_sup_mod->tdata_psh_info_free)) {
		tcpack_sup_mod->tdata_psh_info_free = tdata_psh_info->next;
		tdata_psh_info->next = NULL;
		i++;
	}
	ASSERT(i == TCPDATA_PSH_INFO_MAXNUM);
	MFREE(dhdp->osh, tcpack_sup_mod->tdata_psh_info_pool,
		sizeof(tdata_psh_info_t) * TCPDATA_PSH_INFO_MAXNUM);
	tcpack_sup_mod->tdata_psh_info_pool = NULL;

	return;
}

static void dhd_tcpack_send(ulong data)
{
	tcpack_sup_module_t *tcpack_sup_mod;
	tcpack_info_t *cur_tbl = (tcpack_info_t *)data;
	dhd_pub_t *dhdp;
	int ifidx;
	void* pkt;

	if (!cur_tbl) {
		return;
	}

	dhdp = cur_tbl->dhdp;
	if (!dhdp) {
		return;
	}

	dhd_os_tcpacklock(dhdp);

	tcpack_sup_mod = dhdp->tcpack_sup_module;
	pkt = cur_tbl->pkt_in_q;
	ifidx = cur_tbl->ifidx;
	if (!pkt) {
		dhd_os_tcpackunlock(dhdp);
		return;
	}
	cur_tbl->pkt_in_q = NULL;
	cur_tbl->pkt_ether_hdr = NULL;
	cur_tbl->ifidx = 0;
	cur_tbl->supp_cnt = 0;
	if (--tcpack_sup_mod->tcpack_info_cnt < 0) {
		DHD_ERROR(("%s %d: ERROR!!! tcp_ack_info_cnt %d\n",
			__FUNCTION__, __LINE__, tcpack_sup_mod->tcpack_info_cnt));
	}

	dhd_os_tcpackunlock(dhdp);

	dhd_sendpkt(dhdp, ifidx, pkt);
}

int dhd_tcpack_suppress_set(dhd_pub_t *dhdp, uint8 mode)
{
	int ret = BCME_OK;

	dhd_os_tcpacklock(dhdp);

	if (dhdp->tcpack_sup_mode == mode) {
		DHD_ERROR(("%s %d: already set to %d\n", __FUNCTION__, __LINE__, mode));
		goto exit;
	}

	if (mode >= TCPACK_SUP_LAST_MODE ||
#ifndef BCMSDIO
		mode == TCPACK_SUP_DELAYTX ||
#endif
		FALSE) {
		DHD_ERROR(("%s %d: Invalid mode %d\n", __FUNCTION__, __LINE__, mode));
		ret = BCME_BADARG;
		goto exit;
	}

	DHD_TRACE(("%s: %d -> %d\n",
		__FUNCTION__, dhdp->tcpack_sup_mode, mode));

	
	if (dhdp->tcpack_sup_mode == TCPACK_SUP_DELAYTX) {
		tcpack_sup_module_t *tcpack_sup_mod = dhdp->tcpack_sup_module;
		
		_tdata_psh_info_pool_deinit(dhdp, tcpack_sup_mod);
		tcpack_sup_mod->tcpdata_info_cnt = 0;
		bzero(tcpack_sup_mod->tcpdata_info_tbl,
			sizeof(tcpdata_info_t) * TCPDATA_INFO_MAXNUM);
		
		if (dhdp->bus)
			dhd_bus_set_dotxinrx(dhdp->bus, TRUE);
	}

	dhdp->tcpack_sup_mode = mode;

	if (mode == TCPACK_SUP_OFF) {
		ASSERT(dhdp->tcpack_sup_module != NULL);
		MFREE(dhdp->osh, dhdp->tcpack_sup_module, sizeof(tcpack_sup_module_t));
		dhdp->tcpack_sup_module = NULL;
		goto exit;
	}

	if (dhdp->tcpack_sup_module == NULL) {
		tcpack_sup_module_t *tcpack_sup_mod =
			MALLOC(dhdp->osh, sizeof(tcpack_sup_module_t));
		if (tcpack_sup_mod == NULL) {
			DHD_ERROR(("%s %d: No MEM\n", __FUNCTION__, __LINE__));
			dhdp->tcpack_sup_mode = TCPACK_SUP_OFF;
			ret = BCME_NOMEM;
			goto exit;
		}
		bzero(tcpack_sup_mod, sizeof(tcpack_sup_module_t));
		dhdp->tcpack_sup_module = tcpack_sup_mod;
	}

	if (mode == TCPACK_SUP_DELAYTX) {
		ret = _tdata_psh_info_pool_init(dhdp, dhdp->tcpack_sup_module);
		if (ret != BCME_OK)
			DHD_ERROR(("%s %d: pool init fail with %d\n", __FUNCTION__, __LINE__, ret));
		else if (dhdp->bus)
			dhd_bus_set_dotxinrx(dhdp->bus, FALSE);
	}

	if (mode == TCPACK_SUP_HOLD) {
		int i;
		tcpack_sup_module_t *tcpack_sup_mod =
			(tcpack_sup_module_t *)dhdp->tcpack_sup_module;
		dhdp->tcpack_sup_ratio = TCPACK_SUPP_RATIO;
		dhdp->tcpack_sup_delay = TCPACK_DELAY_TIME;
		for (i = 0; i < TCPACK_INFO_MAXNUM; i++)
		{
			tcpack_sup_mod->tcpack_info_tbl[i].dhdp = dhdp;
			init_timer(&tcpack_sup_mod->tcpack_info_tbl[i].timer);
			tcpack_sup_mod->tcpack_info_tbl[i].timer.data =
				(ulong)&tcpack_sup_mod->tcpack_info_tbl[i];
			tcpack_sup_mod->tcpack_info_tbl[i].timer.function = dhd_tcpack_send;
		}
	}

exit:
	dhd_os_tcpackunlock(dhdp);
	return ret;
}

void
dhd_tcpack_info_tbl_clean(dhd_pub_t *dhdp)
{
	tcpack_sup_module_t *tcpack_sup_mod = dhdp->tcpack_sup_module;
	int i;

	if (dhdp->tcpack_sup_mode == TCPACK_SUP_OFF)
		goto exit;

	dhd_os_tcpacklock(dhdp);

	if (!tcpack_sup_mod) {
		DHD_ERROR(("%s %d: tcpack suppress module NULL!!\n",
			__FUNCTION__, __LINE__));
		dhd_os_tcpackunlock(dhdp);
		goto exit;
	}

	if (dhdp->tcpack_sup_mode == TCPACK_SUP_HOLD) {
		for (i = 0; i < TCPACK_INFO_MAXNUM; i++) {
			if (tcpack_sup_mod->tcpack_info_tbl[i].pkt_in_q) {
				PKTFREE(dhdp->osh, tcpack_sup_mod->tcpack_info_tbl[i].pkt_in_q,
					TRUE);
				tcpack_sup_mod->tcpack_info_tbl[i].pkt_in_q = NULL;
				tcpack_sup_mod->tcpack_info_tbl[i].pkt_ether_hdr = NULL;
				tcpack_sup_mod->tcpack_info_tbl[i].ifidx = 0;
				tcpack_sup_mod->tcpack_info_tbl[i].supp_cnt = 0;
			}
		}
	} else {
		tcpack_sup_mod->tcpack_info_cnt = 0;
		bzero(tcpack_sup_mod->tcpack_info_tbl, sizeof(tcpack_info_t) * TCPACK_INFO_MAXNUM);
	}

	dhd_os_tcpackunlock(dhdp);

	if (dhdp->tcpack_sup_mode == TCPACK_SUP_HOLD) {
		for (i = 0; i < TCPACK_INFO_MAXNUM; i++) {
			del_timer_sync(&tcpack_sup_mod->tcpack_info_tbl[i].timer);
		}
	}

exit:
	return;
}

inline int dhd_tcpack_check_xmit(dhd_pub_t *dhdp, void *pkt)
{
	uint8 i;
	tcpack_sup_module_t *tcpack_sup_mod;
	tcpack_info_t *tcpack_info_tbl;
	int tbl_cnt;
	int ret = BCME_OK;
	void *pdata;
	uint32 pktlen;

	if (dhdp->tcpack_sup_mode == TCPACK_SUP_OFF)
		goto exit;

	pdata = PKTDATA(dhdp->osh, pkt);
	pktlen = PKTLEN(dhdp->osh, pkt) - dhd_prot_hdrlen(dhdp, pdata);

	if (pktlen < TCPACKSZMIN || pktlen > TCPACKSZMAX) {
		DHD_TRACE(("%s %d: Too short or long length %d to be TCP ACK\n",
			__FUNCTION__, __LINE__, pktlen));
		goto exit;
	}

	dhd_os_tcpacklock(dhdp);
	tcpack_sup_mod = dhdp->tcpack_sup_module;

	if (!tcpack_sup_mod) {
		DHD_ERROR(("%s %d: tcpack suppress module NULL!!\n", __FUNCTION__, __LINE__));
		ret = BCME_ERROR;
		dhd_os_tcpackunlock(dhdp);
		goto exit;
	}
	tbl_cnt = tcpack_sup_mod->tcpack_info_cnt;
	tcpack_info_tbl = tcpack_sup_mod->tcpack_info_tbl;

	ASSERT(tbl_cnt <= TCPACK_INFO_MAXNUM);

	for (i = 0; i < tbl_cnt; i++) {
		if (tcpack_info_tbl[i].pkt_in_q == pkt) {
			DHD_TRACE(("%s %d: pkt %p sent out. idx %d, tbl_cnt %d\n",
				__FUNCTION__, __LINE__, pkt, i, tbl_cnt));
			
			if (i < tbl_cnt - 1) {
				bcopy(&tcpack_info_tbl[tbl_cnt - 1],
					&tcpack_info_tbl[i], sizeof(tcpack_info_t));
			}
			bzero(&tcpack_info_tbl[tbl_cnt - 1], sizeof(tcpack_info_t));
			if (--tcpack_sup_mod->tcpack_info_cnt < 0) {
				DHD_ERROR(("%s %d: ERROR!!! tcp_ack_info_cnt %d\n",
					__FUNCTION__, __LINE__, tcpack_sup_mod->tcpack_info_cnt));
				ret = BCME_ERROR;
			}
			break;
		}
	}
	dhd_os_tcpackunlock(dhdp);

exit:
	return ret;
}

static INLINE bool dhd_tcpdata_psh_acked(dhd_pub_t *dhdp, uint8 *ip_hdr,
	uint8 *tcp_hdr, uint32 tcp_ack_num)
{
	tcpack_sup_module_t *tcpack_sup_mod;
	int i;
	tcpdata_info_t *tcpdata_info = NULL;
	tdata_psh_info_t *tdata_psh_info = NULL;
	bool ret = FALSE;

	if (dhdp->tcpack_sup_mode != TCPACK_SUP_DELAYTX)
		goto exit;

	tcpack_sup_mod = dhdp->tcpack_sup_module;

	if (!tcpack_sup_mod) {
		DHD_ERROR(("%s %d: tcpack suppress module NULL!!\n", __FUNCTION__, __LINE__));
		goto exit;
	}

	DHD_TRACE(("%s %d: IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR
		" TCP port %d %d, ack %u\n", __FUNCTION__, __LINE__,
		IPV4_ADDR_TO_STR(ntoh32_ua(&ip_hdr[IPV4_SRC_IP_OFFSET])),
		IPV4_ADDR_TO_STR(ntoh32_ua(&ip_hdr[IPV4_DEST_IP_OFFSET])),
		ntoh16_ua(&tcp_hdr[TCP_SRC_PORT_OFFSET]),
		ntoh16_ua(&tcp_hdr[TCP_DEST_PORT_OFFSET]),
		tcp_ack_num));

	for (i = 0; i < tcpack_sup_mod->tcpdata_info_cnt; i++) {
		tcpdata_info_t *tcpdata_info_tmp = &tcpack_sup_mod->tcpdata_info_tbl[i];
		DHD_TRACE(("%s %d: data info[%d], IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR
			" TCP port %d %d\n", __FUNCTION__, __LINE__, i,
			IPV4_ADDR_TO_STR(ntoh32_ua(tcpdata_info_tmp->src_ip_addr)),
			IPV4_ADDR_TO_STR(ntoh32_ua(tcpdata_info_tmp->dst_ip_addr)),
			ntoh16_ua(tcpdata_info_tmp->src_tcp_port),
			ntoh16_ua(tcpdata_info_tmp->dst_tcp_port)));

		
		if (memcmp(&ip_hdr[IPV4_SRC_IP_OFFSET],
			tcpdata_info_tmp->dst_ip_addr, IPV4_ADDR_LEN) == 0 &&
			memcmp(&ip_hdr[IPV4_DEST_IP_OFFSET],
			tcpdata_info_tmp->src_ip_addr, IPV4_ADDR_LEN) == 0 &&
			memcmp(&tcp_hdr[TCP_SRC_PORT_OFFSET],
			tcpdata_info_tmp->dst_tcp_port, TCP_PORT_LEN) == 0 &&
			memcmp(&tcp_hdr[TCP_DEST_PORT_OFFSET],
			tcpdata_info_tmp->src_tcp_port, TCP_PORT_LEN) == 0) {
			tcpdata_info = tcpdata_info_tmp;
			break;
		}
	}

	if (tcpdata_info == NULL) {
		DHD_TRACE(("%s %d: no tcpdata_info!\n", __FUNCTION__, __LINE__));
		goto exit;
	}

	if (tcpdata_info->tdata_psh_info_head == NULL) {
		DHD_TRACE(("%s %d: No PSH DATA to be acked!\n", __FUNCTION__, __LINE__));
	}

	while ((tdata_psh_info = tcpdata_info->tdata_psh_info_head)) {
		if (IS_TCPSEQ_GE(tcp_ack_num, tdata_psh_info->end_seq)) {
			DHD_TRACE(("%s %d: PSH ACKED! %u >= %u\n",
				__FUNCTION__, __LINE__, tcp_ack_num, tdata_psh_info->end_seq));
			tcpdata_info->tdata_psh_info_head = tdata_psh_info->next;
			tdata_psh_info->next = NULL;
			_tdata_psh_info_pool_enq(tcpack_sup_mod, tdata_psh_info);
			ret = TRUE;
		} else
			break;
	}
	if (tdata_psh_info == NULL)
		tcpdata_info->tdata_psh_info_tail = NULL;

#ifdef DHDTCPACK_SUP_DBG
	DHD_TRACE(("%s %d: PSH INFO ENQ %d\n",
		__FUNCTION__, __LINE__, tcpack_sup_mod->psh_info_enq_num));
#endif 

exit:
	return ret;
}

bool
dhd_tcpack_suppress(dhd_pub_t *dhdp, void *pkt)
{
	uint8 *new_ether_hdr;	
	uint16 new_ether_type;	
	uint8 *new_ip_hdr;		
	uint8 *new_tcp_hdr;		
	uint32 new_ip_hdr_len;	
	uint32 cur_framelen;
	uint32 new_tcp_ack_num;		
	uint16 new_ip_total_len;	
	uint32 new_tcp_hdr_len;		
	tcpack_sup_module_t *tcpack_sup_mod;
	tcpack_info_t *tcpack_info_tbl;
	int i;
	bool ret = FALSE;
	bool set_dotxinrx = TRUE;

	if (dhdp->tcpack_sup_mode == TCPACK_SUP_OFF)
		goto exit;

	new_ether_hdr = PKTDATA(dhdp->osh, pkt);
	cur_framelen = PKTLEN(dhdp->osh, pkt);

	if (cur_framelen < TCPACKSZMIN || cur_framelen > TCPACKSZMAX) {
		DHD_TRACE(("%s %d: Too short or long length %d to be TCP ACK\n",
			__FUNCTION__, __LINE__, cur_framelen));
		goto exit;
	}

	new_ether_type = new_ether_hdr[12] << 8 | new_ether_hdr[13];

	if (new_ether_type != ETHER_TYPE_IP) {
		DHD_TRACE(("%s %d: Not a IP packet 0x%x\n",
			__FUNCTION__, __LINE__, new_ether_type));
		goto exit;
	}

	DHD_TRACE(("%s %d: IP pkt! 0x%x\n", __FUNCTION__, __LINE__, new_ether_type));

	new_ip_hdr = new_ether_hdr + ETHER_HDR_LEN;
	cur_framelen -= ETHER_HDR_LEN;

	ASSERT(cur_framelen >= IPV4_MIN_HEADER_LEN);

	new_ip_hdr_len = IPV4_HLEN(new_ip_hdr);
	if (IP_VER(new_ip_hdr) != IP_VER_4 || IPV4_PROT(new_ip_hdr) != IP_PROT_TCP) {
		DHD_TRACE(("%s %d: Not IPv4 nor TCP! ip ver %d, prot %d\n",
			__FUNCTION__, __LINE__, IP_VER(new_ip_hdr), IPV4_PROT(new_ip_hdr)));
		goto exit;
	}

	new_tcp_hdr = new_ip_hdr + new_ip_hdr_len;
	cur_framelen -= new_ip_hdr_len;

	ASSERT(cur_framelen >= TCP_MIN_HEADER_LEN);

	DHD_TRACE(("%s %d: TCP pkt!\n", __FUNCTION__, __LINE__));

	
	if (new_tcp_hdr[TCP_FLAGS_OFFSET] != TCP_FLAG_ACK) {
		DHD_TRACE(("%s %d: Do not touch TCP flag 0x%x\n",
			__FUNCTION__, __LINE__, new_tcp_hdr[TCP_FLAGS_OFFSET]));
		goto exit;
	}

	new_ip_total_len = ntoh16_ua(&new_ip_hdr[IPV4_PKTLEN_OFFSET]);
	new_tcp_hdr_len = 4 * TCP_HDRLEN(new_tcp_hdr[TCP_HLEN_OFFSET]);

	
	if (new_ip_total_len > new_ip_hdr_len + new_tcp_hdr_len) {
		DHD_TRACE(("%s %d: Do nothing for TCP DATA\n", __FUNCTION__, __LINE__));
		goto exit;
	}

	ASSERT(new_ip_total_len == new_ip_hdr_len + new_tcp_hdr_len);

	new_tcp_ack_num = ntoh32_ua(&new_tcp_hdr[TCP_ACK_NUM_OFFSET]);

	DHD_TRACE(("%s %d: TCP ACK with zero DATA length"
		" IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR" TCP port %d %d\n",
		__FUNCTION__, __LINE__,
		IPV4_ADDR_TO_STR(ntoh32_ua(&new_ip_hdr[IPV4_SRC_IP_OFFSET])),
		IPV4_ADDR_TO_STR(ntoh32_ua(&new_ip_hdr[IPV4_DEST_IP_OFFSET])),
		ntoh16_ua(&new_tcp_hdr[TCP_SRC_PORT_OFFSET]),
		ntoh16_ua(&new_tcp_hdr[TCP_DEST_PORT_OFFSET])));

	
	dhd_os_tcpacklock(dhdp);
#if defined(DEBUG_COUNTER) && defined(DHDTCPACK_SUP_DBG)
	counter_printlog(&tack_tbl);
	tack_tbl.cnt[0]++;
#endif 

	tcpack_sup_mod = dhdp->tcpack_sup_module;
	tcpack_info_tbl = tcpack_sup_mod->tcpack_info_tbl;

	if (!tcpack_sup_mod) {
		DHD_ERROR(("%s %d: tcpack suppress module NULL!!\n", __FUNCTION__, __LINE__));
		ret = BCME_ERROR;
		dhd_os_tcpackunlock(dhdp);
		goto exit;
	}

	if (dhd_tcpdata_psh_acked(dhdp, new_ip_hdr, new_tcp_hdr, new_tcp_ack_num)) {
		
#if defined(DEBUG_COUNTER) && defined(DHDTCPACK_SUP_DBG)
		tack_tbl.cnt[5]++;
#endif 
	} else
		set_dotxinrx = FALSE;

	for (i = 0; i < tcpack_sup_mod->tcpack_info_cnt; i++) {
		void *oldpkt;	
		uint8 *old_ether_hdr, *old_ip_hdr, *old_tcp_hdr;
		uint32 old_ip_hdr_len, old_tcp_hdr_len;
		uint32 old_tcpack_num;	

		if ((oldpkt = tcpack_info_tbl[i].pkt_in_q) == NULL) {
			DHD_ERROR(("%s %d: Unexpected error!! cur idx %d, ttl cnt %d\n",
				__FUNCTION__, __LINE__, i, tcpack_sup_mod->tcpack_info_cnt));
			break;
		}

		if (PKTDATA(dhdp->osh, oldpkt) == NULL) {
			DHD_ERROR(("%s %d: oldpkt data NULL!! cur idx %d, ttl cnt %d\n",
				__FUNCTION__, __LINE__, i, tcpack_sup_mod->tcpack_info_cnt));
			break;
		}

		old_ether_hdr = tcpack_info_tbl[i].pkt_ether_hdr;
		old_ip_hdr = old_ether_hdr + ETHER_HDR_LEN;
		old_ip_hdr_len = IPV4_HLEN(old_ip_hdr);
		old_tcp_hdr = old_ip_hdr + old_ip_hdr_len;
		old_tcp_hdr_len = 4 * TCP_HDRLEN(old_tcp_hdr[TCP_HLEN_OFFSET]);

		DHD_TRACE(("%s %d: oldpkt %p[%d], IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR
			" TCP port %d %d\n", __FUNCTION__, __LINE__, oldpkt, i,
			IPV4_ADDR_TO_STR(ntoh32_ua(&old_ip_hdr[IPV4_SRC_IP_OFFSET])),
			IPV4_ADDR_TO_STR(ntoh32_ua(&old_ip_hdr[IPV4_DEST_IP_OFFSET])),
			ntoh16_ua(&old_tcp_hdr[TCP_SRC_PORT_OFFSET]),
			ntoh16_ua(&old_tcp_hdr[TCP_DEST_PORT_OFFSET])));

		
		if (memcmp(&new_ip_hdr[IPV4_SRC_IP_OFFSET],
			&old_ip_hdr[IPV4_SRC_IP_OFFSET], IPV4_ADDR_LEN * 2) ||
			memcmp(&new_tcp_hdr[TCP_SRC_PORT_OFFSET],
			&old_tcp_hdr[TCP_SRC_PORT_OFFSET], TCP_PORT_LEN * 2))
			continue;

		old_tcpack_num = ntoh32_ua(&old_tcp_hdr[TCP_ACK_NUM_OFFSET]);

		if (IS_TCPSEQ_GT(new_tcp_ack_num, old_tcpack_num)) {
			
			if (new_ip_hdr_len == old_ip_hdr_len &&
				new_tcp_hdr_len == old_tcp_hdr_len) {
				ASSERT(memcmp(new_ether_hdr, old_ether_hdr, ETHER_HDR_LEN) == 0);
				bcopy(new_ip_hdr, old_ip_hdr, new_ip_total_len);
				PKTFREE(dhdp->osh, pkt, FALSE);
				DHD_TRACE(("%s %d: TCP ACK replace %u -> %u\n",
					__FUNCTION__, __LINE__, old_tcpack_num, new_tcp_ack_num));
#if defined(DEBUG_COUNTER) && defined(DHDTCPACK_SUP_DBG)
				tack_tbl.cnt[2]++;
#endif 
				ret = TRUE;
			} else {
#if defined(DEBUG_COUNTER) && defined(DHDTCPACK_SUP_DBG)
				tack_tbl.cnt[6]++;
#endif 
				DHD_TRACE(("%s %d: lenth mismatch %d != %d || %d != %d"
					" ACK %u -> %u\n", __FUNCTION__, __LINE__,
					new_ip_hdr_len, old_ip_hdr_len,
					new_tcp_hdr_len, old_tcp_hdr_len,
					old_tcpack_num, new_tcp_ack_num));
			}
		} else if (new_tcp_ack_num == old_tcpack_num) {
			set_dotxinrx = TRUE;
			
#if defined(DEBUG_COUNTER) && defined(DHDTCPACK_SUP_DBG)
			tack_tbl.cnt[3]++;
#endif 
		} else {
			DHD_TRACE(("%s %d: ACK number reverse old %u(0x%p) new %u(0x%p)\n",
				__FUNCTION__, __LINE__, old_tcpack_num, oldpkt,
				new_tcp_ack_num, pkt));
		}
		dhd_os_tcpackunlock(dhdp);
		goto exit;
	}

	if (i == tcpack_sup_mod->tcpack_info_cnt && i < TCPACK_INFO_MAXNUM) {
		DHD_TRACE(("%s %d: Add pkt 0x%p(ether_hdr 0x%p) to tbl[%d]\n",
			__FUNCTION__, __LINE__, pkt, new_ether_hdr,
			tcpack_sup_mod->tcpack_info_cnt));

		tcpack_info_tbl[tcpack_sup_mod->tcpack_info_cnt].pkt_in_q = pkt;
		tcpack_info_tbl[tcpack_sup_mod->tcpack_info_cnt].pkt_ether_hdr = new_ether_hdr;
		tcpack_sup_mod->tcpack_info_cnt++;
#if defined(DEBUG_COUNTER) && defined(DHDTCPACK_SUP_DBG)
		tack_tbl.cnt[1]++;
#endif 
	} else {
		ASSERT(i == tcpack_sup_mod->tcpack_info_cnt);
		DHD_TRACE(("%s %d: No empty tcp ack info tbl\n",
			__FUNCTION__, __LINE__));
	}
	dhd_os_tcpackunlock(dhdp);

exit:
	
	if (dhdp->tcpack_sup_mode == TCPACK_SUP_DELAYTX && set_dotxinrx)
		dhd_bus_set_dotxinrx(dhdp->bus, TRUE);

	return ret;
}

bool
dhd_tcpdata_info_get(dhd_pub_t *dhdp, void *pkt)
{
	uint8 *ether_hdr;	
	uint16 ether_type;	
	uint8 *ip_hdr;		
	uint8 *tcp_hdr;		
	uint32 ip_hdr_len;	
	uint32 cur_framelen;
	uint16 ip_total_len;	
	uint32 tcp_hdr_len;		
	uint32 tcp_seq_num;		
	uint16 tcp_data_len;	
	uint32 end_tcp_seq_num;	
	tcpack_sup_module_t *tcpack_sup_mod;
	tcpdata_info_t *tcpdata_info = NULL;
	tdata_psh_info_t *tdata_psh_info;

	int i;
	bool ret = FALSE;

	if (dhdp->tcpack_sup_mode != TCPACK_SUP_DELAYTX)
		goto exit;

	ether_hdr = PKTDATA(dhdp->osh, pkt);
	cur_framelen = PKTLEN(dhdp->osh, pkt);

	ether_type = ether_hdr[12] << 8 | ether_hdr[13];

	if (ether_type != ETHER_TYPE_IP) {
		DHD_TRACE(("%s %d: Not a IP packet 0x%x\n",
			__FUNCTION__, __LINE__, ether_type));
		goto exit;
	}

	DHD_TRACE(("%s %d: IP pkt! 0x%x\n", __FUNCTION__, __LINE__, ether_type));

	ip_hdr = ether_hdr + ETHER_HDR_LEN;
	cur_framelen -= ETHER_HDR_LEN;

	ASSERT(cur_framelen >= IPV4_MIN_HEADER_LEN);

	ip_hdr_len = IPV4_HLEN(ip_hdr);
	if (IP_VER(ip_hdr) != IP_VER_4 || IPV4_PROT(ip_hdr) != IP_PROT_TCP) {
		DHD_TRACE(("%s %d: Not IPv4 nor TCP! ip ver %d, prot %d\n",
			__FUNCTION__, __LINE__, IP_VER(ip_hdr), IPV4_PROT(ip_hdr)));
		goto exit;
	}

	tcp_hdr = ip_hdr + ip_hdr_len;
	cur_framelen -= ip_hdr_len;

	ASSERT(cur_framelen >= TCP_MIN_HEADER_LEN);

	DHD_TRACE(("%s %d: TCP pkt!\n", __FUNCTION__, __LINE__));

	ip_total_len = ntoh16_ua(&ip_hdr[IPV4_PKTLEN_OFFSET]);
	tcp_hdr_len = 4 * TCP_HDRLEN(tcp_hdr[TCP_HLEN_OFFSET]);

	
	if (ip_total_len == ip_hdr_len + tcp_hdr_len) {
		DHD_TRACE(("%s %d: Do nothing for no data TCP ACK\n", __FUNCTION__, __LINE__));
		goto exit;
	}

	ASSERT(ip_total_len > ip_hdr_len + tcp_hdr_len);

	if ((tcp_hdr[TCP_FLAGS_OFFSET] & TCP_FLAG_PSH) == 0) {
		DHD_TRACE(("%s %d: Not interested TCP DATA packet\n", __FUNCTION__, __LINE__));
		goto exit;
	}

	DHD_TRACE(("%s %d: TCP DATA with nonzero DATA length"
		" IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR" TCP port %d %d, flag 0x%x\n",
		__FUNCTION__, __LINE__,
		IPV4_ADDR_TO_STR(ntoh32_ua(&ip_hdr[IPV4_SRC_IP_OFFSET])),
		IPV4_ADDR_TO_STR(ntoh32_ua(&ip_hdr[IPV4_DEST_IP_OFFSET])),
		ntoh16_ua(&tcp_hdr[TCP_SRC_PORT_OFFSET]),
		ntoh16_ua(&tcp_hdr[TCP_DEST_PORT_OFFSET]),
		tcp_hdr[TCP_FLAGS_OFFSET]));

	dhd_os_tcpacklock(dhdp);
	tcpack_sup_mod = dhdp->tcpack_sup_module;

	if (!tcpack_sup_mod) {
		DHD_ERROR(("%s %d: tcpack suppress module NULL!!\n", __FUNCTION__, __LINE__));
		ret = BCME_ERROR;
		dhd_os_tcpackunlock(dhdp);
		goto exit;
	}

	
	i = 0;
	while (i < tcpack_sup_mod->tcpdata_info_cnt) {
		tcpdata_info_t *tdata_info_tmp = &tcpack_sup_mod->tcpdata_info_tbl[i];
		uint32 now_in_ms = OSL_SYSUPTIME();
		DHD_TRACE(("%s %d: data info[%d], IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR
			" TCP port %d %d\n", __FUNCTION__, __LINE__, i,
			IPV4_ADDR_TO_STR(ntoh32_ua(tdata_info_tmp->src_ip_addr)),
			IPV4_ADDR_TO_STR(ntoh32_ua(tdata_info_tmp->dst_ip_addr)),
			ntoh16_ua(tdata_info_tmp->src_tcp_port),
			ntoh16_ua(tdata_info_tmp->dst_tcp_port)));

		
		if (memcmp(&ip_hdr[IPV4_SRC_IP_OFFSET],
			tdata_info_tmp->src_ip_addr, IPV4_ADDR_LEN * 2) == 0 &&
			memcmp(&tcp_hdr[TCP_SRC_PORT_OFFSET],
			tdata_info_tmp->src_tcp_port, TCP_PORT_LEN * 2) == 0) {
			tcpdata_info = tdata_info_tmp;
			tcpdata_info->last_used_time = now_in_ms;
			break;
		}

		if (now_in_ms - tdata_info_tmp->last_used_time > TCPDATA_INFO_TIMEOUT) {
			tdata_psh_info_t *tdata_psh_info_tmp;
			tcpdata_info_t *last_tdata_info;

			while ((tdata_psh_info_tmp = tdata_info_tmp->tdata_psh_info_head)) {
				tdata_info_tmp->tdata_psh_info_head = tdata_psh_info_tmp->next;
				tdata_psh_info_tmp->next = NULL;
				DHD_TRACE(("%s %d: Clean tdata_psh_info(end_seq %u)!\n",
					__FUNCTION__, __LINE__, tdata_psh_info_tmp->end_seq));
				_tdata_psh_info_pool_enq(tcpack_sup_mod, tdata_psh_info_tmp);
			}
#ifdef DHDTCPACK_SUP_DBG
			DHD_ERROR(("%s %d: PSH INFO ENQ %d\n",
				__FUNCTION__, __LINE__, tcpack_sup_mod->psh_info_enq_num));
#endif 
			tcpack_sup_mod->tcpdata_info_cnt--;
			ASSERT(tcpack_sup_mod->tcpdata_info_cnt >= 0);

			last_tdata_info =
				&tcpack_sup_mod->tcpdata_info_tbl[tcpack_sup_mod->tcpdata_info_cnt];
			if (i < tcpack_sup_mod->tcpdata_info_cnt) {
				ASSERT(last_tdata_info != tdata_info_tmp);
				bcopy(last_tdata_info, tdata_info_tmp, sizeof(tcpdata_info_t));
			}
			bzero(last_tdata_info, sizeof(tcpdata_info_t));
			DHD_ERROR(("%s %d: tcpdata_info(idx %d) is aged out. ttl cnt is now %d\n",
				__FUNCTION__, __LINE__, i, tcpack_sup_mod->tcpdata_info_cnt));
			
		} else
			 i++;
	}

	tcp_seq_num = ntoh32_ua(&tcp_hdr[TCP_SEQ_NUM_OFFSET]);
	tcp_data_len = ip_total_len - ip_hdr_len - tcp_hdr_len;
	end_tcp_seq_num = tcp_seq_num + tcp_data_len;

	if (tcpdata_info == NULL) {
		ASSERT(i == tcpack_sup_mod->tcpdata_info_cnt);
		if (i >= TCPDATA_INFO_MAXNUM) {
			DHD_TRACE(("%s %d: tcp_data_info_tbl FULL! %d %d"
				" IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR" TCP port %d %d\n",
				__FUNCTION__, __LINE__, i, tcpack_sup_mod->tcpdata_info_cnt,
				IPV4_ADDR_TO_STR(ntoh32_ua(&ip_hdr[IPV4_SRC_IP_OFFSET])),
				IPV4_ADDR_TO_STR(ntoh32_ua(&ip_hdr[IPV4_DEST_IP_OFFSET])),
				ntoh16_ua(&tcp_hdr[TCP_SRC_PORT_OFFSET]),
				ntoh16_ua(&tcp_hdr[TCP_DEST_PORT_OFFSET])));
			dhd_os_tcpackunlock(dhdp);
			goto exit;
		}
		tcpdata_info = &tcpack_sup_mod->tcpdata_info_tbl[i];

		DHD_ERROR(("%s %d: Add data info to tbl[%d]: IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR
			" TCP port %d %d\n",
			__FUNCTION__, __LINE__, tcpack_sup_mod->tcpdata_info_cnt,
			IPV4_ADDR_TO_STR(ntoh32_ua(&ip_hdr[IPV4_SRC_IP_OFFSET])),
			IPV4_ADDR_TO_STR(ntoh32_ua(&ip_hdr[IPV4_DEST_IP_OFFSET])),
			ntoh16_ua(&tcp_hdr[TCP_SRC_PORT_OFFSET]),
			ntoh16_ua(&tcp_hdr[TCP_DEST_PORT_OFFSET])));

		bcopy(&ip_hdr[IPV4_SRC_IP_OFFSET], tcpdata_info->src_ip_addr,
			IPV4_ADDR_LEN * 2);
		bcopy(&tcp_hdr[TCP_SRC_PORT_OFFSET], tcpdata_info->src_tcp_port,
			TCP_PORT_LEN * 2);

		tcpdata_info->last_used_time = OSL_SYSUPTIME();
		tcpack_sup_mod->tcpdata_info_cnt++;
	}

	ASSERT(tcpdata_info != NULL);

	tdata_psh_info = _tdata_psh_info_pool_deq(tcpack_sup_mod);
#ifdef DHDTCPACK_SUP_DBG
	DHD_TRACE(("%s %d: PSH INFO ENQ %d\n",
		__FUNCTION__, __LINE__, tcpack_sup_mod->psh_info_enq_num));
#endif 

	if (tdata_psh_info == NULL) {
		DHD_ERROR(("%s %d: No more free tdata_psh_info!!\n", __FUNCTION__, __LINE__));
		ret = BCME_ERROR;
		dhd_os_tcpackunlock(dhdp);
		goto exit;
	}
	tdata_psh_info->end_seq = end_tcp_seq_num;

#if defined(DEBUG_COUNTER) && defined(DHDTCPACK_SUP_DBG)
	tack_tbl.cnt[4]++;
#endif 

	DHD_TRACE(("%s %d: TCP PSH DATA recvd! end seq %u\n",
		__FUNCTION__, __LINE__, tdata_psh_info->end_seq));

	ASSERT(tdata_psh_info->next == NULL);

	if (tcpdata_info->tdata_psh_info_head == NULL)
		tcpdata_info->tdata_psh_info_head = tdata_psh_info;
	else {
		ASSERT(tcpdata_info->tdata_psh_info_tail);
		tcpdata_info->tdata_psh_info_tail->next = tdata_psh_info;
	}
	tcpdata_info->tdata_psh_info_tail = tdata_psh_info;

	dhd_os_tcpackunlock(dhdp);

exit:
	return ret;
}

bool
dhd_tcpack_hold(dhd_pub_t *dhdp, void *pkt, int ifidx)
{
	uint8 *new_ether_hdr;	
	uint16 new_ether_type;	
	uint8 *new_ip_hdr;		
	uint8 *new_tcp_hdr;		
	uint32 new_ip_hdr_len;	
	uint32 cur_framelen;
	uint32 new_tcp_ack_num;		
	uint16 new_ip_total_len;	
	uint32 new_tcp_hdr_len;		
	tcpack_sup_module_t *tcpack_sup_mod;
	tcpack_info_t *tcpack_info_tbl;
	int i, free_slot = TCPACK_INFO_MAXNUM;
	bool hold = FALSE;

	if (dhdp->tcpack_sup_mode != TCPACK_SUP_HOLD) {
		goto exit;
	}

	if (dhdp->tcpack_sup_ratio == 1) {
		goto exit;
	}

	new_ether_hdr = PKTDATA(dhdp->osh, pkt);
	cur_framelen = PKTLEN(dhdp->osh, pkt);

	if (cur_framelen < TCPACKSZMIN || cur_framelen > TCPACKSZMAX) {
		DHD_TRACE(("%s %d: Too short or long length %d to be TCP ACK\n",
			__FUNCTION__, __LINE__, cur_framelen));
		goto exit;
	}

	new_ether_type = new_ether_hdr[12] << 8 | new_ether_hdr[13];

	if (new_ether_type != ETHER_TYPE_IP) {
		DHD_TRACE(("%s %d: Not a IP packet 0x%x\n",
			__FUNCTION__, __LINE__, new_ether_type));
		goto exit;
	}

	DHD_TRACE(("%s %d: IP pkt! 0x%x\n", __FUNCTION__, __LINE__, new_ether_type));

	new_ip_hdr = new_ether_hdr + ETHER_HDR_LEN;
	cur_framelen -= ETHER_HDR_LEN;

	ASSERT(cur_framelen >= IPV4_MIN_HEADER_LEN);

	new_ip_hdr_len = IPV4_HLEN(new_ip_hdr);
	if (IP_VER(new_ip_hdr) != IP_VER_4 || IPV4_PROT(new_ip_hdr) != IP_PROT_TCP) {
		DHD_TRACE(("%s %d: Not IPv4 nor TCP! ip ver %d, prot %d\n",
			__FUNCTION__, __LINE__, IP_VER(new_ip_hdr), IPV4_PROT(new_ip_hdr)));
		goto exit;
	}

	new_tcp_hdr = new_ip_hdr + new_ip_hdr_len;
	cur_framelen -= new_ip_hdr_len;

	ASSERT(cur_framelen >= TCP_MIN_HEADER_LEN);

	DHD_TRACE(("%s %d: TCP pkt!\n", __FUNCTION__, __LINE__));

	
	if (new_tcp_hdr[TCP_FLAGS_OFFSET] != TCP_FLAG_ACK) {
		DHD_TRACE(("%s %d: Do not touch TCP flag 0x%x\n",
			__FUNCTION__, __LINE__, new_tcp_hdr[TCP_FLAGS_OFFSET]));
		goto exit;
	}

	new_ip_total_len = ntoh16_ua(&new_ip_hdr[IPV4_PKTLEN_OFFSET]);
	new_tcp_hdr_len = 4 * TCP_HDRLEN(new_tcp_hdr[TCP_HLEN_OFFSET]);

	
	if (new_ip_total_len > new_ip_hdr_len + new_tcp_hdr_len) {
		DHD_TRACE(("%s %d: Do nothing for TCP DATA\n", __FUNCTION__, __LINE__));
		goto exit;
	}

	ASSERT(new_ip_total_len == new_ip_hdr_len + new_tcp_hdr_len);

	new_tcp_ack_num = ntoh32_ua(&new_tcp_hdr[TCP_ACK_NUM_OFFSET]);

	DHD_TRACE(("%s %d: TCP ACK with zero DATA length"
		" IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR" TCP port %d %d\n",
		__FUNCTION__, __LINE__,
		IPV4_ADDR_TO_STR(ntoh32_ua(&new_ip_hdr[IPV4_SRC_IP_OFFSET])),
		IPV4_ADDR_TO_STR(ntoh32_ua(&new_ip_hdr[IPV4_DEST_IP_OFFSET])),
		ntoh16_ua(&new_tcp_hdr[TCP_SRC_PORT_OFFSET]),
		ntoh16_ua(&new_tcp_hdr[TCP_DEST_PORT_OFFSET])));

	
	dhd_os_tcpacklock(dhdp);

	tcpack_sup_mod = dhdp->tcpack_sup_module;
	tcpack_info_tbl = tcpack_sup_mod->tcpack_info_tbl;

	if (!tcpack_sup_mod) {
		DHD_ERROR(("%s %d: tcpack suppress module NULL!!\n", __FUNCTION__, __LINE__));
		dhd_os_tcpackunlock(dhdp);
		goto exit;
	}

	hold = TRUE;

	for (i = 0; i < TCPACK_INFO_MAXNUM; i++) {
		void *oldpkt;	
		uint8 *old_ether_hdr, *old_ip_hdr, *old_tcp_hdr;
		uint32 old_ip_hdr_len, old_tcp_hdr_len;
		uint32 old_tcpack_num;	

		if ((oldpkt = tcpack_info_tbl[i].pkt_in_q) == NULL) {
			if (free_slot == TCPACK_INFO_MAXNUM) {
				free_slot = i;
			}
			continue;
		}

		if (PKTDATA(dhdp->osh, oldpkt) == NULL) {
			DHD_ERROR(("%s %d: oldpkt data NULL!! cur idx %d\n",
				__FUNCTION__, __LINE__, i));
			hold = FALSE;
			dhd_os_tcpackunlock(dhdp);
			goto exit;
		}

		old_ether_hdr = tcpack_info_tbl[i].pkt_ether_hdr;
		old_ip_hdr = old_ether_hdr + ETHER_HDR_LEN;
		old_ip_hdr_len = IPV4_HLEN(old_ip_hdr);
		old_tcp_hdr = old_ip_hdr + old_ip_hdr_len;
		old_tcp_hdr_len = 4 * TCP_HDRLEN(old_tcp_hdr[TCP_HLEN_OFFSET]);

		DHD_TRACE(("%s %d: oldpkt %p[%d], IP addr "IPV4_ADDR_STR" "IPV4_ADDR_STR
			" TCP port %d %d\n", __FUNCTION__, __LINE__, oldpkt, i,
			IPV4_ADDR_TO_STR(ntoh32_ua(&old_ip_hdr[IPV4_SRC_IP_OFFSET])),
			IPV4_ADDR_TO_STR(ntoh32_ua(&old_ip_hdr[IPV4_DEST_IP_OFFSET])),
			ntoh16_ua(&old_tcp_hdr[TCP_SRC_PORT_OFFSET]),
			ntoh16_ua(&old_tcp_hdr[TCP_DEST_PORT_OFFSET])));

		
		if (memcmp(&new_ip_hdr[IPV4_SRC_IP_OFFSET],
			&old_ip_hdr[IPV4_SRC_IP_OFFSET], IPV4_ADDR_LEN * 2) ||
			memcmp(&new_tcp_hdr[TCP_SRC_PORT_OFFSET],
			&old_tcp_hdr[TCP_SRC_PORT_OFFSET], TCP_PORT_LEN * 2)) {
			continue;
		}

		old_tcpack_num = ntoh32_ua(&old_tcp_hdr[TCP_ACK_NUM_OFFSET]);

		if (IS_TCPSEQ_GE(new_tcp_ack_num, old_tcpack_num)) {
			tcpack_info_tbl[i].supp_cnt++;
			if (tcpack_info_tbl[i].supp_cnt >= dhdp->tcpack_sup_ratio) {
				tcpack_info_tbl[i].pkt_in_q = NULL;
				tcpack_info_tbl[i].pkt_ether_hdr = NULL;
				tcpack_info_tbl[i].ifidx = 0;
				tcpack_info_tbl[i].supp_cnt = 0;
				hold = FALSE;
			} else {
				tcpack_info_tbl[i].pkt_in_q = pkt;
				tcpack_info_tbl[i].pkt_ether_hdr = new_ether_hdr;
				tcpack_info_tbl[i].ifidx = ifidx;
			}
			PKTFREE(dhdp->osh, oldpkt, TRUE);
		} else {
			PKTFREE(dhdp->osh, pkt, TRUE);
		}
		dhd_os_tcpackunlock(dhdp);

		if (!hold) {
			del_timer_sync(&tcpack_info_tbl[i].timer);
		}
		goto exit;
	}

	if (free_slot < TCPACK_INFO_MAXNUM) {
		DHD_TRACE(("%s %d: Add pkt 0x%p(ether_hdr 0x%p) to tbl[%d]\n",
			__FUNCTION__, __LINE__, pkt, new_ether_hdr,
			free_slot));

		tcpack_info_tbl[free_slot].pkt_in_q = pkt;
		tcpack_info_tbl[free_slot].pkt_ether_hdr = new_ether_hdr;
		tcpack_info_tbl[free_slot].ifidx = ifidx;
		tcpack_info_tbl[free_slot].supp_cnt = 1;
		mod_timer(&tcpack_sup_mod->tcpack_info_tbl[free_slot].timer,
			jiffies + msecs_to_jiffies(dhdp->tcpack_sup_delay));
		tcpack_sup_mod->tcpack_info_cnt++;
	} else {
		DHD_TRACE(("%s %d: No empty tcp ack info tbl\n",
			__FUNCTION__, __LINE__));
	}
	dhd_os_tcpackunlock(dhdp);

exit:
	return hold;
}
#endif 
