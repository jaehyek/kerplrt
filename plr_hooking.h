/*
 * Copyright (c) 2013  ElixirFlash Technology Co., Ltd.
 *              http://www.elixirflash.com/
 *
 * Powerloss Test for U-boot
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _PLR_HOOKING_H_
#define _PLR_HOOKING_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define PLR_SYSTEM  1

extern char *g_read_buf_addr;
extern char *g_write_buf_addr;
extern char *g_extra_buf_addr;
extern char *g_mmc_cond_buf_addr;
extern int mmc_fd;
extern int sd_state_fd;
extern int sw_reset_fd;

#define READ_BUF_ADDR           g_read_buf_addr
#define EXTRA_BUF_ADDR          g_extra_buf_addr
#define WRITE_BUF_ADDR          g_write_buf_addr
#define MMC_COND_BUF_ADDR       g_mmc_cond_buf_addr
#define MMC_DATA_BUF_ADDR       g_mmc_cond_buf_addr

#define WRITE_BUF_SZ                0x01000000
#define READ_BUF_SZ                 0x00800000
//#define EXTRA_BUF_SZ                0x00200000
#define EXTRA_BUF_SZ                0x00800000

#define SDCARD_DEV_NUM          sd_state_fd
#define EMMC_DEV_NUM            mmc_fd
#define SW_RESET_CNT            sw_reset_fd
#define UFS_DEV_NUM				EMMC_DEV_NUM
#define SDCARD_PART_NUM				4
#define SDCARD_FAT_PART_NUM			1

enum {
	SPI_0,
	SPI_1,
	SPI_2,
};

int prepare_devices(void);
void close_devices(void);

void do_dcache_enable(void);
void do_dcache_disable(void);

int get_mmc_product_name(uint dev_num, char *name);
int get_mmc_total_block_count(uint dev_num);
int get_mmc_partition_info(uint dev, int part_num, unsigned int *block_start, 
								unsigned int *block_count, unsigned char *part_Id);
int get_mmc_cache_enable(uint dev_num);

int do_read(uint dev_num, uchar *data, uint start_sector, uint len);
int do_write(uint dev_num, uchar *data, uint start_sector, uint len);
int do_erase(uint dev_num, uint start_sector, uint len);
int do_erase_for_checking(uint dev_num, uint start_sector, uint len);

int do_cache_flush(uint dev_num);
int do_cache_ctrl(uint dev_num, uint enable);

//jj
int do_mmc_suspend(uint dev_num, int wake_time);
//endif
// joys,2014.11.18 -----------------------------------------------
//int do_mmc_sleep(uint dev_num, int b_with_vcc_off);
int do_mmc_sleep(uint dev_num);
int do_mmc_awake(uint dev_num);
int do_mmc_init_awake(uint dev_num);
// ---------------------------------------------------------------

// joys,2014.12.01 ---------------------------------------------
int do_mmc_poweroff_notify(uint dev_num, int notify_type);
// -------------------------------------------------------------

// joys,2015.08.27 BKOPS & HPI power off -----------------
int do_bkops_test_enable(uint dev_num, int b_enable);
int do_hpi_test_enable(uint dev_num, int b_enable);
int do_get_bkops_enable(uint dev_num);
int do_get_hpi_enable(uint dev_num);
int do_get_bkops_urgent_caused(uint dev_num);
// -------------------------------------------------------

int do_get_hpi_out_of_int_time(uint dev_num);
int do_get_poweroff_notify_timeout(uint dev_num, int notify_type);
int do_get_s_a_timeout(uint dev_num);

// joys,2015.07.13 Erase power off --------------------------------------------
int do_erase_for_test(uint dev_num, uint start_sector, uint len, int type);
// ----------------------------------------------------------------------------

int get_mmc_packed_enable(uint dev_num);
int do_packed_add_list(int dev_num, ulong start, lbaint_t blkcnt, void*src, int rw);
int do_packed_send_list(int dev_num);
void* do_packed_create_buff(int dev_num, void *buff);
int do_packed_delete_buff(int dev_num);
struct mmc_packed* do_get_packed_info(int dev_num);
uint get_packed_count(int dev_num);
uint get_packed_max_sectors(int dev_num, int rw);
// joys,2015.06.19 for internal poff
int do_send_internal_info(int dev_num, void * internal_info);
int do_internal_power_control(int dev_num, bool b_power);
int get_erase_count(void);

u32 get_tick_count(void);
void reset_tick_count(int enable);
u64 get_tick_count64(void);
void reset_tick_count64(void);
u32 get_rtc_time(void);
u32 get_cpu_timer (ulong base);
void reset_board(ulong ignored);

void ina266_init(void);
int ina266_read_power(int *pValue);
int ina266_read_current(int *pValue);

void udelay(unsigned long usec);
int run_command (const char *cmd, int flag);
int console_init_r(void);
int tstc(void);
extern int run_command (const char *cmd, int flag);

#endif
