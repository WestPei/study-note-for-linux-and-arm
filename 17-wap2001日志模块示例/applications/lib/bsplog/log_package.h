/******************************************************************************
** Copyright (C),    2018-2025, CETCA. Co., Ltd.
** Description:     
******************************************************************************/
#ifndef _LOG_PACKAGE_H_
#define _LOG_PACKAGE_H_

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/vfs.h>
#include <sys/time.h>
#include "typesDef.h"
#include "logmsg.h"

#define BUFSZ               256
#define DIR_MAXSIZE         128*1024    /*128 MB*/
#define ARCHIVE_DIR         "/var/log/log-archive"
#define MONITOR_CYCLE       10          /*10 seconds*/
#define JOURNAL_MAXSIZE     512*1024    /*512 MB*/
#define JOURNAL_PER_SIZE    8*1024      /*8 MB*/

enum ENUM_ARCHIVE
{
    ARCHIVE_NONE            = 0,
    ARCHIVE_OVERSIZE        = 1,
    ARCHIVE_FLIGHT_CHANGE   = 2,
};

typedef struct
{
    char curr_name[BUFSZ];
    char past_name[BUFSZ];
    int32_t change_flag;
} flight_info_t;

typedef struct
{
    const char name[BUFSZ];
    int32_t archive_flag;
} log_dir_info_t;

int log_package_init(void);

#endif// _LOG_PACKAGE_H_

