/*
 * termios2.h
 *
 *       Created on:  Jul 06, 2023
 *  Last Updated on:  Jul 06, 2023
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2023 Fazecast, Inc.
 *
 * This file is part of jSerialComm.
 *
 * jSerialComm is free software: you can redistribute it and/or modify
 * it under the terms of either the Apache Software License, version 2, or
 * the GNU Lesser General Public License as published by the Free Software
 * Foundation, version 3 or above.
 *
 * jSerialComm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License and the Apache Software License along with jSerialComm. If not,
 * see <http://www.gnu.org/licenses/> and <http://www.apache.org/licenses/>.
 */

#ifndef __TERMIOS2_HEADER_H__
#define __TERMIOS2_HEADER_H__

#include <termios.h>

#define IBAUD0 020000000000

#if defined (__powerpc__) || defined (__powerpc64__)

#define T2CSANOW _IOW('t', 20, struct termios2)
#define BOTHER 00037
#define IBSHIFT 16
#define K_NCCS 19
struct termios2 {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[K_NCCS];
	cc_t c_line;
	speed_t c_ispeed;
	speed_t c_ospeed;
};


#elif defined (__mips__)

#define T2CSANOW TCSETS2
#define BOTHER CBAUDEX
#define IBSHIFT 16
#define K_NCCS 23
struct termios2 {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[K_NCCS];
	speed_t c_ispeed;
	speed_t c_ospeed;
};


#else

#define T2CSANOW TCSETS2
#define BOTHER CBAUDEX
#define IBSHIFT 16
#define K_NCCS 19
struct termios2 {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[K_NCCS];
	speed_t c_ispeed;
	speed_t c_ospeed;
};

#endif

#endif  // #ifndef __TERMIOS2_HEADER_H__
