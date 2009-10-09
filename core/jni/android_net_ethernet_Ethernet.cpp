/*
 * Author: Matthew Ranostay <matt_ranostay@mentor.com>
 * Copyright 2009 Mentor Graphics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "ethernet"

#include <cutils/properties.h>

#include "jni.h"
#include <utils/misc.h>
#include <android_runtime/AndroidRuntime.h>
#include <utils/Log.h>

extern "C" {

extern int ifc_init();
extern void ifc_close();
extern int ifc_get_info(const char *name, unsigned *addr, unsigned *mask, unsigned *flags);

};

/* TODO: Basic support for notifying userspace of a ethernet connection
 * At some point we need to expand to actually configuring from Android userspace
 */

#define ETHERNET_PKG_NAME "android/net/ethernet/EthernetNative"



namespace android {

static const char *ipaddr(unsigned addr)
{
    static char buf[32];

    addr = ntohl(addr);

    sprintf(buf,"%d.%d.%d.%d",
            (addr >> 24),
            ((addr >> 16) & 255),
            ((addr >> 8) & 255),
            addr & 255);
    return buf;
}

static jboolean android_net_ethernet_getEthernetStatus(JNIEnv* env, jobject clazz)
{
    char key[PROPERTY_KEY_MAX];
    char buf[PROPERTY_VALUE_MAX];
	int len;

	len = property_get("net.device", buf, "");

	if (!len)
		return false;

	sprintf(key, "net.%s.status", buf);
	len = property_get(key, buf, "");

	if (!strcmp(buf, "dhcp"))
		return true;
	else if (!strcmp(buf, "up"))
		return true;

	return false;
}

static jstring android_net_ethernet_getEthernetAddress(JNIEnv* env, jobject clazz)
{
	char buf[PROPERTY_VALUE_MAX];
	int len;
	unsigned int addr, mask, flags;

	len = property_get("net.device", buf, "");

	if (!len)
		return env->NewStringUTF(NULL);

    if(ifc_init()) {
		return env->NewStringUTF(NULL);
	}

    if(ifc_get_info(buf, &addr, &mask, &flags)) {
		return env->NewStringUTF(NULL);
    }

    ifc_close();

	return env->NewStringUTF(ipaddr(addr));
}

/*
 * JNI registration.
 */
static JNINativeMethod gEthernetMethods[] = {
    { "getEthernetStatus", "()Z", (void*) android_net_ethernet_getEthernetStatus },
    { "getEthernetAddress", "()Ljava/lang/String;", (void*) android_net_ethernet_getEthernetAddress },
};

int register_android_net_ethernet_EthernetManager(JNIEnv* env)
{
    jclass ethernet = env->FindClass(ETHERNET_PKG_NAME);
    LOG_FATAL_IF(ethernet == NULL, "Unable to find class " ETHERNET_PKG_NAME);

    return AndroidRuntime::registerNativeMethods(env,
            ETHERNET_PKG_NAME, gEthernetMethods, NELEM(gEthernetMethods));
}

}; // namespace android
