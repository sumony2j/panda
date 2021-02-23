/* SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020,2021 SiPanda Inc.
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

#ifndef __PANDA_PROTO_GRE_H__
#define __PANDA_PROTO_GRE_H__

/* GRE node definitions */

#include <arpa/inet.h>
#include <linux/ip.h>

/* Define common GRE constants. These normally come from linux/if_tunnel.h,
 * however that include file has a lot of other definitions beyond just GRE
 * which can be a problem compiling especially with older kernel includes. So
 * we define the GRE values here if they aren't already defined elsewhere.
 */
#ifndef GRE_CSUM
#define GRE_CSUM	__cpu_to_be16(0x8000)
#endif

#ifndef GRE_ROUTING
#define GRE_ROUTING	__cpu_to_be16(0x4000)
#endif

#ifndef GRE_KEY
#define GRE_KEY		__cpu_to_be16(0x2000)
#endif

#ifndef GRE_SEQ
#define GRE_SEQ		__cpu_to_be16(0x1000)
#endif

#ifndef GRE_ACK
#define GRE_ACK		__cpu_to_be16(0x0080)
#endif

#ifndef GRE_VERSION
#define GRE_VERSION	__cpu_to_be16(0x0007)
#endif

#ifndef GRE_VERSION_0
#define GRE_VERSION_0	__cpu_to_be16(0x0000)
#endif

#ifndef GRE_VERSION_1
#define GRE_VERSION_1	__cpu_to_be16(0x0001)
#endif

#ifndef GRE_PROTO_PPP
#define GRE_PROTO_PPP	__cpu_to_be16(0x880b)
#endif

#ifndef GRE_PPTP_KEY_MASK
#define GRE_PPTP_KEY_MASK	__cpu_to_be32(0xffff)
#endif

/* GRE flag-field definitions */
static const struct panda_flag_fields gre_flag_fields = {
	.fields = {
		{
#define GRE_FLAGS_CSUM_IDX	0
			.flag = GRE_CSUM,
			.size = sizeof(__be32),
		},
		{
#define GRE_FLAGS_KEY_IDX	1
			.flag = GRE_KEY,
			.size = sizeof(__be32),
		},
#define GRE_FLAGS_SEQ_IDX	2
		{
			.flag = GRE_SEQ,
			.size = sizeof(__be32),
		},
#define GRE_FLAGS_NUM_IDX	3
	},
	.num_idx = GRE_FLAGS_NUM_IDX
};

static const struct panda_flag_fields pptp_gre_flag_fields = {
	.fields = {
		{
#define GRE_PPTP_FLAGS_CSUM_IDX	0
			.flag = GRE_CSUM,
			.size = sizeof(__be32),
		},
		{
#define GRE_PPTP_FLAGS_KEY_IDX	1
			.flag = GRE_KEY,
			.size = sizeof(__be32),
		},
#define GRE_PPTP_FLAGS_SEQ_IDX	2
		{
			.flag = GRE_SEQ,
			.size = sizeof(__be32),
		},
#define GRE_PPTP_FLAGS_ACK_IDX	3
		{
			.flag = GRE_ACK,
			.size = sizeof(__be32),
		},
#define GRE_PPTP_FLAGS_NUM_IDX	4
	},
	.num_idx = GRE_PPTP_FLAGS_NUM_IDX
};

#define GRE_PPTP_KEY_MASK	__cpu_to_be32(0xffff)

struct gre_hdr {
	__be16 flags;
	__be16 protocol;
	__u8 fields[0];
};

static inline ssize_t gre_len_check(const void *vgre)
{
	/* Only look inside GRE without routing */
	if (((struct gre_hdr *)vgre)->flags & GRE_ROUTING)
		return PANDA_STOP_OKAY;

	return sizeof(struct gre_hdr);
}

static inline int gre_proto_version(const void *vgre)
{
	return ntohs(((struct gre_hdr *)vgre)->flags & GRE_VERSION);
}

static inline ssize_t gre_v0_len(const void *vgre)
{
	return sizeof(struct gre_hdr) +
		panda_flag_fields_length(((struct gre_hdr *)vgre)->flags,
					 &gre_flag_fields);
}

static inline int gre_v0_proto(const void *vgre)
{
	return ((struct gre_hdr *)vgre)->protocol;
}

static inline ssize_t gre_v1_len_check(const void *vgre)
{
	const struct gre_hdr *gre = vgre;

	/* Version1 must be PPTP, and check that keyid id set */
	if (!(gre->protocol == GRE_PROTO_PPP && (gre->flags & GRE_KEY)))
		return PANDA_STOP_OKAY;

	return sizeof(struct gre_hdr) +
		panda_flag_fields_length(gre->flags, &pptp_gre_flag_fields);
}

static inline int gre_v1_proto(const void *vgre)
{
	/* Protocol already checked in gre_v1_len_check */

	return GRE_PROTO_PPP;
}

#endif /* __PANDA_PROTO_GRE_H__ */

#ifdef PANDA_DEFINE_PARSE_NODE

/* panda_parse_gre_base protocol node
 *
 * Parse base GRE header as an overlay to determine GRE version
 *
 * Next protocol operation returns GRE version number (i.e. 0 or 1).
 */
static const struct panda_proto_node panda_parse_gre_base __unused() = {
	.name = "GRE base",
	.overlay = 1,
	.min_len = sizeof(struct gre_hdr),
	.ops.len = gre_len_check,
	.ops.next_proto = gre_proto_version,
};

/* panda_parse_gre_v0 protocol node
 *
 * Parse a version 0 GRE header
 *
 * Next protocol operation returns a GRE protocol (e.g. ETH_P_IPV4).
 */
static const struct panda_proto_node panda_parse_gre_v0 __unused() = {
	.name = "GRE v0",
	.encap = 1,
	.min_len = sizeof(struct gre_hdr),
	.ops.next_proto = gre_v0_proto,
	.ops.len = gre_v0_len,
};

/* panda_parse_gre_v1 protocol node
 *
 * Parse a version 1 GRE header
 *
 * Next protocol operation returns GRE_PROTO_PPP.
 */
static const struct panda_proto_node panda_parse_gre_v1 __unused() = {
	.name = "GRE v1 - pptp",
	.encap = 1,
	.min_len = sizeof(struct gre_hdr),
	.ops.next_proto = gre_v1_proto,
	.ops.len = gre_v1_len_check,
};

#endif /* PANDA_DEFINE_PARSE_NODE */
