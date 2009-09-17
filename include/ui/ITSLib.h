#ifndef __ITSLIB_H__
#define __ITSLIB_H__
#ifdef BOARD_USES_TSLIB

#include <utils/threads.h>
#include <utils.h>

#include <linux/input.h>
#include <tslib.h>
#include <ui/IInputDriver.h>
#include <ui/EventHub.h>

namespace android {

class ITSLib:public IInputDriver, RefBase {
public:
	ITSLib(const char* devname);
	ITSLib(int fd);
	virtual int GetEvent(InEvent *inevt);
	virtual ~ITSLib();
	virtual int Initialize() {return initialized; };
protected:
	virtual void onFirstRef();
private:
	struct tsdev *ts;
	int GetEvent(struct ts_sample *tsmp);
	int initialized;
};

}; //namespace android

#endif // BOARD_USES_TSLIB
#endif // __ITSLIB_H__
