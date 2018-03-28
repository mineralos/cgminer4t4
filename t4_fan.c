#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/ioctl.h>

#include "t4_fan.h"

im_temp_config_s s_temp_config;

im_temp_s g_temp[ASIC_CHAIN_NUM];
im_fan_temp_s g_fan_temp;
pthread_mutex_t g_temp_update_lock;
int g_temp_update_flag;

void im_fan_auto_init(int speed)
{
    s_temp_config.temp_hi_thr = 408;
    s_temp_config.temp_lo_thr = 652;
    s_temp_config.temp_start_thr = 564;
    s_temp_config.dangerous_stat_temp = 475;
    s_temp_config.work_temp = 505;
    s_temp_config.default_fan_speed = 20;

    g_fan_temp.im_temp = g_temp;
    im_fan_temp_init(0, s_temp_config);

    g_temp_update_flag = 0;
    mutex_init(&g_temp_update_lock);
}


static int s_error_num_cnt[6] = {0};
static uint32_t s_last_num[6] = {0};
static pthread_mutex_t fan_detect_lock;
void im_fan_detect_init(void)
{   
    mutex_init(&fan_detect_lock);
}

bool im_fan_speed_test(void)
{
    int i;
    int fd = 0;
    uint32_t num = 0;
    char buffer[32] = {0};
    char fan_reg[8] = {0};
    int32_t min_speed, speed;
    int min_cnt, cnt1, cnt2;

    min_speed = opt_fanspeed * ASIC_FAN_STEP_SPEED;
    if(min_speed == 0)
    {
        return true;
    }
    
    if(total_devices > 4)
    {
        min_cnt = 2;
    }
    else
    {
        min_cnt = 1;
    }
    
    mutex_lock(&fan_detect_lock);

    fd = open(ASIC_INNO_FAN_DECT0_DEVICE_NAME, O_RDWR);
    if(fd < 0)
    {
        applog(LOG_ERR, "open fandect fail");
        return;
    }   

    memset(buffer, 0, sizeof(buffer));
    if(read(fd, buffer, 24) < 0)
    {
       applog(LOG_ERR, "read fandect fail");
    }

    close(fd);

    mutex_unlock(&fan_detect_lock);
    
    cnt1 = cnt2 = 0;
    for(i = 0; i < 6; i++)
    {
        memcpy(fan_reg, buffer + (i * 4), 4);
        num = (fan_reg[3] << 24) + (fan_reg[2] << 16) + (fan_reg[1] << 8) + (fan_reg[0] << 0);
        
        if(num == 0)
        {
            cnt2++;
            applog(LOG_DEBUG, "the fan%d not inserted", i);
            continue;
        }
        
        if(num == s_last_num[i])
        {
            s_error_num_cnt[i]++;
        }
        else
        {
            s_error_num_cnt[i] = 0;
        }
        s_last_num[i] = num;
        
        speed = ZYNQ_MAIN_FUK / num;
        applog(LOG_DEBUG, "fan num=0x%x, speed=%d", num, speed);
        
        if(speed > min_speed)
        {
            cnt1++;
        }

        if(s_error_num_cnt[i] > 10)
        {
            cnt2++;
        }
    }

    if(cnt1 < min_cnt)
    {
        return false;
    }

    if(cnt2 > (6 - min_cnt))
    {
        return false;
    }

    return true;
}


static int s_last_fan_detect_time = 0;
void im_fan_detect(void)
{
    if(s_last_fan_detect_time + FAN_DETECT_MS < get_current_ms())
    {
        if(!im_fan_speed_test())
        {
            applog(LOG_WARNING, "fan error power down all chain");
            im_chain_power_down_all();
        }
        s_last_fan_detect_time = get_current_ms();
    }
}


