#define _GNU_SOURCE
#define _GNU_SOURCE
/////////////////////////////////////////////////////////////////////////////
/////// @file updatemonitor.c
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
#include "updatemonitor.h"

char* retfield( char* parent, char *delim, int field )
{

    char *last_token = NULL;
    char *copy_parent = NULL;
    int token = 0;
    char* found = NULL;

        if( (copy_parent = (char *)calloc((strlen(parent) + 2), sizeof(char))) == NULL)
                err(1, "Unable to allocate copy_parnet for copy from char* parent in %s ", __FUNCTION__);

        if( !(strcpy(copy_parent, parent)))
                err(1, "Unable to strcpy() parent into copy_parent in %s", __FUNCTION__);

        if(delim == NULL)
                err(1, "Unknown or illegal delimiter in %s", __FUNCTION__);

        last_token = strtok( copy_parent, delim);

        while( (last_token != NULL) && (token <= field) ){
                if( token == field) {
                        // found = last_token;
                        if( (found = (char *)calloc((strlen(last_token) + 2), sizeof(char))) == NULL)
                                err(1, "Unable to allocate found for copy from char * token in %s ", __FUNCTION__);
                        if( !(strcpy(found, last_token)))
                                err(1, "Unable to strcpy() token into found in %s ", __FUNCTION__);
                        break;
                }
                last_token = strtok( NULL, delim );
                token++;
        }
    free(copy_parent);
    copy_parent = NULL;    
    last_token = NULL;
    /* you must free the return of this function */
    return found;
}

int status_update(int *update_count, int *security_update_count)
{
        char *str_line = NULL;
        char *update = NULL;
        char *security = NULL;

        FILE* apt_check = popen("/usr/lib/update-notifier/apt-check 2>&1", "r");
        if (apt_check == NULL)
        {
                printf("apt-check does not exist or can't be read.\n");
                return 0;
        }
        char* line = NULL;
        size_t len = 0;
        int res = getline(&line, &len, apt_check);
        pclose(apt_check);
        if (res == -1 || len < 3)
        {
                printf("Couldn't read line. res = %d len = %ld  line = %s \n", res, len, line); 
                return 0;
        }
        if ( asprintf(&str_line, "%s", line) == -1){ 
                printf("unable to copy line into str_line in asprintf() \n");
                return 0;
        }
        
        update = retfield(str_line, ";", 0);
        security = retfield(str_line, ";", 1);
        *update_count = atoi(update);
        *security_update_count = atoi(security);
        
        if(debug) printf("\n\n\n\n\n\n\n\n\n\nUpdate Monitor : Updates: %d \nSecurity Updates: %d\nIn %s of Update Monitor Thread line %d \n\n\n\n\n\n\n", *update_count,*security_update_count, __FUNCTION__, __LINE__);

        if(update != NULL){
                free(update);
                update = NULL;
        }

        if(security != NULL){
                free(security);    
                security = NULL;  
        }
        if(str_line != NULL){
                free(str_line);
                str_line = NULL;
        }
        if(line != NULL){
                free(line);
                line = NULL;
        }
        return 1;
}

int reboot_required(void)
{
        struct stat buffer;
        const char *reboot = "/var/run/reboot-required";
        
        return ( (stat(reboot, &buffer)) );
}

void thread_cleanup_handler(void *arg)
{
        enum {
	LED_BLUE	= 1 << 0,
	LED_RED		= 1 << 1,
        };

        enum {
	LED_OFF		= 1 << 0,
	LED_ON		= 1 << 1,
	LED_BLINK	= 1 << 2,
        };

        update_thread_instance = 0;
        setsystemled( LED_RED, LED_OFF);
        setsystemled( LED_BLUE, LED_OFF);
        syslog(LOG_NOTICE,"Update Monitor Thread Cleaned Up and Ending");
        if(debug) printf("\n\n\nUpdate Monitor Thread Ending in %s line %d\n",__FUNCTION__, __LINE__);

}

void *update_monitor_thread(void *arg)
{
        enum {
	LED_BLUE	= 1 << 0,
	LED_RED		= 1 << 1,
        };

        enum {
	LED_OFF		= 1 << 0,
	LED_ON		= 1 << 1,
	LED_BLINK	= 1 << 2,
        };

        if(debug)
                printf("\n\nUpdate Monitor Thread Executing\n");

        update_thread_instance = 1;
        pthread_cleanup_push(thread_cleanup_handler, NULL);
        syslog(LOG_NOTICE,"Initialized. Now Monitoring for Updates with the Update Monitor Thread");
        
        while(update_thread_instance){
                if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL) != 0)
                        err(1, "Unable to set pthread_setcancelstate to disable in %s line %d", __FUNCTION__, __LINE__);
                int update_count = -1, security_update_count = -1;
                if( status_update(&update_count, &security_update_count) != 1 ) {

                        break;
                }
                if( (reboot_required()) == 0 ){
                        setsystemled(LED_RED, LED_ON);
                        setsystemled(LED_BLUE, LED_OFF);
                        syslog(LOG_NOTICE,"UPDATE MONITOR THREAD: System Reboot Required");
                }
                else if( security_update_count > 0){

                        setsystemled(LED_BLUE | LED_RED, LED_ON);
                        syslog(LOG_NOTICE,"UPDATE MONITOR THREAD: %d Security Updates Found", security_update_count);
                }
                else if( update_count > 0){
                       setsystemled(LED_BLUE, LED_ON);
                       setsystemled(LED_RED, LED_OFF);
                       syslog(LOG_NOTICE,"UPDATE MONITOR THREAD: %d Updates Found", update_count);
                }
                else {
                       setsystemled(LED_BLUE | LED_RED, LED_OFF);
                       // setsystemled(LED_RED, LED_OFF);

                }
                if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0)
                        err(1, "Unable to set pthread_setcancelstate to enable in %s line %d", __FUNCTION__, __LINE__);
                
                sleep(900);
                // cancellation point
                // sigset_t sigempty;
	        // sigemptyset( &sigempty );
	        // struct timespec timeout = { 900, 0 };
		// int res = pselect( 0, 0, 0, 0, &timeout, &sigempty );
                // if( res < 0 ){
                //         if( EINTR != errno ) err(1, "Exiting due to signal");
                //         break;
                // }

        }        
        pthread_cleanup_pop(1); //Remove handler and execute it.

        if(debug) printf("\n\n\nUpdate Monitor Thread Ending\n in %s line %d\n",__FUNCTION__, __LINE__);

        return NULL;
}