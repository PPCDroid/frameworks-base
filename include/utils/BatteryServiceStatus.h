#ifndef _BATTERY_SERVICE_STATUS_H
#define _BATTERY_SERVICE_STATUS_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <utils/IBatteryServiceStatus.h>

namespace android {
#define parent_func(x) IBatteryServiceStatus::x

#define child_func(init,type,func)                          \
    BatteryServiceStatus::init##_##type##_##func(void) {  \
        return parent_func(init##_##type##_##func());     \
    }

#define child_bool_func(type,func) child_func(is,type,func)
#define child_int_func(type,func)  child_func(get,type,func)

#define child_string_func(type,func)                                    \
    BatteryServiceStatus::get_##type##_##func(char *buf,size_t sz) {    \
        return parent_func(get_##type##_##func(buf,sz));                \
    }

    class BatteryServiceStatus : public IBatteryServiceStatus
    {
    public:
        virtual int readFromFile(const char* path, char* buf, size_t size);
        virtual int is_ac_online();
        virtual int is_usb_online();
        virtual int is_bat_present();
        virtual int get_bat_temperature();
        virtual int get_bat_status(char *buf, size_t size);
        virtual int get_bat_health(char *buf, size_t size);
        virtual int get_bat_tech(char *buf, size_t size);
        virtual int get_bat_level();
        virtual int get_bat_voltage();
    };
};
#endif //_BATTERY_SERVICE_STATUS_H
