#ifndef _PLR_SYSTEM_H_
#define _PLR_SYSTEM_H_

#include "plr_internal_state.h"
#define CONFIG_SYS_CBSIZE 256
#define MB (1024 * 1024)

#ifndef PAGE_SIZE
#define PAGE_SIZE               4096
#endif

#define PAGE_ALIGN(addr)        ALIGN(addr, PAGE_SIZE)
#define ALIGN(x, a)             __ALIGN_MASK(x, (typeof(x))(a) - 1)

/*
 * ex) ALIGN(0x12345, 0x1000)
 * if __ALIGN_MASK
 *      ==  ((x) & ~(mask)) => 0x12000
 *      ==  (((x) + mask) & ~(mask) => 0x13000
 * */
#define __ALIGN_MASK(x, mask)   (((x) + mask) & ~(mask))

#define SEC2BYTE(sec)	(((off64_t)sec)<<9)
#define BYTE2SEC(byte)	(u32)(byte>>9)
#if 1
#define MMC_PATH	    "/dev/block/mmcblk0"
#define SD_STATE_PATH	"/data/sdcard_state"
#define SW_RESET_PATH   "/data/sw_reset_flag"
#else
#define MMC_PATH	    "/dev/mmcblk0"
#define SD_STATE_PATH	"/sdcard/sdcard_state"
#endif

#define DEVICE_POWEROFF_DATA_PATH		"/sys/kernel/debug/mmc0/internal_poff"
//#define DEVICE_POWEROFF_COND_PATH		"/sys/devices/platform/dw_mmc.1/poweroff_cond"
#define DEVICE_POWEROFF_COND_PATH		"/sys/devices/soc.0/7824900.sdhci/power_en"
//#define DEVICE_CACHE_CONTROL_PATH   "/sys/devices/platform/dw_mmc.1/mmc_host/mmc0/mmc0:0001/block/mmcblk0/cache_en"
#define  DEVICE_CACHE_CONTROL_PATH "/sys/devices/soc.0/7824900.sdhci/mmc_host/mmc0/mmc0:0001/block/mmcblk0/cache_en"

#define CONFIG_SYS_CBSIZE 256

#define MS_SEND_CMD_TIMEOUT     10000
#define MS_SEND_CMD_DELAY       1
#define MS_BUSY_TIMEOUT         30000
#define MS_BUSY_DELAY           1
#define MS_RELOAD_TIMEOUT       5000
#define MS_RELOAD_DELAY         100

int readline (const char *const prompt);
unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base);
int dev_ext4_mount(char *dev_name, char* test_path);
int dev_get_size(char *dev_name, unsigned long long *size);
int dev_full_erase(char *dev_path);
int dev_erase(uint dev_num, uint start_sector, uint len, int type);
unsigned int dev_write_full_file(char *file_name, unsigned long long file_size);
int excute_internal_check(void);
int excute_internal_reset(void);
int send_poweroff_cmd(int cmd);
int send_cmd_poweroff(void);
int send_cmd_poweron(void);
int send_data_poweroff(internal_info_t *mmc_internal_info);
int poweroff_data_check(void);
int close_mmc(void);
int prepare_mmc(void);
int wait_reload_mmc_device(void);

#endif
