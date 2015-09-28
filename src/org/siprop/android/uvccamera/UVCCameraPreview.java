package org.siprop.android.uvccamera;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.Toast;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;

class UVCCameraPreview extends SurfaceView implements SurfaceHolder.Callback, Runnable {

	private static final boolean DEBUG = true;
	private static final String TAG="UVCCamera";
	
	//正数向左，负数向右,范围是-583200 , 583200
	private final int CONTROL_CAMERA_PAN = 10094856;
	//正数向上，负数向下,范围是-86400 , 324000
	private final int CONTROL_CAMERA_TILT = 10094857;
	//变焦范围是0 , 16384
	private final int CONTROL_CAMERA_ZOOM = 10094861;
	
	//左右转动的角度是170，每移动一次的值是34300，为10度
	private final int PAN_PER_LEFT = -34300;
	private final int PAN_PER_RIGHT = 34300;
	//上下转动的角度是-30 ~ +90
	//向下每次转动的角度是10度
	private final int TILT_PER_DOWN = -28800 + 1;
	//每次向上转动的角度是10度
	private final int TILT_PER_UP = 36000 - 1;
	//每一次变焦是1024，可以变16次
	private final int ZOOM_PER_TIME = 1024;
	
	private final int PAN_LEFT_MAX = -583200;
	private final int PAN_RIGHT_MAX = 583200;
	private final int TILT_UP_MAX = 324000;
	private final int TILT_DOWN_MAX = -86400;
	private final int ZOOM_MIN = 0;
	private final int ZOOM_MAX = 16384;
	
	protected Context context;
	private SurfaceHolder holder;
    Thread mainLoop = null;
	private Bitmap bmp=null;

	private boolean cameraExists = false;
	private boolean shouldStop = false;
	
	// /dev/videoX with Required 666 permission
	private int cameraId=0;
	
	static final int IMG_WIDTH=320;
	static final int IMG_HEIGHT=240;

	// The following variables are used to draw camera images.
    private int winWidth=0;
    private int winHeight=0;
    private Rect rect;
    private int dw, dh;
    private float rate;
  
    // JNI functions
    public native int prepareCamera(int videoid);
    public native void processCamera();
    public native void stopCamera();
    public native void pixeltobmp(Bitmap bitmap);
    public native boolean startControlCamera(int controlId , int controlValue);
    public native int getCurrentControlValue(int controlId);
      
    static {
        System.loadLibrary("UVCCamera");
    }
    
	public UVCCameraPreview(Context context) {
		super(context);
		this.context = context;
		setFocusable(true);
		
		holder = getHolder();
		holder.addCallback(this);
		holder.setType(SurfaceHolder.SURFACE_TYPE_NORMAL);	
		
		if(bmp==null){
			bmp = Bitmap.createBitmap(IMG_WIDTH, IMG_HEIGHT, Bitmap.Config.ARGB_8888);
		}
		
		// /dev/videoX
		int ret = prepareCamera(cameraId);
		
		if(ret != -1){
			cameraExists = true;
		}
		
        mainLoop = new Thread(this);
        mainLoop.start();		
	}

	public UVCCameraPreview(Context context, AttributeSet attrs) {
		super(context, attrs);
		this.context = context;
		setFocusable(true);
		
		holder = getHolder();
		holder.addCallback(this);
		holder.setType(SurfaceHolder.SURFACE_TYPE_NORMAL);	
		
		if(bmp==null){
			bmp = Bitmap.createBitmap(IMG_WIDTH, IMG_HEIGHT, Bitmap.Config.ARGB_8888);
		}
		// /dev/videoX
		int ret = prepareCamera(cameraId);
		
		if(ret!=-1) cameraExists = true;
		
        mainLoop = new Thread(this);
        mainLoop.start();		
	}
	
    @Override
    public void run() {
        while (true && cameraExists) {
			rect = new Rect(0, 0, 640, 480);
        	
        	processCamera();

            //Get Bitmap Image from UVC Camera
        	pixeltobmp(bmp);
        	
            Canvas canvas = getHolder().lockCanvas();
            if (canvas != null)
            {
            	// draw camera bmp on canvas
            	canvas.drawBitmap(bmp,null,rect,null);

            	getHolder().unlockCanvasAndPost(canvas);
            }

            if(shouldStop){
            	shouldStop = false;  
            	break;
            }	        
        }
    }

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
	}
	
	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		if(cameraExists){
			shouldStop = true;
			while(shouldStop){
				try{ 
					Thread.sleep(100); // wait for thread stopping
				}catch(Exception e){}
			}
		}
		stopCamera();
	}   
	
	public void controlUp(){
		if(cameraExists){
			int currentValue = getCurrentControlValue(CONTROL_CAMERA_TILT);
			Log.e(TAG, "当前up值是：" + currentValue);
			if(currentValue >= TILT_UP_MAX){
				Toast.makeText(getContext(), "已经不能再向上转动", Toast.LENGTH_SHORT).show();
			}else{
				boolean isSuccess = startControlCamera(CONTROL_CAMERA_TILT, currentValue + TILT_PER_UP);
				if(isSuccess){
					Toast.makeText(getContext(), "控制成功", Toast.LENGTH_SHORT).show();
				}else{
					Toast.makeText(getContext(), "控制失败", Toast.LENGTH_SHORT).show();
				}
			}
		}else{
			Toast.makeText(getContext(), "摄像头打开失败", Toast.LENGTH_SHORT).show();
		}
	}
	
	public void controlDown(){
		if(cameraExists){
			int currentValue = getCurrentControlValue(CONTROL_CAMERA_TILT);
			Log.e(TAG, "当前down值是：" + currentValue);
			if(currentValue <= TILT_DOWN_MAX){
				Toast.makeText(getContext(), "已经不能再向下转动", Toast.LENGTH_SHORT).show();
			}else{
				boolean isSuccess = startControlCamera(CONTROL_CAMERA_TILT, currentValue + TILT_PER_DOWN);
				if(isSuccess){
					Toast.makeText(getContext(), "控制成功", Toast.LENGTH_SHORT).show();
				}else{
					Toast.makeText(getContext(), "控制失败", Toast.LENGTH_SHORT).show();
				}
			}
		}else{
			Toast.makeText(getContext(), "摄像头打开失败", Toast.LENGTH_SHORT).show();
		}
	}
	
	public void controlLeft(){
		if(cameraExists){
			int currentValue = getCurrentControlValue(CONTROL_CAMERA_PAN);
			Log.e(TAG, "当前left值是：" + currentValue);
			if(currentValue <= PAN_LEFT_MAX){
				Toast.makeText(getContext(), "已经不能再向左转动", Toast.LENGTH_SHORT).show();
			}else{
				boolean isSuccess = startControlCamera(CONTROL_CAMERA_PAN, currentValue + PAN_PER_LEFT);
				if(isSuccess){
					Toast.makeText(getContext(), "控制成功", Toast.LENGTH_SHORT).show();
				}else{
					Toast.makeText(getContext(), "控制失败", Toast.LENGTH_SHORT).show();
				}
			}
		}else{
			Toast.makeText(getContext(), "摄像头打开失败", Toast.LENGTH_SHORT).show();
		}
	}
	
	public void controlRight(){
		if(cameraExists){
			int currentValue = getCurrentControlValue(CONTROL_CAMERA_PAN);
			Log.e(TAG, "当前right值是：" + currentValue);
			if(currentValue >= PAN_RIGHT_MAX){
				Toast.makeText(getContext(), "已经不能再向右转动", Toast.LENGTH_SHORT).show();
			}else{
				boolean isSuccess = startControlCamera(CONTROL_CAMERA_PAN, currentValue + PAN_PER_RIGHT);
				if(isSuccess){
					Toast.makeText(getContext(), "控制成功", Toast.LENGTH_SHORT).show();
				}else{
					Toast.makeText(getContext(), "控制失败", Toast.LENGTH_SHORT).show();
				}
			}
		}else{
			Toast.makeText(getContext(), "摄像头打开失败", Toast.LENGTH_SHORT).show();
		}
	}
	
	public void controlZoomIn(){
		
	}
	
	public void controlZoomOut(){
		
	}
}
