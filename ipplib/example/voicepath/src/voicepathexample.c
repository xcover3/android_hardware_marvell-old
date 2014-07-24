#include "misc.h"
#include "ippVP.h"
#include "stdio.h"
#include "ippLV.h"

#define LINUX_VP
#ifdef LINUX_VP
#include <sys/time.h>
#endif

#define	VOICEPATH_ALIGH_BUF(buf, align)	(((unsigned int)(buf)+(align)-1)&~((unsigned int)(align)-1))

#define CMD_FIRE_AT_FRAMEIDX	-1
#define CMD_FIRE_AT_FRAMEIDX2	-1
#define CMD_FIRE_AT_FRAMEIDX3	-1

/******************************************************************************
// Name:			 DisplayLibVersion
//
// Description:		 Display library build information on the text console
//
// Input Arguments:  None.
//
// Output Arguments: None				
//
// Returns:			 None				
*******************************************************************************/
void DisplayLibVersion()
{
    char libversion[128]={'\0'};
    IppCodecStatus ret;
    ret = GetLibVersion_VoicePath(libversion,sizeof(libversion));
    if(0 == ret){
        IPP_Printf("\n*****************************************************************\n");
        IPP_Printf("This library is built from %s\n",libversion);
        IPP_Printf("*****************************************************************\n");
    }else{
        IPP_Printf("\n*****************************************************************\n");
        IPP_Printf("Can't find this library version information\n");
        IPP_Printf("*****************************************************************\n");
    }
}

void run_ae(char* inFile, char* outFile, int channel, int sr)
{
    FILE* fp_in;
    FILE* fp_out;
    int period = 40; //ms
    int count;
    void* pState;
    ae_cfg_t cfg;
    MiscGeneralCallbackTable* pCallback;
    int frameLen = sr*period/1000;
    IppStatus ret;
    short* pSrc;
    int times = 0;
    int i=0;
#ifdef LINUX_VP
    struct timeval start;
    struct timeval end;
    struct timezone tz1;
    struct timezone tz2;
    int at=0;
#endif
    if( 0 != miscInitGeneralCallbackTable(&pCallback) ) {
        return -1;
    }
    cfg.nSampleRate = sr;
    cfg.nChannelCount = channel;
    cfg.nFlag = (EQ_ENABLE | IIR_ENABLE | DRC_ENABLE | NS_ENABLE);
    cfg.nFlag = NS_ENABLE;
    ret = AEInit(&cfg, &pState, pCallback);
    if(ret != IPP_STATUS_NOERR){
        printf("init error:%d", ret);
        miscFreeGeneralCallbackTable(&pCallback);
        return ;
    }
#ifdef LINUX_VP
    pSrc = (short*)malloc(sizeof(short)*channel*sr*30);
    while(times<20){
        i=0;
        times++;
        fp_in = fopen(inFile,"rb");
        fp_out = fopen(outFile, "wb");
        if(!fp_in || !fp_out){
            return; 
        }
        fread(pSrc,sizeof(short),sr*channel*30,fp_in);             
        gettimeofday(&start, NULL);       
        while(i<750){
            AEProcess(pState, pSrc+i*frameLen, frameLen);
            i++;
        }
        gettimeofday(&end,NULL); 
        int delta = (long long int)(end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
        printf("delta=%d\n",delta);
        at+=delta;
        fwrite(pSrc,sizeof(short),sr*channel*30,fp_out);
        fclose(fp_in);
        fclose(fp_out);
    }
    at=at/20;
    printf("at=%d\n",at);
#else
    pSrc = (short*)malloc(sizeof(short)*channel*frameLen);
    fp_in = fopen(inFile,"rb");
    fp_out = fopen(outFile, "wb");
    if(!fp_in || !fp_out){
        return; 
    }
    while(1){
        times++;
        if(times == 140){
            times  = times;
        }
        count = fread(pSrc, 2, frameLen*channel,fp_in);
        if(count <  frameLen*channel)
            break;
        AEProcess(pState, pSrc, frameLen);
        fwrite(pSrc,2, frameLen*channel,fp_out);
    }
#endif
    AEFree(&pState);
    free(pSrc);
    if(fp_in) fclose(fp_in);
    if(fp_out) fclose(fp_out);
}

#ifdef WINCE
int wmain()
#else
int main(int argc, char **argv)
#endif
{

    char input_filename[128];
    char output_filename[128];
    int channels;
    int samplerate;

    DisplayLibVersion();
    if(argc < 4) {
        fprintf(stderr, "Arguments:\n \
                         input file name\n \
                         output file name\n \
                         channels(1,2)\n \
                         samplerate(8000,16000,44100,48000)\n"); 
        exit(1);
    }
    argv++;
    memset(input_filename, 0x0, sizeof(input_filename));
    memset(output_filename, 0x0, sizeof(output_filename));
    strcpy(input_filename, *argv++);   
    strcpy(output_filename, *argv++);
    channels = atoi(*argv++);
    samplerate = atoi(*argv++);
    run_ae(input_filename, output_filename, channels, samplerate);
    return 0;
}




