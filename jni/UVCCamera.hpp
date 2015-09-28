#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>

#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <termios.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>
#include "uvcvideo.h"
#include "video.h"

#define  LOG_TAG    "UVCCamera"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#define IMG_WIDTH 320
#define IMG_HEIGHT 240

#define ERROR_LOCAL -1
#define SUCCESS_LOCAL 0

/* Data types for UVC control data */
#define UVC_CTRL_DATA_TYPE_RAW		0
#define UVC_CTRL_DATA_TYPE_SIGNED	1
#define UVC_CTRL_DATA_TYPE_UNSIGNED	2
#define UVC_CTRL_DATA_TYPE_BOOLEAN	3
#define UVC_CTRL_DATA_TYPE_ENUM		4
#define UVC_CTRL_DATA_TYPE_BITMASK	5

#define UVC_CONTROL_SET_CUR	(1 << 0)
#define UVC_CONTROL_GET_CUR	(1 << 1)
#define UVC_CONTROL_GET_MIN	(1 << 2)
#define UVC_CONTROL_GET_MAX	(1 << 3)
#define UVC_CONTROL_GET_RES	(1 << 4)
#define UVC_CONTROL_GET_DEF	(1 << 5)
/* Control should be saved at suspend and restored at resume. */
#define UVC_CONTROL_RESTORE	(1 << 6)

#define UVC_CONTROL_GET_RANGE	(UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | \
				 UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | \
				 UVC_CONTROL_GET_DEF)

/* Control flags */
#define UVC_CTRL_FLAG_SET_CUR		(1 << 0)
#define UVC_CTRL_FLAG_GET_CUR		(1 << 1)
#define UVC_CTRL_FLAG_GET_MIN		(1 << 2)
#define UVC_CTRL_FLAG_GET_MAX		(1 << 3)
#define UVC_CTRL_FLAG_GET_RES		(1 << 4)
#define UVC_CTRL_FLAG_GET_DEF		(1 << 5)
/* Control should be saved at suspend and restored at resume. */
#define UVC_CTRL_FLAG_RESTORE		(1 << 6)
/* Control can be updated by the camera. */
#define UVC_CTRL_FLAG_AUTO_UPDATE	(1 << 7)

#define UVC_CTRL_FLAG_GET_RANGE \
	(UVC_CTRL_FLAG_GET_CUR | UVC_CTRL_FLAG_GET_MIN | \
	 UVC_CTRL_FLAG_GET_MAX | UVC_CTRL_FLAG_GET_RES | \
	 UVC_CTRL_FLAG_GET_DEF)

#define V4L2_CID_PAN_RELATIVE_LOGITECH   0x0A046D01
#define V4L2_CID_TILT_RELATIVE_LOGITECH  0x0A046D02
#define V4L2_CID_PANTILT_RESET_LOGITECH  0x0A046D03
#define V4L2_CID_FOCUS_LOGITECH          0x0A046D04
#define V4L2_CID_LED1_MODE_LOGITECH      0x0A046D05
#define V4L2_CID_LED1_FREQUENCY_LOGITECH 0x0A046D06

#define UVC_GUID_LOGITECH_MOTOR_CONTROL {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x56}
#define UVC_GUID_LOGITECH_USER_HW_CONTROL {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x1f}

#define XU_HW_CONTROL_LED1               1
#define XU_MOTORCONTROL_PANTILT_RELATIVE 1
#define XU_MOTORCONTROL_PANTILT_RESET    2
#define XU_MOTORCONTROL_FOCUS            3

#define ONE_DEGREE (64);
#define MAX_PAN  (70*64)
#define MIN_PAN  (-70*64)
#define MAX_TILT (30*64)
#define MIN_TILT (-30*64)
#define MIN_RES  (64*5)

#define UVCIOC_CTRL_ADD		_IOW  ('U', 1, struct uvc_xu_control_info)
//#define UVCIOC_CTRL_MAP		_IOWR ('U', 2, struct uvc_xu_control_mapping)
#define UVCIOC_CTRL_GET		_IOWR ('U', 3, struct uvc_xu_control)
#define UVCIOC_CTRL_SET		_IOW  ('U', 4, struct uvc_xu_control)

struct buffer {
        void *                  start;
        size_t                  length;
};

/**
 * For V4L
*/
static char            dev_name[16];
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;


 /**
  * For yuyv_to_rgb24
  */
int *rgb = NULL;
int *ybuf = NULL;


#ifdef __cplusplus
extern "C" {
#endif

/* some Logitech webcams have pan/tilt/focus controls */
static struct uvc_xu_control_info xu_ctrls[] = {
  {
    .entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
    .index    = 0,
    .selector = XU_MOTORCONTROL_PANTILT_RELATIVE,
    .size     = 4,
    .flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_DEF
  }
};

static struct uvc_xu_control_mapping xu_mappings[] = {
	{
		.id		= V4L2_CID_PAN_ABSOLUTE,
		"Pan (Absolute)",
		.entity		= UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector	= UVC_CT_PANTILT_ABSOLUTE_CONTROL,
		.size		= 32,
		.offset		= 0,
		.v4l2_type	= V4L2_CTRL_TYPE_INTEGER,
		.data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
	},
	{
		.id		= V4L2_CID_TILT_ABSOLUTE,
		"Tilt (Absolute)",
		.entity		= UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector	= UVC_CT_PANTILT_ABSOLUTE_CONTROL,
		.size		= 32,
		.offset		= 32,
		.v4l2_type	= V4L2_CTRL_TYPE_INTEGER,
		.data_type	= UVC_CTRL_DATA_TYPE_UNSIGNED,
	}
};

int errnoexit(const char *s);

int xioctl(int fd, int request, void *arg);

int opendevice(int videoid);
int initdevice(void);
int initmmap(void);
int startcapturing(void);

int readframeonce(void);
int readframe(void);

int stopcapturing(void);
int uninitdevice(void);
int closedevice(void);

void yuyv_to_rgb24 (unsigned char *src);

jint Java_org_siprop_android_uvccamera_UVCCameraPreview_prepareCamera( JNIEnv* env,jobject thiz, jint videoid);
jint Java_org_siprop_android_uvccamera_UVCCameraPreview_prepareCameraWithBase( JNIEnv* env,jobject thiz, jint videoid, jint videobase);
void Java_org_siprop_android_uvccamera_UVCCameraPreview_processCamera( JNIEnv* env,jobject thiz);
void Java_org_siprop_android_uvccamera_UVCCameraPreview_stopCamera(JNIEnv* env,jobject thiz);
void Java_org_siprop_android_uvccamera_UVCCameraPreview_pixeltobmp( JNIEnv* env,jobject thiz,jobject bitmap);
jint Java_org_siprop_android_uvccamera_MainActivity_getCurrentControlValue(JNIEnv* env,jobject thiz , jint controlId);
jintArray Java_org_siprop_android_uvccamera_MainActivity_startControlCamera(JNIEnv* env,jobject thiz , jint controlId ,jint value);
jboolean Java_org_siprop_android_uvccamera_MainActivity_initDevice(JNIEnv* env,jobject thiz);
void Java_org_siprop_android_uvccamera_MainActivity_queryControl(JNIEnv* env,jobject thiz);
#ifdef __cplusplus
}
#endif

