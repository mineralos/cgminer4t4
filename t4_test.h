#ifndef _T4_TEST_
#define _T4_TEST_

#define VID_TEST_NUM    1
#define VID_TEST_STEP   5

#define PLL_TEST_NUM    5
#define PLL_TEST_STEP   50


struct Test_bench {
    uint32_t uiPll; 
    uint32_t uiVid;
    uint32_t uiScore;
    uint32_t uiCoreNum;
    uint32_t uiChipNum;
};

void vid_pll_test_bench_init(uint32_t vid_start, uint32_t pll_start);

bool vid_pll_auto_detect(struct A1_chain *a1);


#endif

