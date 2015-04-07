/*
 * Fundamental constants relating to TCP Protocol
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
 * $Id: bcmtcp.h 458522 2014-02-27 02:26:15Z $
 */

#ifndef _bcmtcp_h_
#define _bcmtcp_h_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

#include <packed_section_start.h>


#define TCP_SRC_PORT_OFFSET	0	
#define TCP_DEST_PORT_OFFSET	2	
#define TCP_SEQ_NUM_OFFSET	4	
#define TCP_ACK_NUM_OFFSET	8	
#define TCP_HLEN_OFFSET		12	
#define TCP_FLAGS_OFFSET	13	
#define TCP_CHKSUM_OFFSET	16	

#define TCP_PORT_LEN		2	

#define TCP_FLAG_URG            0x20
#define TCP_FLAG_ACK            0x10
#define TCP_FLAG_PSH            0x08
#define TCP_FLAG_RST            0x04
#define TCP_FLAG_SYN            0x02
#define TCP_FLAG_FIN            0x01

#define TCP_HLEN_MASK           0xf000
#define TCP_HLEN_SHIFT          12

BWL_PRE_PACKED_STRUCT struct bcmtcp_hdr
{
	uint16	src_port;	
	uint16	dst_port;	
	uint32	seq_num;	
	uint32	ack_num;	
	uint16	hdrlen_rsvd_flags;	
	uint16	tcpwin;		
	uint16	chksum;		
	uint16	urg_ptr;	
} BWL_POST_PACKED_STRUCT;

#define TCP_MIN_HEADER_LEN 20

#define TCP_HDRLEN_MASK 0xf0
#define TCP_HDRLEN_SHIFT 4
#define TCP_HDRLEN(hdrlen) (((hdrlen) & TCP_HDRLEN_MASK) >> TCP_HDRLEN_SHIFT)

#define TCP_FLAGS_MASK  0x1f
#define TCP_FLAGS(hdrlen) ((hdrlen) & TCP_FLAGS_MASK)

#include <packed_section_end.h>

#define IS_TCPSEQ_GE(a, b) ((a - b) < NBITVAL(31))		
#define IS_TCPSEQ_LE(a, b) ((b - a) < NBITVAL(31))		
#define IS_TCPSEQ_GT(a, b) !IS_TCPSEQ_LE(a, b)		
#define IS_TCPSEQ_LT(a, b) !IS_TCPSEQ_GE(a, b)		

#endif	
