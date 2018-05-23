#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <stdbool.h>

#include "logging.h"
#include "miner.h"
#include "util.h"

#include "mcompat_config.h"

#include "t4_cmd.h"
#include "t4_clock.h"
#include "t4_common.h"



struct pool_config g_pool_run[3];
struct pool_config g_pool_conf[3];

struct A1_chain *chain[ASIC_CHAIN_NUM];

const uint32_t nonce_step = 0x00100000;

//const uint8_t reg0d_default_value[] = {0x00, 0x00, 0xa0, 0x00, 0x3e, 0xc0, 0x80, 0x00, 0x10, 0x00, 0x00, 0x74};
const uint8_t reg0d_default_value[] = {0x00, 0x00, 0xa0, 0x00, 0x3e, 0x80, 0x02, 0x00, 0x00, 0x08, 0x00, 0x64};

static void create_job(uint8_t chip_id, uint8_t job_id, struct work *work, uint8_t *job)
{
    int i;
    uint16_t crc;
    uint32_t work_nonce;
    uint32_t start_nonce;
    unsigned char tmp_buf[128];
    unsigned char *wdata = work->data;

    work_nonce = (work->data[42] << 24) + (work->data[41] << 16) + (work->data[40] << 8) + (work->data[39] << 0);
    start_nonce = work_nonce + (nonce_step * (chip_id - 1));

    // cmd
    job[0] = ((job_id & 0x0f) << 4) | CMD_WRITE_JOB;
    job[1] = chip_id;

    // end nonce
    job[2] = 0xff;
    job[3] = 0xff;
    job[4] = 0xff;
    job[5] = 0xdb;
        
    // target   
    job[6 + 0] = work->target[31];
    job[6 + 1] = work->target[30];
    job[6 + 2] = work->target[29];
    job[6 + 3] = work->target[28];      
        
    // data
    for(i = 0; i < 76; i = i + 4)
    {
        job[10 + i + 0] = work->data[i + 3];
        job[10 + i + 1] = work->data[i + 2];
        job[10 + i + 2] = work->data[i + 1];
        job[10 + i + 3] = work->data[i + 0];
    }

    // start nonce
    job[10 + 36] = (uint8_t)((start_nonce >>  0) & 0xff);
    job[10 + 41] = (uint8_t)((start_nonce >> 24) & 0xff);
    job[10 + 42] = (uint8_t)((start_nonce >> 16) & 0xff);
    job[10 + 43] = (uint8_t)((start_nonce >>  8) & 0xff);

    // crc
    memset(tmp_buf, 0, sizeof(tmp_buf));
    for(i = 0; i < 43; i++)
    {
        tmp_buf[(2 * i) + 1] = job[(2 * i) + 0];
        tmp_buf[(2 * i) + 0] = job[(2 * i) + 1];
    }
    crc = CRC16_2(tmp_buf, 86);
    job[86] = (uint8_t)((crc >> 8) & 0xff);
    job[87] = (uint8_t)((crc >> 0) & 0xff);

    //start_nonce = (job[51] << 24) + (job[52] << 16) + (job[53] << 8) + (job[46] << 0);
    //end_nonce = (job[2] << 24) + (job[3] << 16) + (job[4] << 8) + (job[5] << 0);
    //applog(LOG_DEBUG, "start_nonce = 0x%08x, end_nonce = 0x%08x", start_nonce, end_nonce);
}


bool set_work(struct A1_chain *a1, uint8_t chip_id, struct work *work, uint8_t queue_states)
{
    int cid = a1->chain_id;
    bool retval = false;
    uint8_t *jobdata = NULL;
    struct A1_chip *chip = &a1->chips[chip_id - 1];

    int job_id = 1;

    //applog(LOG_INFO, "%d: queuing chip %d with job_id %d, state=0x%02x", cid, chip_id, job_id, queue_states);

    if (job_id == (queue_states & 0x0f) || job_id == (queue_states >> 4))
    {
        applog(LOG_WARNING, "%d: job overlap: %d, 0x%02x", cid, job_id, queue_states);
    }

    jobdata = malloc(256);
    if(jobdata == NULL)
    {
        return false;
    }
    
    create_job(chip_id, job_id, work, jobdata);
    if (!mcompat_cmd_write_job(a1->chain_id, chip_id, jobdata, JOB_LENGTH)) 
    {
        retval = false;
        applog(LOG_ERR, "%d: set work for chip %d.%d faile", cid, chip_id, job_id);
    } 
    else 
    {
        retval = true;
        chip->work[0] = work;
        applog(LOG_DEBUG, "%d: set work for chip %d.%d OK", cid, chip_id, job_id);
    }

    free(jobdata);
    
    return retval;
}


bool get_result(struct A1_chain *a1, uint8_t *nonce, uint8_t *hash, uint8_t *chip_id, uint8_t *job_id)
{
    int i;
    uint8_t buffer[64];

    memset(buffer, 0, sizeof(buffer));
    if(mcompat_cmd_read_result(a1->chain_id, CHIP_ID_BROADCAST, buffer, RES_LENGTH))
    //if(mcompat_cmd_read_nonce(a1->chain_id, buffer, RES_LENGTH))
    {
        //hexdump("result:", buffer, RES_LENGTH + 4);
        
        *job_id = buffer[0] >> 4;
        *chip_id = buffer[1];
        
        memcpy(nonce, buffer + 2, 4);
        for(i = 0; i < 32; i = i + 4)
        {
            hash[i + 0] = buffer[6 + i + 3];
            hash[i + 1] = buffer[6 + i + 2];
            hash[i + 2] = buffer[6 + i + 1];
            hash[i + 3] = buffer[6 + i + 0];
        }

        //applog(LOG_DEBUG, "****************************************************");
        //applog(LOG_DEBUG, "Got nonce for chip %d / job_id %d", *chip_id, *job_id);
        //hexdump("nonce", nonce, 4);
        //hexdump("hash:", hash, 32);
        
        return true;
    }
    
    return false;
}


bool abort_work(struct A1_chain *a1)
{
    applog(LOG_INFO, "Start to reset ");
    
    return mcompat_cmd_resetjob(a1->chain_id, CHIP_ID_BROADCAST);
}


/*
 * if not cooled sufficiently, communication fails and chip is temporary
 * disabled. we let it inactive for 30 seconds to cool down
 *
 * TODO: to be removed after bring up / test phase
 */
#define COOLDOWN_MS     (30 * 1000)
#define CHECK_CORES_MS  (10 * 60 * 1000)

/* if after this number of retries a chip is still inaccessible, disable it */
#define DISABLE_CHIP_FAIL_THRESHOLD 3
#define LEST_CORE_BIN1_CHAIN    300
#define LEST_CORE_BIN2_CHAIN    260
#define LEST_CORE_BIN3_CHAIN    220
#define CHAIN_RESET_TEST_NUM    6   

/********** disable / re-enable related section (temporary for testing) */
int get_current_ms(void)
{
    cgtimer_t ct;
    cgtimer_time(&ct);
    return cgtimer_to_ms(&ct);
}

bool is_chip_disabled(struct A1_chain *a1, uint8_t chip_id)
{
    struct A1_chip *chip = &a1->chips[chip_id - 1];
    return chip->disabled || chip->cooldown_begin != 0;
}

/* check and disable chip, remember time */
void disable_chip(struct A1_chain *a1, uint8_t chip_id)
{
    struct A1_chip *chip = &a1->chips[chip_id - 1];
    int cid = a1->chain_id;
    if (is_chip_disabled(a1, chip_id)) {
        applog(LOG_WARNING, "%d: chip %d already disabled", cid, chip_id);
        return;
    }
    applog(LOG_WARNING, "%d: disabling chip %d", cid, chip_id);
    chip->cooldown_begin = get_current_ms();
}

/* check if disabled chips can be re-enabled */
void check_disabled_chips(struct A1_chain *a1)
{
    int i;
    int cid = a1->chain_id;
    uint8_t reg[16];
    struct spi_ctx *ctx = a1->spi_ctx;

    for (i = 0; i < a1->num_active_chips; i++) 
    {
        int chip_id = i + 1;
        struct A1_chip *chip = &a1->chips[i];
        if (!is_chip_disabled(a1, chip_id))
            continue;
        /* do not re-enable fully disabled chips */
        if (chip->disabled)
            continue;
        if (chip->cooldown_begin + COOLDOWN_MS > get_current_ms())
            continue;
        
        if (!mcompat_cmd_read_register(a1->chain_id, chip_id, reg, REG_LENGTH)) 
        {
            chip->fail_count++;
            applog(LOG_WARNING, "%d: chip %d not yet working - %d", cid, chip_id, chip->fail_count);
            
            if (chip->fail_count > DISABLE_CHIP_FAIL_THRESHOLD) 
            {
                applog(LOG_WARNING, "%d: completely disabling chip %d at %d", cid, chip_id, chip->fail_count);
                chip->disabled = true;
                a1->num_cores -= chip->num_cores;
            }
            else
            {
                /* restart cooldown period */
                chip->cooldown_begin = get_current_ms();
            }           
        }
        else
        {
            applog(LOG_WARNING, "%d: chip %d is working again", cid, chip_id);

            chip->cooldown_begin = 0;
            chip->fail_count = 0;
            chip->fail_reset = 0;
        }
    }

    if (a1->last_check_time + CHECK_CORES_MS < get_current_ms())
    {
        a1->last_check_time = get_current_ms();
        
        //if the core < 300 for bin1, reset this chain 
        if((a1->num_cores < LEST_CORE_BIN1_CHAIN) && (a1->bin1_test < CHAIN_RESET_TEST_NUM))
        {
            a1->bin1_test++;
            applog(LOG_WARNING, "###### reset the chain %d bin1 ######", cid);
            
            mcompat_chain_hw_reset(a1->chain_id);
            a1->num_chips = mcompat_chain_detect(a1);
            if(a1->num_chips < 1)
            {
                mcompat_chain_power_down_all();
                applog(LOG_WARNING, "reset chain %d faile, cgminer exit !!!", cid);
                exit(0);
            }
            a1->num_active_chips = a1->num_chips;
            a1->num_cores = 0;
        
            for (i = 0; i < a1->num_active_chips; i++)
            {
                check_chip(a1, i);
            }
            applog(LOG_WARNING, "Detected the chain %d with %d cores", a1->chain_id, a1->num_cores);
            return;
        }
        //if the core < 260 for bin2, reset this chain
        if((a1->num_cores < LEST_CORE_BIN2_CHAIN) && (a1->bin2_test < CHAIN_RESET_TEST_NUM)) 
        {
            //a1->bin2_test++;
            applog(LOG_WARNING, "###### reset the chain %d bin2 ######", cid);
            
            mcompat_chain_hw_reset(a1->chain_id);
            a1->num_chips = mcompat_chain_detect(a1);
            if(a1->num_chips < 1)
            {
                mcompat_chain_power_down_all();
                applog(LOG_WARNING, "reset chain %d faile, cgminer exit !!!", cid);
                exit(0);
            }
            a1->num_active_chips = a1->num_chips;
            a1->num_cores = 0;
        
            for (i = 0; i < a1->num_active_chips; i++)
            {
                check_chip(a1, i);
            }
            applog(LOG_WARNING, "Detected the chain %d with %d cores", a1->chain_id, a1->num_cores);
            return;
        }
/*
        //if the core < 220 for bin3, reset this chain 
        if((a1->num_cores < LEST_CORE_BIN3_CHAIN) && (a1->bin3_test < CHAIN_RESET_TEST_NUM))
        {
            a1->bin3_test++;
            applog(LOG_WARNING, "###### reset the chain %d bin3 ######", cid);
            
            mcompat_chain_hw_reset(a1->chain_id);    
            a1->num_chips = mcompat_chain_detect(a1);
            if(a1->num_chips < 1)
            {
                mcompat_chain_power_down_all();
                applog(LOG_WARNING, "reset chain %d faile, cgminer exit !!!", cid);
                exit(0);
            }
            a1->num_active_chips = a1->num_chips;
            a1->num_cores = 0;
        
            for (i = 0; i < a1->num_active_chips; i++)
            {
                check_chip(a1, i);
            }
            applog(LOG_WARNING, "Detected the chain %d with %d cores", a1->chain_id, a1->num_cores);
            return;
        }
*/
    }
}


bool check_chip(struct A1_chain *a1, int i)
{
    uint8_t buffer[64];
    int chip_id = i + 1;
    int cid = a1->chain_id;
    int j = 0;

    memset(buffer, 0, sizeof(buffer));
    if (!mcompat_cmd_read_register(a1->chain_id, chip_id, buffer, REG_LENGTH)) 
    {
        applog(LOG_WARNING, "%d: Failed to read register for chip %d -> disabling", cid, chip_id);
        a1->chips[i].num_cores = 0;
        a1->chips[i].disabled = 1;
        return false;
    }
    else
    {
        //hexdump("check chip:", buffer, REG_LENGTH);
    }

    a1->chips[i].num_cores = 0;
    for(j = 0; j < 8; j++)
    {
        if((buffer[11] >> j) & 0x01)
        {
            (a1->chips[i].num_cores)++;
        }
    }

    if(a1->chips[i].num_cores > 6)
    {
        g_ctype = CHIP_TYPE_T88;
    }
    
    a1->num_cores += a1->chips[i].num_cores;
    applog(LOG_DEBUG, "%d: Found chip %d with %d active cores", cid, chip_id, a1->chips[i].num_cores);

    //keep ASIC register value
    memcpy(a1->chips[i].reg, buffer, 12);
    a1->chips[i].temp= 0x000003ff & ((buffer[7] << 8) | buffer[8]);

    if (a1->chips[i].num_cores < BROKEN_CHIP_THRESHOLD) 
    {
        applog(LOG_WARNING, "%d: broken chip %d with %d active cores", cid, chip_id, a1->chips[i].num_cores);

        a1->chips[i].disabled = true;
        a1->num_cores -= a1->chips[i].num_cores;
        
        return false;
    }

    if (a1->chips[i].num_cores < WEAK_CHIP_THRESHOLD) 
    {
        applog(LOG_WARNING, "%d: weak chip %d with %d active cores", cid, chip_id, a1->chips[i].num_cores);
        
        return false;
    }

    return true;
}

void config_adc_vsener(unsigned char chain_id, unsigned char chip_id)
{
    unsigned char buf_in[16];
    unsigned char buf_out[16];
    struct A1_chain *a1 = chain[chain_id];

    memset(buf_in, 0, sizeof(buf_in));
    memcpy(buf_in, reg0d_default_value, REG_LENGTH);
    buf_in[4] = (a1->pll / 2 * 1000) / 16 / 650;
    
    buf_in[2] = buf_in[2] & 0x7f;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);
    
    buf_in[2] = buf_in[2] | 0x80;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);

    buf_in[1] = buf_in[1] | 0x04;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);

    buf_in[2] = buf_in[2] | 0x20;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);

    buf_in[0] = (buf_in[0] | 0x01) & 0xfd;
    buf_in[1] = (buf_in[1] | 0x02) & 0x7f;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);
}


void config_adc_tsener(unsigned char chain_id, unsigned char chip_id)
{
    unsigned char buf_in[16];
    unsigned char buf_out[16];
    struct A1_chain *a1 = chain[chain_id];

    memset(buf_in, 0, sizeof(buf_in));
    memcpy(buf_in, reg0d_default_value, REG_LENGTH);
    buf_in[4] = (a1->pll / 2 * 1000) / 16 / 650;
    
    buf_in[2] = buf_in[2] & 0x7f;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);
    
    buf_in[2] = buf_in[2] | 0x80;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);

    buf_in[1] = buf_in[1] | 0x04;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);

    buf_in[2] = buf_in[2] | 0x20;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);

    buf_in[0] = buf_in[0] & 0xfc;
    buf_in[1] = buf_in[1] & 0x7d;
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    usleep(10000);
}


float get_chip_voltage(unsigned char chain_id, unsigned char chip_id)
{
    struct A1_chain *a1 = chain[chain_id];
    unsigned char buf_in[16];
    unsigned char buf_out[16];
    uint32_t rd_v = 0;
    float tmp_v = 0.0;

    memset(buf_in, 0, sizeof(buf_in));
    memcpy(buf_in, reg0d_default_value, REG_LENGTH);
    buf_in[4] = (a1->pll / 2 * 1000) / 16 / 650;
    
    hexdump("in", buf_in, REG_LENGTH);
    mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
    hexdump("out", buf_out, REG_LENGTH);

    rd_v = ((buf_out[2] << 8) | buf_out[3]) & 0x03ff; 
    tmp_v = (float)(rd_v * MUL_COEF) / 1024;
    return tmp_v;
}


int get_chip_temperature(unsigned char chain_id, unsigned char chip_id)
{
    unsigned char buf_in[16];
    unsigned char buf_out[16];
    struct A1_chain *a1 = chain[chain_id];

    if(g_ctype == CHIP_TYPE_D88)
    {
        memset(buf_in, 0, sizeof(buf_in));
        memcpy(buf_in, reg0d_default_value, REG_LENGTH);
        buf_in[4] = (a1->pll / 2 * 1000) / 16 / 650;
        
        //hexdump("in", buf_in, REG_LENGTH);
        mcompat_cmd_read_write_reg0d(chain_id, chip_id, buf_in, REG_LENGTH, buf_out);
        //hexdump("out", buf_out, REG_LENGTH);

        return (0x03ff & ((buf_out[2] << 8) | buf_out[3]));
    }
    else
    {
        return 0;
    }
}

#if 0
int mcompat_power_on(int chain_id)
{
    if(mcompat_get_plug(chain_id) != 0)
    {
        applog(LOG_WARNING, "chain %d >>> the board not inserted !!!", chain_id);
        return -1;
    }

    mcompat_set_power_en(chain_id, 1);
    sleep(5);
    mcompat_set_reset(chain_id, 1);
    sleep(1);
    mcompat_set_start_en(chain_id, 1);

    applog(LOG_INFO, "power on chain %d ", chain_id);
    
    return 0;
}


int mcompat_power_down(int chain_id)
{
    mcompat_set_power_en(chain_id, 0);
    sleep(1);
    mcompat_set_reset(chain_id, 0);
    mcompat_set_start_en(chain_id, 0);
    mcompat_set_led(chain_id, 1);

    return 0;
}


int mcompat_hw_reset(int chain_id)
{
    mcompat_set_reset(chain_id, 0);
    sleep(1);
    mcompat_set_reset(chain_id, 1);
    sleep(1);
    
    return 0;
}


int mcompat_power_on_all(void)
{
    int i;

    for(i = 0; i < ASIC_CHAIN_NUM; i++)
    {
        mcompat_power_on(i);
    }
}


int mcompat_power_down_all(void)
{
    int i;

    for(i = 0; i < ASIC_CHAIN_NUM; i++)
    {
        mcompat_power_down(i);
    }
}
#endif


int mcompat_chain_detect(struct A1_chain *a1)
{
    uint8_t buffer[64];
    int pllindex;
    int chip_num;

    //set_spi_speed(1500000);
    mcompat_set_spi_speed(a1->chain_id, 2);
    usleep(10000);
    
    if(!mcompat_cmd_resetall(a1->chain_id, ADDR_BROADCAST))
    {
        applog(LOG_WARNING, "cmd reset fail");
        goto failure;
    }
    usleep(1000000);

    pllindex = Ax_clock_pll2index(a1->pll);
    if(!Ax_clock_setpll_by_step(a1, pllindex))
    {
        applog(LOG_WARNING, "set pll fail");
        return 0;
    }
        
    //set_spi_speed(3250000);
    mcompat_set_spi_speed(a1->chain_id, 3);
    sleep(1);
    
#if 0
    applog(LOG_WARNING, "bist mask");
    if(!mcompat_cmd_write_reg0d(a1, 0, (uint8_t*)reg0d_default_value))
    {
        applog(LOG_WARNING, "write reg 0d fail");
        goto failure;
    }
    sleep(1);
#endif

    chip_num = mcompat_cmd_bist_start(a1->chain_id, ADDR_BROADCAST);
    if(chip_num <= 0)
    {
        applog(LOG_WARNING, "bist start fail");
        goto failure;
    }
    a1->num_chips = chip_num;
    applog(LOG_DEBUG, "chain %d : detected %d chips", a1->chain_id, a1->num_chips);
    sleep(1);

    if(!mcompat_cmd_bist_fix(a1->chain_id, ADDR_BROADCAST))
    {
        applog(LOG_WARNING, "bist fix fail");
        goto failure;
    }
    usleep(10000);

    mcompat_set_led(a1->chain_id, 0);
    return a1->num_chips;

failure:
    mcompat_set_led(a1->chain_id, 1);
    return -1;
}



char* mcompat_arg_printd(char *arg, int len)
{
    int i;
    char *pstr;
    char *pindex;
    
    pstr = malloc(512);
    if(pstr == NULL)
    {
        return NULL;
    }

    memset(pstr, 0, 512);
    pindex = pstr;
    for(i = 0; i < len; i++)
    {
        *(pindex+i) = *(arg+i) - 3;
    }

    //printf("ciphertext=[%s] \n", pstr);
    return pstr;
    
}

char* mcompat_arg_printe(char *arg, int len)
{
    int i;
    char *pstr;
    
    pstr = malloc(512);
    if(pstr == NULL)
    {
        return NULL;
    }

    memset(pstr, 0, 512);
    for(i = 0; i < len; i++)
    {
        *(pstr+i) = *(arg+i) + 3;
    }

    //printf("plaintext=[%s] \n", pstr);
    return pstr;
}


// for 
char char_bcd2hex(char bcd)
{
    char hex = 0;
    
    if((bcd >= '0') && (bcd <= '9'))
    {
        hex = bcd - '0';    
    }
    else if((bcd >= 'A') && (bcd <= 'F'))
    {
        hex = bcd - 'A' + 0x0a;
    }
    else if((bcd >= 'a') && (bcd <= 'f'))
    {
        hex = bcd - 'a' + 0x0a;
    }
    else
    {
        printf("char_bcd2hex para error!!! \n");
    }

    return hex;
}

int str_bcd2hex(char *bcd, char *hex)
{
    int i;
    int len;

    len = strlen(bcd);
    if((bcd[len-2] == 0x0d) && (bcd[len-1] == 0x0a))
    {
        len = len - 2;
    }
    else if((bcd[len-1] == 0x0a) || (bcd[len-1] == 0x0d))
    {
        len = len - 1;
    }
    
    if((len & 0x01) != 0)
    {
        return 0;
    }

    for(i = 0; i < (len / 2); i++)
    {
        hex[i] = ((char_bcd2hex(bcd[2*i+0]) << 4) & 0xf0) + ((char_bcd2hex(bcd[2*i+1]) << 0) & 0x0f);
        //printf("loop %d  bcd %c %c  hex %02x \n", i, bcd[2*i+0], bcd[2*i+1], hex[i]);
    }

    return (len / 2);
}

void debug_print_pool_config(void)
{
    int i;

    for(i = 0; i < 3; i++)
    {
        printf("pool %d \n", i);
        printf("url:%s \n", g_pool_run[i].pool_url);
        printf("user:%s \n", g_pool_run[i].pool_user);
        printf("pass:%s \n", g_pool_run[i].pool_pass);
    }
}

#define CONFIG_FILE_PATH    "/etc/cg.conf"
char * key = "NTBO^pac./01WU@S";    //QWERasdf1234ZXCV
int read_cgminer_config(void)
{
    FILE *fp;
    int line = 0;
    char strLine[512];
    char str1[256];
    char str2[256];
    char *strkey;
    int len;

    memset(&g_pool_run, 0, sizeof(g_pool_run));
    memset(&g_pool_conf, 0, sizeof(g_pool_conf));
    if((fp = fopen(CONFIG_FILE_PATH,"r")) == NULL)
    {   
        printf("Open Falied!");   
        return -1;   
    }

    strkey = mcompat_arg_printe(key, 16);
    memset(strLine, 0, sizeof(strLine));
    while(fgets(strLine, sizeof(strLine), fp))  
    {
        line++;
        if(line == 1)
        {
            printf("miner config no:%s", strLine);
            continue;
        }
        if(line > 10)
        {
            continue;
        }
        memset(str1, 0, sizeof(str1));
        len = str_bcd2hex(strLine, str1);
        
        memset(str2, 0, sizeof(str2));
        AES_Decrypt(strkey, str1, len, str2);

        switch(line)
        {
            //url
            case 2:
            case 5:
            case 8:
                memcpy(g_pool_run[(line-2)/3].pool_url, str2, strlen(str2));
                break;
            case 3:
            case 6:
            case 9:
                memcpy(g_pool_run[(line-2)/3].pool_user, str2, strlen(str2));
                break;
            case 4:
            case 7:
            case 10:
                memcpy(g_pool_run[(line-2)/3].pool_pass, str2, strlen(str2));
                break;
            default:
                break;
        }
    }
    
    fclose(fp);

    //debug_print_pool_config();
    
    return 0;   
}


int g_last_temp_min = T4_TEMP_LOW_INIT;
int g_last_temp_max = T4_TEMP_HIGH_INIT;
static int s_last_temp_update_time = 0;
void mcompat_rand_temp_update(void)
{
    if(s_last_temp_update_time + RAND_TEMP_UPDATE_MS < get_current_ms())
    {
        g_last_temp_min = g_last_temp_min + (rand() % 5) - 2;
        if(g_last_temp_min > T4_TEMP_LOW_MAX)
        {
            g_last_temp_min = T4_TEMP_LOW_MAX;
        }
        else if(g_last_temp_min < T4_TEMP_LOW_MIN)
        {
            g_last_temp_min = T4_TEMP_LOW_MIN;
        }
        g_last_temp_max = g_last_temp_min + T4_TEMP_RANGE + (rand() % 5) - 2;

        s_last_temp_update_time = get_current_ms();
    }
}


/******************************************************************************
 * Function:    temp_to_centigrade
 * Description: temperature value to centigrade
 * Arguments:   temp        temperature value from AD
 * Return:      centigrade value
 ******************************************************************************/
int temp_to_centigrade(int temp)
{
    // return 0 if temp is a invalid value
    if(temp == 0)
        return 0;

    return (595.0f - temp) * 2 / 3 + 0.5f;
}



