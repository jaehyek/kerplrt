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
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <linux/fs.h>
//#include <linux/wakelock.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <asm/ioctl.h>

#include "plr_common.h"
#include "plr_hooking.h"
#include "plr_errno.h"
#include "plr_rtc64.h"
#include "plr_mmc.h"
#include "plr_system.h"
#include "plr_internal_state.h"
#include "plr_mmc_poweroff.h"

#define BLKDISCARD _IO(0x12,119)
#define BLKSECDISCARD	_IO(0x12,125)

#define SEC2BYTE(sec)	(((off64_t)sec)<<9)
#define BYTE2SEC(byte)	(u32)(byte>>9)

#define UNSTUFF_BITS(resp,start,size)										\
	({																		\
		const int __size = size;											\
		const unsigned long __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);								\
		const int __shft = (start) & 31;									\
		unsigned long __res;												\
																			\
		__res = resp[__off] >> __shft;										\
		if (__size + __shft > 32)											\
			__res |= resp[__off-1] << ((32 - __shft) % 32);					\
		__res & __mask;														\
	})
	
//jj
#ifndef CONFIG_WD_PERIOD
	# define CONFIG_WD_PERIOD	(10 * 1000 * 1000)	/* 10 seconds default*/
#endif

/* Disable alarm */
#define ANDROID_ALARM_CLEAR(type)           _IO('a', 0 | ((type) << 4))

/* Ack last alarm and wait for next */
#define ANDROID_ALARM_WAIT                  _IO('a', 1)

#define ALARM_IOW(c, type, size)            _IOW('a', (c) | ((type) << 4), size)
/* Set alarm */
#define ANDROID_ALARM_SET(type)             ALARM_IOW(2, type, struct timespec)
#define ANDROID_ALARM_SET_AND_WAIT(type)    ALARM_IOW(3, type, struct timespec)
#define ANDROID_ALARM_GET_TIME(type)        ALARM_IOW(4, type, struct timespec)
#define ANDROID_ALARM_SET_RTC               _IOW('a', 5, struct timespec)
#define ANDROID_ALARM_BASE_CMD(cmd)         (cmd & ~(_IOC(0, 0, 0xf0, 0)))
#define ANDROID_ALARM_IOCTL_TO_TYPE(cmd)    (_IOC_NR(cmd) >> 4)
//endjj

void do_dcache_enable(void)
{
	return;
}

void do_dcache_disable(void)
{
	return;
}

int get_mmc_product_name(uint dev_num, char *name)
{
	return 0;	
}

int get_mmc_total_block_count(uint dev_num)
{
        uint64_t total;
	uint	sec;

	if (dev_num == 0){
		plr_err("device not open..\n");
		return 0;
	}

        if (ioctl(dev_num, BLKGETSIZE64, &total)) {
                plr_err ("%d ioctl BLKGETSIZE64 error!!!\n", dev_num);
                return -1; 
        }   


	sec = BYTE2SEC(total);
	plr_info("total block_count %u(%x)\n", sec, sec);
	return sec;
}

int get_mmc_partition_info(uint dev, int part_num, unsigned int *block_start, 
	unsigned int *block_count, unsigned char *part_Id)
{
	if (dev == SDCARD_DEV_NUM){
		*block_start = 0;
		*block_count = 512;
		*part_Id = 0;
	}
	else {
		// 미구현
		*block_start = 0;
		*block_count = 512;
		*part_Id = 0;
	}
	return 0;
}

int get_mmc_cache_enable(uint dev_num)
{
	int ret = 0;
	int fd = 0;
	char buf[2] = {0};

	fd = open(DEVICE_CACHE_CONTROL_PATH, O_RDWR|O_SYNC);

	if (fd < 0) {
		plr_err("[%s] Open Cache Control fail\n", __func__);
		perror("");
		return -1;
	}
	
	ret = read(fd, buf, sizeof(buf));
	
	if (ret < 0){
		plr_err("[%s] Write Cache Control fail\n", __func__);
		perror("");
		return -1;
	}

	ret = atoi(buf);
	close(fd);

	return ret;
}

int get_mmc_packed_enable(uint dev_num)
{
	return 0;
}

int do_cache_flush(uint dev_num)
{
	int ret = 0;
	ret = fsync(dev_num);

	if (ret < 0){
		if (poweroff_data_check())
			return -(INTERNAL_POWEROFF_FLAG);
		
		plr_err("cache flush fail : ");
		perror("");
		return -1;
	}

	return 0;
}

int do_cache_ctrl(uint dev_num, uint enable)
{
	int ret = 0;
	int fd = 0;
	char send_buf[2] = {0};

	fd = open(DEVICE_CACHE_CONTROL_PATH, O_RDWR|O_SYNC);

	if (fd < 0) {
		plr_err("[%s] Open Cache Control fail\n", __func__);
		perror("");
		return -1;
	}
	
	snprintf(send_buf, sizeof(send_buf), "%u", enable);
	ret = write(fd, send_buf, sizeof(send_buf));
	
	if (ret < 0){
		plr_err("[%s] Write Cache Control fail\n", __func__);
		perror("");
		return -1;
	}

	close(fd);

	return 0;
}

int do_packed_add_list(int dev_num, ulong start, lbaint_t blkcnt, void*src, int rw)
{
	return 0;
}

int do_packed_send_list(int dev_num)
{
	return 0;
}

void* do_packed_create_buff(int dev_num, void *buff)
{
    return buff;
}

int do_packed_delete_buff(int dev_num)
{
	return 0;
}

struct mmc_packed* do_get_packed_info(int dev_num)
{
	return NULL;
}

uint get_packed_count(int dev_num)
{
	return 0;
}

uint get_packed_max_sectors(int dev_num, int rw)
{
	return 0;
}

// lssek sector unit?, read or write uinit
int do_read(uint dev_num, uchar *data, uint start_sector, uint len)
{
	ssize_t ret;
	off64_t ret_off;
	size_t b_len = (size_t)SEC2BYTE(len);

	ret_off = lseek64(dev_num, SEC2BYTE(start_sector), SEEK_SET);
	if (ret_off < 0){
		plr_err("read lseek64 fail. fd : %d, sec : %u, to byte : %llu\n",
				dev_num, start_sector, SEC2BYTE(start_sector));
		perror("read lseek64 fail");
		return -1;
	}

	ret = read(dev_num, data, b_len);

	if (ret <= (ssize_t)b_len && ret >= 0)
		return 0;
	else{
		if (ret == -(INTERNAL_POWEROFF_FLAG))
			return ret;
		else {
			plr_err("ret = %d, len = %d\n", (int)ret, b_len);
			plr_info("Read Fail LSN : 0x%08X \n", start_sector);
			perror("Read ERR : ");

			if (poweroff_data_check() > 0){
				plr_info("KKR already ocurred power off.. try power on\n");
				excute_internal_reset();
				do_internal_power_control(dev_num, TRUE);
			}
			return -PLR_EIO;

		}
	}
}

int do_write(uint dev_num, uchar *data, uint start_sector, uint len)
{
	ssize_t ret;
	off64_t ret_off;
	size_t b_len = (size_t)SEC2BYTE(len);

	ret_off = lseek64(dev_num, SEC2BYTE(start_sector), SEEK_SET);
	if (ret_off < 0){
		plr_err("write lseek64 fail. fd : %d, sec : %u, to byte : %llu\n",
				dev_num, start_sector, SEC2BYTE(start_sector));
		perror("write lseek64 fail");
		return -1;
	}

	ret = write(dev_num, data, b_len);

	if (ret == (ssize_t)b_len)
		return 0;
	else {

		plr_info("internal ? ret %d,  sector 0x%x, b_len %d(KB)\n", ret, start_sector, b_len/1024);

		if (poweroff_data_check())
			return -(INTERNAL_POWEROFF_FLAG);
		else {
			plr_err("ret = %d, len = %d\n", (int)ret, b_len);
			plr_info("Write Fail LSN : 0x%08X \n", start_sector);
			perror("Write ERR : ");
			return -PLR_EIO;
		}
	}
}

int do_erase(uint dev_num, uint start_sector, uint len) 
{
	return dev_erase(dev_num, start_sector, len, TYPE_SANITIZE);
}

int do_mmc_suspend(uint dev_num, int wake_time)
{
    int afd;
	int fd;
	int ret;
	struct timespec ts;

	ret = clock_gettime(CLOCK_BOOTTIME, &ts);

	afd = open("/dev/alarm", O_RDWR);
	if(fd < 0) {
		fprintf(stderr, "Unable to open /dev/alarm: %s\n", strerror(errno));
		return 1;
	}
retry:
	ret = ioctl(afd, ANDROID_ALARM_GET_TIME(0), &ts);
	if(ret < 0) {
		fprintf(stderr, "Unable to get current time: %s\n", strerror(errno));
	}
	ts.tv_sec += wake_time;
//    ts.tv_nsec += wake_time;

	ret = ioctl(afd, ANDROID_ALARM_SET(0), &ts);
	if(ret < 0) {
		fprintf(stderr, "Unable to set alarm: %s\n", strerror(errno));
	}

//	plr_err("wating\n");
	mdelay(1);

    fd = open("/sys/power/state", O_RDWR);
    if(fd < 0) {
        plr_err("Unable to open state: %s\n", fd);
    }
    else
    {
        plr_err("system suspend\n");
        ret = write(fd, "mem\n", strlen("mem")+1);
        if (ret < 0)
        {
            fprintf(stderr, "Write suspend faile: %s\n", strerror(errno));

            ret = ioctl(afd, ANDROID_ALARM_CLEAR(0), &ts);
            if(ret < 0) {
                fprintf(stderr, "Unable to clear alarm: %s\n", strerror(errno));
            }
            plr_err("[%s]retry after 500ms wating because of Write suspend fail\n", __func__);
            mdelay(100);
            goto retry;
        }
        close(afd);
        close(fd);
    }
//    plr_err("resume\n");

	return 0;
}
//end

//int do_mmc_sleep(uint dev_num, int b_with_vcc_off)
int do_mmc_sleep(uint dev_num)
{
	return 0;
}

int do_mmc_awake(uint dev_num)
{
	return 0;
}

int do_mmc_init_awake(uint dev_num)
{
	return 0;
}

// uSec unit
int do_get_s_a_timeout(uint dev_num)
{	
#if 1
	return 0;
#else
	return emmc_get_s_a_timeout((int)dev_num);
#endif
}

int do_mmc_poweroff_notify(uint dev_num, int notify_type)
{	
#if 1
	return 0;
#else
	return emmc_poweroff_notify((int)dev_num, notify_type);
#endif
}

// mSec unit
int do_get_poweroff_notify_timeout(uint dev_num, int notify_type)
{
#if 1
	return 0;
#else
	return emmc_get_poweroff_notify_timeout((uint)dev_num, notify_type);
#endif
}

int do_bkops_test_enable(uint dev_num, int b_enable)
{
#if 1
	return 0;
#else
	return emmc_bkops_test_enable((int)dev_num, b_enable);
#endif
}

int do_hpi_test_enable(uint dev_num, int b_enable)
{
#if 1
	return 0;
#else
	return emmc_hpi_test_enable((int)dev_num, b_enable);
#endif
}

int do_get_bkops_enable(uint dev_num)
{
#if 1
	return 0;
#else
	return mmc->b_bkops_test;
#endif
}

int do_get_hpi_enable(uint dev_num)
{
#if 1
	return 0;
#else
	return mmc->b_hpi_test;
#endif
}

int do_get_bkops_urgent_caused(uint dev_num)
{
#if 1
	return 0;
#else
	return emmc_get_bkops_urgent_caused((int)dev_num);
#endif
}

// mSec unit
int do_get_hpi_out_of_int_time(uint dev_num)
{
#if 1
	return 0;
#else
	return emm_get_hpi_out_of_int_time((int)dev_num);
#endif
}

// joys,2015.06.19 for internal poff
int do_send_internal_info(int dev_num, void * internal_info)
{
	return send_data_poweroff((internal_info_t*)internal_info);
}

// joys,2015.07.13
int do_erase_for_test(uint dev_num, uint start_sector, uint len, int type)
{
	return dev_erase(dev_num, start_sector, len, type);
}

int get_erase_count(void)
{
	return 0;
}

/* RTC module (second) */
u32 get_rtc_time(void)
{
	struct timespec time;

	if (clock_gettime(CLOCK_MONOTONIC, &time)){
		plr_err("get time fail..\n");
		return 0;
	}
	return (u32)time.tv_sec;
}

/* cpu timer register */
u32 get_cpu_timer (ulong base)
{
	struct timespec time;

	if (clock_gettime(CLOCK_REALTIME, &time)){
		plr_err("get time fail..\n");
		return 0;
	}
	return (u32)time.tv_nsec;
}

/* RTC module tick (under ms) */
u32 get_tick_count(void)
{
	/*nothing to do*/
	return 0;
}

/* 64bit tick count */
u64 get_tick_count64(void)
{
	struct timespec time;
	u64 tick;

	if (clock_gettime(CLOCK_MONOTONIC, &time)){
		plr_err("get time fail..\n");
		return 0;
	}

	/*sec멤버를 nsec로 변환후 64bit형에 변환한 sec2nsec와 원래 nsec멤버를 합침*/
	tick = (((u64)time.tv_sec)*1000*1000*1000) + (u64)time.tv_nsec;

	/*1tick = 30.518 us 인것을 감안한 변환*/
	tick = tick/30518;

	return tick;
}

/* 64bit tick counter reset */
void reset_tick_count64(void)
{
	/*nothing to do*/
	return;
}

/* RTC tick count initialize */
void reset_tick_count(int enable)
{
	/*nothing to do*/
	return ;
}

void reset_board(ulong ignored)
{
	system("reboot");
	return;
}

int do_internal_power_control(int dev_num, bool b_power)
{
#if 1
	if (b_power == TRUE)
	{
		plr_info("kkr get out\n");
		mdelay(5000);
	}
	else
	{
		plr_info("kkr get out\n");
		mdelay(5000);
	}
#else
	int ret = 0;
	int fd = 0;
    char buf[1] = {0};

	plr_err("\n[%s] Enter\n", __func__);
//	memset (&recv_info, 0, sizeof(internal_info_t));

	fd = open(DEVICE_POWEROFF_COND_PATH, O_RDWR|O_SYNC);

	if (fd < 0) {
		plr_err("[%s] Open power_control fail\n", __func__);
		perror("");
		return -1;
	}
	
	ret = read(fd, buf, 1);
	if (ret < 0){
		plr_err("[%s] Read power_control info fail\n", __func__);
		perror("");
		return -1;
	}

	if (b_power == TRUE)
	{
        buf[0] = '1';
    }
    else
    {
        buf[0] = '1';
    }
	ret = write(fd, buf, sizeof(char));
	if (ret < 0)
	{
		plr_err("[%s] Write power_control fail\n", __func__);
		perror("");
		return -1;
	}
	close(fd);
    mdelay(5000);

#if 0
	if (b_power == TRUE)
	{
		plr_info("kkr get out\n");
		mdelay(5000);
	}
	else
	{
		plr_info("kkr get out\n");
		mdelay(5000);
	}
#endif
#endif
	return 0;
}
