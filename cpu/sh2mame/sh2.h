/*****************************************************************************
 *
 *   sh2.h
 *   Portable Hitachi SH-2 (SH7600 family) emulator interface
 *
 *   Copyright Juergen Buchmueller <pullmoll@t-online.de>,
 *   all rights reserved.
 *
 *   - This source code is released as freeware for non-commercial purposes.
 *   - You are free to use and redistribute this code in modified or
 *     unmodified form, provided you list me in the credits.
 *   - If you modify this source code, you must add a notice to each modified
 *     source file that it has been changed.  If you're a nice person, you
 *     will clearly mark each change too.  :)
 *   - If you wish to use this for commercial purposes, please contact me at
 *     pullmoll@t-online.de
 *   - The author of this copywritten work reserves the right to change the
 *     terms of its usage and license at any time, including retroactively
 *   - This entire notice must remain in the source code.
 *
 *  This work is based on <tiraniddo@hotmail.com> C/C++ implementation of
 *  the SH-2 CPU core and was heavily changed to the MAME CPU requirements.
 *  Thanks also go to Chuck Mason <chukjr@sundail.net> and Olivier Galibert
 *  <galibert@pobox.com> for letting me peek into their SEMU code :-)
 *
 *****************************************************************************/

#pragma once

#ifndef __SH2_H__
#define __SH2_H__

typedef struct
{
	UINT32	r[16];
	UINT32	ppc;
	UINT32	pc;
	UINT32	pr;
	UINT32	sr;
	UINT32	gbr, vbr;
	UINT32	mach, macl;

	UINT32	ea;
	UINT32	delay;
	UINT32	test_irq;

	int	pending_irq;
	void	(*irq_callback)(int level);

	// XXX: unused, will we ever use?
	int   internal_irq_level;
	int   is_slave;
} SH2;

extern int sh2_icount;

void sh2_init(SH2 *sh2);
void sh2_reset(SH2 *sh2);
int sh2_execute(SH2 *sh2_, int cycles);
void sh2_irl_irq(SH2 *sh2, int level);

#endif /* __SH2_H__ */
