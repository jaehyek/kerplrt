
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

// @sj 150706: Note, This Test Case is buildup upon TestCase DPIN_0004.


/* 	DAXX_0008


	W	= write (default)
	J	= jump
	R	= random

	PATTERN_1, zone interleaving.
		zone 1, 	4K(W)	4K(W)	4K(W)	4K(W)	4K(W)	4K(W)	...	4K(W)
		zone 2, 	4K(W)	4K(W)	4K(W)	4K(W)	4K(W)	4K(W)	...	4K(W)
			.
			.
			.
		zone N-1, 	4K(W)	4K(W)	4K(W)	4K(W)	4K(W)	4K(W)	...	4K(W)


	PATTERN_2, sequencial
		zone 1,	4K(W) 	4K(W)	4K(W)	4K(W)	4K(W)	4K(W)	....	4K(W)
		zone 2, 	4K(W) 	4K(W)	4K(W)	4K(W)	4K(W)	4K(W)	....	4K(W)
			.
			.
			.
		zone N-1,	4K(W) 	4K(W)	4K(W)	4K(W)	4K(W)	4K(W)	....	4K(W)

 */

#include "../plr_deviceio.h"
#include "../plr_common.h"
#include "../plr_err_handling.h"
#include "../plr_protocol.h"
#include "../plr_case_info.h"
#include "../plr_pattern.h"
#include "../plr_verify.h"
#include "../plr_verify_log.h"
//#include "target.h"

#define CHUNK_SIZE	NUM_OF_SECTORS_PER_PAGE


//@sj 150716, I recommend you to introduce following macros to common.h

#ifndef BYTES_PER_BUFFER

#define BUFFER_SIZE				(0x800000)			//@sj 150714, Note: No buffer size is explicitely defined anywhere.
													//If anyone defined the buffer size in global configuration header,
													//this value may changed to the value.

#define BYTES_PER_BUFFER		(BUFFER_SIZE)							// clearer and chainable !
#define WORDS_PER_BUFFER		(BYTES_PER_BUFFER/sizeof(u32))			//
#define BYTES_PER_TEST_AREA		(BUFFER_SIZE)							// Test area is 8 MB
#define BYTES_PER_SECTOR		(SECTOR_SIZE)							// 0x200 	(512)
#define WORDS_PER_SECTOR		(BYTES_PER_SECTOR/sizeof(u32))			// 0x80 	(128)
#define SECTORS_PER_BUFFER		(BYTES_PER_BUFFER / BYTES_PER_SECTOR)	// 0x4000	(16384)
#define SECTORS_PER_PAGE		(NUM_OF_SECTORS_PER_PAGE)				// 0x8		(8)
#define BYTES_PER_PAGE			(BYTES_PER_SECTOR * SECTORS_PER_PAGE)	// 0x1000 	(4096)
#define WORDS_PER_PAGE			(BYTES_PER_PAGE/sizeof(u32))			// 0x400	(1024)
#define PAGES_PER_ZONE			(ZONE_SIZE_X_PAGE)						// 0x4000 	(16384)
#define SECTORS_PER_ZONE		(SECTORS_PER_PAGE * PAGES_PER_ZONE)		// 0x20000 	(131072)
#define BUFFERS_PER_ZONE		(SECTORS_PER_ZONE / SECTORS_PER_BUFFER)
#define PAGES_PER_TEST_AREA		(BYTES_PER_TEST_AREA/BYTES_PER_PAGE)
#define SECTORS_PER_TEST_AREA	(BYTES_PER_TEST_AREA/BYTES_PER_SECTOR)
#define BUFFERS_PER_TEST_AREA	(BYTES_PER_TEST_AREA/BYTES_PER_BUFFER)
#define PAGES_PER_BUFFER		(BYTES_PER_BUFFER / BYTES_PER_PAGE)




#endif



//
// MACROS for read disturbance
//

//#define SJD_DEVELOPMENT_MODE

#ifdef SJD_DEVELOPMENT_MODE

#define RD_PAGES 							(PAGES_PER_TEST_AREA)
#define RD_REPEAT_COUNT					(3)
#define SJD_CAUSE_INTENDED_CRASH

#else

#define RD_PAGES 							(PAGES_PER_TEST_AREA)
#define RD_REPEAT_COUNT					(1000)

#endif

#define	COMPARE_FIRST_4BYTE_ONLY			(FALSE)	// for fast comparison

#define RD_ERR_READ_VERIFY_FAILED			(-1)
#define RD_ERR_WRITE_FAILED				(-2)
#define RD_ERR_INVALID_ZONE_NUMBER		(-3)
#define RD_ERR_READ_IO_FAILED				(-4)

#define RD_MAGIC_NUMBER 					(0xEFCAFE01UL)

#ifdef 	SJD_CAUSE_INTENDED_CRASH

#define RD_MAGIC_NUMBER_INTENSIVE_CRASH		(0xDEAD1234)
#define INTENDED_CRASH_PAGE					(2)
#define INTENDED_CRASH_WORD					(0)	// crash word in the page, must b2 0 for COMPARE_FIRST_4BYTE_ONLY


#endif

typedef struct read_disturb_crash_info
{
	int					err_code;		// error code, reserved
	int 				sec_no_in_zone;	// sector number in a zone
	int 				repeat_no;		// repeat number in a boot
	int					boot_cnt;		// boot count

}  read_disturb_crash_info_s;

struct workload_cmd_line{
    long 				delay;
    long 				cmd;
    long 				sector_cnt;
};

struct workload_list{
	u32 delay;
	u32 cmd;
	u32 sector_cnt;
};


static read_disturb_crash_info_s _rd_crash_info[1];	// read_disturb_crash_info;
static int rd_loop_count = 0;

//
// Helper FUNCTIONS for read disturbance
//

static int compare_buffer(const u32* buf_ref, u32* buf, int word_len, int* word_pos)
{
	u32* 	p = buf;
	u32* 	pe = p + word_len;									// pe : sentinel for more speed
	u32*	p_ref = (u32*)buf_ref;

#if COMPARE_FIRST_4BYTE_ONLY == TRUE
	while(p != pe) {
		if(*p != *p_ref) {
			*word_pos = ((size_t)p - (size_t)buf) / sizeof (u32);	// postion in buffer in words
			plr_debug("word_pos = %d, data = 0x%08x, p = 0x%08X, buf = 0x%08X\n", *word_pos, *p, p, buf);
			return RD_ERR_READ_VERIFY_FAILED;
		}
		p += WORDS_PER_SECTOR;
		p_ref += WORDS_PER_SECTOR
	}
#else
	while(p != pe) {
		if(*p++ != *p_ref++) {
			*word_pos = ((size_t)--p - (size_t)buf) / sizeof (u32);	// postion in buffer in words
			plr_debug("word_pos = %d, data = 0x%08x, p = 0x%08X, buf = 0x%08X\n", *word_pos, *p, p, buf);
			return RD_ERR_READ_VERIFY_FAILED;
		}
	}
#endif
	return 0;	// Ok

}
static void memory_dump(u32* p0, int words)
{
	u32* p = (u32*)p0;
	u32* pe = p + words;

	int i = 0, j = 0;


	plr_info("Memory dump of a buffer: 0x%p...\n", p0);


	plr_info("\n    : ");

	for(;i < 8;i++) {

		plr_info("  %8x ", i);
	}

	plr_info("\n");


	i = 1; j = 0;

	plr_info("\n %03d: ", j);

	while(1) {

		u32 a = *p++;

		if (RD_MAGIC_NUMBER ==  a)
			plr_info("0x%08x ", a);
		else
			plr_err("0x%08x ", a);

		if(p == pe) break;

		if(!((i++ ) % 8)) {
			j++;
			plr_info("\n %03d: ", j);
		}
	}
	plr_info("\n");
}

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

static u32 caculate_kb_per_sec(u32 total_size, u64 elapsed_ticks)
{
	u32 msec = 0;
	u32 byte_per_ms = 0;

	msec = get_tick2msec(elapsed_ticks);

	if (msec < 1)
		msec = 1;

	byte_per_ms = (u64)(total_size) * 512 / msec;

	return (u32)(byte_per_ms * 1000UL / 1024UL);
}

static int read_disturb_verify(u32* ref_buf, int *sec_no_in_zone_p, int b_verify)
{
	int i = 0, j = 0;
	int ret = 0;
	int word_pos_in_buf;
	int secno_in_buf;
	int word_no_in_sec;

	u32 *xbuf 	= ref_buf;
	u32 *rbuf 	= (u32*)plr_get_read_buffer();

	u32 wLen = SECTORS_PER_BUFFER;
	u32 words_per_sectors = wLen * WORDS_PER_SECTOR;
	u32 start_addr = g_plr_state.test_start_sector;
	u32 end_addr = 0; //get_dd_total_sector_count();
	u64 start_ticks = 0, end_ticks = 0;

	/* Elapsed time : 80 sec per 1GB */
	if ((start_addr + g_plr_state.test_sector_length) <= get_dd_total_sector_count())
		end_addr = start_addr + g_plr_state.test_sector_length;
	else
		end_addr = get_dd_total_sector_count();

	if (b_verify)
		plr_info(" Start read verify!!\n");

	start_ticks = get_tick_count64();

	for (i = start_addr; i < end_addr; i += wLen) {
		if ( (i + wLen) > end_addr ) {
			wLen = end_addr - i;
			words_per_sectors = wLen* WORDS_PER_SECTOR;
		}

		if (IS_SLEEP_N_AWAKE_TEST) {
#if 0
			if(dd_sleep())
				return -1;
			if (dd_awake())
				return -1;
#endif 
			if (dd_suspend(3))
				return -1;
		}

		if (dd_read((uchar*)rbuf, i, wLen)) {
			return RD_ERR_READ_IO_FAILED;
		}

		if (b_verify) {
			if (compare_buffer(xbuf, rbuf, words_per_sectors, &word_pos_in_buf)) {

				secno_in_buf 		= word_pos_in_buf / WORDS_PER_SECTOR;	// sec_no in the buffer
				word_no_in_sec 		= word_pos_in_buf % WORDS_PER_SECTOR;
				plr_debug("word_pos_in_buf:%d, secno_in_buf:  %d, word_no_in_sec: %d\n", word_pos_in_buf, secno_in_buf, word_no_in_sec);
				*sec_no_in_zone_p	= i + secno_in_buf;

				plr_debug("i: %d, secno_in_buf: %d\n", i, secno_in_buf);

				plr_info("\nInvalid data at sector_number: %u, word_no_in_sec: %u\n", *sec_no_in_zone_p,  word_no_in_sec);
				plr_info("Crached data: 0x%08X\n", *(rbuf + word_pos_in_buf));
				plr_info("Memory dump just crash sector: %u(0x%x)\n", *sec_no_in_zone_p, *sec_no_in_zone_p);

				memory_dump(rbuf + (word_pos_in_buf - word_no_in_sec), 256);

				ret = RD_ERR_READ_VERIFY_FAILED;
				break;
			}
		}
		if ((j % 10 == 0) || ((i + wLen) >= end_addr))
			plr_info("\r %u / %u", i + wLen - 1, end_addr - 1);
		j++;
	}
	printf("\n");

	end_ticks = get_tick_count64();

	plr_info(" Read disturb speed : %u KB/Sec\n",
		caculate_kb_per_sec(end_addr - start_addr, end_ticks - start_ticks));

	return ret;
}

static u32* prepare_random_ref_buffer(void)
{
	//u64 *xbuf 	= (u32*)plr_get_extra_buffer();
	u32 *xbuf 	= (u32*)plr_get_write_buffer();
	u32 *xbufe 	= (u32*)xbuf + WORDS_PER_BUFFER;
	u32 *p		= xbuf;

	well512_write_seed(get_cpu_timer(0));

	while ( p != xbufe ) {
		*p++ = (well512_write_rand());
	}

	return xbuf;
}

static u32* prepare_random_ref_sector(void)
{
	u32 *xbuf 	= (u32*)plr_get_extra_buffer();
	u32 *xbufe 	= (u32*)xbuf + WORDS_PER_SECTOR;
	u32 *p		= xbuf;

	well512_write_seed(get_cpu_timer(0));

	while ( p != xbufe ) {
		*p++ = (well512_write_rand());
	}

	return xbuf;
}
static int workload_resume()
{
	// VCC_ON
//	turn_on_vcc();
#if 0
	// CMD0 -> CMD1 -> CMD2 -> CMD3 -> CMD9 -> CMD7
//	return (int)dd_cmd0_awake();
#else
	// CMD5 -> CMD7
	return (int)dd_awake();
#endif

	mdelay(1000);
}

static int workload_sleep()
{
	// send CMD5
	dd_sleep();
	// VCC_OFF
//	turn_off_vcc();
	// delay
    
    return 0;
}

static int workload_suspend()
{
   return dd_suspend(3);
}

// write unit = sector
static int workload_write(u32 sector_cnt, u32 random_offset, u32* wbuf){
	int i=0;
	u32 wLen = SECTORS_PER_BUFFER;
	u32 end_addr = random_offset+sector_cnt;

	for (i = random_offset; i < end_addr; i+=wLen) {
		
		if ( (i + wLen) > end_addr )
			wLen = end_addr - i;

		if (dd_write((uchar*)wbuf, i, wLen)) {
			return RD_ERR_WRITE_FAILED;
		}
	}
	
	return 0;
}

// read unit = sector
static int workload_read(u32 sector_cnt, u32 random_offset, u32* rbuf){

		if (dd_read((uchar*)rbuf, random_offset, sector_cnt)) {
			return RD_ERR_READ_IO_FAILED;
		}
		
		return 0;
}

static int do_workload( void )
{
    int i=0, j=0;
	int ret = 0;
	u32 delay = 0;
    u32 wLen = SECTORS_PER_BUFFER;

	u32 *wbuf	= NULL;
	u32 *rbuf = (u32*)plr_get_read_buffer();
	
	u32 start_addr = 0;
	u32 end_addr = 0;
	u32 random_offset = 0;
	u32 repeat = 450*g_plr_state.finish.value;
	int now_sleep = 0;

//	u64 start_time = 0, end_time = 0;
   #if 0 
    struct workload_list workload_cmd_list[400] = {{5032,25,8},{1368277,18,40},{1661509,25,8},{668,25,16},{613,25,8},{483,25,8},{478,25,1849},{2367459,25,96},{5047,25,8},{3455756,25,1265},{15041,25,65},{616157,25,8},{1588,25,96},{3775,25,8},{21654,25,8},{3081,25,80},{3412,25,8},{2449424,25,8},{2052,25,120},{3075,25,8},{2188169,25,2201},{25408,25,121},{3073239,25,64},{2704,25,8},{1997251,25,521},{3657810,25,56},{3270,25,8},{1349492,25,8},{1125,25,585},{3779575,25,8},{2457,25,112},{4203,25,8},{5775,25,8},{2270,25,80},{7499,25,8},{1204480,25,41},{4180,25,1433},{3823948,25,112},{3051,25,8},{1182957,25,585},{4231170,25,56},{12725,25,8},{762844,25,105},{2785,25,328},{5246,25,8},{4631807,25,48},{2844,25,8},{365099,25,361},{1367880,25,57},{8226,25,49},{5512,25,8},{35784,25,8},{2485,25,120},{4598,25,8},{63058,25,8},{13837,25,112},{10627,25,8},{3499787,25,73},{17296,25,1969},{1569822,25,128},{3238,25,8},{3440998,25,577},{2213401,25,56},{2812,25,8},{499686,25,8},{4204,25,96},{3777,25,8},{8653,25,8},{1239,25,80},{2617,25,8},{2284375,25,1513},{17760,25,73},{2965863,25,136},{12832,25,8},{2983486,25,736},{10095,25,48},{1530,25,145},{2005850,25,48},{3287,25,8},{2989743,25,537},{2558369,25,48},{2318,25,8},{2434019,25,57},{4801,25,49},{4979,25,8},{43592,25,8},{1388,25,40},{1863,25,8},{50774,25,8},{2959,25,40},{1703,25,8},{1851992,25,1561},{393802,18,8},{3219198,25,8},{765,25,104},{4409,25,8},{947745,25,8},{1442,25,48},{1444,25,8},{437631,25,8},{893,25,16},{318,25,8},{304,25,896},{10233,25,64},{594187,25,8},{4488,25,72},{2428,25,8},{443187,25,8},{1753,25,48},{2739,25,8},{218541,25,8},{1010,25,48},{1470,25,8},{285229,25,8},{1038,25,48},{1395,25,8},{156794,25,8},{640,25,40},{1500,25,8},{189887,25,8},{4700,25,40},{1853,25,8},{83935,25,8},{21873,25,8},{3201,25,8},{10460,25,104},{21229,25,8},{3888,25,16},{1715,25,48},{1940,25,8},{8881,25,8},{3258,25,40},{5872,25,8},{20654,25,8},{4589,25,40},{1456,25,8},{17476,25,8},{4539,25,40},{1603,25,8},{20023,25,8},{4614,25,40},{1858,25,8},{14973,25,8},{1484,25,40},{1563,25,8},{25620,25,8},{1837,25,40},{1465,25,8},{1465,18,4},{27349,25,8},{1451,25,40},{3060,25,8},{15653,25,8},{579,25,8},{1103,25,8},{2470,25,8},{4539,25,713},{14380,25,8},{2353,25,88},{3643,25,8},{4802,25,8},{2265,25,40},{9611,25,8},{17391,25,8},{3505,25,40},{2654,25,8},{31285,25,8},{2481,25,48},{3178,25,8},{30520,25,8},{1502,25,40},{3732,25,8},{17638,25,8},{2225,25,40},{9966,25,8},{25398,25,8},{7045,25,40},{3148,25,8},{11144,25,8},{1396,25,40},{1673,25,8},{27468,25,8},{1940,25,40},{4691,25,8},{21408,25,8},{1087,25,40},{1382,25,8},{36559,25,8},{1145,25,40},{1712,25,8},{38134,25,8},{1731,25,40},{1497,25,8},{36551,25,8},{1580,25,40},{1655,25,8},{26177,25,8},{1579,25,40},{4504,25,8},{18526,25,8},{1151,25,40},{1560,25,8},{46035,25,8},{1411,25,40},{1417,25,8},{39428,25,8},{2246,25,40},{1570,25,8},{30452,25,8},{1193,25,40},{4369,25,8},{24916,25,8},{1336,25,40},{1452,25,8},{30724,25,8},{24127,25,8},{2603,25,40},{1553,25,8},{26542,25,8},{1684,25,40},{1554,25,8},{26957,25,8},{1573,25,40},{1425,25,8},{86461,25,8},{4231,25,40},{1691,25,8},{104814,25,8},{4002,25,40},{1319,25,8},{84128,25,8},{1067,25,40},{3371,25,8},{135010,25,8},{1965,25,48},{1784,25,8},{66149,25,8},{1395,25,40},{2957,25,8},{106040,25,8},{1446,25,40},{2792,25,8},{115242,25,8},{1141,25,40},{3019,25,8},{86992,25,8},{1357,25,40},{3214,25,8},{24386,25,8},{1363,25,40},{2102,25,8},{39536,25,8},{4083,25,40},{1596,25,8},{51386,25,8},{1396,25,40},{1867,25,8},{87039,25,8},{2837,25,40},{5427,25,8},{51777,25,8},{3454,25,40},{1646,25,8},{55077,25,8},{1541,25,40},{1326,25,8},{56830,25,8},{2289,25,40},{3154,25,8},{44659,25,8},{4273,25,40},{1568,25,8},{47980,25,8},{1896,25,40},{1655,25,8},{53683,25,8},{1585,25,40},{1778,25,8},{56288,25,8},{1043,25,40},{1453,25,8},{46483,25,8},{1973,25,40},{1436,25,8},{47160,25,8},{1163,25,40},{1566,25,8},{43583,25,8},{1540,25,40},{3310,25,8},{49132,25,8},{2598,25,40},{1899,25,8},{44663,25,8},{1878,25,40},{2056,25,8},{56752,25,8},{1404,25,40},{2012,25,8},{46291,25,8},{2987,25,40},{9380,25,8},{39471,25,8},{3572,25,40},{1419,25,8},{2083105,25,73},{1591,25,536},{6740,25,41},{153360,18,15},{5129,18,1311},{5402879,18,23},{10345,18,60},{9396,18,17},{298606,18,13},{3382,18,20},{5061,18,168},{24879,18,22},{5231,18,551},{14721,18,24},{848,18,33},{854,18,124},{3156,18,57},{1491,18,10},{177,18,20},{707,18,72},{1867,18,56},{1462,18,177},{234628,18,21},{7990,18,274},{9043,18,105},{699127,18,21},{7931,18,1105},{573077,5,0},{299503,18,4},{878,18,4},{119472,18,5},{508,18,4},{1028,18,4},{789,18,4},{779,18,4},{798,18,4},{772,18,4},{774,18,4},{241,18,8},{153,18,62904},{113989,18,4},{866031,25,4},{1254,18,4},{349,25,4},{695707,18,15},{5132,18,1311},{404294,18,23},{10432,18,60},{9548,18,17},{299052,18,12},{3452,18,21},{5061,18,169},{24812,18,22},{5200,18,551},{14616,18,24},{846,18,34},{852,18,125},{3146,18,56},{1477,18,11},{571733,5,0},{179,18,19},{698,18,72},{1858,18,55},{1448,18,176},{234083,18,21},{7936,18,276},{9010,18,103},{697671,18,22},{7965,18,1105},{298816,18,4},{880,18,4},{119433,18,5},{507,18,4},{1028,18,4},{786,18,4},{777,18,4},{768,18,4},{202,18,4},{151,18,62904},{113989,18,4},{865394,25,4},{1194,18,4},{351,25,4},{1077,25,40},{1643,25,8},{67173,25,8},{3956,25,40},{2152,25,8},{75281,25,8},{1416,25,40},{1523,25,8},{65095,25,8},{1431,25,48},{2895,25,8},{66172,25,8}};

#endif
    struct workload_list workload_cmd_list[400] = {{5032,25,8},{1368277,18,40},{1661509,25,8},{668,25,16},{613,25,8},{483,25,8},{478,25,1849},{2367459,25,96},{5047,25,8},{3455756,25,1265},{15041,25,65},{616157,25,8},{1588,25,96},{3775,25,8},{21654,25,8},{3081,25,80},{3412,25,8},{2449424,25,8},{2052,25,120},{3075,25,8},{2188169,25,2201},{25408,25,121},{3073239,25,64},{2704,25,8},{1997251,25,521},{3657810,25,56},{3270,25,8},{1349492,25,8},{1125,25,585},{3779575,25,8},{2457,25,112},{4203,25,8},{5775,25,8},{2270,25,80},{7499,25,8},{1204480,25,41},{4180,25,1433},{3823948,25,112},{3051,25,8},{1182957,25,585},{4231170,25,56},{12725,25,8},{762844,25,105},{2785,25,328},{5246,25,8},{4631807,25,48},{2844,25,8},{365099,25,361},{1367880,25,57},{8226,25,49},{5512,25,8},{35784,25,8},{2485,25,120},{4598,25,8},{63058,25,8},{13837,25,112},{10627,25,8},{3499787,25,73},{17296,25,1969},{1569822,25,128},{3238,25,8},{3440998,25,577},{2213401,25,56},{2812,25,8},{499686,25,8},{4204,25,96},{3777,25,8},{8653,25,8},{1239,25,80},{2617,25,8},{2284375,25,1513},{17760,25,73},{2965863,25,136},{12832,25,8},{2983486,25,736},{10095,25,48},{1530,25,145},{2005850,25,48},{3287,25,8},{2989743,25,537},{2558369,25,48},{2318,25,8},{2434019,25,57},{4801,25,49},{4979,25,8},{43592,25,8},{1388,25,40},{1863,25,8},{50774,25,8},{2959,25,40},{1703,25,8},{1851992,25,1561},{393802,18,8},{3219198,25,8},{765,25,104},{4409,25,8},{947745,25,8},{1442,25,48},{1444,25,8},{437631,25,8},{893,25,16},{318,25,8},{304,25,896},{10233,25,64},{594187,25,8},{4488,25,72},{2428,25,8},{443187,25,8},{1753,25,48},{2739,25,8},{218541,25,8},{1010,25,48},{1470,25,8},{285229,25,8},{1038,25,48},{1395,25,8},{156794,25,8},{640,25,40},{1500,25,8},{189887,25,8},{4700,25,40},{1853,25,8},{83935,25,8},{21873,25,8},{3201,25,8},{10460,25,104},{21229,25,8},{3888,25,16},{1715,25,48},{1940,25,8},{8881,25,8},{3258,25,40},{5872,25,8},{20654,25,8},{4589,25,40},{1456,25,8},{17476,25,8},{4539,25,40},{1603,25,8},{20023,25,8},{4614,25,40},{1858,25,8},{14973,25,8},{1484,25,40},{1563,25,8},{25620,25,8},{1837,25,40},{1465,25,8},{1465,18,4},{27349,25,8},{1451,25,40},{3060,25,8},{15653,25,8},{579,25,8},{1103,25,8},{2470,25,8},{4539,25,713},{14380,25,8},{2353,25,88},{3643,25,8},{4802,25,8},{2265,25,40},{9611,25,8},{17391,25,8},{3505,25,40},{2654,25,8},{31285,25,8},{2481,25,48},{3178,25,8},{30520,25,8},{1502,25,40},{3732,25,8},{17638,25,8},{2225,25,40},{9966,25,8},{25398,25,8},{7045,25,40},{3148,25,8},{11144,25,8},{1396,25,40},{1673,25,8},{27468,25,8},{1940,25,40},{4691,25,8},{21408,25,8},{1087,25,40},{1382,25,8},{36559,25,8},{1145,25,40},{1712,25,8},{38134,25,8},{1731,25,40},{1497,25,8},{36551,25,8},{1580,25,40},{1655,25,8},{26177,25,8},{1579,25,40},{4504,25,8},{18526,25,8},{1151,25,40},{1560,25,8},{46035,25,8},{1411,25,40},{1417,25,8},{39428,25,8},{2246,25,40},{1570,25,8},{30452,25,8},{1193,25,40},{4369,25,8},{24916,25,8},{1336,25,40},{1452,25,8},{30724,25,8},{24127,25,8},{2603,25,40},{1553,25,8},{26542,25,8},{1684,25,40},{1554,25,8},{26957,25,8},{1573,25,40},{1425,25,8},{86461,25,8},{4231,25,40},{1691,25,8},{104814,25,8},{4002,25,40},{1319,25,8},{84128,25,8},{1067,25,40},{3371,25,8},{135010,25,8},{1965,25,48},{1784,25,8},{66149,25,8},{1395,25,40},{2957,25,8},{106040,25,8},{1446,25,40},{2792,25,8},{115242,25,8},{1141,25,40},{3019,25,8},{86992,25,8},{1357,25,40},{3214,25,8},{24386,25,8},{1363,25,40},{2102,25,8},{39536,25,8},{4083,25,40},{1596,25,8},{51386,25,8},{1396,25,40},{1867,25,8},{87039,25,8},{2837,25,40},{5427,25,8},{51777,25,8},{3454,25,40},{1646,25,8},{55077,25,8},{1541,25,40},{1326,25,8},{56830,25,8},{2289,25,40},{3154,25,8},{44659,25,8},{4273,25,40},{1568,25,8},{47980,25,8},{1896,25,40},{1655,25,8},{53683,25,8},{1585,25,40},{1778,25,8},{56288,25,8},{1043,25,40},{1453,25,8},{46483,25,8},{1973,25,40},{1436,25,8},{47160,25,8},{1163,25,40},{1566,25,8},{43583,25,8},{1540,25,40},{3310,25,8},{49132,25,8},{2598,25,40},{1899,25,8},{44663,25,8},{1878,25,40},{2056,25,8},{56752,25,8},{1404,25,40},{2012,25,8},{46291,25,8},{2987,25,40},{9380,25,8},{39471,25,8},{3572,25,40},{1419,25,8},{2083105,25,73},{1591,25,536},{6740,25,41},{153360,18,15},{5129,18,1311},{5402879,18,23},{10345,18,60},{9396,18,17},{298606,18,13},{3382,18,20},{5061,18,168},{24879,18,22},{5231,18,551},{14721,18,24},{848,18,33},{854,18,124},{3156,18,57},{1491,18,10},{177,18,20},{707,18,72},{1867,18,56},{1462,18,177},{234628,18,21},{7990,18,274},{9043,18,105},{699127,18,21},{7931,18,1105},{573077,5,0},{299503,18,4},{878,18,4},{119472,18,5},{508,18,4},{1028,18,4},{789,18,4},{779,18,4},{798,18,4},{772,18,4},{774,18,4},{241,18,8},{153,18,6},{113989,18,4},{866031,25,4},{1254,18,4},{349,25,4},{695707,18,15},{5132,18,1311},{404294,18,23},{10432,18,60},{9548,18,17},{299052,18,12},{3452,18,21},{5061,18,169},{24812,18,22},{5200,18,551},{14616,18,24},{846,18,34},{852,18,125},{3146,18,56},{1477,18,11},{571733,5,0},{179,18,19},{698,18,72},{1858,18,55},{1448,18,176},{234083,18,21},{7936,18,276},{9010,18,103},{697671,18,22},{7965,18,1105},{298816,18,4},{880,18,4},{119433,18,5},{507,18,4},{1028,18,4},{786,18,4},{777,18,4},{768,18,4},{202,18,4},{151,18,4},{113989,18,4},{865394,25,4},{1194,18,4},{351,25,4},{1077,25,40},{1643,25,8},{67173,25,8},{3956,25,40},{2152,25,8},{75281,25,8},{1416,25,40},{1523,25,8},{65095,25,8},{1431,25,48},{2895,25,8},{66172,25,8}};
	// end_addr
	if ((start_addr + g_plr_state.test_sector_length) <= get_dd_total_sector_count()){
		end_addr = start_addr + g_plr_state.test_sector_length;
	}else{
		end_addr = get_dd_total_sector_count();
	}

    plr_info("test_length %d\n", g_plr_state.test_sector_length);

	if (repeat >= 1){

		// Do workload //
		while(repeat--) {
			plr_info (" [Cycles : %u / %u]\n", (450*g_plr_state.finish.value - repeat), (450*g_plr_state.finish.value));

	//	start_time = get_rtc_time();
		for(i=0 ; i<400 ; i++){

			// wait for delay time
			delay = (u32)workload_cmd_list[i].delay;
			if(delay > 1000000) delay = 1000000;	// maximum delay time is 1 sec.
			
//				printf("cmd_line[%d] - delay:%u, cmd:%d, sector_cnd:%d\n", i, delay, workload_cmd_list[i].cmd, workload_cmd_list[i].sector_cnt);
			
				udelay(delay);
			
				// sleep cmd
				if(workload_cmd_list[i].cmd == 5){
                    workload_suspend();
//					workload_sleep();
					now_sleep = 1;
				}else{
					// If AP is sleeping now, resume AP first
					if(now_sleep==1){
//						workload_resume();
					}
					now_sleep = 0;
				
					// read/write random address
					well512_write_seed(get_cpu_timer(0));
					random_offset = well512_write_rand() % (g_plr_state.test_sector_length - workload_cmd_list[i].sector_cnt);
								
					// write cmd
					if(workload_cmd_list[i].cmd == 25){
						// fill buffer with random data
						wbuf = prepare_random_ref_buffer();
						// write
						workload_write(workload_cmd_list[i].sector_cnt, random_offset, wbuf);
					}// read cmd
					else if(workload_cmd_list[i].cmd == 18){
						//read
						workload_read(workload_cmd_list[i].sector_cnt, random_offset, rbuf);
					}
				}
			}
		}	
	
		printf("workload finished\n");
	}

	// write known pattern for read disturb test
	wbuf = prepare_ref_buffer();
	plr_info ("[Last Cycles for read disturb test]\n");
	// Erase all
	//dd_erase_for_poff(0, end_addr, TYPE_SANITIZE);
    end_addr = get_dd_total_sector_count();
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
	printf("\n");
	
	return ret;
}

//
// FUNCTIONS for test case
//

static int pattern1_interleaving(uchar *buf, uint start_lsn, uint request_sectors, op_pattern operate)
{
	int ret = 0;

	/* Nothing to do */

	return ret;
}

static int pattern2_sequential(uchar *buf, uint start_lsn, uint request_sectors, op_pattern operate)
{
	int ret = 0;

	/* Nothing to do */

	return ret;
}


static void init_pattern_1_2(struct plr_write_info* t_info)
{
	t_info->lsn			 	= get_first_sector_in_test_area();
	t_info->request_sectors	= CHUNK_SIZE;
}

static void print_rd_crash_info(uchar * buf, struct plr_state * state, struct Unconfirmed_Info * info)
{
	plr_info("Read disturbance crash info\n");
	plr_info("1.    boot cnt: %8d\n", _rd_crash_info->boot_cnt);
	plr_info("2.      repeat: %8d\n", _rd_crash_info->repeat_no);
	plr_info("3. Crashed lsn: %8d (0x%x)\n", _rd_crash_info->sec_no_in_zone, _rd_crash_info->sec_no_in_zone);
}

static int extra_verification(uchar *buf, struct plr_state *state)	// 8 MB read buffer
{
	int 	i = 0;
	int 	ret	= 0;
	int 	b_verify = 0;
	int 	crash_sec_no_in_zone = 0;
	u32* 	ref_buf = NULL;

	ref_buf = prepare_ref_buffer();

	plr_info("\n Loop count: %d\n", ++rd_loop_count);

	plr_info("\n Read disturb verify...%d times\n", RD_REPEAT_COUNT);

	for (i = 0; i < RD_REPEAT_COUNT; i++) {

		plr_info(" [Loop count : %d, Times : #%d / #%d]\n", rd_loop_count, i + 1, RD_REPEAT_COUNT);
#if 0
		// Read disturb verify first and last RD_REPEAT_COUNT or
		// more than read cycle 9K
		if (0 == i || (RD_REPEAT_COUNT - 1) == i ||
			rd_loop_count >= 9) {
			b_verify = TRUE;
		}
#else
		// Read disturb verify when i%100==0
		if (i%100==0) {
			b_verify = TRUE;
        }
#endif
        plr_info("boot_cnt%d\n", g_plr_state.boot_cnt );
        plr_info("loop_cnt%d\n", g_plr_state.loop_cnt );

		ret = read_disturb_verify(ref_buf, &crash_sec_no_in_zone, b_verify);

		if (ret) {

			_rd_crash_info->sec_no_in_zone 	= crash_sec_no_in_zone;
			_rd_crash_info->repeat_no  		= i + 1;
			_rd_crash_info->boot_cnt		= g_plr_state.boot_cnt;

			if (ret == RD_ERR_READ_VERIFY_FAILED)
				verify_set_result(VERI_EREQUEST, crash_sec_no_in_zone);
			else
				verify_set_result(VERI_EIO, crash_sec_no_in_zone);

			return VERI_EREQUEST;
		}
		b_verify = FALSE;
	}

	return VERI_NOERR;
}

/* ---------------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------------------
 */

static void tune_test_area(uint* test_area_start, uint* test_area_length)
{
	*test_area_start 	+= SECTORS_PER_ZONE;	// shift the start area by +1 zone
	*test_area_length 	-= SECTORS_PER_ZONE;	// but end is not changed
}

int initialize_daxx_0012( uchar * buf, uint test_area_start, uint test_area_length )
{
	int ret = 0;

	struct Pattern_Function pattern_linker;
	struct Print_Crash_Func print_crash_info;


	//
	// Change test area for Read disturbance
	//

	tune_test_area(&test_area_start, &test_area_length);


	init_case_info(test_area_start, test_area_length);
	set_test_sectors_in_zone(get_sectors_in_zone());

	/*
	* NOTE *************************************************************************************
	* 포팅을 위한 임시 코드.
	* ******************************************************************************************
	*/
	init_pattern(&g_plr_state, FALSE);

	print_crash_info.default_step 			= NULL;
	print_crash_info.checking_lsn_crc_step 	= NULL;
	print_crash_info.extra_step 			= print_rd_crash_info;
	verify_init_print_func(print_crash_info);


	/*
	* NOTE *************************************************************************************
	* 사용자는 initialize level 에서 register pattern을 호출해야 한다.
	* ******************************************************************************************
	*/
	memset(&pattern_linker, 0, sizeof(struct Pattern_Function));
	pattern_linker.do_pattern_1 = pattern1_interleaving;
	pattern_linker.do_pattern_2 = pattern2_sequential;
	pattern_linker.init_pattern_1 = init_pattern_1_2;
	pattern_linker.init_pattern_2 = init_pattern_1_2;
	pattern_linker.do_extra_verification = extra_verification;

	regist_pattern(pattern_linker);

	/*
	* NOTE *************************************************************************************
	* 포팅을 위한 임시 코드.
	* ******************************************************************************************
	*/
	ret = check_pattern_func();
	if(ret)
		return ret;

	//
	// Fill read disturbance area (zone 0) with RD_MAGIC_NUMBER
	//
	if(g_plr_state.boot_cnt == 1 || g_plr_state.boot_cnt == 0) //ieroa, 20150903 : initialization first once
	{
		if((ret = do_workload())) {
			plr_err("Do workload failed !!!");
			return ret;
		}
	}
	return ret;
}

int read_daxx_0012(uchar * buf, uint test_area_start, uint test_area_length)
{
	verify_set_current_step(VERI_EXTRA_STEP);
	return extra_verification(buf, NULL);	// read verify func
    //return 0;   // we don't need read verify after workload
}

int write_daxx_0012(uchar * buf, uint test_area_start, uint test_area_length)
{
	set_loop_count(++g_plr_state.loop_cnt);
	return 0;
}
