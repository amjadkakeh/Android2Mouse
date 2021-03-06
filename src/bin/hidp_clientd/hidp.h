#ifndef __HIDP_H_
#define __HIDP_H_

#include "types.h"

#define HIDP_MSGT_HANDSHAKE  0

#	define HIDP_RCHSH_SUCCESFULL 0
#	define HIDP_RCHSH_NOT_READY 1
#	define HIDP_RCHSH_ERR_INVALID_REPORT_ID 2
#	define HIDP_RCHSH_ERR_UNSUPPORTED_REQUEST 3
#	define HIDP_RCHSH_ERR_INVALID_PARAMETER 4
#	define HIDP_RCHSH_ERR_UNKNOWN 0xe
#	define HIDP_RCHSH_ERR_FATAL 0xf

#define	HIDP_MSGT_HID_CONTROL 1

#	define HIDP_RCHSH_SUCCESFULL 0

#define	HIDP_MSGT_GET_REPORT  4
#define	HIDP_MSGT_SET_REPORT  5
#define	HIDP_MSGT_GET_PROTOCOL  6
#define	HIDP_MSGT_SET_PROTOCOL  7
#define	HIDP_MSGT_DATA  10

#define GET_HIDP_TYPE(hdr)  (hdr>>4)

#define hidp_hdr_build(t, p) (((t<<4)&0xf0) | (p&0x0f))

static inline void hidp_hdr_prase(u8 hdr, u8 *msg_type, u8 *param)
{
	*msg_type = (hdr & 0xf0) >> 4;
	*param = (hdr & 0xf);
}
struct hidp_pkt {
	u8 hidp_pkt_hdr; /* use hidp_hdr* for manipulations on .hidp_pkt_hdr */
	u8 data[];
};
static inline struct hidp_pkt *hidp_alloc_pkt(u8 size)
{
	struct hidp_pkt *pkt = malloc(sizeof(struct hidp_pkt)+size);
	memset(pkt, 0, sizeof(struct hidp_pkt)+size);
	return pkt;
}
static inline void hidp_free_pkt(struct hidp_pkt *pkt)
{
	free(pkt);
}
struct hdr_lookup_funcs {
	int (*process)(u8 hdr, int sk);
};
extern struct hdr_lookup_funcs hdr_lookup_dropall[];
/* -------- Lightweight cache ----------- */
struct send_cache {
	u8 *buf, *p;
	size_t size;
};
extern struct send_cache common_cache, drop_cache;
extern int start_cache(struct send_cache *cache, size_t size);
/* -------- Packet handeling ----------- */
extern int pkt_drop(u8 hdr, int sk);
extern int process_hdr_dull(u8 hdr, int sk);
extern int send_hidp_report(u8 type, u8 *data, int size, int flags);
extern int send_einval(int sk, int flags);
extern int drop_hidp_pkt(int sk, int size, int flags,struct send_cache *cache);
extern int recv_hidp_pkt(int sk, u8 *data, int size, int flags);
extern int send_hidp_pkt(int sk, u8 hdr, u8 *data, int size, int flags, struct send_cache *cache);
extern int peek_hidp_hdr(int sk, u8 *hdr);
#define REPT_OTHER 0
#define REPT_INPUT 1
#define REPT_OUTPUT 2
#define REPT_FEATURE 3

#define send_input_rep(rep) send_hidp_report(REPT_INPUT,(u8 *) rep, sizeof(*rep), 0)
/* -------- Connection related functions ----------- */
extern int l2cap_connect(int psm);
extern void close_sk(void);


#endif
