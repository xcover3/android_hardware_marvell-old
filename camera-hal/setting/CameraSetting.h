#ifndef ANDROID_HARDWARE_CAMERA_SETTING_H
#define ANDROID_HARDWARE_CAMERA_SETTING_H

#include <utils/threads.h>
#include <cutils/properties.h>
#include <hardware/camera.h>
#include <utils/Vector.h>

#include <camera/CameraParameters.h>
#include "CameraEngine.h"
#include "ConfigParser.h"

namespace android {

    class MrvlCameraInfo{
        private:
            struct camera_info mCameraInfo;
            char sSensorName[32];
            //MUTABLE, used by cameraengine, chich could be changed
            //according to quired results from camengine
            //NOTES:initialized to -1, for sensors that not detected.
            int iSensorID;

        public:
            MrvlCameraInfo(const char* sensorname="",
                    int facing=-1,
                    int orient=-1
                    ):
                iSensorID(-1)
        {
            strncpy(sSensorName,sensorname,sizeof(sSensorName)-1);
            sSensorName[sizeof(sSensorName)-1]='\0';
            mCameraInfo.facing = facing;
            mCameraInfo.orientation = orient;
        }
            friend class CameraSetting;
        private:
            void setSensorName(const char* name)
            {
                strncpy(sSensorName,name,sizeof(sSensorName)-1);
                sSensorName[sizeof(sSensorName)-1]='\0';
            }
            void setSensorID(int sensorid)
            {
                iSensorID = sensorid;
            }
            int getSensorID() const{return iSensorID;}
            void setFacing(int faceTo)
            {
                mCameraInfo.facing = faceTo;
            }
            int getFacing() const{return mCameraInfo.facing;}
            void setOrient(int orientTo)
            {
                mCameraInfo.orientation = orientTo;
            }
            int getOrient() const{return mCameraInfo.orientation;}

            const char* getSensorName() const{
                return sSensorName;
            }
            const camera_info* getCameraInfo() const;
    };

    class CameraArea:public RefBase
    {
        public:
            CameraArea(
                    int top,
                    int left,
                    int bottom,
                    int right,
                    int weight):mTop(top),
            mLeft(left),
            mBottom(bottom),
            mRight(right),
            mWeight(weight) {}

            status_t transform(
                    int width,
                    int height,
                    int &top,
                    int &left,
                    int &areaWidth,
                    int &areaHeight);
            status_t transform(CAM_MultiROI &multiROI);

            bool isValid(){
                return ( ( 0 != mTop ) || ( 0 != mLeft ) || ( 0 != mBottom ) || ( 0 != mRight) );
            }

            bool isZeroArea(){
                return  ( (0 == mTop ) && ( 0 == mLeft ) && ( 0 == mBottom )
                        && ( 0 == mRight ) && ( 0 == mWeight ));
            }

            int getWeight(){
                return mWeight;
            }

            bool compare(const sp<CameraArea> &area);

            static status_t parseAreas(const char *area, int areaLength,
                    Vector< sp<CameraArea> > &areas);

            static status_t checkArea(
                    int top,
                    int left,
                    int bottom,
                    int right,
                    int weight);

            static bool areAreasDifferent(Vector< sp<CameraArea> > &, Vector< sp<CameraArea> > &);

            static const int TOP            = -1000;
            static const int LEFT           = -1000;
            static const int BOTTOM         = 1000;
            static const int RIGHT          = 1000;
            static const int WEIGHT_MIN     = 1;
            static const int WEIGHT_MAX     = 1000;
            static const int INVALID_AREA   = -2000;
        private:
            int mTop;
            int mLeft;
            int mBottom;
            int mRight;
            int mWeight;
    };

    //inherit from android camera parameter, utilize memfunc in class CameraParameters
    class CameraSetting: public CameraParameters{
        public:
            /*
             * NOTES:we put constructor implementation here to isolate the platform
             * dependant parameters.
             */
            virtual ~CameraSetting(){}
            CameraSetting(){};

            CameraSetting(const CameraParameters& param):
                CameraParameters(param),
                iPreviewBufCnt(-1),
                iStillBufCnt(-1),
                iVideoBufCnt(-1),
                mProductName(""){
                }

        public:
            CameraSetting& operator=(const CameraParameters& param){
                CameraParameters::operator=(param);
                mProductName = "";
                return *this;
            }

            CameraSetting(const char* sensorname);
            CameraSetting(int sensorid);

            static status_t initCameraTable(const char* sensorname, int sensorid, int facing, int orient);
            static int getNumOfCameras();
            static int getBufferStrideReq(int& strideX, int& strideY);

            // set/get standard parameters
            // interface exported for cameraservice
            status_t		setBasicCaps(const CAM_CameraCapability& camera_caps);
            status_t		setSupportedSceneModeCap(const CAM_ShotModeCapability& camera_shotcaps);
            CAM_ShotModeCapability getSupportedSceneModeCap(void) const;

            status_t		setParameters(const CameraParameters& param);
            CameraParameters	getParameters() const;

            struct map_ce_andr{
                String8 andr_str;
                int ce_idx;
            };

            static const map_ce_andr map_whitebalance[];
            static const map_ce_andr map_exposure[];
            static const map_ce_andr map_coloreffect[];
            static const map_ce_andr map_bandfilter[];
            static const map_ce_andr map_flash[];
            static const map_ce_andr map_focus[];
            static const map_ce_andr map_scenemode[];
            static const map_ce_andr map_imgfmt[];
            static const map_ce_andr map_exifrotation[];
            static const map_ce_andr map_exifwhitebalance[];
            static const int AndrZoomRatio;

            static const char MRVL_KEY_ZSL_SUPPORTED[];
            static const char MRVL_PREVIOUS_KEY_WHITE_BALANCE[]; //for AWB Lock;save the previous key white balance
            static const char MRVL_KEY_TOUCHFOCUS_SUPPORTED[];

            static String8 map_ce2andr(/*int map_len,*/const map_ce_andr* map_tab, int ce_idx);
            static int map_andr2ce(/*int map_len,*/const map_ce_andr* map_tab, const char* andr_str);
            status_t getBufCnt(int* previewbufcnt,int* stillbufcnt,int* videobufcnt) const;

            const char* getProductName() const;

            int		getSensorID() const;
            const char* getSensorName() const;

            const camera_info* getCameraInfo() const;

        private:
            static int iNumOfSensors;
            static MrvlCameraInfo mMrvlCameraInfo[];
            static CameraProfiles *mCamProfiles;
            const char* mProductName;
            int         iPreviewBufCnt;
            int         iStillBufCnt;
            int         iVideoBufCnt;
            Vector<const char*>     mPlatformSupportedFormats;//list all supported formats by dislay/codec.

            /*
             * mrvl platform parameters
             * NOTES:there is internal relationship between sensorid & CameraInfo.
             */
            const MrvlCameraInfo*  pMrvlCameraInfo;//reference ptr to MrvlCameraInfo[] table
            CAM_FlipRotate  mPreviewFlipRot;

            status_t		initDefaultParameters();

            CAM_CameraCapability    mCamera_caps;
            CAM_ShotModeCapability  mCamera_shotcaps;
    };

}; // namespace android

#endif
