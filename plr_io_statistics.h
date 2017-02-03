/*
 *  linux/include/linux/mmc/discard.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Card driver specific definitions.
 */

#ifndef _PLR_STATISTICS_H
#define _PLR_STATISTICS_H

#include "plr_common.h"

#define REQ_SIZE_COUNT 9
#define MIN_REQ_SIZE 4096

typedef enum {
	STAT_MMC_READ	= 1,
	STAT_MMC_WRITE,
	STAT_MMC_CACHE_FLUSH,
	STAT_MMC_PON_SHORT,
	STAT_MMC_PON_LONG,
	STAT_MMC_SLEEP,
	STAT_MMC_AWAKE,
	STAT_MMC_BKOPS,
	STAT_MMC_HPI,
	STAT_MMC_OP_CON,
	STAT_MMC_OP_CON_WITH_PON
} mmc_cmd_e;

typedef struct {
	u64 cnt;
	u64 total_latency;
	u64 min_latency;
	u64 max_latency;	
}req_stats_s;

typedef struct {
	u32 chunk_size;
	u64 rCnt;
	u64 wCnt;
	u64 r_latency;
	u64 r_min_latency;
	u64 r_max_latency;
	u64 w_latency;
	u64 w_min_latency;
	u64 w_max_latency;
}req_rw_stats_s;

typedef struct {
	int init;
	int enable;
	u64 start_time;
	u64 total_rCnt;
	u64 total_rBlocks;
	u64 total_rlatency;
	u64 total_wCnt;
	u64 total_wBlocks;
	u64 total_wlatency;
	u64 high_latency;
	u32 high_latency_chunk;
	req_stats_s flush_stats;
	req_stats_s pon_short_stats;
	req_stats_s pon_long_stats;
	req_stats_s sleep_stats;
	req_stats_s awake_stats;
	req_stats_s bkops_stats;
	req_stats_s hpi_stats;
	req_stats_s op_con_stats;
	req_stats_s op_con_with_pon_stats;
	req_rw_stats_s *rw_stats;
}mmc_statistic_s;

mmc_statistic_s* get_statistic_info(void);
int statistic_clear(void);
int statistic_enable(int enable);
int statistic_result(void);
void statistic_mmc_request_start(void);
void statistic_mmc_request_done(mmc_cmd_e cmd, uint blocks);

/* debug utility functions */
#ifdef CONFIG_MMC_STATISTIC_DEBUG
#define _sdbg_msg(fmt, args...) printf("[MEMLOG]//%s(%d): " fmt, __func__, __LINE__, ##args)
#else
#define _sdbg_msg(fmt, args...)
#endif /* CONFIG_MMC_MEM_LOG_BUFF_DEBUG */

#ifndef _err_msg
#define _err_msg(fmt, args...) printf("%s(%d): " fmt, __func__, __LINE__, ##args)
#endif

#endif /* _PLR_STATISTICS_H */


