/*
 * Author: Matthew Ranostay <Matt_Ranostay@mentor.com>
 * Copyright (C) 2009 Mentor Graphics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.net.ethernet;

/**
 * Native calls for receiving asynchronous events.
 *
 * {@hide}
 */
public class EthernetNative {
    /**
     * Converts IP address to dot-decimal notation
     */
    public static String formatIPAddress(long addr) {
	return String.format("%d.%d.%d.%d", (addr>>24)&0xff, (addr>>16)&0xff, (addr>>8)&0xff, addr&0xff);
    }
    /**
     *
     */
    public static String getIPAddressAsString() {
	return formatIPAddress(getIPAddress());
    }

    /**
     * Checks ethernet interface link status
     * 
     * @return true if link is up, false otherwise
     */
    public native static boolean isLinkUp();

    /**
     * Returns IPv4 address ethernet interface is bound to
     *
     * @return 32 bit internet address, INADDR_ANY if address  is not yet assigned and INADDR_NONE in case of error
     */
    public native static long  getIPAddress();

}
