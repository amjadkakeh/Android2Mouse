#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


#include <sys/types.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <poll.h>
#include <pthread.h>

#include <string.h>
#include <jni.h>
#define ANDROID_LOG
#include "log.h"

#include "hidp_client.h"


  /* 
   com.example.common.hidp.HidpBcaster.InitBcast 
   com.example.common.hidp.HidpBcaster.InitBcast
   */
JNIEXPORT jboolean JNICALL Java_com_example_common_hidp_HidpBcaster_InitBcast( JNIEnv* env,
                                                  jobject thiz,
                                                  jstring javaString )
{
	jboolean rv = 1;
	 /*Get the native string from javaString*/
	const char *nativeString = (*env)->GetStringUTFChars(env, javaString, 0);
	
	dbg("addr:%s", nativeString);
	

	
	if( start_session(nativeString) == -1) {
		error("Failed to init_hidp with address(%s)", nativeString);
		goto r;
	}
	
	rv = 0;
r:
	(*env)->ReleaseStringUTFChars(env, javaString, nativeString);
    	return rv;
}
JNIEXPORT jboolean JNICALL Java_com_example_common_hidp_HidpBcaster_EndBcast( JNIEnv* env, jobject thiz )
{
	dbg("exitting");
	end_session();
    	return 0;
}


JNIEXPORT void JNICALL Java_com_example_common_hidp_HidpBcaster_reportGpad( JNIEnv* env, jobject thiz, 
			jint x,jint y,jint z,jint rx,jint ry,jint rz,jint buttons)
{
	//dbg("x:%d y:%d z:%d ry:%d rx:%d rz:%d, buttons: 0x%x", x,y,z,rx,ry,rz,buttons);
	send_gpad_report(x,y,z,rx,ry,rz,buttons);			
}

JNIEXPORT void JNICALL Java_com_example_common_hidp_HidpBcaster_reportMouse( JNIEnv* env, jobject thiz, 
			jint x, jint y, jint buttons, jint wheel)
{
	//dbg("x:%d y:%d, buttons: 0x%x, wheel:%d", x, y, buttons, wheel);
	send_mouse_report(x,y,buttons,wheel);			
}
JNIEXPORT void JNICALL Java_com_example_common_hidp_HidpBcaster_reportRawChar( JNIEnv* env, jobject thiz, 
			jchar ch, jint mod)
{
	//dbg("ch:%c, mod: 0x%x", ch, mod);
 	send_char_raw_report(ch, mod);
}
JNIEXPORT void JNICALL Java_com_example_common_hidp_HidpBcaster_reportAsciiChar( JNIEnv* env, jobject thiz, 
			jchar ch, jint mod)
{
	//dbg("ch:%c, mod: 0x%x", ch, mod);
 	send_char_ascii_report(ch, mod);
}
JNIEXPORT void JNICALL Java_com_example_common_hidp_HidpBcaster_reportString( JNIEnv* env, jobject thiz, 
			jstring javaString )
{
	const char *string = (*env)->GetStringUTFChars(env, javaString, 0);
	
	//dbg("string:%s", string);
	send_str_report(string);

	(*env)->ReleaseStringUTFChars(env, javaString, string);
}

