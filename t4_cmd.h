#ifndef _T4_CMD_
#define _T4_CMD_

#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include "elist.h"


#define ADDR_BROADCAST      0x00

#define LEN_BIST_START      6
#define LEN_BIST_COLLECT    4
#define LEN_BIST_FIX        4
#define LEN_RESET           6
#define LEN_WRITE_JOB       94
#define LEN_READ_RESULT     4
#define LEN_WRITE_REG       18
#define LEN_READ_REG        4


#define SPI_REC_DATA_LOOP   10
#define SPI_REC_DATA_DELAY  1

//#define ASIC_REGISTER_NUM 12
#define ASIC_RESULT_LEN     38
#define READ_RESULT_LEN     (ASIC_RESULT_LEN + 2)

#define RES_LENGTH      36        
#define REG_LENGTH      12
#define JOB_LENGTH      88

#define MAX_CHAIN_LENGTH    64
#define MAX_CMD_LENGTH      (JOB_LENGTH + MAX_CHAIN_LENGTH * 2 * 2)

#define WORK_BUSY 0
#define WORK_FREE 1


struct work_ent {
    struct work *work;
    struct list_head head;
};

struct work_queue {
    int num_elems;
    struct list_head head;
};

struct A1_chip {
    uint8_t reg[12];
    int num_cores;
    int last_queued_id;
    struct work *work[4];
    /* stats */
    int hw_errors;
    int stales;
    int nonces_found;
    int nonce_ranges_done;

    /* systime in ms when chip was disabled */
    int cooldown_begin;
    /* number of consecutive failures to access the chip */
    int fail_count;
    int fail_reset;
    /* mark chip disabled, do not try to re-enable it */
    bool disabled;

    /* temp */
    int temp;
    int nVol;

    int pll;
	int cycles;
	double product; // Hashrate product of cycles / time
	bool pllOptimal; // We've stopped tuning frequency
};

struct A1_chain {
    int chain_id;
    int num_chips;
    int num_cores;
    int num_active_chips;
    int chain_skew;
    int chain_status;
    
    struct cgpu_info *cgpu;
    struct mcp4x *trimpot;
    struct spi_ctx *spi_ctx;
    struct A1_chip *chips;

    struct work_queue active_wq;
    pthread_mutex_t lock;

    /* mark chain disabled, do not try to re-enable it */
    bool disabled;
    uint8_t temp;
    int last_temp_time;
    int last_check_time;    
    int work_start_delay;

    struct timeval tvScryptLast;
    struct timeval tvScryptCurr;
    struct timeval tvScryptDiff;
    
    uint32_t vid;
    uint32_t pll;
    bool VidOptimal; // We've stopped tuning voltage
    bool pllOptimal; // We've stopped tuning frequency

    int bin1_test;
    int bin2_test;
    int bin3_test;
};

unsigned char mcompat_cmd_resetall(unsigned char chain_id, unsigned char chip_id);

unsigned char mcompat_cmd_resetjob(unsigned char chain_id, unsigned char chip_id);

void hexdump_error(char *prefix, uint8_t *buff, int len);
void hexdump(char *prefix, uint8_t *buff, int len);




#endif
