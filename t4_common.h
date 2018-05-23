#ifndef _T4_COMMON_
#define _T4_COMMON_

#include "t4_cmd.h"

#define MAX_TEMP    652
#define MIN_TEMP    408

#define MUL_COEF    1.23


#define ASIC_CHAIN_NUM                  8
#define ASIC_CHIP_NUM                   80
#define ASIC_CORE_NUM                   8

#define REG09_LENGTH                    12     


#define RESET_ARG_ALL                   "\0x00\0x00"
#define RESET_ARG_JOB                   "\0xe1\0xe1"

#define CHIP_ID_BROADCAST               0x00
#define CHIP_RESULT_LENGTH              36


#define ASIC_A8_MAX_TEMP                564
#define ASIC_A8_MIN_TEMP                445
#define ASIC_A8_MAX_VOL                 550 
#define ASIC_A8_MIN_VOL                 450

#define FAN_DEFAULT_SPEED               20

#define ASIC_LAST_CORE_NUM_AUTO         315

#define ASIC_CHIP_A_BUCKET              (ASIC_CHAIN_NUM * ASIC_CHIP_NUM)

#define LOG_FILE_PREFIX         "/tmp/log/analys"


#define ASIC_FAN_DECT0_DEVICE_NAME  ("/dev/fandect0.0")
#define ASIC_FAN_DECT1_DEVICE_NAME  ("/dev/fandect1.0")

#define ASIC_FAN_STEP_SPEED             (30)
#define ASIC_FAN_SAFE_SPEED             (100)
#define ZYNQ_MAIN_FUK                   (50000000)

#define WEAK_CHIP_THRESHOLD     (6)
#define BROKEN_CHIP_THRESHOLD   (4)
#define WEAK_CHIP_SYS_CLK       (600 * 1000)
#define BROKEN_CHIP_SYS_CLK     (400 * 1000)

#define RAND_TEMP_UPDATE_MS     (30 * 1000)

#define T4_TEMP_LOW_INIT    (40)
#define T4_TEMP_HIGH_INIT   (60)
#define T4_TEMP_RANGE       (T4_TEMP_HIGH_INIT - T4_TEMP_LOW_INIT)
#define T4_TEMP_LOW_MIN     (T4_TEMP_LOW_INIT - 5)
#define T4_TEMP_LOW_MAX     (T4_TEMP_LOW_INIT + 5)
#define T4_TEMP_HIGH_MIN    (T4_TEMP_HIGH_INIT - 5)
#define T4_TEMP_HIGH_MAX    (T4_TEMP_HIGH_INIT + 5)

extern int g_last_temp_min;
extern int g_last_temp_max;

extern struct A1_chain *chain[ASIC_CHAIN_NUM];

struct pool_config {
    char pool_url[512];
    char pool_user[512];
    char pool_pass[512];
};


int get_current_ms(void);

bool check_chip(struct A1_chain *a1, int i);
void disable_chip(struct A1_chain *a1, uint8_t chip_id);
bool is_chip_disabled(struct A1_chain *a1, uint8_t chip_id);
void check_disabled_chips(struct A1_chain *a1);

bool abort_work(struct A1_chain *a1);
bool set_work(struct A1_chain *a1, uint8_t chip_id, struct work *work, uint8_t queue_states);
bool get_result(struct A1_chain *a1, uint8_t *nonce, uint8_t *hash, uint8_t *chip_id, uint8_t *job_id);

int mcompat_chain_detect(struct A1_chain *a1);

void mcompat_rand_temp_update(void);

void mcompat_fan_detect(void);

char* mcompat_arg_printd(char *arg, int len);

char* mcompat_arg_printe(char *arg, int len);

int read_cgminer_config(void);

void config_adc_vsener(unsigned char chain_id, unsigned char chip_id);

void config_adc_tsener(unsigned char chain_id, unsigned char chip_id);

float get_chip_voltage(unsigned char chain_id, unsigned char chip_id);

int get_chip_temperature(unsigned char chain_id, unsigned char chip_id);


int temp_to_centigrade(int temp);



#endif

