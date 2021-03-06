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
#define LOG_TAG "EventHub"

//#define LOG_NDEBUG 0

#include <ui/EventHub.h>
#include <ui/ITSLib.h>
#include <hardware_legacy/power.h>

#include <cutils/properties.h>
#include <utils/IServiceManager.h>
#include <utils/Log.h>
#include <utils/Timers.h>
#include <utils.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <assert.h>

#include "KeyLayoutMap.h"

#include <string.h>
#include <stdint.h>
#include <dirent.h>
#ifdef HAVE_INOTIFY
# include <sys/inotify.h>
#endif
#ifdef HAVE_ANDROID_OS
# include <sys/limits.h>        /* not part of Linux */
#endif
#include <sys/poll.h>
#include <sys/ioctl.h>

#ifndef LONG_BITS
#define LONG_BITS (sizeof(long) * 8)
#endif

#define LONG_BITMASK (LONG_BITS - 1)
#define LONG_BITSZ(x) (((x) + (LONG_BITS - 1)) / LONG_BITS)

/* this macro is used to tell if "bit" is set in "array"
 * it selects a byte from the array, and does a boolean AND
 * operation with a byte that only has the relevant bit set.
 * eg. to check for the 12th bit, we do (array[1] & 1<<4)
 */
#define test_bit(bit, array)    ((array[bit/LONG_BITS] & (1LU<<(bit%LONG_BITS))) != 0)

#define ID_MASK  0x0000ffff
#define SEQ_MASK 0x7fff0000
#define SEQ_SHIFT 16
#define id_to_index(id)         ((id&ID_MASK)+1)

namespace android {

static const char *WAKE_LOCK_ID = "KeyEvents";
static const char *device_path = "/dev/input";

/* return the larger integer */
static inline int max(int v1, int v2)
{
    return (v1 > v2) ? v1 : v2;
}

EventHub::device_t::device_t(int32_t _id, const char* _path)
    : id(_id), path(_path), classes(0)
    , keyBitmask(NULL), layoutMap(new KeyLayoutMap()), next(NULL),driver(NULL) {
}

EventHub::device_t::~device_t() {
    delete [] keyBitmask;
    delete layoutMap;
}

EventHub::EventHub(void)
    : mError(NO_INIT), mHaveFirstKeyboard(false), mFirstKeyboardId(0)
    , mDevicesById(0), mNumDevicesById(0)
    , mOpeningDevices(0), mClosingDevices(0)
    , mDevices(0), mFDs(0), mFDCount(0)
{
    acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);
#ifdef EV_SW
    memset(mSwitches, 0, sizeof(mSwitches));
#endif
}

/*
 * Clean up.
 */
EventHub::~EventHub(void)
{
    release_wake_lock(WAKE_LOCK_ID);
    // we should free stuff here...
}

void EventHub::onFirstRef()
{
    mError = openPlatformInput() ? NO_ERROR : UNKNOWN_ERROR;
}

status_t EventHub::errorCheck() const
{
    return mError;
}

String8 EventHub::getDeviceName(int32_t deviceId) const
{
    AutoMutex _l(mLock);
    device_t* device = getDevice(deviceId);
    if (device == NULL) return String8();
    return device->name;
}

uint32_t EventHub::getDeviceClasses(int32_t deviceId) const
{
    AutoMutex _l(mLock);
    device_t* device = getDevice(deviceId);
    if (device == NULL) return 0;
    return device->classes;
}

int EventHub::getAbsoluteInfo(int32_t deviceId, int axis, int *outMinValue,
        int* outMaxValue, int* outFlat, int* outFuzz) const
{
    AutoMutex _l(mLock);
    device_t* device = getDevice(deviceId);
    if (device == NULL) return -1;

    struct input_absinfo info;

    if(ioctl(mFDs[id_to_index(device->id)].fd, EVIOCGABS(axis), &info)) {
        LOGE("Error reading absolute controller %d for device %s fd %d\n",
             axis, device->name.string(), mFDs[id_to_index(device->id)].fd);
        return -1;
    }
    *outMinValue = info.minimum;
    *outMaxValue = info.maximum;
    *outFlat = info.flat;
    *outFuzz = info.fuzz;
    return 0;
}

int EventHub::getSwitchState(int sw) const
{
#ifdef EV_SW
    if (sw >= 0 && sw <= SW_MAX) {
        int32_t devid = mSwitches[sw];
        if (devid != 0) {
            return getSwitchState(devid, sw);
        }
    }
#endif
    return -1;
}

int EventHub::getSwitchState(int32_t deviceId, int sw) const
{
#ifdef EV_SW
    AutoMutex _l(mLock);
    device_t* device = getDevice(deviceId);
    if (device == NULL) return -1;
    
    if (sw >= 0 && sw <= SW_MAX) {
        unsigned long sw_bitmask[LONG_BITSZ(SW_MAX)];
        memset(sw_bitmask, 0, sizeof(sw_bitmask));
        if (ioctl(mFDs[id_to_index(device->id)].fd,
                   EVIOCGSW(sizeof(sw_bitmask)), sw_bitmask) >= 0) {
            return test_bit(sw, sw_bitmask) ? 1 : 0;
        }
    }
#endif
    
    return -1;
}

int EventHub::getScancodeState(int code) const
{
    return getScancodeState(mFirstKeyboardId, code);
}

int EventHub::getScancodeState(int32_t deviceId, int code) const
{
    AutoMutex _l(mLock);
    device_t* device = getDevice(deviceId);
    if (device == NULL) return -1;
    
    if (code >= 0 && code <= KEY_MAX) {
        unsigned long key_bitmask[LONG_BITSZ(KEY_MAX)];
        memset(key_bitmask, 0, sizeof(key_bitmask));
        if (ioctl(mFDs[id_to_index(device->id)].fd,
                   EVIOCGKEY(sizeof(key_bitmask)), key_bitmask) >= 0) {
            return test_bit(code, key_bitmask) ? 1 : 0;
        }
    }
    
    return -1;
}

int EventHub::getKeycodeState(int code) const
{
    return getKeycodeState(mFirstKeyboardId, code);
}

int EventHub::getKeycodeState(int32_t deviceId, int code) const
{
    AutoMutex _l(mLock);
    device_t* device = getDevice(deviceId);
    if (device == NULL || device->layoutMap == NULL) return -1;
    
    Vector<int32_t> scanCodes;
    device->layoutMap->findScancodes(code, &scanCodes);
    
    unsigned long key_bitmask[LONG_BITSZ(KEY_MAX)];
    memset(key_bitmask, 0, sizeof(key_bitmask));
    if (ioctl(mFDs[id_to_index(device->id)].fd,
               EVIOCGKEY(sizeof(key_bitmask)), key_bitmask) >= 0) {
        #if 0
        for (size_t i=0; i<=KEY_MAX; i++) {
            LOGI("(Scan code %d: down=%d)", i, test_bit(i, key_bitmask));
        }
        #endif
        const size_t N = scanCodes.size();
        for (size_t i=0; i<N && i<=KEY_MAX; i++) {
            int32_t sc = scanCodes.itemAt(i);
            LOGI("Code %d: down=%d", sc, test_bit(sc, key_bitmask));
            if (sc >= 0 && sc <= KEY_MAX && test_bit(sc, key_bitmask)) {
                return 1;
            }
        }
    }
    
    return 0;
}

EventHub::device_t* EventHub::getDevice(int32_t deviceId) const
{
    if (deviceId == 0) deviceId = mFirstKeyboardId;
    int32_t id = deviceId & ID_MASK;
    if (id >= mNumDevicesById || id < 0) return NULL;
    device_t* dev = mDevicesById[id].device;
    if (dev == NULL) return NULL;
    if (dev->id == deviceId) {
        return dev;
    }
    return NULL;
}

bool EventHub::getEvent(InEvent *inevt)
{
    status_t err;

    fd_set readfds;
    int maxFd = -1;
    int cc;
    int i;
    int res;
    int pollres;
    struct input_event iev;
    memset(inevt, 0, sizeof(*inevt));

    // Note that we only allow one caller to getEvent(), so don't need
    // to do locking here...  only when adding/removing devices.
    
    while(1) {

        // First, report any devices that had last been added/removed.
        if (mClosingDevices != NULL) {
            device_t* device = mClosingDevices;
            LOGV("Reporting device closed: id=0x%x, name=%s\n",
                 device->id, device->path.string());
            mClosingDevices = device->next;
            inevt->DeviceId = device->id;
            if (inevt->DeviceId == mFirstKeyboardId) inevt->DeviceId = 0;
            inevt->Type = DEVICE_REMOVED;
            delete device;
            return true;
        }
        if (mOpeningDevices != NULL) {
            device_t* device = mOpeningDevices;
            LOGV("Reporting device opened: id=0x%x, name=%s\n",
                 device->id, device->path.string());
            mOpeningDevices = device->next;
            inevt->DeviceId = device->id;
            if (inevt->DeviceId == mFirstKeyboardId) inevt->DeviceId = 0;
            inevt->Type = DEVICE_ADDED;
            return true;
        }

        release_wake_lock(WAKE_LOCK_ID);

        pollres = poll(mFDs, mFDCount, -1);

        acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_ID);

        if (pollres <= 0) {
            if (errno != EINTR) {
                LOGW("select failed (errno=%d)\n", errno);
                usleep(100000);
            }
            continue;
        }

        //printf("poll %d, returned %d\n", mFDCount, pollres);

        // mFDs[0] is used for inotify, so process regular events starting at mFDs[1]
        for(i = 1; i < mFDCount; i++) {
            if(mFDs[i].revents) {
                LOGV("revents for %d = 0x%08x", i, mFDs[i].revents);
                if(mFDs[i].revents & POLLIN) {
                    if (mDevices[i]->driver) {
                        if (mDevices[i]->driver->GetEvent(inevt))
                            continue;
                        inevt->DeviceId = mDevices[i]->id; //XXX
                        return true;
                    } else {
                        res = read(mFDs[i].fd, &iev, sizeof(iev));
                        if (res == sizeof(iev)) {
                            LOGV("%s got: t0=%d, t1=%d, type=%d, code=%d, v=%d",
                                 mDevices[i]->path.string(),
                                 (int) iev.time.tv_sec, (int) iev.time.tv_usec,
                                 iev.type, iev.code, iev.value);
                            inevt->DeviceId = mDevices[i]->id;
                            if (inevt->DeviceId == mFirstKeyboardId)
                                inevt->DeviceId = 0;
                            inevt->Type = iev.type;
                            inevt->Scancode = iev.code;
                            if (iev.type == EV_KEY) {
                                err = mDevices[i]->layoutMap->map(iev.code,
                                        &inevt->Keycode,
                                        &inevt->Flags);
                                LOGV("iev.code=%d outKeycode=%d outFlags=0x%08x"
                                                " err=%d\n", iev.code,
                                                inevt->Keycode, inevt->Flags,
                                                err);
                                if (err != 0) {
                                    inevt->Keycode = 0;
                                    inevt->Flags = 0;
                                }
                            } else {
                                inevt->Keycode = iev.code;
                            }
                            inevt->Value = iev.value;
                            inevt->When = s2ns(iev.time.tv_sec) +
                                    us2ns(iev.time.tv_usec);
                            return true;
                        } else {
                            if (res<0) {
                                LOGW("could not get event (errno=%d)", errno);
                            } else {
                                LOGE("could not get event (wrong size: %d)",
                                                res);
                            }
                            continue;
                        }
                    }
                }
            }
        }
        
        // read_notify() will modify mFDs and mFDCount, so this must be done after
        // processing all other events.
        if(mFDs[0].revents & POLLIN) {
            read_notify(mFDs[0].fd);
        }
    }
}

/*
 * Open the platform-specific input device.
 */
bool EventHub::openPlatformInput(void)
{
    /*
     * Open platform-specific input device(s).
     */
    int res;

    mFDCount = 1;
    mFDs = (pollfd *)calloc(1, sizeof(mFDs[0]));
    mDevices = (device_t **)calloc(1, sizeof(mDevices[0]));
    mFDs[0].events = POLLIN;
    mDevices[0] = NULL;
#ifdef HAVE_INOTIFY
    mFDs[0].fd = inotify_init();
    res = inotify_add_watch(mFDs[0].fd, device_path, IN_DELETE | IN_CREATE);
    if(res < 0) {
        LOGE("could not add watch for %s, %s\n", device_path, strerror(errno));
    }
#else
    /*
     * The code in EventHub::getEvent assumes that mFDs[0] is an inotify fd.
     * We allocate space for it and set it to something invalid.
     */
    mFDs[0].fd = -1;
#endif

    res = scan_dir(device_path);
    if(res < 0) {
        LOGE("scan dir failed for %s\n", device_path);
        //open_device("/dev/input/event0");
    }

    return true;
}

/*
 * Inspect the known devices to determine whether physical keys exist for the given
 * framework-domain key codes.
 */
bool EventHub::hasKeys(size_t numCodes, int32_t* keyCodes, uint8_t* outFlags) {
    for (size_t codeIndex = 0; codeIndex < numCodes; codeIndex++) {
        outFlags[codeIndex] = 0;

        // check each available hardware device for support for this keycode
        Vector<int32_t> scanCodes;
        for (int n = 0; (n < mFDCount) && (outFlags[codeIndex] == 0); n++) {
            if (mDevices[n]) {
                status_t err = mDevices[n]->layoutMap->findScancodes(keyCodes[codeIndex], &scanCodes);
                if (!err) {
                    // check the possible scan codes identified by the layout map against the
                    // map of codes actually emitted by the driver
                    for (size_t sc = 0; sc < scanCodes.size(); sc++) {
                        if (test_bit(scanCodes[sc], mDevices[n]->keyBitmask)) {
                            outFlags[codeIndex] = 1;
                            break;
                        }
                    }
                }
            }
        }
    }

    return true;
}

// ----------------------------------------------------------------------------

int EventHub::open_device(const char *deviceName)
{
    int version;
    int fd;
    struct pollfd *new_mFDs;
    device_t **new_devices;
    char **new_device_names;
    char name[80];
    char location[80];
    char idstr[80];
    struct input_id id;

    LOGV("Opening device: %s", deviceName);

    AutoMutex _l(mLock);
    
    fd = open(deviceName, O_RDWR);
    if(fd < 0) {
        LOGE("could not open %s, %s\n", deviceName, strerror(errno));
        return -1;
    }

    if(ioctl(fd, EVIOCGVERSION, &version)) {
        LOGE("could not get driver version for %s, %s\n", deviceName, strerror(errno));
        return -1;
    }
    if(ioctl(fd, EVIOCGID, &id)) {
        LOGE("could not get driver id for %s, %s\n", deviceName, strerror(errno));
        return -1;
    }
    name[sizeof(name) - 1] = '\0';
    location[sizeof(location) - 1] = '\0';
    idstr[sizeof(idstr) - 1] = '\0';
    if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
        //fprintf(stderr, "could not get device name for %s, %s\n", deviceName, strerror(errno));
        name[0] = '\0';
    }
    if(ioctl(fd, EVIOCGPHYS(sizeof(location) - 1), &location) < 1) {
        //fprintf(stderr, "could not get location for %s, %s\n", deviceName, strerror(errno));
        location[0] = '\0';
    }
    if(ioctl(fd, EVIOCGUNIQ(sizeof(idstr) - 1), &idstr) < 1) {
        //fprintf(stderr, "could not get idstring for %s, %s\n", deviceName, strerror(errno));
        idstr[0] = '\0';
    }

    int devid = 0;
    while (devid < mNumDevicesById) {
        if (mDevicesById[devid].device == NULL) {
            break;
        }
        devid++;
    }
    if (devid >= mNumDevicesById) {
        device_ent* new_devids = (device_ent*)realloc(mDevicesById,
                sizeof(mDevicesById[0]) * (devid + 1));
        if (new_devids == NULL) {
            LOGE("out of memory");
            return -1;
        }
        mDevicesById = new_devids;
        mNumDevicesById = devid+1;
        mDevicesById[devid].device = NULL;
        mDevicesById[devid].seq = 0;
    }

    mDevicesById[devid].seq = (mDevicesById[devid].seq+(1<<SEQ_SHIFT))&SEQ_MASK;
    if (mDevicesById[devid].seq == 0) {
        mDevicesById[devid].seq = 1<<SEQ_SHIFT;
    }

    new_mFDs = (pollfd*)realloc(mFDs, sizeof(mFDs[0]) * (mFDCount + 1));
    new_devices = (device_t**)realloc(mDevices, sizeof(mDevices[0]) * (mFDCount + 1));
    if (new_mFDs == NULL || new_devices == NULL) {
        LOGE("out of memory");
        return -1;
    }
    mFDs = new_mFDs;
    mDevices = new_devices;

#if 0
    LOGI("add device %d: %s\n", mFDCount, deviceName);
    LOGI("  bus:      %04x\n"
         "  vendor    %04x\n"
         "  product   %04x\n"
         "  version   %04x\n",
        id.bustype, id.vendor, id.product, id.version);
    LOGI("  name:     \"%s\"\n", name);
    LOGI("  location: \"%s\"\n"
         "  id:       \"%s\"\n", location, idstr);
    LOGI("  version:  %d.%d.%d\n",
        version >> 16, (version >> 8) & 0xff, version & 0xff);
#endif

    device_t* device = new device_t(devid|mDevicesById[devid].seq, deviceName);
    if (device == NULL) {
        LOGE("out of memory");
        return -1;
    }

    mFDs[mFDCount].fd = fd;
    mFDs[mFDCount].events = POLLIN;

    // figure out the kinds of events the device reports
    unsigned long key_bitmask[LONG_BITSZ(KEY_MAX)];
    int ret;
    memset(key_bitmask, 0, sizeof(key_bitmask));
    LOGV("Getting keys...");
    ret = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bitmask)), key_bitmask);
    if (ret < 0) {
        delete device;
        LOGE("Failed to EVIOCGBIT(EV_KEY..)");
        return -1;
    }

#if 1
    LOGI("MAP\n");
    for (unsigned int i=0; i< LONG_BITSZ(KEY_MAX); i++)
        LOGI("%d: 0x%08lx\n", i, key_bitmask[i]);
#endif

    for (unsigned int i=0; i<(BTN_MISC+(sizeof(long)*8 - 1)/(sizeof(long)*8)); i++) {
        if (key_bitmask[i] != 0) {
            device->classes |= CLASS_KEYBOARD;
            LOGI("%s:%d CLASS_KEYBOARD\n", __FILE__, __LINE__);
            // 'Q' key support = cheap test of whether this is an alpha-capable kbd
            if (test_bit(KEY_Q, key_bitmask)) {
                device->classes |= CLASS_ALPHAKEY;
                LOGI("%s:%d CLASS_ALPHAKEY\n", __FILE__, __LINE__);
            }
            break;
        }
    }

    if ((device->classes & CLASS_KEYBOARD) != 0) {
        device->keyBitmask = new unsigned long[LONG_BITSZ(KEY_MAX)];
        if (device->keyBitmask != NULL) {
            memcpy(device->keyBitmask, key_bitmask, sizeof(key_bitmask));
        } else {
            delete device;
            LOGE("out of memory allocating key bitmask");
            return -1;
        }
    }

    if (test_bit(BTN_MOUSE, key_bitmask)) {
        unsigned long rel_bitmask[LONG_BITSZ(REL_MAX)];
        memset(rel_bitmask, 0, sizeof(rel_bitmask));
        LOGV("Getting relative controllers...");
#if 1
        LOGI("REL MAP\n");
        for (unsigned int i=0; i<LONG_BITSZ(REL_MAX); i++)
            LOGI("%d: 0x%08lx\n", i, rel_bitmask[i]);
#endif
        if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bitmask)), rel_bitmask) >= 0) {
            LOGI("%s:%d EV_REL\n", __FILE__, __LINE__);
            if (test_bit(REL_X, rel_bitmask) && test_bit(REL_Y, rel_bitmask)) {
#ifdef BOARD_USES_MOUSE
                if (test_bit(BTN_LEFT, key_bitmask) && test_bit(BTN_RIGHT, key_bitmask))
                    device->classes |= CLASS_MOUSE;
                else
#endif
                    device->classes |= CLASS_TRACKBALL;
            }
        }
    }
    if (test_bit(BTN_TOUCH, key_bitmask)) {
        unsigned long abs_bitmask[LONG_BITSZ(ABS_MAX)];
        memset(abs_bitmask, 0, sizeof(abs_bitmask));
        LOGV("Getting absolute controllers...");
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bitmask)), abs_bitmask) >= 0)
        {
            LOGI("%s:%d EV_ABS\n", __FILE__, __LINE__);
            if (test_bit(ABS_X, abs_bitmask) && test_bit(ABS_Y, abs_bitmask)) {
                device->classes |= CLASS_TOUCHSCREEN;
                LOGI("%s:%d CLASS_TOUCHSCREEN\n", __FILE__, __LINE__);

#ifdef BOARD_USES_TSLIB
                //Attach TSlib if possible
                device->driver = new ITSLib(fd);
                if (device->driver && !device->driver->Initialize()) {
                    delete device->driver;
                    device->driver =  NULL;
                    LOGE("Unable to use TSLib");
                } else {
                    LOGE("TSLib is used to handle TouchScreen events");
                }
#endif
            }
        }
    }

#ifdef EV_SW
    // figure out the switches this device reports
    unsigned long sw_bitmask[LONG_BITSZ(SW_MAX)];
    memset(sw_bitmask, 0, sizeof(sw_bitmask));
    if (ioctl(fd, EVIOCGBIT(EV_SW, sizeof(sw_bitmask)), sw_bitmask) >= 0) {
        for (int i=0; i<EV_SW; i++) {
            LOGI("Device 0x%x sw %d: has=%d", device->id, i, test_bit(i, sw_bitmask));
            if (test_bit(i, sw_bitmask)) {
                if (mSwitches[i] == 0) {
                    mSwitches[i] = device->id;
                }
            }
        }
    }
#endif

    LOGI("New device: path=%s name=%s id=0x%x (of 0x%x) index=%d fd=%d classes=0x%x\n",
         deviceName, name, device->id, mNumDevicesById, mFDCount, fd, device->classes);

    if ((device->classes&CLASS_KEYBOARD) != 0) {
        char devname[101];
        char tmpfn[101];
        char keylayoutFilename[300];

        // a more descriptive name
        ioctl(mFDs[mFDCount].fd, EVIOCGNAME(sizeof(devname)-1), devname);
        devname[sizeof(devname)-1] = 0;
        device->name = devname;

        // replace all the spaces with underscores
        strcpy(tmpfn, devname);
        for (char *p = strchr(tmpfn, ' '); p && *p; p = strchr(tmpfn, ' '))
            *p = '_';

        // find the .kl file we need for this device
        const char* root = getenv("ANDROID_ROOT");
        snprintf(keylayoutFilename, sizeof(keylayoutFilename),
                 "%s/usr/keylayout/%s.kl", root, tmpfn);
        bool defaultKeymap = false;
        if (access(keylayoutFilename, R_OK)) {
            snprintf(keylayoutFilename, sizeof(keylayoutFilename),
                     "%s/usr/keylayout/%s", root, "qwerty.kl");
            defaultKeymap = true;
        }
        device->layoutMap->load(keylayoutFilename);

        // tell the world about the devname (the descriptive name)
        int32_t publicID;
        if (!mHaveFirstKeyboard && !defaultKeymap) {
            publicID = 0;
            // the built-in keyboard has a well-known device ID of 0,
            // this device better not go away.
            mHaveFirstKeyboard = true;
            mFirstKeyboardId = device->id;
        } else {
            publicID = device->id;
            // ensure mFirstKeyboardId is set to -something-.
            if (mFirstKeyboardId == 0) {
                mFirstKeyboardId = device->id;
            }
        }
        char propName[100];
        sprintf(propName, "hw.keyboards.%u.devname", publicID);
        property_set(propName, devname);

        LOGI("New keyboard: publicID=%d device->id=%d devname='%s' propName='%s' keylayout='%s'\n",
                publicID, device->id, devname, propName, keylayoutFilename);
    }

    LOGV("Adding device %s %p at %d, id = %d, classes = 0x%x\n",
         deviceName, device, mFDCount, devid, device->classes);

    mDevicesById[devid].device = device;
    device->next = mOpeningDevices;
    mOpeningDevices = device;
    mDevices[mFDCount] = device;

    mFDCount++;
    return 0;
}

int EventHub::close_device(const char *deviceName)
{
    AutoMutex _l(mLock);
    
    int i;
    for(i = 1; i < mFDCount; i++) {
        if(strcmp(mDevices[i]->path.string(), deviceName) == 0) {
            //LOGD("remove device %d: %s\n", i, deviceName);
            device_t* device = mDevices[i];
            int count = mFDCount - i - 1;
            int index = (device->id&ID_MASK);
            mDevicesById[index].device = NULL;
            memmove(mDevices + i, mDevices + i + 1, sizeof(mDevices[0]) * count);
            memmove(mFDs + i, mFDs + i + 1, sizeof(mFDs[0]) * count);

#ifdef EV_SW
            for (int j=0; j<EV_SW; j++) {
                if (mSwitches[j] == device->id) {
                    mSwitches[j] = 0;
                }
            }
#endif
            
            device->next = mClosingDevices;
            mClosingDevices = device;

            mFDCount--;

            uint32_t publicID;
            if (device->id == mFirstKeyboardId) {
                LOGW("built-in keyboard device %s (id=%d) is closing! the apps will not like this",
                        device->path.string(), mFirstKeyboardId);
                mFirstKeyboardId = 0;
                publicID = 0;
            } else {
                publicID = device->id;
            }
            // clear the property
            char propName[100];
            sprintf(propName, "hw.keyboards.%u.devname", publicID);
            property_set(propName, NULL);
            return 0;
        }
    }
    LOGE("remote device: %s not found\n", deviceName);
    return -1;
}

int EventHub::read_notify(int nfd)
{
#ifdef HAVE_INOTIFY
    int res;
    char devname[PATH_MAX];
    char *filename;
    char event_buf[512];
    int event_size;
    int event_pos = 0;
    struct inotify_event *event;

    res = read(nfd, event_buf, sizeof(event_buf));
    if(res < (int)sizeof(*event)) {
        if(errno == EINTR)
            return 0;
        LOGW("could not get event, %s\n", strerror(errno));
        return 1;
    }
    //printf("got %d bytes of event information\n", res);

    strcpy(devname, device_path);
    filename = devname + strlen(devname);
    *filename++ = '/';

    while(res >= (int)sizeof(*event)) {
        event = (struct inotify_event *)(event_buf + event_pos);
        //printf("%d: %08x \"%s\"\n", event->wd, event->mask, event->len ? event->name : "");
        if(event->len) {
            strcpy(filename, event->name);
            if(event->mask & IN_CREATE) {
                open_device(devname);
            }
            else {
                close_device(devname);
            }
        }
        event_size = sizeof(*event) + event->len;
        res -= event_size;
        event_pos += event_size;
    }
#endif
    return 0;
}


int EventHub::scan_dir(const char *dirname)
{
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        open_device(devname);
    }
    closedir(dir);
    return 0;
}

}; // namespace android
