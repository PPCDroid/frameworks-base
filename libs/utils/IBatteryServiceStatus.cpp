#define LOG_TAG "BatteryStatus"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <utils/Log.h>
#include <cutils/atomic.h>

#include <utils/IBatteryServiceStatus.h>

namespace android {

    int32_t IBatteryServiceStatus::path_checked = 0;
    struct feature_prop IBatteryServiceStatus::power_prop[POWER_FEATURE_MAX] =  {
        { POWER_AC_NAME,   0,"\0",SYS_FS_POWER_SUPPLY_PROP_AC           },
        { POWER_BAT_NAME,  0,"\0",SYS_FS_POWER_SUPPLY_PROP_BAT          },
        { POWER_USB_NAME,  0,"\0",SYS_FS_POWER_SUPPLY_PROP_USB          },
        { POWER_AC_ONLINE, 0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_AC_ONLINE },
        { POWER_USB_ONLINE,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_USB_ONLINE},
        { POWER_BAT_PRESENT,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_PRESENT},
        { POWER_BAT_STATUS,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_STATUS},
        { POWER_BAT_HEALTH,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_HEALTH},
        { POWER_BAT_CAPACITY_NOW,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_CAPACITY_NOW},
        { POWER_BAT_CAPACITY_FULL,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_CAPACITY_FULL},
        { POWER_BAT_VOLTAGE_NOW,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_VOLTAGE_NOW},
        { POWER_BAT_VOLTAGE_FULL,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_VOLTAGE_FULL},
        { POWER_BAT_TEMPERATURE,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_TEMPERATURE},
        { POWER_BAT_TECH,0,"\0",SYS_FS_POWER_SUPPLY_PROP_FEATURE_BAT_TECH}};

  IBatteryServiceStatus::IBatteryServiceStatus() {
    struct feature_prop *prop = power_prop;
    char dir[BATTERY_MAX_PATH_LEN];
    int i,type;
    if (android_atomic_cmpxchg(0,1,&IBatteryServiceStatus::path_checked)) {
      LOGI("The BSS has been inited\n");
      return;
    }
    for (i = 0; i < POWER_FEATURE_MAX; i++) {
      prop->present = property_get(prop->name,dir,NULL);

      if ((prop->id == POWER_AC_NAME) || (prop->id == POWER_BAT_NAME) ||
          (prop->id == POWER_USB_NAME)){
        if( prop->present > 0) {
          memcpy(prop->prop,dir,prop->present);
        } else {
            LOGW("Can not find properity for %s\n",prop->name);
        }
      } else {
        if (i <= POWER_AC_LAST )
	    type = POWER_AC_NAME;
	else if ( i <= POWER_USB_LAST)
            type = POWER_USB_NAME;
	else
	    type = POWER_BAT_NAME;
        BUILD_PATH_ON_PROP(type,prop,dir);
      }
      prop++;
    }
  }

  IBatteryServiceStatus::~IBatteryServiceStatus() {}

  int IBatteryServiceStatus::readFromFile(const char *path, char *buf, size_t size) {
    int fd = open(path, O_RDONLY, 0);
    if (fd == -1) {
      LOGE("Could not open '%s'", path);
      return -1;
    }

    size_t count = read(fd, buf, size);
    if (count > 0) {
      count = (count < size) ? count : size - 1;
      while (count > 0 && buf[count-1] == '\n') count--;
      buf[count] = '\0';
    } else {
      buf[0] = '\0';
    }

    close(fd);
    return count;
  }

  int IBatteryServiceStatus::get_bat_level(void) {
    float now = get_bat_capacity_now();
    float full = get_bat_capacity_full();

    if (full > 0) {
      return (int)((now / full) * 100);
    }
    return (int)now;
  }

  int IBatteryServiceStatus::get_bat_voltage(void) {
    return get_bat_voltage_now();
  }

  int get_path_func(ac,online,POWER_AC_ONLINE)
  int get_path_func(bat,present,POWER_BAT_PRESENT)
  int get_path_func(usb,online,POWER_USB_ONLINE)
  int get_path_func(bat,status,POWER_BAT_STATUS)
  int get_path_func(bat,health,POWER_BAT_HEALTH)
  int get_path_func(bat,capacity_now,POWER_BAT_CAPACITY_NOW)
  int get_path_func(bat,capacity_full,POWER_BAT_CAPACITY_FULL)
  int get_path_func(bat,voltage_now,POWER_BAT_VOLTAGE_NOW)
  int get_path_func(bat,voltage_full,POWER_BAT_VOLTAGE_FULL)
  int get_path_func(bat,temperature,POWER_BAT_TEMPERATURE)
  int get_path_func(bat,tech,POWER_BAT_TECH)


    int get_bool_func(ac,online,POWER_AC_ONLINE)
    int get_bool_func(usb,online,POWER_USB_ONLINE)
    int get_bool_func(bat,present,POWER_BAT_PRESENT)

    int get_int_func(bat,capacity_now,POWER_BAT_CAPACITY_NOW)
    int get_int_func(bat,capacity_full,POWER_BAT_CAPACITY_FULL)
    int get_int_func(bat,voltage_now,POWER_BAT_VOLTAGE_NOW)
    int get_int_func(bat,voltage_full,POWER_BAT_VOLTAGE_FULL)
    int get_int_func(bat,temperature,POWER_BAT_TEMPERATURE)

    int get_string_func(bat,status,POWER_BAT_STATUS)
    int get_string_func(bat,health,POWER_BAT_HEALTH)
    int get_string_func(bat,tech,POWER_BAT_TECH)


};
