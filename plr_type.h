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


// 
// Note: This code is most readable while TAB size as 4.
//

#ifndef _PLR_TYPE_H_
#define _PLR_TYPE_H
#include <sys/types.h>

#ifndef lbaint_t
#define lbaint_t    unsigned long
#endif

#ifndef ushort
#define ushort  unsigned short
#endif

#ifndef uint
#define uint    unsigned int
#endif

#ifndef uchar
#define uchar   unsigned char
#endif

#ifndef ulong
#define ulong   unsigned long
#endif

#ifndef bool
#define bool    unsigned char
#endif

#ifndef u8
#define u8      unsigned char
#endif

#ifndef uint8_t
#define uint8_t u8
#endif

#ifndef u32
#define u32     unsigned int
#endif

#ifndef u64
#define u64     unsigned long long int
#endif

#ifndef uint64_t
#define uint64_t u64
#endif

#ifndef s8
#define s8      signed char
#endif

#ifndef s32
#define s32     signed int
#endif

#ifndef s64
#define s64     signed long long int
#endif

//
// Limits
//

#define PLR_INT_MAX				0x7FFFFFFFL				//@sj 150702
#define PLR_INT_MIN				0x80000000L				//@sj 150702
#define PLR_LONG_MAX			0x7FFFFFFFL				//@sj 150702
#define PLR_LONG_MIN			0x80000000L				//@sj 150702
#define PLR_LONGLONG_MAX		0x7FFFFFFFFFFFFFFFLL	//@sj 150702
#define PLR_LONGLONG_MIN		0x8000000000000000LL	//@sj 150702

#define PLR_INIT_TB_ZERO		0


#undef FALSE
#define FALSE	0

#undef TRUE
#define TRUE	(!FALSE)

#ifndef NULL
	#define NULL ((void*)0)
#endif

#endif
