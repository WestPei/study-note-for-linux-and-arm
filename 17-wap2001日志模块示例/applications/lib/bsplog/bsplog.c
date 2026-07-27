/*****************************File Header************************************/
/**
*  @file     BspLog.c
*  @brief    the source code of run log module
*  @details  detailed descriptions of the src file
*
*  @copyright CETCA Co,.ltd
*  @author   Yang Tao 
*  @date     2023-5-5 
*/
/****************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <semaphore.h>

#include "typesDef.h"
#include "bsplog.h"
#include "logmsg.h"

#define TIME_STRLEN         64
#define BUF_MAX_LEN         256
#define LOG_MSG_MAX_SIZE    512

#define FORMAT_TIME         "%Y-%m-%d %H:%M:%S"

static int32_t  console_print_level = CONSOLE_PRINT_LEVEL;
static int32_t  log_init_flag = 0;
static int8_t msg_buf[LOG_MSG_MAX_SIZE];

#define DATE_BACKUP_FILE   "/var/log/date_current"
#define SECONDS_IN_YEAR   31536000    


int32_t add_alarm_log_entry(const char *message);
int32_t rotate_alarm_log();
int count_lines_in_file(FILE *file);

#define MAX_ALARM_LINES 100
#define BATCH_ROTATE 30 
#define ALARM_LOG_FILE "/var/log/alarm_history"
#define ALARM_ROTATE_FILE "/var/log/alarm_rotate"

static int current_line_count = 0;

void get_time_string(int8_t *str, int32_t str_max_len)
{
    time_t t;
    struct tm *lt;

    time(&t);
    lt = localtime(&t);
    strftime(str, str_max_len, FORMAT_TIME, lt);
}

int32_t update_log_folders(void)
{
    DIR *dir;
    struct dirent *ent;
        
    int32_t log_max, log_min, log_num, tmp_num, item, first, cur_log_exist;
    int8_t name_cmd[BUF_MAX_LEN];
    
    if ( !(dir = opendir(LOG_PATH_BASE)) )
    {
        perror("update_log_folders, opendir");
        return ERROR;
    }
    
    log_min = 0;
    log_max = 0;
    log_num = 0;
    first = 0;
    cur_log_exist = 0;
    
    /* check bsplog folder number */
    while ( (ent = readdir(dir)) )
    {
        if(ent->d_type == DT_DIR) /* 匹配目录项 */
        {
            //printf("\n%s is directory\n", ent->d_name);

            if( (item = sscanf(ent->d_name, "%[^-]-%d", name_cmd, &tmp_num)) > 0 ) /* 解析目录名称 */
            {
                //printf("\td_name: %s, tmp_num: %d, item: %d\n", name_cmd, tmp_num, item);
                if( 0 == strcmp(name_cmd, LOG_FOLDER) ) /* 找到 bsplog 目录 */
                {
                    log_num++;
                    if(item < 2)
                    {
                        /* 如果不-* 编号，则目录唯一 */
                        cur_log_exist = 1;
                        //printf("\tthis is current bsplog!!\n");                     
                    }
                    else 
                    {
                        /* 如果带 -* 编号，则记录最大和最小的编号 */
                        if(!first)
                        {
                            log_min = log_max = tmp_num;
                            first = 1;
                        }                       
                        else if(tmp_num > log_max)
                        {
                            log_max = tmp_num;
                        }
                        else if(tmp_num < log_min)
                        {
                            log_min = tmp_num;
                        }
                    }
                }
                else
                {
                    //printf("\tthis is other folder!!\n");
                }
            }
        }
        else
        {
            //printf("\n%s is file\n", ent->d_name);          
        }
    }
    //printf("\ntotal %d bsplog folders, max index is %d, min index is %d.\n", log_num, log_max, log_min);
    closedir(dir);
    
    /* archive last bsplog folder */
    if(cur_log_exist)
    {
        snprintf(name_cmd, BUF_MAX_LEN, "mv %s %s-%d", LOG_PATH, LOG_PATH, ++log_max);
        //printf("run '%s'\n", name_cmd);
                
        FILE* fp; 
        if( (fp = popen(name_cmd, "r")) == NULL ) 
        {
            printf("%s: run '%s' fail!\n", __func__, name_cmd);
            return ERROR;
        }

        fread(name_cmd, 1, BUF_MAX_LEN-1, fp);    
        pclose(fp); 
    }
    
    /* only adopt no more than 10 latest log folders */
    while(log_num > LOG_SAVE_MAX)
    {
        snprintf(name_cmd, BUF_MAX_LEN, "%s-%d", LOG_PATH, log_min);
        if( access(name_cmd, F_OK) == 0 )
        {
            snprintf(name_cmd, BUF_MAX_LEN, "rm -rf %s-%d", LOG_PATH, log_min);
            //printf("run '%s'\n", name_cmd);
            
            FILE* fp; 
            if((fp = popen(name_cmd, "r")) == NULL) 
            {
                printf("%s: run '%s' fail!\n", __func__, name_cmd);
                return ERROR;
            }

            fread(name_cmd, 1, BUF_MAX_LEN-1, fp);    
            pclose(fp); 
        }
        
        log_num--;
        log_min++;
        
        if( (log_max - log_min) < LOG_SAVE_MAX )
        {
            break;
        }
    }
    
    /* create current bsplog folder */
    snprintf(name_cmd, BUF_MAX_LEN, "%s", LOG_PATH);
    if(mkdir(name_cmd, 0777) != 0)
    {
        perror("update_log_folders, mkdir");
        return ERROR;
    }

    return OK;
}

int32_t bsplog_record(int8_t *mod, int32_t level, int8_t *format, ...)
{
    va_list ap, cpy;
    int32_t msg_len, offset;
    FILE* log_fp;
    int8_t time[TIME_STRLEN] = {0};
    
    va_start(ap, format);       

    /* print log message to console */
    if(level >= CONSOLE_PRINT_LEVEL)
    {
        va_copy(cpy, ap);
        vprintf(format, cpy);
        va_end(cpy);
        printf("\n");
    }

    /* check level to decide if write to log file */
    if(level < WRITE_LOGFILE_LEVEL)
    {
        va_end(ap);
        return OK;
    }

    if(!log_init_flag)
    {
        printf("%s: bsplog not init!\n", __func__);
        va_end(ap);
        return ERROR;
    }

    get_time_string(time, TIME_STRLEN-1);
    /* 这里都是用的 MAX_SIZE-1，是因为为末尾的\0预留空间 */
    offset = snprintf(msg_buf, LOG_MSG_MAX_SIZE-1, "%s  %s<%d>: ", time, mod, level);
    
    vsnprintf(msg_buf+offset, LOG_MSG_MAX_SIZE-offset-1, format, ap);
    va_end(ap);

    /* save log message to file */
    log_fp = fopen(BSP_RUNLOG_FILE, "a+");
    if(NULL == log_fp) 
    {
        printf("%s: open %s fail!\n", __func__, BSP_RUNLOG_FILE);
        return ERROR;
    }

    fprintf(log_fp, "%s\n", msg_buf);

    fclose(log_fp);
    return OK;
}

int32_t bsplog_init(void)
{
    int8_t cmdstr[BUF_MAX_LEN];

    if(log_init_flag)
    {
        return OK;
    }

    if( update_log_folders() )
    {
        printf("%s: update_log_folders fail!\n", __func__);
        return ERROR;
    }

#ifdef  INCLUDE_JOURNAL_LOG
    snprintf(cmdstr, BUF_MAX_LEN, "mkdir -p %s/journal", LOG_PATH);
    system(cmdstr);

    if( access("/var/log", F_OK) != 0 )
    {
        system("rm -rf /var/log");
        system("mkdir -p /var/log");
    }

    snprintf(cmdstr, BUF_MAX_LEN, "ln -s %s/journal /var/log/journal", LOG_PATH);
    system(cmdstr);

    system("journalctl --flush");
#endif

    log_init_flag = 1;
    return OK;
}

int32_t get_shell_output(uint8_t *cmd, uint8_t *buff, uint32_t buflen)
{
    FILE* fp; 
    
    if( (fp = popen(cmd,"r")) == NULL ) 
    {
        return ERROR; 
    }

    memset(buff,0,buflen);
    fread(buff,1,buflen-1,fp);
    
    pclose(fp); 
    return OK; 
}

#define SYSLOG_BUF_SIZE         1024
#define MOD_NAME_LEN            64
#define LOG_MEM_KEY             2234

typedef struct {
    char module[MOD_NAME_LEN];
    int level;
    char submodule[MOD_NAME_LEN];
    char log[SYSLOG_BUF_SIZE];
    int repeat_times;
} log_info;

//static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t *log_sem = NULL;
static log_info *last_log = NULL;

void *get_log_shm(int shm_key, int shm_len)
{
    int shmid;
    void *shm;

    /* creat share memory */ 
    shmid = shmget(shm_key, shm_len, 0666|IPC_CREAT);
    if (shmid == ERROR)
    {  
        perror("shmget");
        return NULL;
    }  
    
    /* connect share memory to current process address space */
    shm = (void*)shmat(shmid, NULL, 0);  
    if ((int32_t)shm == ERROR)
    {  
        perror("shmat"); 
        return NULL;
    }
    
    return shm;
}

int syslog_init(void)
{
    last_log = get_log_shm( LOG_MEM_KEY, sizeof(log_info) );

    if(last_log != NULL)
    {
        memset(last_log, 0, sizeof(log_info));
        return OK;
    }
    else
    {
        return ERROR;
    }
}

void syslog_record(const char *module, int level, int prt_flg, const char *submodule, const char *msg, ...)
{
    char cur_log[SYSLOG_BUF_SIZE];
    va_list ap;
    int offset;

    va_start(ap, msg);     
    offset = snprintf(cur_log, SYSLOG_BUF_SIZE-1, " %s: ", module);
    vsnprintf(cur_log+offset, SYSLOG_BUF_SIZE-offset-1, msg, ap);
    va_end(ap);

    /* print log message to console */
    if(prt_flg)
    {
        printf("%s\n", cur_log);
    }

    if(NULL == log_sem)
    {
        log_sem = sem_open("log_mutex", O_RDWR | O_CREAT, 0664, 1);
    }

    sem_wait(log_sem);

    if(NULL == last_log)
    {
        last_log = get_log_shm( LOG_MEM_KEY, sizeof(log_info) );
        if(NULL == last_log)
        {
            syslog(level|LOG_LOCAL7 , "%s", cur_log);
            
            sem_post(log_sem);
            return;
        }
    }

    /* filter out duplicate logs */    
    if( strncmp(cur_log, last_log->log, SYSLOG_BUF_SIZE) ) /* 完全一样返回 0 */
    {    
        if(last_log->repeat_times)
        {
            syslog( (last_log->level)|LOG_LOCAL7, "......above log msg repeat %d times......", last_log->repeat_times );
        }
        
        syslog(level|LOG_LOCAL7, "%s", cur_log);
        
        strncpy(last_log->log, cur_log, SYSLOG_BUF_SIZE);
        last_log->level = level;
        last_log->repeat_times = 0;
    }
    else
    {
        last_log->repeat_times++; 
    }
    
    sem_post(log_sem);
}

static sem_t *faultlog_sem = NULL;

int32_t faultlog_record(uint8_t *fault, log_data_t *log_item, int32_t count)
{
    FILE* log_fp;
    int8_t time_str[TIME_STRLEN] = {0};
    int32_t i=0;

    if(NULL == faultlog_sem)
    {
        faultlog_sem = sem_open("faultlog_mutex", O_RDWR | O_CREAT, 0664, 1);
    }

    sem_wait(faultlog_sem);
    
    log_fp = fopen(BSP_FAULTLOG_FILE, "a+");
    if (log_fp == NULL) 
    {
        syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s] open %s fail!", __func__, BSP_FAULTLOG_FILE);
        sem_post(faultlog_sem);
        return ERROR;
    }

    get_time_string(time_str, TIME_STRLEN-1);
    fprintf(log_fp, "*********************************************************************************\n");
    fprintf(log_fp, "\n                 %s at %s                   \n", fault, time_str);
    fprintf(log_fp, "\n*********************************************************************************\n");

    for(i=0; i<count; i++)
    {
        fprintf(log_fp, "<<<<         %s          >>>>\n", log_item[i].name);
        fprintf(log_fp, "%s\n", log_item[i].data);
    }
    
    fclose(log_fp);
    sem_post(faultlog_sem);
    return OK;
}

uint32_t get_date_from_file(time_t *date)
{
    FILE* datefile_fp;
    char buff[128];
    int count=0;

    if((access(DATE_BACKUP_FILE,F_OK))==0)
    {
        datefile_fp=fopen(DATE_BACKUP_FILE,"r");
        if (datefile_fp==NULL) 
        {
            syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  can not read date file", __func__, 0);
            return -1;
        }

        count=fread(buff,1,sizeof(buff), datefile_fp);
        if(count>0)
        {
            sscanf(buff,"%ld",date);
        }
        fclose(datefile_fp);
    }
	
	return 0;
}

int32_t set_valid_date()
{
    uint32_t ret=0;
    time_t t_cur,t_file=0;
    uint8_t cmd[256]={0};

    time(&t_cur);
    ret=get_date_from_file(&t_file);
    if(ret)
    {
        syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  get date from file failed,use system date,time=%ld", __func__, t_cur);
        return -1;
    }  
    
     /*date can be only rollbacked one year*/
    if(t_cur+SECONDS_IN_YEAR > t_file)
    {
        syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  system current date is valid ,use system date,cur_time=%ld, file_time=%ld", __func__, t_cur,t_file);
    }
    else
    {
        sprintf(cmd, "date -s @%ld",t_file); 
        if(runShell(cmd)==OK)
        {
            syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  system current date is invalid ,use file date, cur_time=%ld, file_time=%ld", __func__, t_cur,t_file);
        }
        else
        {
            syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  set file date failed,use system date,time=%ld", __func__, t_cur);
            return -1;
        }      
    }
	
    return 0;
}

void* date_refresh_task(void* arg)
{
    time_t t_cur=0;
    FILE* datefile_fp;
    time_t t_file=0;

    get_date_from_file(&t_file);
    
    while(1)
    {
        time(&t_cur);
        /*date can be only rollbacked one year*/
        if(t_cur+SECONDS_IN_YEAR > t_file)
        {
            datefile_fp=fopen(DATE_BACKUP_FILE,"w");
            if (datefile_fp==NULL) 
            {
                syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  can not open date file", __func__, 0);
                return ERROR;
            }
            
            ftruncate((int)(long)(datefile_fp),0);
            fseek(datefile_fp,0,SEEK_SET);
            fprintf(datefile_fp,"%ld\n",t_cur);
            fflush(datefile_fp);
            fclose(datefile_fp);  
            t_file = t_cur;
        }

        sleep(60);
    }
}

/* date_refresh_task 用于更新备份时间，set_valid_date 避免时间复位 */

int32_t date_refresh_init(void)
{
    pthread_t date_pthread;
    pthread_attr_t date_attr;

    set_valid_date();

    if (pthread_attr_init(&date_attr))
    {
        syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  date refresh attr init fail.", __func__, 0);
        return ERROR;
    }

    pthread_attr_setdetachstate(&date_attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setinheritsched(&date_attr, PTHREAD_EXPLICIT_SCHED);

    if (0 != pthread_create(&date_pthread, &date_attr, date_refresh_task, (void *)NULL))
    {
        syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  date refresh pthread create fail.", __func__, 0);
        return ERROR;
    }

    pthread_setname_np(date_pthread, "date_refresh");
    
    return OK;
}

int32_t bsplog_archive(void)
{
    system("/home/root/log-manage.sh");
    return OK;
}

int32_t add_alarm_log_entry(const char *message) 
{
    
    FILE *file = NULL;

    if( access(ALARM_ROTATE_FILE, F_OK) == 0 )
    {
        if( access(ALARM_LOG_FILE, F_OK) != 0 )
        {
            if (rename(ALARM_ROTATE_FILE, ALARM_LOG_FILE) != 0) 
            {
                syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  move rotate file to alarm history log file[%s] failed", __func__,ALARM_LOG_FILE);
            }
        }
        else
        {
            if (remove(ALARM_ROTATE_FILE) != 0) 
            {
                syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  remove alarm rotate log file[%s] failed", __func__,ALARM_ROTATE_FILE);
            }
        }
    }

    file = fopen(ALARM_LOG_FILE, "a+");
    if (file == NULL) 
    {
        syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  can not alarm history log file[%s]", __func__,ALARM_LOG_FILE);
        return ERROR;
    }
    
    if (current_line_count == 0) 
    {
        current_line_count = count_lines_in_file(file);
    }
    
    fprintf(file, "%s\n",  message);
    current_line_count++;
    
    if (current_line_count >= MAX_ALARM_LINES + BATCH_ROTATE) 
    {
        fclose(file);
        rotate_alarm_log();
        return ERROR;
    }
    
    fclose(file);
    return OK;
}

int32_t rotate_alarm_log() 
{
    char buffer[LOG_MSG_MAX_SIZE];
    int i=0;
    int lines_to_skip=0;
    FILE *original = NULL;
    FILE *temp = NULL;
    
    original = fopen(ALARM_LOG_FILE, "r");
    if (original == NULL) 
    {
        syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  can not alarm history log file[%s]", __func__,ALARM_LOG_FILE);
        return ERROR;
    }

    temp = fopen(ALARM_ROTATE_FILE, "w");
    if (temp == NULL) 
    {
        syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  can not alarm rotate file[%s]", __func__,ALARM_LOG_FILE);
        fclose(original);
        return ERROR;
    }
    
    lines_to_skip = current_line_count - MAX_ALARM_LINES;
    
    /* 跳过 */
    for (i = 0; i < lines_to_skip; i++) 
    {
        if (fgets(buffer, sizeof(buffer), original) == NULL) 
        {
            break;
        }
    }
    
    /* 从 history 移到 alarm */
    while (fgets(buffer, sizeof(buffer), original) != NULL) 
    {
        fputs(buffer, temp);
    }
    
    fclose(original);
    fclose(temp);
    
    if (remove(ALARM_LOG_FILE) != 0) 
    {
        syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  remove alarm history log file[%s] failed", __func__,ALARM_LOG_FILE);
        return ERROR;
    }
    
    /* 将 alarm rename 为 hiostory，等价于保留 history 的最后 100 行*/
    if (rename(ALARM_ROTATE_FILE, ALARM_LOG_FILE) != 0) 
    {
       syslog_record(SYSTEM_MOD, LOG_ERR, LOG_PRT_ON, NULL, " [%s]  move rotate file to alarm history log file[%s] failed", __func__,ALARM_LOG_FILE);
       current_line_count=0;
       return ERROR;
    }
    
    current_line_count = MAX_ALARM_LINES;
    return OK;
}

int count_lines_in_file(FILE *file) 
{
    int count = 0;
    char buffer[LOG_MSG_MAX_SIZE];
    
    rewind(file); 
    
    while (fgets(buffer, sizeof(buffer), file) != NULL) 
    {
        count++;
    }
    
    rewind(file); 
    return count;
}

