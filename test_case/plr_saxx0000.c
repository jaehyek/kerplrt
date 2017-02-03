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
#include "test_case/plr_case.h"

static unsigned long long _total_writing_page = 0;
static unsigned long long _total_writing_time = 0;
static uint _start_time = 0;
static uint _end_time = 0;
static bool _writing_sequence = TRUE;

static uint get_request_length(void)
{
	if (g_plr_state.chunk_size.type) {
		uint rand = well512_write_rand() % 100;

		if (rand < g_plr_state.chunk_size.ratio_of_4kb) {
			return 8;
		}
		else if (rand < g_plr_state.chunk_size.ratio_of_4kb + g_plr_state.chunk_size.ratio_of_8kb_64kb) {
			rand = well512_write_rand();
			return ((rand % 15) << 3) + 16;
		}
		else {
			rand = well512_write_rand();
			return ((rand % 112) << 3) + 136;
		}
	}
	else {
		return (g_plr_state.chunk_size.req_len >> 2) << 3;
	}
}

static uint get_skip_length(void) 
{
	if (g_plr_state.chunk_size.type) {
		uint rand = well512_write_rand() % 100;
		if (rand < g_plr_state.chunk_size.ratio_of_4kb) {
			return 8;
		}
		else if (rand < g_plr_state.chunk_size.ratio_of_4kb + g_plr_state.chunk_size.ratio_of_8kb_64kb) {
			rand = well512_write_rand();
			return ((rand % 15) << 3) + 16;
		}
		else {
			rand = well512_write_rand();
			
			return ((rand % 112) << 3) + 136;
		}
	}
	else {
		return (g_plr_state.chunk_size.skip_size>> 2) << 3;
	}
}

static void print_daxx_0000(void)
{
	plr_info_highlight("\n [ Sequential Write Test ]\n");
	if (g_plr_state.chunk_size.type) {
		plr_info(" Chunk Size    : 4KB [%d%%] 8KB ~ 64KB [%d%%] 68KB ~ 512KB [%d%%]\n",
			g_plr_state.chunk_size.ratio_of_4kb, g_plr_state.chunk_size.ratio_of_8kb_64kb, g_plr_state.chunk_size.ratio_of_64kb_over);
		plr_info(" Skip          : 4KB [%d%%] 8KB ~ 64KB [%d%%] 68KB ~ 512KB [%d%%]\n",
			g_plr_state.chunk_size.ratio_of_4kb, g_plr_state.chunk_size.ratio_of_8kb_64kb, g_plr_state.chunk_size.ratio_of_64kb_over);
	}
	else {
		plr_info(" Chunk Size    : %dKB\n", g_plr_state.chunk_size.req_len);
		plr_info(" Skip          : %dKB\n", g_plr_state.chunk_size.skip_size);
	}
}

static void print_statistics(void)
{
	char send_buf[128];

	plr_info(" Loop Count    : %u\n", g_plr_state.loop_cnt - 1);
	plr_info(" Total Data    : %llu GB %llu MB \n",
							_total_writing_page/PAGE_PER_GB, (_total_writing_page%PAGE_PER_GB)/PAGE_PER_MB);
	plr_info(" Total Time    : %llu days %llu hours %llu minutes %llu seconds \n",
							_total_writing_time / SECONDS_PER_DAY, ( _total_writing_time % SECONDS_PER_DAY ) / SECONDS_PER_HOUR,
							(_total_writing_time % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE, _total_writing_time % SECONDS_PER_MINUTE); 
	plr_info(" Writing Speed : %llu MB %llu KB / second \n\n", _total_writing_page / _total_writing_time / 256, ((_total_writing_page / _total_writing_time) % 256) * 4);	

	sprintf(send_buf, "%llu/%llu/%llu/", _total_writing_page, _total_writing_time, _total_writing_page * 4 / _total_writing_time); 
	send_token(PLRTOKEN_SET_STATISTICS, send_buf);		
}
	 
static int pattern(uchar *buf, uint start_lsn, uint request_sectors, op_pattern operate)
{
	int ret = 0;
	uint request_count 		= 0;
	uint lsn 				= start_lsn;
	uint request_length		= 0;
	uint skip_sectors 		= 0;
	uint next_request_lsn	= 0;
	int remain_sectors 		= get_last_sector_in_test_area() - get_first_sector_in_test_area();	
	int total_sectors 		= remain_sectors;

	if(start_lsn == get_first_sector_in_test_area())
		request_length = get_request_length();	

	while (remain_sectors > NUM_OF_SECTORS_PER_PAGE) 
	{			
		request_length	= get_request_length();			
		skip_sectors 	= get_skip_length();
		
		next_request_lsn = lsn + request_length + skip_sectors;
		if (request_count % 99 == 0) 
			plr_info("\r %u / %u", total_sectors - remain_sectors, total_sectors);
					
		ret = operate(buf, lsn, request_length, 0, next_request_lsn);
		if (ret != 0)
			return ret;

		if(_writing_sequence == TRUE)
		{
			_total_writing_page += (unsigned long long)(request_length / NUM_OF_SECTORS_PER_PAGE);			

			if (_total_writing_page / PAGE_PER_GB >= g_plr_state.finish.value) 
			{
				g_plr_state.b_finish_condition = TRUE;	
				break;
			}
		}
		
		request_count++;
		lsn = next_request_lsn;
		remain_sectors -= (request_length + skip_sectors);	
	}

	return ret;
}

static void init_pattern_1_2(struct plr_write_info* t_info)
{
	t_info->lsn			 	= get_first_sector_in_test_area();
	t_info->request_sectors	= 0;
}


/* ---------------------------------------------------------------------------------------
 * Test Main Function
 *----------------------------------------------------------------------------------------
 */

int initialize_saxx_0000( uchar * buf, uint test_start_sector, uint test_sector_length )
{
	int ret = 0;	
	struct Pattern_Function pattern_linker;

	init_case_info(test_start_sector, test_sector_length);
	set_test_sectors_in_zone(get_sectors_in_zone());	

	g_plr_state.boot_cnt = 1;

	/*
	* NOTE *************************************************************************************
	* ������ ���� �ӽ� �ڵ�. 
	* ******************************************************************************************
	*/	
	init_pattern(&g_plr_state, FALSE);	

	/*
	* NOTE *************************************************************************************
	* ����ڴ� initialize level ���� register pattern�� ȣ���ؾ� �Ѵ�.
	* ******************************************************************************************
	*/	
	memset(&pattern_linker, 0, sizeof(struct Pattern_Function));
	pattern_linker.do_pattern_1 = pattern;
	pattern_linker.do_pattern_2 = pattern;
	pattern_linker.init_pattern_1 = init_pattern_1_2;
	pattern_linker.init_pattern_2 = init_pattern_1_2;
	pattern_linker.do_extra_verification = NULL;
	
	regist_pattern(pattern_linker);

	/*
	* NOTE *************************************************************************************
	* ������ ���� �ӽ� �ڵ�. 
	* ******************************************************************************************
	*/	
	ret = check_pattern_func();
	if(ret)
		return ret;
		
	return ret;
}

int read_saxx_0000( uchar * buf, uint test_start_sector, uint test_sector_length)
{
	return verify_pattern(buf);
}

int write_saxx_0000( uchar * buf, uint test_start_sector, uint test_sector_length )
{
	int ret = 0;
			
	if(_total_writing_page / PAGE_PER_GB >= g_plr_state.finish.value)
	{
		plr_info_highlight("\n\n========== Finish ===========\n");
		print_statistics();
		g_plr_state.b_finish_condition = TRUE;
		plr_info_highlight("=============================\n");
		return 0;
	}

	_start_time = get_rtc_time();
	_writing_sequence = TRUE;
	
	ret = write_pattern(buf);
	if(ret != 0)
		return ret;
	
	_writing_sequence = FALSE;
	_end_time = get_rtc_time();
	_total_writing_time += (_end_time - _start_time);

	print_daxx_0000();
	print_statistics();	

	return ret;
}

