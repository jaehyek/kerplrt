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
#include "plr_hooking.h"
#include "plr_err_handling.h"
#include "plr_deviceio.h"
#include "plr_protocol.h"
#include "plr_calibration.h"
#include "plr_internal_state.h"

// mmc.h
#define PON_ON			1
#define PON_SHORT		2
#define PON_LONG		3

enum mmc_packed_prepare {
	MMC_PACKED_ADD = 1,
	MMC_PACKED_PASS,
	MMC_PACKED_INVALID, 
	MMC_PACKED_COUNT_FULL,
	MMC_PACKED_BLOCKS_FULL,
	MMC_PACKED_BLOCKS_OVER,
	MMC_PACKED_CHANGE,
};

enum mmc_packed_cmd {
	MMC_PACKED_NONE = 0,
	MMC_PACKED_WR_HDR,
	MMC_PACKED_WRITE,
	MMC_PACKED_READ,
};

static uint g_packed_buf_offset = 0;
static uint g_last_packed_sector = 0;
static erase_type_e g_erase_type = TYPE_ERASE;

extern u32 g_powerloss_time_boundary;
extern bool g_require_powerloss;
int write_request_normal(uchar * buf, uint req_start, uint req_len, uint req_seq_num, uint next_start_sector, bool is_commit);



/* ------------------------------------------------
* Static Function 
* ------------------------------------------------*/

static void make_header(uchar *buf, uint req_seq_num, uint req_start, uint req_len, uint page_index, uint next_start_sector, bool is_commit_pg)
{	
	struct plr_header* p_header = (struct plr_header*)buf;
	if(is_commit_pg)
		p_header->magic_number = MAGIC_NUMBER_FLUSH;
	else
		p_header->magic_number = MAGIC_NUMBER;

	p_header->lsn = req_start + (page_index * NUM_OF_SECTORS_PER_PAGE);
	p_header->boot_cnt = g_plr_state.boot_cnt;
	p_header->loop_cnt = g_plr_state.loop_cnt;
	p_header->req_info.req_seq_num = req_seq_num;
	p_header->req_info.start_sector = req_start;
	p_header->req_info.page_num = req_len / NUM_OF_SECTORS_PER_PAGE;
	p_header->req_info.page_index = page_index;
	p_header->next_start_sector = next_start_sector;
	p_header->reserved1 			= 0;
	p_header->reserved2 			= 0;
	p_header->crc_checksum = crc32(0, (const uchar *)(&(p_header->lsn)), sizeof(struct plr_header) - sizeof(uint)* 4);
}

static void make_header_data(uchar * buf, uint req_start, uint req_len, uint req_seq_num, uint next_start_sector, bool is_commit)
{
	uint index = 0;
	
	for (index = 0; index < req_len / NUM_OF_SECTORS_PER_PAGE; index++)	{
		make_header(buf + (index * PAGE_SIZE), req_seq_num, req_start, req_len, index, next_start_sector, is_commit);
	}
}

static processing_phase_e random_ratio_poweroff_phase(void)
{
	int i = 0;
	int rand = 0;
	int max_ratio = 0;
	int pos_region = 0;

	/* index[PROCESS_PHA_OP_CONDITION,
			PROCESS_PHA_SEND_CMD, 
			PROCESS_PHA_DATA_TRANS, 
			PROCESS_PHA_WRITE_BUSY, 
			PROCESS_PHA_CACHE_FLUSH, 
			PROCESS_PHA_AFTER_WRITE_DONE, 
			PROCESS_PHA_ERASE, 
			PROCESS_PHA_SANITIZE, 
			PROCESS_PHA_SLEEP_AWAKE, 
			PROCESS_PHA_BKOPS,
			PROCESS_PHA_HPI,
			PROCESS_PHA_PON] */
			 
	uint pos_ratio[PROCESS_PHA_MAX] = {0,};

	if (IS_SLEEP_N_AWAKE_TEST) {
		pos_ratio[PROCESS_PHA_SLEEP_AWAKE] = 10UL;
		max_ratio += pos_ratio[PROCESS_PHA_SLEEP_AWAKE];
	}

	if (IS_PON_TEST) {
		pos_ratio[PROCESS_PHA_PON] = 10UL;
		max_ratio += pos_ratio[PROCESS_PHA_PON];
	}

	if (IS_OPCON_TEST) {
		pos_ratio[PROCESS_PHA_OP_CONDITION] = 10UL;
		max_ratio += pos_ratio[PROCESS_PHA_OP_CONDITION];	
	}
	
	if (IS_HPI_TEST && get_dd_bkops_urgent_caused()) {
		pos_ratio[PROCESS_PHA_BKOPS] = 10UL;
		max_ratio += pos_ratio[PROCESS_PHA_BKOPS];
		pos_ratio[PROCESS_PHA_HPI] = 10UL;
		max_ratio += pos_ratio[PROCESS_PHA_HPI];
	}
	else if (IS_BKOPS_TEST && get_dd_bkops_urgent_caused()) {
		pos_ratio[PROCESS_PHA_BKOPS] = 10UL;
		max_ratio += pos_ratio[PROCESS_PHA_BKOPS];
	}
#ifdef PLR_SYSTEM
	if (IS_CACHEON_TEST)
	{
		pos_ratio[PROCESS_PHA_WRITE_BUSY] = 20UL;
		max_ratio += pos_ratio[PROCESS_PHA_WRITE_BUSY];
		pos_ratio[PROCESS_PHA_DATA_TRANS] = 20UL;
		max_ratio += pos_ratio[PROCESS_PHA_DATA_TRANS];
		
		pos_ratio[PROCESS_PHA_CACHE_FLUSH] = 100 - max_ratio;
	}
	else {
		/* KKR 160122 calib를 안하기때문에 임시로 
		 * TRANS와 BUSY를 반반 확률로 설정한다
		 */
//		pos_ratio[PROCESS_PHA_WRITE_BUSY] = 100;
		pos_ratio[PROCESS_PHA_WRITE_BUSY] = (100 - max_ratio)/2;
		pos_ratio[PROCESS_PHA_DATA_TRANS] = (100 - max_ratio)/2;
	}

#else
	if (IS_CACHEON_TEST)
	{
		pos_ratio[PROCESS_PHA_CACHE_FLUSH] = 100 - max_ratio;
	}
	else {
		pos_ratio[PROCESS_PHA_WRITE_BUSY] = 100 - max_ratio;
	}
#endif

	max_ratio = 0;
	
	for (i = 0; i < PROCESS_PHA_MAX; i++) 
	{
		max_ratio += pos_ratio[i]; 
	}

	rand = well512_rand2() % max_ratio;
	
	for (i = 0; i < PROCESS_PHA_MAX; i++)
	{
		pos_region += pos_ratio[i];
		
		if (rand < pos_region)
			break;
	}	
	
	if (i == PROCESS_PHA_MAX)
		i = PROCESS_PHA_WRITE_BUSY;

// debug -------------------------------------------------
//	printf ("===== rand : %u\n", rand);
//	int j = 0;
//	for (j = 0; j < PROCESS_PHA_MAX; j++)
//	{
//		printf ("[%d] ratio : %d\n", j, pos_ratio[j]);
//	}

//	printf ("===== select phase : %u\n", i);
	return i;
}

static processing_phase_e select_poweroff_phase(poweroff_info_t *poff_info)
{
	processing_phase_e pos = PROCESS_PHA_WRITE_BUSY;

	// Erase test case dpin 0009 ~ 0012
	if (g_plr_state.test_num >= 9 && g_plr_state.test_num < 12)
	{
		pos = PROCESS_PHA_ERASE;
	}
	else if (g_plr_state.test_num == 12)
	{
		pos = PROCESS_PHA_SANITIZE;
	}
	else {
		pos = random_ratio_poweroff_phase();
	}

	return pos;
}

static poweroff_type_e select_poweroff_type(processing_phase_e poff_phase)
{
	/*
	 * KKR 16022 calib를 안하고 있기때문에 delay로 한다.
	 */
//	poweroff_type_e poff_type = POFF_TYPE_TIMING;
	poweroff_type_e poff_type = POFF_TYPE_DELAY;

	if (poff_phase == PROCESS_PHA_OP_CONDITION) {
		poff_type = POFF_TYPE_DELAY;
	}

	return poff_type;
}

static u64 get_poweroff_value_for_timing(processing_phase_e poff_phase, uint req_len, int option)
{
	u64 poff_value = 0;
	u32 max_timeout = 0;
	
	switch (poff_phase) {

	case PROCESS_PHA_WRITE_BUSY:
	case PROCESS_PHA_DATA_TRANS:
	case PROCESS_PHA_CACHE_FLUSH: 		//ieroa. 2015.11.18
		if (g_plr_state.b_packed_enable) {
			poff_value = (u64)calib_generate_internal_po_time(g_packed_buf_offset / SECTOR_SIZE);	
		}
		else {
			poff_value = (u64)calib_generate_internal_po_time(req_len);
		}
		break;
	case PROCESS_PHA_ERASE:
		// 60us ~ 10ms
		poff_value = get_usec2tick((well512_rand() % 99940) + 60);
		break;
		
	case PROCESS_PHA_SANITIZE:
		// 100ms ~ 1000ms
		poff_value = get_msec2tick((well512_rand() % 900) + 100);
		break;
		
	case PROCESS_PHA_SLEEP_AWAKE:
		// 30us ~ max_timeout/10
		max_timeout = get_dd_s_a_timeout();
		max_timeout = (max_timeout / 10);

		// Minimum
		if (max_timeout > 30)
			max_timeout -= 30;
		else if (max_timeout < 1)
			max_timeout = 1;
		
		poff_value = get_usec2tick( (well512_rand() % max_timeout) + 30 );
		break;
		
	case PROCESS_PHA_BKOPS:
		// 200ms ~ 1000ms
		poff_value = get_msec2tick( (well512_rand() % 800) + 200 );
		break;
		
	case PROCESS_PHA_HPI:
		// 50us ~ max_timeout/10
		max_timeout = get_dd_hpi_out_of_int_time();

		// to uSec & 1/10
		max_timeout = (max_timeout * 1000) / 10;

		// Minimum 50 us
		if (max_timeout > 50)
			max_timeout -= 50;
		else if (max_timeout < 1)
			max_timeout = 1;
		
		poff_value = get_usec2tick( (well512_rand() % max_timeout) + 50 );
		break;
		
	case PROCESS_PHA_PON:
		// 50us ~ max_timeout/10
		if (option == PON_SHORT)
			max_timeout = get_dd_poweroff_notify_timeout(PON_SHORT);
		else
			max_timeout = get_dd_poweroff_notify_timeout(PON_LONG);

		// to uSec & 1/10
		max_timeout = (max_timeout * 1000) / 10;

		// Minimum
		if (max_timeout > 50)
			max_timeout -= 50;
		else if (max_timeout < 1)
			max_timeout = 1;

		poff_value = get_usec2tick( (well512_rand() % max_timeout) + 50 );
		break;
		
	default:
		poff_value = get_msec2tick(well512_rand() % 10);
			break;
		
	}

	return poff_value;
}

static u64 get_poweroff_value_for_delay(processing_phase_e poff_phase, uint req_len, int option)
{
	u64 poff_value = 0;
	u32 max_timeout = 0;

	switch (poff_phase) {
		
	case PROCESS_PHA_SLEEP_AWAKE:
		// 10us ~ max_timeout/10
		max_timeout = get_dd_s_a_timeout();
		max_timeout = (max_timeout / 10) - 10;
		if (max_timeout < 1)
			max_timeout = 1;
		
		poff_value = get_usec2tick( (well512_rand() % max_timeout) + 10 );
		break;
	case PROCESS_PHA_BKOPS:
		// 200ms ~ 2000ms
		poff_value = (u64)((well512_rand() % 1800) + 200);
		break;
	case PROCESS_PHA_HPI:
		// 50us ~ 1000us
		poff_value = (u64)((well512_rand() % 950) + 50);
		break;
	case PROCESS_PHA_OP_CONDITION:
		// 0 ~ 50 ms
		poff_value = (u64)well512_rand() % 50;
		break;
	case PROCESS_PHA_DATA_TRANS:
	case PROCESS_PHA_WRITE_BUSY:
		/*
		 * 16KB 이하의 request의 경우 
		 */
		if (req_len <= 32)
			poff_value = (u64)well512_rand() % 20;
		else
			poff_value = (u64)well512_rand() % 90 + 10;

		break;
	default:
		// 0 ~ 100 ms
		poff_value = (u64)well512_rand() % 100;
		break;				
	}

	return poff_value;
}

static u64 get_poweroff_value(poweroff_info_t *poff_info, uint req_len, int option)
{
	u64 poff_value = 0;
	poweroff_type_e 	poff_value_type = poff_info->poff_type;
	processing_phase_e 	poff_phase = poff_info->poff_phase;

	if (poff_value_type == POFF_TYPE_TIMING) 
	{
		poff_value = get_poweroff_value_for_timing(poff_phase, req_len, option);
	} 
	else if (poff_value_type == POFF_TYPE_DELAY) 
	{
		poff_value = get_poweroff_value_for_delay(poff_phase, req_len, option);
	}

	return poff_value;
}

// joys,2015.06.19
static processing_phase_e poweroff_processing(uint req_len, int option)
{
	processing_phase_e phase = PROCESS_PHA_NONE;
	u32 current_time = 0;
	static u32 keep_up_phase_cnt = 0;

	static internal_info_t int_info = {0};
	poweroff_info_t *poff_info = NULL;

	if (!g_plr_state.internal_poweroff_type || IS_AGING_TEST)
		return 0;

	poff_info = &int_info.poff_info;

	/* Set poweroff require value */
	if (g_plr_state.b_first_commit == TRUE)
	{
		current_time = get_rtc_time();
		
		if (g_require_powerloss == FALSE && current_time > g_powerloss_time_boundary) {
			g_require_powerloss = TRUE;
			keep_up_phase_cnt = 0;
			int_info.action = INTERNAL_ACTION_POWEROFF;
			poff_info->b_internal = g_plr_state.internal_poweroff_type;
			poff_info->poff_phase = select_poweroff_phase(poff_info);
			poff_info->poff_type = select_poweroff_type(poff_info->poff_phase);
			poff_info->b_poff_require = g_require_powerloss;
		}

		if (g_require_powerloss) {
			if (++keep_up_phase_cnt > 5000) {
				poff_info->poff_phase = select_poweroff_phase(poff_info);
				poff_info->poff_type = select_poweroff_type(poff_info->poff_phase);
				keep_up_phase_cnt = 0;
				printf ("Change poff phase!!!======\n");
			}
			poff_info->poff_value = get_poweroff_value(poff_info, req_len, option);
//			printf ("\n Send internal info\n");
//            plr_info("poff_info->poff_phase = %d\n", poff_info->poff_phase); 
			dd_send_internal_info((void*)&int_info);
			phase = poff_info->poff_phase;
		}
	}

	return phase;
}

static int packed_send_list(uchar* buf, uint req_start, uint req_len, bool is_commit)
{
	int ret = 0;

	if ( g_plr_state.b_packed_enable ) {
		if (dd_mmc_get_packed_count()) {
			ret = dd_packed_send_list();
		}
		
		g_packed_buf_offset = 0;
			
		if (ret)
			return ret;
		
		g_last_packed_sector = req_start;

	}

	return ret;
}

static int write_request_packed(uchar* buf, uint req_start, uint req_len, uint req_seq_num, uint next_start_sector, bool is_commit)
{
	int ret = 0;

	make_header_data(buf + g_packed_buf_offset, req_start, req_len, req_seq_num, next_start_sector, is_commit);
	
retry:
	ret = dd_packed_add_list(req_start, (lbaint_t)req_len, buf + g_packed_buf_offset, MMC_PACKED_WR_HDR);
	
	switch (ret){
		case MMC_PACKED_ADD :
			g_packed_buf_offset += (req_len * SECTOR_SIZE);
			ret = 0;
			break;
		case MMC_PACKED_PASS :
			ret = 0;
			break;
		case MMC_PACKED_COUNT_FULL :
		case MMC_PACKED_BLOCKS_FULL :
		case MMC_PACKED_CHANGE :
			ret = packed_send_list(buf, req_start, req_len, is_commit);
			break;
		case MMC_PACKED_BLOCKS_OVER :
			ret = packed_send_list(buf, req_start, req_len, is_commit);
			goto retry;
			break;
		default :
			break;
	}
	
	return ret;
}

static int write_data_pages(uchar * buf, uint req_start, uint req_len, uint req_seq_num, uint next_start_sector, bool is_commit_page)
{
	int ret = 0;
	
	if (req_start < g_plr_state.test_start_sector) {
		plr_err("start sector is smaller than test_start_sector!!\n");
		return -PLR_EINVAL;
	}
	else if ( req_len > g_plr_state.test_sector_length + g_plr_state.test_start_sector - req_start )
		req_len = g_plr_state.test_sector_length + g_plr_state.test_start_sector - req_start;
	else if ( req_start > g_plr_state.test_sector_length + g_plr_state.test_start_sector ) {
		plr_err("start sector overflow!!\n");
		return -PLR_EINVAL;
	}
	else if (req_len == 0) {
		plr_err("length is zero!!\n");
		return -PLR_EINVAL;
	}

	/* Ensure the First commit write */
	if (g_plr_state.b_first_commit == FALSE && is_commit_page)
	{
		g_powerloss_time_boundary = get_rtc_time() + (u32)get_power_loss_time();	
		g_plr_state.b_first_commit = TRUE;
	}
	
	/*
	* ieroa, 2015.11.18
	* previcous 
	* 	: poweroff_processing(req_len, 0);
	*/
	if(g_plr_state.b_cache_enable == TRUE)
	{	
		calib_add_cached_write_data_size(req_len);			
		poweroff_processing(calib_get_cached_write_size_before_flush(), 0);
	}
	else 
		poweroff_processing(req_len, 0);
	
	if (g_plr_state.b_packed_enable) {
		ret = write_request_packed(buf, req_start, req_len, req_seq_num, next_start_sector, is_commit_page);

		if (ret)
			return ret;

		if (g_plr_state.packed_flush_cycle != 0) 
			if (DO_PACKED_FLUSH(req_seq_num, g_plr_state.packed_flush_cycle))
				ret = packed_send_list(buf, req_start, req_len, is_commit_page);
	}
	else 
		ret = write_request_normal(buf, req_start, req_len, req_seq_num, next_start_sector, is_commit_page);
	
	return ret;
}


static int erase_data_pages(uint req_start, uint req_len, uint req_seq_num)
{
	int ret = 0;
	
	if (req_start < g_plr_state.test_start_sector) {
		plr_err("length is zero!!\n");
		return -PLR_EINVAL;
	}
	else if ( req_len > g_plr_state.test_sector_length + g_plr_state.test_start_sector - req_start )
		req_len = g_plr_state.test_sector_length + g_plr_state.test_start_sector - req_start;
	else if ( req_start > g_plr_state.test_sector_length + g_plr_state.test_start_sector ) {
		plr_err("start sector overflow!!\n");
		return -PLR_EINVAL;
	}
	else if (req_len == 0) {
		plr_err("length is zero!!\n");
		return -PLR_EINVAL;
	}

	// joys,2015.08.11 
	// TBD
	if (g_plr_state.b_first_commit == FALSE)
	{
		g_powerloss_time_boundary = get_rtc_time() + (u32)get_power_loss_time();	
		g_plr_state.b_first_commit = TRUE;
	}

	poweroff_processing(req_len, 0);

	ret = dd_erase_for_poff(req_start, req_len, get_erase_test_type());

	return ret;
}

static int feature_enable(void)
{
	dd_cache_ctrl(g_plr_state.b_cache_enable);
	
	if (IS_BKOPS_TEST || IS_HPI_TEST) {
		if (dd_bkops_test_enable(TRUE))
			return -1;
	}
	
	if (IS_HPI_TEST) {
		if (dd_hpi_test_enable(TRUE))
			return -1;
	}

	return 0;
}

static int flush_all(void)
{
	int ret = 0;

	ret = packed_flush();

	if (ret)
		return ret;

	if (get_dd_cache_enable()) {
		ret = dd_cache_flush();
		if (ret)
			return ret;
	}
	return ret;
}

static int sleep_request(void)
{
	int ret = 0;

	ret = flush_all();

	if (ret)
		return ret;

//	ret = dd_sleep(TRUE);
	ret = dd_sleep();

	return ret;
}

static int awake_request(void)
{
	int ret = 0;
	
	ret = dd_awake();
	// Will be able to internal poweroff while op command sending
	//ret = dd_init_awake();

	return ret;
}

// poweroff notification feature
static int poff_notify_request(void)
{
	int ret = 0, option = PON_SHORT;

	ret = flush_all();

	if (ret)
		return ret;

	if ((well512_rand() % 2U ) == 0)
		option = PON_LONG;

	ret = dd_poweroff_notify(option);

	if (ret)
		return ret;

	if (dd_internal_power_control(FALSE)) {
		plr_err("[%s] Internal power off failed\n", __func__);
		return -1;
	}

	mdelay(10);

	// Will be able to internal poweroff while op command sending
	ret = dd_internal_power_control(TRUE);
	if (ret)
		return ret;

	ret = feature_enable();

	return ret;
}

static int op_condition_request(void)
{
	int ret = 0, option = PON_SHORT;

	if (IS_INTERNAL_TEST) {
		if (poweroff_processing(0, 0) != PROCESS_PHA_OP_CONDITION)
			return 0;
	}

	ret = flush_all();

	if (ret)
		return ret;

	if (IS_AGING_TEST && IS_PON_TEST)
	{
		if ((well512_rand() % 2U ) == 0) {
			if ((well512_rand() % 2U ) == 0)
				option = PON_LONG;
			
			ret = dd_poweroff_notify(option);
			if (ret)
				return ret;
		}
	}

	if (dd_internal_power_control(FALSE)) {
		plr_err("[%s] Internal power off failed\n", __func__);
		return -1;
	}

	mdelay(10);

	// Occur internal poff
	ret = dd_internal_power_control(TRUE);

	if (ret)
		return ret;

	ret = feature_enable();

	return ret;
}

// joys,2015.09.23 Extend feature
static int  extend_feature_request(uint lsn)
{
	int ret = 0;
	uint rand = 0;
	
	if (IS_SLEEP_N_AWAKE_TEST || IS_PON_TEST || IS_OPCON_TEST) {
		rand = (well512_rand2() % 1000U);
	}

	if (IS_SLEEP_N_AWAKE_TEST) {
		if ( rand < 1) {
			ret = sleep_request();
			if (ret) 
				goto out;
			mdelay(10);
			ret = awake_request();
			if (ret)
				goto out;
			ret = feature_enable();
			if (ret)
				goto out;
		}
	}

	if (IS_OPCON_TEST) {
		if (rand == 1) {
			ret = op_condition_request();
			if (ret)
				goto out;
		}
	}
	
	if (IS_PON_TEST) {
		if (rand > 998) {
			ret = poff_notify_request();
			if (ret)
				goto out;
		}
	}

out:	
	if (ret && ret != -(INTERNAL_POWEROFF_FLAG))
		ret = PLR_EIO;
	return ret;
}

/* ------------------------------------------------
* Middle Layer Function 
* ------------------------------------------------*/

void* plr_get_read_buffer(void)
{
	return (void *)READ_BUF_ADDR;	
}	

void* plr_get_write_buffer(void)
{
	return (void *)WRITE_BUF_ADDR;	
}	

void* plr_get_extra_buffer(void)
{
	return (void *)EXTRA_BUF_ADDR;	
}	

void plr_print_state_info(void)
{
	plr_info_highlight ("\n ----------------------------------- \n"
		" test_name : %s\n"
		" test_type1 : %c\n"
		" test_type2 : %c\n"
		" test_type3 : %c\n"
		" test_type4 : %c\n"
		" test_num : %u\n"
		" minor version : %u\n"
		" boot_cnt : %u\n"
		" loop_cnt : %u\n"
		" checked_addr : %u\n"
		" poweroff sector : %u\n"
		" last flush sector : %u\n"
		" total device sectors : %u\n"
		" test_start_sector : %u\n"
		" test_sector_length : %u\n",
		g_plr_state.test_name, 
		g_plr_state.test_type1, g_plr_state.test_type2, 
		g_plr_state.test_type3, g_plr_state.test_type4, 
		g_plr_state.test_num, g_plr_state.test_minor,
		g_plr_state.boot_cnt, 
        g_plr_state.loop_cnt, 
        g_plr_state.checked_addr, 
        g_plr_state.poweroff_pos,
        g_plr_state.last_flush_pos,
        g_plr_state.total_device_sector,
        g_plr_state.test_start_sector, g_plr_state.test_sector_length);

	plr_info_highlight(
		" event1_type : %u\n"
		" event1_cnt : %u\n"
		" event2_type : %u\n"
		" event2_cnt : %u\n",
		g_plr_state.event1_type,
		g_plr_state.event1_cnt,
		g_plr_state.event2_type,
		g_plr_state.event2_cnt);			

	plr_info_highlight(" cache : %d\n"
		" cache flush cycle : %u\n"
		" packed : %d\n"
		" packed flush cycle : %u\n"
		" reserved : %u\n",
		g_plr_state.b_cache_enable,
		g_plr_state.cache_flush_cycle,
		g_plr_state.b_packed_enable,
		g_plr_state.packed_flush_cycle,
		g_plr_state.reserved);			

	plr_info_highlight(
		" internal_poweroff_type : %d\n"
		" pl_time_min : %u\n"
		" pl_time_max : %u\n",
		g_plr_state.internal_poweroff_type,  
		g_plr_state.pl_info.pl_time_min, 
		g_plr_state.pl_info.pl_time_max);
	
	plr_info_highlight(
		" finish : type [%d] value [%d]\n"
		" filled : type [%d] value[%x]\n"
		" Ratio Filled : %d%%\n",
		g_plr_state.finish.type,  g_plr_state.finish.value,
		g_plr_state.filled_data.type, g_plr_state.filled_data.value,
		g_plr_state.ratio_of_filled );

	if (g_plr_state.chunk_size.type) {
		plr_info_highlight(
			" plr_chunk_size : Random[%d] 4K[%d%%] 4K~64KB[%d%%] 68KB ~[%d%%]\n",
			g_plr_state.chunk_size.type, g_plr_state.chunk_size.ratio_of_4kb,
			g_plr_state.chunk_size.ratio_of_8kb_64kb, g_plr_state.chunk_size.ratio_of_64kb_over
		);
	}
	else {
		plr_info_highlight(
			" plr_chunk_size : Fixed[%d] %dK\n",
			g_plr_state.chunk_size.type, g_plr_state.chunk_size.ratio_of_4kb
		);
	}
	plr_info_highlight (" ----------------------------------- \n");
}


void print_head_info(struct plr_header *header)
{	
	plr_err ("--------------------------------------\n");
	plr_err ("magic number   : 0x%08X\n", header->magic_number);	
	plr_err ("sector number  : 0x%07X\n", header->lsn);
	plr_err ("boot cnt       : %d\n", header->boot_cnt);		
	plr_err ("loop cnt       : %d\n", header->loop_cnt);		
	plr_err ("request cnt    : %d\n", header->req_info.req_seq_num);
	plr_err ("start sector   : 0x%07X\n", header->req_info.start_sector);
	plr_err ("page index     : %d\n", header->req_info.page_index);
	plr_err ("page num       : %d\n", header->req_info.page_num);
	plr_err ("next req addr  : 0x%07X\n", header->next_start_sector);
	plr_err ("reserved1      : %d\n", header->reserved1);
	// plr_err ("reserved2      : %d\n", header->reserved2);
	plr_err ("CRC            : 0x%08X\n", header->crc_checksum);	
	plr_err ("--------------------------------------\n");
}

static bool is_digit(char ch)
{
	if (ch < '0' || ch > '9')
		return FALSE;

	return TRUE;
}

static bool is_xdigit_lower(char ch)
{
	if (ch < 'a' || ch > 'f')
		return FALSE;

	return TRUE;
}

static bool is_xdigit_upper(char ch)
{
	if (ch < 'A' || ch > 'F')
		return FALSE;

	return TRUE;
}


bool is_numeric(const char * cp)
{
	uint base = 10;
	char ch = 0;
	
	if (*cp == '0') {
		cp++;
		if ((*cp == 'x')) {
			base = 16;
			cp++;
		}
		else {
			base = 10;
		}
	}
	
	while ((ch = *cp++) != '\0') {
		if ( base == 10) {
			if (is_digit(ch) == FALSE)
				return FALSE;
		}
		else if (base == 16) {
			if (is_digit(ch) == FALSE && is_xdigit_lower(ch) == FALSE && is_xdigit_upper(ch) == FALSE)
				return FALSE;
		}
	}
	
	return TRUE;
}

uint _strspn(const char *s, const char *accept)
{
	const char *p;
	const char *a;
	size_t count = 0;

	for (p = s; *p != '\0'; ++p) {
		for (a = accept; *a != '\0'; ++a) {
			if (*p == *a)
				break;
		}
		if (*a == '\0')
			return count;
		++count;
	}

	return count;
}

uint strtoud(const char *cp)
{
	uint result = 0, value = 0, base = 10;
	char ch = 0;

	if (*cp == '0') {
		cp++;
		if ((*cp == 'x')) {
			base = 16;
			cp++;
		}
		else {
			base = 10;
		}
	}
	
	while ((ch = *cp++) != '\0') {
		if ( base == 10) {
			if (is_digit(ch)) 
				value = ch - '0';
			else {
				plr_err("invalid digit\n");
				return 0;
			}
		}
		else if (base == 16) {
			if (is_digit(ch))
				value = ch - '0';
			else if (is_xdigit_lower(ch))
				value = ch - 'a' + 10;
			else if (is_xdigit_upper(ch))
				value = ch - 'A' + 10;
			else {
				plr_err("invalid digit\n");
				return 0;
			}
		}
		result = result * base + value;
	}
	return result;
}

void display_date(void)
{
	// joys,2015.08.04
	/* Occur reset rtc_ctrl_register in date command */
	/* Don't use */
	return;
//	puts("\n");
//	run_command("date", 0);
}

void set_poweroff_info(void)
{
	char send_buf[128];

	if (!g_plr_state.b_cache_enable)
		g_plr_state.last_flush_pos = 0;
	
	sprintf(send_buf, "%d/%d", g_plr_state.poweroff_pos, g_plr_state.b_cache_enable? g_plr_state.last_flush_pos : 0);
	send_token( PLRTOKEN_SET_PLOFF_INFO, send_buf );
}

int write_request_normal(uchar * buf, uint req_start, uint req_len, uint req_seq_num, uint next_start_sector, bool is_commit)
{
	int ret = 0;

	make_header_data(buf, req_start, req_len, req_seq_num, next_start_sector, is_commit);

	ret = dd_write(buf, req_start, req_len);
	
	return ret;
}

/* ------------------------------------------------
* Common Function 
* ------------------------------------------------*/
uint get_power_loss_time(void)
{
	return g_plr_state.pl_info.pl_time_min 
			+ (well512_rand()%(g_plr_state.pl_info.pl_time_max - g_plr_state.pl_info.pl_time_min));
}

int write_request(uchar * buf, uint req_start, uint req_len, uint req_seq_num, uint next_start_sector)
{
	int ret = 0;

	ret = extend_feature_request(req_start);

	if(ret)
		return ret;
	
	return write_data_pages(buf, req_start, req_len, req_seq_num, next_start_sector, FALSE);
}		
#if 0
int write_flush_page(uchar * buf, uint req_start, uint req_len, uint req_seq_num, uint next_start_sector)
{
	int ret = 0;

	if (g_plr_state.b_cache_enable) {

		if (g_plr_state.b_packed_enable && g_packed_buf_offset != 0)
		{
			ret = packed_send_list(buf, req_start, req_len, FALSE);
			if (ret)
				return ret;
		}

		ret = dd_cache_flush();
		if (ret)
			return ret;
		
		ret = write_data_pages(buf, req_start, req_len, req_seq_num, next_start_sector, TRUE);
		
		if (ret)
			return ret;

		if (g_plr_state.b_packed_enable && g_packed_buf_offset != 0)
			ret = packed_send_list(buf, req_start, req_len, TRUE);

		if (ret)
			return ret;
		
		ret = dd_cache_flush();

		if (ret)
			return ret;

		g_plr_state.last_flush_pos = req_start;
	}
	else {
		plr_err("Cache is not supported!!!");
		ret = -PLR_EIO;
	}
	return ret;
}
#else
int write_flush_request(uchar * buf, uint req_start, uint req_len, uint req_seq_num, uint next_start_sector)
{
	int ret = 0;

	if (g_plr_state.b_cache_enable) {

		if (g_plr_state.b_packed_enable && g_packed_buf_offset != 0)
		{
			ret = packed_send_list(buf, req_start, req_len, FALSE);
			if (ret)
				return ret;
		}
		
		ret = dd_cache_flush();
		if (ret)
			return ret;

		//ieroa, 2015.11.18
		//
		calib_reset_cached_write_data_size();
		
		ret = write_data_pages(buf, req_start, req_len, req_seq_num, next_start_sector, TRUE);		
		if (ret)
			return ret;

		if (g_plr_state.b_packed_enable && g_packed_buf_offset != 0)
		{	
			ret = packed_send_list(buf, req_start, req_len, TRUE);
			if (ret)
				return ret;
		}
		
		ret = dd_cache_flush();
		if (ret)
			return ret;

		//ieroa, 2015.11.18
		calib_reset_cached_write_data_size();

		g_plr_state.last_flush_pos = req_start;
	}
	else {
		plr_err("Cache is not supported!!!");
		ret = -PLR_EIO;
	}

	return ret;
}

//
// @SJD 150629
// Create this function for Cached Internal Power Off
// 
int write_flush_request_for_calib(uchar * buf, uint req_start, uint req_len, uint req_seq_num, uint next_start_sector)
{
	int ret = 0;

	
	if (g_plr_state.b_cache_enable) {

		if (g_plr_state.b_packed_enable && g_packed_buf_offset != 0)
		{
			ret = packed_send_list(buf, req_start, req_len, FALSE);
			if (ret)
				return ret;
		}

		//ieroa 2015.11.10
#ifdef  PLR_CALIBRATION_TEST_IEROA
		calib_set_last_calib_phase(PROCESS_PHA_WRITE_BUSY);
#endif
		
		ret = write_request_normal(buf, req_start, req_len, req_seq_num, next_start_sector, TRUE);		
		if (ret)
			return ret;

		//@sj 150706, for support test case module {{ 		
		if (IS_CACHEON_TEST) {
			calib_add_cached_write_data_size(req_len);	
		}
		//}}

		if (g_plr_state.b_packed_enable && g_packed_buf_offset != 0)
		{	
			ret = packed_send_list(buf, req_start, req_len, TRUE);
			if (ret)
				return ret;
		}

		//ieroa 2015.11.10
#ifdef  PLR_CALIBRATION_TEST_IEROA		
		calib_set_last_calib_phase(PROCESS_PHA_CACHE_FLUSH);
#endif
		
		ret = dd_cache_flush();

		if (ret)
			return ret;

		//@sj 150706, for support test case module {{ 		
		if (IS_CACHEON_TEST) {		
			calib_reset_cached_write_data_size();
		}

		//}}

		g_plr_state.last_flush_pos = req_start;
	}
	else {
		plr_err("Cache is not supported!!!");
		ret = -PLR_EIO;
	}
	return ret;
}

#endif

int get_erase_test_type(void)
{	
	return g_erase_type;
}

void set_erase_test_type(erase_type_e type)
{
	g_erase_type = type;
}

int erase_request(uint req_start, uint req_len, uint req_seq_num)
{
	return erase_data_pages(req_start, req_len, req_seq_num);
}

int packed_flush(void) 
{
	int ret = 0;

	if (g_plr_state.b_packed_enable) {
		if (dd_mmc_get_packed_count())
			ret = dd_packed_send_list();
	
		g_packed_buf_offset = 0;
	}
	
	return ret;
}

uint get_last_packed_sector(void)
{
	return g_last_packed_sector;
}

void set_loop_count(uint n_loop_cnt)
{	
	g_plr_state.loop_cnt = n_loop_cnt;
	
	//send_token_param( PLRTOKEN_SET_LOOPCNT, g_plr_state.loop_cnt );
}

void set_checked_addr(uint n_check_addr)
{
	g_plr_state.checked_addr = n_check_addr;
	
	send_token_param( PLRTOKEN_SET_ADDRESS, g_plr_state.checked_addr);
}

//
// basename(): returns pure filename from file path
// @SJ 150620
//

#ifndef PLR_SYSTEM
extern char* strstr(char* str, char* sub_str);
#endif

char* util_file_basename(char* path)
{
	char *p = path, *p1 = 0;

	int n = MAXPATH; // path length limit

	if ((p = strstr(p, "/"))) 	
	{ 
		p1 = p++;	
		n--;

		while ((p = strstr(p + 1, "/"))) 	
		{ 
			p1 = p++;	
			n--;
		}
	}

	//
	// @sj 150706, Note : you should not call the function uses this function.
	//		  May cause infinite loop trap!!!
	
	if( 0 == n || 1024 == n)  return path;

	return p1 + 1;	
}

