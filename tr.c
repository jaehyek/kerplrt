/*
 * Copyright (c) ElixirFlash Technology
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "plr_common.h"

extern int test_report(char *args[], uint argc);
extern int prepare_mmc(void);
extern int prepare_other_devices(void);
extern int prepare_buffers(void);

int main(int argc, char *argv[])
{
	prepare_mmc();
	prepare_other_devices();
	prepare_buffers();
	return test_report(argv, argc);	
}
