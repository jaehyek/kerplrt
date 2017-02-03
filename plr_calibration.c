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
#include "plr_deviceio.h"
#include "plr_err_handling.h"
#include "plr_calibration.h"
#include "plr_case_info.h"

#include <string.h>

#define PRIVATE static	// data and function used internally in this module
#define PUBLIC			// data and function can be accessed or called by other module externally, whitch should be declared in the header file.


#define SJD_NUM_OF_CNK_SIZE_TYPE 				9 		// originally 9		
#define SJD_CALIBRATION_REQUSET_COUNT 			1024	// originally 512 * 2


static uint _calib_request_count = SJD_CALIBRATION_REQUSET_COUNT;

//
// Macros for Code control
//
//#define SJD_GET_DISTRIBUTION_DATA
//#define SJD_RECORD_TICKS_RAW_DATA					
//#define SJD_INTEREST_ON_SOB


#ifdef SJD_GET_DISTRIBUTION_DATA
PRIVATE void _record_distribution_data(s32 cnk_siz, tick_s* ticks );
#endif


extern int write_request_normal(uchar * buf, 
				uint req_start, 
				uint req_len, 
				uint req_seq_num, 
				uint next_start_sector, 
				bool is_commit);

//
// @SJD 150629
// Create this function for Cached Internal Power Off. The name influenced from write_request_normal() 
// that non cache flush version write function.

extern int write_flush_request_for_calib(uchar * buf, 
				uint req_start, 
				uint req_len, 
				uint req_seq_num, 
				uint next_start_sector);

typedef struct  {
	s32		rank;				// 0, 1, 2, ...
	s32		cnk_siz;			// 8 * 2^rank = 8, 16, 32, ...
	s32		num_try;			// # of sampling
	tick_s	min_eod;			// minimum of End of DMA
	tick_s	max_eod;			// maximum of End of DMA
	tick_s	min_eob;			// ... BUSY
	tick_s	max_eob;			// ... BUSY	
	tick_s	min_eof;			// ... CACHE FLUSH
	tick_s	max_eof;			// ... CACHE FLUSH
	tick_s	average;
	tick_s	total;
} calib_record_s;

typedef struct {
	s32					curr_rank;
	calib_record_s		*record;
}calib_s;


//
// 0. COMMON DATA REGION
//////////////////////////////////////////////////////////////////////////

PRIVATE calib_record_s calib_table[SJD_NUM_OF_CNK_SIZE_TYPE];

static processing_phase_e	_last_calib_phase =  -1;


#ifdef SJD_RECORD_TICKS_RAW_DATA
PRIVATE tick_s** ticks_raw_data;
#endif

// for log scale histogram data about less than 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192
#ifdef SJD_GET_DISTRIBUTION_DATA
#define MAX_TICK_RANK	14 //
static u32 dist_record[SJD_NUM_OF_CNK_SIZE_TYPE][3][MAX_TICK_RANK]; // 3 counts eod, eob and eof
#endif

//
// 1. CALIBRATION DATA REGION
//////////////////////////////////////////////////////////////////////////

PUBLIC void calib_init(void)
{
	int rank;
	for(rank = 0; rank < SJD_NUM_OF_CNK_SIZE_TYPE; rank++)
	{
		if(IS_CACHEON_TEST){	
			calib_record_s r = 
				{	rank, 														//rank
					8<<rank,													//chunk_size
					0, 															//num_try
					{PLR_LONG_MAX, PLR_LONG_MAX, PLR_LONG_MAX},					//EoD min
					{0, 0, 0}, 													//EoD max
					{PLR_LONG_MAX, PLR_LONG_MAX, PLR_LONG_MAX},					//EoB min
					{0, 0, 0},													//EoB max
					{PLR_LONG_MAX, PLR_LONG_MAX, PLR_LONG_MAX},					//EoF min
					{0, 0, 0},													//EoF max
					{0, 0, 0}, 													//Aver
					{0, 0, 0}													//total		
				};													
					
			memcpy(&calib_table[rank], &r, sizeof(calib_record_s)); 	

			if(g_plr_state.cache_flush_cycle <= 0)
				_calib_request_count = 8 * 256;
			else 
				_calib_request_count = g_plr_state.cache_flush_cycle * 256;
		}
		else {
			calib_record_s r = 
				{	rank, 														//rank
					8<<rank,													//chunk_size
					0, 															//num_try
					{PLR_LONG_MAX, PLR_LONG_MAX, 0},							//EoD min
					{0, 0, 0}, 												//EoD max
					{PLR_LONG_MAX, PLR_LONG_MAX, 0},							//EoB min
					{0, 0, 0},													//EoB max
					{0, 0, 0},												//EoF min
					{0, 0, 0},												//EoF max
					{0, 0, 0}, 												//Aver
					{0, 0, 0}													//total		
				};	

			memcpy(&calib_table[rank], &r, sizeof(calib_record_s)); 						
			_calib_request_count = SJD_CALIBRATION_REQUSET_COUNT;
		}
	}


#ifdef SJD_RECORD_TICKS_RAW_DATA	
	ticks_raw_data = (tick_s**) malloc(sizeof(tick_s*)*SJD_NUM_OF_CNK_SIZE_TYPE);

	for(rank = 0; rank < SJD_NUM_OF_CNK_SIZE_TYPE; rank++)
	{
		ticks_raw_data[rank] = (tick_s*)malloc(sizeof(tick_s)*_calib_request_count);
	}
#endif
// joys,2015.08.05 Do not use
//	rtc64_reset_tick_count();	// Actually starts RTC
}


PRIVATE int _get_rank(u32 cnk_siz)
{
	s32 rank, ret = SJD_NUM_OF_CNK_SIZE_TYPE -  1;
	

	for (rank = SJD_NUM_OF_CNK_SIZE_TYPE - 1; rank >= 0; rank--)
	{
		if (calib_table[rank].cnk_siz <= cnk_siz) 
		{
			ret = rank;
			break;
		}
	}
	return ret;		
}

//
// todo : calc for size / 4k = not power of 2 but integer.
//

PRIVATE calib_record_s* _get_record(u32 cnk_siz)
{
	if (0 == cnk_siz) 
	{
		return &calib_table[0];
	}
	
 	return &calib_table[_get_rank(cnk_siz)];
}

//
// calculates min and max value of ticks
//

PUBLIC void calib_calc_ticks_min_max(s32 cnk_siz, tick_s* ticks)
{
	calib_record_s * r = _get_record(cnk_siz);

	r->num_try++;
	
	r->total.eod += ticks->eod;
	r->total.eob += ticks->eob;	
	r->total.eof += ticks->eof;		


	if (r->min_eod.eod > ticks->eod)
	{
		if(ticks->eod == 0)
			r->min_eod.eod = r->min_eod.eob = r->min_eod.eof = 0;
		else
		{	
			r->min_eod.eod = ticks->eod;
			r->min_eod.eob = ticks->eob;				
			r->min_eod.eof = ticks->eof;					
		}
	}	
		
	if (r->max_eod.eod < ticks->eod) 
	{
		if(ticks->eod == 0)
			r->max_eod.eod = r->max_eod.eob = r->max_eod.eof = 0;
		else
		{	
			r->max_eod.eod = ticks->eod;		
			r->max_eod.eob = ticks->eob;				
			r->max_eod.eof = ticks->eof;
		}
	}

	if (r->min_eob.eob > ticks->eob)
	{ 
		if(ticks->eob == 0)
			r->min_eob.eod = r->min_eob.eob = r->min_eob.eof = 0;
		else
		{	
			r->min_eob.eod = ticks->eod;
			r->min_eob.eob = ticks->eob;
			r->min_eob.eof = ticks->eof;		
		}
	}	

	if (r->max_eob.eob < ticks->eob)
	{
		if(ticks->eob == 0)
			r->max_eob.eod = r->max_eob.eob = r->max_eob.eof = 0;
		else
		{
			r->max_eob.eod = ticks->eod;
			r->max_eob.eob = ticks->eob;
			r->max_eob.eof = ticks->eof;		
		}
	}

	if (IS_CACHEON_TEST)
	{
		if (r->min_eof.eof > ticks->eof)
		{ 
			if(ticks->eof == 0)
				r->min_eof.eod = r->min_eof.eob = r->min_eof.eof = 0;
			else
			{
				r->min_eof.eod = ticks->eod;
				r->min_eof.eob = ticks->eob;
				r->min_eof.eof = ticks->eof;		
			}
		}	

		if (r->max_eof.eof < ticks->eof)
		{
			if(ticks->eof == 0)
				r->max_eof.eod = r->max_eof.eob = r->max_eof.eof = 0;
			else
			{
				r->max_eof.eod = ticks->eod;
				r->max_eof.eob = ticks->eob;
				r->max_eof.eof = ticks->eof;		
			}
		}
	}

#ifdef SJD_GET_DISTRIBUTION_DATA
	_record_distribution_data(cnk_siz, ticks);
#endif

#ifdef SJD_RECORD_TICKS_RAW_DATA
	_record_ticks_raw_data(cnk_siz, ticks);
#endif
}


//#define SJ_TICK_2_USEC(tick) ((tick) * (TICCNT_RESOLUTION))
#define SJ_KB_2_SECTOR(kB)   ((kB) / 2)

//
// Prints calibration tabe
//

PUBLIC void calib_print_table(void)
{
	int i;
	unsigned long long tatal_written_kb = 0, total_time_tick = 0;
	
//	display_date();	// joys,2015.08.05 Do not use
	printf("\n\n");
	plr_info_highlight (" 1. The statistics about calibration test with units of kB and tick. Note: EoD, EoB and EoF is the abbreviations of the End of DMA, BUSY and Cache Flush respectively.\n"); 
	plr_info_highlight ("  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- \n");
	plr_info_highlight (" |         |          EoD_min            |          EoB_min            |          EoF_min            |          Average            |          EoD_max            |          EoB_max            |          EoF_max            |      |\n");
	plr_info_highlight (" | CNK SIZ |-----------------------------|-----------------------------|-----------------------------|-----------------------------|-----------------------------|-----------------------------|-----------------------------|  N   |\n");
	plr_info_highlight (" |         |   EoD   |   EoB   |   EoF   |   EoD   |   EoB   |   EoF   |   EoD   |   EoB   |   EoF   |   EoD   |   EoB   |   EoF   |   EoD   |   EoB   |   EoF   |   EoD   |   EoB   |   EoF   |   EoD   |   EoB   |   EoF   |      |\n");
	plr_info_highlight (" |---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|------|\n");

	for (i = 0; i < SJD_NUM_OF_CNK_SIZE_TYPE; i++) {

		calib_record_s* p = &calib_table[i];

		//p->num_try = SJD_CALIBRATION_REQUSET_COUNT;			

		if (p->cnk_siz != 8<<i) 
		{
			p->num_try--;
		}		

		p->average.eod = (s32)(double)p->total.eod / (double)p->num_try;
		p->average.eob = (s32)(double)p->total.eob / (double)p->num_try;		

		if(IS_CACHEON_TEST)
			p->average.eof = (s32)(double)p->total.eof / (double)p->num_try;		
		else
		{
			//ieroa, 2015.10.19
			p->average.eof = 0;
			//p->average.eof = -1;
		}

		plr_info_highlight (" | %5u   | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %7d | %4u |\n", 

		 	SJ_KB_2_SECTOR(p->cnk_siz),

			(p->min_eod.eod),	(p->min_eod.eob),	(p->min_eod.eof),
			(p->min_eob.eod),	(p->min_eob.eob),	(p->min_eob.eof),
			(p->min_eof.eod),	(p->min_eof.eob),	(p->min_eof.eof),
			(p->average.eod),	(p->average.eob),	(p->average.eof),
			(p->max_eod.eod),	(p->max_eod.eob),	(p->max_eod.eof),
			(p->max_eob.eod),	(p->max_eob.eob),	(p->max_eob.eof),
			(p->max_eof.eod),	(p->max_eof.eob),	(p->max_eof.eof),

			p->num_try
		);


		tatal_written_kb  += p->num_try * SJ_KB_2_SECTOR(p->cnk_siz);
		total_time_tick   += p->total.eob;
	}
	plr_info_highlight ("  --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- \n");		

	if(IS_CACHEON_TEST) {
		plr_info_highlight ("  Write count per size: %d, Cache: %s,  Cache flush period: %d\n", _calib_request_count,  IS_CACHEON_TEST ? "On": "Off", g_plr_state.cache_flush_cycle);			
	}else {
		plr_info_highlight ("  Write count per size: %d, Cache: %s\n", _calib_request_count,  IS_CACHEON_TEST ? "On": "Off");				
	}
}


//
// Decide internal power off time
//


 /*
 *
 * Algorithm
 *
 * 1. Device�� ���� �Ǹ�, �ش� device�� page size ���� �۰ų� ����	data chunk �� write �� ��� DMA ������ BUSY ������ ���� ��Ȯ�ϰ� ������ �� ���̴�.
 * 2	����, Data chunk �� page size ���� ũ�ų� ���� ���� page size �� �ش��ϴ� DMA ������ ������ ���� BUSY�� ���� �� ���̶�� ������ ������� ���� �� �ִ�.
 * 3. �̷� �����Ͽ� page size ���� ���� data chunk�� write �� ����	DMA �� ������ BUSY �� ���� �������� Power Off �� �������� ���, 
 *    page size ���� ū data chunk��	write �� ���� page size write �� ������ DMA ������ ������ BUSY�� ���� �������� Power Off �� �������� ������ �� �ִ�.
 * 4. ���⼭ Power Off�� Upper Bound �� ���迡 ���� ������ ���� ���� �� �� �ִ�.
 *
 */



#define SJD_PAGE_SIZE_RANK	2														// Assume device PAGE size is around 16 kB		

PUBLIC int calib_generate_internal_po_time(u32 size)
{
	calib_record_s *cr = _get_record(size);
	s32 dn, up;
	processing_phase_e lp = calib_get_last_calib_phase();
	int r2 = well512_rand() % 100;													// Random number for area selecion
	
	if (PROCESS_PHA_CACHE_FLUSH != lp ) {											// CACHE FLUSH case
		if (cr->rank <= SJD_PAGE_SIZE_RANK) {										// Chunk size <= PAGE size

			if (r2 < 90) {	// 90%
				dn = cr->average.eod;			
				up = cr->average.eob; 				
			}
			else {			// 10%
				dn = cr->average.eob;
				up = cr->max_eob.eob; 		
			}			

		}	
		else{

			calib_record_s *crz = _get_record(8 << SJD_PAGE_SIZE_RANK); 			// Get the record for PAGE size chunk		

			if (r2 < 90) {	// 90%
				dn = crz->average.eod;												// End of DMA time informed by 16 kB block write			    	  			
				up = cr->average.eob;				
			}
			else {			// 10%
				dn = cr->average.eob;
				up = cr->max_eob.eob;		
			}
		}		
	}else {
		if (cr->rank <= SJD_PAGE_SIZE_RANK) {										// Chunk size <= PAGE size

			if (r2 < 90) {	// 90%
				dn = cr->min_eof.eof;												// min.eob <---> avg.eof
				up = cr->average.eof; 				
			}
			else {			// 10%
				dn = cr->average.eof;												// 
				up = cr->max_eof.eof; 		
			}
		}			
		else if(cr->rank < SJD_NUM_OF_CNK_SIZE_TYPE - 2 - 1) { 						// Chunk size > PAGE size		
			calib_record_s *crz = _get_record(8 << SJD_PAGE_SIZE_RANK); 			// Get the record for PAGE size chunk				

			if (r2 < 90) {	// 90%
				dn = crz->min_eof.eof;												// End of DMA time informed by 16 kB block write			    	  			
				up = cr->average.eof;				
			}
			else {			// 10%
				dn = cr->average.eof;
				up = cr->max_eof.eof;		
			}	
		}		
		else {
			calib_record_s *crz = _get_record(8 << SJD_PAGE_SIZE_RANK); 			// Get the record for PAGE size chunk		
			calib_record_s *crx = _get_record(8 << (SJD_NUM_OF_CNK_SIZE_TYPE-1)); 	// Get the record of MAX size chunk, use the data from max chunk size (why? requirement of boss!)					

			if (r2 < 90) {	// 90%
				dn = crz->min_eof.eof;												// End of DMA time informed by 16 kB block write			    	  			
				up = crx->average.eof;				
			}
			else {			// 10%
				dn = cr->average.eof;
				up = crx->max_eof.eof;		
			}				
		}
	}

	if (up == dn)
		return 10;
		
	return (int)dn + well512_rand() % (up - dn);									//
}

//
// Helper functions for cache Flush
//

#ifndef  PLR_CALIBRATION_TEST_IEROA
PRIVATE	int _cached_write_count;

PRIVATE void _inc_cached_write_count(void)
{
	_cached_write_count++;	
}

PRIVATE int _get_cached_write_count(void)
{
	return _cached_write_count;
}

PRIVATE void _reset_cached_write_count(void)
{
	_cached_write_count = 0;	
}
#endif


PUBLIC processing_phase_e calib_get_last_calib_phase(void)
{
	return _last_calib_phase;
}

PUBLIC void calib_set_last_calib_phase(processing_phase_e phase)
{
	_last_calib_phase = phase;
}


//
// calib write request
//
PUBLIC int calib_write_request(uchar * buf, u32 req_start, u32 req_len, u32 req_seq_num, u32 next_start_sector)
{
	u32 dirty_start_sector, dirty_area_length;

	// joys,2015.06.29
	//get_dd_dirty_part_info(&dirty_start_sector, &dirty_area_length);

	if (get_dd_dirty_part_info(&dirty_start_sector, &dirty_area_length))
	{
		return -PLR_EINVAL;
	}

	if ( req_len > dirty_start_sector + dirty_area_length - req_start ) 
	{
		req_len = dirty_area_length + dirty_start_sector - req_start;
	}
	else if ( req_start > dirty_area_length + dirty_start_sector ) 
	{
		plr_err("start sector overflow!!\n");
		return -PLR_EINVAL;
	}
	else if (req_len == 0) 
	{
		plr_err("length is zero!!\n");
		return -PLR_EINVAL;
	}

	if (IS_CACHEON_TEST) // @sj 150706 check
	{
		int ret;


#ifdef  PLR_CALIBRATION_TEST_IEROA
		ret = write_flush_request_for_calib(buf, req_start, req_len, req_seq_num, next_start_sector);
		if(ret)
			return ret;		
#else
		//ieroa. 2015.11.10
		if(DO_CACHE_FLUSH(_get_cached_write_count()+1, g_plr_state.cache_flush_cycle))
		{		
			calib_set_last_calib_phase(PROCESS_PHA_CACHE_FLUSH);
			ret = write_flush_request_for_calib(buf, req_start, req_len, req_seq_num, next_start_sector);
			if(ret)	
				return ret;		

			_reset_cached_write_count();
		}
		else {			
			calib_set_last_calib_phase(PROCESS_PHA_WRITE_BUSY);		
			ret = write_request_normal(buf, req_start, req_len, req_seq_num, next_start_sector, FALSE);
			if(ret)	
				return ret;
			
			_inc_cached_write_count();
		}
#endif
	}
	else 
	{
		calib_set_last_calib_phase(PROCESS_PHA_WRITE_BUSY);
		return  write_request_normal(buf, req_start, req_len, req_seq_num, next_start_sector, FALSE);
	}

	return 0;
}



//joys,2015.06.22, 
//@sj 150706, limit function access acope with static keyword

#if 0
PUBLIC int send_internal_calibration_info(int cal_test_enable)
#else
PUBLIC int send_internal_calibration_info(int action, bool cal_test_enable) //@sj 150709, action as a parameter
#endif

{
	internal_info_t int_info = {0};

	if (cal_test_enable) {
		int_info.action = action;
		g_plr_state.b_calibration_test = TRUE;
	}
	else
		g_plr_state.b_calibration_test = FALSE;

	dd_send_internal_info((void*)&int_info);

	return 0;
}

//
// @sj 150702, There is no way of getting data size in cache flush. 
// So, this is a tricky way of getting written data size before flush, needed for getting flush data size in cache flush phase.
//


//
// @sj 150706, _written_size_before_flush: 
//
// this valiable used for accumulate the cached data size till cache flush.
// So, this value should be set to 0 at starting time and just after cache flush.
//

PRIVATE uint _written_size_before_flush;

// add cached write data size before cache flush
PUBLIC void calib_add_cached_write_data_size(int data_size)
{
	_written_size_before_flush += data_size;
}

// get cached write data size before cache flush
PUBLIC uint calib_get_cached_write_size_before_flush(void)
{
	return _written_size_before_flush;
}

// reset cached write data size before cache flush
PUBLIC void calib_reset_cached_write_data_size(void)
{
	_written_size_before_flush = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


typedef struct {
	u32 start; 										// chunk start sector index		
	u32 length;										// chunk length in sectors
} chunk_s;

#define UNIT_SECTOR_SIZE	512
#define ONE_GB ((1<<30) / UNIT_SECTOR_SIZE)

//
// Make calibration table
//

#ifdef SJD_GET_DISTRIBUTION_DATA

//
// 2. DISTRIBUTIN REGION
//////////////////////////////////////////////////////////////////////////



//
// records distribution data
//
#define EoD  0
#define EoB  1
#define EoF  2

PRIVATE void _record_distribution_data(s32 cnk_siz, tick_s* t)
{
	int tick_rank = 0;

	s32 tick_count = 1;

	int rank = _get_rank(cnk_siz);	
	
	for(tick_rank = 0; tick_rank < MAX_TICK_RANK; tick_rank++)	
	{
		if (t->eod <= tick_count) {			  
			dist_record[rank][EoD][tick_rank]++;
			break;			
		}

		tick_count <<= 1;					
	}

	tick_count =  1;
	
	for(tick_rank = 0; tick_rank < MAX_TICK_RANK; tick_rank++)
	{
		if (t->eob <= tick_count) {
			dist_record[rank][EoB][tick_rank]++;
			break;
		}

		tick_count <<= 1;		
	}

	tick_count =  1;
	
	for(tick_rank = 0; tick_rank < MAX_TICK_RANK; tick_rank++)
	{
		if (t->eof <= tick_count) {
			dist_record[rank][EoF][tick_rank]++;
			break;
		}

		tick_count <<= 1;		
	}
}

//
// prints histogram data in log scale
//

PRIVATE void _print_tick_histogram_data(void)
{
	int i;
	int chunk_size_index = 0;
	int tick_rank = 0;


	plr_info_highlight ("\n 2. Time tick distribution of calibration test result, where 2^n <= t < 2^(n + 1) with n = 0, 1, 2, ..., MAX_TICK_RANK - 1");		

	plr_info_highlight ("\n  -----------------");	


	for (i = 0; i < MAX_TICK_RANK; i++){
		plr_info_highlight ("--------");		
	}	

	plr_info_highlight ("\n | CNK SIZ \\  Time | "); 	
	
	for (i = 0; i < MAX_TICK_RANK; i++){
		plr_info_highlight (" %4d | ", 1<<i);		
	}
	
	for (chunk_size_index = 0; chunk_size_index < SJD_NUM_OF_CNK_SIZE_TYPE; chunk_size_index++) { 

		plr_info_highlight ("\n |---------|-------|");		

		for (i = 0; i < MAX_TICK_RANK; i++){
			plr_info_highlight ("-------|");		
		}			

		plr_info_highlight ("\n | %5u   |  EoD  | ", 4 << chunk_size_index);		

		for (tick_rank = 0; tick_rank < MAX_TICK_RANK; tick_rank++){
			plr_info_highlight (" %4d | ", dist_record[chunk_size_index][EoD][tick_rank]);		
		}	
		
		plr_info_highlight ("\n |         |  EoB  | ");			

		for (tick_rank = 0; tick_rank < MAX_TICK_RANK; tick_rank++){
			plr_info_highlight (" %4d | ", dist_record[chunk_size_index][EoB][tick_rank]);		
		}

		plr_info_highlight ("\n |         |  EoF  | ");			

		for (tick_rank = 0; tick_rank < MAX_TICK_RANK; tick_rank++){
			plr_info_highlight (" %4d | ", dist_record[chunk_size_index][EoF][tick_rank]);		
		}
	}

	plr_info_highlight ("\n  -----------------");	

	for (i = 0; i < MAX_TICK_RANK; i++){
		plr_info_highlight ("--------");		
	}	
	plr_info_highlight ("\n");			
	
}

#endif //SJD_GET_DISTRIBUTION_DATA


PUBLIC int calib_make_table(void)
{
	u32 seq_num = 0;	
	
	u32 upper_bound = get_dd_total_sector_count(); 	// upper bound
	
	u32 test_start  =  upper_bound - ONE_GB;		// test around end of device
	u32 test_end	=  upper_bound;	
#if 0
	u8  * buf = plr_get_write_buffer();
#endif
	int i, j;

//	plr_info_highlight("\n << Calibration >> \n");

	send_internal_calibration_info(INTERNAL_ACTION_TIMING_CALIBRATION, TRUE);				// @sj 150709

	calib_init();									// initialize calibration table			
	
	SJD_CALIBRATION_MODE(TRUE);

	{
		SJD_DEBUG_PRINTF("(upper_bound, maximum_pages) = (%d, %d)\n", upper_bound, get_pages_in_zone() * get_total_zone());
	}
	

	for (j = 0; j < SJD_NUM_OF_CNK_SIZE_TYPE; j++) 
	{
		chunk_s chunk;
		chunk.start  = test_start;
		chunk.length = calib_table[j].cnk_siz;		

//		plr_info("\n[ %d ] Write %d kB chunk \n", j, calib_table[j].cnk_siz / 2);
		
		for (i = 0; i < _calib_request_count; i++) 
		{
			u32 skip_length = random_sector_count(6);
			u32 next = chunk.start + chunk.length + skip_length;
			
#if 0 //kkr calib skip
			if(calib_write_request(buf, chunk.start, chunk.length, seq_num, next))
			{
				return PLR_EINVAL;
			}
#endif
/*
			if (seq_num % 100 == 0) {
				printf("\r (start, length, skip) = (0x%x, %u, %u)", chunk.start, chunk.length, skip_length);
			}			
*/
			chunk.start = next;
			
			if (chunk.start + chunk.length > test_end) 
			{
				chunk.start = test_start;
			}			
			
			seq_num++;
		}
	}

	send_internal_calibration_info(INTERNAL_ACTION_TIMING_CALIBRATION, FALSE);			// joys,2015.06.22

//	calib_print_table();
#ifdef SJD_GET_DISTRIBUTION_DATA
//	_print_tick_histogram_data(); 				// @sj 150622
#endif	

#ifdef SJD_RECORD_TICKS_RAW_DATA
//	_print_tick_raw_data(); 			 			// @sj 150624
#endif	
/*
	for (j = 0; j < SJD_NUM_OF_CNK_SIZE_TYPE; j++) {

		printf("\n(%d)\n", j);		
		
		for(i = 0; i < 1024; i++)
		{			
			printf(" %4d, ", calib_generate_internal_po_time( 8 << j ));
		}
	}
	
	printf("\n");	
*/
	SJD_CALIBRATION_MODE(FALSE);
	return 0;
}



#ifdef SJD_RECORD_TICKS_RAW_DATA

//
// 2. RAW DATA REGION
//////////////////////////////////////////////////////////////////////////

//
// calculate distribution
//

PRIVATE s32 dist(s32 x, s32 m)
{
	return x*x - m*m;
}

//
// prints ticks raw data
//

PRIVATE void _print_tick_raw_data(void)
{
	int i, j;

	for (j = 0; j < SJD_NUM_OF_CNK_SIZE_TYPE; j++) {

		plr_info_highlight (">>> %d kB Chunk (j, dma, wrt) : ", calib_table[j].cnk_siz);

		for(i = 0; i < _calib_request_count; i++) 
		{
				tick_s* p = &ticks_raw_data[j][i];			
				plr_info_highlight ("%4d, %4d, %4d, %4d; ", i, p->eod, p->eob, p->eof);		
				
				free(p);
		}
		
		plr_info_highlight ("\n");
		
	}
	free(ticks_raw_data);
	
}


//
// Records ticks raw data
//

PRIVATE void _record_ticks_raw_data(s32 cnk_siz, tick_s* t)
{
	static int tr_count = 0;
	calib_record_s *cr = _get_record(cnk_siz);
	tick_s* p = &ticks_raw_data[cr->rank][tr_count++];
	
	p->eod = t->t_eod;
	p->eob = t->t_eob;
	p->eof = t->t_eof;	
}

#endif // SJD_RECORD_TICKS_RAW_DATA

