

#include <fcntl.h>
#include <errno.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/types.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/android_pmem.h>
#include <cutils/ashmem.h>
#include <cutils/log.h>

#include "HWCFenceManager.h"

using namespace android;

void *sync_thread(void *data)
{
    int32_t fd = *((int32_t*)data);
    sp<Fence> pFence = new Fence(fd);

    if(NO_ERROR != pFence->wait(5000)){
        ALOGE("ERROR! TIMEOUT when get fence back.");
        return NULL;
    }

    ALOGD("Get Fence back!");
    return NULL;
}

int main(int argc, char** argv)
{
    HWCFenceManager manager[2];

    int32_t fd1 = manager[0].createFence(1);
    int32_t fd2 = manager[1].createFence(2);

    pthread_t thread;
    pthread_create(&thread, NULL, sync_thread, &fd1);
    pthread_create(&thread, NULL, sync_thread, &fd2);


    {
        int err;
        char str[256];
        printf("press enter to inc to %d\n", 1);
        fgets(str, sizeof(str), stdin);
        manager[0].signalFence(1);
        printf("press enter to inc to %d\n", 1);
        fgets(str, sizeof(str), stdin);
        manager[0].signalFence(2);
        printf("press enter to inc to %d\n", 1);
        fgets(str, sizeof(str), stdin);
        manager[0].signalFence(3);

        printf("---------------------------------------------------------------\n");
        printf("press enter to inc to %d\n", 1);
        fgets(str, sizeof(str), stdin);
        manager[1].signalFence(1);
        printf("press enter to inc to %d\n", 1);
        fgets(str, sizeof(str), stdin);
        manager[1].signalFence(2);
        printf("press enter to inc to %d\n", 1);
        fgets(str, sizeof(str), stdin);
        manager[1].signalFence(3);


        printf("press enter to exit\n");
        fgets(str, sizeof(str), stdin);
    }


    return 1;
}
