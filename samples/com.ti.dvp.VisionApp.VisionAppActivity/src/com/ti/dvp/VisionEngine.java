/*
 *  Copyright (C) 2012 Texas Instruments, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.ti.dvp;

import java.io.IOException;
import java.util.List;

//import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

public class VisionEngine implements Camera.PreviewCallback {
    static {
        System.loadLibrary("dvp_jni"); // this will cause the JNI layer to be loaded and run-time linked.
    }

    private static String TAG = "VisionEngine";
    private Camera m_camera;
    private Camera.Parameters m_cparams;
    private int    m_numCameras;
    private int    m_camIdx;
    private int    m_graphEnum;

    private static final int GRAPH_MIN = -1;
    public static final int GRAPH_DEFAULT = 0;
    public static final int GRAPH_EDGE = 1;
    private static final int GRAPH_MAX = 2;

    private int m_width;
    private int m_height;
    private int m_format;

    private GraphCompletedThread m_thread;
    private GraphCompletedListener m_listener;

    private SurfaceTexture m_DummyTexture;
    private Surface m_DummySurface;
    private Surface m_DisplaySurface;

    private boolean m_useNativeCamera;

    
    public interface GraphCompletedListener {
        public void onGraphCompleted();
    }

    public VisionEngine(GraphCompletedListener listener, boolean useNativeCamera)
    {
        m_camera = null;
        m_useNativeCamera = useNativeCamera;

        m_listener = listener;
        handle = InitNative(m_useNativeCamera);

        m_DummyTexture = new SurfaceTexture(1234);
        m_DummySurface = new Surface(m_DummyTexture);
        
    }

    public void destroy()
    {
        GraphTeardown();
        FinalizeNative(handle);
        if (m_camera != null) {
            m_camera.release();
        }
        m_DummyTexture.release();
        m_DummySurface.release();
        m_DisplaySurface.release();
    }

    protected void finalize()
    {
        destroy();
    }

    public void onPreviewFrame(byte[] data, Camera camera) {
        Log.d(TAG, "Calling GraphUpdate!");
        GraphUpdate(data, m_width, m_height, m_format);
    }

    public void setPreviewSurface(SurfaceHolder surface) {
        if (m_camera != null) {
            try {
                m_camera.setPreviewDisplay(surface);
            } catch (IOException io) {}
        }
    }

    public void setPreviewSurfaceTexture(SurfaceTexture texture) {
        if (m_camera != null) {
            try {
                m_camera.setPreviewTexture(m_DummyTexture);
            } catch (IOException io) {}
        }

        m_DisplaySurface = new Surface(texture);
        SetPreviewSurfaceNative(m_DisplaySurface, m_width, m_height, 0x100); //NV12 format

        if (m_useNativeCamera)
        {
            SelectCameraNative(0);
            DumpCameraParametersNative();

            m_DummyTexture.setDefaultBufferSize(m_width, m_height);
            StartPreviewNative(m_DummyTexture);
        }
    }

    public boolean CameraInit()
    {
        Log.d(TAG, "CameraInit()");
        try {
            CameraInfo info = new Camera.CameraInfo();

            m_camera = null;
            for (m_camIdx = 0; m_camIdx < Camera.getNumberOfCameras(); m_camIdx++)
            {
                Camera.getCameraInfo(m_camIdx, info);
                Log.d(TAG, "Camera: "+m_camIdx+" is a "+info.facing+" facing camera");
                if (CameraInfo.CAMERA_FACING_FRONT == info.facing)
                {
                    m_camera = Camera.open(m_camIdx);
                    break;
                }
            }
        } catch (RuntimeException re) {
            Log.e(TAG, "Failed to connect to camera!");
            m_camera = null;
        }
        if (m_camera == null)
        {
            Log.e(TAG, "Could not find a front camera to open!");
            return false;
        }
        Log.d(TAG, "Opened Camera!");

        // get the buffers from the lower layers
        m_camera.setPreviewCallback(this); // @TODO add with Buffers later
//      m_camera.setPreviewCallbackWithBuffer(this);
//      m_camera.addCallbackBuffer(GetBuffersNative(0));

        m_cparams = m_camera.getParameters();
        if (m_cparams != null)
        {
//          m_cparams.setExposureCompensation(m_cparams.getMinExposureCompensation());
            m_cparams.setFocusMode("auto");
            m_cparams.setPreviewSize(m_width, m_height);
            m_cparams.setPreviewFormat(m_format);
            List<int[]> range = m_cparams.getSupportedPreviewFpsRange();
            for (int i = 0; i < range.size(); i++)
            {
                int[] subrange = range.get(i);
                for (int j = 0; j < subrange.length; j++)
                    Log.d(TAG, "FPS["+i+"]["+j+"]: "+subrange[j]+"");
            }
            m_cparams.setPreviewFpsRange(24*1000, 24*1000);
            m_camera.setParameters(m_cparams);
            return true;
        }
        else
            return false;
    }

    public boolean GraphSetup(int gEnum, int width, int height, int format)
    {
        if (GRAPH_MIN < gEnum && gEnum < GRAPH_MAX)
        {
            boolean ret = false;
            m_width = width;
            m_height = height;
            m_format = format;
            m_graphEnum = gEnum;

            ret = GraphSetupNative(handle, m_graphEnum, m_width, m_height, m_format);
            if (ret == true && !m_useNativeCamera)
                ret = CameraInit();
            return ret;
        }
        else
            return false;
    }

    public boolean GraphUpdate(byte[] buffer, int width, int height, int format)
    {
        Log.d(TAG, "Calling GraphUpdateNative for "+width+","+height+","+format);
        return GraphUpdateNative(handle, graphEnum, buffer, width, height, format);
    }

    public boolean GraphTeardown()
    {
        return GraphTeardownNative(handle, m_graphEnum);
    }

    public void WaitForComplete()
    {
        WaitForCompleteNative();
    }

    public void CancelWaitForComplete()
    {
        CancelWaitForCompleteNative();
    }

    public void Start() {
        if (m_camera != null) {
            m_camera.startPreview();
        }
//          if (m_useNativeCamera)
//          {
//          m_DummyTexture.setDefaultBufferSize(m_width, m_height);
//          StartPreviewNative(m_DummyTexture);
//          }
        StartNotification();
    }

    public void Stop() {
        StopNotification();
        if (m_useNativeCamera) {
            StopPreviewNative();
        }
        if (m_camera != null) {
            m_camera.stopPreview();
        }
    }

    private void StartNotification() {
        m_thread = new GraphCompletedThread();
        m_thread.setRunning(true);
        m_thread.start();
    }

    private void StopNotification() {
        m_thread.setRunning(false);
        CancelWaitForComplete();
        try {
            m_thread.join();
        } catch (InterruptedException e) {
            // we will try it again and again...
        }
    }

    private class GraphCompletedThread extends Thread {
        private boolean m_running = false;

        public GraphCompletedThread() {
        }

        public void setRunning(boolean running) {
            m_running = running;
        }

        @Override
        public void run() {
            while (m_running) {
                WaitForComplete();
                if (m_running) {
//                  m_camera.addCallbackBuffer(GetBuffersNative(0));
                    m_listener.onGraphCompleted();
                }
            }
        }
    }

    // PRIVATE NATIVE INTERFACES

    private int handle;
    private int graphEnum;
    private native int InitNative(boolean initCamera);
    private native void FinalizeNative(int handle);
    private native boolean GraphSetupNative(int handle, int graphEnum, int width, int height, int format);
    private native boolean GraphUpdateNative(int handle, int graphEnum, byte[] buffer, int width, int height, int bpp);
    private native boolean GraphTeardownNative(int handle, int graphEnum);
    private native void WaitForCompleteNative();
    private native void CancelWaitForCompleteNative();
    private native void DumpCameraParametersNative();
    private native boolean SelectCameraNative(int id);
    private native boolean SetPreviewSurfaceNative(Surface surface, int width, int height, int format);
    private native boolean StartPreviewNative(SurfaceTexture surface);
    private native boolean StopPreviewNative();
    private native byte[] GetBuffersNative(int buffID);
}

