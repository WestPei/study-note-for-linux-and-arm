/*****************************File Header************************************/                                   
/**                                                                           
*  @file     start.c                                                                                                   
*  @brief    to start bsp board applications
*  @details  this is first bsp excutable program after system startup.                                                                                                                                                                                                                                                
*  @copyright Copyright (c) 2019  CETCA, Inc. 
*  The copyright to the source herein is the property of CETCA. The source may 
*  be used and/or copied only with the written permission from CETCA or in 
*  accordance with the terms and conditions stipulated in the agreement/contract 
*  under which the programs have been supplied.                                               
*/                                                                                                                      
/****************************************************************************/

/******************************Include files*********************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/ioctl.h>
#include "typesDef.h"
#include "board_para.h"
#include "gpio.h"
#include "ledlight_api.h"
#include "watchdog_api.h"
#include "discrete_api.h"
#include "logmsg.h"
#include "common.h"
#include "config.h"
#include "mt.h"
#include "data_daemon.h"


/****************************************************************************/

/*********************************Macros*************************************/
#define APPL_DEBUG_MSG(fmt, a1, a2, a3, a4, a5, a6) \
    syslog_record(APPL_MOD, LOG_DEBUG, LOG_PRT_OFF, NULL, " [%s] "fmt, __func__, a1, a2, a3, a4, a5, a6)
#define APPL_INFO_MSG(fmt, a1, a2, a3, a4, a5, a6) \
    syslog_record(APPL_MOD, LOG_INFO, LOG_PRT_ON, NULL, " [%s] "fmt, __func__, a1, a2, a3, a4, a5, a6)
#define APPL_WARN_MSG(fmt, a1, a2, a3, a4, a5, a6) \
    syslog_record(APPL_MOD, LOG_WARNING, LOG_PRT_ON, NULL, " [%s] "fmt, __func__, a1, a2, a3, a4, a5, a6)
#define APPL_ERR_MSG(fmt, a1, a2, a3, a4, a5, a6) \
    syslog_record(APPL_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s] "fmt, __func__, a1, a2, a3, a4, a5, a6)

#define BUF_LEN         200

#define FILE_START_AGAIN_FLAG     "/tmp/proc_started"
#define RST_WIFI_CPU      (GPIO_3 + 23)   /* GPIO3_IO23 */
#define POWER_EN_AP_CPU      (GPIO_5 + 6)   /* GPIO5_IO06 */


/****************************************************************************/

/****************************variable definitions****************************/

static uint8_t mgt_ip[] = {172, 17, 1, 3};
static uint8_t maint_ip[] = {192, 168, 1, 66};

/****************************************************************************/
/*!      
*   @brief         main\n
*   @details       start bsp program \n
*   @param[in]     arg            no used\n
*
*   @return        void \n                                  
*/
/****************************************************************************/
int main(int argc, char *argv[])
{
    int32_t i, rtmp, ret = 0;
    uint32_t rval, ipaddr_disc_chn[] = {1};
    int8_t cfg_str[BUF_LEN];
    int8_t cmd[BUF_LEN]={0};
    FILE* fp;

    (void)syslog_init();

    create_daemon();

    APPL_INFO_MSG("=========================start ap gpio set.", 0, 0, 0, 0, 0, 0);
    gpio_open(RST_WIFI_CPU, GPIO_SET_OUTPUT, 1);
    usleep(10000);	
    gpio_set_value(RST_WIFI_CPU, 0);
    usleep(50000);	
    gpio_close(RST_WIFI_CPU);

    gpio_open(POWER_EN_AP_CPU, GPIO_SET_OUTPUT, 0);
    usleep(10000);	
    gpio_set_value(POWER_EN_AP_CPU, 1);
    usleep(50000);	
    gpio_close(POWER_EN_AP_CPU);
    APPL_INFO_MSG("=============================end ap gpio set.", 0, 0, 0, 0, 0, 0);

    ret |= date_refresh_init();

    if(access(FILE_START_AGAIN_FLAG,F_OK) !=0)
    {
        APPL_INFO_MSG("the program is the first time starting,archive the logs.", 0, 0, 0, 0, 0, 0);
        ret |= bsplog_archive();
        sprintf(cmd, "touch %s",FILE_START_AGAIN_FLAG); 
        runShell(cmd);
    }
    else
    {
        APPL_ERR_MSG("the program maybe restarted,skip to archive the logs", 0, 0, 0, 0, 0, 0);
    }
    
    
    ret |= watchdog_init();
    ret |= disc_init();
    ret |= ledlight_init();
    ret |= led_light_mode_config(GREEN_HALF_ONE_HZ_LIGHT);
    #ifdef OFP_VERSION
    ret |= sensors_init();
    ret |= eeprom_init();
    ret |=data_daemon_init();
    ret |= bite_init();
    ret |= mt_init();
    #endif
/*
    if( rtmp = gpio_open(AP_CTRL_KEEP1, GPIO_SET_OUTPUT, 1) )
    {
        APPL_ERR_MSG("gpio_open AP_CTRL_KEEP1 fail!", 0, 0, 0, 0, 0, 0);
    }
    ret |= rtmp;
*/
    for(i = 0; i < NELEMENTS(ipaddr_disc_chn); i++)
    {
        (void)get_disc_input(ipaddr_disc_chn[i], &rval);
        mgt_ip[3] += (uint8_t)( (!!rval) << i );
    }
    #ifdef OFP_VERSION
    APPL_INFO_MSG("set br0 ip addr to '%d.%d.%d.%d'.", maint_ip[0], maint_ip[1], maint_ip[2], maint_ip[3], 0, 0);
    #else
    APPL_INFO_MSG("set br0 ip addr to '%d.%d.%d.%d'.", mgt_ip[0], mgt_ip[1], mgt_ip[2], mgt_ip[3], 0, 0);
    #endif
    //ret |= set_eth_ipaddr("br0", mgt_ip);
    if( fp = fopen("/etc/systemd/network/20-br0.network", "w") )
    {
        fprintf(fp, "[Match]\n");
        fprintf(fp, "Name=br0\n");
        fprintf(fp, "\n");
        fprintf(fp, "[Network]\n");
        #ifdef OFP_VERSION
        fprintf(fp, "Address=%d.%d.%d.%d/24\n", maint_ip[0], maint_ip[1], maint_ip[2], maint_ip[3]);
        #else
        fprintf(fp, "Address=%d.%d.%d.%d/21\n", mgt_ip[0], mgt_ip[1], mgt_ip[2], mgt_ip[3]);
        #endif
        fclose(fp);

        chmod("/etc/systemd/network/20-br0.network", 0644);
    }
    else
    {
        ret = ERROR;
        APPL_ERR_MSG("set br0 ip addr failed!", 0, 0, 0, 0, 0, 0);
    }

    system("systemctl reload systemd-networkd.service");
    if(!ret)
    {
        APPL_INFO_MSG("start program launch successfully, ver:%s!", SW_VERSION, 0, 0, 0, 0, 0);
    }

    while(1)
    {
        sleep(60);
    }

	/* should never go here */
    return EXIT_SUCCESS;
}


