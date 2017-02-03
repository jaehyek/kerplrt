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

#ifndef _PLR_TFTP_H_
#define _PLR_TFTP_H_

#define TFTP_BUF_SZ EXTRA_BUF_SZ

struct tftp_state {
	uint option;
	uint state;
	char client_ip[16];
	char server_ip[16];
	char gateway_ip[16];
	char netmask[16];
	char mac_addr[18];
	char file_path[128];
};

void tftp_fw_upgrade(void);
int tftp_power_monitor_upload(uchar *addr, uint len, uint req_length, uint type, bool is_dirty, uint index);
int tftp_erase_count_upload(uchar *addr);

#endif
