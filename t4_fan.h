#ifndef _T4_FAN_
#define _T4_FAN_

#include <stdint.h>
#include "logging.h"
#include "miner.h"
#include "util.h"

#include "mcompat_fan.h"


#define FAN_DETECT_MS           (3 * 1000)


extern mcompat_temp_s g_temp[];
extern mcompat_fan_temp_s g_fan_temp;
extern pthread_mutex_t g_temp_update_lock;
extern int g_temp_update_flag;


void mcompat_fan_auto_init(int speed);

void mcompat_fan_detect(void);



#endif

