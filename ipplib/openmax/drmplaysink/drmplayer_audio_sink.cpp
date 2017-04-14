#include <stdio.h>
#include <string.h>

#include <media/AudioTrack.h>

using namespace android;

typedef int (*UsrAudioCallback)(void *pPlatform, void *raw, size_t size);

typedef struct  DrmPlayerAudioSink{

    AudioTrack *hAudioTrack;
    int64_t mLatencyUs; 
    UsrAudioCallback UsrDefinedAudioSinkCallback;    
    void *pPlatformPrivate;
}DrmPlayerAudioSink;

static void AudioCallback(int event,void *usr,  void *info){
    DrmPlayerAudioSink *DrmPlayerSinkPrivate= (DrmPlayerAudioSink *)usr;

    if (event != AudioTrack::EVENT_MORE_DATA) {
        return;
    }

    AudioTrack::Buffer *buffer = (AudioTrack::Buffer *)info;
    size_t numBytesWritten = DrmPlayerSinkPrivate->UsrDefinedAudioSinkCallback(DrmPlayerSinkPrivate->pPlatformPrivate, buffer->raw, buffer->size);

    buffer->size = numBytesWritten;
    

}
extern "C" void *drmplayer_audiosink_init(unsigned int mSampleRate, unsigned int numChannels, int (*UsrAudioCallback)(void *pPlatform,  void *raw, unsigned int size), void *pPlatform){

    DrmPlayerAudioSink *handle=NULL;
    handle = (DrmPlayerAudioSink*)malloc(sizeof(DrmPlayerAudioSink));

    if (UsrAudioCallback){
        handle->hAudioTrack = new AudioTrack(
                AudioSystem::MUSIC, mSampleRate, AudioSystem::PCM_16_BIT,
                (numChannels == 2)
                    ? AudioSystem::CHANNEL_OUT_STEREO
                    : AudioSystem::CHANNEL_OUT_MONO,
                0, 0, &AudioCallback, handle, 0);
    }else{
        handle->hAudioTrack = new AudioTrack(
                AudioSystem::MUSIC, mSampleRate, AudioSystem::PCM_16_BIT,
                (numChannels == 2)
                    ? AudioSystem::CHANNEL_OUT_STEREO
                    : AudioSystem::CHANNEL_OUT_MONO,
                0, 0, NULL, handle, 0);
        
    }

    if(OK != handle->hAudioTrack->initCheck()){
        handle->hAudioTrack->stop();
        delete handle->hAudioTrack;
        free(handle);
        return NULL;
    }
    handle->UsrDefinedAudioSinkCallback = UsrAudioCallback;
    handle->pPlatformPrivate            = pPlatform;
    handle->mLatencyUs = (int64_t)handle->hAudioTrack->latency() * 1000;
    handle->hAudioTrack->start();
    return handle;
}
extern "C" unsigned int drmplayer_audiosink_write(void *Handle, void *src, unsigned int size){
    DrmPlayerAudioSink *hDrmPlayerAudioSink = (DrmPlayerAudioSink *)Handle;

    return hDrmPlayerAudioSink->hAudioTrack->write(src, size);
    
}
extern "C" int64_t drmplayer_audiosink_getLatencyUs(void *Handle){
    DrmPlayerAudioSink *hDrmPlayerAudioSink = (DrmPlayerAudioSink *)Handle;
    if (hDrmPlayerAudioSink){
        return hDrmPlayerAudioSink->mLatencyUs;
    }
    return 0;
}
extern "C" void drmplayer_audiosink_deinit(void *Handle){
    DrmPlayerAudioSink *hDrmPlayerAudioSink = (DrmPlayerAudioSink *)Handle;

   delete hDrmPlayerAudioSink->hAudioTrack;
   free(Handle);

}

