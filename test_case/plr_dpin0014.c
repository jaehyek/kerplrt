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

#include "../plr_deviceio.h"
#include "../plr_common.h"
#include "../plr_err_handling.h"
#include "../plr_protocol.h"
#include "../plr_case_info.h"
#include "../plr_pattern.h"
#include "../plr_verify.h"
#include "../plr_verify_log.h"
#include "../plr_precondition.h"
#include "plr_case.h"

#define SEQ_WRITE	0
#define RANDOM_WRITE	1
#define RANDOM_WRITE_CNT 4

/* P/E Cycle Define */
//#define PE_CYCLE        600
#define PE_CYCLE        6
#define ADDITIONAL_DAYS_PER_1K 3
/* P/E Cycle Define */

#if 1
//#define RANDOM_LOOP_CNT	50000
#define RANDOM_LOOP_CNT	1000
#else
/* Skip precondition for continue the test which stopped by EMP shock*/
#define RANDOM_LOOP_CNT	1
/* Skip precondition for continue the test which stopped by EMP shock*/
#endif 

#define SLEEP_DELAY_MSEC	100

#define RD_ERR_READ_VERIFY_FAILED			(-1)
#define RD_ERR_WRITE_FAILED				(-2)
#define RD_ERR_INVALID_ZONE_NUMBER		(-3)
#define RD_ERR_READ_IO_FAILED				(-4)

#define K_BYTES	(1024)
#define M_BYTES (1024*1024)
#define G_BYTES	(1024*1024*1024)

#define BUFFER_SIZE				(0x800000)			//@sj 150714, Note: No buffer size is explicitely defined anywhere.
#define BYTES_PER_BUFFER		(BUFFER_SIZE)							// clearer and chainable !
#define WORDS_PER_SECTOR		(BYTES_PER_SECTOR/sizeof(u64))			// 0x80 	(128)
#define WORDS_PER_BUFFER		(BYTES_PER_BUFFER/sizeof(u64))			//
#define BYTES_PER_SECTOR		(SECTOR_SIZE)							// 0x200 	(512)
#define SECTORS_PER_BUFFER		(BYTES_PER_BUFFER / BYTES_PER_SECTOR)	// 0x4000	(16384)
#define RD_MAGIC_NUMBER 					(0xEFCAFE01EFCAFE01ULL)

typedef struct read_disturb_crash_info
{
	int					err_code;		// error code, reserved
	int 				sec_no_in_zone;	// sector number in a zone
	int 				repeat_no;		// repeat number in a boot
	int					boot_cnt;		// boot count

}  read_disturb_crash_info_s;

static read_disturb_crash_info_s _rd_crash_info[1];	// read_disturb_crash_info;

static u64* prepare_write_buffer()
{
	u64 *wbuf 	= (u64*)plr_get_write_buffer();
	u64 *wbufe 	= (u64*)wbuf + WORDS_PER_BUFFER;
	u64 *p		= wbuf;

	while ( p != wbufe ) {
		*p++ = RD_MAGIC_NUMBER;
	}
	return wbuf;
}

static u64* prepare_random_write_buffer(void)
{

	u64 *wbuf 	= (u64*)plr_get_write_buffer();
	u64 *wbufe 	= (u64*)wbuf + WORDS_PER_BUFFER;
	u64 *p		= wbuf;

	well512_write_seed(get_cpu_timer(0));

	while ( p != wbufe ) {
		*p++ = (well512_write_rand());
	}
	return wbuf;
}

static void write_buffer(u32 start_addr, u64* buffer, u8 is_random)
{
	u32 len = (is_random)?4*K_BYTES/SECTOR_SIZE:SECTORS_PER_BUFFER;
	dd_write((uchar*)buffer, start_addr, len);
}

static int sleep_and_resume(uint delay_msec)
{
#if 0
	dd_sleep();
	// delay
	mdelay(delay_msec);
    dd_awake();
#endif
	if (dd_suspend(3))
		return -1;

	return 0;
}

static int known_pattern_write()
{
    u32 wLen = SECTORS_PER_BUFFER;
	u32 start_addr = 0;
	u32 end_addr = get_dd_total_sector_count();
	u64* buffer = prepare_write_buffer();
    int i=0, j=0, ret=0;
    
 	for (i = start_addr; i < end_addr; i += wLen) {
		if ( (i + wLen) > end_addr )
			wLen = end_addr - i;

        if (dd_write((uchar*)buffer, i, wLen)) {
    		return RD_ERR_WRITE_FAILED;
	    }

		if ((j % 20 == 0) || ((i + wLen) >= end_addr))
			plr_info("\r %u / %u", i + wLen - 1, end_addr - 1);
    		j++;
	}
    printf("\n");

    return ret;
}

static int random_write_and_sleep(int loop, int delay_msec)
{
    u64* buffer = NULL;
	u32 buf_offset = 0;
	u32 start_addr = g_plr_state.test_start_sector;
    u32 test_sector_length = g_plr_state.test_sector_length;
	u32 random_offset = 0;
	u32 i = 0;
	int ret = 0;
    u32 start_time=0, end_time=0;

	// set seed
	well512_write_seed(get_cpu_timer(0));

    printf("random write / sleep - start_addr: %d, test_sector_size: %d\n", start_addr, test_sector_length);

	for(;loop > 0; --loop) {
		buffer = prepare_random_write_buffer();
		
        if(loop%100 == 0)
			plr_info("\r loop %u / %u, ~%us", RANDOM_LOOP_CNT-loop+1, RANDOM_LOOP_CNT, end_time);
        start_time = get_rtc_time(); 

		for(i = 0; i < RANDOM_WRITE_CNT; ++i) {
		    // get random
		    random_offset = (well512_write_rand()*4*(K_BYTES/SECTOR_SIZE)) % (test_sector_length - 4*(K_BYTES/SECTOR_SIZE));
		    write_buffer(start_addr+random_offset, (u64*)((u8*)buffer+(buf_offset%BYTES_PER_BUFFER)), RANDOM_WRITE);
		    buf_offset = (buf_offset + 4*K_BYTES) % (8*M_BYTES);
		}
		ret = sleep_and_resume(delay_msec);
		if(ret){
			plr_err("failed to excute slee_and_resume : %d\n", ret);
			break;
		}
        end_time = get_rtc_time()-start_time;
	}

    buffer = prepare_write_buffer();

    return ret;
}

/* P/E Cycle */
#ifdef SJD_CAUSE_INTENDED_CRASH
static u32* prepare_crash_buffer(void)
{
	int i, j;
	u32 *xbuf 	= (u32*)plr_get_extra_buffer();
	u32 *p		= xbuf;

	for (i = 0; i < PAGES_PER_TEST_AREA; i++) {
		for (j = 0; j < WORDS_PER_PAGE; j++) {
			if( j == INTENDED_CRASH_WORD) {
				*(p + i * WORDS_PER_PAGE + j) = RD_MAGIC_NUMBER_INTENSIVE_CRASH;
			}
			else {
				*(p + i * WORDS_PER_PAGE + j) = RD_MAGIC_NUMBER;
			}
		}
	}

	return xbuf;
}
#endif
#if 0
static u32* prepare_ref_buffer(void)
{
	u32 *xbuf 	= (u32*)plr_get_extra_buffer();
	u32 *xbufe 	= (u32*)xbuf + WORDS_PER_BUFFER;
	u32 *p		= xbuf;
	while ( p != xbufe ) {
		*p++ = RD_MAGIC_NUMBER;
	}

	return xbuf;
}
#else
static u64* prepare_ref_buffer(void)
{
	u64 *xbuf 	= (u64*)plr_get_extra_buffer();
	u64 *xbufe 	= (u64*)xbuf + WORDS_PER_BUFFER;
	u64 *p		= xbuf;

	while ( p != xbufe ) {
		*p++ = RD_MAGIC_NUMBER;
	}
	return xbuf;
}
#endif

#if 0
static u32* prepare_random_ref_buffer(void)
{
	//u64 *xbuf 	= (u64*)plr_get_extra_buffer();
	u32 *xbuf 	= (u32*)plr_get_write_buffer();
	u32 *xbufe 	= (u32*)xbuf + WORDS_PER_BUFFER;
	u32 *p		= xbuf;

	well512_write_seed(get_cpu_timer(0));

	while ( p != xbufe ) {
		*p++ = (well512_write_rand());
	}

	return xbuf;
}
#else
static u64* prepare_random_ref_buffer(void)
{
	//u64 *xbuf 	= (u64*)plr_get_extra_buffer();
	u64 *xbuf 	= (u64*)plr_get_write_buffer();
	u64 *xbufe 	= (u64*)xbuf + WORDS_PER_BUFFER;
	u64 *p		= xbuf;

	well512_write_seed(get_cpu_timer(0));

	while ( p != xbufe ) {
		*p++ = (well512_write_rand());
	}

	return xbuf;
}
#endif

static int fill_read_disturb_area( void )
{
	int i = 0, j = 0;
	int ret = 0;
	int percentage = 100;
	u32 delay = 0;

	u64 *wbuf	= NULL;
	u32 wLen = SECTORS_PER_BUFFER;

	u32 start_addr = 0;
	u32 end_addr = get_dd_total_sector_count();
	//u32 repeat = g_plr_state.finish.value;
//	u32 repeat = PE_CYCLE;
	u32 repeat = 1;
    u32 start_time = 0, end_time = 0;

	delay = 86400*ADDITIONAL_DAYS_PER_1K/(end_addr/wLen)*percentage/100; //24*60*60*ADDITIONAL_DAYS_PER_1K/(end_addr/wLen)*10%;
	plr_info(" Set precondition : Erase & Write %u cycles (delay per write one : %u)(%u times 8MB write per cycle)\n", repeat, delay, end_addr/wLen);

	if (repeat < 1)
		return 0;

	/* Write Random pattern */
	while(--repeat) {
        start_time = get_rtc_time();

//		plr_info (" [Cycles : %u / %u]\n", (g_plr_state.finish.value - repeat), (g_plr_state.finish.value));
		plr_info (" [Cycles : %u / %u]\n", (PE_CYCLE-repeat), PE_CYCLE );
		// fill random pattern
		wbuf = prepare_random_ref_buffer();
		// Erase all
		//dd_erase_for_poff(0, end_addr, TYPE_SANITIZE);
		for (i = start_addr; i < end_addr; i += wLen) {
			if ( (i + wLen) > end_addr )
				wLen = end_addr - i;

			if (dd_write((uchar*)wbuf, i, wLen)) {
				return RD_ERR_WRITE_FAILED;
			}
//			/* P/E 3K process should be operated more that 3 weeks 
//			   So.. I add some delay for that */
			mdelay(delay);

			if ((j % 20 == 0) || ((i + wLen) >= end_addr))
            {
				plr_info("\r %u / %u", i + wLen - 1, end_addr - 1);
            }
			j++;
		}
        end_time = get_rtc_time();
	    plr_info("1 Cycle time : %us\n", (end_time-start_time));
		printf("\n");
	}

	/* Write known pattern */
	wbuf = prepare_ref_buffer();
//	plr_info (" [Last Cycles : %u / %u]\n", g_plr_state.finish.value, g_plr_state.finish.value);
	plr_info (" [Last Cycles : %u / %u]\n", (PE_CYCLE), PE_CYCLE );
	// Erase all
	//dd_erase_for_poff(0, end_addr, TYPE_SANITIZE);
    start_time = get_rtc_time();
	for (i = start_addr; i < end_addr; i += wLen) {
		if ( (i + wLen) > end_addr )
			wLen = end_addr - i;

		if (dd_write((uchar*)wbuf, i, wLen)) {
			return RD_ERR_WRITE_FAILED;
		}

		if ((j % 20 == 0) || ((i + wLen) >= end_addr))
			plr_info("\r %u / %u", i + wLen - 1, end_addr - 1);
		j++;
	}
	plr_info("Last Cycle time : %us\n", (end_time-start_time));
	printf("\n");

#ifdef SJD_CAUSE_INTENDED_CRASH

	plr_info(" Cause intentional crash for debugging ...\n");

	wbuf = prepare_crash_buffer();

	if (dd_write((u8*)wbuf, INTENDED_CRASH_PAGE * SECTORS_PER_PAGE, 1)) { // forced crash of page INTENDED_CRASH_PAGE
		ret = RD_ERR_WRITE_FAILED;
		return ret;
	}

#endif

	return ret;
}
/* P/E Cycle end */

#define RD_REPEAT_COUNT	1
static int compare_buffer(const u64* buf_ref, u64* buf, int word_len, int* word_pos)
{
	u64* 	p = buf;
	u64* 	pe = p + word_len;									// pe : sentinel for more speed
	u64*	p_ref = (u64*)buf_ref;

	while(p != pe) {
		if(*p++ != *p_ref++) {
			*word_pos = ((size_t)--p - (size_t)buf) / sizeof (u64);	// postion in buffer in words
			plr_info("word_pos = %d, data = 0x%16x, p = 0x%16X, buf = 0x%16X, buf_ref = 0x%16X\n", *word_pos, *p, p, buf, *buf_ref);
			return RD_ERR_READ_VERIFY_FAILED;
		}
	}
	return 0;	// Ok
}

static void memory_dump(u64* p0, int words)
{
	u64* p = (u64*)p0;
	u64* pe = p + words;

	int i = 0, j = 0;


	plr_info("Memory dump of a buffer: 0x%p...\n", p0);


	plr_info("\n    : ");

	for(;i < 8;i++) {

		plr_info("  %16x ", i);
	}

	plr_info("\n");


	i = 1; j = 0;

	plr_info("\n %03d: ", j);

	while(1) {

		u64 a = *p++;

		if (RD_MAGIC_NUMBER ==  a)
			plr_info("0x%16x ", a);
		else
			plr_err("0x%16x ", a);

		if(p == pe) break;

		if(!((i++ ) % 8)) {
			j++;
			plr_info("\n %03d: ", j);
		}
	}
	plr_info("\n");
}

static int read_uecc_verify(u64* ref_buf, int *sec_no_in_zone_p )
{
	int i = 0, j = 0;
	int ret = 0;
	int word_pos_in_buf;
	int secno_in_buf;
	int word_no_in_sec;

	u64 *xbuf 	= ref_buf;
	u64 *rbuf 	= (u64*)plr_get_read_buffer();

	u32 wLen = SECTORS_PER_BUFFER;
	u32 words_per_sectors = wLen * WORDS_PER_SECTOR;
	u32 start_addr = 0;
//	u32 end_addr = start_addr + g_plr_state.test_sector_length;
	u32 end_addr = get_dd_total_sector_count();
	u32 test_start_addr = g_plr_state.test_start_sector;
	u32 test_end_addr = 0;

	if ((start_addr + g_plr_state.test_sector_length) <= get_dd_total_sector_count())
		test_end_addr = start_addr + g_plr_state.test_sector_length;
	else
		test_end_addr = get_dd_total_sector_count();

	plr_info(" Start read verify!!\n");

	for (i = start_addr; i < end_addr; i += wLen) {
		if ( (i + wLen) > end_addr ) {
			wLen = end_addr - i;
			words_per_sectors = wLen* WORDS_PER_SECTOR;
		}

        if ( i >= test_start_addr && i < test_end_addr )
        {
            continue;
        }

		if (dd_read((uchar*)rbuf, i, wLen)) {
			return RD_ERR_READ_IO_FAILED;
		}

		if (compare_buffer(xbuf, rbuf, words_per_sectors, &word_pos_in_buf)) {
			secno_in_buf 		= word_pos_in_buf / WORDS_PER_SECTOR;	// sec_no in the buffer
			word_no_in_sec 		= word_pos_in_buf % WORDS_PER_SECTOR;
			plr_info("word_pos_in_buf:%d, secno_in_buf:  %d, word_no_in_sec: %d\n", word_pos_in_buf, secno_in_buf, word_no_in_sec);
			*sec_no_in_zone_p	= i + secno_in_buf;

			plr_info("i: %d, secno_in_buf: %d\n", i, secno_in_buf);

			plr_info("\nInvalid data at sector_number: %u, word_no_in_sec: %u\n", *sec_no_in_zone_p,  word_no_in_sec);
			plr_info("Crached data: 0x%16X\n", *(rbuf + word_pos_in_buf));
			plr_info("Memory dump just crash sector: %u(0x%x)\n", *sec_no_in_zone_p, *sec_no_in_zone_p);

			memory_dump(rbuf + (word_pos_in_buf - word_no_in_sec), 256);

			ret = RD_ERR_READ_VERIFY_FAILED;
			break;
		}
		if ((j % 10 == 0) || ((i + wLen) >= end_addr))
			plr_info("\r %u / %u", i + wLen - 1, end_addr - 1);
		j++;
	}
	printf("\n");

	return ret;
}

static int uecc_verification()	// 8 MB read buffer
{
	int 	i;
	int 	ret	= 0;
	u64* 	ref_buf;
	int crash_sec_no_in_zone = 0;

	ref_buf = (u64*)plr_get_write_buffer();

	for (i = 0; i < RD_REPEAT_COUNT; i++) {

		plr_info("\r #%d / #%d ", i+1, RD_REPEAT_COUNT);

		ret = read_uecc_verify(ref_buf, &crash_sec_no_in_zone);

		if (ret) {

			_rd_crash_info->sec_no_in_zone 	= crash_sec_no_in_zone;
			_rd_crash_info->repeat_no  		= i + 1;
			_rd_crash_info->boot_cnt		= g_plr_state.boot_cnt;

			verify_set_result(VERI_EREQUEST, crash_sec_no_in_zone);
			return VERI_EREQUEST;
		}
	}
	return VERI_NOERR;
}

static void sequential_write()
{
	int i=0, j=0, tot=0, loop=0;
	u32 len = SECTORS_PER_BUFFER;
	u32 start_addr = g_plr_state.test_start_sector;
	u32 new_start_addr = g_plr_state.test_start_sector;
	u32 end_addr = start_addr + g_plr_state.test_sector_length;
    u32 test_sector_length = (G_BYTES/SECTOR_SIZE);
	u64* buffer = NULL;
    u32 start_time=0, end_time=0;

    if(end_addr > get_dd_total_sector_count()) end_addr = get_dd_total_sector_count();

//    for(;loop<(K_BYTES*3);loop++){
    for(;loop<(3);loop++){
        printf("sequential write loop count: %d / %d, start_addr: %u, end_addr:%u, ~%us\n", loop+1, (K_BYTES*3), start_addr, end_addr, end_time);
        start_time = get_rtc_time();
		buffer = prepare_random_write_buffer();

    	for (tot=0, j=0, i = start_addr; tot < test_sector_length; tot+=len, i+=len, j++) {
		    if ( (i + len) > end_addr ){
		    	i = new_start_addr;
            }

		    write_buffer(i, buffer, SEQ_WRITE);
	
		    if ((j % 20 == 0) || ((tot + len) >= test_sector_length))
	    		plr_info("\r %u / %u", tot + len - 1, test_sector_length - 1);
    	}printf("\n");
        
        start_addr = i;
        end_time = get_rtc_time()-start_time;
   }
}

#define MIN_REQUEST_SECTORS	64
#define MAX_REQUEST_SECTORS 1024

enum CURRENT_REQUEST
{
	WRITE_RANDOM = 0,
	WRITE_4K,
};

static int pattern_interleaving(uchar *buf, uint start_lsn, uint request_sectors, op_pattern operate)
{
	int ret = 0;
	uint request_count	= 1;
	uint lsn			= start_lsn;
	uint next_lsn 		= 0;
	uint request_length	= request_sectors;
	uint part1_start_lsn= get_first_sector_in_zone(0);
	uint part1_last_lsn	= get_last_sector_in_zone((get_total_zone()/2) -1);
	uint part2_start_lsn= get_first_sector_in_zone(get_total_zone()/2);
	uint skip_size 		= part2_start_lsn - part1_start_lsn;
	enum CURRENT_REQUEST current_req = WRITE_RANDOM;

	if(start_lsn >= part1_last_lsn)
	{
		lsn -= skip_size;
		request_length = random_sector_count(4);
	}

	next_lsn = lsn + request_length;

	while(TRUE)
	{
		current_req = WRITE_RANDOM;
		if(lsn + request_length >= part1_last_lsn)
		{
			if(lsn >= part1_last_lsn)
				break;

			if(request_length <= MAX_REQUEST_SECTORS)
				request_length = part1_last_lsn - lsn;
		}

		while(TRUE)
		{
			if(request_count % 29 == 0)
				plr_info("\rlsn : 0x%08X, request size : %dKB, request num : %d", lsn, SECTORS_TO_SIZE_KB(request_length), request_count);

			ret = operate(buf, lsn, request_length, request_count, 0);
			if(ret != 0)
				return ret;

			request_count++;
			if(current_req == WRITE_4K)
				break;

			current_req = WRITE_4K;
			lsn += skip_size;
			request_length = NUM_OF_SECTORS_PER_PAGE;
		}

		lsn = next_lsn;
		if(g_plr_state.b_cache_enable == TRUE && request_count <= g_plr_state.cache_flush_cycle)
			request_length = MAX_REQUEST_SECTORS;
		else
			request_length = random_sector_count(4);

		next_lsn = lsn + request_length;
	}

	return ret;
}

static void init_pattern_1_2(struct plr_write_info* t_info)
{
	t_info->lsn			 	= get_first_sector_in_test_area();
	t_info->request_sectors	= NUM_OF_SECTORS_PER_PAGE << 4;
}

static int verify_customizing_00(uchar *buf, uint start_lsn, uint end_lsn, uint expected_loop)
{
	int ret 			= VERI_NOERR;
	uint s_lsn 			= 0;
	uint e_lsn			= 0;
	uint page_index		= 0;
	uint pages_in_chunk	=0;
	uint skip_size = get_first_sector_in_zone(get_total_zone()/2) - get_first_sector_in_zone(0);
	bool flag_find = FALSE;
	bool occurred_crash = FALSE;

	struct plr_header *header_info = NULL;

	VERIFY_DEBUG_MSG("param_start_lsn : 0x%08X, param_end_lsn : 0x%08X, expected loop : %d \n", start_lsn, end_lsn, expected_loop);

	if(start_lsn >= end_lsn)
		return VERI_NOERR;

	if(expected_loop <= 0)
		return VERI_NOERR;

	if(get_zone_num(start_lsn) >= get_total_zone()/2)
	{
		VERIFY_DEBUG_MSG("You can't reach here.\n");
		return VERI_NOERR;
	}

	ret = verify_find_chunk(buf, start_lsn, end_lsn, expected_loop, &s_lsn, &pages_in_chunk, &flag_find);
	if(ret != 0)
		return ret;

	if(flag_find == FALSE)
		return VERI_NOERR;

	VERIFY_DEBUG_MSG("s_lsn : 0x%08X, end_lsn : 0x%08X, skip size : 0x%08X \n", s_lsn, end_lsn, skip_size);

	while(s_lsn < end_lsn)
	{
		occurred_crash = FALSE;
		if(s_lsn % 2048 == 0)
			plr_info("\rCheck LSN, CRC, Loop(0x%08X, 0x%08X)", s_lsn, s_lsn + skip_size);

		ret = dd_read(buf, s_lsn + skip_size, NUM_OF_SECTORS_PER_PAGE);
		if(ret != PLR_NOERROR)
		{
			verify_set_result(VERI_EIO, s_lsn + skip_size);
			return VERI_EIO;
		}

		header_info = (struct plr_header *)(buf);
		ret = verify_check_page(header_info, s_lsn + skip_size);
		if(ret != VERI_NOERR)
			occurred_crash = TRUE;

		if(header_info->loop_cnt != expected_loop)
			occurred_crash = TRUE;

		if(occurred_crash == TRUE)
		{
			VERIFY_DEBUG_MSG("Crash LSN : 0x%08X \n", s_lsn  + skip_size);
			verify_set_result(VERI_EREQUEST, s_lsn  + skip_size);
			verify_insert_request_info(s_lsn  + skip_size, NUM_OF_SECTORS_PER_PAGE, NULL, VERI_EREQUEST);
		}
		else
			verify_insert_request_info(s_lsn  + skip_size, NUM_OF_SECTORS_PER_PAGE, NULL, VERI_NOERR);

		ret = dd_read(buf, s_lsn, MAX_REQUEST_SECTORS);
		if(ret != PLR_NOERROR)
		{
			verify_set_result(VERI_EIO, s_lsn);
			return VERI_EIO;
		}

		pages_in_chunk = ((struct plr_header *)(buf))->req_info.page_num;
		e_lsn = s_lsn + PAGES_TO_SECTORS(pages_in_chunk);
		while(s_lsn < e_lsn)
		{
			occurred_crash = FALSE;			
			header_info = (struct plr_header *)(buf + (page_index * PAGE_SIZE));
			ret = verify_check_page(header_info, s_lsn);
			if(ret != VERI_NOERR)
				occurred_crash = TRUE;

			if(header_info->loop_cnt != expected_loop)
				occurred_crash = TRUE;

			if(header_info->req_info.page_index != page_index)
				occurred_crash = TRUE;

			if(occurred_crash == TRUE)
			{
				VERIFY_DEBUG_MSG("Crash LSN : 0x%08X \n", s_lsn);
				verify_set_result(VERI_EREQUEST, s_lsn);
				verify_insert_request_info(s_lsn, PAGES_TO_SECTORS(pages_in_chunk), NULL, VERI_EREQUEST);
			}
			else 
				verify_insert_request_info(s_lsn, PAGES_TO_SECTORS(pages_in_chunk), NULL, VERI_NOERR);

			page_index++;
			s_lsn += NUM_OF_SECTORS_PER_PAGE;
		}

		page_index = 0;
	}

	return VERI_NOERR;
}

static int extra_verification(uchar *buf, struct plr_state *state)
{
	int ret = 0;
	uint selected_zone	= 0;
	uint start_lsn 		= 0;
	uint end_lsn 		= 0;
	uint current_zone_num = 0;//= get_zone_num(state->write_info.lsn);

	uint skip_size		= get_first_sector_in_zone(get_total_zone()/2) - get_first_sector_in_zone(0);
	uint adjustment_lsn = state->write_info.lsn;
	uint adjustment_last_lsn_in_packed = verify_get_predicted_last_lsn_previous_boot();

	if(adjustment_lsn >= get_first_sector_in_zone(get_total_zone()/2))
		adjustment_lsn -= skip_size;

	if(adjustment_last_lsn_in_packed >= get_first_sector_in_zone(get_total_zone()/2))
		adjustment_last_lsn_in_packed -= skip_size;

	current_zone_num = get_zone_num(adjustment_lsn);

	selected_zone = well512_rand() % (get_total_zone()/2);
	start_lsn 	= get_first_sector_in_zone(selected_zone);
	end_lsn 	= get_last_sector_in_zone(selected_zone) - get_reserved_sectors_in_zone();

	VERIFY_DEBUG_MSG("Predict SPO LSN : 0x%08X, selected zone num : %d, current zone num : %d \n", state->write_info.lsn, selected_zone, current_zone_num);

	if(selected_zone == current_zone_num)
	{
		end_lsn = adjustment_lsn;
		VERIFY_DEBUG_MSG("LSN and CRC, LOOP Check : Start LSN : 0x%08X, End LSN : 0x%08X, expected loop : %d \n", start_lsn, end_lsn, state->loop_cnt);
		ret = verify_customizing_00(buf, start_lsn, end_lsn, state->loop_cnt);
		if(ret != VERI_NOERR)
			return ret;

		if(state->b_packed_enable == TRUE || state->b_cache_enable == TRUE)
		{
			if(selected_zone == get_zone_num(adjustment_last_lsn_in_packed))
				start_lsn = adjustment_last_lsn_in_packed + MAX_REQUEST_SECTORS;
			else
				return VERI_NOERR;
		}
		else
			start_lsn = end_lsn + state->write_info.request_sectors;
		end_lsn = get_last_sector_in_zone(selected_zone) - get_reserved_sectors_in_zone();

		VERIFY_DEBUG_MSG("LSN and CRC, LOOP Check : Start LSN : 0x%08X, End LSN : 0x%08X, expected loop : %d \n", start_lsn, end_lsn, state->loop_cnt - 1);
		ret = verify_customizing_00(buf, start_lsn, end_lsn, state->loop_cnt - 1);
		if(ret != VERI_NOERR)
			return ret;
	}

	if(selected_zone < current_zone_num)
	{
		VERIFY_DEBUG_MSG("LSN and CRC, LOOP Check : Start LSN : 0x%08X, End LSN : 0x%08X, expected loop : %d \n", start_lsn, end_lsn, state->loop_cnt);
		ret = verify_customizing_00(buf, start_lsn, end_lsn, state->loop_cnt);
		if(ret != VERI_NOERR)
			return ret;
	}

	if(selected_zone > current_zone_num)
	{
		if((state->b_packed_enable == TRUE || state->b_cache_enable == TRUE) && selected_zone == get_zone_num(adjustment_last_lsn_in_packed))
			start_lsn = adjustment_last_lsn_in_packed + MAX_REQUEST_SECTORS;

		VERIFY_DEBUG_MSG("LSN and CRC, LOOP Check : Start LSN : 0x%08X, End LSN : 0x%08X, expected loop : %d \n", start_lsn, end_lsn, state->loop_cnt - 1);
		ret = verify_customizing_00(buf, start_lsn, end_lsn, state->loop_cnt - 1);
		if(ret != VERI_NOERR)
			return ret;
	}

	return VERI_NOERR;
}



/* ---------------------------------------------------------------------------------------
 * Common Api
 *----------------------------------------------------------------------------------------
 */
int initialize_dpin_0014( uchar * buf, uint test_area_start, uint test_area_length )
{
    int reset_cnt=0;
	int ret = 0;
	struct Pattern_Function pattern_linker;
    
	init_case_info(test_area_start, test_area_length);
	set_test_sectors_in_zone(get_sectors_in_zone());

	init_pattern(&g_plr_state, FALSE);

	memset(&pattern_linker, 0, sizeof(struct Pattern_Function));
	pattern_linker.do_pattern_1 = pattern_interleaving;		//call by verify, write
	pattern_linker.init_pattern_1 = init_pattern_1_2;
	pattern_linker.do_extra_verification = extra_verification;
	regist_pattern(pattern_linker);

	ret = check_pattern_func();
	if(ret)
		return ret;

#if 1
#if 0
	if (g_plr_state.finish.type == 0 && g_plr_state.boot_cnt > g_plr_state.finish.value)
	{
		// To-do real_workload
		/* Read-workload write */

		g_plr_state.finish.value = 36;	//real_workload day
		g_plr_state.boot_cnt = 1;
		plr_info_highlight("initialize_daxx_0011 Call\n");
		ret = initialize_daxx_0011(buf, test_area_start, test_area_length);
		if(ret)
			plr_err("failed to excute initialize_daxx_0011 func - %d\n", ret);

		/* Read-Disturb Test*/
		g_plr_state.test_start_sector = 0;
		plr_info_highlight("read_daxx_0011 Call\n");
		ret = read_daxx_0011(buf, test_area_start, test_area_length);
		if(ret)
			plr_err("failed to excute read_daxx_0011 func - %d\n", ret);

		plr_info_highlight("write_daxx_0011 Call\n");
		ret = write_daxx_0011(buf, test_area_start, test_area_length);
		if(ret)
			plr_err("failed to excute write_daxx_0011 func - %d\n", ret);

		plr_info_highlight("+++++++real_workload Done++++++++\n");
		return ret;
	}
#endif

    if( g_plr_state.boot_cnt == (g_plr_state.finish.value/2)+1 || 
            g_plr_state.boot_cnt == (g_plr_state.finish.value)+1 )
    {
        /* Check UECC  */
        plr_info("check UECC\n");
#if 0
        if(uecc_verification() == VERI_EREQUEST) 
        {
            plr_err("UECC (maybe) occurred!!!");
            g_plr_state.b_finish_condition = TRUE;
            return VERI_EREQUEST;
        }
#endif
    }

    /* check sw_reset_cnt */
    get_sw_reset_cnt(&reset_cnt);
    plr_info("[%s]\ncheck reset_cnt : %d\n", __func__, reset_cnt);
    if( reset_cnt || g_plr_state.boot_cnt == (g_plr_state.finish.value)+1 )
    {
        if( reset_cnt == (g_plr_state.finish.value/2)+1 )
        {
            plr_info("RESET finish\n");
            g_plr_state.b_finish_condition = 1;
            return 0;
        }

        plr_info("send_token RESET\n");
        send_token(PLRTOKEN_RESET, "1");
        set_sw_reset_cnt(++reset_cnt);

        plr_info("RESET cmd Call\n");
        reset_board(0);
        plr_info("SW_RESET Done\n");
    }

    if( g_plr_state.boot_cnt == 1 )
    {
        /* 600 P/E Cycle (3K 20%) */
		plr_info("P/E cycle\n");
        fill_read_disturb_area();
		if( ret < 0 )
			plr_err("failed to fill_area - %d\n", ret);
    }

	if(	g_plr_state.boot_cnt == 1 || g_plr_state.boot_cnt == ((g_plr_state.finish.value)/2)+1 )
	{
		/* random write + sleep & resume */
		plr_info("random write and sleep/resume\n");
		ret = random_write_and_sleep(RANDOM_LOOP_CNT, SLEEP_DELAY_MSEC);	//to-do 11.5days
		if(ret)
			plr_err("failed to execute random_write_and_sleep func - %d\n", ret);

		/* 3TB Seq write  */
		plr_info("3TB seq write\n");
		sequential_write();

		plr_info("write known pattern\n");
//		ret = known_pattern_write();
		if(ret)
			plr_err("failed to excute known_pattern_write func - %d\n", ret);

        /* Erase test area before SPOR TEST */
    	ret = plr_prepare(test_area_start, test_area_length, FALSE);
    	if(ret)
    		plr_err("failed to execute plr_prepare func - %d\n", ret);
	}

#else
/* Skip precondition for continue the test which stopped by EMP shock */
#endif

	return ret;
}

int read_dpin_0014( uchar * buf, uint test_start_sector, uint test_sector_length )
{
	return verify_pattern(buf);
}

int write_dpin_0014( uchar * buf, uint test_start_sector, uint test_sector_length )
{
	return write_pattern(buf);
}
