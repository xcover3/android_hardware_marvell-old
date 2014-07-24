#ifndef ANDROID_HARDWARE_CAMERA_LOG_MRVL_H
#define ANDROID_HARDWARE_CAMERA_LOG_MRVL_H

#include <utils/Log.h>
#include <utils/Timers.h>

#undef __MASS_PRODUCT__
#define __MASS_PRODUCT__

#ifndef __MASS_PRODUCT__

/*{{{*/
#define FUNC_TAG    do{ALOGD("-%s", __FUNCTION__);}while(0)
#define FUNC_TAG_E  do{ALOGD("-%s e", __FUNCTION__);}while(0)
#define FUNC_TAG_X  do{ALOGD("-%s x", __FUNCTION__);}while(0)

#define TRACE_E(...)\
    do \
{\
    ALOGE("MrvlCameraHal_ERROR: %s:%s:%d",__FUNCTION__,__FILE__,__LINE__); \
    ALOGE(__VA_ARGS__); \
} while(0);

#define TRACE_D(...)\
    do \
{\
    ALOGD(__VA_ARGS__); \
} while(0);

#define TRACE_V(...)\
    do \
{\
    ALOGD(__VA_ARGS__); \
} while(0);
//--------------------------------------------------------------------
//show preview/camcorder fps, undef for mass product
#define __SHOW_FPS__
//make fake camera visable, undef for mass product
#define __FAKE_CAM_AVAILABLE__
//enable the dump capability, undef for mass product
//__DUMP_IMAGE__ macro depends on __SHOW_FPS__
#define __DUMP_IMAGE__
#ifdef __DUMP_IMAGE__
#define __SHOW_FPS__
#endif

#define __DEBUG_TIME1__
#define __DEBUG_TIME2__
/*}}}*/

#else //__MASS_PRODUCT__

/*{{{*/

#define FUNC_TAG    do{}while(0)
#define FUNC_TAG_E  do{}while(0)
#define FUNC_TAG_X  do{}while(0)

#define TRACE_E(...)\
    do \
{\
    ALOGE("ERROR: %s:%s:%d",__FUNCTION__,__FILE__,__LINE__); \
    ALOGE(__VA_ARGS__); \
} while(0);

#define TRACE_D(...)\
    do \
{\
} while(0);
// {\
    // ALOGD(__VA_ARGS__); \
// } while(0);

#define TRACE_V(...)\
    do \
{\
} while(0);

// {\
    // ALOGV(__VA_ARGS__); \
// } while(0);
//--------------------------------------------------------------------
#undef __SHOW_FPS__
#undef __FAKE_CAM_AVAILABLE__
#undef __DUMP_IMAGE__

#undef __DEBUG_TIME1__
#undef __DEBUG_TIME2__
#undef __DEBUG_MSG__
/*}}}*/

#endif //__MASS_PRODUCT__

#define __EXIF_GENERATOR__
//#define __PREVIE_SKIP_FRAME_CNT__ (0)
#undef __PREVIE_SKIP_FRAME_CNT__

//whether enable the dedicated msg thread
//#define __DEBUG_WITHOUT_MSG_THREAD__
#undef __DEBUG_WITHOUT_MSG_THREAD__

//--------------------------------------------------------------------

//debug time profile, undef for mass product
/*{{{*/
#ifdef __DEBUG_TIME1__
static nsecs_t told,delta;
static int lineold;
#define TIMETAG do{\
    TRACE_D("************************"); \
    delta=systemTime()-told; \
    TRACE_D("*line:%d - %d,\t dt=%f ms",lineold, __LINE__, double(delta/1000000)); \
    lineold=int(__LINE__); \
    told=systemTime(); \
    TRACE_D("************************");\
    }while(0)
#else
#define TIMETAG do{}while(0)
#endif

/*}}}*/

//debug time profile, undef for mass product
/*{{{*/
namespace android {
    class CamTimer:public DurationTimer{
        private:
            struct timeval mNowtime;
            nsecs_t mSystemTime;
        public:
#ifdef __DEBUG_TIME2__
            long long showDur(const char* str=""){
                long long dur_ms = durationUsecs()/1000L;
                ALOGD("CAMERA_TIME_PROFILE:%s:duration=%lldms",
                        str,dur_ms);
                return dur_ms;
            }
            long long showFPS(const char* str="",const long cnt = 0){
                long long dur_ms = durationUsecs()/1000L;
                long long fps = cnt * 1000.0f / dur_ms;
                ALOGD("CAMERA_TIME_PROFILE:%s:fps=%lld",
                        str,fps);
                return fps;
            }
            /*
            void showtime(const char* str=""){
                gettimeofday(&mNowtime, NULL);

                long long nowUSec = ((long long) mNowtime.tv_sec) * 1000000LL +
                    ((long long) mNowtime.tv_usec);

                ALOGD("CAMERA_TIME_PROFILE:%s:time=%.3fms",
                        str,nowUSec/1000.0);
            }
            */

            void showNow(const char* str=""){
                mSystemTime = systemTime();
                ALOGD("CAMERAHAL_TIME_PROFILE:%s:time=%.3fms",
                        str,double(mSystemTime/1000000.0));
            }
#else
            void start(void){
                return;
            }
            void stop(void){
                return;
            }
            void showNow(const char* str=""){
                return;
            }
            long long showDur(const char* str=""){
                return 0;
            }
            long long showFPS(const char* str="",const long cnt = 0){
                return 0;
            }
#endif
    };
    static CamTimer camtimer;
}
/*}}}*/



//--------------------------------------------------------------------
//

#endif
