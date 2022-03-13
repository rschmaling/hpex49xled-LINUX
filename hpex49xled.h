#ifndef INCLUDED_HPEX49XLED
#define INCLUDED_HPEX49XLED
#define _GNU_SOURCE
/////////////////////////////////////////////////////////////////////////////
/////// @file hpex49xled.h
///////
/////// Daemon for controlling the LEDs on the HP MediaSmart Server EX49X
/////// Linux Support - written for Ubuntu 20.04 or greater. 
///////
/////// -------------------------------------------------------------------------
///////
/////// Copyright (c) 2022 Robert Schmaling
/////// Most code taken from https://github.com/merelin/mediasmartserverd 
/////// written by Chris Byrne/Kai Hendrik Behrends/Brian Teague
/////// 
///////	This software is provided 'as-is', without any express or implied
/////// warranty. In no event will the authors be held liable for any damages
/////// arising from the use of this software.
/////// 
/////// Permission is granted to anyone to use this software for any purpose,
/////// including commercial applications, and to alter it and redistribute it
/////// freely, subject to the following restrictions:
/////// 
/////// 1. The origin of this software must not be misrepresented; you must not
/////// claim that you wrote the original software. If you use this software
/////// in a product, an acknowledgment in the product documentation would be
/////// appreciated but is not required.
/////// 
/////// 2. Altered source versions must be plainly marked as such, and must not
/////// be misrepresented as being the original software.
/////// 
/////// 3. This notice may not be removed or altered from any source
/////// distribution.
///////
/////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <sys/io.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <err.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include <libudev.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <syslog.h>
#include <getopt.h>
#include <pwd.h>

extern const unsigned int pci_addr;
extern const unsigned int pci_data;

extern const size_t MAX_HDD_LEDS;

extern const unsigned int PCI_CONFIG_ADDRESS;
extern const unsigned int PCI_CONFIG_DATA;
extern size_t debug;
extern size_t HP;
extern size_t thread_run;
extern size_t update_monitor;

const char VERSION[] = "0.9.9";

extern int64_t retbytes(char* statfile, int field);
extern void setsystemled( int led_type, int state );
extern size_t init_hpex49x(void);
extern size_t init_acer_altos(void);
extern size_t init_h340(void);
extern size_t init_h341(void);
extern void set_hpex_led( int led_type, int state, size_t led, pthread_t thread_id ) ;
extern void set_acer_led( int led_type, size_t led_idx, int state, pthread_t thread_id );
extern void *update_monitor_thread(void *arg);
extern void setbrightness( int val );
extern size_t run_disk_tread(void);
size_t get_led_interface(void);
extern void sigterm_handler(int s);
const char* desc(void);
void drop_priviledges(void);
int show_help(char * progname );
int show_version(char * progname );
char* curdir(char *str);

const char *hardware;

/////////////////////////////////////////////////////////////////////////
// LED definitions
enum ledcolor {
	LED_BLUE	= 1 << 0,
	LED_RED		= 1 << 1,
};

enum ledstate {
	LED_OFF		= 1 << 0,
	LED_ON		= 1 << 1,
	LED_BLINK	= 1 << 2,
};

enum { 
	OFF = 0,
	ON = 1,
};
#endif //INCLUDED_HPEX49XLED