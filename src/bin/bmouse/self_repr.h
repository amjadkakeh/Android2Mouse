#include <linux/types.h>
#include "sdp.h"
#include "sdp_lib.h"
#include "bluetooth.h"

/* 0x5a020c -- a phone */
/* 0x10590  -- a mouse */
/* 0x105CC  -- a combo(multi) */

#define DC_MOUSE 0x10590
#define DC_MULTI 0x105CC

#define DC_PHONE 0x5a020c

struct sdp_record_builder {
	sdp_record_t record;
	
	int (*build_record)(sdp_record_t *record, void *parsed);
	int (*parse_args)(char *args, void **parsed);
};

extern struct sdp_record_builder hid_mouse;
extern struct sdp_record_builder hid_multi;


int _get_dev_class(uint32_t *class, int dev_id);
int _set_dev_class(uint32_t  class, int dev_id);

static inline int get_dev_class(uint32_t *class) 
{
	return _get_dev_class(class, 0);
}
static inline int set_dev_class(uint32_t class) 
{
	return _set_dev_class(class, 0);
}

int add_sdp_record(struct sdp_record_builder *rep, char *args, int *handle);
int remove_sdp_record(int handle);
