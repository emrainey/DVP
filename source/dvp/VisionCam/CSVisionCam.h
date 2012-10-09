
#ifndef _CS_VISION_CAM_H_
#define _CS_VISION_CAM_H_

#include <dvp/VisionCam.h>

#if defined(ANDROID) && (defined(ICS) || defined(JELLYBEAN))

#include <binder/IMemory.h>
#include <camera/ICamera.h>
#include <gui/SurfaceTexture.h>

using namespace android;

const uint32_t MAX_FRAMES = 8;

class CSVisionCam : public VisionCam,
                    public CameraListener,
                    public virtual RefBase
{
public:
    CSVisionCam();
    virtual ~CSVisionCam();

    // interface methods
    status_e init(void * cookie);
    status_e deinit();
    status_e useBuffers(DVP_Image_t *prvBufArr, uint32_t numPrvBuf, VisionCamPort_e port=VCAM_PORT_PREVIEW);
    status_e releaseBuffers(VisionCamPort_e port=VCAM_PORT_PREVIEW);
    status_e flushBuffers(VisionCamPort_e port=VCAM_PORT_PREVIEW);
    status_e sendCommand(VisionCamCmd_e cmdId, void *param, uint32_t size, VisionCamPort_e port=VCAM_PORT_PREVIEW);
    status_e setParameter(VisionCamParam_e paramId, void* param, uint32_t size, VisionCamPort_e port=VCAM_PORT_PREVIEW);
    status_e getParameter(VisionCamParam_e paramId, void* param, uint32_t size, VisionCamPort_e port=VCAM_PORT_PREVIEW);
    status_e returnFrame(VisionCamFrame *cameraFrame);

    // CameraListener
    virtual void notify(int32_t msgType, int32_t ext1, int32_t ext2);
    virtual void postData(int32_t msgType, const sp<IMemory> &dataPtr,
                          camera_frame_metadata_t *metadata);
    virtual void postDataTimestamp(
            nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr);

protected:

private:
    status_e connect(int32_t cameraId);

    status_e startPreview();
    status_e stopPreview();
    status_e toggleFaceDetection();
    status_e lockAutoWhitebalance(bool_e lock);
    status_e lockAutoExposure(bool_e lock);

    status_e setFlicker(VisionCamFlickerType type);
    status_e setExposureCompensation(int32_t compensation);

    status_e setCameraParameters();
    status_e initCameraParameters();

    status_e initFrameDescriptors();
    status_e deinitFrameDescriptors();

    const char* colorSpaceToPreviewFormat(_fourcc colorSpace);
    _fourcc previewFormatToColorSpace(const char* previewFormat);

    const char* flickerToAntibanding(VisionCamFlickerType type);
    VisionCamFlickerType antibandingToFlicker(const char* antibanding);

    const char* flickerToString(VisionCamFlickerType flicker);

    // Callback called when still camera raw data is available.
    void dataCallback(int32_t msgType, const sp<IMemory> &data, camera_frame_metadata_t *metadata);
    void dataCallbackTimestamp(int64_t timestampUs, int32_t msgType, const sp<IMemory> &data);

private:
    VisionCamSensorSelection        mCameraId;
    sp<Camera>                      mCamera;
    sp<SurfaceTexture>              mSurfaceTexture;

    uint32_t mFrameIndex;
    VisionCamFrame * mFrameDescriptors[MAX_FRAMES];

    bool_e   mInited;

    bool_e   mConnected;
    bool_e   mFaceDetectionEnabled;

    uint32_t mWidth;
    uint32_t mHeight;

    int32_t  mContrast;
    int32_t  mSharpness;
    int32_t  mBrightness;
    int32_t  mSaturation;
    int32_t  mEVCompensation;

    VisionCamFlickerType mFlicker;

    _fourcc mColorSpace;
    VisionCamVarFramerateType mVfr;
};

#endif // ANDROID

#endif // _CS_VISION_CAM_H_
