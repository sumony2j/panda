// SPDX-License-Identifier: BSD-2-Clause-FreeBSD
/*
 * Copyright (c) 2020, 2021 SiPanda Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* PANDA Big Parser
 *
 * Implement flow dissector in PANDA. A protocol parse graph is created and
 * metadata is extracted at various nodes.
 */

#include <arpa/inet.h>
#include <linux/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "panda/parsers/parser_big.h"
#include "siphash/siphash.h"

/* Define protocol nodes that are used below */
#include "panda/proto_nodes_def.h"

/* Meta data functions for parser nodes. Use the canned templates
 * for common metadata
 */
PANDA_METADATA_TEMP_ether(ether_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_ipv4(ipv4_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_ipv6(ipv6_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_ip_overlay(ip_overlay_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_ipv6_eh(ipv6_eh_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_ipv6_frag(ipv6_frag_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_ports(ports_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_gre_v0(gre_v0_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_gre_v1(gre_v1_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_icmp(icmp_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_vlan_8021AD(e8021AD_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_vlan_8021Q(e8021Q_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_mpls(mpls_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_arp_rarp(arp_rarp_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_tipc(tipc_metadata, panda_metadata_all)

PANDA_METADATA_TEMP_tcp_option_mss(tcp_opt_mss_metadata, panda_metadata_all)
PANDA_METADATA_TEMP_tcp_option_window_scaling(tcp_opt_window_scaling_metadata,
					      panda_metadata_all)
PANDA_METADATA_TEMP_tcp_option_timestamp(tcp_opt_timestamp_metadata,
					 panda_metadata_all)
PANDA_METADATA_TEMP_tcp_option_sack(tcp_opt_sack_metadata, panda_metadata_all)

/* Parse nodes. Parse nodes are composed of the common PANDA Parser protocol
 * nodes, metadata functions defined above, and protocol tables defined
 * below
 */

PANDA_MAKE_PARSE_NODE(ether_node, panda_parse_ether, ether_metadata,
		      panda_null_handle_proto, ether_table);
PANDA_MAKE_PARSE_NODE(ipv4_check_node, panda_parse_ip,
		      panda_null_extract_metadata,
		      panda_null_handle_proto, ipv4_check_table);
PANDA_MAKE_PARSE_NODE(ipv4_node, panda_parse_ipv4, ipv4_metadata,
		      panda_null_handle_proto, ipv4_table);
PANDA_MAKE_PARSE_NODE(ipv6_check_node, panda_parse_ip,
		      panda_null_extract_metadata,
		      panda_null_handle_proto, ipv6_check_table);
PANDA_MAKE_PARSE_NODE(ipv6_node, panda_parse_ipv6_stopflowlabel,
		      ipv6_metadata, panda_null_handle_proto, ipv6_table);
PANDA_MAKE_PARSE_NODE(ip_overlay_node, panda_parse_ip, ip_overlay_metadata,
		      panda_null_handle_proto, ip_table);
PANDA_MAKE_PARSE_NODE(ipv6_eh_node, panda_parse_ipv6_eh, ipv6_eh_metadata,
		      panda_null_handle_proto, ipv6_table);
PANDA_MAKE_PARSE_NODE(ipv6_frag_node, panda_parse_ipv6_frag_eh,
		      ipv6_frag_metadata, panda_null_handle_proto, ipv6_table);
PANDA_MAKE_PARSE_NODE(gre_base_node, panda_parse_gre_base,
		      panda_null_extract_metadata, panda_null_handle_proto,
		      gre_base_table);
PANDA_MAKE_PARSE_NODE(gre_v0_node, panda_parse_gre_v0, gre_v0_metadata,
		      panda_null_handle_proto, gre_v0_table);
PANDA_MAKE_PARSE_NODE(gre_v1_node, panda_parse_gre_v1, gre_v1_metadata,
		      panda_null_handle_proto, gre_v1_table);
PANDA_MAKE_PARSE_NODE(e8021AD_node, panda_parse_vlan, e8021AD_metadata,
		      panda_null_handle_proto, ether_table);
PANDA_MAKE_PARSE_NODE(e8021Q_node, panda_parse_vlan, e8021Q_metadata,
		      panda_null_handle_proto, ether_table);
PANDA_MAKE_PARSE_NODE(ppp_node, panda_parse_ppp, panda_null_extract_metadata,
		      panda_null_handle_proto, ppp_table);
PANDA_MAKE_PARSE_NODE(ipv4ip_node, panda_parse_ipv4ip,
		      panda_null_extract_metadata, panda_null_handle_proto,
		      ipv4ip_table);
PANDA_MAKE_PARSE_NODE(ipv6ip_node, panda_parse_ipv6ip,
		      panda_null_extract_metadata, panda_null_handle_proto,
		      ipv6ip_table);
PANDA_MAKE_PARSE_NODE(batman_node, panda_parse_batman,
		      panda_null_extract_metadata, panda_null_handle_proto,
		      ether_table);

PANDA_MAKE_LEAF_PARSE_NODE(ports_node, panda_parse_ports, ports_metadata,
			   panda_null_handle_proto);
PANDA_MAKE_LEAF_PARSE_NODE(icmpv4_node, panda_parse_icmpv4, icmp_metadata,
			   panda_null_handle_proto);
PANDA_MAKE_LEAF_PARSE_NODE(icmpv6_node, panda_parse_icmpv6, icmp_metadata,
			   panda_null_handle_proto);
PANDA_MAKE_LEAF_PARSE_NODE(mpls_node, panda_parse_mpls, mpls_metadata,
			   panda_null_handle_proto);
PANDA_MAKE_LEAF_PARSE_NODE(arp_node, panda_parse_arp, arp_rarp_metadata,
			   panda_null_handle_proto);
PANDA_MAKE_LEAF_PARSE_NODE(rarp_node, panda_parse_rarp, arp_rarp_metadata,
			   panda_null_handle_proto);
PANDA_MAKE_LEAF_PARSE_NODE(tipc_node, panda_parse_tipc, tipc_metadata,
			   panda_null_handle_proto);
PANDA_MAKE_LEAF_PARSE_NODE(fcoe_node, panda_parse_fcoe,
			   panda_null_extract_metadata,
			   panda_null_handle_proto);
PANDA_MAKE_LEAF_PARSE_NODE(igmp_node, panda_parse_igmp,
			   panda_null_extract_metadata,
			   panda_null_handle_proto);

PANDA_MAKE_LEAF_TLVS_PARSE_NODE(tcp_node, panda_parse_tcp_tlvs,	ports_metadata,
				panda_null_handle_proto,
				panda_null_post_tlv_handle, tcp_tlv_table);

PANDA_MAKE_TLV_PARSE_NODE(tcp_opt_mss_node, tcp_option_mss_check_length,
			  tcp_opt_mss_metadata, panda_null_handle_tlv);
PANDA_MAKE_TLV_PARSE_NODE(tcp_opt_window_scaling_node,
			  tcp_option_window_scaling_check_length,
			  tcp_opt_window_scaling_metadata,
			  panda_null_handle_tlv);
PANDA_MAKE_TLV_PARSE_NODE(tcp_opt_timestamp_node,
			  tcp_option_timestamp_check_length,
			  tcp_opt_timestamp_metadata, panda_null_handle_tlv);
PANDA_MAKE_TLV_PARSE_NODE(tcp_opt_sack_node, tcp_option_sack_check_length,
			  tcp_opt_sack_metadata, panda_null_handle_tlv);

/* Define parsers. Two of them: one for packets starting with an
 * Ethernet header, and one for packets starting with an IP header.
 */
PANDA_PARSER_ADD(panda_parser_big_ether, "PANDA big parser for Ethernet",
		 &ether_node);
PANDA_PARSER_ADD(panda_parser_big_ip, "PANDA big parser for IP",
		 &ip_overlay_node);

/* Protocol tables */

PANDA_MAKE_PROTO_TABLE(ether_table,
	{ __cpu_to_be16(ETH_P_IP), &ipv4_check_node },
	{ __cpu_to_be16(ETH_P_IPV6), &ipv6_check_node },
	{ __cpu_to_be16(ETH_P_8021AD), &e8021AD_node },
	{ __cpu_to_be16(ETH_P_8021Q), &e8021Q_node },
	{ __cpu_to_be16(ETH_P_MPLS_UC), &mpls_node },
	{ __cpu_to_be16(ETH_P_MPLS_MC), &mpls_node },
	{ __cpu_to_be16(ETH_P_ARP), &arp_node },
	{ __cpu_to_be16(ETH_P_RARP), &rarp_node },
	{ __cpu_to_be16(ETH_P_TIPC), &tipc_node },
	{ __cpu_to_be16(ETH_P_BATMAN), &batman_node },
	{ __cpu_to_be16(ETH_P_FCOE), &fcoe_node },
);

PANDA_MAKE_PROTO_TABLE(ipv4_check_table,
	{ 4, &ipv4_node },
);

PANDA_MAKE_PROTO_TABLE(ipv4_table,
	{ IPPROTO_TCP, &tcp_node.parse_node },
	{ IPPROTO_UDP, &ports_node },
	{ IPPROTO_SCTP, &ports_node },
	{ IPPROTO_DCCP, &ports_node },
	{ IPPROTO_GRE, &gre_base_node },
	{ IPPROTO_ICMP, &icmpv4_node },
	{ IPPROTO_IGMP, &igmp_node },
	{ IPPROTO_MPLS, &mpls_node },
	{ IPPROTO_IPIP, &ipv4ip_node },
	{ IPPROTO_IPV6, &ipv6ip_node },
);

PANDA_MAKE_PROTO_TABLE(ipv6_check_table,
	{ 6, &ipv6_node },
);

PANDA_MAKE_PROTO_TABLE(ipv6_table,
	{ IPPROTO_HOPOPTS, &ipv6_eh_node },
	{ IPPROTO_ROUTING, &ipv6_eh_node },
	{ IPPROTO_DSTOPTS, &ipv6_eh_node },
	{ IPPROTO_FRAGMENT, &ipv6_frag_node },
	{ IPPROTO_TCP, &tcp_node.parse_node },
	{ IPPROTO_UDP, &ports_node },
	{ IPPROTO_SCTP, &ports_node },
	{ IPPROTO_DCCP, &ports_node },
	{ IPPROTO_GRE, &gre_base_node },
	{ IPPROTO_ICMP, &icmpv6_node },
	{ IPPROTO_IGMP, &igmp_node },
	{ IPPROTO_MPLS, &mpls_node },
);

PANDA_MAKE_PROTO_TABLE(ip_table,
	{ 4, &ipv4_node },
	{ 6, &ipv6_node },
);

PANDA_MAKE_PROTO_TABLE(ipv4ip_table,
	{ 0, &ipv4_node },
);

PANDA_MAKE_PROTO_TABLE(ipv6ip_table,
	{ 0, &ipv6_node },
);

PANDA_MAKE_PROTO_TABLE(gre_base_table,
	{ 0, &gre_v0_node },
	{ 1, &gre_v1_node },
);

PANDA_MAKE_PROTO_TABLE(gre_v0_table,
	{ __cpu_to_be16(ETH_P_IP), &ipv4_check_node },
	{ __cpu_to_be16(ETH_P_IPV6), &ipv6_check_node },
	{ __cpu_to_be16(ETH_P_TEB), &ether_node },
);

PANDA_MAKE_PROTO_TABLE(gre_v1_table,
	{ 0, &ppp_node },
);

PANDA_MAKE_PROTO_TABLE(ppp_table,
	{ PPP_IP, &ipv4_check_node },
	{ PPP_IPV6, &ipv6_check_node },
);

PANDA_MAKE_TLV_TABLE(tcp_tlv_table,
	{ TCPOPT_MSS, &tcp_opt_mss_node },
	{ TCPOPT_WINDOW, &tcp_opt_window_scaling_node },
	{ TCPOPT_TIMESTAMP, &tcp_opt_timestamp_node },
	{ TCPOPT_SACK, &tcp_opt_sack_node }
);

/* Ancilary functions */

void panda_parser_big_print_frame(struct panda_metadata_all *frame)
{
	PANDA_PRINT_METADATA(frame);
}

void panda_parser_big_print_hash_input(struct panda_metadata_all *frame)
{
	const void *start = PANDA_HASH_START(frame,
					     PANDA_HASH_START_FIELD_ALL);
	size_t len = PANDA_HASH_LENGTH(frame,
				       PANDA_HASH_OFFSET_ALL);

	panda_print_hash_input(start, len);
}