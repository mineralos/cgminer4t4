/*
 * cgminer SPI driver for Bitmine.ch A1 devices
 *
 * Copyright 2013, 2014 Zefir Kurtisi <zefir.kurtisi@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <stdbool.h>

#include "logging.h"
#include "miner.h"
#include "util.h"

#include "t4_cmd.h"
#include "t4_clock.h"
#include "t4_common.h"
#include "getwork.h"

#include "mcompat_aes.h"
#include "mcompat_config.h"
#include "mcompat_drv.h"
#include "mcompat_fan.h"
#include "mcompat_lib.h"

#include "mcompat_chain.h"
#include "mcompat_tempctrl.h"
#include "mcompat_fanctrl.h"


static int s_log_cnt[ASIC_CHAIN_NUM] = {0};
static char s_log[ASIC_CHAIN_NUM][ASIC_CHIP_NUM][256];

static int s_temp_cnt[ASIC_CHAIN_NUM] = {0};

int g_hwver;
int g_mtype;
int g_ctype;
int g_auto_fan = 1;
int g_fan_speed = 1;
int g_reset_delay = 0xffff;
//int g_miner_state = 0;

int g_chip_temp[ASIC_CHAIN_NUM][ASIC_CHIP_NUM];

static volatile uint8_t g_debug_stats[ASIC_CHAIN_NUM] = {0};


/* one global board_selector and spi context is enough */
//static struct board_selector *board_selector;
//static struct spi_ctx *spi;

/********** work queue */
static bool wq_enqueue(struct work_queue *wq, struct work *work)
{
    if (work == NULL) {
        applog(LOG_DEBUG, "wq_enqueue para error");
        return false;
    }
    
    struct work_ent *we = malloc(sizeof(*we));
    if(we == NULL) {
        applog(LOG_DEBUG, "wq_enqueue malloc error");
        return false;
    }

    we->work = work;
    INIT_LIST_HEAD(&we->head);
    list_add_tail(&we->head, &wq->head);
    wq->num_elems++;
    return true;
}

static struct work *wq_dequeue(struct work_queue *wq)
{
    if (wq == NULL){
        applog(LOG_DEBUG, "wq_dequeue para error");
        return NULL;
    }
    
    if (wq->num_elems == 0){
        //applog(LOG_DEBUG, "the queue is empty");
        return NULL;
    }
    
    struct work_ent *we;
    we = list_entry(wq->head.next, struct work_ent, head);
    struct work *work = we->work;

    list_del(&we->head);
    free(we);
    wq->num_elems--;
    
    return work;
}

/* Show fan speed just after cgminer start mining in Web */
void set_cgpu_init_value(struct cgpu_info *cgpu)
{
    struct A1_chain *a1 = cgpu->device_data;

    cgpu->fan_duty = (opt_fanspeed * A8_FAN_STEP_DUTY);
    cgpu->chip_num = a1->num_active_chips;
    cgpu->core_num = a1->num_cores;
}

#if 0
void set_cgpu(struct cgpu_info *cgpu)
{
    struct A1_chain *a1 = cgpu->device_data;
    int cid = a1->chain_id;

    cgpu->temp_min  = (double)temp_to_centigrade(g_temp[cid].temp_lowest[0]);
    cgpu->temp_max  = (double)temp_to_centigrade(g_temp[cid].temp_highest[0]);
    cgpu->temp      = (double)temp_to_centigrade(g_temp[cid].final_temp_avg);


    if (g_fan_temp.speed)
        cgpu->fan_duty = g_fan_temp.speed;
    else
        cgpu->fan_duty = (opt_fanspeed * A8_FAN_STEP_DUTY);

    cgpu->chip_num = a1->num_active_chips;
    cgpu->core_num = a1->num_cores; 
}
#endif

/*
 * for now, we have one global config, defaulting values:
 * - ref_clk 16MHz / sys_clk 800MHz
 * - 2000 kHz SPI clock
 */
struct A1_config_options A1_config_options = {
    .ref_clk_khz = 16000, .sys_clk_khz = 800000, .spi_clk_khz = 2000,
};

/* override values with --bitmine-a1-options ref:sys:spi: - use 0 for default */
static struct A1_config_options *parsed_config_options;


/********** test fanction **********************/
void hw_test(void)
{
    int i;
    char buffer[256];

#if 0
    char *pstr;
    pstr = mcompat_arg_printd(g_url1, strlen(g_url1));
    printf("[%s] \n[%s] \n", g_url1, pstr);

    pstr = mcompat_arg_printd(g_url2, strlen(g_url2));
    printf("[%s] \n[%s] \n", g_url2, pstr);

    pstr = mcompat_arg_printd(g_user1, strlen(g_user1));
    printf("[%s] \n[%s] \n", g_user1, pstr);
    
    pstr = mcompat_arg_printd(g_user2, strlen(g_user2));
    printf("[%s] \n[%s] \n", g_user2, pstr);
#endif
}



void software_test(void)
{
#if 0
    uint32_t rd_v = 0;
    float tmp_v = 0.0;

    config_adc_vsener(0, 1);
    sleep(1);    
    tmp_v = get_chip_voltage(0, 1);
    applog(LOG_WARNING, "vol:%f", tmp_v);

    sleep(1);
    
    config_adc_tsener(0, 1);
    sleep(1);
    rd_v = get_chip_temperature(0, 1);
    applog(LOG_WARNING, "temp:%d", rd_v);
#endif
#if 1
    int i;
    unsigned char reg[16];

    for(i = 0; i < ASIC_CHAIN_NUM; i++)
    {
        memset(reg, 0, sizeof(reg));
        mcompat_cmd_read_register(i, 0, reg, REG_LENGTH);
        hexdump("reg", reg, REG_LENGTH);

        
    }
#endif
}

extern int g_chain_alive[MCOMPAT_CONFIG_MAX_CHAIN_NUM];
/********** driver interface **********************/
int init_one_A1_chain(struct A1_chain *a1)
{
    int i;

    applog(LOG_INFO, "init chain:%d", a1->chain_id);
    
    if(mcompat_get_plug(a1->chain_id) != 0)
    {
        applog(LOG_INFO, "chain:%d power on fail", a1->chain_id);
        mcompat_chain_power_down(a1->chain_id);
        return -1;
    }

#if 0
    a1->num_chips = mcompat_chain_preinit(a1->chain_id);
    if (a1->num_chips == 0) {
    	return 1;
    }

    if (!mcompat_chain_set_pll(a1->chain_id, opt_A1Pll1, opt_voltage1)) {
    	return 1;
    }

    mcompat_chain_init(a1->chain_id, SPI_SPEED_6250K, false);
#else
    a1->num_chips = mcompat_chain_detect(a1);
    if (a1->num_chips < 1)
    {
        return 1;
    }
    g_chain_alive[a1->chain_id] = 1;
#endif
    
    usleep(10000);

    /* override max number of active chips if requested */
    a1->num_active_chips = a1->num_chips;
    if (A1_config_options.override_chip_num > 0 && a1->num_chips > A1_config_options.override_chip_num) 
    {
        a1->num_active_chips = A1_config_options.override_chip_num;
        applog(LOG_WARNING, "%d: limiting chain to %d chips", a1->chain_id, a1->num_active_chips);
    }

    a1->num_cores = 0;
    for (i = 0; i < a1->num_active_chips; i++)
    {
        check_chip(a1, i);
    }

    /* A8+ does not need auto tune, end auto tune. */
    a1->VidOptimal = true;
    a1->pllOptimal = true;
    applog(LOG_DEBUG, "%d: found %d chips with total %d active cores", a1->chain_id, a1->num_active_chips, a1->num_cores);

    mutex_init(&a1->lock);
    INIT_LIST_HEAD(&a1->active_wq.head);

    return 0;
}


bool chain_strcut_init(void)
{
    int i;
    
    for(i = 0; i < ASIC_CHAIN_NUM; i++)
    {
        chain[i] = malloc(sizeof(struct A1_chain));
        if(chain[i] == NULL)
        {
            applog(LOG_ERR, "malloc A1_chain %d fail", i);
            return false;
        }
        
        memset(chain[i], 0, sizeof(struct A1_chain));
        chain[i]->chain_id = i;
        chain[i]->last_check_time = get_current_ms();       

        chain[i]->chips = calloc(ASIC_CHIP_NUM, sizeof(struct A1_chip));
        if(chain[i]->chips == NULL) {
            applog(LOG_INFO, "chain:%d calloc chips fail", i);
            return false;
        }

        switch(i)
        {
            case 0: 
                chain[i]->pll = opt_A1Pll1;
                chain[i]->vid = opt_voltage1;
                break;
            case 1: 
                chain[i]->pll = opt_A1Pll2;
                chain[i]->vid = opt_voltage2;
                break;
            case 2: 
                chain[i]->pll = opt_A1Pll3;
                chain[i]->vid = opt_voltage3;
                break;
            case 3: 
                chain[i]->pll = opt_A1Pll4;
                chain[i]->vid = opt_voltage4;
                break;
            case 4: 
                chain[i]->pll = opt_A1Pll5;
                chain[i]->vid = opt_voltage5;
                break;
            case 5: 
                chain[i]->pll = opt_A1Pll6;
                chain[i]->vid = opt_voltage6;
                break;
            case 6: 
                chain[i]->pll = opt_A1Pll7;
                chain[i]->vid = opt_voltage7;
                break;
            case 7: 
                chain[i]->pll = opt_A1Pll8;
                chain[i]->vid = opt_voltage8;
                break;
            default: break;
        }
    }
}

void mcompat_chain_set_vid_all()
{
    int i = 0;
    struct A1_chain *a1;

    for(i = 0; i < ASIC_CHAIN_NUM; i++)
    {
        a1 = chain[i];
        mcompat_set_vid(i, a1->vid);
        //sleep(1);
    }
}

void miner_preheat(int time)
{
    int i;
    
    for(i = 0; i < (time * 6); i++)
    {
        sleep(10);
        applog(LOG_INFO, "waiting for appropriate temperature %d s", (i+1)*10);
    }
}


void update_time_form_net(int seconds)
{
    int i;
    struct timeval test_tv;
    
    for(i = 0; i < seconds; i++)
    {
        cgtime(&test_tv);
        if(test_tv.tv_sec > 1000000000)
        {
            applog(LOG_INFO, "update time success");
            break;
        }
        applog(LOG_INFO, "update time faile %d", i);
        sleep(1);
    }
}

bool detect_all_A1_chain(void)
{
    int i;
    int ret;
    struct A1_chain *a1;
    struct cgpu_info *cgpu;
    
    applog(LOG_DEBUG, "detect_all_A1_chain()");

    //vid_pll_test_bench_init(175, 900);
    
    for(i = 0; i < ASIC_CHAIN_NUM; i++)
    {
        a1 = chain[i];      

        ret = init_one_A1_chain(a1);
        if(ret != 0)
        {
            continue;
        }

        config_adc_tsener(i, 0);
        
        cgpu = malloc(sizeof(*cgpu));
        if(cgpu == NULL) {
            continue;
        }
        memset(cgpu, 0, sizeof(*cgpu));
        cgpu->drv = &bitmineA1_drv;
        cgpu->name = "BitmineA1.SingleChain";
        cgpu->threads = 1;
        cgpu->device_data = a1;
        cgtime(&(cgpu->dev_start_tv));

        a1->cgpu = cgpu;
        add_cgpu(cgpu);
        
        applog(LOG_INFO, "Detected chain:%d with chips:%d cores:%d vid:%d pll:%d", i, a1->num_active_chips, a1->num_cores, a1->vid, a1->pll);
    }

    applog(LOG_INFO, "new_devices=%d, total_devices=%d", new_devices, total_devices);
    
    if(total_devices > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}


/* Probe SPI channel and register chip chain */
void A1_detect(bool hotplug)
{
    int i = 0;
    
    /* no hotplug support for SPI */
    if (hotplug)
        return;
    
    applog(LOG_WARNING, "A1_detect()");

    /* parse bimine-a1-options */
    if (opt_bitmine_a1_options != NULL && parsed_config_options == NULL) {
        int ref_clk = 0;
        int sys_clk = 0;
        int spi_clk = 0;
        int override_chip_num = 0;
        int wiper = 0;

        sscanf(opt_bitmine_a1_options, "%d:%d:%d:%d:%d",
               &ref_clk, &sys_clk, &spi_clk,  &override_chip_num, &wiper);
        if (ref_clk != 0)
            A1_config_options.ref_clk_khz = ref_clk;
        if (sys_clk != 0) {
            if (sys_clk < 100000)
                quit(1, "system clock must be above 100MHz");
            A1_config_options.sys_clk_khz = sys_clk;
        }
        if (spi_clk != 0)
            A1_config_options.spi_clk_khz = spi_clk;
        if (override_chip_num != 0)
            A1_config_options.override_chip_num = override_chip_num;
        if (wiper != 0)
            A1_config_options.wiper = wiper;

        /* config options are global, scan them once */
        parsed_config_options = &A1_config_options;
    }

    //g_hwver = misc_get_board_version();
    g_hwver = inno_get_hwver();
    //g_mtype = misc_get_miner_type();
    g_ctype = CHIP_TYPE_D88;



    //sys_platform_debug_init(MCOMPAT_LOG_DEBUG);
    sys_platform_debug_init(MCOMPAT_LOG_INFO);
#ifdef USE_HARDWARE_SOC
    sys_platform_init(PLATFORM_SOC_HUB, MCOMPAT_LIB_MINER_TYPE_A8, ASIC_CHAIN_NUM, ASIC_CHIP_NUM);
#else
    //sys_platform_init(PLATFORM_ZYNQ_SPI_G9, MCOMPAT_LIB_MINER_TYPE_A8, ASIC_CHAIN_NUM, ASIC_CHIP_NUM);
    sys_platform_init(PLATFORM_ZYNQ_HUB_G19, MCOMPAT_LIB_MINER_TYPE_A8, ASIC_CHAIN_NUM, ASIC_CHIP_NUM);
#endif

    /* Init temp ctrl */
    c_temp_cfg tmp_cfg;
    mcompat_tempctrl_get_defcfg(&tmp_cfg);
    tmp_cfg.tmp_min      = -40;     // min value of temperature
    tmp_cfg.tmp_max      = 125;     // max value of temperature
    tmp_cfg.tmp_target   = 50;      // target temperature
    tmp_cfg.tmp_thr_lo   = 30;      // low temperature threshold
    tmp_cfg.tmp_thr_hi   = 75;      // high temperature threshold
    tmp_cfg.tmp_thr_warn = 80;     // warning threshold
    tmp_cfg.tmp_thr_pd   = 85;     // power down threshold
    tmp_cfg.tmp_exp_time = 2000;   // temperature expiring time (ms)
    mcompat_tempctrl_init(&tmp_cfg);

    /* Start fan ctrl thread */
    c_fan_cfg fan_cfg;
    mcompat_fanctrl_get_defcfg(&fan_cfg);
    fan_cfg.preheat = false;
    fan_cfg.fan_mode = g_auto_fan ? FAN_MODE_AUTO : FAN_MODE_MANUAL;
    fan_cfg.fan_speed = 10;
    fan_cfg.fan_speed_target = 10;
    mcompat_fanctrl_init(&fan_cfg);
    mcompat_fanctrl_set_bypass(false);

    pthread_t tid;
    pthread_create(&tid, NULL, mcompat_fanctrl_thread, NULL);



    /* Register PLL map config */
    mcompat_chain_set_pllcfg(g_pll_list, g_pll_regs, PLL_LV_NUM);

    mcompat_chain_power_down_all();
    chain_strcut_init();

    sleep(5);

    mcompat_chain_set_vid_all();
    mcompat_chain_power_on_all();


    // preheat
    miner_preheat(opt_heattime);
    
    // update time
    update_time_form_net(60);

    if(!detect_all_A1_chain())
    {
        sys_platform_exit();
        applog(LOG_WARNING, "A1 dectect faile");
        return;
    }

#if 0
    if(g_ctype == CHIP_TYPE_T88)
    {
        g_fan_speed = opt_fanspeed;
        mcompat_fan_speed_set(0, opt_fanspeed * A8_FAN_STEP_DUTY);
    }
    else
    {
        mcompat_fan_auto_init(FAN_DEFAULT_SPEED);
    }
#endif
    
    // init finshed
    //g_miner_state = 1;

    applog(LOG_INFO, "A1 dectect finshed");
}


uint8_t cLevelError1[3] = "!";
uint8_t cLevelError2[3] = "#";
uint8_t cLevelError3[3] = "$";
uint8_t cLevelError4[3] = "%";
uint8_t cLevelError5[3] = "*";
uint8_t cLevelNormal[3] = "+";

void dm_Log_Save(struct A1_chip *chip, int nChain, int nChip)
{
    uint8_t szInNormal[8] = {0};
    
    //memset(szInNormal,0, sizeof(szInNormal));

    if(chip->hw_errors > 0){
        strcat(szInNormal,cLevelError1);
    }
    if(chip->stales > 0){
        strcat(szInNormal,cLevelError2);
    }

    if(g_ctype == CHIP_TYPE_D88)
    {
        if((chip->temp > MAX_TEMP_CENTIGRADE) || (chip->temp < MIN_TEMP_CENTIGRADE)){
            strcat(szInNormal,cLevelError3);
        }
    }
    
    if(chip->num_cores < ASIC_D88_CORE_NUM){
        strcat(szInNormal,cLevelError4);
    }
#if 0
    if(g_ctype == CHIP_TYPE_D88)
    {
        if((chip->nVol > MAX_VOL) || (chip->nVol < MIN_VOL)){
            strcat(szInNormal,cLevelError5);
        }
    }
#endif    
    if((chip->hw_errors == 0) && (chip->stales == 0) 
         && ((chip->temp < MAX_TEMP_CENTIGRADE) && (chip->temp > MIN_TEMP_CENTIGRADE)) 
        /* &&((chip->nVol < MAX_VOL) && (chip->nVol > MIN_VOL)) */
        && (chip->num_cores == ASIC_D88_CORE_NUM)){
        strcat(szInNormal,cLevelNormal);
    }

    memset(s_log[nChain][nChip], 0, 256);
    sprintf(s_log[nChain][nChip], "\n%-8s|%32d|%8d|%8d|%8d|%8d|%8d|%8d|%8d", szInNormal, chip->nonces_found,
        chip->hw_errors, chip->stales, chip->temp, chip->nVol, chip->num_cores, nChip, nChain);
}

void dm_log_print(int cid, void* log, int len)
{
    FILE* fd;
    char fileName[128] = {0};
    
    sprintf(fileName, "%s%d.log", LOG_FILE_PREFIX, cid);
    fd = fopen(fileName, "w+"); 
    if(fd == NULL)
    {               
        applog(LOG_ERR, "Open log File%d Failed!", cid);
        return;
    }

    fwrite(log, len, 1, fd);
    fflush(fd);
    fclose(fd);
}

static int Ax_temp_compare(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}


#define LOG_SAVE_CNT 60
#define TEMP_UPDATE_CNT 60
#define TEMP_UPDATE_INT_MS  1000


static bool overheated_flag[ASIC_CHAIN_NUM] = {0};
static void overheated_blinking(int cid)
{
    /* Mark chain as overhteated */
    overheated_flag[cid] = true;
    
    // block thread (scan_thread) and blink led
    while (42)
    {
        mcompat_set_led(cid, LED_OFF);
        cgsleep_ms(500);
        mcompat_set_led(cid, LED_ON);
        cgsleep_ms(500);
    }
}

void overheated_check(struct thr_info *thr)
{
    struct cgpu_info *cgpu = thr->cgpu;
    struct A1_chain *a1 = cgpu->device_data;
    int cid = a1->chain_id;
    
    // block thread (miner_thread) if overheated
    if (unlikely(overheated_flag[cid]))
   {   
        while (42)
            ;
    }
}



static void get_chain_temperature(struct thr_info *thr)
{
    struct cgpu_info *cgpu = thr->cgpu;
    struct A1_chain *a1 = cgpu->device_data;
    int cid = a1->chain_id; 

    /* Temperature control */
    int chain_temp_status;

    chain_temp_status = mcompat_tempctrl_update_chain_temp(cid);
    //applog(LOG_WARNING, "status=%d", chain_temp_status);
    if (chain_temp_status != TEMP_INVALID)
    {
        cgpu->temp_min  = (double)g_chain_tmp[cid].tmp_lo;
        cgpu->temp_max = (double)g_chain_tmp[cid].tmp_hi;
        cgpu->temp          = (double)g_chain_tmp[cid].tmp_avg;
        //applog(LOG_WARNING, "min=%f, max=%f, temp=%f", cgpu->temp_min, cgpu->temp_max, cgpu->temp);
    }

    if (chain_temp_status == TEMP_SHUTDOWN)
    {
        /* Shutdown chain */
        applog(LOG_ERR, "DANGEROUS TEMPERATURE(%.0f): power down chain %d",
			cgpu->temp_max, cid);
        mcompat_chain_power_down(cid);
        cgpu->status = LIFE_DEAD;
        cgtime(&thr->sick);

        /* Function doesn't currently return */
        overheated_blinking(cid);
    }

    cgpu->fan_duty = g_fan_cfg.fan_speed;
    cgpu->chip_num = a1->num_active_chips;
    cgpu->core_num = a1->num_cores;
}

extern volatile c_fan_cfg	g_fan_cfg;
static void get_chip_temperatures(struct cgpu_info *cgpu)
{
    struct A1_chain *a1 = cgpu->device_data;

    int i;
    int temp[ASIC_CHIP_NUM] = {0};
    c_temp chain_tmp = {0};

    mcompat_get_chip_temp(a1->chain_id, temp);

    for (i = 0; i < a1->num_active_chips; i++)
        if ((temp[i]>=-40) && (temp[i] <= 125))
    	    a1->chips[i].temp = temp[i];

    //applog(LOG_WARNING, "chain_id %d, temp[0] %d, %d, fan duty %d", a1->chain_id, a1->chips[0].temp, temp[0], g_fan_cfg.fan_speed);
}


static int64_t A1_scanwork(struct thr_info *thr)
{
    struct cgpu_info *cgpu = thr->cgpu;
    struct A1_chain *a1 = cgpu->device_data;
    struct A1_chip *chip = NULL;
    struct work *work = NULL;
    
    int32_t nonce_ranges_processed = 0;
    
    int i;
    int cid = a1->chain_id; 
    bool work_updated = false;
    
    uint8_t chip_id;
    uint8_t job_id;
    uint8_t nonce[4];
    uint8_t hash[32];
    uint8_t reg[REG_LENGTH];
    char noncehex[16];
    char hashhex[128];  
    int64_t hashes = 0;
    struct timeval now;

    //applog(LOG_DEBUG, "------------------ scan work -----------------");
    
    if (a1->num_cores == 0) {
        cgpu->deven = DEV_DISABLED;
        applog(LOG_WARNING, "A1_scanwork() cores num is 0");
        return 0;
    }


    /* We start with low diff to speed up hashrate
     * calculation and then increase diff after an hour to decrease
     * load. */
    if (cgpu->drv->max_diff != ((double)0xffffffff))
    {
        int hours;

        cgtime(&now);
        hours = tdiff(&now, &cgpu->dev_start_tv) / 3600;
        if (hours >= DIFF_DEF_HOURS)
        {
            /* Set to the max double type value, so always device_diff = work_difficulty */
            cgpu->drv->max_diff = ((double)0xffffffff);
        }
    }




    /* poll queued results */
    while (true)
    {
        if (!get_result(a1, nonce, hash, &chip_id, &job_id))
        {
            applog(LOG_DEBUG, "%d: have no nonce", cid);
            break;
        }

        work_updated = true;
        __bin2hex(noncehex, (const unsigned char *)nonce, 4);
        __bin2hex(hashhex, (const unsigned char *)hash, 32);
        
        if (chip_id < 1 || chip_id > a1->num_active_chips) 
        {
            applog(LOG_WARNING, "%d: wrong chip_id %d", cid, chip_id);
            break;
        }
#if 1
        if (job_id < 1 || job_id > 4) 
        {
            applog(LOG_WARNING, "%d: chip %d: result has wrong job_id %d", cid, chip_id, job_id);
            continue;
        }
#endif
        chip = &a1->chips[chip_id - 1];
        work = chip->work[0];
        if (work == NULL) 
        {
            /* already been flushed => stale */
            applog(LOG_WARNING, "%d: chip %d: stale nonce:%s", cid, chip_id, noncehex);
            chip->stales++;
            continue;
        }
        
        if (!submit_nonce_hash(thr, work, nonce, hash)) 
        {
            applog(LOG_WARNING, "%d: chip %d: invalid nonce:%s", cid, chip_id, noncehex);
            chip->hw_errors++;
            /* add a penalty of a full nonce range on HW errors */
            nonce_ranges_processed--;
            continue;
        }
        
        applog(LOG_INFO, "YEAH: %d: chip %d / job_id %d: nonce:%s", cid, chip_id, job_id, noncehex);
        //hexdump("###data1", work->data + 0, 32);
        //hexdump("###data2", work->data + 32, 32);
        //hexdump("###data3", work->data + 64, 32);
        //hexdump("###data4", work->data + 96, 32);
        //hexdump("###target", work->target, 32);
        //applog(LOG_INFO, "YEAH: %d: chip %d / job_id %d: nonce:%s hash:%s", cid, chip_id, job_id, noncehex, hashhex);
        
        chip->nonces_found++;
        hashes += work->device_diff;
    }

    mutex_lock(&a1->lock);
    //mcompat_cmd_auto_nonce(a1->chain_id, 0, RES_LENGTH);
    
    /* check for completed works */
    if(a1->work_start_delay > 0)
    {
        applog(LOG_INFO, "wait for pll stable");
        a1->work_start_delay--;
    }
    else
    {
        struct work *work;
        work = wq_dequeue(&a1->active_wq);
        if (work != NULL) 
        {
            uint8_t result[16] = {0};
            mcompat_cmd_resetjob(a1->chain_id, 0, result);
            usleep(100);
            
            for (i = a1->num_active_chips; i > 0; i--) 
            {
                uint8_t c = i;
                uint8_t qbuff = 0;
                struct A1_chip *chip = &a1->chips[i - 1];

                if(set_work(a1, c, work, qbuff)) 
                {
                    chip->nonce_ranges_done++;
                    nonce_ranges_processed++;
                    chip->work[0] = work;
                    applog(LOG_DEBUG, "%d: chip %d: set work success", cid, c);
                }
                else
                {
                    //disable_chip(a1, c);
                    applog(LOG_DEBUG, "%d: chip %d: set work fail", cid, c);
                }
            }
        }
        else
        {
            //applog(LOG_DEBUG, "the work queue is empty");
        }
    }


    get_chain_temperature(thr);
    
    /* read chip temperatures and voltages */
    if (g_debug_stats[cid]) 
    {
        cgsleep_ms(1);
        get_chip_temperatures(cgpu);
        //get_voltages(t1);
        g_debug_stats[cid] = 0;
    }


    mutex_unlock(&a1->lock);
  
    check_disabled_chips(a1);

    if (nonce_ranges_processed < 0)
    {
        applog(LOG_INFO, "nonce_ranges_processed less than 0");
        nonce_ranges_processed = 0;
    }

    if (nonce_ranges_processed != 0) 
    {
        //applog(LOG_INFO, "%d, nonces processed %d", cid, nonce_ranges_processed);
    }
    
    //mcompat_cmd_auto_nonce(a1->chain_id, 1, RES_LENGTH);

#if 0
    cgtime(&a1->tvScryptCurr);
    timersub(&a1->tvScryptCurr, &a1->tvScryptLast, &a1->tvScryptDiff);
    cgtime(&a1->tvScryptLast);
#endif

    //return (int64_t)(480.0 * (a1->pll) / 1000 * (a1->num_cores / 8.0) * (a1->tvScryptDiff.tv_usec / 1000.0));
    //return (int64_t)(520.0 * (a1->pll/ 1000.0) * (a1->num_cores / 8.0) * (a1->tvScryptDiff.tv_usec / 1000.0));
    return hashes;
}


/* queue two work items per chip in chain */
static bool A1_queue_full(struct cgpu_info *cgpu)
{
    struct A1_chain *a1 = cgpu->device_data;
    int queue_full = false;

    //applog(LOG_DEBUG, "%d, A1 running queue_full: %d/%d", a1->chain_id, a1->active_wq.num_elems, a1->num_active_chips);
    
    mutex_lock(&a1->lock);
    wq_enqueue(&a1->active_wq, get_queued(cgpu));
    
    //if (a1->active_wq.num_elems >= a1->num_active_chips)
    if (a1->active_wq.num_elems >= 1)
    {
        queue_full = true;
    }

    mutex_unlock(&a1->lock);

    return queue_full;
}


static void A1_flush_work(struct cgpu_info *cgpu)
{
    struct A1_chain *a1 = cgpu->device_data;
    int cid = a1->chain_id;
    int i;

    applog(LOG_DEBUG, "enter A1_flush_work()");

    mutex_lock(&a1->lock);
    
    /* stop chips hashing current work */
    if (!abort_work(a1)) 
    {
        applog(LOG_ERR, "%d: failed to abort work in chip chain!", cid);
    }
    
    /* flush queued work */
    while (a1->active_wq.num_elems > 0) 
    {
        struct work *work = wq_dequeue(&a1->active_wq);
        assert(work != NULL);
        work_completed(cgpu, work);
    }
    
    mutex_unlock(&a1->lock);
}


static void A1_get_statline_before(char *buf, size_t len, struct cgpu_info *cgpu)
{
    char temp[10];
    struct A1_chain *a1 = cgpu->device_data;
    
    if (a1->temp != 0){
        snprintf(temp, 9, "%2dC", a1->temp);
    }
    tailsprintf(buf, len, " %2d:%2d/%3d %s", a1->chain_id, a1->num_active_chips, a1->num_cores, a1->temp == 0 ? "   " : temp);
}

static struct api_data *A1_api_stats(struct cgpu_info *cgpu)
{
	struct A1_chain *t1 = cgpu->device_data;
	unsigned long long int chipmap = 0;
	struct api_data *root = NULL;
	char s[32];
	int i;
	int fan_speed = g_fan_cfg.fan_speed;

	ROOT_ADD_API(int, "Chain ID", t1->chain_id, false);
	ROOT_ADD_API(int, "Num chips", t1->num_chips, false);
	ROOT_ADD_API(int, "Num cores", t1->num_cores, false);
	ROOT_ADD_API(int, "Num active chips", t1->num_active_chips, false);
	ROOT_ADD_API(int, "Chain skew", t1->chain_skew, false);
	ROOT_ADD_API(double, "Temp max", cgpu->temp_max, false);
	ROOT_ADD_API(double, "Temp min", cgpu->temp_min, false);
	for (i = 0; i < 4; i++) {
		sprintf(s, "Temp prewarn %d", i);
		ROOT_ADD_API(int, s, cgpu->temp_prewarn[i], false);
	}
	ROOT_ADD_API(int, "Fan duty", fan_speed, true);
    //ROOT_ADD_API(bool, "FanOptimal", g_fan_ctrl.optimal, false);
	ROOT_ADD_API(int, "iVid", t1->vid, false);
	ROOT_ADD_API(bool, "VidOptimal", t1->VidOptimal, false);
	ROOT_ADD_API(bool, "pllOptimal", t1->pllOptimal, false);
	ROOT_ADD_API(int, "Chain num", cgpu->chainNum, false);
	ROOT_ADD_API(double, "MHS av", cgpu->mhs_av, false);
	ROOT_ADD_API(bool, "Disabled", t1->disabled, false);
	for (i = 0; i < t1->num_chips; i++) {
		if (!t1->chips[i].disabled)
			chipmap |= 1 << i;
	}
	sprintf(s, "%Lx", chipmap);
	ROOT_ADD_API(string, "Enabled chips", s[0], true);
	ROOT_ADD_API(double, "Temp", cgpu->temp, false);
	ROOT_ADD_API(int, "Last temp time", t1->last_temp_time, false);

	for (i = 0; i < t1->num_chips; i++) {
		sprintf(s, "%02d HW errors", i);
		ROOT_ADD_API(int, s, t1->chips[i].hw_errors, false);
		sprintf(s, "%02d Stales", i);
		ROOT_ADD_API(int, s, t1->chips[i].stales, false);
		sprintf(s, "%02d Nonces found", i);
		ROOT_ADD_API(int, s, t1->chips[i].nonces_found, false);
		sprintf(s, "%02d Nonce ranges", i);
		ROOT_ADD_API(int, s, t1->chips[i].nonce_ranges_done, false);
		sprintf(s, "%02d Cooldown", i);
		ROOT_ADD_API(int, s, t1->chips[i].cooldown_begin, false);
		sprintf(s, "%02d Fail count", i);
		ROOT_ADD_API(int, s, t1->chips[i].fail_count, false);
		sprintf(s, "%02d Fail reset", i);
		ROOT_ADD_API(int, s, t1->chips[i].fail_reset, false);
		sprintf(s, "%02d Temp", i);
		ROOT_ADD_API(int, s, t1->chips[i].temp, false);
		sprintf(s, "%02d nVol", i);
		ROOT_ADD_API(int, s, t1->chips[i].nVol, false);
		sprintf(s, "%02d PLL", i);
		ROOT_ADD_API(int, s, t1->chips[i].pll, false);
		sprintf(s, "%02d pllOptimal", i);
		ROOT_ADD_API(bool, s, t1->chips[i].pllOptimal, false);
	}
	return root;
}

static struct api_data *A1_api_debug(struct cgpu_info *cgpu)
{
	struct A1_chain *a1 = cgpu->device_data;
	int timeout = 1000;

	g_debug_stats[a1->chain_id] = 1;

	// Wait for g_debug_stats cleared or timeout
	while (g_debug_stats[a1->chain_id] && timeout) {
		timeout -= 10;
		cgsleep_ms(10);
	}

	return A1_api_stats(cgpu);
}



struct device_drv bitmineA1_drv = {
    .drv_id = DRIVER_bitmineA1,
    .dname = "BitmineA1",
    .name = "BA1",
    .drv_detect = A1_detect,

    .hash_work = hash_queued_work,
    .scanwork = A1_scanwork,
    .queue_full = A1_queue_full,
    .flush_work = A1_flush_work,
    .get_api_stats = A1_api_stats,
    .get_api_debug = A1_api_debug,
    .get_statline_before = A1_get_statline_before,

    /* Set to lowest diff we can reliably use to get accurate hashrates
     * during tuning and initially. */
    .max_diff = DIFF_DEF,
};
