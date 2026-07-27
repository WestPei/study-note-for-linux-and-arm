/******************************************************************************
** Copyright (C),    2018-2025, CETCA. Co., Ltd.
** Description:     
******************************************************************************/
#include <stdio.h>
#include "log_package.h"

flight_info_t flight_info = {"", "", 0};

log_dir_info_t logdir_info[] =
{
    {"/var/log/rsyslog/system",     ARCHIVE_NONE},
    {"/var/log/rsyslog/platform",   ARCHIVE_NONE},
    {"/var/log/rsyslog/state",      ARCHIVE_NONE},
    {"/var/log/journal",            ARCHIVE_NONE},
    {"/var/log/coredump",           ARCHIVE_NONE},
};

sem_t sem_monitor, sem_package;

static void get_system_time(char *sys_time)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *times = (localtime(&tv.tv_sec));
    strftime(sys_time, 27, "%Y%m%d_%I%M%S", times);
}

static void deldir()
{
    char  cmd[BUFSZ];
    char  buff[BUFSZ];
    DIR *dir;
    const char *path = "/var/log/journal";
    struct dirent *stdinfo;
    FILE *fp;
    dir = opendir(path);

    if (dir == NULL)
    {
        return;
    }
    
    while(1)
    {
        if ((stdinfo = readdir(dir)) == 0)
        {
            break;
        }

        if (strcmp(stdinfo->d_name, "..") == 0 || strcmp(stdinfo->d_name, ".") == 0)
        {
            continue;
        }        
        
        memset(cmd, 0, BUFSZ);
        memset(buff, 0, BUFSZ);
        
        sprintf(cmd, "fuser %s/%s/system.journal", path, stdinfo->d_name);
        if ((fp = popen(cmd, "r")) != NULL)
        {
            if ((fgets(buff, BUFSZ, fp)) == NULL)
            {
                memset(cmd, 0, BUFSZ);
                sprintf(cmd, "rm -rf %s/%s", path, stdinfo->d_name);            
                system(cmd);
                syslog_record(SYSTEM_MOD, LOG_INFO, LOG_PRT_ON, "Log", "%s", cmd);
            }
        }
        
        pclose(fp);
    }
    
    memset(cmd, 0, BUFSZ);
    sprintf(cmd, "journalctl --vacuum-size=%dK", JOURNAL_PER_SIZE);            
    system(cmd);
}

static int log_monitor_task(void)
{
    char  buff[BUFSZ];
    char  cmd[BUFSZ];
    char  dir[BUFSZ];
    uint32_t dir_size;
    FILE *fp;
    
    while (1)
    {
        uint16_t loop;
        for (loop = 0; loop < NELEMENTS(logdir_info); loop++)
        {
            sem_wait(&sem_monitor);
            if (flight_info.change_flag == 1)
            {
                syslog_record(SYSTEM_MOD, LOG_INFO, LOG_PRT_ON, "Log", "Flight change");
                logdir_info[loop].archive_flag = ARCHIVE_FLIGHT_CHANGE;
                sem_post(&sem_package);
            }
            else
            {
                memset(cmd, 0, BUFSZ);            
                sprintf(cmd, "du -d0 %s", logdir_info[loop].name);

                if ((fp = popen(cmd, "r")) == NULL)
                {
                    sem_post(&sem_monitor);
                    return -1;
                }

                memset(buff, 0, BUFSZ);
                if ((fgets(buff, BUFSZ, fp)) != NULL)
                {
                    sscanf(buff, "%d%s", &dir_size, dir);
                }
                pclose(fp);
                
                char *bname = strrchr(logdir_info[loop].name, '/') + 1;
                if (strcmp(bname, "journal") == 0)
                {
                    if (dir_size >= JOURNAL_MAXSIZE)
                    {
                        syslog_record(SYSTEM_MOD, LOG_INFO, LOG_PRT_ON, "Log", "%s check size over", dir);
                        logdir_info[loop].archive_flag = ARCHIVE_OVERSIZE;
                        sem_post(&sem_package);
                    }
                    else
                    {
                        sem_post(&sem_monitor);
                    }
                }
                else if (dir_size >= DIR_MAXSIZE)
                {
                    syslog_record(SYSTEM_MOD, LOG_INFO, LOG_PRT_ON, "Log", "%s check size over", dir);
                    logdir_info[loop].archive_flag = ARCHIVE_OVERSIZE;
                    sem_post(&sem_package);
                }
                else
                {
                    sem_post(&sem_monitor);
                }
            }
        }
        flight_info.change_flag = 0;
        sleep(MONITOR_CYCLE);
    }

    return 0;
}

static int log_package_task()
{    
    char  cmd[BUFSZ];
    char  time[BUFSZ];
    char  flight[BUFSZ];
    char  achive[BUFSZ];
    
    while (1)
    {
        uint16_t loop;
        sem_wait(&sem_package);

        for (loop = 0; loop < NELEMENTS(logdir_info); loop++)
        {
            if ( (logdir_info[loop].name != NULL) && (logdir_info[loop].archive_flag != ARCHIVE_NONE) )
            {
                memset(cmd, 0, BUFSZ);
                memset(time, 0, BUFSZ);
                get_system_time(time);

                //sprintf(cmd, "mkdir -p %s", ARCHIVE_DIR);            
                //system(cmd);
                mkdir(ARCHIVE_DIR, 0666);

                if (logdir_info[loop].archive_flag == ARCHIVE_FLIGHT_CHANGE)
                {
                    strcpy(flight, flight_info.past_name);
                }
                else
                {
                    strcpy(flight, flight_info.curr_name);
                }

                if (strlen(flight) == 0)
                {
                    strcpy(flight, "FLIGHT");
                }
                
                char *bname = strrchr(logdir_info[loop].name, '/') + 1;
                sprintf(cmd, "tar -czf %s/%s-%s-%s.tar.gz %s", ARCHIVE_DIR, flight, bname, time, logdir_info[loop].name);        
                syslog_record(SYSTEM_MOD, LOG_INFO, LOG_PRT_ON, "Log", "%s", cmd);
                system(cmd);

                if (strcmp(bname, "journal") == 0)
                {
                    deldir();
                }
                else if (strcmp(bname, "coredump") == 0)
                {
                    memset(cmd, 0, BUFSZ);            
                    sprintf(cmd, "rm -rf %s/*", logdir_info[loop].name);  
                    syslog_record(SYSTEM_MOD, LOG_INFO, LOG_PRT_ON, "Log", "%s", cmd);
                    system(cmd);
                }
                else
                {
                    memset(cmd, 0, BUFSZ);            
                    sprintf(cmd, "rm -f %s/*.log*.gz", logdir_info[loop].name);
                    syslog_record(SYSTEM_MOD, LOG_INFO, LOG_PRT_ON, "Log", "%s", cmd);
                    system(cmd);
                }
                logdir_info[loop].archive_flag = ARCHIVE_NONE;
            }
        }
        sem_post(&sem_monitor);
    }

    return 0;
}

int log_package_init(void)
{
    pthread_t pthread1;
    pthread_t pthread2;
    pthread_attr_t attr1;
    pthread_attr_t attr2;

    sem_init(&sem_monitor, 0, 1);
    sem_init(&sem_package, 0, 0);

    if (pthread_attr_init(&attr1))
    {
        syslog_record(SYSTEM_MOD, LOG_EMERG, LOG_PRT_ON, "INIT", "log checksize task init fail");
        return -1;
    }

    pthread_attr_setdetachstate(&attr1, PTHREAD_CREATE_DETACHED);
    pthread_attr_setinheritsched(&attr1, PTHREAD_EXPLICIT_SCHED);

    if (0 != pthread_create(&pthread1, &attr1, log_monitor_task, (void *)NULL))
    {
        syslog_record(SYSTEM_MOD, LOG_EMERG, LOG_PRT_ON, "INIT", "log checksize task create fail");
        return -1;
    }

    pthread_setname_np(pthread1, "log_monitor");

    if (pthread_attr_init(&attr2))
    {
        syslog_record(SYSTEM_MOD, LOG_EMERG, LOG_PRT_ON, "INIT", "log package task init fail");
        return -1;
    }

    pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_DETACHED);
    pthread_attr_setinheritsched(&attr2, PTHREAD_EXPLICIT_SCHED);

    if (0 != pthread_create(&pthread2, &attr2, log_package_task, (void *)NULL))
    {
        syslog_record(SYSTEM_MOD, LOG_EMERG, LOG_PRT_ON, "INIT", "log package task create fail");
        return -1;
    }

    pthread_setname_np(pthread2, "log_package");

    return 0;
}

