/*
 * Copyright (c) ElixirFlash Technology
 *
 * plrtest command for U-boot
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "plr_common.h"

extern int plr_prepare(char *test_name, bool bdirty_fill, unsigned int test_sectors);

int main(int argc, char *argv[])
{
	uint test_sectors = 0;
        int dirty_fill = 0;
	char *test_name = NULL;

	if (argc >1) {
		if (!memcmp(argv[1], "full", 4)) {
                        //test_name = strtoud(argv[2]);
                        dirty_fill = strtoud(argv[3]);
                        test_sectors = strtoud(argv[4]);
                } else if (!memcmp(argv[1], "part", 4)) {
                       // test_name = strtoud(argv[2]);
                        dirty_fill = strtoud(argv[3]);
                        test_sectors = strtoud(argv[4]);
                } else 
                        goto help;
                plr_prepare(test_name, dirty_fill, test_sectors);
	} else
                goto help;
	
	return 0;

help:
        if (argc < 3 ) 
                printf("plrprepare full [dirty_fill] [sector count]\n"
                       "plrprepare part [dirty_fill] [sector count]");
        
        return -1;
}

