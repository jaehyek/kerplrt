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

#ifndef _PLR_PROTOCOL_H_
#define _PLR_PROTOCOL_H_

#define PLRTOKEN_BUFFER_SIZE	256

typedef enum  _e_send_token_{
	PLRTOKEN_TEST_CASE,
	PLRTOKEN_PREPARE_DONE,
	PLRTOKEN_TEST_INFO,
	PLRTOKEN_VERI_START,
	
	PLRTOKEN_RECV_WRITE_INFO,
	PLRTOKEN_RECV_RANDOM_SEED,
	
	PLRTOKEN_CRASH_LOG_START,
	PLRTOKEN_CRASH_LOG_END,
	PLRTOKEN_WRITE_START,

	PLRTOKEN_SEND_WRITE_INFO,
	PLRTOKEN_SEND_RANDOM_SEED,
	
	PLRTOKEN_INIT_INFO,
	PLRTOKEN_POWERLOSS_CONFIG,
	PLRTOKEN_WRITE_CONFIG,
	PLRTOKEN_INIT_START,
	PLRTOKEN_RESET,
	PLRTOKEN_ENV_ERROR,
	PLRTOKEN_BOOT_DONE,
	PLRTOKEN_BOOT_CNT,
	PLRTOKEN_VERI_DONE,
	PLRTOKEN_WRITE_DONE,
	PLRTOKEN_INIT_DONE,
	PLRTOKEN_SET_LOOPCNT, 
	PLRTOKEN_SET_ADDRESS,
	PLRTOKEN_SET_PLOFF_INFO,
	PLRTOKEN_SET_STATISTICS,
	PLRTOKEN_TEST_FINISH,
#ifdef PLR_TFTP_FW_UP
	PLRTOKEN_UTFTP_STATE,
	PLRTOKEN_UTFTP_SETTING,
	PLRTOKEN_UTFTP_APPLITED,
	PLRTOKEN_UFILE_PATH,
	PLRTOKEN_UDNLOAD_START,
	PLRTOKEN_UDNLOAD_DONE,
	PLRTOKEN_KTFTP_SETTING,
	PLRTOKEN_KFILE_PATH,
	PLRTOKEN_KDNLOAD_START,
	PLRTOKEN_KDNLOAD_DONE,
#endif
	PLRTOKEN_UUPLOAD_START,
	PLRTOKEN_UUPLOAD_DONE,
	PLRTOKEN_EUPLOAD_START,
	PLRTOKEN_EUPLOAD_DONE,
	PLRTOKEN_WAIT_EXT_SW_RESET,
	MAX_SEND_TOKEN
} e_send_token;

// 2013.08.21 edward	add receive tokens (success, fail)
typedef enum  _e_receive_token_{
	PLRACK_SUCCESS,
	PLRACK_FAIL,
	PLRACK_RESET,
	MAX_RECIEVE_TOKEN
} e_receive_token;

int wait_getc(char c);
int send_token( e_send_token token, char *param );
int send_token_param( e_send_token token, uint param );

#endif
