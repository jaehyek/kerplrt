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
#include "test_case/plr_case.h"
#include "plr_err_handling.h"
#include "plr_protocol.h"
#include "plr_deviceio.h"
#include "plr_precondition.h"
#include "plr_tftp.h"
#include "plr_calibration.h"
#include "plr_verify.h"

//#include <linux/wakelock.h>


enum  verification_event {
	EVENT_PARTIAL_WRITE_1 = 1,
	EVENT_PARTIAL_WRITE_2,
	MAX_EVENT
};

extern struct tftp_state g_tftp_state;


int g_prepare_option = 0;



struct plr_state g_plr_state __attribute__ ((aligned(0x8)));

u32 g_powerloss_time_boundary = 0xFFFFFFFF;
bool g_require_powerloss = FALSE;
//bool g_b_powerloss_done = FALSE;


#ifdef PLR_AVOID_CRASH
uint g_crash_cnt;
#endif

typedef int (*plrtest_fn)( uchar * buf, uint test_start_sector, uint test_sector_length );

struct plr_func {
	plrtest_fn initialize;
	plrtest_fn read;
	plrtest_fn write;
};

static struct plr_func *p_plr_func = NULL;

static struct plr_func poff_driver_test[] = {
	/* dpin_test_0.c  */
	{ initialize_dpin_0000, read_dpin_0000, write_dpin_0000 },
	/* dpin_test_1.c  */
	{ initialize_dpin_0001, read_dpin_0001, write_dpin_0001 },
	/* dpin_test_2.c  */
	{ initialize_dpin_0002, read_dpin_0002, write_dpin_0002 },
	/* dpin_test_3.c  */
	{ initialize_dpin_0003, read_dpin_0003, write_dpin_0003 },
	/* dpin_test_4.c  */
	{ initialize_dpin_0004, read_dpin_0004, write_dpin_0004 },
	/* dpin_test_5.c  */
	{ initialize_dpin_0005, read_dpin_0005, write_dpin_0005 },
	/* dpin_test_6.c  */
	{ initialize_dpin_0006, read_dpin_0006, write_dpin_0006 },
	/* dpin_test_7.c  */
	{ initialize_dpin_0007, read_dpin_0007, write_dpin_0007},
	/* dpin_test_08.c  @sj 150716	For Read disturbance test */
	{ initialize_dpin_0008, read_dpin_0008, write_dpin_0008},
	/* dpin_test_9.c 	erase*/
	{ initialize_dpin_0009_12, read_dpin_0009_12, write_dpin_0009_12},	
	/* dpin_test_10.c 	trim*/
	{ initialize_dpin_0009_12, read_dpin_0009_12, write_dpin_0009_12},	
	/* dpin_test_11.c  	discard*/
	{ initialize_dpin_0009_12, read_dpin_0009_12, write_dpin_0009_12},	
	/* dpin_test_12.c  	sanitize*/
	{ initialize_dpin_0009_12, read_dpin_0009_12, write_dpin_0009_12},
	/* dpin_test_13.c */
	{ initialize_dpin_0013, read_dpin_0013, write_dpin_0013},
	/* dpin_test_14.c */
	{ initialize_dpin_0014, read_dpin_0014, write_dpin_0014},
	/* dpin_test_15.c */
	{ initialize_dpin_0015, read_dpin_0015, write_dpin_0015}
};

static struct plr_func aging_normal_test[] = {
	/* daxx_test_0.c  */
	{ initialize_daxx_0000, read_daxx_0000, write_daxx_0000 },
	/* daxx_test_1.c  */
	{ initialize_daxx_0001, read_daxx_0001, write_daxx_0001 },
	/* daxx_test_2.c  */
	{ initialize_daxx_0002, read_daxx_0002, write_daxx_0002 },
	/* daxx_test_2.c  */ // for static performance test // joys,2015.07.13
	{ initialize_daxx_0003_4, read_daxx_0003_4, write_daxx_0003_4 },

	//@sj 150805 {
	{ initialize_daxx_0003_4, read_daxx_0003_4, write_daxx_0003_4 }, //4
	{ initialize_dummy, read_dummy, write_dummy }, //5
	{ initialize_dummy, read_dummy, write_dummy }, //6
	{ initialize_dummy, read_dummy, write_dummy }, //7
	//@sj 150805 }
	{ initialize_daxx_0008, read_daxx_0008, write_daxx_0008 },
	{ initialize_daxx_0009, read_daxx_0009, write_daxx_0009 },
	{ initialize_daxx_0010, read_daxx_0010, write_daxx_0010 },
	{ initialize_daxx_0011, read_daxx_0011, write_daxx_0011 },	
	{ initialize_daxx_0012, read_daxx_0012, write_daxx_0012 }
};

 /* ---------------------------------------------------------------------------------------
  * Static function
  *----------------------------------------------------------------------------------------
  */
static void make_body_data(uchar * buf)
{
	uint data = g_plr_state.filled_data.value;
	uchar *buf_offset = 0;
	uint page_index = 0;
	uint data_index = 0;
	uint data_cnt = 0;

	// loop for max_read_chunk
	for (page_index = 0; page_index < MAX_PAGE_OF_READ_CHUNK; page_index++) {

		// Search data index in page
		buf_offset = buf + (page_index * PAGE_SIZE) + sizeof(struct plr_header);

		// Exclude header size 
		data_cnt = (PAGE_SIZE - sizeof(struct plr_header)) / sizeof(data);

		// loop for one_page_end
		for (data_index = 0; data_index < data_cnt; data_index++) {
			*( ((uint*)buf_offset) + data_index ) = data;
		}
	}
}
 
int plr_start_initialization(void)
{
	uchar * buf = (uchar *)plr_get_read_buffer();

	if (p_plr_func[g_plr_state.test_num].initialize != NULL)
		return p_plr_func[g_plr_state.test_num].initialize(	buf, 
														g_plr_state.test_start_sector, 
														g_plr_state.test_sector_length);

	return 0;
}

static int plr_start_verification(void)
{
	uchar * buf = (uchar *)plr_get_read_buffer();

	if (IS_AGING_TEST  && g_plr_state.loop_cnt == 1) 
		return 0;

	if (IS_POWERLOSS_TEST && g_plr_state.boot_cnt == 1)	
		return 0;

	if (g_plr_state.internal_poweroff_type)
		memset(buf, 0, READ_CHUNK_SIZE);

	return p_plr_func[g_plr_state.test_num].read(	buf, 
													g_plr_state.test_start_sector, 
													g_plr_state.test_sector_length);
}

extern uint g_packed_buf_offset;

static int plr_start_writing( void )
{
	uchar * buf = plr_get_write_buffer();
	int ret = 0;

	if (g_plr_state.internal_poweroff_type) {
		g_powerloss_time_boundary = get_rtc_time() + (u32)get_power_loss_time();
	}
	
	if (g_plr_state.b_cache_enable) {
		plr_info_highlight("Cache Enable\n");
		if (!get_dd_cache_enable()) {
			plr_err("Cache is not supported!!\n");
			return -PLR_EINVAL;
		}
	}
	else {
		plr_info_highlight("Cache Disable\n");
	}
	
	if (g_plr_state.b_packed_enable) {
		plr_info_highlight("Packed Enable\n");		
		if (get_dd_packed_enable()) {
			buf = (uchar*)dd_packed_create_buff(buf);
		}
		else {
			plr_err("Packed is not supported!!\n");
			return -PLR_EINVAL;
		}
	}
	else {
		plr_info_highlight("Packed Disable\n");		
	}

	if (IS_BKOPS_TEST || IS_HPI_TEST) {
		dd_bkops_test_enable(TRUE);
		plr_info_highlight("Bkops Enable\n");
		if (!get_dd_bkops_enable()) {
			plr_err("Bkops is not supported!!\n");
				return -PLR_EINVAL;
		}
	}
	
	if (IS_HPI_TEST) {
		dd_hpi_test_enable(TRUE);

		plr_info_highlight("HPI Enable\n");
		if (!get_dd_hpi_enable()) {
			plr_err("HPI is not supported!!\n");
				return -PLR_EINVAL;
		}
	}

	if (IS_SLEEP_N_AWAKE_TEST) {
		plr_info_highlight("Sleep N Awake Enable\n");
	}

	if (IS_PON_TEST) {
		plr_info_highlight("PON Enable\n");
	}

	if (IS_OPCON_TEST) {
		plr_info_highlight("OP Condition test Enable\n");
	}

	make_body_data(buf);

	plr_debug("0. g_plr_state.test_num[%d]\n", g_plr_state.test_num);
	ret =  p_plr_func[g_plr_state.test_num].write(	buf, 
													g_plr_state.test_start_sector, 
													g_plr_state.test_sector_length);

	if (g_plr_state.b_packed_enable) {
		if (IS_AGING_TEST)
			packed_flush();
		dd_packed_delete_buff();
	}
	
	if (ret == -(INTERNAL_POWEROFF_FLAG)) {
		plr_info_highlight("================= Power Off occured ==================\n");
	}
	
	return ret;
}

static int previous_crash_state_result(void)
{
	int crash_num = 0;
	char send_buf[PLRTOKEN_BUFFER_SIZE] = {0};

	if(get_crash_state_sdcard(&crash_num)) {
		plr_err("[%s]previous_crash_state read failed.\nPlease check for sdcard\n", __func__);
		send_token(PLRTOKEN_ENV_ERROR, NULL);
		return -1;
	}

	if(crash_num == 0)
		return 0;

	if (crash_num < 0) {
		crash_num *= -1;
	}

	sprintf (send_buf, "%d/%d/%d/%d/%d/", 
				crash_num, 
				EVENT_PARTIAL_WRITE_1, 
				g_plr_state.event1_cnt, 
				EVENT_PARTIAL_WRITE_2, 
				g_plr_state.event2_cnt);
	
	send_token(PLRTOKEN_VERI_DONE, send_buf);

	send_token(PLRTOKEN_CRASH_LOG_START, NULL);
	verify_print_crash_info((uchar*)plr_get_read_buffer(), &g_plr_state);
	send_token(PLRTOKEN_CRASH_LOG_END, NULL);

	plr_info_highlight("If you want to see the information on broken data, Enter tr command\n");

	return crash_num;
}

static int verification_error_result(int result)
{
	int err_num 	= 0;
	int bcrashed 	= FALSE;
	char send_buf[PLRTOKEN_BUFFER_SIZE] = {0};

	if (result) {
		err_num = verify_get_error_num(&g_plr_state);

		if (!err_num) 
		{
			err_num = verify_get_crash_num(&g_plr_state);
			bcrashed = TRUE;
			if(set_crash_state_sdcard(PLR_ECRASH)) {
				plr_err("[%s]previous_crash_state read failed.\nPlease check for sdcard\n", __func__);
				send_token(PLRTOKEN_ENV_ERROR, NULL);
				return -1;
			}
		}
	}

	if (err_num < 0) {
		err_num *= -1;
	}

	sprintf (send_buf, "%d/%d/%d/%d/%d/", 
				err_num,
				EVENT_PARTIAL_WRITE_1, 
				g_plr_state.event1_cnt, 
				EVENT_PARTIAL_WRITE_2, 
				g_plr_state.event2_cnt);
	
	send_token(PLRTOKEN_VERI_DONE, send_buf);

	if (bcrashed == TRUE) {
		send_token(PLRTOKEN_CRASH_LOG_START, NULL);
		verify_print_crash_info((uchar*)plr_get_read_buffer(), &g_plr_state);
		send_token(PLRTOKEN_CRASH_LOG_END, NULL);
	}
	
	return err_num;
}

static int send_write_result(int result)
{
	if (result == -(INTERNAL_POWEROFF_FLAG))
		result = 0;
	
	if (result < 0) {
		result *= -1;
	}

	if (g_plr_state.internal_poweroff_type && !IS_AGING_TEST) {
		set_poweroff_info();
	}

	send_token_param(PLRTOKEN_WRITE_DONE, (uint)result);	

	if (result)
		plr_err("Start writing failed\n");
	
	return result;
}

static int send_device_info(void)
{	
	int ret = 0, i = 0;
	char send_buf[PLRTOKEN_BUFFER_SIZE] = {0};
	char proc_name[32] = {0};
	int bcache_enable = 0;
	g_plr_state.total_device_sector = get_dd_total_sector_count();
	
	bcache_enable = get_dd_cache_enable();
	ret = get_dd_product_name(proc_name);

	// trim trailing non-alpha-numeric characters
	for(; i < 32; ++i){
		plr_debug("proc_name %d [0x%02x][%c]\n", i, proc_name[i], proc_name[i]);
		
        if (!(proc_name[i] >= 'A' && proc_name[i] <= 'z') && !(proc_name[i] >= '0' && proc_name[i] <= '9')){
			proc_name[i] = ' ';
		}
	}
	
	if (ret || strlen(proc_name) < 1) {
		plr_err("Error!! get device information failed!\n");
		sprintf (proc_name, "%s", "unknown");
		
		return -1;
	}

	// product name/capacity(MB)/cache on/off
	sprintf (send_buf, "%s/%d/%d/", proc_name, g_plr_state.total_device_sector/2048, bcache_enable);

#ifndef PLR_SYSTEM
	send_token(PLRTOKEN_BOOT_DONE, send_buf);
#else
//	send_token(PLRTOKEN_BOOT_DONE, send_buf);
#endif

	// set date time with date/time information received from EF Monitor
	// Caption!! rtc tick counter is reset
	sprintf(send_buf, "date %s", g_plr_state.date_info);

	ret = run_command(send_buf, 0);
	
	if(ret == -1) {
	 	plr_err("date configuration failed\n");
		send_token(PLRTOKEN_ENV_ERROR, NULL);
		run_command("reset", 0);
	}

	return 0;
}


//ieroa, 20150901 : Warning "send_version_info" defined but not used.
#if 0
static void send_version_info(void)
{
	/*
	int ret = 0;
	char proc_revision[24] = {0};
	ret = get_dd_firmware_version(proc_revision);
	send_token(PLRTOKEN_PREPARE_DONE, proc_revision);
	*/
	
	char send_buf[PLRTOKEN_BUFFER_SIZE] = {0};
	char temp_buf[128] = {0};
	char year_char = 'A';  
	char month_char = 'A';
	char day_str[3] = {0,};
	const char month[12][4] = { "Jan","Feb","Mar","Apr","May","Jun",
                                "Jul","Aug","Sep","Oct","Nov","Dec" };
	int i = 0;
	uint year = 0;
		
	snprintf(temp_buf, 128, (const char*)"%s", __DATE__);

	printf("DATE : %s", temp_buf);
	year = strtoud(&temp_buf[7]);
	year_char =  'A' + ((year - 2001) % 26);
		
	for (i=0; i<12; i++) {
		if (!strncmp(&temp_buf[0], month[i], 3)) {
			month_char = 'A' + i;
			break;
		}
	}

	memcpy(day_str, &temp_buf[4], 2);

	plr_debug("%s %d %02d\n", day_str, strtoud(day_str), strtoud(day_str));
	sprintf(send_buf, "%c%c%02d/", year_char, month_char, strtoud(day_str));
	send_token(PLRTOKEN_PREPARE_DONE, send_buf);
	
	return;

}
#endif

static int check_device_command(void)
{
	uchar *buffer = (uchar *)plr_get_write_buffer();	
	
	plr_info("Check the eMMC operation...\n");

	/*read test*/
	plr_info("Read...   ");
	if(dd_read(buffer, g_plr_state.total_device_sector - 40, 40) < 0) {
		plr_err("[%s] Read failed! sector : 0x%x, len : 40\n", __func__, g_plr_state.total_device_sector - 40);
		return -1;
	}
	plr_info ("success\n");
	
	/*write test*/
	plr_info("Write...   ");
	if(dd_write(buffer, g_plr_state.total_device_sector - 40, 40) < 0) {
		plr_err("[%s] Write failed! sector : 0x%x, len : 40\n", __func__, g_plr_state.total_device_sector - 40);
		return -1;
	}
	plr_info ("success\n");

	plr_info("Finish the Checking. \n");
	return 0;
}

int register_func_table(void)
{
#ifndef PLR_SYSTEM
	if (!IS_DRIVER_TEST && !IS_SYSTEM_TEST){
		return -1;
	}
#endif
	if (IS_POWERLOSS_TEST) {
		p_plr_func = poff_driver_test;

		if (g_plr_state.test_num >= sizeof(poff_driver_test) / sizeof(struct plr_func)) {
					return -1;
		}	 
	}
	else if (IS_AGING_TEST) {
		if (IS_NORMAL_AGING_TEST) {
			p_plr_func = aging_normal_test;

            plr_info("%d\n", sizeof(aging_normal_test) / sizeof(struct plr_func));
            plr_info("test_num %d\n", g_plr_state.test_num);
			if (g_plr_state.test_num >= sizeof(aging_normal_test) / sizeof(struct plr_func)) {
				return -1;
			}	 
		}
		else {
			return -1;
		}		
	}
	else {
		return -1;
	}
	return 0;
}

static void adjust_test_sector_length(void)
{
	// Adjust test sector length
	if (g_plr_state.test_sector_length == 0) 
	{
		if (IS_AGING_TEST)
			g_plr_state.test_sector_length = 
				get_dd_total_sector_count() - g_plr_state.test_start_sector;
		else	// Remain 1GB for calibration
			g_plr_state.test_sector_length = 
				get_dd_total_sector_count() - g_plr_state.test_start_sector - ((1<<30) / 512);
	}	
}

/* ---------------------------------------------------------------------------------------
 * Test Main Fuction
 *----------------------------------------------------------------------------------------
 */

int plr_main(int argc, char *argv[])
{
    int fd;
 	int ret = 0;
	char input_buf[PLRTOKEN_BUFFER_SIZE] ={0};
    //struct wake_lock plr_wake_lock;
    int nfd;
//    const char wake_lock_id[] = "plr_test";
    //nfd = open("/sys/android_power/acquire_full_wake_lock", O_RDWR);
//    nfd = open("/sys/power/wake_lock", O_RDWR);
//    write(nfd, wake_lock_id, sizeof(wake_lock_id) - 1);

//    wake_lock_init(&plr_wake_lock, WAKE_LOCK_SUSPEND, "plr_wake_lock");
//    wake_lock(&plr_wake_lock);
//	do_dcache_enable();

	/* Serial console initialization */
	console_init_r();
	
	/* Random seeding */
	well512_seed(get_cpu_timer(0));
	well512_seed2(get_cpu_timer(0));
	
	/* Get crash information from SD card */
	//get_crash_state_sdcard(&crash_state);	//by jk

	plr_info("Booting Completed!!\n");

#if 0
	plr_err("get wake_lock\n");
	fd = open("/sys/power/wake_lock", O_RDWR);                                           
	if(fd < 0) {                                                               
		plr_err("Unable to open wake_lock: %s\n", fd);           
	}
	else
	{
		ret = write(fd, "1", strlen("1")+1);
		if (ret < 0)                                                                
		{                                                                           
			plr_err("[%s] Write power_control fail\n", __func__);                   
		}                                                                           
		close(fd);  
	}

	dd_suspend(10);
	plr_err("resume\n");
#endif

#if 0
	if (!strcmp(basename(argv[0]), "ueventd"))
	{
	plr_info("Booting Completed!!\n");

    //    return ueventd_main(argc, argv);
	}

    if (!strcmp(basename(argv[0]), "watchdogd"))
	{
	plr_info("Booting Completed!!\n");
      //  return watchdogd_main(argc, argv);
	}
#endif
	/* Send MMC device information to EF-Monitor */
	if(send_device_info() < 0){
		send_token(PLRTOKEN_ENV_ERROR, NULL);
		goto out;
	}

	/* Occur reset a rtc_ctrl_register in send_device_info */
	/* @sj 150703 Starts RTC64 */
	reset_tick_count64();

	/* Check whether MMC I/O command can be processed or not */
	if(check_device_command() < 0){
		send_token(PLRTOKEN_ENV_ERROR, NULL);
		ret = -1;
		goto out;
	}

	/* Check whether MMC I/O command can be processed or not */
	switch(g_tftp_state.option){
		case 1:
			// u-boot binary upgrade
			tftp_fw_upgrade();
			goto out;
		case 2:
			// system level execution file upgrade
			goto out;
		case 3:
			// u-boot network setting only
		case 4:
			// u-boot network setting and upload erase count
			send_token(PLRTOKEN_UTFTP_SETTING, NULL);
			
			sprintf(input_buf,
				"setenv ipaddr %s; setenv serverip %s; setenv gatewayip %s; setenv netmask %s; setenv ethaddr %s; setenv tftptimeout 10000; saveenv",
				g_tftp_state.client_ip, g_tftp_state.server_ip, g_tftp_state.gateway_ip, g_tftp_state.netmask, g_tftp_state.mac_addr);

			ret = run_command(input_buf, 0);
						
			if(ret == -1) {
			 	plr_err("u-boot network configuration failed\n");
				send_token(PLRTOKEN_ENV_ERROR, NULL);
				return run_command("reset", 0);
			}
			break;
		default :
			break;
	}

#ifndef PLR_SYSTEM
	send_token(PLRTOKEN_TEST_CASE, NULL);

	ret = register_func_table();

	if (ret) {
	 	plr_err("Not registered test case [%s]\n", g_plr_state.test_name);
		send_token(PLRTOKEN_ENV_ERROR, NULL);
		goto out;
	}

	if (IS_SYSTEM_TEST)
		goto out;
#endif

	sprintf(input_buf, "%s/", PLR_VERSION);
	send_token(PLRTOKEN_PREPARE_DONE, input_buf);
	// send_version_info();

	if (g_prepare_option == 1)
	{
    plr_info("test_num %d\n", g_plr_state.test_num);
    plr_info("test_type2 %s\n", g_plr_state.test_type2);
		if (precondition_by_protocol()) {
			ret = -1;
			goto out;
		}
		else
			goto out;
	}	

	send_token(PLRTOKEN_TEST_INFO, NULL);

	// joys,2016.01.12
	adjust_test_sector_length();
	
#ifdef PLR_SYSTEM
	ret = register_func_table();

	if (ret) {
	 	plr_err("Not registered test case [%s]\n", g_plr_state.test_name);
		send_token(PLRTOKEN_ENV_ERROR, NULL);
		return -1;
	}

#endif

	send_token(PLRTOKEN_POWERLOSS_CONFIG, NULL);

	plr_print_state_info();
	
	/*ieroa*/
	g_plr_state.b_commit_err_cnt_enable = TRUE;
	g_plr_state.commit_err_cnt = 0;

	if (g_plr_state.internal_poweroff_type && g_plr_state.loop_cnt == 0) {
		g_plr_state.loop_cnt = 1;
	}

	ret = plr_start_initialization();

	if (g_plr_state.b_finish_condition)
		goto finish;

	dd_cache_ctrl(g_plr_state.b_cache_enable);

	if (ret) {
		plr_err("initialization failed\n");
		send_token(PLRTOKEN_ENV_ERROR, NULL);
		goto out;
	}
	
	if (g_plr_state.internal_poweroff_type 
		&& !g_plr_state.b_calibration_test 
		&& !IS_AGING_TEST) {
		
		ret = calib_make_table();					// build up calibration table

		if (ret) {
			send_token(PLRTOKEN_ENV_ERROR, NULL);
			goto out;
		}
	}
//    do_mmc_sleep(1);

repeat :
	// joys,2015.08.14
	reset_tick_count64();
	
	//ieroa, power off after first flush.	
	if (g_plr_state.internal_poweroff_type)
	{
		++g_plr_state.boot_cnt;
		dd_send_internal_info(NULL);			// joys,2015.06.25
		
		if (g_plr_state.b_cache_enable == TRUE) {
			g_plr_state.b_first_commit = FALSE;
			g_require_powerloss = FALSE;
		}
		else
			g_plr_state.b_first_commit = TRUE;
	}
	
	dd_cache_ctrl(g_plr_state.b_cache_enable);
	send_token_param(PLRTOKEN_BOOT_CNT, g_plr_state.boot_cnt);

	if (g_plr_state.finish.type == 0 && g_plr_state.boot_cnt > g_plr_state.finish.value)
		goto finish;
	
	if (IS_AGING_TEST)
		plr_info("\n======= Aging Test ======= \n");
	else if (IS_DRIVER_TEST)
		plr_info("\n===== Power Off Test ===== \n");
	
	plr_info(" Test Case [%s]\n", g_plr_state.test_name);
	if (IS_DRIVER_TEST) {
		plr_info(" Boot_Cnt [%d]\n", g_plr_state.boot_cnt);
		plr_info(" Loop_Cnt [%d]\n", g_plr_state.loop_cnt);
	}
	
 	if (g_plr_state.test_minor > 0 && g_plr_state.event1_type != 0)
		plr_info(" %s [%d]\n", "Partial Write 1", g_plr_state.event1_cnt);

	if (g_plr_state.test_minor > 1 && g_plr_state.event2_type != 0)
		plr_info(" %s [%d]\n", "Partial Write 2", g_plr_state.event2_cnt);

	plr_info("========================== \n\n");

	if (g_plr_state.internal_poweroff_type 
		&& !g_plr_state.b_calibration_test 
		&& !IS_AGING_TEST) {
			
		if (g_plr_state.boot_cnt % 1024 == 0 ) {
			ret = calib_make_table();

			if (ret) {
				plr_err("Calibration failed\n");
				send_token(PLRTOKEN_ENV_ERROR, NULL);
				goto out;
			}
		}
	}

	send_token( PLRTOKEN_VERI_START, NULL );
	
	#ifndef PLR_DEBUG
	if (previous_crash_state_result()) {
		ret = -1;
		goto out;
	}
	#endif
	
	/* -----------------------------------------------
	* start verification
	*-------------------------------------------------
	*/

	plr_info("Verification Start!\n");
	ret = plr_start_verification();

	#ifdef PLR_DEBUG
	plr_print_state_info();
	#endif

	if (verification_error_result(ret)) {
		ret = -1;
		goto out;
	}
	
	plr_info("Verification Completed!\n");

	/* -----------------------------------------------
	* start write test
	*-------------------------------------------------
	*/
	
	send_token(PLRTOKEN_WRITE_START, NULL);
	plr_info("Writing Start!\n");

	ret = plr_start_writing();
	
	if (send_write_result(ret)) {
		ret = -1;
		goto out;
	}

	if (g_plr_state.b_finish_condition)
		goto finish;

	if (ret == -(INTERNAL_POWEROFF_FLAG)) {
		mdelay(1000);
		g_require_powerloss = FALSE;

        plr_info("write internal_power_control Call\n");
		if (dd_internal_power_control(TRUE)) {
			printf ("Internal power Control failed!\n");
			send_token(PLRTOKEN_ENV_ERROR, NULL);
			ret = -1;
			goto out;
		}
	}

	goto repeat;

finish:
	send_token(PLRTOKEN_TEST_FINISH, NULL);
	plr_info_highlight("\nTest Finish!\n");
	ret = -1;
out:
//    wake_unlock(&plr_wake_lock);
	return ret;
}
