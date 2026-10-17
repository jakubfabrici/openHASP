/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#ifndef HASP_PING_H
#define HASP_PING_H

#if defined(ARDUINO_ARCH_ESP32)

/* "ping <ip> [p#b#]": send 3 ICMP echo requests to an IPv4 address, publish the result on the
 * "ping" state topic and, when an object id was given, write it into that object's text. */
void dispatch_ping(const char*, const char* payload, uint8_t source);
void pingLoop(void);

#endif // ARDUINO_ARCH_ESP32
#endif // HASP_PING_H
