#ifndef _IBATTERY_SERVICE_STATUS_H
#define _IBATTERY_SERVICE_STATUS_H

#include <cutils/properties.h>

#define SYS_FS_POWER_SUPPLY_PROP_BASE "ro.sys.fs.power_supply"
#define SYS_FS_POWER_SUPPLY_BASE "/sys/class/power_supply"

#define SYS_FS_POWER_SUPPLY_PROP_AC   "ro.sys.fs.power_supply.ac"
#define SYS_FS_POWER_SUPPLY_PROP_BAT  "ro.sys.fs.power_supply.bat"
#define SYS_FS_POWER_SUPPLY_PROP_USB  "ro.sys.fs.power_supply.usb"
#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_AC_ONLINE  \
    "ro.sys.fs.power_supply.ac.feature.online"

#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_USB_ONLINE \
    "ro.sys.fs.power_supply.usb.feature.online"

#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_PRESENT \
    "ro.sys.fs.power_supply.bat.feature.present"

#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_CAPACITY_NOW \
        "ro.sys.fs.power_supply.bat.feature.capacity.now"
#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_CAPACITY_FULL \
        "ro.sys.fs.power_supply.bat.feature.capacity.full"

#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_VOLTAGE_NOW \
    "ro.sys.fs.power_supply.bat.feature.voltage.now"

#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_VOLTAGE_FULL \
    "ro.sys.fs.power_supply.bat.feature.voltage.full"

#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_TEMPERATURE \
    "ro.sys.fs.power_supply.bat.feature.temperature"

#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_HEALTH \
        "ro.sys.fs.power_supply.bat.feature.health"

#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_TECH \
    "ro.sys.fs.power_supply.bat.feature.tech"

#define SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_STATUS \
    "ro.sys.fs.power_supply.bat.feature.status"


#define PATH_PARAM_LEN(id)  (power_prop[(id)].present)

#define PATH_PARAM_CHECK_OK(id,o_len)                         \
    (__extension__                                            \
     ( {                                                      \
         __typeof__ (id) _id = id;                            \
         int __ret = (PATH_PARAM_LEN(_id) > 0 &&               \
                      ((PATH_PARAM_LEN(_id)) < (o_len)));      \
         _ret;                                                 \
     })                                                        \
     )
/*
 * do this to save some space in the prop memory (hopefully)
 * otherwise, the full path should be defined in the prop file
 * directly instead of going through these copies.
 * <Fixit:These whole thing may not even worth to do and cause lower perf>
 */
#define BUILD_PATH_ON_PROP(type,prop,dir) do {                     \
        struct feature_prop *_prop = prop;                               \
        char *_dir = dir;                                               \
        char *_path = NULL;                                             \
        int _x = strlen(SYS_FS_POWER_SUPPLY_BASE);                      \
        if (_dir && _prop && (_prop->present > 0)                       \
            && power_prop[type].present){                \
            _path = _prop->prop;                                        \
            memcpy(_path,SYS_FS_POWER_SUPPLY_BASE,_x);                  \
            _path = _path+_x;                                           \
            memcpy(_path,power_prop[type].prop,          \
                   strlen(power_prop[type].prop));       \
            _x = _x + power_prop[type].present;          \
            _path = _path+strlen(power_prop[type].prop); \
            memcpy(_path,_dir,_prop->present);                          \
            _x = _x + _prop->present;                                   \
            _prop->present = _x;                                        \
        } else {                                                        \
            _prop->present = 0;                                         \
        }                                                               \
    } while(0)

#define GET_PATH_ON_ID(path,id,sz)                                      \
    (__extension__                                                      \
     ({__typeof__ (path) _path = path;                                  \
         __typeof__(id) _id = id;                                       \
         __typeof__(sz) _sz = sz;                                       \
         int _x = 0;                                                    \
         if (_path && PATH_PARAM_CHECK_OK(_id,_sz)) {                   \
             memcpy(_path,power_prop[_id].prop,                         \
                    power_prop[_id].present);                           \
             _x = power_prop[id].present;                               \
         }                                                              \
         _x;                                                            \
     }))



#define get_path_func(dev,type,id)                                      \
    IBatteryServiceStatus::get_##dev##_##type##_path(char **path) {     \
        *path = power_prop[id].prop ;                                   \
        return power_prop[id].present;                                  \
    }

#define POWER_BUF_SZ_SMALL 16
#define POWER_BUF_SZ_MID 128

#define get_bool_func(dev,type,id)                                      \
    IBatteryServiceStatus::is_##dev##_##type(void){                     \
      int _value = 0;                                                   \
      char _buf[POWER_BUF_SZ_SMALL];                                    \
      if (power_prop[id].present > 0 &&                                 \
          (readFromFile(power_prop[id].prop,_buf,POWER_BUF_SZ_SMALL) > 0)) { \
        if (_buf[0] == '1') {                                           \
          _value = 1;                                                   \
        }                                                               \
      }                                                                 \
      return _value;                                                    \
    }

#define get_int_func(dev,type,id)                                       \
    IBatteryServiceStatus::get_##dev##_##type(void){                     \
      int _value = -1;                                                   \
      char _buf[POWER_BUF_SZ_MID];                                      \
      if (power_prop[id].present > 0 &&                                 \
          readFromFile(power_prop[id].prop,_buf,POWER_BUF_SZ_MID) > 0 ) { \
          _value = atoi(_buf);                                          \
      }                                                                 \
      return _value;                                                    \
    }

#define get_string_func(dev,type,id)                                    \
    IBatteryServiceStatus::get_##dev##_##type(char *buf,size_t sz){  \
        int _value = -1;                                                 \
        if (power_prop[id].present > 0 ) {                              \
            _value = readFromFile(power_prop[id].prop,buf,sz);         \
        }                                                              \
        return _value;                                                 \
    }

namespace android {
    enum {
        POWER_AC_NAME = 0,
        POWER_BAT_NAME,
        POWER_USB_NAME,
        POWER_AC_ONLINE,
        POWER_USB_ONLINE,
        POWER_BAT_PRESENT,
        POWER_BAT_STATUS,
        POWER_BAT_HEALTH,
        POWER_BAT_CAPACITY_NOW,
        POWER_BAT_CAPACITY_FULL,
        POWER_BAT_VOLTAGE_NOW,
        POWER_BAT_VOLTAGE_FULL,
        POWER_BAT_TEMPERATURE,
        POWER_BAT_TECH,
        POWER_FEATURE_MAX
    };

#define BATTERY_MAX_PATH_LEN 512
#define POWER_AC_LAST  POWER_AC_ONLINE
#define POWER_USB_LAST POWER_USB_ONLINE
#define POWER_BAT_LAST POWER_BAT_TECH

    struct feature_prop {
        int id;
        int present;
        char prop[PROPERTY_KEY_MAX];
        const char *name;
    };

    class IBatteryServiceStatus {
        static struct feature_prop power_prop[POWER_FEATURE_MAX];
        static int32_t path_checked;

    public:
        IBatteryServiceStatus();
        virtual ~IBatteryServiceStatus();

        int get_ac_online_path(char **path);
        int get_usb_online_path(char **path);
        int get_bat_status_path(char **path);
        int get_bat_health_path(char **path);
        int get_bat_present_path(char **path);
        int get_bat_capacity_now_path(char **path);
        int get_bat_capacity_full_path(char **path);
        int get_bat_voltage_now_path(char **path);
        int get_bat_voltage_full_path(char **path);
        int get_bat_temperature_path(char **path);
        int get_bat_tech_path(char **path);

        int get_bat_status_id(const char *status);
        int get_bat_health_id(const char *status);
        int get_bat_capacity_now();
        int get_bat_capacity_full();
        int get_bat_voltage_now();
        int get_bat_voltage_full();

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
#endif
