package org.siprop.android.uvccamera;

import org.siprop.android.uvccamera.ShellUtils.CommandResult;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity implements OnClickListener{

	private UVCCameraPreview uvcCameraView;
	private Button up , down , left , right , zoomIn , zoomOut;
	private TextView resultView;
	
	private static final String TAG="UVCCamera";
	
	//正数向左，负数向右,范围是-583200 , 583200
	private final int CONTROL_CAMERA_PAN = 10094856;
	//正数向上，负数向下,范围是-86400 , 324000
	private final int CONTROL_CAMERA_TILT = 10094857;
	//变焦范围是0 , 16384
	private final int CONTROL_CAMERA_ZOOM = 10094861;
	
	//左右转动的角度是170，每移动一次的值是34300，为10度
	private final int PAN_PER_LEFT = 10;
	private final int PAN_PER_RIGHT = -10;
	//上下转动的角度是-30 ~ +90
	//向下每次转动的角度是10度
	private final int TILT_PER_DOWN = -5;
	//每次向上转动的角度是10度
	private final int TILT_PER_UP = 5;
	//每一次变焦是32
	private final int ZOOM_IN = -64;
	private final int ZOOM_OUT = 64;
	//左边移动最大值
	private final int PAN_LEFT_MAX = -583200;
	//右边移动最大值
	private final int PAN_RIGHT_MAX = 583200;
	//上边移动最大值
	private final int TILT_UP_MAX = 324000;
	//下边移动最大值
	private final int TILT_DOWN_MAX = -86400;
	//变焦最小值
	private final int ZOOM_MIN = 0;
	//变焦最大值
	private final int ZOOM_MAX = 16384;
	//一下三个Array用来存储每次控制之后的数值，大小为2第一个是控制之前的数值，第二个是控制后的数值
	private int[] upDownArray;
	private int[] leftRightArray;
	private int[] zoomInOutArray;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		
		RelativeLayout view = (RelativeLayout)findViewById(R.id.video_view);
		UVCCameraPreview childView = new UVCCameraPreview(this);
		view.addView(childView);
		
		upDownArray = new int[]{0,0};
		leftRightArray = new int[]{0,0};
		zoomInOutArray = new int[]{0,0};
		
		up = (Button)findViewById(R.id.up);
		down = (Button)findViewById(R.id.down);
		left = (Button)findViewById(R.id.left);
		right = (Button)findViewById(R.id.right);
		zoomIn = (Button)findViewById(R.id.zoom_in);
		zoomOut = (Button)findViewById(R.id.zoom_out);
		
		resultView = (TextView)findViewById(R.id.result);
		
		if(!initDevice()){
			Toast.makeText(this, "设备打开失败", Toast.LENGTH_SHORT).show();
		}else{
			up.setOnClickListener(this);
			down.setOnClickListener(this);
			left.setOnClickListener(this);
			right.setOnClickListener(this);
			zoomIn.setOnClickListener(this);
			zoomOut.setOnClickListener(this);
		}
	}

	@Override
	public void onClick(View v) {
		switch (v.getId()) {
		case R.id.up:
			controlUp();
			break;
		case R.id.down:
			controlDown();
			break;
		case R.id.left:
			controlLeft();
			break;
		case R.id.right:
			controlRight();
			break;
		case R.id.zoom_in:
			controlZoomIn();
			break;
		case R.id.zoom_out:
			controlZoomOut();
			break;
		default:
			break;
		}		
	}   
	
	public void controlUp(){
		upDownArray = startControlCamera(CONTROL_CAMERA_TILT, TILT_PER_UP);
		if(canContinueControl(upDownArray , 2)){
			setControlValue(upDownArray);
		}		
	}
	
	public void controlDown(){
		upDownArray = startControlCamera(CONTROL_CAMERA_TILT, TILT_PER_DOWN);			
		if(canContinueControl(upDownArray , 2)){
			setControlValue(upDownArray);
		}	
	}
	
	public void controlLeft(){
		leftRightArray = startControlCamera(CONTROL_CAMERA_PAN, PAN_PER_LEFT);
		if(canContinueControl(leftRightArray , 1)){
			setControlValue(leftRightArray);
		}	
	}
	
	public void controlRight(){
		leftRightArray = startControlCamera(CONTROL_CAMERA_PAN, PAN_PER_RIGHT);
		if(canContinueControl(leftRightArray , 1)){
			setControlValue(leftRightArray);
		}	
	}
	
	public void controlZoomIn(){
		if(zoomInOutArray[1] == 0){
			Toast.makeText(this, "已经是最小了", Toast.LENGTH_SHORT).show();
			return;
		}
		zoomInOutArray = startControlCamera(CONTROL_CAMERA_ZOOM, ZOOM_IN);
		if(canContinueControl(zoomInOutArray , 3)){
			setControlValue(zoomInOutArray);
		}	
	}
	
	public void controlZoomOut(){
		zoomInOutArray = startControlCamera(CONTROL_CAMERA_ZOOM, ZOOM_OUT);
		if(canContinueControl(zoomInOutArray , 3)){
			setControlValue(zoomInOutArray);
		}
	}
	
	private boolean isArrayNull(int[] array){
		if(array != null && array.length > 0){
			return false;
		}
		return true;
	}
	
	private void setControlValue(int[] array){
		if(!isArrayNull(array)){
			resultView.setText("控制成功,控制之前的值是：" + array[0] + " 控制之后的值是：" + array[1]);
		}else{
			resultView.setText("控制失败");
		}
	}
	
	private int getCurrentPosition(int[] array){
		if(!isArrayNull(array)){
			return array[1];
		}
		return 0;
	}
	
	private boolean canContinueControl(int array[] , int flag){
		if(!isArrayNull(array)){
			if(flag == 1){
				if(array[1] >= PAN_RIGHT_MAX || array[1] <= PAN_LEFT_MAX){
					Toast.makeText(this, "超出控制范围了", Toast.LENGTH_SHORT).show();
					return false;
				}
			}else if(flag == 2){
				if(array[1] >= TILT_UP_MAX || array[1] <= TILT_DOWN_MAX){
					Toast.makeText(this, "超出控制范围了", Toast.LENGTH_SHORT).show();
					return false;
				}
			}else{
				if(array[1] >= ZOOM_MAX || array[1] <= ZOOM_MIN){
					Toast.makeText(this, "超出控制范围了", Toast.LENGTH_SHORT).show();
					return false;
				}
			}
		}
		return true;
	}
	
    static {
        System.loadLibrary("UVCCamera");
    }
    
    public native int[] startControlCamera(int controlId , int controlValue);
    public native int getCurrentControlValue(int controlId);
    public native boolean initDevice();
    public native void queryControl();
    public native boolean isSupportPtz();
}
