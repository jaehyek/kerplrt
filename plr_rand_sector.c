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
 
#include "plr_type.h"

static unsigned int state[16];

static unsigned int index;

void well512_seed(unsigned int nSeed)
{
	int i = 0;
	unsigned int s = nSeed;
	index = 0;
		
    for (i = 0; i < 16; i++)
    {
        state[i] = s;
		s += s + 73;
    }
	return;
}

unsigned int well512_rand(void)
{
    unsigned int a, b, c, d;
	
    a = state[index];
    c = state[(index + 13) & 15];
    b = a ^ c ^ (a << 16) ^ (c << 15);
    c = state[(index + 9) & 15];
    c ^= (c >> 11);
    a = state[index] = b ^ c;
    d = a ^ ((a << 5) & 0xda442d24U);
    index = (index + 15) & 15;
    a = state[index];
    state[index] = a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28);

    return state[index];
}

static unsigned int state2[16];

static uint index2;

void well512_seed2(unsigned int nSeed)
{
	int i = 0;
	unsigned int s = nSeed;
	index2 = 0;
		
    for (i = 0; i < 16; i++)
    {
        state2[i] = s;
		s += s + 73;
    }
	return;
}

unsigned int well512_rand2(void)
{
    unsigned int a, b, c, d;
	
    a = state2[index2];
    c = state2[(index2 + 13) & 15];
    b = a ^ c ^ (a << 16) ^ (c << 15);
    c = state2[(index2 + 9) & 15];
    c ^= (c >> 11);
    a = state2[index2] = b ^ c;
    d = a ^ ((a << 5) & 0xda442d24U);
    index2 = (index2 + 15) & 15;
    a = state2[index2];
    state2[index2] = a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28);

    return state2[index2];
}

