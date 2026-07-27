/*****************************File Header************************************/
/*
*  @file     logmsg.h
*  @brief    Define the functions for log message record (use rsyslog)
*  @details  Define the functions for log message record (use rsyslog)
*
*  @copyright CETCA Co,.ltd
*/
/****************************************************************************/

#ifndef _LOGMSG_H_
#define _LOGMSG_H_

#include "syslog.h"

#define RUN_INTERVAL_TIME       600U

#define SYSTEM_MOD              "SYSTEM"
#define HARDWARE_MOD            "HARDWARE"
#define STORAGE_MOD             "STORAGE"
#define MEMORY_MOD              "MEMORY"
#define BLUETOOTH_MOD           "BLUETOOTH"
#define NETWORK_MOD             "NETWORK"
#define BITE_MOD                "BITE"
#define SWITCH_MOD              "SWITCH"
#define EEPROM_MOD              "EEPROM"
#define GPIO_MOD                "GPIO"
#define LED_MOD                 "LED"
#define SENSOR_MOD              "SENSOR"
#define SERIAL_MOD              "SERIAL"
#define DISC_MOD                "DISCRETE"
#define MT_MOD                  "MT"
#define WDOG_MOD                "WATCHDOG"
#define SYSAPI_MOD              "SYSAPI"
#define COMMON_MOD              "COMMON"
#define APPL_MOD                "APPL"
#define STATUS_MOD              "STATUS"
#define SPI_MOD              "SPI"
#define A717_MOD              "A717"
#define A429_MOD              "A429"
#define RS422_MOD              "RS422"
#define DATADAEMON_MOD          "DATADAEMON"
#define COMMUNICATE_MOD          "COMMUNICATE"

#define LOG_EMERG               0
#define LOG_ALERT               1
#define LOG_CRIT                2
#define LOG_ERR                 3
#define LOG_WARNING             4
#define LOG_NOTICE              5
#define LOG_INFO                6
#define LOG_DEBUG               7

enum
{
	LOG_PRT_OFF = 0,
	LOG_PRT_ON,
};


#if 0
#define syslog_record(module, level, submodule, msg, ...) \
    {   \
        openlog(module, 0, LOG_LOCAL7); \
        char _log_[1024];   \
        sprintf(_log_, msg, ##__VA_ARGS__); \
        printf("%s\n", _log_); \
        syslog(level, "%s [%s(%s, %d)]", _log_, __FILE__, __func__, __LINE__);  \
        closelog(); \
    }
#else
void syslog_record(const char *module, int level, int prt_flg, const char *submodule, const char *msg, ...);
#endif

#define syslog_runstate(msg, ...) \
    {   \
        openlog(STATUS_MOD, 0, LOG_LOCAL5); \
        syslog(LOG_DEBUG, msg, ##__VA_ARGS__);  \
        closelog(); \
    }

#endif

