//
// Copyright 2005 The Android Open Source Project
//
// Handle events, like key input and vsync.
//
// The goal is to provide an optimized solution for Linux, not an
// implementation that works well across all platforms.  We expect
// events to arrive on file descriptors, so that we can use a select()
// select() call to sleep.
//
// We can't select() on anything but network sockets in Windows, so we
// provide an alternative implementation of waitEvent for that platform.
//
#define LOG_TAG "ITSLib"

//#define LOG_NDEBUG 0

#include <ui/EventHub.h>
#include <ui/ITSLib.h>

#include <string.h>
#include <stdint.h>
#include <dirent.h>
#ifdef HAVE_ANDROID_OS
# include <sys/limits.h>        /* not part of Linux */
#endif
#include <sys/poll.h>
#include <sys/ioctl.h>

#include <tslib-private.h>
namespace android {

ITSLib::ITSLib(const char* devname):
	ts(NULL), initialized(0)
{
	ts = ts_open(devname, 0);
	if (!ts) {
		perror("ts_open");
		LOGE("Unbale to open touchscreen device\n");
		return;
	}
	if (ts_config(ts)) {
		perror("ts_config");
		LOGE("Unbale to confugure touchscreen device\n");
		ts_close(ts);
		return;
	}
	LOGI("touchscreen device configured sucessefully\n");
	initialized = 1;
}

ITSLib::ITSLib(int fd):
	ts(NULL), initialized(0)
{

	if (fd == -1)
		return;
	/* Hack: Simulate fd_open*/
        ts = (struct tsdev*)malloc(sizeof(struct tsdev));
        if (!ts) {
	    LOGE("Unable to allocate tslib structure\n");
	    return;
        }

        memset(ts, 0, sizeof(struct tsdev));
        ts->fd = fd;

	if (ts_config(ts)) {
		perror("ts_config");
		LOGE("Unbale to confugure touchscreen device\n");
		free(ts);
		return;
	}
	LOGI("touchscreen device configured sucessefully\n");
	initialized = 1;
}

ITSLib::~ITSLib()
{
	ts_close(ts);
}

int ITSLib::GetEvent(struct ts_sample *tsmp)
{
	int ret;

	ret = ts_read(ts, tsmp, 1);

	if (ret < 0) {
		LOGE("ts_read: error:%d", ret);
		return 1;
	}

	if (ret != 1)
		return 1;

	return 0;
}

int ITSLib::GetEvent(InEvent *inevt)
{
    struct ts_sample tsmp;
    if (!inevt)
            return -EINVAL;

	int ret = GetEvent(&tsmp);
	if (ret) {
		return ret;
	}
	inevt->When = tsmp.tv.tv_sec*1000 + tsmp.tv.tv_usec/1000;
	inevt->Type = EventHub::EV_TS;
    inevt->X= tsmp.x;
    inevt->Y= tsmp.y;
    inevt->Pressure = tsmp.pressure;
	return 0;
}

void ITSLib::onFirstRef()
{
}

}; // namespace android

