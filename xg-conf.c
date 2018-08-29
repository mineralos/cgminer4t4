/*
 * Copyright 2011 Kano
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

/* Compile:
 *   gcc api-example.c -Icompat/jansson-2.9/src -o cgminer-api
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "compat.h"
#include "util.h"


#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <jansson.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "mcompat_aes.h"


extern char* mcompat_arg_printe(char *arg, int len);

//#define __DEBUG

#ifdef __DEBUG
#define DEBUG(format, ...) printf(format, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif


#define INVALID_NUM     (-99)

#define JSON_LOAD_ERROR "JSON decode of file '%s' failed\n %s"
#define JSON_LOAD_ERROR_LEN strlen(JSON_LOAD_ERROR)

#define CGMINER_CONF_FILE "/config/cgminer.conf"
#define ENCRYPTED_CONF_FILE "/config/cg.conf"
#define ENCRYPTED_TEMP_FILE "/config/cg.conf.temp"




struct pool_config {
    char pool_url[512];
    char pool_user[512];
    char pool_pass[512];
};

struct pool_config g_pool[3];


/***  Read out cgminer.conf file ***/
const char pool_key_array[][6] = {"url", "user", "pass"};

static char *parse_one_pool_object(json_t *array, int pool_num)
{
    int i, key_num;
    const char *key;
    json_t *val;
    const char *str;
    
    key_num = sizeof(pool_key_array)/sizeof(pool_key_array[0]);
    DEBUG("%s, key_num %d\n", __func__, key_num);
    for (i=0; i<key_num; i++)
    {
        key = pool_key_array[i];
        val = json_object_get(array, key);
        if (json_is_string(val))
        {
            str = json_string_value(val);
            if (strcmp(key, "url") == 0)
                strcpy(g_pool[pool_num].pool_url, str);
            else if (strcmp(key, "user") == 0)
                strcpy(g_pool[pool_num].pool_user, str);
            else if (strcmp(key, "pass") == 0)
                strcpy(g_pool[pool_num].pool_pass, str);
            else
            {
                DEBUG("%s, Invalid key %s\n", __func__, key);
                return -1;
            }

            DEBUG("%s, key %s, value %s\n", __func__, key, str);
        }
        else
        {
            DEBUG("%s, found invalid value (key %s)\n", __func__, key);
            return -1;
        }
    }
    
    return 0;
}


static char *parse_pools_array(json_t *config)
{
    int ret = 0;
    json_t *val;
    
    val = json_object_get(config, "pools");
    if (json_is_array(val))
    {
        json_t *arr_val;
        size_t index;

        json_array_foreach(val, index, arr_val) {
            if (json_is_object(arr_val))
                ret = parse_one_pool_object(arr_val, index);
        }
    }
    else
    {
        DEBUG("%s, No pools JSON array found\n", __func__);
        return -1;
    }
    
    return ret;
}


static int print_result(void)
{
    DEBUG("Pool 1: %s %s %s\n", g_pool[0].pool_url, g_pool[0].pool_user, g_pool[0].pool_pass);
    DEBUG("Pool 2: %s %s %s\n", g_pool[1].pool_url, g_pool[1].pool_user, g_pool[1].pool_pass);
    DEBUG("Pool 3: %s %s %s\n", g_pool[2].pool_url, g_pool[2].pool_user, g_pool[2].pool_pass);
}

static int load_conf_file(void)
{
    json_t *config;
    json_error_t err;
    int ret;
    
    config = json_load_file(CGMINER_CONF_FILE, 0, &err);
    if (!json_is_object(config)) {
        DEBUG("%s, %s %s %s\n", __func__, CGMINER_CONF_FILE, JSON_LOAD_ERROR, err.text);
        return -1;
    }

    memset(g_pool, 0, sizeof(g_pool));
    ret = parse_pools_array(config);
    return ret;
}





/** Generate encrypted pool config file **/
static int encrypted(char *strkey, char *src, int len, char *dest)
{
    char str1[512];
    int tmp_len, i;

    memset(str1, 0, sizeof(str1));
    AES_Encrypt(strkey, src, len, str1);

    if((len & 0x0f) == 0) {
        tmp_len = len;
    } else {
        tmp_len = (len & 0xfff0) + 0x10;
    }

    for (i=0; i<tmp_len; i++)
    {
        sprintf(&dest[i*2], "%02x", str1[i]);
    }

    return 0;
}


#define BUF_SIZE            8192
#define POOL_NUM          3
const char * key = "NTBO^pac./01WU@S";
static int gen_encrypted_buf(void)
{
    int i;
    char *strkey;
    char str2[512];
    int len;
    char *buf;

    if ( (strlen(g_pool[0].pool_url) == 0)
        && (strlen(g_pool[1].pool_url) == 0)
        && (strlen(g_pool[2].pool_url) == 0) )
    {
        DEBUG("%s, no pool info\n", __func__);
        return NULL;
    }

    buf = malloc(BUF_SIZE);
    if (buf == NULL)
    {
        DEBUG("%s, malloc failed %d, %s\n", __func__, errno, strerror(errno));
        return NULL;
    }

    memset(buf, 0, BUF_SIZE);
    strcat(buf, "A0000\r\n");

    strkey = mcompat_arg_printe(key, 16);
    for (i=0; i<POOL_NUM; i++)
    {
        if ( strlen(g_pool[i].pool_url) && strlen(g_pool[i].pool_user) 
            && strlen(g_pool[i].pool_pass) )
        {
            memset(str2, 0, sizeof(str2));
            encrypted(strkey, g_pool[i].pool_url, strlen(g_pool[i].pool_url), str2);
            strcat(buf, str2);
            strcat(buf, "\r\n");

            memset(str2, 0, sizeof(str2));
            encrypted(strkey, g_pool[i].pool_user, strlen(g_pool[i].pool_user), str2);
            strcat(buf, str2);
            strcat(buf, "\r\n");

            memset(str2, 0, sizeof(str2));
            encrypted(strkey, g_pool[i].pool_pass, strlen(g_pool[i].pool_pass), str2);
            strcat(buf, str2);
            strcat(buf, "\r\n");
        }
    }

    return buf;
}

int write_file(char buf[], int len)
{
    char cmd[200];
    int fd, ret;

    memset(cmd, 0, 200);
    sprintf(cmd, "rm -rf %s", ENCRYPTED_TEMP_FILE);
    system(cmd);

    fd = open(ENCRYPTED_TEMP_FILE, O_CREAT|O_TRUNC|O_RDWR, 0644);
    if (fd == -1)
    {
        DEBUG("%s, open file failed %d, %s\n", __func__, errno, strerror(errno));
        return -1;
    }

    ret = write(fd, buf, len);
    DEBUG("%s, write result %d\n", __func__, ret);
    if (ret == -1)
    {
        DEBUG("%s, write failed %d, %s\n", __func__, errno, strerror(errno));
    }
    else if (ret == len)
    {
        memset(cmd, 0, 200);
        sprintf(cmd, "mv %s %s", ENCRYPTED_TEMP_FILE, ENCRYPTED_CONF_FILE);
        system(cmd);
    }

    return ret;
}





int encrypt_conf(void)
{
    char *buf;
    int ret = -1;
    
    buf = gen_encrypted_buf();
    if (buf != NULL)
    {
        ret = write_file(buf, strlen(buf));
    }

    return ret;
}


int main(void)
{
    load_conf_file();
    print_result();

    encrypt_conf();

    return 0;
}

