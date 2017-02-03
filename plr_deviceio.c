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

#include "plr_common.h"
#include "plr_errno.h"
#include "plr_err_handling.h"


#define RWRETRY 5
 
/* ------------------------------------------------
* Static Function (SD Card I/O) 
* ------------------------------------------------*/

int sdmmc_read(uchar *data, uint start_sector, uint len)
{
	return do_read(SDCARD_DEV_NUM, data, start_sector, len);
}

int sdmmc_write(uchar *data, uint start_sector, uint len)
{
	// 160126 sdcard state는 sync를 해줘야한다.
#ifndef PLR_SYSTEM
	return do_write(SDCARD_DEV_NUM, data, start_sector, len);
#else
	int ret;
	ret = do_write(SDCARD_DEV_NUM, data, start_sector, len);
	fsync(SDCARD_DEV_NUM);
	return ret;
#endif
}

/* ------------------------------------------------
* Device I/O Function
* ------------------------------------------------*/

int dd_read(uchar *data, uint start_sector, uint len)
{
	int retry = RWRETRY;
	int ret = 0;
// joys,2015.06.01. for LG Sandisk sample
	do {
		ret = do_read(EMMC_DEV_NUM, data, start_sector, len);

		if ( !ret || ret == -(INTERNAL_POWEROFF_FLAG) )
			break;

		plr_info("[%s] retry %d! start sector : 0x%08x, len : 0x%x\n", 
			__func__, retry, start_sector, len);
		
	} while(--retry);

	if (retry < 1) {
		plr_info("Read Fail LSN : 0x%08X \n", start_sector);
	}

	return ret;	
}

int dd_write(uchar *data, uint start_sector, uint len)
{
	int retry = RWRETRY;
	int ret = 0;
// joys,2015.06.01. for LG Micron sample
	do {
		ret = do_write(EMMC_DEV_NUM, data, start_sector, len);

		if ( !ret || ret == -(INTERNAL_POWEROFF_FLAG) )
			break;

		plr_info("[%s] retry %d! start sector : 0x%08x, len : 0x%x\n", 
			__func__, retry, start_sector, len);
		
	} while(--retry);

	if (retry < 1) {
		plr_info("Write Fail LSN : 0x%08X \n", start_sector);
	}

	return ret;
}

int dd_erase(uint start_sector, uint len) 
{
	return do_erase(EMMC_DEV_NUM, start_sector, len);
}

// joys,2015.07.13
int dd_erase_for_poff(uint start_sector, uint len, int type)
{
	return do_erase_for_test(EMMC_DEV_NUM, start_sector, len, type);
}

int dd_cache_flush(void)
{
	return do_cache_flush(EMMC_DEV_NUM);
}

int dd_cache_ctrl(uint enable)
{
	return do_cache_ctrl(EMMC_DEV_NUM, enable);
}
//jj
int dd_suspend(int wake_time)
{
	return do_mmc_suspend(EMMC_DEV_NUM, wake_time);
}
//
// joys,2014.11.18 ----------------------
//int dd_sleep(int b_with_vcc_off)
int dd_sleep(void)
{
//	return do_mmc_sleep(b_with_vcc_off, EMMC_DEV_NUM);
	return do_mmc_sleep(EMMC_DEV_NUM);
}

int dd_awake(void)
{
	return do_mmc_awake(EMMC_DEV_NUM);
}

int dd_init_awake(void)
{
	return do_mmc_init_awake(EMMC_DEV_NUM);
}

// ---------------------------------------

// joys,2014.12.01 ---------------------------------------------
int dd_poweroff_notify(int notify_type)
{	
	return do_mmc_poweroff_notify(EMMC_DEV_NUM, notify_type);
}
// -------------------------------------------------------------

int get_dd_total_sector_count(void)
{	
	return get_mmc_total_block_count(EMMC_DEV_NUM);
}

int get_dd_product_name(char *proc_name)
{
	return get_mmc_product_name(EMMC_DEV_NUM, proc_name);
}

int get_dd_cache_enable(void)
{
	return get_mmc_cache_enable(EMMC_DEV_NUM);
}

int get_dd_packed_enable(void)
{
	return get_mmc_packed_enable(EMMC_DEV_NUM);
}

int get_dd_bkops_enable(void)
{
	return do_get_bkops_enable(EMMC_DEV_NUM);
}

int get_dd_hpi_enable(void)
{
	return do_get_bkops_enable(EMMC_DEV_NUM);
}

int dd_packed_add_list(ulong start, lbaint_t blkcnt, void* src, int rw)
{
	return do_packed_add_list(EMMC_DEV_NUM, start, blkcnt, src, rw);
}

int dd_packed_send_list(void)
{
	int ret = 0;
	
	if (g_plr_state.b_packed_enable) {
		ret = do_packed_send_list(EMMC_DEV_NUM);

		if (ret == -(INTERNAL_POWEROFF_FLAG))
			return -(INTERNAL_POWEROFF_FLAG);
		
		else if (ret < 1) {
			plr_info("Packed send list Fail\n");
			ret = -PLR_EIO;
		}
		else
			ret = 0;
	}
	else 
		ret = 0;

	return ret;
}

void* dd_packed_create_buff(void *buff)
{
	return do_packed_create_buff(EMMC_DEV_NUM, buff);
}

int dd_packed_delete_buff(void)
{
	return do_packed_delete_buff(EMMC_DEV_NUM);
}

int dd_get_packed_max_sectors(int rw)
{
	return get_packed_max_sectors(EMMC_DEV_NUM, rw);
}

int dd_mmc_get_packed_count(void)
{
	return get_packed_count(EMMC_DEV_NUM);
}

int dd_send_internal_info(void * internal_info)
{
	return do_send_internal_info(EMMC_DEV_NUM, internal_info);
}

int dd_bkops_test_enable(int b_enable)
{
	return do_bkops_test_enable(EMMC_DEV_NUM, b_enable);
}

int dd_hpi_test_enable(int b_enable)
{
	return do_hpi_test_enable(EMMC_DEV_NUM, b_enable);
}

int dd_internal_power_control(int b_power)
{
	return do_internal_power_control(EMMC_DEV_NUM, b_power);
}

// mSec unit
int get_dd_hpi_out_of_int_time(void)
{
	return do_get_hpi_out_of_int_time(EMMC_DEV_NUM);
}

// mSec unit
int get_dd_poweroff_notify_timeout(int notify_type)
{
	return do_get_poweroff_notify_timeout(EMMC_DEV_NUM, notify_type);
}

// uSec unit
int get_dd_s_a_timeout(void)
{
	return 	do_get_s_a_timeout(EMMC_DEV_NUM);
}

int get_dd_bkops_urgent_caused(void)
{
	return do_get_bkops_urgent_caused(EMMC_DEV_NUM);
}

/* ------------------------------------------------
* Device Partition Information Function
* ------------------------------------------------*/

int get_sdmmc_part_info(uint *start, uint *count, uchar *pid)
{
	return get_mmc_partition_info(SDCARD_DEV_NUM, SDCARD_PART_NUM, start, count, pid);        
}

void get_dd_part_info(uint *start, uint *count)
{
	*start = g_plr_state.test_start_sector;
	*count = g_plr_state.test_sector_length;
	
	return;
}

int get_dd_dirty_part_info(uint *start, uint *count)
{
	int dirty_count = 0;
	int dirty_start[2] = {0,};
	int dirty_len[2] = {0,};
	
	*start = g_plr_state.test_start_sector;
	*count = g_plr_state.test_sector_length;

	// front dirty area
	if (g_plr_state.test_start_sector > 0) {
		dirty_start[0] = 0;
		dirty_len[0] = g_plr_state.test_start_sector;
		dirty_count++;
	}

	// back dirty area
	if ( (g_plr_state.test_start_sector + g_plr_state.test_sector_length) < g_plr_state.total_device_sector) {
		dirty_start[1] = g_plr_state.test_start_sector + g_plr_state.test_sector_length;
		dirty_len[1] = g_plr_state.total_device_sector - dirty_start[1];
		dirty_count++;
	}

	if (dirty_count == 0) {
		plr_info("No space to dirty part\n");
		return -1;
	}

	// set large area
	if (dirty_len[0] >= dirty_len[1]) {
		*start = dirty_start[0];
		*count = dirty_len[0];
	}
	else {
		*start = dirty_start[1];
		*count = dirty_len[1];
	}

	*start = dirty_start[1];
	*count = dirty_len[1];

	return 0 ;
}
