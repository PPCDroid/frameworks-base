#ifndef __IINPUTDRIVER_H__
#define __IINPUTDRIVER_H__
#include <utils/String8.h>
#include <utils.h>

namespace android {

struct InEvent {
    int32_t  DeviceId;
    int32_t  Type;
    int32_t  Scancode;
    int32_t  Keycode;
    uint32_t Flags;
    int32_t  Value;
    nsecs_t  When;
    int32_t  X;
    int32_t  Y;
    int32_t  Pressure;
};


class KeyLayoutMap;

class IInputDriver {
    public:
	virtual int GetEvent(InEvent *inevt) = 0;
	virtual int Initialize() = 0;
};

}; //namespace android

#endif //__IINPUTDRIVER_H__
