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
#include "plr_tftp.h"
#include "plr_protocol.h"
 
#define MAX_UPLOAD_RETRY_COUNT	5

struct tftp_state g_tftp_state;

void tftp_fw_upgrade(void)
{
#ifdef PLR_TFTP_FW_UP
	int ret = 0;
	char cmd[PLRTOKEN_BUFFER_SIZE] = {0,};
	
	do_dcache_disable();
	send_token(PLRTOKEN_UTFTP_STATE, NULL);

	switch(g_tftp_state.state){
		case 0:
			send_token(PLRTOKEN_UTFTP_SETTING, NULL);
			
			sprintf(cmd,
				"setenv ipaddr %s; setenv serverip %s; setenv gatewayip %s; setenv netmask %s; setenv ethaddr %s; setenv tftptimeout 10000; saveenv",
				g_tftp_state.client_ip, g_tftp_state.server_ip, g_tftp_state.gateway_ip, g_tftp_state.netmask, g_tftp_state.mac_addr);
			
			ret = run_command(cmd, 0);
			if(ret == -1) {
			 	plr_err("tftp configuration failed\n");
				send_token(PLRTOKEN_ENV_ERROR, NULL);
				ret = run_command("reset", 0);
				do_dcache_enable();
				return;
			}
			
			send_token(PLRTOKEN_UTFTP_APPLITED, NULL);
			
			ret = run_command("reset", 0);
			break;

		case 1:
			send_token(PLRTOKEN_UFILE_PATH, NULL);
			send_token(PLRTOKEN_UDNLOAD_START, NULL);
			
			sprintf(cmd, "tftpboot 41000000 %s", g_tftp_state.file_path);
			
			ret = run_command(cmd, 0);
			if(ret == -1) {
			 	plr_err("tftp client execution failed\n");
				send_token_param(PLRTOKEN_UDNLOAD_DONE, 1);
				ret = run_command("reset", 0);
				do_dcache_enable();
				return;
			}

			ret = run_command("movi write u-boot 0 41000000", 0);

			// plr_debug("send token : %d\n", PLRTOKEN_UDNLOAD_DONE);
			send_token_param(PLRTOKEN_UDNLOAD_DONE, 0);

			if(g_tftp_state.option == 0)
				ret = run_command("reset", 0);

			break;
	}
	do_dcache_enable();
#endif	// PLR_TFTP_FW_UP
}

 int tftp_power_monitor_upload(uchar *addr, uint len, uint req_length, uint type, bool is_dirty, uint index)
 {
	 int ret = 0;
	 char buf[PLRTOKEN_BUFFER_SIZE] = {0,};
	 char run_command_buf[PLRTOKEN_BUFFER_SIZE] = {0,};
	 int retry = 0;
	 
	 do_dcache_disable();
	 while(1)
	 {
 
		 sprintf(buf, "%d/%d/%d/%d/", type, req_length, is_dirty, index);
		 send_token(PLRTOKEN_UUPLOAD_START, buf);
		 mdelay(100);
 
		 sprintf(run_command_buf, "tftpput 0x%X 0x%X %d", (uint)addr, len, index);
 
		 if(run_command(run_command_buf, 0))
		 {
			 send_token(PLRTOKEN_UUPLOAD_START, buf);
			 if(++retry > MAX_UPLOAD_RETRY_COUNT)
			 {
				 send_token_param(PLRTOKEN_UUPLOAD_DONE, 1);
				 ret = 1;
				 break;
			 }
			 
			 continue;
		 }
 
		 mdelay(100);
		 send_token_param(PLRTOKEN_UUPLOAD_DONE, 0);
 
		 // plrtoken uupload_done 의 ack 는 g_tftp_state.option 에 저장됨
		 // ack 가 0 이면 성공, 0 이외의 값은 실패
		 if(!g_tftp_state.option)	 break;
		 else
		 {
			 if(++retry > MAX_UPLOAD_RETRY_COUNT)
			 {
				 send_token(PLRTOKEN_ENV_ERROR, NULL);
				 ret = 1;
				 break;
			 }
		 }
	 }
	 do_dcache_enable();
	 
	 return ret;
 }
 
int tftp_erase_count_upload(uchar *addr)
{
	int ret = 0;
	char run_command_buf[256] = {0,};
	int cnt_page = 0;
	int retry = 0;


	do_dcache_disable();
	plr_info("Checking erase count...\n");

	ret = get_erase_count();
	cnt_page = *addr;

	if (ret) {
		send_token(PLRTOKEN_ENV_ERROR, NULL);
		do_dcache_enable();
		return -1;
	}

	while(1) {
		send_token(PLRTOKEN_EUPLOAD_START, NULL);
		mdelay(100);

		sprintf(run_command_buf, "tftpput 0x%X 0x%X %d", (uint)addr, (cnt_page + 1) * 512, 1);

		ret = run_command((const char *)run_command_buf, 0);

		if (ret) {
			send_token(PLRTOKEN_EUPLOAD_START, NULL);
			
			if(++retry > MAX_UPLOAD_RETRY_COUNT) {
				send_token_param(PLRTOKEN_EUPLOAD_DONE, 1);
				ret = 1;
				break;
			}

			continue;
		}

		mdelay(100);
		send_token_param(PLRTOKEN_EUPLOAD_DONE, 0);

		// plrtoken uupload_done 의 ack 는 g_tftp_state.option 에 저장됨
		// ack 가 0 이면 성공, 0 이외의 값은 실패
		if(!g_tftp_state.option) {
			break;
		}
		else
		{
			if(++retry > MAX_UPLOAD_RETRY_COUNT) {
				send_token(PLRTOKEN_ENV_ERROR, NULL);
				ret = 1;
				break;
			}
		}
	}
	do_dcache_enable();
	return ret;
}
 
