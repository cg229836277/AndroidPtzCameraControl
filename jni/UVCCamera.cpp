#include "UVCCamera.hpp"
#include <unistd.h>

#define LENGTH_OF(x) (sizeof(x)/sizeof(x[0]))

const char* TAG = "UVCCamera";
jintArray controlData;
jint setBeforeAfterData[2];

int ret = -1;

const char* CONTROL_FLAG_PAN = "Pan (Absolute)";
const char* CONTROL_FLAG_TILT = "Tilt (Absolute)";
const char* CONTROL_FLAG_ZOOM = "Zoom (Absolute)";

int errnoexit(const char *s) {
	LOGE("%s error %d, %s", s, errno, strerror (errno));
	return ERROR_LOCAL;
}


int xioctl(int fd, int request, void *arg) {
	int r;

	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

int opendevice(int i) {
	struct stat st;

	sprintf(dev_name,"/dev/video%d",i);

	if (-1 == stat (dev_name, &st)) {
		LOGE("Cannot identify '%s': %d, %s", dev_name, errno, strerror (errno));
		return ERROR_LOCAL;
	}
	LOGE("%s device is ", dev_name);
	if (!S_ISCHR (st.st_mode)) {
		LOGE("%s is no device", dev_name);
		return ERROR_LOCAL;
	}

	fd = open (dev_name, O_RDWR | O_NONBLOCK, 0);
//	fd = open("dev/video0" , O_RDWR);

	if (-1 == fd) {
		LOGE("Cannot open '%s': %d, %s", dev_name, errno, strerror (errno));
		return ERROR_LOCAL;
	}
	return SUCCESS_LOCAL;
}

int initdevice(void) {
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			LOGE("%s is no V4L2 device", dev_name);
			return ERROR_LOCAL;
		} else {
			return errnoexit ("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		LOGE("%s is no video capture device", dev_name);
		return ERROR_LOCAL;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		LOGE("%s does not support streaming i/o", dev_name);
		return ERROR_LOCAL;
	}

	CLEAR (cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;

		if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					break;
				default:
					break;
			}
		}
	} else {
	}

	CLEAR (fmt);

	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	fmt.fmt.pix.width       = IMG_WIDTH;
	fmt.fmt.pix.height      = IMG_HEIGHT;

	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

	if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
		return errnoexit ("VIDIOC_S_FMT");

	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	return initmmap ();

}

int initmmap(void) {
	struct v4l2_requestbuffers req;

	CLEAR (req);

	req.count               = 4;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			LOGE("%s does not support memory mapping", dev_name);
			return ERROR_LOCAL;
		} else {
			return errnoexit ("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		LOGE("Insufficient buffer memory on %s", dev_name);
		return ERROR_LOCAL;
 	}
	//(buffer*)
	buffers = calloc (req.count, sizeof (*buffers));

	if (!buffers) {
		LOGE("Out of memory");
		return ERROR_LOCAL;
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		 CLEAR (buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
			return errnoexit ("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start =
		mmap (NULL ,
			buf.length,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			return errnoexit ("mmap");
	}

	return SUCCESS_LOCAL;
}

int startcapturing(void) {
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;

		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			return errnoexit ("VIDIOC_QBUF");
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
		return errnoexit ("VIDIOC_STREAMON");

	return SUCCESS_LOCAL;
}

int readframeonce(void) {
	for (;;) {
		fd_set fds;
		struct timeval tv;
		int r;

		FD_ZERO (&fds);
		FD_SET (fd, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select (fd + 1, &fds, NULL, NULL, &tv);

		if (-1 == r) {
			if (EINTR == errno)
				continue;

			return errnoexit ("select");
		}

		if (0 == r) {
			LOGE("select timeout");
			return ERROR_LOCAL;

		}

		if (readframe ()==1)
			break;

	}

	return SUCCESS_LOCAL;

}

int readframe(void) {

	struct v4l2_buffer buf;
	unsigned int i;

	CLEAR (buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:
			default:
				return errnoexit ("VIDIOC_DQBUF");
		}
	}

	assert (buf.index < n_buffers);

	yuyv_to_rgb24((unsigned char*)(buffers[buf.index].start));

//	LOGI("Image Size:%d", buffers[buf.index].length);

	if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
		return errnoexit ("VIDIOC_QBUF");

	return 1;
}

int stopcapturing(void) {
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
		return errnoexit ("VIDIOC_STREAMOFF");

	return SUCCESS_LOCAL;

}

int uninitdevice(void) {
	unsigned int i;

	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap (buffers[i].start, buffers[i].length))
			return errnoexit ("munmap");

	free (buffers);

	return SUCCESS_LOCAL;
}

int closedevice(void) {
	__android_log_print(ANDROID_LOG_ERROR , TAG , "关闭设备");
	if (-1 == close (fd)){
		fd = -1;
		return errnoexit ("close");
	}

	fd = -1;
	return SUCCESS_LOCAL;
}



#define SAT(c) \
        if (c & (~255)) { if (c < 0) c = 0; else c = 255; }
void yuyv_to_rgb24 (unsigned char *src) {
   unsigned char *s;

	if((!rgb || !ybuf)){
		return;
	}
   int *lrgb = NULL;
   int l, c;
   int r, g, b, cr, cg, cb, y1, y2;

   l = IMG_HEIGHT;
   s = src;
   lrgb = &rgb[0];
   while (l--) {
      c = IMG_WIDTH >> 1;
      while (c--) {
         y1 = *s++;
         cb = ((*s - 128) * 454) >> 8;
         cg = (*s++ - 128) * 88;
         y2 = *s++;
         cr = ((*s - 128) * 359) >> 8;
         cg = (cg + (*s++ - 128) * 183) >> 8;

         r = y1 + cr;
         b = y1 + cb;
         g = y1 - cg;
         SAT(r);
         SAT(g);
         SAT(b);

         *lrgb++ = 0xff000000 | b<<16 | g<<8 | r;

         r = y2 + cr;
         b = y2 + cb;
         g = y2 - cg;
         SAT(r);
         SAT(g);
         SAT(b);

         *lrgb++ = 0xff000000 | b<<16 | g<<8 | r;
      }
   }
}




void Java_org_siprop_android_uvccamera_UVCCameraPreview_pixeltobmp( JNIEnv* env,jobject thiz,jobject bitmap){
	jboolean bo;
	AndroidBitmapInfo  info;
	void*              pixels;
	int                ret;
	int i;
	int *colors;

	int width=0;
	int height=0;

	if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
		LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
		return;
	}

	width = info.width;
	height = info.height;

	if(!rgb || !ybuf) return;

	if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
		LOGE("Bitmap format is not RGBA_8888 !");
		return;
	}

	if ((ret = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) {
		LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
	}

	colors = (int*)pixels;
	int *lrgb =NULL;
	lrgb = &rgb[0];

	for(i=0 ; i<width*height ; i++){
		*colors++ = *lrgb++;
	}

	AndroidBitmap_unlockPixels(env, bitmap);
}

int getControlValue(int controlId){
	//an array of v4l2_ext_control
	struct v4l2_ext_control clist[1];
	struct v4l2_ext_controls ctrls;

	memset(&clist, 0, sizeof(clist));
	memset(&ctrls, 0, sizeof(ctrls));

	clist[0].id    = controlId;
	clist[0].value = 0;

	//v4l2_ext_controls with list of v4l2_ext_control
	ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
	ctrls.count = 1;
	ctrls.controls = clist;

	//read back the value
	if (-1 == xioctl (fd, VIDIOC_G_EXT_CTRLS, &ctrls))
	{
		__android_log_print(ANDROID_LOG_ERROR,TAG,"get current value failed fd = %d,reason=%s" , fd,strerror(errno));
		return -1;
	}
	__android_log_print(ANDROID_LOG_ERROR,TAG,"get before value success , %d" , clist[0].value);
	return clist[0].value;
}

int startControl(int controlId , int value){
	__android_log_print(ANDROID_LOG_ERROR , TAG , "start control");
	//jint setBefore;
	if(fd < 0){
		__android_log_print(ANDROID_LOG_ERROR , TAG , "device open fail");
		return -1;
	}
	//an array of v4l2_ext_control
	struct v4l2_ext_control clist[1];
	struct v4l2_ext_controls ctrls;

	CLEAR(clist);
	CLEAR(ctrls);

	clist[0].id    = controlId;
	int currentValue = getControlValue(controlId);
	setBeforeAfterData[0] = currentValue;
	if(currentValue != -1){
		if(value == -64 || value == 64){
			clist[0].value = currentValue + value;
		}else{
			clist[0].value = currentValue + value * 3600;
		}
		setBeforeAfterData[1] = clist[0].value;
		__android_log_print(ANDROID_LOG_ERROR , TAG , "currentValue = %d , setValue = %d" , currentValue , clist[0].value);
	}else{
		__android_log_print(ANDROID_LOG_ERROR , TAG , "get current value error");
		return -1;
	}

	//v4l2_ext_controls with list of v4l2_ext_control
	ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
	ctrls.count = 1;
	ctrls.controls = clist;

	int result = xioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);
	__android_log_print(ANDROID_LOG_ERROR , TAG , "result = %d" , result);
	if (result == -1){
		__android_log_print(ANDROID_LOG_ERROR , TAG , "VIDIOC_S_EXT_CTRLS failed while tilting down , reason = %s" , strerror (errno));
		return -1;
	}else{
		__android_log_print(ANDROID_LOG_ERROR , TAG , "VIDIOC_S_EXT_CTRLS success while tilting down , after value = %d" , clist[0].value);
	}

	int afterValue = getControlValue(controlId);
	__android_log_print(ANDROID_LOG_ERROR , TAG , "after set value = %d" , afterValue);
	if(afterValue == setBeforeAfterData[1]){
		return 0;
	}else{
		CLEAR(clist);
		CLEAR(ctrls);

		clist[0].id    = controlId;
		clist[0].value = setBeforeAfterData[1];

		ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
		ctrls.count = 1;
		ctrls.controls = clist;

		int result = xioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls);

		__android_log_print(ANDROID_LOG_ERROR , TAG , "after set second value = %d" , result);
	}
	return 0;
}

jboolean queryControls(){
	jint canControl = 0;
	__android_log_print(ANDROID_LOG_ERROR , TAG , "设备号=%d" , fd);

	struct v4l2_queryctrl qctrl;
	qctrl.id = V4L2_CTRL_CLASS_CAMERA | V4L2_CTRL_FLAG_NEXT_CTRL;
	int i = ioctl(fd, VIDIOC_QUERYCTRL, &qctrl);
	while (0 == i){
		__android_log_print(ANDROID_LOG_ERROR , TAG , "开始查找");
		if (V4L2_CTRL_ID2CLASS(qctrl.id) != V4L2_CTRL_CLASS_CAMERA)
			continue;

		if(strcmp(qctrl.name , CONTROL_FLAG_PAN) == 0 || strcmp(qctrl.name , CONTROL_FLAG_TILT) == 0
							|| strcmp(qctrl.name , CONTROL_FLAG_ZOOM) == 0){
			++canControl;
		}

		__android_log_print(ANDROID_LOG_ERROR , TAG , "找到的控制函数是%s" , qctrl.name);
		__android_log_print(ANDROID_LOG_ERROR , TAG , "继续查找");
		__android_log_print(ANDROID_LOG_ERROR , TAG , "id = %d" , qctrl.id);
		__android_log_print(ANDROID_LOG_ERROR , TAG , "Next_Ctrl = %x" , V4L2_CTRL_FLAG_NEXT_CTRL);
		__android_log_print(ANDROID_LOG_ERROR , TAG , "Camera_Class = %x" , V4L2_CTRL_CLASS_CAMERA);

		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;

		__android_log_print(ANDROID_LOG_ERROR , TAG , "id+ = %x" , qctrl.id);

		i = ioctl(fd, VIDIOC_QUERYCTRL, &qctrl);
		if(i != 0){
			__android_log_print(ANDROID_LOG_ERROR, TAG,"uvcioc ctrl add error: errno=%d (reason=%s)\n", errno,strerror(errno));
		}
	}
	//如果存在ptz控制的话，应该会有Pan,Tilt,Zoom字符串，变量自加三次
	return canControl == 3;
}

jint Java_org_siprop_android_uvccamera_UVCCameraPreview_prepareCamera( JNIEnv* env,jobject thiz, jint videoid){
	ret = opendevice(videoid);

	if(ret != ERROR_LOCAL){
		ret = initdevice();
	}
	if(ret != ERROR_LOCAL){
		ret = startcapturing();

		if(ret != SUCCESS_LOCAL){
			stopcapturing();
			uninitdevice ();
			closedevice ();
			LOGE("device resetted");
		}

	}

	if(ret != ERROR_LOCAL){
		rgb = (int *)malloc(sizeof(int) * (IMG_WIDTH*IMG_HEIGHT));
		ybuf = (int *)malloc(sizeof(int) * (IMG_WIDTH*IMG_HEIGHT));
	}
	return ret;
}

void
Java_org_siprop_android_uvccamera_UVCCameraPreview_processCamera( JNIEnv* env,jobject thiz){
	readframeonce();
}

void
Java_org_siprop_android_uvccamera_UVCCameraPreview_stopCamera(JNIEnv* env,jobject thiz){

	stopcapturing ();

	uninitdevice ();

	closedevice ();

	if(rgb) free(rgb);
	if(ybuf) free(ybuf);

	fd = -1;
}

jboolean Java_org_siprop_android_uvccamera_MainActivity_initDevice(JNIEnv* env,jobject thiz){
	return ret == 0 ? true : false;
}

//返回控制是否成功
jintArray Java_org_siprop_android_uvccamera_MainActivity_startControlCamera(JNIEnv* env,jobject thiz , jint controlId ,jint value){
	switch (controlId) {
		case 10094856:
			//控制左右转动
			__android_log_print(ANDROID_LOG_ERROR , TAG , "控制左右的值是 = %d" , value);
			controlId = V4L2_CID_PAN_ABSOLUTE;
			//controlType = "V4L2_CID_PAN_ABSOLUTE ";
			break;
		case 10094857:
			//控制上下转动
			__android_log_print(ANDROID_LOG_ERROR , TAG , "控制上下的值是 = %d" , value);
			controlId = V4L2_CID_TILT_ABSOLUTE;
			//controlType = "V4L2_CID_TILT_ABSOLUTE ";
			break;
		case 10094861:
			//控制聚焦
			controlId = V4L2_CID_ZOOM_ABSOLUTE;
			break;
		default:
			break;
	}
	int result = startControl(controlId , value);
	controlData = env->NewIntArray(2);
	if (controlData == NULL) {
		return NULL; /* out of memory error thrown */
	}
	if (result == -1) {
		setBeforeAfterData[0] = 0;
		setBeforeAfterData[1] = 0;
		env->SetIntArrayRegion(controlData, 0, 2, setBeforeAfterData);
		return controlData;
	}
	env->SetIntArrayRegion(controlData, 0, 2, setBeforeAfterData);
	return controlData;
}

jint Java_org_siprop_android_uvccamera_MainActivity_getCurrentControlValue(JNIEnv* env,jobject thiz , jint controlId){
	return getControlValue(controlId);
}


jboolean Java_org_siprop_android_uvccamera_MainActivity_isSupportPtz(JNIEnv* env,jobject thiz){
	return queryControls();
}
