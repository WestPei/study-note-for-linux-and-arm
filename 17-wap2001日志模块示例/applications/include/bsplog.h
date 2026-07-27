/*****************************File Header************************************/
/**
*  @file     Bsplog.h
*  @brief    brief descriptions of the header file in a sentence
*  @details  detailed descriptions of the header file
*
*  @copyright CETCA Co,.ltd
*  @author   Yang Tao 
*  @date     2023-5-5 
*/
/****************************************************************************/


#ifndef __BSPLOG_H__
#define __BSPLOG_H__

#ifdef __cplusplus
extern "C" {
#endif


/******************************Include files*********************************/


/****************************************************************************/


/*********************************Macros*************************************/

#define LOG_PATH_BASE           "/mnt/log"
#define LOG_FOLDER              "bsplog"
#define LOG_PATH                LOG_PATH_BASE"/"LOG_FOLDER

#define BSP_RUNLOG_FILE         LOG_PATH"/bsp_runlog"
#define BSP_FAULTLOG_FILE       "/var/log/rsyslog/state/bsp_fault.log"

#define LOG_SAVE_MAX            10

#define LOG_MOD_SYSTEM          "SYSTEM"
#define LOG_MOD_HARDWARE        "HARDWARE"
#define LOG_MOD_STORAGE         "STORAGE"
#define LOG_MOD_GRAPHICS        "GRAPHICS"
#define LOG_MOD_NETWORK         "NETWORK"
#define LOG_MOD_RS485           "RS485"
#define LOG_MOD_RS422           "RS422"
#define LOG_MOD_ARINC429        "ARINC429"
#define LOG_MOD_ARINC485        "ARINC485"
#define LOG_MOD_CAN             "CAN"
#define LOG_MOD_EEPROM          "EEPROM"
#define LOG_MOD_SERIAL          "SERIAL"
#define LOG_MOD_DISC            "DISCRETE"
#define LOG_MOD_BLUETOOTH       "BLUETOOTH"
#define LOG_MOD_BITE            "BITE"

#define SIZE_128B               128
#define SIZE_256B               256
#define SIZE_512B               512
#define SIZE_1KB                1024
#define SIZE_2KB                2048
#define SIZE_4KB                4096
#define SIZE_8KB                8192

#define INCLUDE_JOURNAL_LOG

/******************************Global Types**********************************/

/* add global type definitions here */
typedef enum
{
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_MAX = LOG_LEVEL_ERROR,
} BSP_LOG_LEVEL;

typedef struct 
{
    uint8_t *name;
    uint8_t *data;
}log_data_t;

/****************************************************************************/


/************************Global Variable Declarations*************************/

/* add global variable declarations here */
#define CONSOLE_PRINT_LEVEL     LOG_LEVEL_WARN
#define WRITE_LOGFILE_LEVEL     LOG_LEVEL_INFO

/****************************************************************************/


/***********************Global Function Declarations*************************/

/* add Global func declarations here */
int32_t bsplog_init(void);
int32_t bsplog_record(int8_t *mod, int32_t level, int8_t *format, ...);
int32_t get_shell_output(uint8_t *cmd, uint8_t *buff, uint32_t buflen);
int32_t faultlog_record(uint8_t *fault, log_data_t *log_item, int32_t count);
int32_t add_alarm_log_entry(const char *message);

/****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
