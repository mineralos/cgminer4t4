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

#include "t4_test.h"



static uint32_t s_vid;
static uint32_t s_pll;

struct Test_bench Test_bench_Array[VID_TEST_NUM][PLL_TEST_NUM];

void vid_pll_test_bench_init(uint32_t vid_start, uint32_t pll_start)
{
    int i, j;
    struct Test_bench * ptest;
    
    for(i = 0; i < VID_TEST_NUM; i++)
    {
        for(j = 0; j < PLL_TEST_NUM; j++)
        {
            ptest = &Test_bench_Array[i][j];

            ptest->uiVid = vid_start + (i * VID_TEST_STEP);
            ptest->uiPll = pll_start + (j * PLL_TEST_STEP);
            ptest->uiCoreNum = 0;
            ptest->uiScore = 0;
        }
    }
}


void vid_pll_test_bench_init1(void)
{
    int i, j;
    struct Test_bench * ptest;
    
    for(i = 0; i < VID_TEST_NUM; i++)
    {
        for(j = 0; j < PLL_TEST_NUM; j++)
        {
            ptest = &Test_bench_Array[i][j];

            ptest->uiVid = 175;
            ptest->uiPll = 1000;
            ptest->uiCoreNum = 0;
            ptest->uiScore = 0;
        }
    }
}



bool vid_pll_get_core_num(struct A1_chain *a1, struct Test_bench *ptest)
{
    int i, j;
    uint8_t chip_num;
    uint8_t buffer[64];
    
    memset(buffer, 0, sizeof(buffer));
    chip_num = mcompat_cmd_bist_start(a1->chain_id, 0);
    if(chip_num == 0)
    {
        applog(LOG_WARNING, "bist start fail");
        return false;
    }    
    a1->num_chips = chip_num;
    applog(LOG_WARNING, "chain %d detected %d chips", a1->chain_id, a1->num_chips);
    sleep(1);

    if(!mcompat_cmd_bist_fix(a1, ADDR_BROADCAST))
    {
        applog(LOG_WARNING, "bist fix fail");
        return false;
    }
    usleep(10000);

    ptest->uiChipNum = a1->num_chips;
    ptest->uiCoreNum = 0;
    for (i = 0; i < ptest->uiChipNum; i++)
    {
        int core_num;
        int chip_id = i + 1;
        int cid = a1->chain_id;
        uint8_t buffer[64];
        
        memset(buffer, 0, sizeof(buffer));
        if(!mcompat_cmd_read_register(a1->chain_id, chip_id, buffer, REG_LENGTH)) 
        {
            applog(LOG_WARNING, "chain %d: Failed to read register for chip %d", cid, chip_id);
            return false;;
        }
        else
        {
            hexdump("reg", buffer, REG_LENGTH);
        }
    
        core_num = 0;
        for(j = 0; j < 8; j++)
        {
            if((buffer[11] >> j) & 0x01)
            {
                core_num++;
            }
        }

        ptest->uiCoreNum += core_num;
    }

    return true;
}

bool vid_pll_one_detect(struct A1_chain *a1, struct Test_bench *ptest)
{
    int i;
    int pllindex;
    uint8_t buffer[64];

    //mcompat_hw_reset(a1->chain_id);

    // update pll
    s_pll = ptest->uiPll;

    //set_spi_speed(1500000);    
    mcompat_set_spi_speed(a1->chain_id, 2);
    usleep(10000);
    
    if(!mcompat_cmd_resetall(a1->chain_id, ADDR_BROADCAST))
    {
        applog(LOG_WARNING, "cmd reset fail");
        return false;
    }
    usleep(1000000);

    pllindex = Ax_clock_pll2index(ptest->uiPll);
    if(!Ax_clock_setpll_by_step(a1, pllindex))
    {
        return 0;
    }

    //set_spi_speed(3250000);
    mcompat_set_spi_speed(a1->chain_id, 3);
    usleep(10000);  
    
    vid_pll_get_core_num(a1, ptest);
    
    ptest->uiScore = (ptest->uiCoreNum) * (ptest->uiPll);
    
    applog(LOG_INFO, "detect : vid=%d pll=%d chip=%d core=%d score=%d", 
           ptest->uiVid, ptest->uiPll, ptest->uiChipNum, ptest->uiCoreNum, ptest->uiScore);

    return true;
}


bool vid_pll_auto_detect(struct A1_chain *a1)
{
    int i, j;
    struct Test_bench * pTest = &Test_bench_Array[0][0];
    struct Test_bench * pTestTemp;

    s_vid = a1->vid;
    s_pll = a1->pll;
    
    for(i = 0; i < VID_TEST_NUM; i++)
    {
        for(j = 0; j < PLL_TEST_NUM; j++)
        {
            pTestTemp = &Test_bench_Array[i][j];
            
            vid_pll_one_detect(a1, pTestTemp);
            if(pTestTemp->uiScore > pTest->uiScore)
            {
                pTest = pTestTemp;
            }

            sleep(1);
        }
    }
    
    a1->vid = pTest->uiVid;
    a1->pll = pTest->uiPll;
    
    applog(LOG_INFO, "best : vid=%d pll=%d", a1->vid, a1->pll);
    return true;
}




