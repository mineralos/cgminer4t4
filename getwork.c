/*
 * Copyright 2011-2017 Con Kolivas
 * Copyright 2011-2015 Andrew Smith
 * Copyright 2010 Jeff Garzik
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <jansson.h>
#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
# include <sys/prctl.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netdb.h>
#include <fcntl.h>
#include <sched.h>

#include "miner.h"
#include "elist.h"
#include "compat.h"
#include "util.h"

#include "t4_common.h"

typedef unsigned char uchar;

pthread_mutex_t g_status_lock;
unsigned char g_last_job_id[8][64];
bool g_status_Invalid_job[8] = {false, false, false, false, false, false, false, false};

char rpc2_id[3][64];
char *rpc2_blob = NULL;
size_t rpc2_bloblen = 0;
uint32_t rpc2_target = 0;
char *rpc2_job_id = NULL;

double *thr_hashrates;

extern pthread_mutex_t sshare_lock;
extern pthread_mutex_t stats_lock;
extern pthread_mutex_t rpc2_job_lock;

//extern int opt_shares;
extern int opt_n_threads;
extern bool opt_getwork;
extern bool opt_showdiff;
extern double stratum_diff;
extern unsigned int work_block;

extern struct work *make_work(void);
extern struct stratum_share *stratum_shares;

extern int total_staged(void);

extern void restart_threads(void);
extern struct cgpu_info *get_thr_cgpu(int thr_id);
extern void show_hash(struct work *work, char *hashshow);
extern void enable_pool(struct pool *pool);

extern bool pool_tset(struct pool *pool, bool *var);
extern bool hash_push(struct work *work);

extern bool cnx_needed(struct pool *pool);
extern bool setup_stratum_socket(struct pool *pool);
extern bool parse_stratum_response(struct pool *pool, char *s);
extern bool supports_resume(struct pool *pool);
extern void wait_lpcurrent(struct pool *pool);

extern bool test_work_current(struct work *work);

void hexdump(char *prefix, uint8_t *buff, int len);
void hashmeter(int thr_id, uint64_t hashes_done);

extern void set_cgpu_init_value(struct cgpu_info *cgpu);


bool jobj_binary(const json_t *obj, const char *key, void *buf, size_t buflen)
{
    const char *hexstr;
    json_t *tmp;

    tmp = json_object_get(obj, key);
    if (unlikely(!tmp)) {
        applog(LOG_ERR, "JSON key '%s' not found", key);
        return false;
    }
    hexstr = json_string_value(tmp);
    if (unlikely(!hexstr)) {
        applog(LOG_ERR, "JSON key '%s' is not a string", key);
        return false;
    }
    if (!hex2bin((uchar*) buf, hexstr, buflen))
        return false;

    return true;
}


bool rpc2_job_decode(const json_t *job, struct work *work)
{
    json_t *tmp;
    uint32_t target = 0;
    
    tmp = json_object_get(job, "job_id");
    if (!tmp) {
        applog(LOG_ERR, "JSON invalid job id");
        goto err_out;
    }
    const char *job_id = json_string_value(tmp);
    
    tmp = json_object_get(job, "blob");
    if (!tmp) {
        applog(LOG_ERR, "JSON invalid blob");
        goto err_out;
    }
    const char *hexblob = json_string_value(tmp);
    size_t blobLen = strlen(hexblob);
    
    if (blobLen % 2 != 0 || ((blobLen / 2) < 40 && blobLen != 0) || (blobLen / 2) > 128) {
        applog(LOG_ERR, "JSON invalid blob length");
        goto err_out;
    }
    
    if (blobLen != 0) {
        pthread_mutex_lock(&rpc2_job_lock);
        uchar *blob = (uchar*) malloc(blobLen / 2);
        if (!hex2bin(blob, hexblob, blobLen / 2)) {
            applog(LOG_ERR, "JSON invalid blob");
            pthread_mutex_unlock(&rpc2_job_lock);
            goto err_out;
        }
        rpc2_bloblen = blobLen / 2;
        if (rpc2_blob) free(rpc2_blob);
        rpc2_blob = (char*) malloc(rpc2_bloblen);
        if (!rpc2_blob)  {
            applog(LOG_ERR, "RPC2 OOM!");
            goto err_out;
        }
        memcpy(rpc2_blob, blob, blobLen / 2);
        free(blob);

        jobj_binary(job, "target", &target, 4);
        if(rpc2_target != target)
                {
           double hashrate = 0.0;
           /*
                   pthread_mutex_lock(&stats_lock);
           for (int i = 0; i < opt_n_threads; i++)
              hashrate += thr_hashrates[i];
                   pthread_mutex_unlock(&stats_lock);
            */
           double diff = trunc( ( ((double)0xffffffff) / target ) );
           if ( opt_showdiff )
              // xmr pool diff can change a lot...
              applog(LOG_WARNING, "Stratum difficulty set to %g", diff);
           stratum_diff = diff;
           rpc2_target = target;
        }

        if (rpc2_job_id) free(rpc2_job_id);
        rpc2_job_id = strdup(job_id);
        pthread_mutex_unlock(&rpc2_job_lock);
    }
    
    if(work) {
        if (!rpc2_blob) {
            applog(LOG_WARNING, "Work requested before it was received");
            goto err_out;
        }
        memcpy(work->data, rpc2_blob, rpc2_bloblen);
        memset(work->target, 0xff, sizeof(work->target));

        work->target[28] = (uint8_t)((target >> 0) & 0xff);
        work->target[29] = (uint8_t)((target >> 8) & 0xff);
        work->target[30] = (uint8_t)((target >> 16) & 0xff);
        work->target[31] = (uint8_t)((target >> 24) & 0xff);

        if (work->job_id) 
            free(work->job_id);
        
        work->job_id = strdup(rpc2_job_id);

        work->work_difficulty = stratum_diff;
    }
    
    return true;

err_out:
    applog(LOG_WARNING, "%s", __func__);
    return false;
}

bool gen_getwork_work(struct pool *pool, json_t *job, bool firstjob)
{
    int i;
    struct work *work = make_work();
    struct work *work_temp;
    unsigned int start_nonce_gap;

#if 0
    if(pool != current_pool())
    {
        return false;
    }
#endif        
    work->pool = pool;
    work->stratum = true;
    work->nonce = 0;
    work->longpoll = false;
    work->getwork_mode = GETWORK_MODE_STRATUM;
    //work->work_block = work_block;
    /* Nominally allow a driver to ntime roll 60 seconds */
    work->drv_rolllimit = 60;

#if 1   
    rpc2_job_decode(job, work);
#else
    if(!rpc2_job_decode(job, work))
    {
        applog(LOG_INFO, "decode job faile");
        free(work);
        return false;
    }
#endif

    //applog(LOG_DEBUG, "job -> work");
    //applog(LOG_DEBUG, "job id : %s", work->job_id);
    //hexdump("data", work->data, 128);
    //hexdump("target", work->target, 32);
#if 0   
    //
    if(firstjob)
    {
        // back-up the job id
        memset(g_last_job_id[pool->pool_no], 0, 64);
        memcpy(g_last_job_id[pool->pool_no], work->job_id, strlen(work->job_id));
    }
    else
    {
        if(strncmp(g_last_job_id[pool->pool_no], work->job_id, strlen(work->job_id)) == 0)
        {
            applog(LOG_INFO, "get a duplicate job");
            free(work);
            return false;
        }

        // back-up the job id
        memset(g_last_job_id[pool->pool_no], 0, 64);
        memcpy(g_last_job_id[pool->pool_no], work->job_id, strlen(work->job_id));
    }
#endif

    pool->works++;
    pool->getwork_requested++;
    total_getworks++;

    if (total_devices != 0)
        start_nonce_gap = (256+total_devices-1)/total_devices;
    
    for(i = 0; i < total_devices; i++)
    {
        usleep(35000);
        work_temp = copy_work(work);
        local_work++;
        if (pool->is_nicehash_pool == false)
            work_temp->data[42] = (start_nonce_gap * i);
        else
            work_temp->data[41] = (start_nonce_gap * i);
        cgtime(&work_temp->tv_staged);
        hash_push(work_temp);        
    }
   
    free(work);
    
    return true;
}

void share_getwork_result(const json_t *val, struct stratum_share *sshare)
{
    struct work *work = sshare->work;
    struct pool *pool = work->pool;
    struct cgpu_info *cgpu;
    json_t *err_val, *res_val, *code_val;
    bool resubmit = false;
    const char *status;
    const char *message;
    double reject_rate;
    int64_t reject, accept;

    cgpu = get_thr_cgpu(work->thr_id);
    
    res_val = json_object_get(val, "result");
    if (res_val) 
    {
        mutex_lock(&stats_lock);
        cgpu->accepted++;
        total_accepted++;
        pool->accepted++;
        cgpu->diff_accepted += work->work_difficulty;
        total_diff_accepted += work->work_difficulty;
        pool->diff_accepted += work->work_difficulty;
        mutex_unlock(&stats_lock);

        pool->seq_rejects = 0;
        cgpu->last_share_pool = pool->pool_no;
        cgpu->last_share_pool_time = time(NULL);
        cgpu->last_share_diff = work->work_difficulty;
        pool->last_share_time = cgpu->last_share_pool_time;
        pool->last_share_diff = work->work_difficulty;
        //applog(LOG_DEBUG, "PROOF OF WORK RESULT: true (yay!!!)");
        
        applog(LOG_DEBUG, "Accepted %s %d pool %d", cgpu->drv->name, cgpu->device_id, pool->pool_no);
#if 0
        /* Detect if a pool that has been temporarily disabled for
         * continually rejecting shares has started accepting shares.
         * This will only happen with the work returned from a
         * longpoll */
        err_val = json_object_get(val, "error");
        code_val = json_object_get(err_val, "code");
        if (!code_val) {
            applog(LOG_WARNING, "Rejecting pool %d now accepting shares, re-enabling!", pool->pool_no);
            enable_pool(pool);
            switch_pools(NULL);
        }
#endif      
        /* If we know we found the block we know better than anyone
         * that new work is needed. */
        if (unlikely(work->block))
            restart_threads();
    }
    else 
    {
        mutex_lock(&stats_lock);
        cgpu->rejected++;
        total_rejected++;
        pool->rejected++;
        cgpu->diff_rejected += work->work_difficulty;
        total_diff_rejected += work->work_difficulty;
        pool->diff_rejected += work->work_difficulty;
        pool->seq_rejects++;
        mutex_unlock(&stats_lock);

        //applog(LOG_DEBUG, "PROOF OF WORK RESULT: false (booooo)");

        err_val = json_object_get(val, "error");
        if(err_val)
        {
            message = json_string_value(json_object_get(err_val, "message"));
            applog(LOG_NOTICE, "Rejected %s %d pool %d:%s", 
                cgpu->drv->name, cgpu->device_id, work->pool->pool_no, message);            
#if 0           
            // receive the first Invalid job id, send get job
            if(strstr(message, "Invalid job id") != NULL) 
            {
                applog(LOG_DEBUG, "g_status : %d", g_status_Invalid_job[pool->pool_no]);
                if(!g_status_Invalid_job[pool->pool_no])
                {
                    char str[128];
                    memset(str, 0, sizeof(str));
                    //sprintf(str, "{\"id\": 8, \"jsonrpc\": \"2.0\", \"method\": \"getjob\", \"params\": {}}");
                    sprintf(str, "{\"id\": 8, \"jsonrpc\": \"2.0\", \"method\": \"getjob\", \"params\": {\"id\": \"%s\"}}", rpc2_id);
                    stratum_send(pool, str, strlen(str));

                    g_status_Invalid_job[pool->pool_no] = true;
                }
            }
#endif
        }

    }
    
    mutex_lock(&stats_lock);
    reject = total_rejected;
    accept = total_accepted;
    mutex_unlock(&stats_lock);
    
    reject_rate = (double)(reject * 100) / (double)(accept + reject);
    applog(LOG_INFO, "Rejected: %lld, Accepted: %lld, Reject ratio: %.1f%%", reject, accept, reject_rate);
}


bool parse_getwork_response(struct pool *pool, const json_t *val)
{
    json_t *err_val, *res_val, *id_val;
    struct stratum_share *sshare;
    json_error_t err;
    bool ret = false;
    int id;
    
    //err_val = json_object_get(val, "error");
    id_val = json_object_get(val, "id");
/*
    if (json_is_null(id_val) || !id_val) {
        char *ss;

        if (err_val)
            ss = json_dumps(err_val, JSON_INDENT(3));
        else
            ss = strdup("(unknown reason)");

        applog(LOG_INFO, "JSON-RPC non method decode failed: %s", ss);

        free(ss);

        goto out;
    }
*/
    id = json_integer_value(id_val);

    //applog(LOG_INFO, "get response form pool%d, id:%d", pool->pool_no, id);
    
    mutex_lock(&sshare_lock);
    HASH_FIND_INT(stratum_shares, &id, sshare);
    if (sshare) {
        HASH_DEL(stratum_shares, sshare);
        pool->sshares--;
    }
    mutex_unlock(&sshare_lock);

    if (!sshare) {
        double pool_diff;

        res_val = json_object_get(val, "result");
        /* Since the share is untracked, we can only guess at what the
         * work difficulty is based on the current pool diff. */
        cg_rlock(&pool->data_lock);
        pool_diff = pool->sdiff;
        cg_runlock(&pool->data_lock);

        if (res_val) {
            applog(LOG_DEBUG, "Accepted untracked stratum share from pool %d", pool->pool_no);
            
            /* We don't know what device this came from so we can't
             * attribute the work to the relevant cgpu */
            mutex_lock(&stats_lock);
            total_accepted++;
            pool->accepted++;
            total_diff_accepted += pool_diff;
            pool->diff_accepted += pool_diff;
            mutex_unlock(&stats_lock);
        } else {            
            applog(LOG_DEBUG, "Rejected untracked stratum share from pool %d", pool->pool_no);          
            mutex_lock(&stats_lock);
            total_rejected++;
            pool->rejected++;
            total_diff_rejected += pool_diff;
            pool->diff_rejected += pool_diff;
            mutex_unlock(&stats_lock);
        }
        goto out;
    }
    
    share_getwork_result(val, sshare);

    ret = true;

out:
    return ret;
}


int gen_getwork_revdata(char *in, char *out)
{
    int len;
    char *pstr = in + 4;
/*
    char *pstr1 = in;

    printf("receive data:<<<<<< \n");
    while((*pstr1) != 0)
    {
        printf("0x%02x ", *(pstr1++));
    }
    printf("\n");
*/   
    if((*in != 0xFE) || (*(in+1) != 0x7C))
    {
        applog(LOG_ERR, "not have head 0xFE7C");
        return -1;
    }

    len = (*(in+2) << 8) + (*(in+3));
    len = ~len;
    len = len / 29;
    
    while(*pstr != 0)
    {
        *(out++) = ~(*(pstr++));
    }

    return true;
}


bool gen_getwork_sendata(char *in, char *out)
{
    int len;
    char *pstr = out;

    *(out++) = 0xFE;
    *(out++) = 0x7C;
    
    len = strlen(in);
    len = len * 29;
    len = ~len;
    *(out++) = (len >> 8) & 0xFF;
    *(out++) = (len >> 0) & 0xFF;

    while(*in != 0)
    {
        *(out++) = ~(*(in++));
    }
/*
    printf("send data:>>>>>> \n");
    while(*pstr != 0)
    {
        printf("0x%02x ", *(pstr++));
    }
    printf("\n");
*/   
    return true;
}

bool initiate_getwork(struct pool *pool)
{
    bool ret;
    
    ret = setup_stratum_socket(pool);
    if(ret)
    {       
        pool->stratum_active = true;
        pool->stratum_notify = true;
        return true;
    }
    else
    {
        pool->stratum_active = false;
        pool->stratum_notify = false;
        return false;
    }
}


bool login_getwork(struct pool *pool)
{
    char *s, *sret;
    json_t *val, *result_val, *job_val, *error_val;
    json_error_t err;
    const char *error;
    const char *job;
    const char *id;

    s = (char*) malloc(300 + strlen(pool->rpc_user) + strlen(pool->rpc_pass));\
    if (pool->is_nicehash_pool == false)
    {
        sprintf(s, "{\"method\": \"login\", \"params\": {"
            "\"login\": \"%s\", \"pass\": \"%s\", \"agent\": \"%s\"}, \"id\": 1}\n",
            pool->rpc_user, pool->rpc_pass, "cpuminer-opt/3.6.5");
    }
    else
    {
        sprintf(s, "{\"method\": \"login\", \"jsonrpc\": \"2.0\", \"params\": {"
            "\"login\": \"%s\", \"pass\": \"%s\", \"agent\": \"%s\"}, \"id\": 1}\n",
            pool->rpc_user, pool->rpc_pass, "XMRig/2.5.2");
    }
            
    if (!stratum_send(pool, s, strlen(s))) {
        goto out;
    }
    
    while (42) {
        sret = recv_line(pool);
        if (sret == NULL) {
            applog(LOG_DEBUG, "getwork login receive data faile");
            goto out;
        }

        val = JSON_LOADS(sret, &err);
        if (!val) {
            applog(LOG_ERR, "getwork login JSON decode failed1(%d): %s", err.line, err.text);
            goto out;
        }
        
        // receive the job
        result_val = json_object_get(val, "result");
        if (!result_val){
            applog(LOG_ERR, "json_object_get(result) false");
            goto out;
        }

        id = json_string_value(json_object_get(result_val, "id"));
        if(!id) {
            applog(LOG_ERR, "rpc2 id is not a string");
            goto out;
        }
        memset(&rpc2_id[pool->pool_no], 0, 64);
        memcpy(&rpc2_id[pool->pool_no], id, 64);
#if (!FIRST_JOB_DUPLICATE_WITH_LOGIN_JOB)       /* Disabled only when the first job is duplicated with the login return job */
        job_val = json_object_get(result_val, "job");
        if (job_val) {
            gen_getwork_work(pool, job_val, true);                          
        }   
#endif
        free(s);
        return true;
    }

out:
    free(s);
    return false;
}

bool restart_getwork(struct pool *pool)
{
    bool ret = false;

    if (pool->stratum_active)
        suspend_stratum(pool);
    if (!initiate_getwork(pool))
        goto out;
    if (!login_getwork(pool))
        goto out;
    ret = true;
out:
    if (!ret)
        pool_died(pool);
    else
        stratum_resumed(pool);
    return ret;
}

static void *getwork_sthread(void *userdata)
{
    struct pool *pool = (struct pool *)userdata;
    uint64_t last_nonce2 = 0;
    uint32_t last_nonce = 0;
    char threadname[16];
    int i;

    pthread_detach(pthread_self());

    snprintf(threadname, sizeof(threadname), "%d/SGetwork", pool->pool_no);
    applog(LOG_DEBUG, "creat thread %s", threadname);
    RenameThread(threadname);
    
    pool->stratum_q = tq_new();
    if (!pool->stratum_q)
        quit(1, "Failed to create stratum_q in getwork_sthread");   

    while (42) {
        char s[1024];
        char noncehex[12]; 
        char hashhex[128]; 
        struct stratum_share *sshare;
        struct work *work;
        bool submitted;

        struct timespec then;
        struct timeval now;
                    
        if (unlikely(pool->removed)){
            applog(LOG_DEBUG, "pool->removed is %d", pool->removed);
            break;
        }

        work = tq_pop(pool->stratum_q, NULL);
        if (unlikely(!work))
            quit(1, "Stratum q returned empty work");
        
        __bin2hex(noncehex, (const unsigned char *)&(work->nonce), 4);
        __bin2hex(hashhex, (const unsigned char *)(work->hash), 32);
        
        sshare = cgcalloc(sizeof(struct stratum_share), 1);
        submitted = false;
        
        sshare->sshare_time = time(NULL);
        //sshare->id = pool->sshares;
        
        /* This work item is freed in parse_stratum_response */
        sshare->work = work;
        memset(s, 0, sizeof(s));

        mutex_lock(&sshare_lock);
        /* Give the stratum share a unique id */
        sshare->id = swork_id++;
        mutex_unlock(&sshare_lock);

        if (pool->is_nicehash_pool == false)
        {
            sprintf( s, "{\"method\": \"submit\", \"params\": "
                "{\"id\": \"%s\", \"job_id\": \"%s\", \"nonce\": \"%s\", \"result\": \"%s\"},"
                "\"id\":%d}\n", rpc2_id[pool->pool_no], work->job_id, noncehex, hashhex, sshare->id);
        }
        else
        {
            sprintf( s, "{\"method\": \"submit\",  \"jsonrpc\": \"2.0\", \"params\": "
                "{\"id\": \"%s\", \"job_id\": \"%s\", \"nonce\": \"%s\", \"result\": \"%s\"},"
                "\"id\":%d}\n", rpc2_id[pool->pool_no], work->job_id, noncehex, hashhex, sshare->id);
        }
        
        /* Try resubmitting for up to 2 minutes if we fail to submit
         * once and the stratum pool nonce1 still matches suggesting
         * we may be able to resume. */
        while (time(NULL) < sshare->sshare_time + 120) {
            bool sessionid_match;
            
            if (likely(stratum_send(pool, s, strlen(s)))) {
                mutex_lock(&sshare_lock);
                HASH_ADD_INT(stratum_shares, id, sshare);
                pool->sshares++;
                mutex_unlock(&sshare_lock);

                if (pool_tclear(pool, &pool->submit_fail)) {
                        applog(LOG_WARNING, "Pool %d communication resumed, submitting work", pool->pool_no);
                }
                submitted = true;
                
                break;
            }
                        
            if (!pool_tset(pool, &pool->submit_fail) && cnx_needed(pool)) {
                applog(LOG_WARNING, "Pool %d stratum share submission failure", pool->pool_no);
                total_ro++;
                pool->remotefail_occasions++;
            }
            
            if (opt_lowmem) {
                applog(LOG_DEBUG, "Lowmem option prevents resubmitting stratum share");
                break;
            }

            cg_rlock(&pool->data_lock);
            sessionid_match = (pool->nonce1 && !strcmp(work->nonce1, pool->nonce1));
            cg_runlock(&pool->data_lock);
            
            if (!sessionid_match) {
                applog(LOG_DEBUG, "No matching session id for resubmitting stratum share");
                break;
            }
            /* Retry every 5 seconds */
            sleep(5);
        }

        if (unlikely(!submitted)) {
            applog(LOG_DEBUG, "Failed to submit stratum share, discarding");
            free_work(work);
            free(sshare);
            pool->stale_shares++;
            total_stale++;
        } else {
            int ssdiff;

            sshare->sshare_sent = time(NULL);
            ssdiff = sshare->sshare_sent - sshare->sshare_time;
            if (opt_debug || ssdiff > 0) {
                //applog(LOG_INFO, "Pool %d stratum share submission lag time %d seconds", pool->pool_no, ssdiff);
            }
        }
    }
    
    /* Freeze the work queue but don't free up its memory in case there is
     * work still trying to be submitted to the removed pool. */
    tq_freeze(pool->stratum_q);

    return NULL;
}


static int s_pool_error_cnt;
static void *getwork_rthread(void *userdata)
{
    struct pool *pool = (struct pool *)userdata;
    char threadname[16];
    json_t *val, *params;
    json_t *result_val;
    json_t *method_val;
    json_t *job_val;
    json_error_t err;
    const char *method;
    const char *result;
    //char str[256];

    pthread_detach(pthread_self());

    snprintf(threadname, sizeof(threadname), "%d/RGetwork", pool->pool_no);
    RenameThread(threadname);
    
    s_pool_error_cnt = 0;

    //memset(str, 0, sizeof(str));
    //sprintf(str, "{\"id\": 8, \"jsonrpc\": \"2.0\", \"method\": \"getjob\", \"params\": {}}");    
    
    while (42) {
        struct timeval timeout;
        int sel_ret;
        fd_set rd;
        char *s;
    
        if (unlikely(pool->removed)) {
            suspend_stratum(pool);
            break;
        }
#if 0
        if(pool != current_pool())
        {
            sleep(15);
            continue;
        }
#endif
        /* Check to see whether we need to maintain this connection
         * indefinitely or just bring it up when we switch to this
         * pool */
        if (!sock_full(pool) && !cnx_needed(pool)) {
            suspend_stratum(pool);
            clear_stratum_shares(pool);
            clear_pool_work(pool);

            wait_lpcurrent(pool);
            while (!restart_getwork(pool)) {
                pool_died(pool);
                if (pool->removed)
                    goto out;
                cgsleep_ms(5000);
            }
        }

        FD_ZERO(&rd);
        FD_SET(pool->sock, &rd);
        timeout.tv_sec = 90;
        timeout.tv_usec = 0;

        /* The protocol specifies that notify messages should be sent
         * every minute so if we fail to receive any for 90 seconds we
         * assume the connection has been dropped and treat this pool
         * as dead */
        if (!sock_full(pool) && (sel_ret = select(pool->sock + 1, &rd, NULL, NULL, &timeout)) < 1) {
            applog(LOG_INFO, "Stratum select failed on pool %d with value %d", pool->pool_no, sel_ret);
            s = NULL;
        } else {
            //applog(LOG_DEBUG, "recv_line in getwork_rthread");
            s = recv_line(pool);
        }

        if (!s) {
            applog(LOG_NOTICE, "Stratum connection to pool %d interrupted", pool->pool_no);
            pool->getfail_occasions++;
            total_go++;

            /* If the socket to our stratum pool disconnects, all
             * tracked submitted shares are lost and we will leak
             * the memory if we don't discard their records. */
            if (!supports_resume(pool) || opt_lowmem)
            {
                clear_stratum_shares(pool);
            }
            clear_pool_work(pool);
            if (pool == current_pool())
            {
                restart_threads();
            }

            while (!restart_getwork(pool)) 
            {
                pool_died(pool);
                if (pool->removed)
                {
                    goto out;
                }
                
                s_pool_error_cnt++;
                if(s_pool_error_cnt > (4 * total_pools))
                {
                    mcompat_chain_power_down_all();
                    exit(1);
                }
               
                cgsleep_ms(5000);
            }
            
            continue;
        }

        /* Check this pool hasn't died while being a backup pool and
         * has not had its idle flag cleared */
        stratum_resumed(pool);

        val = JSON_LOADS(s, &err);
        if (!val) 
        {
            applog(LOG_ERR, "getwork receive JSON decode failed(%d): %s", err.line, err.text);
            free(s);
            continue;
        }

        method_val = json_object_get(val, "method");
        if(method_val)
        {
            if(strcmp(json_string_value(method_val), "job") == 0)
            {
                json_t *params = json_object_get(val, "params");

                //applog(LOG_DEBUG, "receive a method job");
                gen_getwork_work(pool, params, false);
#if 0
                applog(LOG_DEBUG, "g_status : %d", g_status_Invalid_job[pool->pool_no]);
                if(g_status_Invalid_job[pool->pool_no])
                {
                    g_status_Invalid_job[pool->pool_no] = false;
                }
#endif
                free(s);
                continue;
            }
            else
            {
                applog(LOG_DEBUG, "the method string is not job [%s]", json_string_value(method_val));
            }
        }
        else
        {
            applog(LOG_DEBUG, "not have method key");
        }

        result_val = json_object_get(val, "result");
        if (result_val)
        {
            job_val = json_object_get(result_val, "job");
            if(job_val)
            {
                applog(LOG_DEBUG, "receive a result job");
                gen_getwork_work(pool, job_val, false);
#if 0
                applog(LOG_DEBUG, "g_status : %d", g_status_Invalid_job[pool->pool_no]);
                if(g_status_Invalid_job[pool->pool_no])
                {
                    g_status_Invalid_job[pool->pool_no] = false;
                }
#endif
                free(s);
                continue;
            }           
        }
        else
        {
            applog(LOG_DEBUG, "not have result key");
        }

        applog(LOG_DEBUG, "receive a responses for submit");
        parse_getwork_response(pool, val);
        free(s);
    }

out:

    return NULL;

}

static void init_getwork_threads(struct pool *pool)
{
    have_longpoll = true;

    if (unlikely(pthread_create(&pool->stratum_sthread, NULL, getwork_sthread, (void *)pool)))
        quit(1, "Failed to create stratum sthread");
    if (unlikely(pthread_create(&pool->stratum_rthread, NULL, getwork_rthread, (void *)pool)))
        quit(1, "Failed to create stratum rthread");
}

bool pool_active_getwork(struct pool *pool, bool pinging)
{
    if(!opt_getwork){
        applog(LOG_INFO, "in pool_active_getwork() opt_getwork is false");
        return false;
    }
    
    applog(LOG_DEBUG, "rpc_url=[%s], active=%d", pool->rpc_url, pool->stratum_active);
    //if (pool->has_stratum) 
    {
        /* We create the stratum thread for each pool just after
         * successful authorisation. Once the init flag has been set
         * we never unset it and the stratum thread is responsible for
         * setting/unsetting the active flag */
        bool init = pool_tset(pool, &pool->stratum_init);

        if (!init) {
            bool ret; 
            ret = initiate_getwork(pool);
            
            if ( strstr(pool->rpc_url, NICEHASH_POOL_URL_STR) )
                pool->is_nicehash_pool = true;
            else
                pool->is_nicehash_pool = false;

            ret = login_getwork(pool);

            if (ret) {
                //applog(LOG_DEBUG, "enter init_getwork_threads()");
                init_getwork_threads(pool);
            }
            else {
                //applog(LOG_DEBUG, "in pool_active_getwork() pool_tclear");
                pool_tclear(pool, &pool->stratum_init);
            }
            
            return ret;
        }

        return pool->stratum_active;
    }

    return true;
}

bool submit_nonce_hash(struct thr_info *thr, struct work *work, uint8_t *nonce, uint8_t *hash)
{
    //hexdump("hash", hash, 32);
    //hexdump("target", work->target, 32);

    if(fulltest(hash, work->target))
    {
        work->nonce = (nonce[0] << 24) + (nonce[1] << 16) + (nonce[2] << 8) + (nonce[3] << 0);
        memcpy(work->hash, hash, 32);
        
        submit_tested_work(thr, work);
        return true;
    }
    else 
    {
#if 0
        inc_hw_errors(thr);
        return false;
#else
        return true;
#endif
    }
}


void scan_nonce_hash(struct thr_info *thr)
{
    struct timeval tv_start = {0, 0}, tv_end;
    struct cgpu_info *cgpu = thr->cgpu;
    struct device_drv *drv = cgpu->drv;
    const int thr_id = thr->id;
    int64_t hashes_done = 0;

    set_cgpu_init_value(cgpu);
    while(42) {
        struct timeval diff;
        int64_t hashes;
        
        hashes = drv->scanwork(thr);
        
        thr->work_restart = false;
        
        if (unlikely(hashes == -1 )) {
            applog(LOG_ERR, "%s %d failure, disabling!", drv->name, cgpu->device_id);
            cgpu->deven = DEV_DISABLED;
            dev_error(cgpu, REASON_THREAD_ZERO_HASH);
            break;
        }
        
        hashes_done += hashes;
        cgtime(&tv_end);
        timersub(&tv_end, &tv_start, &diff);
        /* Update the hashmeter at most 5 times per second */
        if ((hashes_done && (diff.tv_sec > 0 || diff.tv_usec > 200000)) ||
            diff.tv_sec >= opt_log_interval) {
            hashmeter(thr_id, hashes_done);
            hashes_done = 0;
            copy_time(&tv_start, &tv_end);
        }

        usleep(30000);
    }
}

