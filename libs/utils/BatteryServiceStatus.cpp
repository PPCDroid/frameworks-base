#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <utils/Log.h>

#include <utils/BatteryServiceStatus.h>

namespace android {

  int BatteryServiceStatus::readFromFile(const char *path, char *buf, size_t size) {
    return parent_func(readFromFile(path,buf,size));
  }
  int child_bool_func(ac,online)
  int child_bool_func(bat,present)
  int child_bool_func(usb,online)
  int child_int_func(bat,temperature)
  int child_int_func(bat,level)
  int child_int_func(bat,voltage)
  int child_string_func(bat,status)
  int child_string_func(bat,health)
  int child_string_func(bat,tech)
};
