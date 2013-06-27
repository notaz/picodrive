/*
 * PicoDrive
 * Copyright (C) 2009,2010 notaz
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */
#include <stdio.h>

#include <pico/pico.h>
#include "cmn.h"

u8 __attribute__((aligned(4096))) tcache[DRC_TCACHE_SIZE];


void drc_cmn_init(void)
{
	plat_mem_set_exec(tcache, sizeof(tcache));
}

void drc_cmn_cleanup(void)
{
}
