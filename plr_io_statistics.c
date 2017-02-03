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
 //@ joys,2015.07.24

#include "plr_io_statistics.h"
#include "plr_hooking.h"

#define CONFIG_SUM_WRITE_BUSYTIME

typedef struct {
	mmc_cmd_e cmd;
	u32 blocks;
	u32 blksize;
} mmc_request_s;

mmc_statistic_s statistics;

/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */

static inline int generic_ffs(int x)
{
	int r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}
	return r;
}

/**
 * fls - find last (most-significant) bit set
 * @x: the word to search
 *
 * This is defined the same way as ffs.
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */
static inline int generic_fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

static void statistic_init_sector_size(void)
{
	int i = 0;

	req_rw_stats_s* buf;

	if (!statistics.rw_stats)
		return;
	
	for (i = 0; i < REQ_SIZE_COUNT; i++)
	{
		buf = statistics.rw_stats + i;

		buf->chunk_size = (MIN_REQ_SIZE << i) / 1024;
		buf->w_min_latency = PLR_LONGLONG_MAX;
		buf->r_min_latency = PLR_LONGLONG_MAX;
		
	}
}


static int statistic_init(void)
{	
	if (statistics.init)
		return 0;
	
	memset(&statistics, 0, sizeof(mmc_statistic_s));

	if (statistics.rw_stats == NULL)
		statistics.rw_stats = (req_rw_stats_s*)malloc(sizeof(req_rw_stats_s) * REQ_SIZE_COUNT);
	
	if (statistics.rw_stats == NULL) {
		_err_msg("Memorypool alloc fail!\n");
		return -1;
	}

	memset(statistics.rw_stats, 0, sizeof(req_rw_stats_s) * REQ_SIZE_COUNT);

	statistic_init_sector_size();

	statistics.flush_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.pon_short_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.pon_long_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.sleep_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.awake_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.bkops_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.hpi_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.op_con_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.op_con_with_pon_stats.min_latency = PLR_LONGLONG_MAX;
	
	statistics.init = 1;

	_sdbg_msg("Init Success! // sizeof(req_rw_stats_s) : %d\n", sizeof(req_rw_stats_s));

	return 0;
}

static int statistic_destroy(void)
{
	if (statistics.rw_stats)
		free(statistics.rw_stats);

	memset(&statistics, 0, sizeof(mmc_statistic_s));

	return 0;
}

static int statistic_get_index(u32 chunk_size) 
{
	int last = generic_fls((int)chunk_size);
	int first = generic_ffs((int)chunk_size);
	int index = 0;

	if (last == first) {
		index = last - generic_fls(MIN_REQ_SIZE);
	}
	else {
		index = (last - generic_fls(MIN_REQ_SIZE)) + 1;
	}
 
	if (index >= REQ_SIZE_COUNT)
		index = REQ_SIZE_COUNT - 1;
	
	return index * (index > 0);
}

static req_rw_stats_s* get_request_buff(u32 size)
{
	req_rw_stats_s* req_buf = NULL;
	
	int index = statistic_get_index(size);

	if (index < 0)
		return NULL;
	
	req_buf = (req_rw_stats_s*)(statistics.rw_stats + index);

	return req_buf;
}

static int statistic_rw_parcer_print(req_rw_stats_s* log_parcer)
{
	u64 r_min_latency = 0;
	u64 w_min_latency = 0;
	u64 r_avr_latency = 0;
	u64 w_avr_latency = 0;
	
	if (log_parcer->r_min_latency == PLR_LONGLONG_MAX)
		r_min_latency = 0;
	else
		r_min_latency = log_parcer->r_min_latency;

	if (log_parcer->w_min_latency == PLR_LONGLONG_MAX)
		w_min_latency = 0;
	else
		w_min_latency = log_parcer->w_min_latency;

	if (log_parcer->rCnt < 1)
		r_avr_latency = 0;
	else
		r_avr_latency = log_parcer->r_latency / log_parcer->rCnt;

	if (log_parcer->wCnt < 1)
		w_avr_latency = 0;
	else
		w_avr_latency = log_parcer->w_latency / log_parcer->wCnt;
	
	plr_info_highlight (" |   %7u |  %10llu |  %7u |  %7u |  %7u ||  %10llu |  %7u |  %7u |  %7u |\n",
		log_parcer->chunk_size,
		log_parcer->rCnt,
		get_tick2usec(r_min_latency),
		get_tick2usec(log_parcer->r_max_latency),
		get_tick2usec(r_avr_latency),
		log_parcer->wCnt,
		get_tick2usec(w_min_latency),
		get_tick2usec(log_parcer->w_max_latency),
		get_tick2usec(w_avr_latency)
		);

#if 0	
	printf ("--------- statistic log ----------!!\n");
	printf ("request size		: %d K\n", log_parcer->chuck_size);
	printf ("read count		: %llu\n", log_parcer->rCnt);
	printf ("write count		: %llu\n", log_parcer->wCnt);
	printf ("read latency		: %llu\n", log_parcer->r_latency);
	printf ("read max latency	: %llu\n", log_parcer->r_max_latency);
	printf ("read min latency	: %llu\n", log_parcer->r_min_latency);
	printf ("write latency		: %llu\n", log_parcer->w_latency);
	printf ("write max latency	: %llu\n", log_parcer->w_max_latency);
	printf ("write min latency	: %llu\n", log_parcer->w_min_latency);
	printf ("----------------------------------!!\n");
#endif	
	
	return 0;
}

#if 1
static int statistic_parcer_print(req_stats_s *stats1, req_stats_s *stats2)
{
	u64 min_latency1 = 0;
	u64 avr_latency1 = 0;	
	u64 min_latency2 = 0;
	u64 avr_latency2 = 0;

	if (stats1 == NULL)
		return 0;
	
	if (stats1->min_latency == PLR_LONGLONG_MAX)
		min_latency1 = 0;
	else
		min_latency1 = stats1->min_latency;

	if (stats1->cnt < 1)
		avr_latency1 = 0;
	else
		avr_latency1 = stats1->total_latency / stats1->cnt;


	if (stats2) {	
		if (stats2->min_latency == PLR_LONGLONG_MAX)
			min_latency2 = 0;
		else
			min_latency2 = stats2->min_latency;


		if (stats2->cnt < 1)
			avr_latency2 = 0;
		else
			avr_latency2 = stats2->total_latency / stats2->cnt;
	}

	if (stats2) {
		plr_info_highlight (" |  %13llu |  %8u |  %8u |  %8u ||  %13llu |  %8u |  %8u |  %8u |\n",
			stats1->cnt, 
			get_tick2usec(min_latency1), 
			get_tick2usec(stats1->max_latency), 
			get_tick2usec(avr_latency1),
			stats2->cnt,
			get_tick2usec(min_latency2),
			get_tick2usec(stats2->max_latency),
			get_tick2usec(avr_latency2) );
	}
	else {
		plr_info_highlight (" |  %13llu |  %8u |  %8u |  %8u |\n",
			stats1->cnt, 
			get_tick2usec(min_latency1), 
			get_tick2usec(stats1->max_latency), 
			get_tick2usec(avr_latency1) );
	}

#if 0	
	printf ("--------- statistic log ----------!!\n");
	printf ("request size		: %d K\n", log_parcer->chuck_size);
	printf ("read count		: %llu\n", log_parcer->rCnt);
	printf ("write count		: %llu\n", log_parcer->wCnt);
	printf ("read latency		: %llu\n", log_parcer->r_latency);
	printf ("read max latency	: %llu\n", log_parcer->r_max_latency);
	printf ("read min latency	: %llu\n", log_parcer->r_min_latency);
	printf ("write latency		: %llu\n", log_parcer->w_latency);
	printf ("write max latency	: %llu\n", log_parcer->w_max_latency);
	printf ("write min latency	: %llu\n", log_parcer->w_min_latency);
	printf ("----------------------------------!!\n");
#endif	
	
	return 0;
}
#endif

static int statistic_rw_print(void)
{
	int index = 0;
	u64 total_latency = 0;
	req_rw_stats_s* buf = statistics.rw_stats;
	
	if (buf == NULL) {
		plr_err("Warning.. rw buf is NULL\n");
		return 0;
	}

	total_latency = statistics.total_rlatency + statistics.total_wlatency + 
		statistics.flush_stats.total_latency;

	// Read, Write summury
	plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
	plr_info_highlight (" |    Latency analysis   |    Latency (msec)    ||   Traffic analysis |        Read      |       Write      |\n");
	plr_info_highlight (" |-----------------------|----------------------||--------------------|------------------|------------------|\n");
	plr_info_highlight (" |   Total r/w latency   | %20u ||  Action sectors    |  %15llu |  %15llu |\n",
		get_tick2msec(total_latency), statistics.total_rBlocks, statistics.total_wBlocks);
	plr_info_highlight (" |   Total write latency | %20u ||  Number of action  |  %15llu |  %15llu |\n",
		get_tick2msec(statistics.total_wlatency), statistics.total_rCnt, statistics.total_wCnt);
	plr_info_highlight (" |   Total read latency  | %20u ||                    |                  |                  |\n",   
		get_tick2msec(statistics.total_rlatency));
	plr_info_highlight (" |   Total flush latency | %20u ||                    |                  |                  |\n",   
		get_tick2msec(statistics.flush_stats.total_latency));
	plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");

	// Read, Write statistics per request size
	plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
	plr_info_highlight (" |           |             Read latency (usec)              ||             Write latency (usec)             |\n");
	plr_info_highlight (" |  Request  |----------------------------------------------||----------------------------------------------|\n");
	plr_info_highlight (" | size (KB) |     Count   |    Min   |    Max   |    Avg   ||     Count   |    Min   |    Max   |    Avg   |\n");
	plr_info_highlight (" |-----------|-------------|----------|----------|----------||-------------|----------|----------|----------|\n");
	
	do {
		statistic_rw_parcer_print(buf + index);
		index++;
	} while (index < REQ_SIZE_COUNT);
	
	plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");

	return 0;
}

static int statistic_feature_print(void)
{
	// Flush & PON statistics
	if (statistics.flush_stats.cnt > 0) {
		plr_info_highlight ("  ----------------------------------------------------\n");
		plr_info_highlight (" |                Flush latency (usec)                |\n");
		plr_info_highlight (" |----------------------------------------------------|\n");
		plr_info_highlight (" |      Count     |    Min    |    Max    |    Avg    |\n");
		plr_info_highlight (" |----------------|-----------|-----------|-----------|\n");
		statistic_parcer_print(&statistics.flush_stats, NULL);
		plr_info_highlight ("  ----------------------------------------------------\n");
	}

	if (statistics.pon_short_stats.cnt > 0 || statistics.pon_long_stats.cnt > 0) {
	// PON statistics
		plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
		plr_info_highlight (" |                PON short latency (usec)            ||                PON long latency (usec)             |\n");
		plr_info_highlight (" |----------------------------------------------------||----------------------------------------------------|\n");
		plr_info_highlight (" |      Count     |    Min    |    Max    |    Avg    ||      Count     |    Min    |    Max    |    Avg    |\n");
		plr_info_highlight (" |----------------|-----------|-----------|-----------||----------------|-----------|-----------|-----------|\n");
		statistic_parcer_print(&statistics.pon_short_stats, &statistics.pon_long_stats);
		plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
	}

	// Sleep & Awake statistics
	if (statistics.sleep_stats.cnt > 0 || statistics.awake_stats.cnt > 0) {
		plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
		plr_info_highlight (" |                Sleep latency (usec)                ||                 Awake latency (usec)               |\n");
		plr_info_highlight (" |----------------------------------------------------||----------------------------------------------------|\n");
		plr_info_highlight (" |      Count     |    Min    |    Max    |    Avg    ||      Count     |    Min    |    Max    |    Avg    |\n");
		plr_info_highlight (" |----------------|-----------|-----------|-----------||----------------|-----------|-----------|-----------|\n");
		statistic_parcer_print(&statistics.sleep_stats, &statistics.awake_stats);
		plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
	}

	// Bkops & HPI statistics
	if (statistics.bkops_stats.cnt > 0 || statistics.hpi_stats.cnt > 0) {
		plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
		plr_info_highlight (" |                BKOPS latency (usec)                ||            HPI while BKOPS latency (usec)          |\n");
		plr_info_highlight (" |----------------------------------------------------||----------------------------------------------------|\n");
		plr_info_highlight (" |      Count     |    Min    |    Max    |    Avg    ||      Count     |    Min    |    Max    |    Avg    |\n");
		plr_info_highlight (" |----------------|-----------|-----------|-----------||----------------|-----------|-----------|-----------|\n");
		statistic_parcer_print(&statistics.bkops_stats, &statistics.hpi_stats);
		plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
	}

	// Operation condition statistics
	if (statistics.op_con_stats.cnt > 0 || statistics.op_con_with_pon_stats.cnt > 0) {
		plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
		plr_info_highlight (" |                OP_CON latency (usec)               ||           OP_CON with PON latency (usec)           |\n");
		plr_info_highlight (" |----------------------------------------------------||----------------------------------------------------|\n");
		plr_info_highlight (" |      Count     |    Min    |    Max    |    Avg    ||      Count     |    Min    |    Max    |    Avg    |\n");
		plr_info_highlight (" |----------------|-----------|-----------|-----------||----------------|-----------|-----------|-----------|\n");
		statistic_parcer_print(&statistics.op_con_stats, &statistics.op_con_with_pon_stats);
		plr_info_highlight ("  ----------------------------------------------------------------------------------------------------------\n");
	}

	return 0;
}

static int statistic_print(void)
{
	statistic_rw_print();
	statistic_feature_print();
	
	return 0;
}

static void statistic_set_top_latency(u32 size, u64 latency)
{
	if (statistics.high_latency < latency) {
		statistics.high_latency = latency;
		statistics.high_latency_chunk = size / 1024;
	}
}

static void statistic_set_min_latency(u64* target, u64 compare)
{
	if (*target > compare) {
		*target = compare;
	}
}

static void statistic_set_max_latency(u64* target, u64 compare)
{
	if (*target < compare) {
		*target = compare;
	}
}

static int statistic_emmc_add(mmc_request_s *mrq, u64 curTime, u64 latency)
{
	req_rw_stats_s* req_buf;
	u32 chunk_size = 0;
	
	if (statistics.enable == 0 || statistics.rw_stats == NULL)
		goto out;
	
	chunk_size = mrq->blocks * mrq->blksize;

	if (mrq->cmd == STAT_MMC_WRITE) {
		req_buf = get_request_buff(chunk_size);
		if (req_buf == NULL)
			goto out;
		req_buf->wCnt++;
		req_buf->w_latency += latency;
		statistics.total_wCnt++;
		statistics.total_wBlocks += (u64)mrq->blocks;
		statistics.total_wlatency += latency;
		statistic_set_min_latency(&req_buf->w_min_latency, latency);
		statistic_set_max_latency(&req_buf->w_max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_READ) {
		req_buf = get_request_buff(chunk_size);
		if (req_buf == NULL)
			goto out;
		req_buf->rCnt++;
		req_buf->r_latency += latency;
		statistics.total_rCnt++;
		statistics.total_rBlocks += (u64)mrq->blocks;
		statistics.total_rlatency += latency;
		statistic_set_min_latency(&req_buf->r_min_latency, latency);
		statistic_set_max_latency(&req_buf->r_max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_CACHE_FLUSH) {
		statistics.flush_stats.cnt++;
		statistics.flush_stats.total_latency += latency;
		statistic_set_min_latency(&statistics.flush_stats.min_latency, latency);
		statistic_set_max_latency(&statistics.flush_stats.max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_PON_SHORT) {
		statistics.pon_short_stats.cnt++;
		statistics.pon_short_stats.total_latency += latency;
		statistic_set_min_latency(&statistics.pon_short_stats.min_latency, latency);
		statistic_set_max_latency(&statistics.pon_short_stats.max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_PON_LONG) {
		statistics.pon_long_stats.cnt++;
		statistics.pon_long_stats.total_latency += latency;
		statistic_set_min_latency(&statistics.pon_long_stats.min_latency, latency);
		statistic_set_max_latency(&statistics.pon_long_stats.max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_SLEEP) {
		statistics.sleep_stats.cnt++;
		statistics.sleep_stats.total_latency += latency;
		statistic_set_min_latency(&statistics.sleep_stats.min_latency, latency);
		statistic_set_max_latency(&statistics.sleep_stats.max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_AWAKE) {
		statistics.awake_stats.cnt++;
		statistics.awake_stats.total_latency += latency;
		statistic_set_min_latency(&statistics.awake_stats.min_latency, latency);
		statistic_set_max_latency(&statistics.awake_stats.max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_BKOPS) {
		statistics.bkops_stats.cnt++;
		statistics.bkops_stats.total_latency += latency;
		statistic_set_min_latency(&statistics.bkops_stats.min_latency, latency);
		statistic_set_max_latency(&statistics.bkops_stats.max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_HPI) {
		statistics.hpi_stats.cnt++;
		statistics.hpi_stats.total_latency += latency;
		statistic_set_min_latency(&statistics.hpi_stats.min_latency, latency);
		statistic_set_max_latency(&statistics.hpi_stats.max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_OP_CON) {
		statistics.op_con_stats.cnt++;
		statistics.op_con_stats.total_latency += latency;
		statistic_set_min_latency(&statistics.op_con_stats.min_latency, latency);
		statistic_set_max_latency(&statistics.op_con_stats.max_latency, latency);
	}
	else if (mrq->cmd == STAT_MMC_OP_CON_WITH_PON) {
		statistics.op_con_with_pon_stats.cnt++;
		statistics.op_con_with_pon_stats.total_latency += latency;
		statistic_set_min_latency(&statistics.op_con_with_pon_stats.min_latency, latency);
		statistic_set_max_latency(&statistics.op_con_with_pon_stats.max_latency, latency);
	}
//	req_buf->chuck_size = chunk_size;
	statistic_set_top_latency(chunk_size, latency);
	
out:
	return 0;
}

int statistic_clear(void)
{
	if (statistics.rw_stats)
		memset(statistics.rw_stats, 0, sizeof(req_rw_stats_s) * REQ_SIZE_COUNT);

	statistic_init_sector_size();
#if 1
	memset(&statistics.flush_stats, 0, sizeof(req_stats_s));
	memset(&statistics.pon_short_stats, 0, sizeof(req_stats_s));
	memset(&statistics.pon_long_stats, 0, sizeof(req_stats_s));
	memset(&statistics.sleep_stats, 0, sizeof(req_stats_s));
	memset(&statistics.awake_stats, 0, sizeof(req_stats_s));
	memset(&statistics.bkops_stats, 0, sizeof(req_stats_s));
	memset(&statistics.hpi_stats, 0, sizeof(req_stats_s));
	memset(&statistics.op_con_stats, 0, sizeof(req_stats_s));
	memset(&statistics.op_con_with_pon_stats, 0, sizeof(req_stats_s));
	statistics.total_wCnt = 0;
	statistics.total_wBlocks = 0;
	statistics.total_wlatency = 0;
	statistics.total_rCnt = 0;
	statistics.total_rBlocks = 0;
	statistics.total_rlatency = 0;
	statistics.high_latency = 0;
	statistics.high_latency_chunk = 0;
	statistics.flush_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.pon_short_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.pon_long_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.sleep_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.awake_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.bkops_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.hpi_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.op_con_stats.min_latency = PLR_LONGLONG_MAX;
	statistics.op_con_with_pon_stats.min_latency = PLR_LONGLONG_MAX;
#else	
	//memset(&statistics, 0, sizeof(mmc_statistic_s) - sizeof(statistics.rw_stats));
	memset(&statistics.start_time, 0, 
		sizeof(mmc_statistic_s) - sizeof(statistics.init) - sizeof(statistics.enable) - sizeof(statistics.rw_stats));
#endif
	return 0;
}

int statistic_enable(int enable)
{
	int ret = 0;
	
	if (enable)
		ret = statistic_init();
	else
		ret = statistic_destroy();

	if (ret)
		return ret;

	statistics.enable = enable;
	return 0;
}

int statistic_result(void)
{	
	//statistic_init_sector_size();
	statistic_print();
//	statistic_clear();
	 
	return 0;
}

void statistic_mmc_request_done(mmc_cmd_e cmd, uint blocks)
{
	u64 currentTime = 0;	

	mmc_request_s mrq;

	mrq.cmd = cmd;
	mrq.blocks = blocks;
	mrq.blksize = 512;
	
	if (statistics.enable == 0 || statistics.rw_stats == NULL || statistics.start_time == 0) { 
		return;
	}

	currentTime = get_tick_count64();

	if (statistics.start_time > currentTime)
	{
		plr_err("[%s] Warnning... Time is reversed.. start tick : %llu, current tick : %llu\n",
			__func__, statistics.start_time, currentTime);
		statistics.start_time = 0;
		return;
	}

	statistic_emmc_add(&mrq, currentTime, currentTime - statistics.start_time);
	statistics.start_time = 0;

	return;
}

void statistic_mmc_request_start(void)
{	
	if (statistics.enable == 0 || statistics.rw_stats == NULL)
		return;

	statistics.start_time = get_tick_count64();
}

mmc_statistic_s* get_statistic_info(void)
{
	return &statistics;
}
