#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "bluetooth.h"
#include "l2cap.h"

#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>


#include "log.h"

#include "hidp.h"

#include "hidp_client.h"
#include "combo_conn.h"
#include "ascii_map.inc.h"

/* -------- Multi conn_info implementation ----------- */

#define KBD_MOD_CTRL		1 //0x80
#define KBD_MOD_SHIFT		2 //0x81
#define KBD_MOD_ALT		4 //0x82
#define KBD_MOD_GUI		8 //0x83

#define KBD_GETCH_MODS(c) ( c=='c' ? KBD_MOD_CTRL : c=='a' ? KBD_MOD_ALT : c=='s' ? KBD_MOD_SHIFT : c=='g' ? KBD_MOD_GUI : 0 )



static int press_kbd_char(struct keyboard_report *kbd, uint8_t keycode, uint8_t modifier)
{		

	kbd->modifier |= modifier;
	if(kbd->keycodes[0] != keycode && kbd->keycodes[1] != keycode &&
	   kbd->keycodes[2] != keycode && kbd->keycodes[3] != keycode &&
	   kbd->keycodes[4] != keycode && kbd->keycodes[5] != keycode ) {
		int i;
		for(i=0; i<6; ++i) {
			if(kbd->keycodes[i] == 0x00) {
				kbd->keycodes[i] = keycode;
				break;
			} 
		}
		if(i == 6) {
			info("Pressed more then 6 buttons");
			return 1;
		}				
	}
	dbg("processing keyboard (%d, %d)",kbd->reserved, kbd->modifier);
	dbg("processing keycodes (0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)",kbd->keycodes[0],kbd->keycodes[1],kbd->keycodes[2],
								  kbd->keycodes[3],kbd->keycodes[4],kbd->keycodes[5]);
	if(send_input_rep(kbd) == -1) {
		error("send_input_rep()");
		return -1;
	}
	
	return 0;
}
int release_kbd_char(struct keyboard_report *kbd, uint8_t keycode, uint8_t modifier)
{		

	kbd->modifier &= ~modifier;
	
	if(keycode){
		int i;
		for(i=0; i<6; ++i) {
			if(kbd->keycodes[i] == keycode) {
				kbd->keycodes[i] = 0x00;				
				break;
			} 
		}			
	}
	dbg("processing keyboard %d (%d, %d)", kbd->report_id, kbd->reserved, kbd->modifier);
	dbg("processing keycodes (0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)",kbd->keycodes[0],kbd->keycodes[1],kbd->keycodes[2],
								  kbd->keycodes[3],kbd->keycodes[4],kbd->keycodes[5]);
	if(send_input_rep(kbd) == -1) {
		error("send_input_rep()");
		return -1;
	}
	
	return 0;
}

static int send_char_raw(struct keyboard_report *kbd, uint8_t key, uint8_t modifier) 
{
	int r = press_kbd_char(kbd, key, modifier);
	(long) release_kbd_char(kbd, key, modifier);
	return r;
}
static int _send_char_decode(struct keyboard_report *kbd, uint8_t keycode, uint8_t modifier, const uint8_t *map, size_t map_len) 
{
	if(keycode < map_len) {
	
		int key = map[keycode];
		info("%c/0x%x -----> 0x%x",keycode ,keycode ,key);
		if (key & SHIFT ) {
			modifier |= 0x02;
			key &= ~SHIFT;
		}		
		
		return send_char_raw(kbd, key, modifier);
	}
	return 0;
}
static inline int send_char_ascii(struct keyboard_report *kbd, uint8_t keycode, uint8_t modifier) 
{
	return  _send_char_decode(kbd, keycode, modifier, kbd_asciimap, sizeof(kbd_asciimap));
}
static int send_string(struct keyboard_report *kbd, char *string, size_t len)
{
	int i;
	
	for(i=0; i<len; ++i) {	
		if(send_char_ascii(kbd, string[i], 0)) {
			break;
		}
		
	}
	return i;
}
#ifdef MULTI_CHAR
static int process_multi_char(int fd, struct conn_info *info) 
{
	struct keyboard_report kbd; //= { .report_id = KBD_REP_ID }; BUGS in gcc ?!
	struct mouse_report    mouse;// = { .report_id = MOUSE_REP_ID };
	struct gamepad_report  gpad;// = { .report_id = GPAD_REP_ID };
	int i, f, t;
	char c, mod;
        char *line = NULL, *tmp_line, *next;
        size_t dummy, len;
        ssize_t read;
        static int8_t st = 0;
	dbg("Hello");        
	if(!info->data) {
		info->data = fdopen(fd, "r");
		if(!info->data) {
			error("fdopen(\"r\")");
			return -1;
		} 			
	}


#define __ZERO(r, R) \
	memset(&r, 0, sizeof(r));\
	r.report_id = R;
	__ZERO(mouse, MOUSE_REP_ID);	
	__ZERO(kbd, KBD_REP_ID);
	__ZERO(gpad, GPAD_REP_ID);
	
        while ((read = getline(&line, &dummy, (FILE*) info->data)) != -1) {

		dbg("-----[%c]", line[0]);
        	switch(line[0]) {
        		case 'f':
      				f=0;
		        	sscanf(&line[1], "%d", &f);
		        	       	
				if(f & HCR_SHUTDOWN) {
					dbg("receved SHUTDOWN rq");
					return METHOD_SHUTDOWN;
				}
        			break;
        			
        		case 'm': /* Mouse */
		        	sscanf(&line[1], "%d %d %d %d", &mouse.x, &mouse.y, &mouse.buttons, &mouse.wheel);
   			   	mouse.x = mouse.y = 22;
		   		dbg("processing mouse (%d, %d, 0x%x, %d)", mouse.x, mouse.y, mouse.buttons, mouse.wheel);
		   		dbg("%s", &line[1]);

				if(send_input_rep(&mouse) == -1) {
					error("send_input_rep()");
					return -1;
				}
        			__ZERO(mouse, MOUSE_REP_ID);	
				break;		
			case 'c': /* Keyboard */	
				c = mod = t = 0;
				sscanf(&line[1], " %c %c %d", &c, &mod, &t);
				dbg("%c %c %d", c, mod, t); 
				if(t)
					(long)press_kbd_char(&kbd, kbd_asciimap[c], KBD_GETCH_MODS(mod));
				else 
					(long)release_kbd_char(&kbd, kbd_asciimap[c], KBD_GETCH_MODS(mod));			
				break;
			case 'C':			
				c = mod = 0;
				sscanf(&line[1], " %c %c", &c, &mod);
				dbg("%c %c", c, mod); 
				send_char_ascii(&kbd,c, KBD_GETCH_MODS(mod));
				break;
			case 'S':			
				len = strlen(&line[2]);		
				(&line[2])[--len]='\0';         
				dbg("%d -> '%s'",len, &line[2]); 
				send_string(&kbd,&line[2],len);
				break;				
			case 'k':
				sscanf(&line[1], "%d %d", &kbd.reserved, &kbd.modifier);         			
				
		   		dbg("processing keyboard (%d, %d)",kbd.reserved, kbd.modifier);
		   		break;
			case 'K':
				i=0; tmp_line = &line[1];
				
				while( i < 6){					
					kbd.keycodes[i++] = strtol(tmp_line , &next, 0);			
					tmp_line = next;
				} 
				dbg("processing keyboard (%d, %d)",kbd.reserved, kbd.modifier);
		   		dbg("processing keycodes (0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)",kbd.keycodes[0],
		   									kbd.keycodes[1],kbd.keycodes[2],kbd.keycodes[3],kbd.keycodes[4], kbd.keycodes[5]);
				if(send_input_rep(&kbd) == -1) {
					error("send_input_rep()");
					return -1;
				}
				__ZERO(kbd, KBD_REP_ID);
				break;
			case 'g': /* Gamepad */
				sscanf(&line[1], "%d %d %d %d %d %d %x", &gpad.x,&gpad.y,&gpad.z,&gpad.rx,&gpad.ry,&gpad.rz,
									 &gpad.buttons);
									 
  				gpad.x=gpad.y=gpad.z=gpad.rx=gpad.ry=gpad.rz = st++;					 
		   		dbg("processing gamepad (%d %d %d %d %d %d %x)", gpad.x,gpad.y,gpad.z,gpad.rx,gpad.ry,gpad.rz,
									 gpad.buttons);
				if(send_input_rep(&gpad) == -1) {
					error("send_mouse_rep()");
					return -1;
				}
				__ZERO(gpad, GPAD_REP_ID);
        	}
#undef __ZERO
        	
        }
        
	free(line);
	
	return 0;
}
struct conn_info conn_multi_char = {
	.hdr_lookup = hdr_lookup_dropall,
	.process_src = process_multi_char,
};
#endif /* MULTI_CHAR */
struct conn_info conn_multi_char = {
	.hdr_lookup = hdr_lookup_dropall,
	.process_src = NULL
};
static int process_multi_bin(int fd, struct conn_info *info) 
{
	struct keyboard_report kbd; //= { .report_id = KBD_REP_ID }; BUGS in gcc ?!
	struct mouse_report    mouse;
	struct gamepad_report  gpad;
        ssize_t numRead, i;

       	uint8_t report_id;
       	struct bin_packet packet;
	dbg("Hello");        
#define __ZERO(r, R) \
	memset(&r, 0, sizeof(r));\
	r.report_id = R;
	__ZERO(mouse, MOUSE_REP_ID);	
	__ZERO(kbd, KBD_REP_ID);
	__ZERO(gpad, GPAD_REP_ID);
	
        while ((numRead = read(fd, &packet , sizeof(packet)))>0) {

		dbg("-----[%d]", packet.report_id);
		
        	switch(packet.report_id) {
        		case KBD_REP_ID: /* Keyboard */	  
        			dbg("--Keyboard--");
				if(packet.flags & BRPT_STRING) {
					packet.report.string.s[MAX_STRING-1] = 0;
					send_string(&kbd,packet.report.string.s,strlen(packet.report.string.s));
				} else if(packet.flags & BRPT_CHAR) {
					dbg("Char -> %c/0x%x %d (raw %d)", packet.report.letter.c, 
								  packet.report.letter.c, packet.report.letter.mods,
								  (packet.flags2 & CHAR_RAW));
					if(packet.flags2 & CHAR_RAW) {
						send_char_raw(&kbd, packet.report.letter.c,packet.report.letter.mods);
					} else if(packet.flags2 & CHAR_ASCII) {
						send_char_ascii(&kbd,packet.report.letter.c,packet.report.letter.mods);
					} else {
						info("WARNING!");
						info("Bad flag2 0x%x! Interpreting letter as ascii.", packet.flags);
						send_char_ascii(&kbd,packet.report.letter.c,packet.report.letter.mods);
					}
				} else {
					for(i=0; i < 6;++i)
						kbd.keycodes[i] = packet.report.keyboard.keycodes[i];
					kbd.modifier = packet.report.keyboard.modifier;
					
					dbg("processing keyboard (%d, %d)",(int)kbd.reserved, (int)kbd.modifier);
			   		dbg("processing keycodes (0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)",(int)kbd.keycodes[0],
		   									(int)kbd.keycodes[1],(int)kbd.keycodes[2],(int)kbd.keycodes[3],(int)kbd.keycodes[4], (int)kbd.keycodes[5]);
					if(send_input_rep(&kbd) == -1) {
						error("send_input_rep()");
						return METHOD_ERROR;
					}
				}	
				__ZERO(kbd, KBD_REP_ID);
				break;        		
        		case  GPAD_REP_ID: /* Gamepad */       									 
			#define S(x) gpad.x = packet.report.gamepad.x
				S(x);S(y);S(z);
				S(rx);S(ry);S(rz);
				S(buttons);
			#undef S
		   		dbg("processing gamepad (%d %d %d %d %d %d %x)", (int) gpad.x, (int)gpad.y,(int)gpad.z,
		   		(int)gpad.rx,(int)gpad.ry,(int)gpad.rz,
									 (int)gpad.buttons);
				if(send_input_rep(&gpad) == -1) {
					error("send_mouse_rep()");
					return METHOD_ERROR;
				}
				__ZERO(gpad, GPAD_REP_ID);
				break;
		        	
			case  MOUSE_REP_ID: /* Mouse */ 

   			   	mouse.x = packet.report.mouse.x;
   			   	mouse.y = packet.report.mouse.y;
  			   	mouse.wheel = packet.report.mouse.wheel;
   			   	mouse.buttons = packet.report.mouse.buttons;
   			   	
		   		dbg("processing mouse (%d, %d, 0x%x, %d)", 
		   			(int)mouse.x,(int) mouse.y,(int) mouse.buttons,(int) mouse.wheel);


				if(send_input_rep(&mouse) == -1) {
					error("send_input_rep()");
					return METHOD_ERROR;
				}
        			__ZERO(mouse, MOUSE_REP_ID);	
				break;					
			case SYS_REP_ID: /* for IPC */	        				
				if(packet.flags & HCR_SHUTDOWN){				
					dbg("receved SHUTDOWN rq");
					return METHOD_SHUTDOWN;
				}
        			break;
				
        	}
#undef __ZERO
        	
        }
        	
	return METHOD_SUCCESS;
}

struct conn_info conn_multi_bin = {
	.hdr_lookup = hdr_lookup_dropall,
	.process_src = process_multi_bin,
};
