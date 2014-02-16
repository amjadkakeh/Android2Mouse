#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "bluetooth.h"
#include "l2cap.h"

#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <poll.h>

#include "log.h"

#include "hidp.h"

#include "hidp_client.h"
#include "combo_conn.h"

/* linux/uhid.h ? */
bdaddr_t daddr, laddr;
int int_sock = -1, cnt_sock = -1, src_fd = -1;

// static int (*process_evfd) (int fd);	

int process_sk(int sk, struct conn_info *info) 
{
	u8 hdr;
	int err = peek_hidp_hdr(sk, &hdr);
	info("Got pkt (%x)", GET_HIDP_TYPE(hdr));
	
	if(GET_HIDP_TYPE(hdr) <= 0xa && info->hdr_lookup[GET_HIDP_TYPE(hdr)].process) {
		dbg("processing pkt with method");
		info->hdr_lookup[GET_HIDP_TYPE(hdr)].process(hdr, sk);
	} else {
		dbg("no method, dropping pkt, sending einval");
		int len = recv_hidp_pkt(sk, &hdr, 1, 0);
		if(len == -1) {
			error("errno=%d",errno);
			error("recv");
			return -1;
		}
			
		if(send_einval(sk, 0) == -1) {
			error("errno=%d",errno);
			error("send");
			return -1;
		}
			
	}
	return 0;
		

}

/* -------- Mouse conn_info implementation ----------- */


	
static int send_mouse_rep(u8 b, u8 x, u8 y, u8 w)
{
	u8 m[5] = {1,b, x, y, w};
	
	return send_hidp_report(REPT_INPUT, m, sizeof(m), 0);
}
int process_mouse(int fd, struct conn_info *info) 
{
	struct hcr_mouse_report m;
	
	while(read(fd, &m, sizeof(m)) == sizeof(m)) {
		if(m.flags & HCR_SHUTDOWN) {
			dbg("receved SHUTDOWN rq");
			return SHUTDOWN;
		}
		dbg("processing mouse (%d, %d, %x)", m.x, m.y, m.b);
		if(send_mouse_rep(m.b, m.x, m.y, 0) == -1) {
			error("send_mouse_rep()");
			return -1;
		}
	
	}
	return 0;
}
int process_mouse_nice(int fd, struct conn_info *info) 
{
        char *line = NULL;
        size_t dummy;
        ssize_t read;
	dbg("Hello");        
	if(!info->data) {
		info->data = fdopen(fd, "r");
		if(!info->data) {
			error("fdopen(\"r\")");
			return -1;
		} 			
	}
	
        while ((read = getline(&line, &dummy, info->data)) != -1) {
        	int x, y, b , f; 
        	x=y=b=f=0;
        	sscanf(line, "%d %d %d %d", &x, &y, &b, &f); 
        	dbg("Hello");       	
		if(f & HCR_SHUTDOWN) {
			dbg("receved SHUTDOWN rq");
			return SHUTDOWN;
		}
		dbg("processing mouse (%d, %d, %x)", x, y, b);
		if(send_mouse_rep(b, x, y, 0) == -1) {
			error("send_mouse_rep()");
			return -1;
		}
        }

        /* ----- Not freeing the line when exit()'ing, I know ------ */

	return 0;
}
struct conn_info conn_mouse = {
	.hdr_lookup = hdr_lookup_dropall,
	.process_src = process_mouse_nice,
};

static struct {
	char *name;
	struct conn_info *info;
} conn_lookup[] = {
	{ .name = "mouse", .info = &conn_mouse },
	{ .name = "multi-char", .info = &conn_multi_char },
	{ .name = "multi", .info = &conn_multi_bin }
};
static inline struct conn_info *lookup_conn_type(char *name) 
{
#define ARR_SIZE(a) sizeof(a)/sizeof(a[0])
	int i;
	for(i=0; i<ARR_SIZE(conn_lookup); ++i) 
		if(strcmp(name, conn_lookup[i].name) == 0)
			return conn_lookup[i].info;
	return NULL;
}

struct {
	char *addr;
	char *type;
} cmd_line;

int init_conn(struct conn_info **info)
{
	check_keyboard_report();
	check_mouse_report();
	check_gamepad_report();

	dbg("addr:%s",cmd_line.addr);
	if(str2ba(cmd_line.addr, &daddr) == -1) {	
		error( "Failed to get dest adress (%s)", cmd_line.addr);
		bacpy(&daddr, BDADDR_ANY);
		return -1;
	}
	*info = lookup_conn_type(cmd_line.type);
	if(!*info) {
		error( "Failed to get connection type (%s)", cmd_line.type);
		return -1;
	}
	info("Starting with %s conn type.",cmd_line.type);
	return 0;
}

void hipd_cleanup(void)
{
	close_sk();
	end_log();
}

static void print_signal(int s, siginfo_t *t, void *v)
{
	info("Signal %d(%s) caught.", s, strsignal(s));
}
static int global_sighandler(void (*sighandler)(int , siginfo_t *, void *))
{
	struct sigaction sa;
	int i;
	
	memset(&sa, 0, sizeof(sa));
	
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = sighandler;
	
	for(i=0; i< _NSIG; i++) 
		if(sigaction(i, &sa, NULL) == -1 && errno != EINVAL)
			return -1;
	return 0;
}

int main ( int argc, char * argv [] )
{
	struct pollfd p_fds[3];
	struct conn_info *conn_info;
	int (*process[3]) (int fd, struct conn_info *info);

	
	
	if(start_log("hidp_clientd.log") == -1) {
		exit(EXIT_FAILURE);
	}
	if(!argv[1] || !argv[2]) {
usage:
		info("Usage: %s <conn-type> <bluetooth-address>");
		exit(EXIT_FAILURE);		
	}
	cmd_line.type = argv[1];
	cmd_line.addr = argv[2];
	
	if(mkfifo(FIFO_PATH, FIFO_MODE) == -1 && errno != EEXIST) {
		error("mkfifo %s %o", FIFO_PATH, FIFO_MODE);
		exit(EXIT_FAILURE);
	}

	src_fd = open(FIFO_PATH, O_RDONLY);
	if(src_fd == -1) {
		error("open()");
		exit(EXIT_FAILURE);
	
	}

	
	printf("Hello\n");
	dbg("reading connection header");
	if(init_conn(&conn_info) == -1 ) {
		error("Failed to init connection.");
		goto usage;
	}
	
	/* Assign the header somewhere */
	
	if(start_cache(&common_cache, 16)) {
		error("Failed to start_cache(16)");
		exit(EXIT_FAILURE);
	}

	if(atexit(hipd_cleanup) != 0) {
		error("atexit");
		exit(EXIT_FAILURE);
	}	
	info("Start");
	
	cnt_sock = l2cap_connect(0x11); /* Control channel */
	if(cnt_sock == -1) {
		error("Failed to open cnt socket");
		exit(EXIT_FAILURE);
	}
	
	int_sock = l2cap_connect(0x13); /* Interrupt channel */
	if(int_sock == -1) {
		error("Failed to open int socket");
		exit(EXIT_FAILURE);
	}

	print_sockopts("INT:",int_sock);
	print_sockopts("CNT:",cnt_sock);
	
	info("cnt_sock(%i),int_sock(%i),src_fd(%i)",cnt_sock,int_sock,src_fd);	
		
	memset(p_fds, 0, sizeof(p_fds));
	
	p_fds[0].fd = cnt_sock;
	p_fds[1].fd = int_sock;
	p_fds[2].fd = src_fd;
	
	p_fds[0].events = p_fds[1].events = p_fds[2].events = POLLIN;
	
	process[0] = process[1] = process_sk;
	process[2] = conn_info->process_src;

	
	info("Started polling.\n");		
	
	for(;;) {
		int i, rv = poll(p_fds, 3, -1);
		if(rv == -1) {
			error("poll");
			exit(EXIT_FAILURE);
		}
	
		
		for(i=0 ;rv && i <3; ++i) {
			if(p_fds[i].revents & POLLIN) {
				info("========================> POLLIN:%d",i);
				switch(process[i](p_fds[i].fd, conn_info)) {
					case SHUTDOWN:
						info("shutdown");
						exit(EXIT_SUCCESS);
					case -1:
						error("Error processing fd %d", i);
				}
				info("POLL-OK\n");
				p_fds[i].revents = 0;
				rv --;
			} 
		}
			
			
	}
		
}
	
	
	

