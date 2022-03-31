#define _GNU_SOURCE
/////////////////////////////////////////////////////////////////////////////
/////// @file hpex49xled.c
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
/////// This software is provided 'as-is', without any express or implied
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
/////// 
/////// Changelog
///////
/////// March 11, 2022
/////// - initial release
/////// - Most code taken from https://github.com/merelin/mediasmartserverd - written by Chris Byrne/Kai Hendrik Behrends/Brian Teague
/////// - udev code and others taken from examples around the net - sorry I don't recall all the sites I visited looking for answers - you do deserve the credit.
/////// - thread code curtesy of me with examples taken from the Internet to help me on my way.
/////// - each drive will flash based on IO activity derrived from /sys/ * /stat files. If you use a drive connected by eSATA - YMMV. Please provide
///////   the /sys/* information path so I can add it to the ide structures, ignore, or exclude them from detection. 
///////
///////
/////// March 11, 2022
/////// - added pthread_spin_lock and pthread_spin_unlock to resolve race issue
/////// - as a 'just in case' added pthread_spin_lock/unlock to the LED control function to give exclusive access during on/off writes.
/////// - ensure that the function that taking the return from retpath() calls free() when finished.
///////
/////// - March 15, 2022
/////// - Added updatemonitor.c to complete the conversion and prepare for FreeBSD port.
/////// - added second pthread_spin_lock/unlock - this way locking occurs independently based on critical section
///////

#include "hpex49xled.h"

const char* desc(void) 
{ 
	return hardware;	
}
const char *systemid(const char *subsystem, const char *sysname, const char *sysattr)
{

    struct udev *udev;
    struct udev_device *device;
    char *value;

    if( !( udev = udev_new()))
        err(1, "Cannot create udev context");

    device = udev_device_new_from_subsystem_sysname(udev, subsystem, sysname);
    if (asprintf(&value, "%s", udev_device_get_sysattr_value(device, sysattr)) == -1)
        errx(1, "asprintf");

    udev_device_unref(device);
    udev_unref(udev);
        
	return value;

}
char* curdir(char *str)
{
	char *cp = strrchr(str, '/');
	return cp ? cp+1 : str;
}
int show_help(char * progname ) 
{

	char *this = curdir(progname);
	printf("%s %s %s", "Usage: ", this,"\n");
	printf("-d, --debug 	Print Debug Messages\n");
	printf("-D, --daemon 	Detach and Run as a Daemon - do not use this in service setup \n");
	printf("-h, --help	Print This Message\n");
	printf("-u, --update	Run Update Monitor Thread \n");
	printf("-v, --version	Print Version Information\n");

       return 0;
}

int show_version(char * progname ) 
{
	char *this = curdir(progname);
        printf("%s %s %s %s %s %s %s",this,VERSION,"compiled on", __DATE__,"at", __TIME__ ,"\n") ;
        return 0;
}
void drop_priviledges( void ) 
{
	struct passwd* pw = getpwnam( "nobody" );
	if ( !pw ) return; /* huh? */
	if ( (setgid( pw->pw_gid )) && (setuid( pw->pw_uid )) != 0 )
		err(1, "Unable to set gid or uid to nobody");

	if(debug) {
		printf("Successfully dropped priviledges to %s \n",pw->pw_name);
		printf("We should now be safe to continue \n");
	}
}
/////////////////////////////////////////////////////////////////////////////
/// attempt to get an LED control interface
size_t get_led_interface(void) 
{
	const char *systemVendor = systemid("dmi", "id", "sys_vendor");
	const char *productName = systemid("dmi", "id", "product_name");
	const char *biosVersion = systemid("dmi", "id", "bios_version");
	
	if(debug) {
		printf("--- SystemVendor : %s \n", systemVendor);
		printf("--- ProductName  : %s \n", productName);
		printf("--- Bios Version : %s \n", biosVersion);
	}

	if (strcmp(systemVendor, "Acer") == 0) {
		
		if(debug) 
			printf("Recognized SystemVendor: %s \n", systemVendor);
		
		if (strcmp(productName, "Aspire easyStore H340") == 0) {
			// H340
		
			if(debug) 
				printf("Recognized ProductName: %s \n", productName);
			hardware = "Acer Altos easyStore M2";
			if (init_h340() ) 
				return 1;
		}
		if (strcmp(productName, "Altos easyStore M2") == 0) {
			// Altos M2
			if(debug) 
				printf("Recognized ProductName: %s \n", productName);

			if ( init_acer_altos() ) 
				return 1;
		}
		if (strcmp(productName, "Aspire easyStore H341") == 0 || strcmp(productName, "Aspire easyStore H342") == 0) {
			// H341 or H342
			if(debug) 
				printf("Recognized ProductName: %s \n", productName);
			hardware = "Acer Aspire easyStore H341/H342";
			if ( init_h341() ) 
				return 1;
		}
	}
	else if (strcmp(systemVendor, "LENOVO") == 0) {
		if(debug) 
			printf("Recognized SystemVendor: %s \n", systemVendor);
		if (strcmp(productName, "IdeaCentre D400 10023") == 0) {
			// IdeaCentre D400
			if(debug) 
				printf("Recognized ProductName: %s \n", productName);
			//Compatiple with H340.
			hardware = "Acer Aspire easyStore H340";
			if ( init_h340() ) 
				return 1;
		}
	}
	else if(strcmp(systemVendor, "HP") == 0 ) {
		if(debug) 
			printf("Recognized SystemVendor: %s \n", systemVendor);
		if(strcmp(productName, "MediaSmart Server") == 0) {
			if(debug) 
				printf("Recognized ProductName: %s \n", productName);
			if(strcmp("EX49x 1.00", biosVersion) == 0) // validate and let author know if different for your EX49x
				hardware = "HP MediaSmart Server 49X";
			else
				hardware = "HP Mediasmart Server 48X";
			HP = 1;
			if ( init_hpex49x() ) 
				return 1;
		}	
	}
	else {
		if(debug) {
			printf("Unrecognized SystemVendor: %s \n", systemVendor); 
		}
		return 0;
	}
	//free
	free((char*)systemVendor);
	free((char*)productName);
	return 0;
}

int main (int argc, char **argv) 
{
	int run_as_daemon = 0;

	if (geteuid() !=0 ) {
		printf("Try running as root to avoid Segfault and core dump \n");
		errx(1, "not running as root user");
	}
  	const struct option long_opts[] = {
        { "debug",          no_argument,       0, 'd' },
        { "daemon",         no_argument,       0, 'D' },
        { "help",           no_argument,       0, 'h' },
		{ "update",         no_argument,       0, 'u' },
        { "version",        no_argument,       0, 'v' },
        { 0, 0, 0, 0 },
    };

    // pass command line arguments
    while ( true ) {
        const int c = getopt_long( argc, argv, "dDhv?", long_opts, 0 );
        if ( -1 == c ) break;

        switch ( c ) {
			case 'D': // daemon
				run_as_daemon++;
				break;
            case 'd': // debug
                debug++;
                break;
            case 'h': // help!
                return show_help(argv[0]);
			case 'u': //update_monitor
				update_monitor++;
				break;
            case 'v': // our version
                return show_version(argv[0] );
            case '?': // no idea
                return show_help(argv[0] );
            default:
                printf("++++++....\n"); 
        }
    }
	openlog("hpex49xled:", LOG_CONS | LOG_PID, LOG_DAEMON );

	signal( SIGTERM, sigterm_handler);
    signal( SIGINT, sigterm_handler);
    signal( SIGQUIT, sigterm_handler);
    signal( SIGILL, sigterm_handler);

	if( !get_led_interface() )
		err(1, "Illegal or unknown return from get_led_interface() line %d of %s", __LINE__, __FUNCTION__);

	/* System LED function takes from enum { LED_OFF, LED_ON, LED_BLINK } in header */
	setsystemled( LED_RED, LED_OFF);
	setsystemled( LED_BLUE, LED_OFF);

	/* Try and drop root priviledges now that we have initialized */
	drop_priviledges();

	if ( run_as_daemon ) {
		if (daemon( 0, 0 ) > 0 )
			err(1, "Unable to daemonize :");
		syslog(LOG_NOTICE,"Forking to background, running in daemon mode");
	}
	/* all thread actions are handled here - including the update_monitor thread */
	if( run_disk_tread() ) 
		err(1, "Unknown return from thread runner line %d of %s", __LINE__, __FUNCTION__);
	
	/* we should never get here - signal handler should take care of cleanup */
	thread_run = 0;
	ioperm(pci_data, 4, 0);
	ioperm(pci_addr, 4, 0);
	return 0;
}
