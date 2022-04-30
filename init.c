#define _GNU_SOURCE 
#include "init_led.h"
/////////////////////////////////////////////////////////////////////////////
/////// @file init.c
///////
/////// Daemon for controlling the LEDs on the HP MediaSmart Server EX47X
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

////////////////////////////////////////////////////////////////////
//// Return path information - setting up the struct hpled instance
char* retpath( char* parent, char *delim, int field )
{

    char *last_token = NULL;
    char *copy_parent = NULL;
    int token = 0;
    char *found = NULL;

	if( (copy_parent = (char *)calloc((strlen(parent) + 2), sizeof(char))) == NULL)
		err(1, "Unable to allocate copy_parnet for copy from char* parent in retpath() ");

	if( strcpy(copy_parent, parent) == NULL)
		err(1, "Unable to strcpy() parent into copy_parent in %s line %d", __FUNCTION__, __LINE__);

	if(delim == NULL)
		err(1, "Unknown or illegal delimiter in repath() in %s line %d", __FUNCTION__, __LINE__);

	last_token = strtok( copy_parent, delim);

	while( (last_token != NULL) && (token <= field) ){
		if( token == field) {
			// found = last_token;
			if( (found = (char *)calloc((strlen(last_token) + 2), sizeof(char))) == NULL)
				err(1, "Unable to allocate found for copy from char * token in retpath() ");
			if( strcpy(found, last_token) == NULL)
				err(1, "Unable to strcpy() last_token into found in %s() line %d ", __FUNCTION__, __LINE__);
			break;
        }
        last_token = strtok( NULL, delim );
        token++;
	}
	if(debug)
		printf("The value of retpath field %i is %s \n", token, found);

    free(copy_parent);
    copy_parent = NULL;    
    last_token = NULL;
	/* you must free() the return of this function */
    return found;
}
/////////////////////////////////////////////////////////////////////
///// New parse function - scan stat file in, return pointers to specified stats
size_t parse_stat(char* statfile, uint64_t* rd_ios, uint64_t* w_ios)
{
	FILE *stats;
	int in, w_ticks, in_flight, io_ticks, time_in_queue, discard_ticks, flush_ticks;
	u_int64_t rd_merges, rd_sectors, rd_ticks, w_merges, w_sectors;
	u_int64_t discard_ios, discard_merges, discard_sectors, flush_ios;

	if ((stats = fopen(statfile, "r")) == NULL)
		err(1, "Unable to open %s for reading in %s line %d", statfile, __FUNCTION__, __LINE__);

	in = fscanf(stats, "%lu %lu %lu %lu %lu %lu %lu %u %u %u %u %lu %lu %lu %u %lu %u",
				rd_ios, &rd_merges, &rd_sectors, &rd_ticks, w_ios, &w_merges, &w_sectors, &w_ticks, &in_flight,
				&io_ticks, &time_in_queue, &discard_ios, &discard_merges, &discard_sectors, &discard_ticks,
				&flush_ios, &flush_ticks);

	if (in < 17)
		err(1, "Values of stat file missing. Expected 17 but received %u in %s line %d", in, __FUNCTION__, __LINE__);

	fclose(stats);

	return ((*rd_ios) && (*w_ios) >= 0) ? 1 : 0;
}
/////////////////////////////////////////////////////////////////////
//// parse the statfile and get I/O read and I/O write
size_t retbytes(char* statfile, int field, uint64_t* operations)
{
    const char *delimiter_characters = " ";
    char buffer[ BUFFER_SIZE ];
    char *last_token;
    char *end;
    int token = 0;
    // uint64_t found = 0;

    FILE *input_file = fopen( statfile, "r" );

    if( input_file == NULL ) {
		err(1, "Unable to open statfile in %s line %d", __FUNCTION__, __LINE__);
    }
    else {

        while( (fgets(buffer, BUFFER_SIZE, input_file) != NULL)) {

            last_token = strtok( buffer, delimiter_characters );

            while( (last_token != NULL) && (token <= field) ){
                if( token == field) {
                        *operations = strtoul( last_token, &end, 10 );
					if(*end)
						err(1, "Unable to convert string to int64_t in %s line %d", __FUNCTION__, __LINE__);
					break;
                }
                last_token = strtok( NULL, delimiter_characters );
                token++;
            }

        }

        if( ferror(input_file) ){
            perror( "The following error occurred" );
	   		exit(1);
        }

        fclose( input_file );
    }

    return (*operations >= 0) ? 1 : 0;

}
/////////////////////////////////////////////////////////////
//// run disk monintoring thread for HP EX48x or EX49x
void* acer_thread_run (void *arg)
{
	struct hpled hpex49x = *(struct hpled *)arg;
	int led_state = 0;
	struct timespec tv = { .tv_sec = 0, .tv_nsec = BLINK_DELAY };
	struct timespec blink = { .tv_sec = 0, .tv_nsec = 7500000 };
	pthread_t thId = pthread_self();

	while(thread_run) {

		if ((pthread_spin_lock(&hpex49x_gpio_lock)) == EDEADLOCK)
		{

			thread_run = 0;
			pthread_spin_unlock(&hpex49x_gpio_lock); /* bailing out anyway - no need to check for the return here */
			syslog(LOG_NOTICE, "Deadlock condition in thread %ld function %s line %d", thId, __FUNCTION__, __LINE__);
			err(1, "Deadlock return from pthread_spin_lock from thread ID %ld in %s line %d", thId, __FUNCTION__, __LINE__);
		}
		if ((parse_stat(hpex49x.statfile, &hpex49x.n_rio, &hpex49x.n_wio)) == 0)
			err(1, "Bad return from parse_stat() in %s line %d", __FUNCTION__, __LINE__);

		if ((pthread_spin_unlock(&hpex49x_gpio_lock)) != 0)
			err(1, "invalid return from pthread_spin_unlock from thread ID %ld in %s line %d", thId, __FUNCTION__, __LINE__);

		if (debug)
			printf("the disk is: %li \n", hpex49x.hphdd);

		if ((hpex49x.rio != hpex49x.n_rio) && (hpex49x.wio != hpex49x.n_wio))
		{

			hpex49x.rio = hpex49x.n_rio;
			hpex49x.wio = hpex49x.n_wio;

			if (debug)
			{
				printf("Read I/O = %li Write I/O = %li \n", hpex49x.n_rio, hpex49x.n_wio);
				printf("HP HDD is: %li \n", hpex49x.hphdd);
				printf("Thread ID: %ld \n", thId);
			}
			/* the lights are on and we want them to blink. Turn them off, wait, turn them on, wait, rinse...repeat */
			if (led_state)
			{
				set_acer_led(LED_BLUE, OFF, hpex49x.blue, thId);
				set_acer_led(LED_RED, OFF, hpex49x.red, thId);

				nanosleep(&blink, NULL);
			}
			// void set_acer_led( int led_type, int state, size_t led )
			set_acer_led(LED_BLUE, ON, hpex49x.blue, thId);
			set_acer_led(LED_RED, OFF, hpex49x.red, thId);
			nanosleep(&blink, NULL);
			led_state = 1;
		}
		else if (hpex49x.rio != hpex49x.n_rio)
		{

			hpex49x.rio = hpex49x.n_rio;

			if (debug)
			{
				printf("Read I/O only and is: %li \n", hpex49x.n_rio);
				printf("HP HDD is: %li \n", hpex49x.hphdd);
				printf("Thread ID: %ld \n", thId);
			}
			if (led_state)
			{
				set_acer_led(LED_BLUE, OFF, hpex49x.blue, thId);
				set_acer_led(LED_RED, OFF, hpex49x.red, thId);

				nanosleep(&blink, NULL);
			}
			set_acer_led(LED_BLUE, ON, hpex49x.blue, thId);
			set_acer_led(LED_RED, ON, hpex49x.red, thId);
			nanosleep(&blink, NULL);
			led_state = 1;
		}
		else if (hpex49x.wio != hpex49x.n_wio)
		{

			hpex49x.wio = hpex49x.n_wio;

			if (debug)
			{
				printf("Write I/O only and is: %li \n", hpex49x.n_wio);
				printf("HP HDD is: %li \n", hpex49x.hphdd);
				printf("Thread ID: %ld \n", thId);
			}
			if (led_state)
			{
				set_acer_led(LED_BLUE, OFF, hpex49x.blue, thId);
				set_acer_led(LED_RED, OFF, hpex49x.red, thId);

				nanosleep(&blink, NULL);
			}
			set_acer_led(LED_BLUE, ON, hpex49x.blue, thId);
			set_acer_led(LED_RED, OFF, hpex49x.red, thId);
			nanosleep(&blink, NULL);
			led_state = 1;
		}
		else
		{
			nanosleep(&tv, NULL);

			if ((led_state != 0))
			{
				set_acer_led(LED_BLUE, OFF, hpex49x.blue, thId);
				set_acer_led(LED_RED, OFF, hpex49x.red, thId);
				led_state = 0;
			}
			continue;
		}
	}
	pthread_exit(NULL);   

}
/////////////////////////////////////////////////////////////
//// run disk monintoring thread for HP EX48x or EX49x
void* hpex49x_thread_run (void *arg)
{
	struct hpled hpex49x = *(struct hpled *)arg;
	int led_state = 0;
	struct timespec t_led = { .tv_sec = 0, .tv_nsec = LED_DELAY };
	struct timespec t_blink = { .tv_sec = 0, .tv_nsec = BLINK_DELAY };
	pthread_t thId = pthread_self();

	while(thread_run) {

		if ((pthread_spin_lock(&hpex49x_gpio_lock)) == EDEADLOCK)
		{

			thread_run = 0;
			pthread_spin_unlock(&hpex49x_gpio_lock); /* bailing out anyway - no need to check for the return here */
			syslog(LOG_NOTICE, "Deadlock condition in thread %ld function %s line %d", thId, __FUNCTION__, __LINE__);
			err(1, "Deadlock return from pthread_spin_lock from thread ID %ld in %s line %d", thId, __FUNCTION__, __LINE__);
		}
		if ((parse_stat(hpex49x.statfile, &hpex49x.n_rio, &hpex49x.n_wio)) == 0)
			err(1, "Bad return from parse_stat() in %s line %d", __FUNCTION__, __LINE__);

		if ((pthread_spin_unlock(&hpex49x_gpio_lock)) != 0)
			err(1, "invalid return from pthread_spin_unlock from thread ID %ld in %s line %d", thId, __FUNCTION__, __LINE__);

		if ((hpex49x.rio != hpex49x.n_rio) && (hpex49x.wio != hpex49x.n_wio))
		{

			hpex49x.rio = hpex49x.n_rio;
			hpex49x.wio = hpex49x.n_wio;

			if (debug)
			{
				printf("Read I/O = %li Write I/O = %li \n", hpex49x.n_rio, hpex49x.n_wio);
				printf("HP HDD is: %li \n", hpex49x.hphdd);
				printf("Thread ID: %ld \n", thId);
			}
			/* the lights are on and we want them to blink. Turn them off, wait, turn them on, wait, rinse...repeat */
			if (led_state)
			{
				set_hpex_led(LED_BLUE, OFF, hpex49x.blue, thId);
				set_hpex_led(LED_RED, OFF, hpex49x.red, thId);

				nanosleep(&t_blink, NULL);
			}
			set_hpex_led(LED_BLUE, ON, hpex49x.blue, thId);
			set_hpex_led(LED_RED, OFF, hpex49x.red, thId);
			led_state = 1;
			nanosleep(&t_blink, NULL);
		}
		else if (hpex49x.rio != hpex49x.n_rio)
		{

			hpex49x.rio = hpex49x.n_rio;

			if (debug)
			{
				printf("Read I/O only and is: %li \n", hpex49x.n_rio);
				printf("HP HDD is: %li \n", hpex49x.hphdd);
				printf("Thread ID: %ld \n", thId);
			}
			if (led_state)
			{
				set_hpex_led(LED_BLUE, OFF, hpex49x.blue, thId);
				set_hpex_led(LED_RED, OFF, hpex49x.red, thId);

				nanosleep(&t_blink, NULL);
			}
			set_hpex_led(LED_BLUE, ON, hpex49x.blue, thId);
			set_hpex_led(LED_RED, ON, hpex49x.red, thId);
			led_state = 1;			
			nanosleep(&t_blink, NULL);
		}
		else if (hpex49x.wio != hpex49x.n_wio)
		{

			hpex49x.wio = hpex49x.n_wio;

			if (debug)
			{
				printf("Write I/O only and is: %li \n", hpex49x.n_wio);
				printf("HP HDD is: %li \n", hpex49x.hphdd);
				printf("Thread ID: %ld \n", thId);
			}
			if (led_state)
			{
				set_hpex_led(LED_BLUE, OFF, hpex49x.blue, thId);
				set_hpex_led(LED_RED, OFF, hpex49x.red, thId);

				nanosleep(&t_blink, NULL);
			}
			set_hpex_led(LED_BLUE, ON, hpex49x.blue, thId);
			set_hpex_led(LED_RED, OFF, hpex49x.red, thId);
			led_state = 1;			
			nanosleep(&t_blink, NULL);
		}
		else
		{
			/* turn off the active light */
			nanosleep(&t_led, NULL);
			if (led_state != 0)
			{
				set_hpex_led(LED_BLUE, OFF, hpex49x.blue, thId);
				set_hpex_led(LED_RED, OFF, hpex49x.red, thId);
				led_state = 0;
			}
			continue;
		}
	}
	pthread_exit(NULL);   

}
//////////////////////////////////////////////////////////////////
//// Thread runner to determine HP or H340/341/342/Altos
size_t run_disk_tread(void)
{
	
	size_t i = 0;

	if( (pthread_spin_init(&hpex49x_gpio_lock, PTHREAD_PROCESS_PRIVATE)) !=0 )
		err(1,"Unable to initialize first spin_lock in %s at %d", __FUNCTION__, __LINE__);

	if( (pthread_spin_init(&hpex49x_gpio_lock2, PTHREAD_PROCESS_PRIVATE)) !=0 )
		err(1,"Unable to initialize second spin_lock in %s at %d", __FUNCTION__, __LINE__);

	if ((pthread_attr_init(&attr)) < 0 )
		err(1, "Unable to execute pthread_attr_init(&attr) in main()");

	thread_run = 1;

	if(HP) {
		for(i = 0; i < *hpdisks; i++) {
        	if ( (pthread_create(&hpexled_led[i], &attr, &hpex49x_thread_run, &hpex49x[i])) != 0)
				err(1, "Unable to create thread for hpex47x_thread_run");
        	++num_threads;
        	if(debug)
				printf("HP HDD is %li - created thread %li \n", hpex49x[i].hphdd, num_threads);
    	}
		
	}
	else {
		for(i = 0; i < *hpdisks; i++) {
        	if ( (pthread_create(&hpexled_led[i], &attr, &acer_thread_run, &hpex49x[i])) != 0)
				err(1, "Unable to create thread for hpex47x_thread_run");
        	++num_threads;
        	if(debug)
				printf("HP HDD is %li - created thread %li \n", hpex49x[i].hphdd, num_threads);
    	}
		
		
	}
	if(update_monitor){
		if(pthread_create(&updatemonitor, &attr, &update_monitor_thread, NULL) != 0)
			err(1, "Unable to create thread for update monitor");
	}

	syslog(LOG_NOTICE,"Initialized monitor threads. Monitoring with %li threads", num_threads);
	syslog(LOG_NOTICE,"Initialized. Now monitoring for drive activity - Enjoy the light show!");
		

	for(i = 0; i < *hpdisks; i++) {
        if ( (pthread_join(hpexled_led[i], NULL)) != 0)
			err(1, "Unable to join threads - pthread_join in %s before close", __FUNCTION__);
    }
	if(update_monitor){
		if( (pthread_cancel(updatemonitor)) != 0)
			err(1, "Unable to cancel update monitor thread");
		if( (pthread_join(updatemonitor, NULL)) != 0)
			err(1, "Unable to join thread update_monitor_thread in %s before close", __FUNCTION__);
	}

	return i;
}
///////////////////////////////////////////////////////////////////
//// Initialize the disks - assumption here. Unable to test with anything other than HP. Need to validate process.
void* disk_init(void *arg) 
{

	struct udev *udev = NULL;
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *devices = NULL;
	struct udev_list_entry *dev_list_entry = NULL; 
	struct udev_device *dev = NULL;
	char *statpath = NULL;
	char *ppath = NULL;
	int numdisks = 0;

	udev = udev_new();

	if (!udev)
		err(1, "Unable to create struct udev in %s line %d ", __FUNCTION__, __LINE__);
	
	enumerate = udev_enumerate_new(udev); 
	if(!enumerate)
		err(1, "Unable to get struct udev_enumerate in %s line %d", __FUNCTION__, __LINE__);

	udev_enumerate_add_match_subsystem(enumerate, "block");
	udev_enumerate_add_match_property(enumerate, "ID_BUS", "ata");
	udev_enumerate_scan_devices(enumerate);

	devices = udev_enumerate_get_list_entry(enumerate);
	if(!devices)
		err(1, "Unable to get struct udev_list_entry in %s line %d", __FUNCTION__, __LINE__);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;
		char *host_bus = NULL;
		char *find_ATA = NULL;
		char *check_ATA = NULL;

		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev,path);

		if( !dev )
			err(1, "Unable to retrieve dev from udev_device_new_from_syspath in %s line %d", __FUNCTION__, __LINE__);

		if(strncmp(udev_device_get_devtype(dev), "partition", 9) == 0 || strncmp(udev_device_get_sysname(dev), "loop", 4) == 0 ) {
			dev = udev_device_unref(dev);
			continue;
		}
		if( (statpath = (char *)calloc(128, sizeof(char))) == NULL)
			err(1, "Unable to allocate statfile for copy from udev_list_entry_get_name() in %s line %d", __FUNCTION__, __LINE__);

		if( !(strcpy(statpath, path))) 
			err(1, "Error in return of strcpy() while copying path to statfile in %s line %d ", __FUNCTION__, __LINE__);

		if(! (check_ATA = retpath(statpath, "/", 4)))
			err(1, "NULL return from retpath in %s line %d", __FUNCTION__, __LINE__);

		/* We only want these host busses as they are associated with the bays ata2, ata3, ata4, ata5 */
		if( strncmp(check_ATA, "ata0", 4) == 0 || strncmp(check_ATA, "ata1", 4) == 0) {
			dev = udev_device_unref(dev);
			continue;
		}

		if(debug)
			printf("Device host bus is: %s \n", check_ATA);
		
		if( !(strcat(statpath, "/stat"))) 
			err(1, "Unable to concatinate /stat to path in %s line %d", __FUNCTION__, __LINE__);

		if( debug ) {
			printf("Device Node (/dev) path: %s\n", udev_device_get_devnode(dev));
			printf("Device sys path is: %s \n", path);
			printf("Device stat file is at: %s \n", statpath);
			printf("Device type is: %s \n", udev_device_get_devtype(dev));
		}
		/* reset to get parent device so we can scrape the last section and determine which bay is in use. */
		dev = udev_device_get_parent(dev);
		if(!dev)
			err(1, "Unable to find parent path of scsi device in %s line %d", __FUNCTION__, __LINE__);

		if( (ppath = (char *)calloc( 128 , sizeof(char)) ) == NULL )
			err(1, "Unable to allocate statfile for copy from udev_list_entry_get_name() in %s line %d", __FUNCTION__, __LINE__);

		if( !(strcpy(ppath, udev_device_get_devpath(dev)))) 
			err(1, "Error in return from strcpy() when copying into ppath in %s line %d", __FUNCTION__, __LINE__);

		if(debug)
			printf("Device parent path is: %s \n", ppath);
		
		if( !(host_bus = retpath(ppath,"/",4)))
			err(1, "NULL return from retpath in %s", __FUNCTION__);
		if(debug)
			printf("The host controller is : %s \n", host_bus);	
		if( !(find_ATA = retpath(statpath,"/",4)))
			err(1, "NULL return from retpath in %s line %d for find_ATA", __FUNCTION__, __LINE__);
		/* since the HPEX49x only has 4 bays and they are located on ata2 through ata5 - allocate them like this. */
		/* I do not know if this is the same for ACER h340-342/Altos */
		if( (strcmp("ata2",find_ATA) )== 0 && (strcmp("host1", host_bus)) == 0 ) {

			if( (ide0.statfile = (char *)calloc(128, sizeof(char))) == NULL)
				err(1, "Unable to allocate statfile for copy from udev_list_entry_get_name() in %s line %d", __FUNCTION__, __LINE__);
			if( strcpy(ide0.statfile, statpath) == NULL) 
				err(1, "Unable to strcpy() statpath into ide0.statpath in %s line %d", __FUNCTION__, __LINE__);
			if(debug)
				printf("ide0.statfile is: %s \n", ide0.statfile);
			ide0.hphdd = 1;
			if( (parse_stat(ide0.statfile, &ide0.rio, &ide0.wio)) == 0)
			       err(1, "Error on return from parse_stat() in %s line %d ", __FUNCTION__, __LINE__);
			ide0.n_rio = 0;
			ide0.n_wio = 0;	
			hpex49x[numdisks] = ide0;
			syslog(LOG_NOTICE,"Adding %s Disk 1 to monitor pool.", desc());
			syslog(LOG_NOTICE,"Statfile path for Disk 1 is %s",hpex49x[numdisks].statfile);

			if(debug)
				printf("Found HDD1 \n");
		}
		else if( (strcmp("ata3",find_ATA)) == 0 && (strcmp("host2", host_bus)) == 0) {
			if( (ide1.statfile = (char *)calloc(128, sizeof(char))) == NULL)
				err(1, "Unable to allocate statfile for copy from udev_list_entry_get_name() in %s line %d", __FUNCTION__, __LINE__);
			if( strcpy(ide1.statfile, statpath) == NULL) 
				err(1, "Unable to strcpy() statpath into ide1.statpath in %s line %d", __FUNCTION__, __LINE__);
			if(debug)
				printf("ide1.statfile is: %s \n", ide1.statfile);
			ide1.hphdd = 2;
			if( (parse_stat(ide1.statfile, &ide1.rio, &ide1.wio)) == 0)
			       err(1, "Error on return from parse_stat() in %s line %d ", __FUNCTION__, __LINE__);
			ide1.n_rio = 0;
			ide1.n_wio = 0;	
			hpex49x[numdisks] = ide1;
			syslog(LOG_NOTICE,"Adding %s Disk 2 to monitor pool.", desc());
			syslog(LOG_NOTICE,"Statfile path for Disk 2 is %s",hpex49x[numdisks].statfile);

			if(debug)
				printf("Found HDD2 \n");
		}
		else if( (strcmp("ata4",find_ATA)) == 0 && (strcmp("host3", host_bus)) == 0) {
			if( (ide2.statfile = (char *)calloc(128, sizeof(char))) == NULL)
				err(1, "Unable to allocate statfile for copy from udev_list_entry_get_name() in %s line %d ", __FUNCTION__, __LINE__);
			if( strcpy(ide2.statfile, statpath) == NULL) 
				err(1, "Unable to strcpy() statpath into ide2.statpath in %s line %d ", __FUNCTION__, __LINE__);
			if(debug)
				printf("ide2.statfile is: %s \n", ide2.statfile);
			ide2.hphdd = 3;
			if( (parse_stat(ide2.statfile, &ide2.rio, &ide2.wio)) == 0)
			       err(1, "Error on return from parse_stat() in %s line %d ", __FUNCTION__, __LINE__);
			ide2.n_rio = 0;
			ide2.n_wio = 0;
			hpex49x[numdisks] = ide2;
			syslog(LOG_NOTICE,"Adding %s Disk 3 to monitor pool.", desc());
			syslog(LOG_NOTICE,"Statfile path for Disk 3 is %s",hpex49x[numdisks].statfile);

			if(debug)
				printf("Found HDD3 \n");
		}
		else if( (strcmp("ata5",find_ATA)) == 0 && (strcmp("host4", host_bus)) == 0) {
			if( (ide3.statfile = (char *)calloc(128, sizeof(char))) == NULL)
				err(1, "Unable to allocate statfile for copy from udev_list_entry_get_name() in %s ", __FUNCTION__);
			if( strcpy(ide3.statfile, statpath) == NULL ) 
				err(1, "Unable to strcpy() statpath into ide3.statpath in %s line %d", __FUNCTION__, __LINE__);
			if(debug)
				printf("ide3.statfile is: %s \n", ide3.statfile);
			ide3.hphdd = 4;
			if( (parse_stat(ide3.statfile, &ide3.rio, &ide3.wio)) == 0)
			       err(1, "Error on return from parse_stat() in %s line %d ", __FUNCTION__, __LINE__);
			ide3.n_rio = 0;
			ide3.n_wio = 0;
			hpex49x[numdisks] = ide3;
			syslog(LOG_NOTICE,"Adding %s Disk 4 to monitor pool.", desc());
			syslog(LOG_NOTICE,"Statfile path for Disk 4 is %s",hpex49x[numdisks].statfile);

			if(debug)
				printf("Found HDD4 \n");
		}
		else 
			err(1,"Unknown host bus found during udev_device_get_devpath(dev)");

		free(statpath);
		statpath = NULL;
		free(ppath);
		ppath = NULL;
		dev = udev_device_unref(dev);
		dev = NULL;
		free(host_bus);
		host_bus = NULL;
		free(find_ATA);
		find_ATA = NULL;
		free(check_ATA);
		check_ATA = NULL;
		numdisks++;
	}
	/* Free the enumerator object */
	enumerate = udev_enumerate_unref(enumerate);

	udev = udev_unref(udev);

	if( statpath != NULL ) {
		free(statpath);
		statpath = NULL;
	}
	if( ppath != NULL ) {
		free(ppath);
		ppath = NULL;
	}
	int *answer = malloc(sizeof(*answer));
	*answer = numdisks;
	pthread_exit(answer);       
}

///////////////////////////////////////////////////////////
//// Initialize the SCH5127 Interface - where applicable
size_t initsch5127(const unsigned int vendor) 
{
	enum {
		IDX_LDN		= 0x07,	///< Logical Device Number
		IDX_ID		= 0x20,	///< device identification
		IDX_BASE_MSB	= 0x60,	///< base address MSB register
		IDX_BASE_LSB	= 0x61,	///< base address LSB register
		IDX_ENTER	= 0x55,	///< enter configuration mode
		IDX_EXIT	= 0xaa,	///< exit configuration mode
	};
		
	enum {
		CONF_VENDOR_ID	= 0x8000F800,   ///< Vendor Identification (enable, bus 0, device 31, function 0, register 0x00)
		CONF_GPIOBASE	= 0x8000F848,   ///< GPIO Base address     (enable, bus 0, device 31, function 0, register 0x48)
	};

	const unsigned int PCI_CONFIG_ADDRESS = 0x0CF8;
	const unsigned int PCI_CONFIG_DATA = 0x0CFC;

	if ( ioperm(PCI_CONFIG_DATA,    4, 1) )
		err(1, "Unable to set ioperm for PCI_CONFIG_DATA %#08X on line %d", PCI_CONFIG_DATA, __LINE__);
	if ( ioperm(PCI_CONFIG_ADDRESS, 4, 1) )
		err(1, "Unable to set ioperm for PCI_CONFIG_ADDRESS %#08X on line %d", PCI_CONFIG_ADDRESS, __LINE__);
		
	// retrieve vendor and device identification
	outl( CONF_VENDOR_ID, PCI_CONFIG_ADDRESS );
	const unsigned int did_vid = inl( PCI_CONFIG_DATA );
	
	if ( vendor != did_vid ) return 0;
		
	// retrieve GPIO Base Address
	outl( CONF_GPIOBASE, PCI_CONFIG_ADDRESS );
	gpiobase = inl( PCI_CONFIG_DATA );

	if (debug)
		printf("in %s gpiobase is: %#08X on line %d\n",__FUNCTION__, gpiobase, __LINE__);
		
	// sanity check the address
	// (only bits 15:6 provide an address while the rest are reserved as always being zero)
	if ( 0x1 != (gpiobase & 0xFFFF007F) ) {
		if ( debug ) 
			printf("%s : Expected 0x1 but got - %#08X on line %d \n",desc(), (gpiobase & 0xFFFF007F), __LINE__ );
		return 0;
	}
	gpiobase &= ~0x1; // remove hardwired 1 which indicates I/O space

	if (debug)
	 	printf("In %s gpiobase after 0x1 line %d application is: %#08x \n",__FUNCTION__, __LINE__, gpiobase);
		
	// finished with these ports
	ioperm( PCI_CONFIG_DATA,    4, 0 );
	ioperm( PCI_CONFIG_ADDRESS, 4, 0 );
	
	// try LPC SIO @ 0x2e
	unsigned int sio_addr = 0x2e;
	unsigned int sio_data = sio_addr + 1;
	if ( ioperm(sio_addr, 1, 1) ) 
		err(1, "Unable to set permissions for sio_addr %#08X on line %d", sio_addr, __LINE__);
	if ( ioperm(sio_data, 1, 1) )
		err(1, "Unable to set permissions for sio_data %#08X on line %d", sio_data, __LINE__);
		
	// enter configuration mode
	outb( IDX_ENTER, sio_addr );
		
	// retrieve identification
	outb( IDX_ID, sio_addr );
	const unsigned int device_id = inb( sio_data );
	if ( debug ) 
		printf("%s: Device 0x %#08X on line %d \n", desc(), device_id, __LINE__);
	{
		outb( 0x26, sio_addr );
		const unsigned int in = inb( sio_data );
		if( debug )
			printf("In %s in is 0x%#08X on line %d \n",__FUNCTION__, in, __LINE__);
		if ( 0x4e == in ) {
			outb( IDX_EXIT, sio_addr );
			
			// finished with these ports
			ioperm( sio_addr, 1, 0 );
			ioperm( sio_data, 1, 0 );
				
			// and switch to these if we are told to
			if ( debug ) 
				printf("%s: Using 0x4e\n", desc());
			sio_addr = 0x4e;
			sio_data = sio_addr + 1;
				
			if ( ioperm(sio_addr, 1, 1) )
				err(1, "Unable to set permissions for sio_addr %#08X on line %d", sio_addr, __LINE__);
			if ( ioperm(sio_data, 1, 1) )
				err(1, "Unable to set permissions for sio_data %#08X on line %d", sio_data, __LINE__);
				
			outb( IDX_ENTER, sio_addr );
		}
	}
	// select logical device 0x0a (base address?)
	outb( IDX_LDN, sio_addr );
	outb( 0x0a, sio_data );
		
	// get base address of runtime registers
	outb( IDX_BASE_MSB, sio_addr );
	const unsigned int index_msb = inb( sio_data );
	if( debug )
		printf("in %s and index_msb is: %#08X on line %d \n",__FUNCTION__, index_msb, __LINE__);
	outb( IDX_BASE_LSB, sio_addr );
	const unsigned int index_lsb = inb( sio_data );
	if( debug )
		printf("in int %s and index_lsb is: %#08X on line %d \n",__FUNCTION__, index_lsb, __LINE__);
	
	sch5127_regs = index_msb << 8 | index_lsb;

	if( debug ) 
		printf("in int %s and sch5127_regs is now: %#08X on line %d \n",__FUNCTION__, sch5127_regs, __LINE__);
		
	// exit configuration
	outb( IDX_EXIT, sio_addr );
		
	// finished with SuperI/O ports
	ioperm(sio_data, 1, 0);
	ioperm(sio_addr, 1, 0);
	
	// watchdog registers to zero out
	// These are in order from smallest to largest value eliminating the need for *std::min_element and *std::max_element
	const int WDT_REGS[] = { REG_WDT_TIME_OUT, REG_WDT_VAL, REG_WDT_CFG, REG_WDT_CTRL };
	// const int WDT_REGS[] = { REG_WDT_VAL, REG_WDT_TIME_OUT, REG_WDT_CFG, REG_WDT_CTRL };
	const size_t WDT_REGS_CNT = sizeof(WDT_REGS) / sizeof(WDT_REGS[0]);
		
	const int reg_min = WDT_REGS[0];
	const int reg_max = WDT_REGS[3];
	const int reg_cnt = ( reg_max - reg_min + 1 );
		
	// get access to the entire range
	if ( ioperm(sch5127_regs + reg_min, reg_cnt, 1) )
		err(1, "Unable to set ioperm line %d for sch5127_regs + reg_min %#08x",__LINE__, (sch5127_regs + reg_min));	
	// zero them out
	for ( size_t i = 0; i < WDT_REGS_CNT; ++i ) {
		outb( 0, sch5127_regs + WDT_REGS[i] );
	}
	
	// done
	ioperm(sch5127_regs + reg_min, reg_cnt, 0);

	if(debug)
		printf("In %s and disabled watchdog registers - about to return from call line %d \n", __FUNCTION__, __LINE__);

	return 1;
}

/////////////////////////////////////////////////////////////////////////
/// attempt to initialise HP EX48x and EX49x device LED
size_t init_hpex49x(void) 
{	
	int bits1 = 0, bits2 = 0;
	// ISA bridge [0601]: Intel Corporation 82801IR (ICH9R) LPC Interface Controller [8086:2916] (rev 02)
	const unsigned int HPEX49X = 0x29168086;
	// Thread IDs
	pthread_t tid;
	// Create Thread Attributes
	pthread_attr_t attr;
	const int IO_LEDS_BLUE[] = { OUT_BLUE0, OUT_BLUE1, OUT_BLUE2, OUT_BLUE3 };
	const int IO_LEDS_RED[] = { OUT_RED0, OUT_RED1, OUT_RED2, OUT_RED3 };

	out_system_blue = OUT_SYSTEM_BLUE;
	out_system_red = OUT_SYSTEM_RED;

	if ((pthread_attr_init(&attr)) < 0 )
		err(1, "Unable to execute pthread_attr_init(&attr) in %s line %d", __FUNCTION__, __LINE__);

	if( (pthread_create(&tid, &attr, disk_init, NULL)) != 0)
		err(1, "Unable to init disks in %s - bad return from disk_init() line %d", __FUNCTION__, __LINE__);

	if ( !initsch5127(HPEX49X) ) 
		return 0;

	// set up io permissions to other ports we may use
	if ( ioperm(sch5127_regs + REG_HWM_INDEX, 1, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_HWM_INDEX %#08X", __FUNCTION__, __LINE__, ( sch5127_regs + REG_HWM_INDEX) );
	if ( ioperm(sch5127_regs + REG_HWM_DATA,  1, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_HWM_DATA %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_HWM_DATA ) );
	if ( ioperm(gpiobase + GPO_BLINK,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GPO_BLINK %#08X",  __FUNCTION__, __LINE__, ( gpiobase + GPO_BLINK ) );
	if ( ioperm(gpiobase + GP_IO_SEL,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_IO_SEL %#08X", __FUNCTION__, __LINE__, ( gpiobase + GP_IO_SEL ) );
	if ( ioperm(gpiobase + GP_IO_SEL2,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_IO_SEL2 %#08X", __FUNCTION__, __LINE__,  ( gpiobase + GP_IO_SEL2 ) );
	if ( ioperm(gpiobase + GP_LVL,      4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_LVL %#08X",  __FUNCTION__, __LINE__, ( gpiobase + GP_LVL ) );
	if ( ioperm(gpiobase + GP_LVL2,		4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_LVL2 %#08X", __FUNCTION__, __LINE__, ( gpiobase + GP_LVL2 ) );

	/////////////////////////////////////////////////////////////////////////
	/// enable LEDs for HP
	for ( size_t i = 0; i < MAX_HDD_LEDS; ++i ) {
		setbits32( ioledblue( i ), &bits1, &bits2 );
		setbits32( ioledred( i ),  &bits1, &bits2 );
	}	

	setbits32( OUT_USB_DEVICE,  &bits1, &bits2 );
	setbits32( OUT_SYSTEM_BLUE, &bits1, &bits2 );
	setbits32( OUT_SYSTEM_RED,  &bits1, &bits2 );
	
	setgpioselinput( bits1, bits2 );

	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);
	if( (pthread_join(tid,(void**)&hpdisks)) != 0)
		err(1, "Unable to rejoin thread prior to execution in %s", __FUNCTION__);
	for(int i = 0; i < *hpdisks; i++){
		hpex49x[i].blue = IO_LEDS_BLUE[ ((hpex49x[i].hphdd) - 1) ];
		hpex49x[i].red = IO_LEDS_RED[ ((hpex49x[i].hphdd) - 1) ];
	}
	if(debug)
		printf("Number of disks is: %d \n", *hpdisks);

	return *hpdisks;
}
/////////////////////////////////////////////////////////////////////////////
//// initialize the Acer Altos
size_t init_acer_altos(void)
{
	// ISA bridge [0601]: Intel Corporation 82801IR (ICH9R) LPC Interface Controller [8086:2916] (rev 02)
	const unsigned int ALTOS = 0x27B88086;
	int bits1 = 0, bits2 = 0;
	pthread_t tid;
	pthread_attr_t attr;

	const int IO_LEDS_BLUE[] = { ALTOS_BLUE0, ALTOS_BLUE1, ALTOS_BLUE2, ALTOS_BLUE3 };
	const int IO_LEDS_RED[] = { ALTOS_RED0, ALTOS_RED1, ALTOS_RED2, ALTOS_RED3 };

	out_system_blue = ALTOS_SYSTEM_BLUE;
	out_system_red = ALTOS_SYSTEM_RED;

	if ((pthread_attr_init(&attr)) < 0 )
		err(1, "Unable to execute pthread_attr_init(&attr) in main()");

	if( (pthread_create(&tid, &attr, disk_init, NULL)) != 0)
		err(1, "Unable to init in main - bad return from disk_init() ");

	if ( !initsch5127(ALTOS) ) 
		return 0;

	// set up io permissions to other ports we may use
	if ( ioperm(sch5127_regs + REG_HWM_INDEX, 1, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_HWM_INDEX %#08X", __FUNCTION__, __LINE__, ( sch5127_regs + REG_HWM_INDEX) );
	if ( ioperm(sch5127_regs + REG_HWM_DATA,  1, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_HWM_DATA %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_HWM_DATA ) );
	if ( ioperm(sch5127_regs + REG_GP1,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP1 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP1 ) );
	if ( ioperm(sch5127_regs + REG_GP2,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP2 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP2 ) );
	if ( ioperm(sch5127_regs + REG_GP3,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP3 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP3 ) );
	if ( ioperm(sch5127_regs + REG_GP4,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP4 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP4 ) );
	if ( ioperm(sch5127_regs + REG_GP5,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP5 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP5 ) );
	if ( ioperm(sch5127_regs + REG_GP6,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP6 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP6 ) );
	//
	if ( ioperm(gpiobase + GPO_BLINK,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GPO_BLINK %#08X",  __FUNCTION__, __LINE__, ( gpiobase + GPO_BLINK ) );
	if ( ioperm(gpiobase + GP_IO_SEL,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_IO_SEL %#08X", __FUNCTION__, __LINE__, ( gpiobase + GP_IO_SEL ) );
	if ( ioperm(gpiobase + GP_IO_SEL2,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_IO_SEL2 %#08X", __FUNCTION__, __LINE__,  ( gpiobase + GP_IO_SEL2 ) );
	if ( ioperm(gpiobase + GP_LVL,		4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_LVL %#08X",  __FUNCTION__, __LINE__, ( gpiobase + GP_LVL ) );
	if ( ioperm(gpiobase + GP_LVL2,		4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_LVL2 %#08X", __FUNCTION__, __LINE__, ( gpiobase + GP_LVL2 ) );

	setbits32( ALTOS_USB_DEVICE, &bits1, &bits2 );
	setbits32( ALTOS_USB_LED,	 &bits1, &bits2 );
	setbits32( ALTOS_POWER,		 &bits1, &bits2 );
	setbits32( ALTOS_SYSTEM_BLUE,&bits1, &bits2 );
	setbits32( ALTOS_SYSTEM_RED, &bits1, &bits2 );
		
	setgpioselinput( bits1, bits2 );

	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);
	if( (pthread_join(tid,(void**)&hpdisks)) != 0)
		err(1, "Unable to rejoin thread prior to execution in %s", __FUNCTION__);
	
	for(int i = 0; i < *hpdisks; i++){
		hpex49x[i].blue = IO_LEDS_BLUE[ ((hpex49x[i].hphdd) - 1) ];
		hpex49x[i].red = IO_LEDS_RED[ ((hpex49x[i].hphdd) - 1) ];
	}

	if(debug)
		printf("Number of disks is: %d \n", *hpdisks);
	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);

	return *hpdisks;
}
//////////////////////////////////////////////////////////////////////////////
//// initialize the H340 
size_t init_h340(void)
{
	int bits1 = 0, bits2 = 0;
	// ISA bridge [0601]: Intel Corporation 82801IR (ICH9R) LPC Interface Controller [8086:2916] (rev 02)
	const unsigned int H340 = 0x27B88086;
	pthread_t tid;
	pthread_attr_t attr;

	const int IO_LEDS_BLUE[] = { H340_BLUE0, H340_BLUE1, H340_BLUE2, H340_BLUE3 };
	const int IO_LEDS_RED[] = { H340_RED0, H340_RED1, H340_RED2, H340_RED3 };

	out_system_blue = H340_SYSTEM_BLUE;
	out_system_red = H340_SYSTEM_RED;

	if ((pthread_attr_init(&attr)) < 0 )
		err(1, "Unable to execute pthread_attr_init(&attr) in main()");

	if( (pthread_create(&tid, &attr, disk_init, NULL)) != 0)
		err(1, "Unable to init in main - bad return from disk_init() ");

	if ( !initsch5127(H340) ) 
		return 0;

	// set up io permissions to other ports we may use
	if ( ioperm(sch5127_regs + REG_HWM_INDEX, 1, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_HWM_INDEX %#08X", __FUNCTION__, __LINE__, ( sch5127_regs + REG_HWM_INDEX) );
	if ( ioperm(sch5127_regs + REG_HWM_DATA,  1, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_HWM_DATA %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_HWM_DATA ) );
	if ( ioperm(sch5127_regs + REG_GP1,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP1 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP1 ) );
	if ( ioperm(sch5127_regs + REG_GP2,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP2 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP2 ) );
	if ( ioperm(sch5127_regs + REG_GP3,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP3 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP3 ) );
	if ( ioperm(sch5127_regs + REG_GP4,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP4 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP4 ) );
	if ( ioperm(sch5127_regs + REG_GP5,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP5 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP5 ) );
	if ( ioperm(sch5127_regs + REG_GP6,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP6 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP6 ) );
	//
	if ( ioperm(gpiobase + GPO_BLINK,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GPO_BLINK %#08X",  __FUNCTION__, __LINE__, ( gpiobase + GPO_BLINK ) );
	if ( ioperm(gpiobase + GP_IO_SEL,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_IO_SEL %#08X", __FUNCTION__, __LINE__, ( gpiobase + GP_IO_SEL ) );
	if ( ioperm(gpiobase + GP_IO_SEL2,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_IO_SEL2 %#08X", __FUNCTION__, __LINE__,  ( gpiobase + GP_IO_SEL2 ) );
	if ( ioperm(gpiobase + GP_LVL,		4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_LVL %#08X",  __FUNCTION__, __LINE__, ( gpiobase + GP_LVL ) );
	if ( ioperm(gpiobase + GP_LVL2,		4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_LVL2 %#08X", __FUNCTION__, __LINE__, ( gpiobase + GP_LVL2 ) );

	setbits32( H340_USB_DEVICE,	&bits1, &bits2 );
	setbits32( H340_USB_LED,	&bits1, &bits2 );
	setbits32( H340_POWER,		&bits1, &bits2 );
	setbits32( H340_SYSTEM_BLUE,&bits1, &bits2 );
	setbits32( H340_SYSTEM_RED,	&bits1, &bits2 );
		
	setgpioselinput( bits1, bits2 );

	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);
	if( (pthread_join(tid,(void**)&hpdisks)) != 0)
		err(1, "Unable to rejoin thread prior to execution in %s", __FUNCTION__);

	for(int i = 0; i < *hpdisks; i++){
		hpex49x[i].blue = IO_LEDS_BLUE[ ((hpex49x[i].hphdd) - 1) ];
		hpex49x[i].red = IO_LEDS_RED[ ((hpex49x[i].hphdd) - 1) ];
	}

	if(debug)
		printf("Number of disks is: %d \n", *hpdisks);
	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);

	return *hpdisks;
}
//////////////////////////////////////////////////////////////////////////////
//// initialize the H341/H342
size_t init_h341(void)
{
	int bits1 = 0, bits2 = 0;
	const unsigned int H341 = 0x29168086;
	pthread_t tid;
	pthread_attr_t attr;
	const int IO_LEDS_BLUE[] = { H341_BLUE0, H341_BLUE1, H341_BLUE2, H341_BLUE3 };
	const int IO_LEDS_RED[] = { H341_RED0, H341_RED1, H341_RED2, H341_RED3 };

	out_system_blue = H341_SYSTEM_BLUE;
	out_system_red = H341_SYSTEM_RED;

	if ((pthread_attr_init(&attr)) < 0 )
		err(1, "Unable to execute pthread_attr_init(&attr) in main()");

	if( (pthread_create(&tid, &attr, disk_init, NULL)) != 0)
		err(1, "Unable to init in main - bad return from disk_init() ");

	if ( !initsch5127(H341) ) 
		return 0;

		// set up io permissions to other ports we may use
	if ( ioperm(sch5127_regs + REG_HWM_INDEX, 1, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_HWM_INDEX %#08X", __FUNCTION__, __LINE__, ( sch5127_regs + REG_HWM_INDEX) );
	if ( ioperm(sch5127_regs + REG_HWM_DATA,  1, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_HWM_DATA %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_HWM_DATA ) );
	if ( ioperm(sch5127_regs + REG_GP1,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP1 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP1 ) );
	if ( ioperm(sch5127_regs + REG_GP2,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP2 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP2 ) );
	if ( ioperm(sch5127_regs + REG_GP3,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP3 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP3 ) );
	if ( ioperm(sch5127_regs + REG_GP4,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP4 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP4 ) );
	if ( ioperm(sch5127_regs + REG_GP5,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP5 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP5 ) );
	if ( ioperm(sch5127_regs + REG_GP6,       4, 1) )
		err(1, "Unable to set permissions in %s line %d for sch5127_regs + REG_GP6 %#08X",__FUNCTION__, __LINE__, ( sch5127_regs + REG_GP6 ) );
	//
	if ( ioperm(gpiobase + GPO_BLINK,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GPO_BLINK %#08X",  __FUNCTION__, __LINE__, ( gpiobase + GPO_BLINK ) );
	if ( ioperm(gpiobase + GP_IO_SEL,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_IO_SEL %#08X", __FUNCTION__, __LINE__, ( gpiobase + GP_IO_SEL ) );
	if ( ioperm(gpiobase + GP_IO_SEL2,	4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_IO_SEL2 %#08X", __FUNCTION__, __LINE__,  ( gpiobase + GP_IO_SEL2 ) );
	if ( ioperm(gpiobase + GP_LVL,		4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_LVL %#08X",  __FUNCTION__, __LINE__, ( gpiobase + GP_LVL ) );
	if ( ioperm(gpiobase + GP_LVL2,		4, 1) )
		err(1, "Unable to set permissions in %s line %d for gpiobase + GP_LVL2 %#08X", __FUNCTION__, __LINE__, ( gpiobase + GP_LVL2 ) );

	setbits32( H341_USB_DEVICE,	&bits1, &bits2 );
	setbits32( H341_USB_LED,	&bits1, &bits2 );
	setbits32( H341_POWER,		&bits1, &bits2 );
	setbits32( H341_SYSTEM_BLUE,&bits1, &bits2 );
	setbits32( H341_SYSTEM_RED,	&bits1, &bits2 );
		
	setgpioselinput( bits1, bits2 );

	if( (pthread_join(tid,(void**)&hpdisks)) != 0)
		err(1, "Unable to rejoin thread prior to execution in %s", __FUNCTION__);

	for(int i = 0; i < *hpdisks; i++){
		hpex49x[i].blue = IO_LEDS_BLUE[ ((hpex49x[i].hphdd) - 1) ];
		hpex49x[i].red = IO_LEDS_RED[ ((hpex49x[i].hphdd) - 1) ];
	}

	if(debug)
		printf("Number of disks is: %d \n", *hpdisks);
	if(debug)
		printf("In %s() line %d performed port initialization - about to return \n",__FUNCTION__, __LINE__);

	return *hpdisks;
}

void setbits32( int bit, int *bits1, int *bits2 ) 
{
	int *bits;
	bits = (bit < 32) ? bits1 : bits2;
	*bits |= 1 << bit;
}

void dobits( unsigned int bits, unsigned int port, int state ) 
{
	const unsigned int val = inl( port );
	const unsigned int new_val = ( state ) ? val | bits : val & ~bits;

	if ( val != new_val ) outl( new_val, port );
}

void setgplpllvl( int bit, int state ) 
{
	dobits( (1 << (bit % 32)), gpiobase + ((bit < 32) ? GP_LVL : GP_LVL2),state);
}

void setgpregslvl( int bit, int state ) 
{
	const int reg = ((bit >> 4) & 0xF) - 1;
	assert( reg >= 0 );
						
	dobits( (1 << (bit & 0xF)), sch5127_regs + REG_GP1 + reg, state );
}
////////////////////////////////////////////////////////
//// Set GPIO Select Input
void setgpioselinput( int bits1, int bits2 ) 
{
	// Use Select (0 = native function, 1 = GPIO)

	const unsigned int gpio_use_sel  = gpiobase + GPIO_USE_SEL;
	const unsigned int gpio_use_sel2 = gpiobase + GPIO_USE_SEL2;
	if ( ioperm(gpio_use_sel,  4, 1) )
		err(1, "Unable to set ioperm for gpio_use_sel in void %s() line %d",  __FUNCTION__, __LINE__ );
	if ( ioperm(gpio_use_sel2, 4, 1) )
		err(1, "Unable to set ioperm for gpio_use_sel2 in void %s() line %d",__FUNCTION__,  __LINE__ );
	
	outl( inl(gpio_use_sel)  | bits1, gpio_use_sel  );
	outl( inl(gpio_use_sel2) | bits2, gpio_use_sel2 );
	
	ioperm( gpio_use_sel, 4, 0 );
	ioperm( gpio_use_sel2, 4, 0 );

	// Input/Output select (0 = Output, 1 = Input)
	
	const unsigned int gp_io_sel  = gpiobase + GP_IO_SEL;
	const unsigned int gp_io_sel2 = gpiobase + GP_IO_SEL2;
	if ( ioperm(gp_io_sel,  4, 1) )
		err(1, "Unable to set ioperm for gpio_io_sel in void %s() line %d",__FUNCTION__, __LINE__ );
	if ( ioperm(gp_io_sel2, 4, 1) )
		err(1, "Unable to set ioperm for gpio_io_sel2 in void %s() line %d",__FUNCTION__, __LINE__ );
			
	outl( inl(gp_io_sel)  & ~bits1, gp_io_sel  );
	outl( inl(gp_io_sel2) & ~bits2, gp_io_sel2 );
			
	ioperm( gp_io_sel, 4, 0 );
	ioperm( gp_io_sel2, 4, 0 );

}
/////////////////////////////////////////////////////////////////////////
/// function to set the System LED (blinking light on front of HPEX4xx) pass blue, red 
/// or combine turning each on at once for purple
void setsystemled( int led_type, int state ) 
{
	const int on_off_state = ( LED_ON == state );
	if ( led_type & LED_BLUE ) setgplpllvl( out_system_blue, !on_off_state );
	if ( led_type & LED_RED  ) setgplpllvl( out_system_red,  !on_off_state );
	
	const int blink_state  = ( LED_BLINK == state );
	int val = 0;
	if ( led_type & LED_BLUE ) val |= 1 << out_system_blue;
	if ( led_type & LED_RED  ) val |= 1 << out_system_red;
	if ( val ) dobits( val, gpiobase + GPO_BLINK, blink_state );
}
/////////////////////////////////////////////////////////////////////////
/// set brightness level
/// @param val Brightness level from 0 to 9
void setbrightness( int val ) 
{
	/// SCH5127 Hardware monitoring register set
	static const unsigned int HWM_PWM3_DUTY_CYCLE = 0x32;	///< PWM3 Current Duty Cycle
    static const unsigned char LED_BRIGHTNESS[] = { 0x00, 0xbe, 0xc3, 0xcb, 0xd3, 0xdb, 0xe3, 0xeb, 0xf3, 0xff };
	val = fmax( 0, fmin( val, sizeof(LED_BRIGHTNESS) / sizeof(LED_BRIGHTNESS[0]) - 1 ) );
	outb( HWM_PWM3_DUTY_CYCLE, sch5127_regs + REG_HWM_INDEX );
	outb( LED_BRIGHTNESS[val], sch5127_regs + REG_HWM_DATA  );
}
/////////////////////////////////////////////////////////////////////////
/// function to set the LEDs - pass blue, red 
/// or combine turning each on at once for purple
/// gcc atomic built-in while(__sync_lock_test_and_set(&hpex49xgpio_lock2, 1));	and __sync_lock_release(&hpex49xgpio_lock2);
void set_hpex_led( int led_type, int state, size_t led, pthread_t thread_id ) 
{
	if( (pthread_spin_lock(&hpex49x_gpio_lock2)) != 0 )
		err(1,"Invalid return from pthread_spin_lock for thread %ld in %s line %d", thread_id, __FUNCTION__, __LINE__);
	if ( led_type & LED_BLUE ) setgplpllvl( led, !state );
	if ( led_type & LED_RED  ) setgplpllvl( led, !state );
	if( (pthread_spin_unlock(&hpex49x_gpio_lock2)) != 0)
		err(1, "Invalid return from pthread_spin_unlock for thread %ld in %s line %d", thread_id, __FUNCTION__, __LINE__);
}
/////////////////////////////////////////////////////////////////////////
/// control leds
/// @param led_type LED type to turn on/off LED_BLUE, LED_RED, LED_BLUE | LED_RED
/// @param led Which LED to turn on/off (0 -> 3)
/// @param state Whether we are turning LED on or off 
/// @param thread_id ID of the calling thread - for diagnostic purposes
void set_acer_led( int led_type, int state, size_t led, pthread_t thread_id ) 
{
	if( (pthread_spin_lock(&hpex49x_gpio_lock2)) == EDEADLOCK )
		err(1,"Invalid return from pthread_spin_lock for thread %ld in %s line %d", thread_id, __FUNCTION__, __LINE__);

	if ( led_type & LED_BLUE ) setgpregslvl( led, state );
	if ( led_type & LED_RED  ) setgpregslvl( led, state );

	if( (pthread_spin_unlock(&hpex49x_gpio_lock2)) != 0)
		err(1, "Invalid return from pthread_spin_unlock for thread %ld in %s line %d", thread_id, __FUNCTION__, __LINE__);
}

/////////////////////////////////////////////////////////////////////////
/// blue LED mappings for HP disks only - *UNUSED*
int ioledblue( size_t led_idx )
{
	const int IO_LEDS_BLUE[] = { OUT_BLUE0, OUT_BLUE1, OUT_BLUE2, OUT_BLUE3 };
	assert( led_idx < MAX_HDD_LEDS );
	return IO_LEDS_BLUE[ led_idx ];
}
/////////////////////////////////////////////////////////////////////////
/// red LED mappings for HP disks only - *UNUSED*
int ioledred( size_t led_idx )
{
	const int IO_LEDS_RED[] = { OUT_RED0, OUT_RED1, OUT_RED2, OUT_RED3 };
	assert( led_idx < MAX_HDD_LEDS );
	return IO_LEDS_RED[ led_idx ];
}
//////////////////////////////////////////////////////////////////////////
//// general signal handler
void sigterm_handler(int s)
{
	thread_run = 0;
	setsystemled( LED_RED, LED_OFF);
	setsystemled( LED_BLUE, LED_OFF);

	for(size_t i = 0; i < *hpdisks; i++){
		if( pthread_cancel(hpexled_led[i]) != 0)
			err(1, "Unable to cancel threads - pthread_cancel in %s", __FUNCTION__);
	}
	for(size_t i = 0; i < *hpdisks; i++) {
        if ( (pthread_join(hpexled_led[i], NULL)) != 0)
			err(1, "Unable to join threads - pthread_join in %s before close", __FUNCTION__);
    }
	if(HP) {
		for(size_t i = 0; i < MAX_HDD_LEDS; i++){
			set_hpex_led(LED_BLUE, i, OFF, 0);
			set_hpex_led(LED_RED, i , OFF, 0);
		}
	}
	else {
		for(size_t i = 0; i< MAX_HDD_LEDS; i++){
			set_acer_led(LED_BLUE, i, OFF, 0);
			set_acer_led(LED_RED, i, OFF, 0);
		}
	}
	if (hpdisks != NULL) {
		for(size_t a = 0; a < *hpdisks; a++) {
			free(hpex49x[a].statfile);
			hpex49x[a].statfile = NULL;		
		}
	}
	if( (pthread_spin_destroy(&hpex49x_gpio_lock)) != 0 )
		perror("pthread_spin_destroy lock 1");
	if( (pthread_spin_destroy(&hpex49x_gpio_lock2)) != 0 )
		perror("pthread_spin_destroy lock 2");

	if(update_monitor){
		if(update_thread_instance == 1){
			update_thread_instance = 0;
		if(pthread_cancel(updatemonitor) != 0)
			err(1, "Unable to cancel update monitor thread in %s", __FUNCTION__);
		if( (pthread_join(updatemonitor, NULL)) != 0)
			err(1, "Unable to join update_monitor_thread in %s before close", __FUNCTION__);
		}
	}
	pthread_attr_destroy(&attr);

	free((void *)hpdisks);
	hpdisks = NULL;
	ioperm(pci_data, 4, 0);
	ioperm(pci_addr, 4, 0);
	syslog(LOG_NOTICE,"Signal Received. Exiting");
	closelog();
	errx(0, "\nExiting From Signal Handler\n");

}
