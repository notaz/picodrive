#include "driver.h"

/* Multi-Z80 32 Bit emulator */

/* Copyright 1996-2000 Neil Bradley, All rights reserved
 *
 * License agreement:
 *
 * (MZ80 Refers to both the assembly code emitted by makeZ80.c and makeZ80.c
 * itself)
 *
 * MZ80 May be distributed in unmodified form to any medium.
 *
 * MZ80 May not be sold, or sold as a part of a commercial package without
 * the express written permission of Neil Bradley (neil@synthcom.com). This
 * includes shareware.
 *
 * Modified versions of MZ80 may not be publicly redistributed without author
 * approval (neil@synthcom.com). This includes distributing via a publicly
 * accessible LAN. You may make your own source modifications and distribute
 * MZ80 in source or object form, but if you make modifications to MZ80
 * then it should be noted in the top as a comment in makeZ80.c.
 *
 * MZ80 Licensing for commercial applications is available. Please email
 * neil@synthcom.com for details.
 *
 * Synthcom Systems, Inc, and Neil Bradley will not be held responsible for
 * any damage done by the use of MZ80. It is purely "as-is".
 *
 * If you use MZ80 in a freeware application, credit in the following text:
 *
 * "Multi-Z80 CPU emulator by Neil Bradley (neil@synthcom.com)"
 *
 * must accompany the freeware application within the application itself or
 * in the documentation.
 *
 * Legal stuff aside:
 *
 * If you find problems with MZ80, please email the author so they can get
 * resolved. If you find a bug and fix it, please also email the author so
 * that those bug fixes can be propogated to the installed base of MZ80
 * users. If you find performance improvements or problems with MZ80, please
 * email the author with your changes/suggestions and they will be rolled in
 * with subsequent releases of MZ80.
 *
 * The whole idea of this emulator is to have the fastest available 32 bit
 * Multi-Z80 emulator for the PC, giving maximum performance. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mz80.h"
UINT32 z80intAddr;
UINT32 z80pc;


/* Modular global variables go here*/

static CONTEXTMZ80 cpu;	/* CPU Context */
static UINT8 *pbPC;			/* Program counter normalized */
static UINT8 *pbSP;			/* Stack pointer normalized */
static struct MemoryReadByte *psMemRead; /* Read memory structure */
static struct MemoryWriteByte *psMemWrite; /* Write memory structure */
static struct z80PortRead *psIoRead; /* Read I/O structure */
static struct z80PortWrite *psIoWrite; /* Write memory structure */
static INT32 sdwCyclesRemaining; /* Used as a countdown */
static UINT32 dwReturnCode; /* Return code from exec() */
static UINT32 dwOriginalCycles; /* How many cycles did we start with? */
static UINT32 dwElapsedTicks;	/* How many ticks did we elapse? */
static INT32 sdwAddr;		/* Temporary address storage */
static UINT32 dwAddr;		/* Temporary stack address */
static UINT8 *pbAddAdcTable;	/* Pointer to add/adc flag table */
static UINT8 *pbSubSbcTable;	/* Pointer to sub/sbc flag table */
static UINT32 dwTemp;			/* Temporary value */

static UINT8 bTemp;			/* Temporary value */

static UINT8 bTemp2; 		/* Temporary value */

/* Precomputed flag tables */

static UINT8 bPostIncFlags[0x100] = 
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x94,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x90,
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x50
};

static UINT8 bPostDecFlags[0x100] = 
{
	0x92,0x42,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x12,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x16,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,
	0x92,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x82
};

static UINT8 bPostORFlags[0x100] = 
{
	0x44,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,
	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,
	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,
	0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,
	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,
	0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,
	0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,
	0x00,0x04,0x04,0x00,0x04,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x04,0x04,0x00,
	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,
	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,
	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,
	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,
	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,
	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,
	0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,
	0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84
};

static UINT8 bPostANDFlags[0x100] = 
{
	0x54,0x10,0x10,0x14,0x10,0x14,0x14,0x10,0x10,0x14,0x14,0x10,0x14,0x10,0x10,0x14,
	0x10,0x14,0x14,0x10,0x14,0x10,0x10,0x14,0x14,0x10,0x10,0x14,0x10,0x14,0x14,0x10,
	0x10,0x14,0x14,0x10,0x14,0x10,0x10,0x14,0x14,0x10,0x10,0x14,0x10,0x14,0x14,0x10,
	0x14,0x10,0x10,0x14,0x10,0x14,0x14,0x10,0x10,0x14,0x14,0x10,0x14,0x10,0x10,0x14,
	0x10,0x14,0x14,0x10,0x14,0x10,0x10,0x14,0x14,0x10,0x10,0x14,0x10,0x14,0x14,0x10,
	0x14,0x10,0x10,0x14,0x10,0x14,0x14,0x10,0x10,0x14,0x14,0x10,0x14,0x10,0x10,0x14,
	0x14,0x10,0x10,0x14,0x10,0x14,0x14,0x10,0x10,0x14,0x14,0x10,0x14,0x10,0x10,0x14,
	0x10,0x14,0x14,0x10,0x14,0x10,0x10,0x14,0x14,0x10,0x10,0x14,0x10,0x14,0x14,0x10,
	0x90,0x94,0x94,0x90,0x94,0x90,0x90,0x94,0x94,0x90,0x90,0x94,0x90,0x94,0x94,0x90,
	0x94,0x90,0x90,0x94,0x90,0x94,0x94,0x90,0x90,0x94,0x94,0x90,0x94,0x90,0x90,0x94,
	0x94,0x90,0x90,0x94,0x90,0x94,0x94,0x90,0x90,0x94,0x94,0x90,0x94,0x90,0x90,0x94,
	0x90,0x94,0x94,0x90,0x94,0x90,0x90,0x94,0x94,0x90,0x90,0x94,0x90,0x94,0x94,0x90,
	0x94,0x90,0x90,0x94,0x90,0x94,0x94,0x90,0x90,0x94,0x94,0x90,0x94,0x90,0x90,0x94,
	0x90,0x94,0x94,0x90,0x94,0x90,0x90,0x94,0x94,0x90,0x90,0x94,0x90,0x94,0x94,0x90,
	0x90,0x94,0x94,0x90,0x94,0x90,0x90,0x94,0x94,0x90,0x90,0x94,0x90,0x94,0x94,0x90,
	0x94,0x90,0x90,0x94,0x90,0x94,0x94,0x90,0x90,0x94,0x94,0x90,0x94,0x90,0x90,0x94
};

static UINT16 wDAATable[0x800] = 
{
	0x5400,0x1001,0x1002,0x1403,0x1004,0x1405,0x1406,0x1007,
	0x1008,0x1409,0x1010,0x1411,0x1412,0x1013,0x1414,0x1015,
	0x1010,0x1411,0x1412,0x1013,0x1414,0x1015,0x1016,0x1417,
	0x1418,0x1019,0x1020,0x1421,0x1422,0x1023,0x1424,0x1025,
	0x1020,0x1421,0x1422,0x1023,0x1424,0x1025,0x1026,0x1427,
	0x1428,0x1029,0x1430,0x1031,0x1032,0x1433,0x1034,0x1435,
	0x1430,0x1031,0x1032,0x1433,0x1034,0x1435,0x1436,0x1037,
	0x1038,0x1439,0x1040,0x1441,0x1442,0x1043,0x1444,0x1045,
	0x1040,0x1441,0x1442,0x1043,0x1444,0x1045,0x1046,0x1447,
	0x1448,0x1049,0x1450,0x1051,0x1052,0x1453,0x1054,0x1455,
	0x1450,0x1051,0x1052,0x1453,0x1054,0x1455,0x1456,0x1057,
	0x1058,0x1459,0x1460,0x1061,0x1062,0x1463,0x1064,0x1465,
	0x1460,0x1061,0x1062,0x1463,0x1064,0x1465,0x1466,0x1067,
	0x1068,0x1469,0x1070,0x1471,0x1472,0x1073,0x1474,0x1075,
	0x1070,0x1471,0x1472,0x1073,0x1474,0x1075,0x1076,0x1477,
	0x1478,0x1079,0x9080,0x9481,0x9482,0x9083,0x9484,0x9085,
	0x9080,0x9481,0x9482,0x9083,0x9484,0x9085,0x9086,0x9487,
	0x9488,0x9089,0x9490,0x9091,0x9092,0x9493,0x9094,0x9495,
	0x9490,0x9091,0x9092,0x9493,0x9094,0x9495,0x9496,0x9097,
	0x9098,0x9499,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,0x1506,0x1107,
	0x1108,0x1509,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,0x1116,0x1517,
	0x1518,0x1119,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,0x1126,0x1527,
	0x1528,0x1129,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,0x1536,0x1137,
	0x1138,0x1539,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,0x1146,0x1547,
	0x1548,0x1149,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,0x1556,0x1157,
	0x1158,0x1559,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,0x1566,0x1167,
	0x1168,0x1569,0x1170,0x1571,0x1572,0x1173,0x1574,0x1175,
	0x1170,0x1571,0x1572,0x1173,0x1574,0x1175,0x1176,0x1577,
	0x1578,0x1179,0x9180,0x9581,0x9582,0x9183,0x9584,0x9185,
	0x9180,0x9581,0x9582,0x9183,0x9584,0x9185,0x9186,0x9587,
	0x9588,0x9189,0x9590,0x9191,0x9192,0x9593,0x9194,0x9595,
	0x9590,0x9191,0x9192,0x9593,0x9194,0x9595,0x9596,0x9197,
	0x9198,0x9599,0x95a0,0x91a1,0x91a2,0x95a3,0x91a4,0x95a5,
	0x95a0,0x91a1,0x91a2,0x95a3,0x91a4,0x95a5,0x95a6,0x91a7,
	0x91a8,0x95a9,0x91b0,0x95b1,0x95b2,0x91b3,0x95b4,0x91b5,
	0x91b0,0x95b1,0x95b2,0x91b3,0x95b4,0x91b5,0x91b6,0x95b7,
	0x95b8,0x91b9,0x95c0,0x91c1,0x91c2,0x95c3,0x91c4,0x95c5,
	0x95c0,0x91c1,0x91c2,0x95c3,0x91c4,0x95c5,0x95c6,0x91c7,
	0x91c8,0x95c9,0x91d0,0x95d1,0x95d2,0x91d3,0x95d4,0x91d5,
	0x91d0,0x95d1,0x95d2,0x91d3,0x95d4,0x91d5,0x91d6,0x95d7,
	0x95d8,0x91d9,0x91e0,0x95e1,0x95e2,0x91e3,0x95e4,0x91e5,
	0x91e0,0x95e1,0x95e2,0x91e3,0x95e4,0x91e5,0x91e6,0x95e7,
	0x95e8,0x91e9,0x95f0,0x91f1,0x91f2,0x95f3,0x91f4,0x95f5,
	0x95f0,0x91f1,0x91f2,0x95f3,0x91f4,0x95f5,0x95f6,0x91f7,
	0x91f8,0x95f9,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,0x1506,0x1107,
	0x1108,0x1509,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,0x1116,0x1517,
	0x1518,0x1119,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,0x1126,0x1527,
	0x1528,0x1129,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,0x1536,0x1137,
	0x1138,0x1539,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,0x1146,0x1547,
	0x1548,0x1149,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,0x1556,0x1157,
	0x1158,0x1559,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x1406,0x1007,0x1008,0x1409,0x140a,0x100b,0x140c,0x100d,
	0x100e,0x140f,0x1010,0x1411,0x1412,0x1013,0x1414,0x1015,
	0x1016,0x1417,0x1418,0x1019,0x101a,0x141b,0x101c,0x141d,
	0x141e,0x101f,0x1020,0x1421,0x1422,0x1023,0x1424,0x1025,
	0x1026,0x1427,0x1428,0x1029,0x102a,0x142b,0x102c,0x142d,
	0x142e,0x102f,0x1430,0x1031,0x1032,0x1433,0x1034,0x1435,
	0x1436,0x1037,0x1038,0x1439,0x143a,0x103b,0x143c,0x103d,
	0x103e,0x143f,0x1040,0x1441,0x1442,0x1043,0x1444,0x1045,
	0x1046,0x1447,0x1448,0x1049,0x104a,0x144b,0x104c,0x144d,
	0x144e,0x104f,0x1450,0x1051,0x1052,0x1453,0x1054,0x1455,
	0x1456,0x1057,0x1058,0x1459,0x145a,0x105b,0x145c,0x105d,
	0x105e,0x145f,0x1460,0x1061,0x1062,0x1463,0x1064,0x1465,
	0x1466,0x1067,0x1068,0x1469,0x146a,0x106b,0x146c,0x106d,
	0x106e,0x146f,0x1070,0x1471,0x1472,0x1073,0x1474,0x1075,
	0x1076,0x1477,0x1478,0x1079,0x107a,0x147b,0x107c,0x147d,
	0x147e,0x107f,0x9080,0x9481,0x9482,0x9083,0x9484,0x9085,
	0x9086,0x9487,0x9488,0x9089,0x908a,0x948b,0x908c,0x948d,
	0x948e,0x908f,0x9490,0x9091,0x9092,0x9493,0x9094,0x9495,
	0x9496,0x9097,0x9098,0x9499,0x949a,0x909b,0x949c,0x909d,
	0x909e,0x949f,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x1506,0x1107,0x1108,0x1509,0x150a,0x110b,0x150c,0x110d,
	0x110e,0x150f,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1116,0x1517,0x1518,0x1119,0x111a,0x151b,0x111c,0x151d,
	0x151e,0x111f,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1126,0x1527,0x1528,0x1129,0x112a,0x152b,0x112c,0x152d,
	0x152e,0x112f,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1536,0x1137,0x1138,0x1539,0x153a,0x113b,0x153c,0x113d,
	0x113e,0x153f,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1146,0x1547,0x1548,0x1149,0x114a,0x154b,0x114c,0x154d,
	0x154e,0x114f,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1556,0x1157,0x1158,0x1559,0x155a,0x115b,0x155c,0x115d,
	0x115e,0x155f,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x1566,0x1167,0x1168,0x1569,0x156a,0x116b,0x156c,0x116d,
	0x116e,0x156f,0x1170,0x1571,0x1572,0x1173,0x1574,0x1175,
	0x1176,0x1577,0x1578,0x1179,0x117a,0x157b,0x117c,0x157d,
	0x157e,0x117f,0x9180,0x9581,0x9582,0x9183,0x9584,0x9185,
	0x9186,0x9587,0x9588,0x9189,0x918a,0x958b,0x918c,0x958d,
	0x958e,0x918f,0x9590,0x9191,0x9192,0x9593,0x9194,0x9595,
	0x9596,0x9197,0x9198,0x9599,0x959a,0x919b,0x959c,0x919d,
	0x919e,0x959f,0x95a0,0x91a1,0x91a2,0x95a3,0x91a4,0x95a5,
	0x95a6,0x91a7,0x91a8,0x95a9,0x95aa,0x91ab,0x95ac,0x91ad,
	0x91ae,0x95af,0x91b0,0x95b1,0x95b2,0x91b3,0x95b4,0x91b5,
	0x91b6,0x95b7,0x95b8,0x91b9,0x91ba,0x95bb,0x91bc,0x95bd,
	0x95be,0x91bf,0x95c0,0x91c1,0x91c2,0x95c3,0x91c4,0x95c5,
	0x95c6,0x91c7,0x91c8,0x95c9,0x95ca,0x91cb,0x95cc,0x91cd,
	0x91ce,0x95cf,0x91d0,0x95d1,0x95d2,0x91d3,0x95d4,0x91d5,
	0x91d6,0x95d7,0x95d8,0x91d9,0x91da,0x95db,0x91dc,0x95dd,
	0x95de,0x91df,0x91e0,0x95e1,0x95e2,0x91e3,0x95e4,0x91e5,
	0x91e6,0x95e7,0x95e8,0x91e9,0x91ea,0x95eb,0x91ec,0x95ed,
	0x95ee,0x91ef,0x95f0,0x91f1,0x91f2,0x95f3,0x91f4,0x95f5,
	0x95f6,0x91f7,0x91f8,0x95f9,0x95fa,0x91fb,0x95fc,0x91fd,
	0x91fe,0x95ff,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x1506,0x1107,0x1108,0x1509,0x150a,0x110b,0x150c,0x110d,
	0x110e,0x150f,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1116,0x1517,0x1518,0x1119,0x111a,0x151b,0x111c,0x151d,
	0x151e,0x111f,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1126,0x1527,0x1528,0x1129,0x112a,0x152b,0x112c,0x152d,
	0x152e,0x112f,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1536,0x1137,0x1138,0x1539,0x153a,0x113b,0x153c,0x113d,
	0x113e,0x153f,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1146,0x1547,0x1548,0x1149,0x114a,0x154b,0x114c,0x154d,
	0x154e,0x114f,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1556,0x1157,0x1158,0x1559,0x155a,0x115b,0x155c,0x115d,
	0x115e,0x155f,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x5600,0x1201,0x1202,0x1603,0x1204,0x1605,0x1606,0x1207,
	0x1208,0x1609,0x1204,0x1605,0x1606,0x1207,0x1208,0x1609,
	0x1210,0x1611,0x1612,0x1213,0x1614,0x1215,0x1216,0x1617,
	0x1618,0x1219,0x1614,0x1215,0x1216,0x1617,0x1618,0x1219,
	0x1220,0x1621,0x1622,0x1223,0x1624,0x1225,0x1226,0x1627,
	0x1628,0x1229,0x1624,0x1225,0x1226,0x1627,0x1628,0x1229,
	0x1630,0x1231,0x1232,0x1633,0x1234,0x1635,0x1636,0x1237,
	0x1238,0x1639,0x1234,0x1635,0x1636,0x1237,0x1238,0x1639,
	0x1240,0x1641,0x1642,0x1243,0x1644,0x1245,0x1246,0x1647,
	0x1648,0x1249,0x1644,0x1245,0x1246,0x1647,0x1648,0x1249,
	0x1650,0x1251,0x1252,0x1653,0x1254,0x1655,0x1656,0x1257,
	0x1258,0x1659,0x1254,0x1655,0x1656,0x1257,0x1258,0x1659,
	0x1660,0x1261,0x1262,0x1663,0x1264,0x1665,0x1666,0x1267,
	0x1268,0x1669,0x1264,0x1665,0x1666,0x1267,0x1268,0x1669,
	0x1270,0x1671,0x1672,0x1273,0x1674,0x1275,0x1276,0x1677,
	0x1678,0x1279,0x1674,0x1275,0x1276,0x1677,0x1678,0x1279,
	0x9280,0x9681,0x9682,0x9283,0x9684,0x9285,0x9286,0x9687,
	0x9688,0x9289,0x9684,0x9285,0x9286,0x9687,0x9688,0x9289,
	0x9690,0x9291,0x9292,0x9693,0x9294,0x9695,0x9696,0x9297,
	0x9298,0x9699,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x1340,0x1741,0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,
	0x1748,0x1349,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x1750,0x1351,0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,
	0x1358,0x1759,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x1760,0x1361,0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,
	0x1368,0x1769,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x1370,0x1771,0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,
	0x1778,0x1379,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x9380,0x9781,0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,
	0x9788,0x9389,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x9790,0x9391,0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,
	0x9398,0x9799,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,
	0x97a0,0x93a1,0x93a2,0x97a3,0x93a4,0x97a5,0x97a6,0x93a7,
	0x93a8,0x97a9,0x93a4,0x97a5,0x97a6,0x93a7,0x93a8,0x97a9,
	0x93b0,0x97b1,0x97b2,0x93b3,0x97b4,0x93b5,0x93b6,0x97b7,
	0x97b8,0x93b9,0x97b4,0x93b5,0x93b6,0x97b7,0x97b8,0x93b9,
	0x97c0,0x93c1,0x93c2,0x97c3,0x93c4,0x97c5,0x97c6,0x93c7,
	0x93c8,0x97c9,0x93c4,0x97c5,0x97c6,0x93c7,0x93c8,0x97c9,
	0x93d0,0x97d1,0x97d2,0x93d3,0x97d4,0x93d5,0x93d6,0x97d7,
	0x97d8,0x93d9,0x97d4,0x93d5,0x93d6,0x97d7,0x97d8,0x93d9,
	0x93e0,0x97e1,0x97e2,0x93e3,0x97e4,0x93e5,0x93e6,0x97e7,
	0x97e8,0x93e9,0x97e4,0x93e5,0x93e6,0x97e7,0x97e8,0x93e9,
	0x97f0,0x93f1,0x93f2,0x97f3,0x93f4,0x97f5,0x97f6,0x93f7,
	0x93f8,0x97f9,0x93f4,0x97f5,0x97f6,0x93f7,0x93f8,0x97f9,
	0x5700,0x1301,0x1302,0x1703,0x1304,0x1705,0x1706,0x1307,
	0x1308,0x1709,0x1304,0x1705,0x1706,0x1307,0x1308,0x1709,
	0x1310,0x1711,0x1712,0x1313,0x1714,0x1315,0x1316,0x1717,
	0x1718,0x1319,0x1714,0x1315,0x1316,0x1717,0x1718,0x1319,
	0x1320,0x1721,0x1722,0x1323,0x1724,0x1325,0x1326,0x1727,
	0x1728,0x1329,0x1724,0x1325,0x1326,0x1727,0x1728,0x1329,
	0x1730,0x1331,0x1332,0x1733,0x1334,0x1735,0x1736,0x1337,
	0x1338,0x1739,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x1340,0x1741,0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,
	0x1748,0x1349,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x1750,0x1351,0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,
	0x1358,0x1759,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x1760,0x1361,0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,
	0x1368,0x1769,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x1370,0x1771,0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,
	0x1778,0x1379,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x9380,0x9781,0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,
	0x9788,0x9389,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x9790,0x9391,0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,
	0x9398,0x9799,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,
	0x97fa,0x93fb,0x97fc,0x93fd,0x93fe,0x97ff,0x5600,0x1201,
	0x1202,0x1603,0x1204,0x1605,0x1606,0x1207,0x1208,0x1609,
	0x160a,0x120b,0x160c,0x120d,0x120e,0x160f,0x1210,0x1611,
	0x1612,0x1213,0x1614,0x1215,0x1216,0x1617,0x1618,0x1219,
	0x121a,0x161b,0x121c,0x161d,0x161e,0x121f,0x1220,0x1621,
	0x1622,0x1223,0x1624,0x1225,0x1226,0x1627,0x1628,0x1229,
	0x122a,0x162b,0x122c,0x162d,0x162e,0x122f,0x1630,0x1231,
	0x1232,0x1633,0x1234,0x1635,0x1636,0x1237,0x1238,0x1639,
	0x163a,0x123b,0x163c,0x123d,0x123e,0x163f,0x1240,0x1641,
	0x1642,0x1243,0x1644,0x1245,0x1246,0x1647,0x1648,0x1249,
	0x124a,0x164b,0x124c,0x164d,0x164e,0x124f,0x1650,0x1251,
	0x1252,0x1653,0x1254,0x1655,0x1656,0x1257,0x1258,0x1659,
	0x165a,0x125b,0x165c,0x125d,0x125e,0x165f,0x1660,0x1261,
	0x1262,0x1663,0x1264,0x1665,0x1666,0x1267,0x1268,0x1669,
	0x166a,0x126b,0x166c,0x126d,0x126e,0x166f,0x1270,0x1671,
	0x1672,0x1273,0x1674,0x1275,0x1276,0x1677,0x1678,0x1279,
	0x127a,0x167b,0x127c,0x167d,0x167e,0x127f,0x9280,0x9681,
	0x9682,0x9283,0x9684,0x9285,0x9286,0x9687,0x9688,0x9289,
	0x928a,0x968b,0x928c,0x968d,0x968e,0x928f,0x9690,0x9291,
	0x9292,0x9693,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x173a,0x133b,0x173c,0x133d,0x133e,0x173f,0x1340,0x1741,
	0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x134a,0x174b,0x134c,0x174d,0x174e,0x134f,0x1750,0x1351,
	0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x175a,0x135b,0x175c,0x135d,0x135e,0x175f,0x1760,0x1361,
	0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x176a,0x136b,0x176c,0x136d,0x136e,0x176f,0x1370,0x1771,
	0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x137a,0x177b,0x137c,0x177d,0x177e,0x137f,0x9380,0x9781,
	0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x938a,0x978b,0x938c,0x978d,0x978e,0x938f,0x9790,0x9391,
	0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,
	0x979a,0x939b,0x979c,0x939d,0x939e,0x979f,0x97a0,0x93a1,
	0x93a2,0x97a3,0x93a4,0x97a5,0x97a6,0x93a7,0x93a8,0x97a9,
	0x97aa,0x93ab,0x97ac,0x93ad,0x93ae,0x97af,0x93b0,0x97b1,
	0x97b2,0x93b3,0x97b4,0x93b5,0x93b6,0x97b7,0x97b8,0x93b9,
	0x93ba,0x97bb,0x93bc,0x97bd,0x97be,0x93bf,0x97c0,0x93c1,
	0x93c2,0x97c3,0x93c4,0x97c5,0x97c6,0x93c7,0x93c8,0x97c9,
	0x97ca,0x93cb,0x97cc,0x93cd,0x93ce,0x97cf,0x93d0,0x97d1,
	0x97d2,0x93d3,0x97d4,0x93d5,0x93d6,0x97d7,0x97d8,0x93d9,
	0x93da,0x97db,0x93dc,0x97dd,0x97de,0x93df,0x93e0,0x97e1,
	0x97e2,0x93e3,0x97e4,0x93e5,0x93e6,0x97e7,0x97e8,0x93e9,
	0x93ea,0x97eb,0x93ec,0x97ed,0x97ee,0x93ef,0x97f0,0x93f1,
	0x93f2,0x97f3,0x93f4,0x97f5,0x97f6,0x93f7,0x93f8,0x97f9,
	0x97fa,0x93fb,0x97fc,0x93fd,0x93fe,0x97ff,0x5700,0x1301,
	0x1302,0x1703,0x1304,0x1705,0x1706,0x1307,0x1308,0x1709,
	0x170a,0x130b,0x170c,0x130d,0x130e,0x170f,0x1310,0x1711,
	0x1712,0x1313,0x1714,0x1315,0x1316,0x1717,0x1718,0x1319,
	0x131a,0x171b,0x131c,0x171d,0x171e,0x131f,0x1320,0x1721,
	0x1722,0x1323,0x1724,0x1325,0x1326,0x1727,0x1728,0x1329,
	0x132a,0x172b,0x132c,0x172d,0x172e,0x132f,0x1730,0x1331,
	0x1332,0x1733,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x173a,0x133b,0x173c,0x133d,0x133e,0x173f,0x1340,0x1741,
	0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x134a,0x174b,0x134c,0x174d,0x174e,0x134f,0x1750,0x1351,
	0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x175a,0x135b,0x175c,0x135d,0x135e,0x175f,0x1760,0x1361,
	0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x176a,0x136b,0x176c,0x136d,0x136e,0x176f,0x1370,0x1771,
	0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x137a,0x177b,0x137c,0x177d,0x177e,0x137f,0x9380,0x9781,
	0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x938a,0x978b,0x938c,0x978d,0x978e,0x938f,0x9790,0x9391,
	0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799 
};

void DDFDCBHandler(UINT32 dwWhich);


static void InvalidInstruction(UINT32 dwCount)
{
	pbPC -= dwCount; /* Invalid instruction - back up */
	dwReturnCode = (UINT32) pbPC - (UINT32) cpu.z80Base;
	dwOriginalCycles -= sdwCyclesRemaining;
	sdwCyclesRemaining = 0;
}

void CBHandler(void)
{
	switch (*pbPC++)
	{
 		case 0x00:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				bTemp2 = (cpu.z80B >> 7);
				cpu.z80B = (cpu.z80B << 1) | bTemp2;
				cpu.z80F |= bTemp2 | bPostORFlags[cpu.z80B];
			break;
		}
 		case 0x01:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				bTemp2 = (cpu.z80C >> 7);
				cpu.z80C = (cpu.z80C << 1) | bTemp2;
				cpu.z80F |= bTemp2 | bPostORFlags[cpu.z80C];
			break;
		}
 		case 0x02:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				bTemp2 = (cpu.z80D >> 7);
				cpu.z80D = (cpu.z80D << 1) | bTemp2;
				cpu.z80F |= bTemp2 | bPostORFlags[cpu.z80D];
			break;
		}
 		case 0x03:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				bTemp2 = (cpu.z80E >> 7);
				cpu.z80E = (cpu.z80E << 1) | bTemp2;
				cpu.z80F |= bTemp2 | bPostORFlags[cpu.z80E];
			break;
		}
 		case 0x04:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				bTemp2 = (cpu.z80H >> 7);
				cpu.z80H = (cpu.z80H << 1) | bTemp2;
				cpu.z80F |= bTemp2 | bPostORFlags[cpu.z80H];
			break;
		}
 		case 0x05:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				bTemp2 = (cpu.z80L >> 7);
				cpu.z80L = (cpu.z80L << 1) | bTemp2;
				cpu.z80F |= bTemp2 | bPostORFlags[cpu.z80L];
			break;
		}
 		case 0x06:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				bTemp2 = (bTemp >> 7);
				bTemp = (bTemp << 1) | bTemp2;
				cpu.z80F |= bTemp2 | bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x07:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				bTemp2 = (cpu.z80A >> 7);
				cpu.z80A = (cpu.z80A << 1) | bTemp2;
				cpu.z80F |= bTemp2 | bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x08:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80B & Z80_FLAG_CARRY);
				cpu.z80B = (cpu.z80B >> 1) | (cpu.z80B << 7);
				cpu.z80F |= bPostORFlags[cpu.z80B];
			break;
		}
 		case 0x09:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80C & Z80_FLAG_CARRY);
				cpu.z80C = (cpu.z80C >> 1) | (cpu.z80C << 7);
				cpu.z80F |= bPostORFlags[cpu.z80C];
			break;
		}
 		case 0x0a:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80D & Z80_FLAG_CARRY);
				cpu.z80D = (cpu.z80D >> 1) | (cpu.z80D << 7);
				cpu.z80F |= bPostORFlags[cpu.z80D];
			break;
		}
 		case 0x0b:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80E & Z80_FLAG_CARRY);
				cpu.z80E = (cpu.z80E >> 1) | (cpu.z80E << 7);
				cpu.z80F |= bPostORFlags[cpu.z80E];
			break;
		}
 		case 0x0c:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80H & Z80_FLAG_CARRY);
				cpu.z80H = (cpu.z80H >> 1) | (cpu.z80H << 7);
				cpu.z80F |= bPostORFlags[cpu.z80H];
			break;
		}
 		case 0x0d:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80L & Z80_FLAG_CARRY);
				cpu.z80L = (cpu.z80L >> 1) | (cpu.z80L << 7);
				cpu.z80F |= bPostORFlags[cpu.z80L];
			break;
		}
 		case 0x0e:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp & Z80_FLAG_CARRY);
				bTemp = (bTemp >> 1) | (bTemp << 7);
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x0f:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80A & Z80_FLAG_CARRY);
				cpu.z80A = (cpu.z80A >> 1) | (cpu.z80A << 7);
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x10:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = cpu.z80F & Z80_FLAG_CARRY;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80B >> 7);
				cpu.z80B = (cpu.z80B << 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80B];
			break;
		}
 		case 0x11:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = cpu.z80F & Z80_FLAG_CARRY;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80C >> 7);
				cpu.z80C = (cpu.z80C << 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80C];
			break;
		}
 		case 0x12:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = cpu.z80F & Z80_FLAG_CARRY;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80D >> 7);
				cpu.z80D = (cpu.z80D << 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80D];
			break;
		}
 		case 0x13:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = cpu.z80F & Z80_FLAG_CARRY;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80E >> 7);
				cpu.z80E = (cpu.z80E << 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80E];
			break;
		}
 		case 0x14:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = cpu.z80F & Z80_FLAG_CARRY;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80H >> 7);
				cpu.z80H = (cpu.z80H << 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80H];
			break;
		}
 		case 0x15:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = cpu.z80F & Z80_FLAG_CARRY;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80L >> 7);
				cpu.z80L = (cpu.z80L << 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80L];
			break;
		}
 		case 0x16:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp2 = cpu.z80F & Z80_FLAG_CARRY;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp >> 7);
				bTemp = (bTemp << 1) | bTemp2;
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x17:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = cpu.z80F & Z80_FLAG_CARRY;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80A >> 7);
				cpu.z80A = (cpu.z80A << 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x18:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80B & Z80_FLAG_CARRY);
				cpu.z80B = (cpu.z80B >> 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80B];
			break;
		}
 		case 0x19:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80C & Z80_FLAG_CARRY);
				cpu.z80C = (cpu.z80C >> 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80C];
			break;
		}
 		case 0x1a:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80D & Z80_FLAG_CARRY);
				cpu.z80D = (cpu.z80D >> 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80D];
			break;
		}
 		case 0x1b:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80E & Z80_FLAG_CARRY);
				cpu.z80E = (cpu.z80E >> 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80E];
			break;
		}
 		case 0x1c:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80H & Z80_FLAG_CARRY);
				cpu.z80H = (cpu.z80H >> 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80H];
			break;
		}
 		case 0x1d:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80L & Z80_FLAG_CARRY);
				cpu.z80L = (cpu.z80L >> 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80L];
			break;
		}
 		case 0x1e:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp & Z80_FLAG_CARRY);
				bTemp = (bTemp >> 1) | bTemp2;
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x1f:
 		{
			sdwCyclesRemaining -= 8;
				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80A & Z80_FLAG_CARRY);
				cpu.z80A = (cpu.z80A >> 1) | bTemp2;
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x20:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80B >> 7);
				cpu.z80B = (cpu.z80B << 1);
				cpu.z80F |= bPostORFlags[cpu.z80B];
			break;
		}
 		case 0x21:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80C >> 7);
				cpu.z80C = (cpu.z80C << 1);
				cpu.z80F |= bPostORFlags[cpu.z80C];
			break;
		}
 		case 0x22:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80D >> 7);
				cpu.z80D = (cpu.z80D << 1);
				cpu.z80F |= bPostORFlags[cpu.z80D];
			break;
		}
 		case 0x23:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80E >> 7);
				cpu.z80E = (cpu.z80E << 1);
				cpu.z80F |= bPostORFlags[cpu.z80E];
			break;
		}
 		case 0x24:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80H >> 7);
				cpu.z80H = (cpu.z80H << 1);
				cpu.z80F |= bPostORFlags[cpu.z80H];
			break;
		}
 		case 0x25:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80L >> 7);
				cpu.z80L = (cpu.z80L << 1);
				cpu.z80F |= bPostORFlags[cpu.z80L];
			break;
		}
 		case 0x26:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp >> 7);
				bTemp = (bTemp << 1);
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x27:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80A >> 7);
				cpu.z80A = (cpu.z80A << 1);
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x28:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80B & Z80_FLAG_CARRY);
				cpu.z80B = (cpu.z80B >> 1) | (cpu.z80B & 0x80);
				cpu.z80F |= bPostORFlags[cpu.z80B];
			break;
		}
 		case 0x29:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80C & Z80_FLAG_CARRY);
				cpu.z80C = (cpu.z80C >> 1) | (cpu.z80C & 0x80);
				cpu.z80F |= bPostORFlags[cpu.z80C];
			break;
		}
 		case 0x2a:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80D & Z80_FLAG_CARRY);
				cpu.z80D = (cpu.z80D >> 1) | (cpu.z80D & 0x80);
				cpu.z80F |= bPostORFlags[cpu.z80D];
			break;
		}
 		case 0x2b:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80E & Z80_FLAG_CARRY);
				cpu.z80E = (cpu.z80E >> 1) | (cpu.z80E & 0x80);
				cpu.z80F |= bPostORFlags[cpu.z80E];
			break;
		}
 		case 0x2c:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80H & Z80_FLAG_CARRY);
				cpu.z80H = (cpu.z80H >> 1) | (cpu.z80H & 0x80);
				cpu.z80F |= bPostORFlags[cpu.z80H];
			break;
		}
 		case 0x2d:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80L & Z80_FLAG_CARRY);
				cpu.z80L = (cpu.z80L >> 1) | (cpu.z80L & 0x80);
				cpu.z80F |= bPostORFlags[cpu.z80L];
			break;
		}
 		case 0x2e:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp & Z80_FLAG_CARRY);
				bTemp = (bTemp >> 1) | (bTemp & 0x80);
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x2f:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80A & Z80_FLAG_CARRY);
				cpu.z80A = (cpu.z80A >> 1) | (cpu.z80A & 0x80);
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x30:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80B >> 7);
				cpu.z80B = (cpu.z80B << 1);
				cpu.z80F |= bPostORFlags[cpu.z80B];
			break;
		}
 		case 0x31:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80C >> 7);
				cpu.z80C = (cpu.z80C << 1);
				cpu.z80F |= bPostORFlags[cpu.z80C];
			break;
		}
 		case 0x32:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80D >> 7);
				cpu.z80D = (cpu.z80D << 1);
				cpu.z80F |= bPostORFlags[cpu.z80D];
			break;
		}
 		case 0x33:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80E >> 7);
				cpu.z80E = (cpu.z80E << 1);
				cpu.z80F |= bPostORFlags[cpu.z80E];
			break;
		}
 		case 0x34:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80H >> 7);
				cpu.z80H = (cpu.z80H << 1);
				cpu.z80F |= bPostORFlags[cpu.z80H];
			break;
		}
 		case 0x35:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80L >> 7);
				cpu.z80L = (cpu.z80L << 1);
				cpu.z80F |= bPostORFlags[cpu.z80L];
			break;
		}
 		case 0x36:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp >> 7);
				bTemp = (bTemp << 1);
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x37:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80A >> 7);
				cpu.z80A = (cpu.z80A << 1);
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x38:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80B & Z80_FLAG_CARRY);
				cpu.z80B = (cpu.z80B >> 1);
				cpu.z80F |= bPostORFlags[cpu.z80B];
			break;
		}
 		case 0x39:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80C & Z80_FLAG_CARRY);
				cpu.z80C = (cpu.z80C >> 1);
				cpu.z80F |= bPostORFlags[cpu.z80C];
			break;
		}
 		case 0x3a:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80D & Z80_FLAG_CARRY);
				cpu.z80D = (cpu.z80D >> 1);
				cpu.z80F |= bPostORFlags[cpu.z80D];
			break;
		}
 		case 0x3b:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80E & Z80_FLAG_CARRY);
				cpu.z80E = (cpu.z80E >> 1);
				cpu.z80F |= bPostORFlags[cpu.z80E];
			break;
		}
 		case 0x3c:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80H & Z80_FLAG_CARRY);
				cpu.z80H = (cpu.z80H >> 1);
				cpu.z80F |= bPostORFlags[cpu.z80H];
			break;
		}
 		case 0x3d:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80L & Z80_FLAG_CARRY);
				cpu.z80L = (cpu.z80L >> 1);
				cpu.z80F |= bPostORFlags[cpu.z80L];
			break;
		}
 		case 0x3e:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp & Z80_FLAG_CARRY);
				bTemp = (bTemp >> 1);
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x3f:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (cpu.z80A & Z80_FLAG_CARRY);
				cpu.z80A = (cpu.z80A >> 1);
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x40:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80B & 0x01))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x41:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80C & 0x01))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x42:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80D & 0x01))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x43:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80E & 0x01))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x44:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80H & 0x01))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x45:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80L & 0x01))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x46:
 		{
			sdwCyclesRemaining -= 12;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(bTemp & 0x01))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x47:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80A & 0x01))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x48:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80B & 0x02))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x49:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80C & 0x02))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x4a:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80D & 0x02))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x4b:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80E & 0x02))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x4c:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80H & 0x02))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x4d:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80L & 0x02))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x4e:
 		{
			sdwCyclesRemaining -= 12;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(bTemp & 0x02))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x4f:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80A & 0x02))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x50:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80B & 0x04))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x51:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80C & 0x04))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x52:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80D & 0x04))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x53:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80E & 0x04))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x54:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80H & 0x04))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x55:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80L & 0x04))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x56:
 		{
			sdwCyclesRemaining -= 12;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(bTemp & 0x04))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x57:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80A & 0x04))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x58:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80B & 0x08))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x59:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80C & 0x08))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x5a:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80D & 0x08))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x5b:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80E & 0x08))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x5c:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80H & 0x08))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x5d:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80L & 0x08))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x5e:
 		{
			sdwCyclesRemaining -= 12;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(bTemp & 0x08))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x5f:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80A & 0x08))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x60:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80B & 0x10))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x61:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80C & 0x10))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x62:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80D & 0x10))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x63:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80E & 0x10))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x64:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80H & 0x10))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x65:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80L & 0x10))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x66:
 		{
			sdwCyclesRemaining -= 12;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(bTemp & 0x10))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x67:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80A & 0x10))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x68:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80B & 0x20))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x69:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80C & 0x20))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x6a:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80D & 0x20))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x6b:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80E & 0x20))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x6c:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80H & 0x20))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x6d:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80L & 0x20))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x6e:
 		{
			sdwCyclesRemaining -= 12;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(bTemp & 0x20))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x6f:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80A & 0x20))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x70:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80B & 0x40))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x71:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80C & 0x40))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x72:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80D & 0x40))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x73:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80E & 0x40))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x74:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80H & 0x40))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x75:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80L & 0x40))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x76:
 		{
			sdwCyclesRemaining -= 12;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(bTemp & 0x40))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x77:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80A & 0x40))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x78:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80B & 0x80))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x79:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80C & 0x80))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x7a:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80D & 0x80))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x7b:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80E & 0x80))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x7c:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80H & 0x80))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x7d:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80L & 0x80))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x7e:
 		{
			sdwCyclesRemaining -= 12;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(bTemp & 0x80))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x7f:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO);
				cpu.z80F |= (Z80_FLAG_HALF_CARRY);
				if (!(cpu.z80A & 0x80))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x80:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B &= 0xfe;
			break;
		}
 		case 0x81:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C &= 0xfe;
			break;
		}
 		case 0x82:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D &= 0xfe;
			break;
		}
 		case 0x83:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E &= 0xfe;
			break;
		}
 		case 0x84:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H &= 0xfe;
			break;
		}
 		case 0x85:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L &= 0xfe;
			break;
		}
 		case 0x86:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp &= 0xfe;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x87:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A &= 0xfe;
			break;
		}
 		case 0x88:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B &= 0xfd;
			break;
		}
 		case 0x89:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C &= 0xfd;
			break;
		}
 		case 0x8a:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D &= 0xfd;
			break;
		}
 		case 0x8b:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E &= 0xfd;
			break;
		}
 		case 0x8c:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H &= 0xfd;
			break;
		}
 		case 0x8d:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L &= 0xfd;
			break;
		}
 		case 0x8e:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp &= 0xfd;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x8f:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A &= 0xfd;
			break;
		}
 		case 0x90:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B &= 0xfb;
			break;
		}
 		case 0x91:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C &= 0xfb;
			break;
		}
 		case 0x92:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D &= 0xfb;
			break;
		}
 		case 0x93:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E &= 0xfb;
			break;
		}
 		case 0x94:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H &= 0xfb;
			break;
		}
 		case 0x95:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L &= 0xfb;
			break;
		}
 		case 0x96:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp &= 0xfb;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x97:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A &= 0xfb;
			break;
		}
 		case 0x98:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B &= 0xf7;
			break;
		}
 		case 0x99:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C &= 0xf7;
			break;
		}
 		case 0x9a:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D &= 0xf7;
			break;
		}
 		case 0x9b:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E &= 0xf7;
			break;
		}
 		case 0x9c:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H &= 0xf7;
			break;
		}
 		case 0x9d:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L &= 0xf7;
			break;
		}
 		case 0x9e:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp &= 0xf7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x9f:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A &= 0xf7;
			break;
		}
 		case 0xa0:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B &= 0xef;
			break;
		}
 		case 0xa1:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C &= 0xef;
			break;
		}
 		case 0xa2:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D &= 0xef;
			break;
		}
 		case 0xa3:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E &= 0xef;
			break;
		}
 		case 0xa4:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H &= 0xef;
			break;
		}
 		case 0xa5:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L &= 0xef;
			break;
		}
 		case 0xa6:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp &= 0xef;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xa7:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A &= 0xef;
			break;
		}
 		case 0xa8:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B &= 0xdf;
			break;
		}
 		case 0xa9:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C &= 0xdf;
			break;
		}
 		case 0xaa:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D &= 0xdf;
			break;
		}
 		case 0xab:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E &= 0xdf;
			break;
		}
 		case 0xac:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H &= 0xdf;
			break;
		}
 		case 0xad:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L &= 0xdf;
			break;
		}
 		case 0xae:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp &= 0xdf;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xaf:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A &= 0xdf;
			break;
		}
 		case 0xb0:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B &= 0xbf;
			break;
		}
 		case 0xb1:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C &= 0xbf;
			break;
		}
 		case 0xb2:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D &= 0xbf;
			break;
		}
 		case 0xb3:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E &= 0xbf;
			break;
		}
 		case 0xb4:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H &= 0xbf;
			break;
		}
 		case 0xb5:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L &= 0xbf;
			break;
		}
 		case 0xb6:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp &= 0xbf;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xb7:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A &= 0xbf;
			break;
		}
 		case 0xb8:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B &= 0x7f;
			break;
		}
 		case 0xb9:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C &= 0x7f;
			break;
		}
 		case 0xba:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D &= 0x7f;
			break;
		}
 		case 0xbb:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E &= 0x7f;
			break;
		}
 		case 0xbc:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H &= 0x7f;
			break;
		}
 		case 0xbd:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L &= 0x7f;
			break;
		}
 		case 0xbe:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp &= 0x7f;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xbf:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A &= 0x7f;
			break;
		}
 		case 0xc0:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B |= 0x01;
			break;
		}
 		case 0xc1:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C |= 0x01;
			break;
		}
 		case 0xc2:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D |= 0x01;
			break;
		}
 		case 0xc3:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E |= 0x01;
			break;
		}
 		case 0xc4:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H |= 0x01;
			break;
		}
 		case 0xc5:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L |= 0x01;
			break;
		}
 		case 0xc6:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp |= 0x01;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xc7:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A |= 0x01;
			break;
		}
 		case 0xc8:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B |= 0x02;
			break;
		}
 		case 0xc9:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C |= 0x02;
			break;
		}
 		case 0xca:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D |= 0x02;
			break;
		}
 		case 0xcb:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E |= 0x02;
			break;
		}
 		case 0xcc:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H |= 0x02;
			break;
		}
 		case 0xcd:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L |= 0x02;
			break;
		}
 		case 0xce:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp |= 0x02;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xcf:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A |= 0x02;
			break;
		}
 		case 0xd0:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B |= 0x04;
			break;
		}
 		case 0xd1:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C |= 0x04;
			break;
		}
 		case 0xd2:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D |= 0x04;
			break;
		}
 		case 0xd3:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E |= 0x04;
			break;
		}
 		case 0xd4:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H |= 0x04;
			break;
		}
 		case 0xd5:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L |= 0x04;
			break;
		}
 		case 0xd6:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp |= 0x04;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xd7:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A |= 0x04;
			break;
		}
 		case 0xd8:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B |= 0x08;
			break;
		}
 		case 0xd9:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C |= 0x08;
			break;
		}
 		case 0xda:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D |= 0x08;
			break;
		}
 		case 0xdb:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E |= 0x08;
			break;
		}
 		case 0xdc:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H |= 0x08;
			break;
		}
 		case 0xdd:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L |= 0x08;
			break;
		}
 		case 0xde:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp |= 0x08;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xdf:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A |= 0x08;
			break;
		}
 		case 0xe0:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B |= 0x10;
			break;
		}
 		case 0xe1:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C |= 0x10;
			break;
		}
 		case 0xe2:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D |= 0x10;
			break;
		}
 		case 0xe3:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E |= 0x10;
			break;
		}
 		case 0xe4:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H |= 0x10;
			break;
		}
 		case 0xe5:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L |= 0x10;
			break;
		}
 		case 0xe6:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp |= 0x10;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xe7:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A |= 0x10;
			break;
		}
 		case 0xe8:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B |= 0x20;
			break;
		}
 		case 0xe9:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C |= 0x20;
			break;
		}
 		case 0xea:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D |= 0x20;
			break;
		}
 		case 0xeb:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E |= 0x20;
			break;
		}
 		case 0xec:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H |= 0x20;
			break;
		}
 		case 0xed:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L |= 0x20;
			break;
		}
 		case 0xee:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp |= 0x20;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xef:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A |= 0x20;
			break;
		}
 		case 0xf0:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B |= 0x40;
			break;
		}
 		case 0xf1:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C |= 0x40;
			break;
		}
 		case 0xf2:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D |= 0x40;
			break;
		}
 		case 0xf3:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E |= 0x40;
			break;
		}
 		case 0xf4:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H |= 0x40;
			break;
		}
 		case 0xf5:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L |= 0x40;
			break;
		}
 		case 0xf6:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp |= 0x40;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xf7:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A |= 0x40;
			break;
		}
 		case 0xf8:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80B |= 0x80;
			break;
		}
 		case 0xf9:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80C |= 0x80;
			break;
		}
 		case 0xfa:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80D |= 0x80;
			break;
		}
 		case 0xfb:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80E |= 0x80;
			break;
		}
 		case 0xfc:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80H |= 0x80;
			break;
		}
 		case 0xfd:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80L |= 0x80;
			break;
		}
 		case 0xfe:
 		{
			sdwCyclesRemaining -= 15;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp |= 0x80;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xff:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80A |= 0x80;
			break;
		}
	}
}
void EDHandler(void)
{
	switch (*pbPC++)
	{
 		case 0x00:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x01:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x02:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x03:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x04:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x05:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x06:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x07:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x08:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x09:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x10:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x11:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x12:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x13:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x14:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x15:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x16:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x17:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x18:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x19:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x20:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x21:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x22:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x23:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x24:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x25:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x26:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x27:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x28:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x29:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x2a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x2b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x2c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x2d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x2e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x2f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x30:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x31:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x32:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x33:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x34:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x35:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x36:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x37:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x38:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x39:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x40:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoRead->lowIoAddr) && (dwAddr <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						cpu.z80B = psIoRead->IOCall(dwAddr, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					cpu.z80B = 0xff; /* Unclaimed I/O read */
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80B];
			break;
		}
 		case 0x41:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoWrite->lowIoAddr) && (dwAddr <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(dwAddr, cpu.z80B, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

			break;
		}
 		case 0x42:
 		{
			sdwCyclesRemaining -= 15;
				dwTemp = cpu.z80HL - cpu.z80BC - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= ((dwTemp >> 8) & Z80_FLAG_SIGN);
				if (0 == (dwTemp & 0xffff))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
				cpu.z80F |= (((cpu.z80HL ^ dwTemp ^ cpu.z80BC) >> 8) & Z80_FLAG_HALF_CARRY);
				cpu.z80F |= ((((cpu.z80BC ^ cpu.z80HL) & (cpu.z80BC ^ dwTemp)) >> 13) & Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY);
				cpu.z80HL = dwTemp & 0xffff;
			break;
		}
 		case 0x43:
 		{
			sdwCyclesRemaining -= 20;
		dwTemp = *pbPC++;
		dwTemp |= ((UINT32) *pbPC++ << 8);
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemWrite->lowAddr) && (dwTemp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwTemp, (cpu.z80BC & 0xff), psMemWrite);
							psMemWrite->memoryCall(dwTemp + 1, (cpu.z80BC >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr)) = cpu.z80BC;
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr) + 1) = cpu.z80BC >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwTemp] = (UINT8) cpu.z80BC;
					cpu.z80Base[dwTemp + 1] = (UINT8) ((UINT32) cpu.z80BC >> 8);
				}

			break;
		}
 		case 0x44:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) 0 << 8) | cpu.z80A];
				cpu.z80A = 0 - cpu.z80A;
			break;
		}
 		case 0x45:
 		{
			sdwCyclesRemaining -= 14;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				cpu.z80iff &= ~(IFF1);	/* Keep IFF2 around */
				cpu.z80iff |= ((cpu.z80iff >> 1) & IFF1);	/* IFF2->IFF1 */
			break;
		}
 		case 0x46:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80interruptMode = 0;
			break;
		}
 		case 0x47:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80i = cpu.z80A;
			break;
		}
 		case 0x48:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoRead->lowIoAddr) && (dwAddr <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						cpu.z80C = psIoRead->IOCall(dwAddr, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					cpu.z80C = 0xff; /* Unclaimed I/O read */
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80C];
			break;
		}
 		case 0x49:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoWrite->lowIoAddr) && (dwAddr <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(dwAddr, cpu.z80C, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

			break;
		}
 		case 0x4a:
 		{
			sdwCyclesRemaining -= 15;
				dwTemp = cpu.z80HL + cpu.z80BC + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= ((dwTemp >> 8) & Z80_FLAG_SIGN);
				if (0 == (dwTemp & 0xffff))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
				cpu.z80F |= (((cpu.z80HL ^ dwTemp ^ cpu.z80BC) >> 8) & Z80_FLAG_HALF_CARRY);
				cpu.z80F |= ((((cpu.z80BC ^ cpu.z80HL ^ 0x8000) & (cpu.z80BC ^ dwTemp)) >> 13) & Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY);
				cpu.z80HL = dwTemp & 0xffff;
			break;
		}
 		case 0x4b:
 		{
			sdwCyclesRemaining -= 20;
		dwTemp = *pbPC++;
		dwTemp |= ((UINT32) *pbPC++ << 8);
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemRead->lowAddr) && (dwTemp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80BC = psMemRead->memoryCall(dwTemp, psMemRead);
							cpu.z80BC |= (UINT32) ((UINT32) psMemRead->memoryCall(dwTemp + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80BC = *((UINT8 *) psMemRead->pUserArea + (dwTemp - psMemRead->lowAddr));
							cpu.z80BC |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (dwTemp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80BC = cpu.z80Base[dwTemp];
					cpu.z80BC |= (UINT32) ((UINT32) cpu.z80Base[dwTemp + 1] << 8);
				}

			break;
		}
 		case 0x4c:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x4d:
 		{
			sdwCyclesRemaining -= 14;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
			break;
		}
 		case 0x4e:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x4f:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80r = cpu.z80A;
			break;
		}
 		case 0x50:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoRead->lowIoAddr) && (dwAddr <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						cpu.z80D = psIoRead->IOCall(dwAddr, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					cpu.z80D = 0xff; /* Unclaimed I/O read */
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80D];
			break;
		}
 		case 0x51:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoWrite->lowIoAddr) && (dwAddr <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(dwAddr, cpu.z80D, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

			break;
		}
 		case 0x52:
 		{
			sdwCyclesRemaining -= 15;
				dwTemp = cpu.z80HL - cpu.z80DE - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= ((dwTemp >> 8) & Z80_FLAG_SIGN);
				if (0 == (dwTemp & 0xffff))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
				cpu.z80F |= (((cpu.z80HL ^ dwTemp ^ cpu.z80DE) >> 8) & Z80_FLAG_HALF_CARRY);
				cpu.z80F |= ((((cpu.z80DE ^ cpu.z80HL) & (cpu.z80DE ^ dwTemp)) >> 13) & Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY);
				cpu.z80HL = dwTemp & 0xffff;
			break;
		}
 		case 0x53:
 		{
			sdwCyclesRemaining -= 20;
		dwTemp = *pbPC++;
		dwTemp |= ((UINT32) *pbPC++ << 8);
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemWrite->lowAddr) && (dwTemp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwTemp, (cpu.z80DE & 0xff), psMemWrite);
							psMemWrite->memoryCall(dwTemp + 1, (cpu.z80DE >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr)) = cpu.z80DE;
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr) + 1) = cpu.z80DE >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwTemp] = (UINT8) cpu.z80DE;
					cpu.z80Base[dwTemp + 1] = (UINT8) ((UINT32) cpu.z80DE >> 8);
				}

			break;
		}
 		case 0x54:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x55:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x56:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80interruptMode = 1;
				cpu.z80intAddr = 0x38;
			break;
		}
 		case 0x57:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= ((cpu.z80iff & IFF2) << 1);
				cpu.z80A = cpu.z80i;
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x58:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoRead->lowIoAddr) && (dwAddr <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						cpu.z80E = psIoRead->IOCall(dwAddr, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					cpu.z80E = 0xff; /* Unclaimed I/O read */
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80E];
			break;
		}
 		case 0x59:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoWrite->lowIoAddr) && (dwAddr <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(dwAddr, cpu.z80E, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

			break;
		}
 		case 0x5a:
 		{
			sdwCyclesRemaining -= 15;
				dwTemp = cpu.z80HL + cpu.z80DE + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= ((dwTemp >> 8) & Z80_FLAG_SIGN);
				if (0 == (dwTemp & 0xffff))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
				cpu.z80F |= (((cpu.z80HL ^ dwTemp ^ cpu.z80DE) >> 8) & Z80_FLAG_HALF_CARRY);
				cpu.z80F |= ((((cpu.z80DE ^ cpu.z80HL ^ 0x8000) & (cpu.z80DE ^ dwTemp)) >> 13) & Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY);
				cpu.z80HL = dwTemp & 0xffff;
			break;
		}
 		case 0x5b:
 		{
			sdwCyclesRemaining -= 20;
		dwTemp = *pbPC++;
		dwTemp |= ((UINT32) *pbPC++ << 8);
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemRead->lowAddr) && (dwTemp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80DE = psMemRead->memoryCall(dwTemp, psMemRead);
							cpu.z80DE |= (UINT32) ((UINT32) psMemRead->memoryCall(dwTemp + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80DE = *((UINT8 *) psMemRead->pUserArea + (dwTemp - psMemRead->lowAddr));
							cpu.z80DE |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (dwTemp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80DE = cpu.z80Base[dwTemp];
					cpu.z80DE |= (UINT32) ((UINT32) cpu.z80Base[dwTemp + 1] << 8);
				}

			break;
		}
 		case 0x5c:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x5d:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x5e:
 		{
			sdwCyclesRemaining -= 8;
				cpu.z80interruptMode = 2;
			break;
		}
 		case 0x5f:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80r];
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_OVERFLOW_PARITY)) | ((cpu.z80iff & IFF2) << 1);
				cpu.z80A = cpu.z80r;
				bTemp = (cpu.z80r + (cpu.z80B + sdwCyclesRemaining + 1 + cpu.z80H)) ^ cpu.z80A;
				cpu.z80r = (cpu.z80r & 0x80) | (bTemp & 0x7f);
			break;
		}
 		case 0x60:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoRead->lowIoAddr) && (dwAddr <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						cpu.z80H = psIoRead->IOCall(dwAddr, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					cpu.z80H = 0xff; /* Unclaimed I/O read */
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80H];
			break;
		}
 		case 0x61:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoWrite->lowIoAddr) && (dwAddr <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(dwAddr, cpu.z80H, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

			break;
		}
 		case 0x62:
 		{
			sdwCyclesRemaining -= 15;
				dwTemp = cpu.z80HL - cpu.z80HL - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= ((dwTemp >> 8) & Z80_FLAG_SIGN);
				if (0 == (dwTemp & 0xffff))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
				cpu.z80F |= (((cpu.z80HL ^ dwTemp ^ cpu.z80HL) >> 8) & Z80_FLAG_HALF_CARRY);
				cpu.z80F |= ((((cpu.z80HL ^ cpu.z80HL) & (cpu.z80HL ^ dwTemp)) >> 13) & Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY);
				cpu.z80HL = dwTemp & 0xffff;
			break;
		}
 		case 0x63:
 		{
			sdwCyclesRemaining -= 20;
		dwTemp = *pbPC++;
		dwTemp |= ((UINT32) *pbPC++ << 8);
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemWrite->lowAddr) && (dwTemp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwTemp, (cpu.z80HL & 0xff), psMemWrite);
							psMemWrite->memoryCall(dwTemp + 1, (cpu.z80HL >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr)) = cpu.z80HL;
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr) + 1) = cpu.z80HL >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwTemp] = (UINT8) cpu.z80HL;
					cpu.z80Base[dwTemp + 1] = (UINT8) ((UINT32) cpu.z80HL >> 8);
				}

			break;
		}
 		case 0x64:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x65:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x66:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x67:
 		{
			sdwCyclesRemaining -= 18;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp2 = (cpu.z80A & 0x0f) << 4;
				cpu.z80A = (cpu.z80A & 0xf0) | (bTemp & 0x0f);
				bTemp = (bTemp >> 4) | bTemp2;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x68:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoRead->lowIoAddr) && (dwAddr <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						cpu.z80L = psIoRead->IOCall(dwAddr, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					cpu.z80L = 0xff; /* Unclaimed I/O read */
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80L];
			break;
		}
 		case 0x69:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoWrite->lowIoAddr) && (dwAddr <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(dwAddr, cpu.z80L, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

			break;
		}
 		case 0x6a:
 		{
			sdwCyclesRemaining -= 15;
				dwTemp = cpu.z80HL + cpu.z80HL + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= ((dwTemp >> 8) & Z80_FLAG_SIGN);
				if (0 == (dwTemp & 0xffff))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
				cpu.z80F |= (((cpu.z80HL ^ dwTemp ^ cpu.z80HL) >> 8) & Z80_FLAG_HALF_CARRY);
				cpu.z80F |= ((((cpu.z80HL ^ cpu.z80HL ^ 0x8000) & (cpu.z80HL ^ dwTemp)) >> 13) & Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY);
				cpu.z80HL = dwTemp & 0xffff;
			break;
		}
 		case 0x6b:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(2);
			break;
		}
 		case 0x6c:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x6d:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x6e:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x6f:
 		{
			sdwCyclesRemaining -= 18;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp2 = (cpu.z80A & 0x0f);
				cpu.z80A = (cpu.z80A & 0xf0) | (bTemp >> 4);
				bTemp = (bTemp << 4) | bTemp2;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x70:
 		{
			sdwCyclesRemaining -= 12;
				InvalidInstruction(2);
			break;
		}
 		case 0x71:
 		{
			sdwCyclesRemaining -= 12;
				InvalidInstruction(2);
			break;
		}
 		case 0x72:
 		{
			sdwCyclesRemaining -= 15;
				dwTemp = cpu.z80HL - cpu.z80sp - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= ((dwTemp >> 8) & Z80_FLAG_SIGN);
				if (0 == (dwTemp & 0xffff))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
				cpu.z80F |= (((cpu.z80HL ^ dwTemp ^ cpu.z80sp) >> 8) & Z80_FLAG_HALF_CARRY);
				cpu.z80F |= ((((cpu.z80sp ^ cpu.z80HL) & (cpu.z80sp ^ dwTemp)) >> 13) & Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY);
				cpu.z80HL = dwTemp & 0xffff;
			break;
		}
 		case 0x73:
 		{
			sdwCyclesRemaining -= 20;
		dwTemp = *pbPC++;
		dwTemp |= ((UINT32) *pbPC++ << 8);
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemWrite->lowAddr) && (dwTemp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwTemp, (cpu.z80sp & 0xff), psMemWrite);
							psMemWrite->memoryCall(dwTemp + 1, (cpu.z80sp >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr)) = cpu.z80sp;
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr) + 1) = cpu.z80sp >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwTemp] = (UINT8) cpu.z80sp;
					cpu.z80Base[dwTemp + 1] = (UINT8) ((UINT32) cpu.z80sp >> 8);
				}

			break;
		}
 		case 0x74:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x75:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x76:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x77:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x78:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoRead->lowIoAddr) && (dwAddr <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						cpu.z80A = psIoRead->IOCall(dwAddr, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					cpu.z80A = 0xff; /* Unclaimed I/O read */
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostORFlags[cpu.z80A];
			break;
		}
 		case 0x79:
 		{
			sdwCyclesRemaining -= 12;
				dwAddr = cpu.z80C;
				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((dwAddr >= psIoWrite->lowIoAddr) && (dwAddr <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(dwAddr, cpu.z80A, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

			break;
		}
 		case 0x7a:
 		{
			sdwCyclesRemaining -= 15;
				dwTemp = cpu.z80HL + cpu.z80sp + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= ((dwTemp >> 8) & Z80_FLAG_SIGN);
				if (0 == (dwTemp & 0xffff))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
				cpu.z80F |= (((cpu.z80HL ^ dwTemp ^ cpu.z80sp) >> 8) & Z80_FLAG_HALF_CARRY);
				cpu.z80F |= ((((cpu.z80sp ^ cpu.z80HL ^ 0x8000) & (cpu.z80sp ^ dwTemp)) >> 13) & Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY);
				cpu.z80HL = dwTemp & 0xffff;
			break;
		}
 		case 0x7b:
 		{
			sdwCyclesRemaining -= 20;
		dwTemp = *pbPC++;
		dwTemp |= ((UINT32) *pbPC++ << 8);
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemRead->lowAddr) && (dwTemp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80sp = psMemRead->memoryCall(dwTemp, psMemRead);
							cpu.z80sp |= (UINT32) ((UINT32) psMemRead->memoryCall(dwTemp + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80sp = *((UINT8 *) psMemRead->pUserArea + (dwTemp - psMemRead->lowAddr));
							cpu.z80sp |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (dwTemp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80sp = cpu.z80Base[dwTemp];
					cpu.z80sp |= (UINT32) ((UINT32) cpu.z80Base[dwTemp + 1] << 8);
				}

			break;
		}
 		case 0x7c:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x7d:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x7e:
 		{
			sdwCyclesRemaining -= 8;
				InvalidInstruction(2);
			break;
		}
 		case 0x7f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x80:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x81:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x82:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x83:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x84:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x85:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x86:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x87:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x88:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x89:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x90:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x91:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x92:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x93:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x94:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x95:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x96:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x97:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x98:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x99:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa0:
 		{
			sdwCyclesRemaining -= 16;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80DE >= psMemWrite->lowAddr) && (cpu.z80DE <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80DE, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80DE - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80DE] = (UINT8) bTemp;
				}

					++cpu.z80HL;
					++cpu.z80DE;
				--cpu.z80BC;
				cpu.z80HL &= 0xffff;
				cpu.z80DE &= 0xffff;
				cpu.z80BC &= 0xffff;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY);
				if (cpu.z80BC)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xa1:
 		{
			sdwCyclesRemaining -= 16;
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80HL++;
				cpu.z80HL &= 0xffff;
				cpu.z80BC--;
				cpu.z80BC &= 0xffff;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO));
				if (cpu.z80BC)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xa2:
 		{
			sdwCyclesRemaining -= 16;
				{
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((cpu.z80B >= psIoRead->lowIoAddr) && (cpu.z80B <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						bTemp = psIoRead->IOCall(cpu.z80B, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					bTemp = 0xff; /* Unclaimed I/O read */
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

				cpu.z80HL++;
				cpu.z80HL &= 0xffff;
				sdwCyclesRemaining -= 16;
				cpu.z80B--;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (bPostORFlags[bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY));
				if (cpu.z80B)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
					pbPC -= 2;
				}
			break;
		}
 		case 0xa3:
 		{
			sdwCyclesRemaining -= 16;
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((cpu.z80BC >= psIoWrite->lowIoAddr) && (cpu.z80BC <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(cpu.z80BC, bTemp, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

				cpu.z80HL++;
				cpu.z80HL &= 0xffff;
				sdwCyclesRemaining -= 16;
				cpu.z80B--;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (bPostORFlags[bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY));
				if (cpu.z80B)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xa4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa8:
 		{
			sdwCyclesRemaining -= 16;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80DE >= psMemWrite->lowAddr) && (cpu.z80DE <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80DE, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80DE - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80DE] = (UINT8) bTemp;
				}

					--cpu.z80HL;
					--cpu.z80DE;
				--cpu.z80BC;
				cpu.z80HL &= 0xffff;
				cpu.z80DE &= 0xffff;
				cpu.z80BC &= 0xffff;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY);
				if (cpu.z80BC)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xa9:
 		{
			sdwCyclesRemaining -= 16;
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80HL--;
				cpu.z80HL &= 0xffff;
				cpu.z80BC--;
				cpu.z80BC &= 0xffff;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO));
				if (cpu.z80BC)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xaa:
 		{
			sdwCyclesRemaining -= 16;
				{
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((cpu.z80B >= psIoRead->lowIoAddr) && (cpu.z80B <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						bTemp = psIoRead->IOCall(cpu.z80B, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					bTemp = 0xff; /* Unclaimed I/O read */
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

				cpu.z80HL--;
				cpu.z80HL &= 0xffff;
				sdwCyclesRemaining -= 16;
				cpu.z80B--;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (bPostORFlags[bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY));
				if (cpu.z80B)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
					pbPC -= 2;
				}
			break;
		}
 		case 0xab:
 		{
			sdwCyclesRemaining -= 16;
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((cpu.z80BC >= psIoWrite->lowIoAddr) && (cpu.z80BC <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(cpu.z80BC, bTemp, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

				cpu.z80HL--;
				cpu.z80HL &= 0xffff;
				sdwCyclesRemaining -= 16;
				cpu.z80B--;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (bPostORFlags[bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY));
				if (cpu.z80B)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xac:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xad:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xae:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xaf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb0:
 		{
			sdwCyclesRemaining -= 16;
				while ((sdwCyclesRemaining > 0) && (cpu.z80BC))
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80DE >= psMemWrite->lowAddr) && (cpu.z80DE <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80DE, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80DE - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80DE] = (UINT8) bTemp;
				}

					++cpu.z80HL;
					++cpu.z80DE;
				--cpu.z80BC;
				cpu.z80HL &= 0xffff;
				cpu.z80DE &= 0xffff;
				cpu.z80BC &= 0xffff;
				sdwCyclesRemaining -= 21;
				}
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY);
				if (cpu.z80BC)
				{
					pbPC -= 2;	/* Back up so we hit this instruction again */
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
				sdwCyclesRemaining -= 16;
			break;
		}
 		case 0xb1:
 		{
			sdwCyclesRemaining -= 16;
				while ((sdwCyclesRemaining >= 0) && (cpu.z80BC))
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80HL++;
				cpu.z80HL &= 0xffff;
				cpu.z80BC--;
				cpu.z80BC &= 0xffff;
				sdwCyclesRemaining -= 16;
				if (cpu.z80A == bTemp)
				{
					break;
				}
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO));
				if (cpu.z80BC)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xb2:
 		{
			sdwCyclesRemaining -= 16;
				while ((sdwCyclesRemaining > 0) && (cpu.z80B))
				{
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((cpu.z80B >= psIoRead->lowIoAddr) && (cpu.z80B <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						bTemp = psIoRead->IOCall(cpu.z80B, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					bTemp = 0xff; /* Unclaimed I/O read */
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

				cpu.z80HL++;
				cpu.z80HL &= 0xffff;
				sdwCyclesRemaining -= 16;
				cpu.z80B--;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (bPostORFlags[bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY));
				if (cpu.z80B)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
					pbPC -= 2;
				}
			break;
		}
 		case 0xb3:
 		{
			sdwCyclesRemaining -= 16;
				while ((sdwCyclesRemaining > 0) && (cpu.z80B))
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((cpu.z80BC >= psIoWrite->lowIoAddr) && (cpu.z80BC <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(cpu.z80BC, bTemp, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

				cpu.z80HL++;
				cpu.z80HL &= 0xffff;
				sdwCyclesRemaining -= 16;
				cpu.z80B--;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (bPostORFlags[bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY));
				if (cpu.z80B)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xb4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb8:
 		{
			sdwCyclesRemaining -= 16;
				while ((sdwCyclesRemaining > 0) && (cpu.z80BC))
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80DE >= psMemWrite->lowAddr) && (cpu.z80DE <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80DE, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80DE - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80DE] = (UINT8) bTemp;
				}

					--cpu.z80HL;
					--cpu.z80DE;
				--cpu.z80BC;
				cpu.z80HL &= 0xffff;
				cpu.z80DE &= 0xffff;
				cpu.z80BC &= 0xffff;
				sdwCyclesRemaining -= 21;
				}
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY);
				if (cpu.z80BC)
				{
					pbPC -= 2;	/* Back up so we hit this instruction again */
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
				sdwCyclesRemaining -= 16;
			break;
		}
 		case 0xb9:
 		{
			sdwCyclesRemaining -= 16;
				while ((sdwCyclesRemaining >= 0) && (cpu.z80BC))
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80HL--;
				cpu.z80HL &= 0xffff;
				cpu.z80BC--;
				cpu.z80BC &= 0xffff;
				sdwCyclesRemaining -= 16;
				if (cpu.z80A == bTemp)
				{
					break;
				}
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_ZERO));
				if (cpu.z80BC)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xba:
 		{
			sdwCyclesRemaining -= 16;
				while ((sdwCyclesRemaining > 0) && (cpu.z80B))
				{
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((cpu.z80B >= psIoRead->lowIoAddr) && (cpu.z80B <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						bTemp = psIoRead->IOCall(cpu.z80B, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					bTemp = 0xff; /* Unclaimed I/O read */
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

				cpu.z80HL--;
				cpu.z80HL &= 0xffff;
				sdwCyclesRemaining -= 16;
				cpu.z80B--;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (bPostORFlags[bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY));
				if (cpu.z80B)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
					pbPC -= 2;
				}
			break;
		}
 		case 0xbb:
 		{
			sdwCyclesRemaining -= 16;
				while ((sdwCyclesRemaining > 0) && (cpu.z80B))
				{
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((cpu.z80BC >= psIoWrite->lowIoAddr) && (cpu.z80BC <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(cpu.z80BC, bTemp, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

				cpu.z80HL--;
				cpu.z80HL &= 0xffff;
				sdwCyclesRemaining -= 16;
				cpu.z80B--;
				}
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= (bPostORFlags[bTemp] & (Z80_FLAG_SIGN | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY));
				if (cpu.z80B)
				{
					cpu.z80F |= Z80_FLAG_OVERFLOW_PARITY;
				}
			break;
		}
 		case 0xbc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xbd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xbe:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xbf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xca:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xce:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xda:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xde:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xea:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xeb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xec:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xed:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xee:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xef:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfa:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfe:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xff:
 		{
				InvalidInstruction(2);
			break;
		}
	}
}

void DDHandler(void)
{
	switch (*pbPC++)
	{
 		case 0x00:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x01:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x02:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x03:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x04:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x05:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x06:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x07:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x08:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x09:
 		{
			sdwCyclesRemaining -= 15;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80IX + cpu.z80BC;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80IX ^ dwTemp ^ cpu.z80BC) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80IX = dwTemp & 0xffff;
			break;
		}
 		case 0x0a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x10:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x11:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x12:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x13:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x14:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x15:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x16:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x17:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x18:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x19:
 		{
			sdwCyclesRemaining -= 15;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80IX + cpu.z80DE;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80IX ^ dwTemp ^ cpu.z80DE) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80IX = dwTemp & 0xffff;
			break;
		}
 		case 0x1a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x20:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x21:
 		{
			sdwCyclesRemaining -= 14;
		cpu.z80IX = *pbPC++;
		cpu.z80IX |= ((UINT32) *pbPC++ << 8);
			break;
		}
 		case 0x22:
 		{
			sdwCyclesRemaining -= 20;
				dwAddr = *pbPC++;
				dwAddr |= ((UINT32) *pbPC++ << 8);
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, (cpu.z80IX & 0xff), psMemWrite);
							psMemWrite->memoryCall(dwAddr + 1, (cpu.z80IX >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = cpu.z80IX;
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr) + 1) = cpu.z80IX >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) cpu.z80IX;
					cpu.z80Base[dwAddr + 1] = (UINT8) ((UINT32) cpu.z80IX >> 8);
				}

			break;
		}
 		case 0x23:
 		{
			sdwCyclesRemaining -= 10;
				cpu.z80IX++;
				cpu.z80IX &= 0xffff;
			break;
		}
 		case 0x24:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80XH++];
			break;
		}
 		case 0x25:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostDecFlags[cpu.z80XH--];
			break;
		}
 		case 0x26:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XH = *pbPC++;
			break;
		}
 		case 0x27:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x28:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x29:
 		{
			sdwCyclesRemaining -= 15;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80IX + cpu.z80IX;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80IX ^ dwTemp ^ cpu.z80HL) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80IX = dwTemp & 0xffff;
			break;
		}
 		case 0x2a:
 		{
			sdwCyclesRemaining -= 20;
				dwAddr = *pbPC++;
				dwAddr |= ((UINT32) *pbPC++ << 8);
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80IX = psMemRead->memoryCall(dwAddr, psMemRead);
							cpu.z80IX |= (UINT32) ((UINT32) psMemRead->memoryCall(dwAddr + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80IX = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
							cpu.z80IX |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80IX = cpu.z80Base[dwAddr];
					cpu.z80IX |= (UINT32) ((UINT32) cpu.z80Base[dwAddr + 1] << 8);
				}

			break;
		}
 		case 0x2b:
 		{
			sdwCyclesRemaining -= 10;
				cpu.z80IX--;
				cpu.z80IX &= 0xffff;
			break;
		}
 		case 0x2c:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80XL++];
			break;
		}
 		case 0x2d:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostDecFlags[cpu.z80XL--];
			break;
		}
 		case 0x2e:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XL = *pbPC++;
			break;
		}
 		case 0x2f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x30:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x31:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x32:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x33:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x34:
 		{
			sdwCyclesRemaining -= 23;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[bTemp++];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x35:
 		{
			sdwCyclesRemaining -= 23;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostDecFlags[bTemp--];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x36:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, *pbPC++, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = *pbPC++;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) *pbPC++;
				}

			break;
		}
 		case 0x37:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x38:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x39:
 		{
			sdwCyclesRemaining -= 15;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80IX + cpu.z80sp;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80IX ^ dwTemp ^ cpu.z80sp) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80IX = dwTemp & 0xffff;
			break;
		}
 		case 0x3a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x40:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x41:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x42:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x43:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x44:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80B = cpu.z80XH;
			break;
		}
 		case 0x45:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80B = cpu.z80XL;
			break;
		}
 		case 0x46:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80B = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80B = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80B = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x47:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x48:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x49:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x4a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x4b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x4c:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80C = cpu.z80XH;
			break;
		}
 		case 0x4d:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80C = cpu.z80XL;
			break;
		}
 		case 0x4e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80C = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80C = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80C = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x4f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x50:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x51:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x52:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x53:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x54:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80D = cpu.z80XH;
			break;
		}
 		case 0x55:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80D = cpu.z80XL;
			break;
		}
 		case 0x56:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80D = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80D = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80D = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x57:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x58:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x59:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x5a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x5b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x5c:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80E = cpu.z80XH;
			break;
		}
 		case 0x5d:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80E = cpu.z80XL;
			break;
		}
 		case 0x5e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80E = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80E = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80E = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x5f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x60:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XH = cpu.z80B;
			break;
		}
 		case 0x61:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XH = cpu.z80C;
			break;
		}
 		case 0x62:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XH = cpu.z80D;
			break;
		}
 		case 0x63:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XH = cpu.z80E;
			break;
		}
 		case 0x64:
 		{
			sdwCyclesRemaining -= 9;
			break;
		}
 		case 0x65:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XH = cpu.z80XL;
			break;
		}
 		case 0x66:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80H = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80H = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80H = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x67:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XH = cpu.z80A;
			break;
		}
 		case 0x68:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XL = cpu.z80B;
			break;
		}
 		case 0x69:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XL = cpu.z80C;
			break;
		}
 		case 0x6a:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XL = cpu.z80D;
			break;
		}
 		case 0x6b:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XL = cpu.z80E;
			break;
		}
 		case 0x6c:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XL = cpu.z80XH;
			break;
		}
 		case 0x6d:
 		{
			sdwCyclesRemaining -= 9;
			break;
		}
 		case 0x6e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80L = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80L = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80L = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x6f:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80XL = cpu.z80A;
			break;
		}
 		case 0x70:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80B, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80B;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80B;
				}

			break;
		}
 		case 0x71:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80C, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80C;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80C;
				}

			break;
		}
 		case 0x72:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80D, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80D;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80D;
				}

			break;
		}
 		case 0x73:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80E, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80E;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80E;
				}

			break;
		}
 		case 0x74:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80H, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80H;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80H;
				}

			break;
		}
 		case 0x75:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80L, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80L;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80L;
				}

			break;
		}
 		case 0x76:
 		{
			sdwCyclesRemaining -= 19;
				InvalidInstruction(2);
			break;
		}
 		case 0x77:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80A, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80A;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80A;
				}

			break;
		}
 		case 0x78:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x79:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x7a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x7b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x7c:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80A = cpu.z80XH;
			break;
		}
 		case 0x7d:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80A = cpu.z80XL;
			break;
		}
 		case 0x7e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IX + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80A = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80A = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80A = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x7f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x80:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x81:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x82:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x83:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x84:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A + cpu.z80XH;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80XH];
				InvalidInstruction(2);
			break;
		}
 		case 0x85:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A + cpu.z80XL;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80XL];
				InvalidInstruction(2);
			break;
		}
 		case 0x86:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | bTemp];
				cpu.z80A += bTemp;
			break;
		}
 		case 0x87:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x88:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x89:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8c:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A + cpu.z80XH + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80XH | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				InvalidInstruction(2);
			break;
		}
 		case 0x8d:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A + cpu.z80XL + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80XL | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				InvalidInstruction(2);
			break;
		}
 		case 0x8e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | bTemp | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A += bTemp + bTemp2;
			break;
		}
 		case 0x8f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x90:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x91:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x92:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x93:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x94:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A - cpu.z80XH;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80XH];
				InvalidInstruction(2);
			break;
		}
 		case 0x95:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A - cpu.z80XL;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80XL];
				InvalidInstruction(2);
			break;
		}
 		case 0x96:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp];
				cpu.z80A -= bTemp;
			break;
		}
 		case 0x97:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x98:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x99:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9c:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A - cpu.z80XH - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80XH | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				InvalidInstruction(2);
			break;
		}
 		case 0x9d:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A - cpu.z80XL - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80XL | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				InvalidInstruction(2);
			break;
		}
 		case 0x9e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				bTemp2 = cpu.z80A;
				cpu.z80A = cpu.z80A - bTemp - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) bTemp2 << 8) | bTemp | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
			break;
		}
 		case 0x9f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa4:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A &= cpu.z80XH;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xa5:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A &= cpu.z80XL;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xa6:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80A &= bTemp;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

			break;
		}
 		case 0xa7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xaa:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xab:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xac:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A ^= cpu.z80XH;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xad:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A ^= cpu.z80XL;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xae:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80A ^= bTemp;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

			break;
		}
 		case 0xaf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb4:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A |= cpu.z80XH;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xb5:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A |= cpu.z80XL;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xb6:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80A |= bTemp;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

			break;
		}
 		case 0xb7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xba:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xbb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xbc:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xbd:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xbe:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IX) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp];
			break;
		}
 		case 0xbf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xca:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcb:
 		{
				DDFDCBHandler(0);
			break;
		}
 		case 0xcc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xce:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xda:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xde:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe1:
 		{
			sdwCyclesRemaining -= 14;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemRead->lowAddr) && (cpu.z80sp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80IX = psMemRead->memoryCall(cpu.z80sp, psMemRead);
							cpu.z80IX |= (UINT32) ((UINT32) psMemRead->memoryCall(cpu.z80sp + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80IX = *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr));
							cpu.z80IX |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80IX = cpu.z80Base[cpu.z80sp];
					cpu.z80IX |= (UINT32) ((UINT32) cpu.z80Base[cpu.z80sp + 1] << 8);
				}

					cpu.z80sp += 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
			break;
		}
 		case 0xe2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe3:
 		{
			sdwCyclesRemaining -= 23;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemRead->lowAddr) && (cpu.z80sp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							dwAddr = psMemRead->memoryCall(cpu.z80sp, psMemRead);
							dwAddr |= (UINT32) ((UINT32) psMemRead->memoryCall(cpu.z80sp + 1, psMemRead) << 8);
						}
						else
						{
							dwAddr = *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr));
							dwAddr |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					dwAddr = cpu.z80Base[cpu.z80sp];
					dwAddr |= (UINT32) ((UINT32) cpu.z80Base[cpu.z80sp + 1] << 8);
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemWrite->lowAddr) && (cpu.z80sp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80sp, (cpu.z80IX & 0xff), psMemWrite);
							psMemWrite->memoryCall(cpu.z80sp + 1, (cpu.z80IX >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr)) = cpu.z80IX;
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr) + 1) = cpu.z80IX >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80sp] = (UINT8) cpu.z80IX;
					cpu.z80Base[cpu.z80sp + 1] = (UINT8) ((UINT32) cpu.z80IX >> 8);
				}

				cpu.z80IX = dwAddr;
			break;
		}
 		case 0xe4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe5:
 		{
			sdwCyclesRemaining -= 15;
					cpu.z80sp -= 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemWrite->lowAddr) && (cpu.z80sp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80sp, (cpu.z80IX & 0xff), psMemWrite);
							psMemWrite->memoryCall(cpu.z80sp + 1, (cpu.z80IX >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr)) = cpu.z80IX;
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr) + 1) = cpu.z80IX >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80sp] = (UINT8) cpu.z80IX;
					cpu.z80Base[cpu.z80sp + 1] = (UINT8) ((UINT32) cpu.z80IX >> 8);
				}

			break;
		}
 		case 0xe6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe9:
 		{
			sdwCyclesRemaining -= 8;
				pbPC = cpu.z80Base + cpu.z80IX;
			break;
		}
 		case 0xea:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xeb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xec:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xed:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xee:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xef:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf9:
 		{
			sdwCyclesRemaining -= 10;
				cpu.z80sp = cpu.z80IX;
			break;
		}
 		case 0xfa:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfe:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xff:
 		{
				InvalidInstruction(2);
			break;
		}
	}
}
void DDFDCBHandler(UINT32 dwWhich)
{
	if (dwWhich)
	{
		dwAddr = (UINT32) ((INT32) cpu.z80IY + ((INT32) *pbPC++)) & 0xffff;
	}
	else
	{
		dwAddr = (UINT32) ((INT32) cpu.z80IX + ((INT32) *pbPC++)) & 0xffff;
	}

				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

	switch (*pbPC++)
	{
 		case 0x00:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x01:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x02:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x03:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x04:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x05:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x06:
 		{
			sdwCyclesRemaining -= 23;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				bTemp2 = (bTemp >> 7);
				bTemp = (bTemp << 1) | bTemp2;
				cpu.z80F |= bTemp2 | bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x07:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x08:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x09:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x0a:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x0b:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x0c:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x0d:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x0e:
 		{
			sdwCyclesRemaining -= 23;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp & Z80_FLAG_CARRY);
				bTemp = (bTemp >> 1) | (bTemp << 7);
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x0f:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x10:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x11:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x12:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x13:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x14:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x15:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x16:
 		{
			sdwCyclesRemaining -= 23;
				bTemp2 = cpu.z80F & Z80_FLAG_CARRY;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp >> 7);
				bTemp = (bTemp << 1) | bTemp2;
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x17:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x18:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x19:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x1a:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x1b:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x1c:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x1d:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x1e:
 		{
			sdwCyclesRemaining -= 23;
				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp & Z80_FLAG_CARRY);
				bTemp = (bTemp >> 1) | bTemp2;
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x1f:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x20:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x21:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x22:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x23:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x24:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x25:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x26:
 		{
			sdwCyclesRemaining -= 23;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp >> 7);
				bTemp = (bTemp << 1);
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x27:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x28:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x29:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x2a:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x2b:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x2c:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x2d:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x2e:
 		{
			sdwCyclesRemaining -= 23;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp & Z80_FLAG_CARRY);
				bTemp = (bTemp >> 1) | (bTemp & 0x80);
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x2f:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x30:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x31:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x32:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x33:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x34:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x35:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x36:
 		{
			sdwCyclesRemaining -= 23;
				InvalidInstruction(4);
			break;
		}
 		case 0x37:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x38:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x39:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x3a:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x3b:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x3c:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x3d:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x3e:
 		{
			sdwCyclesRemaining -= 23;
				cpu.z80F &= ~(Z80_FLAG_ZERO | Z80_FLAG_SIGN | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE | Z80_FLAG_CARRY);
				cpu.z80F |= (bTemp & Z80_FLAG_CARRY);
				bTemp = (bTemp >> 1);
				cpu.z80F |= bPostORFlags[bTemp];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x3f:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x40:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x41:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x42:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x43:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x44:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x45:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x46:
 		{
			sdwCyclesRemaining -= 20;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_ZERO | Z80_FLAG_NEGATIVE)) | Z80_FLAG_HALF_CARRY;
				if (!(bTemp & 0x01))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x47:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x48:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x49:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x4a:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x4b:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x4c:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x4d:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x4e:
 		{
			sdwCyclesRemaining -= 20;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_ZERO | Z80_FLAG_NEGATIVE)) | Z80_FLAG_HALF_CARRY;
				if (!(bTemp & 0x02))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x4f:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x50:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x51:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x52:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x53:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x54:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x55:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x56:
 		{
			sdwCyclesRemaining -= 20;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_ZERO | Z80_FLAG_NEGATIVE)) | Z80_FLAG_HALF_CARRY;
				if (!(bTemp & 0x04))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x57:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x58:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x59:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x5a:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x5b:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x5c:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x5d:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x5e:
 		{
			sdwCyclesRemaining -= 20;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_ZERO | Z80_FLAG_NEGATIVE)) | Z80_FLAG_HALF_CARRY;
				if (!(bTemp & 0x08))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x5f:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x60:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x61:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x62:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x63:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x64:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x65:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x66:
 		{
			sdwCyclesRemaining -= 20;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_ZERO | Z80_FLAG_NEGATIVE)) | Z80_FLAG_HALF_CARRY;
				if (!(bTemp & 0x10))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x67:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x68:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x69:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x6a:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x6b:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x6c:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x6d:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x6e:
 		{
			sdwCyclesRemaining -= 20;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_ZERO | Z80_FLAG_NEGATIVE)) | Z80_FLAG_HALF_CARRY;
				if (!(bTemp & 0x20))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x6f:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x70:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x71:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x72:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x73:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x74:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x75:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x76:
 		{
			sdwCyclesRemaining -= 20;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_ZERO | Z80_FLAG_NEGATIVE)) | Z80_FLAG_HALF_CARRY;
				if (!(bTemp & 0x40))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x77:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x78:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x79:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x7a:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x7b:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x7c:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x7d:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x7e:
 		{
			sdwCyclesRemaining -= 20;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_ZERO | Z80_FLAG_NEGATIVE)) | Z80_FLAG_HALF_CARRY;
				if (!(bTemp & 0x80))
				{
					cpu.z80F |= Z80_FLAG_ZERO;
				}
			break;
		}
 		case 0x7f:
 		{
			sdwCyclesRemaining -= 20;
				InvalidInstruction(4);
			break;
		}
 		case 0x80:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x81:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x82:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x83:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x84:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x85:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x86:
 		{
			sdwCyclesRemaining -= 23;
				bTemp &= 0xfe;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x87:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x88:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x89:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x8a:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x8b:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x8c:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x8d:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x8e:
 		{
			sdwCyclesRemaining -= 23;
				bTemp &= 0xfd;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x8f:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x90:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x91:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x92:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x93:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x94:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x95:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x96:
 		{
			sdwCyclesRemaining -= 23;
				bTemp &= 0xfb;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x97:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x98:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x99:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x9a:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x9b:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x9c:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x9d:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0x9e:
 		{
			sdwCyclesRemaining -= 23;
				bTemp &= 0xf7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x9f:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xa0:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xa1:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xa2:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xa3:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xa4:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xa5:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xa6:
 		{
			sdwCyclesRemaining -= 23;
				bTemp &= 0xef;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xa7:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xa8:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xa9:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xaa:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xab:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xac:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xad:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xae:
 		{
			sdwCyclesRemaining -= 23;
				bTemp &= 0xdf;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xaf:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xb0:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xb1:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xb2:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xb3:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xb4:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xb5:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xb6:
 		{
			sdwCyclesRemaining -= 23;
				bTemp &= 0xbf;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xb7:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xb8:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xb9:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xba:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xbb:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xbc:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xbd:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xbe:
 		{
			sdwCyclesRemaining -= 23;
				bTemp &= 0x7f;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xbf:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xc0:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xc1:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xc2:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xc3:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xc4:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xc5:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xc6:
 		{
			sdwCyclesRemaining -= 23;
				bTemp |= 0x01;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xc7:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xc8:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xc9:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xca:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xcb:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xcc:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xcd:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xce:
 		{
			sdwCyclesRemaining -= 23;
				bTemp |= 0x02;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xcf:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xd0:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xd1:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xd2:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xd3:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xd4:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xd5:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xd6:
 		{
			sdwCyclesRemaining -= 23;
				bTemp |= 0x04;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xd7:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xd8:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xd9:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xda:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xdb:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xdc:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xdd:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xde:
 		{
			sdwCyclesRemaining -= 23;
				bTemp |= 0x08;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xdf:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xe0:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xe1:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xe2:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xe3:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xe4:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xe5:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xe6:
 		{
			sdwCyclesRemaining -= 23;
				bTemp |= 0x10;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xe7:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xe8:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xe9:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xea:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xeb:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xec:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xed:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xee:
 		{
			sdwCyclesRemaining -= 23;
				bTemp |= 0x20;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xef:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xf0:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xf1:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xf2:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xf3:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xf4:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xf5:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xf6:
 		{
			sdwCyclesRemaining -= 23;
				bTemp |= 0x40;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xf7:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xf8:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xf9:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xfa:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xfb:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xfc:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xfd:
 		{
				InvalidInstruction(4);
			break;
		}
 		case 0xfe:
 		{
			sdwCyclesRemaining -= 23;
				bTemp |= 0x80;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0xff:
 		{
				InvalidInstruction(4);
			break;
		}
	}
}
void FDHandler(void)
{
	switch (*pbPC++)
	{
 		case 0x00:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x01:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x02:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x03:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x04:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x05:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x06:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x07:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x08:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x09:
 		{
			sdwCyclesRemaining -= 15;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80IY + cpu.z80BC;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80IY ^ dwTemp ^ cpu.z80BC) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80IY = dwTemp & 0xffff;
			break;
		}
 		case 0x0a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x0f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x10:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x11:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x12:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x13:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x14:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x15:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x16:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x17:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x18:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x19:
 		{
			sdwCyclesRemaining -= 15;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80IY + cpu.z80DE;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80IY ^ dwTemp ^ cpu.z80DE) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80IY = dwTemp & 0xffff;
			break;
		}
 		case 0x1a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x1f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x20:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x21:
 		{
			sdwCyclesRemaining -= 14;
		cpu.z80IY = *pbPC++;
		cpu.z80IY |= ((UINT32) *pbPC++ << 8);
			break;
		}
 		case 0x22:
 		{
			sdwCyclesRemaining -= 20;
				dwAddr = *pbPC++;
				dwAddr |= ((UINT32) *pbPC++ << 8);
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, (cpu.z80IY & 0xff), psMemWrite);
							psMemWrite->memoryCall(dwAddr + 1, (cpu.z80IY >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = cpu.z80IY;
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr) + 1) = cpu.z80IY >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) cpu.z80IY;
					cpu.z80Base[dwAddr + 1] = (UINT8) ((UINT32) cpu.z80IY >> 8);
				}

			break;
		}
 		case 0x23:
 		{
			sdwCyclesRemaining -= 10;
				cpu.z80IY++;
				cpu.z80IY &= 0xffff;
			break;
		}
 		case 0x24:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80YH++];
			break;
		}
 		case 0x25:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostDecFlags[cpu.z80YH--];
			break;
		}
 		case 0x26:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YH = *pbPC++;
			break;
		}
 		case 0x27:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x28:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x29:
 		{
			sdwCyclesRemaining -= 15;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80IY + cpu.z80IY;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80IY ^ dwTemp ^ cpu.z80HL) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80IY = dwTemp & 0xffff;
			break;
		}
 		case 0x2a:
 		{
			sdwCyclesRemaining -= 20;
				dwAddr = *pbPC++;
				dwAddr |= ((UINT32) *pbPC++ << 8);
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80IY = psMemRead->memoryCall(dwAddr, psMemRead);
							cpu.z80IY |= (UINT32) ((UINT32) psMemRead->memoryCall(dwAddr + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80IY = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
							cpu.z80IY |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80IY = cpu.z80Base[dwAddr];
					cpu.z80IY |= (UINT32) ((UINT32) cpu.z80Base[dwAddr + 1] << 8);
				}

			break;
		}
 		case 0x2b:
 		{
			sdwCyclesRemaining -= 10;
				cpu.z80IY--;
				cpu.z80IY &= 0xffff;
			break;
		}
 		case 0x2c:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80YL++];
			break;
		}
 		case 0x2d:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostDecFlags[cpu.z80YL--];
			break;
		}
 		case 0x2e:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YL = *pbPC++;
			break;
		}
 		case 0x2f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x30:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x31:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x32:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x33:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x34:
 		{
			sdwCyclesRemaining -= 23;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[bTemp++];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x35:
 		{
			sdwCyclesRemaining -= 23;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostDecFlags[bTemp--];
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemWrite->lowAddr) && (dwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwAddr, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwAddr - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwAddr] = (UINT8) bTemp;
				}

			break;
		}
 		case 0x36:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, *pbPC++, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = *pbPC++;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) *pbPC++;
				}

			break;
		}
 		case 0x37:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x38:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x39:
 		{
			sdwCyclesRemaining -= 15;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80IY + cpu.z80sp;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80IY ^ dwTemp ^ cpu.z80sp) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80IY = dwTemp & 0xffff;
			break;
		}
 		case 0x3a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3c:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3d:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3e:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x3f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x40:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x41:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x42:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x43:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x44:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80B = cpu.z80YH;
			break;
		}
 		case 0x45:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80B = cpu.z80YL;
			break;
		}
 		case 0x46:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80B = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80B = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80B = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x47:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x48:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x49:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x4a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x4b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x4c:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80C = cpu.z80YH;
			break;
		}
 		case 0x4d:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80C = cpu.z80YL;
			break;
		}
 		case 0x4e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80C = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80C = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80C = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x4f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x50:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x51:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x52:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x53:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x54:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80D = cpu.z80YH;
			break;
		}
 		case 0x55:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80D = cpu.z80YL;
			break;
		}
 		case 0x56:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80D = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80D = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80D = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x57:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x58:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x59:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x5a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x5b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x5c:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80E = cpu.z80YH;
			break;
		}
 		case 0x5d:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80E = cpu.z80YL;
			break;
		}
 		case 0x5e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80E = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80E = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80E = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x5f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x60:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YH = cpu.z80B;
			break;
		}
 		case 0x61:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YH = cpu.z80C;
			break;
		}
 		case 0x62:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YH = cpu.z80D;
			break;
		}
 		case 0x63:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YH = cpu.z80E;
			break;
		}
 		case 0x64:
 		{
			sdwCyclesRemaining -= 9;
			break;
		}
 		case 0x65:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YH = cpu.z80YL;
			break;
		}
 		case 0x66:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80H = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80H = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80H = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x67:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YH = cpu.z80A;
			break;
		}
 		case 0x68:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YL = cpu.z80B;
			break;
		}
 		case 0x69:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YL = cpu.z80C;
			break;
		}
 		case 0x6a:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YL = cpu.z80D;
			break;
		}
 		case 0x6b:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YL = cpu.z80E;
			break;
		}
 		case 0x6c:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YL = cpu.z80YH;
			break;
		}
 		case 0x6d:
 		{
			sdwCyclesRemaining -= 9;
			break;
		}
 		case 0x6e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80L = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80L = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80L = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x6f:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80YL = cpu.z80A;
			break;
		}
 		case 0x70:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80B, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80B;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80B;
				}

			break;
		}
 		case 0x71:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80C, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80C;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80C;
				}

			break;
		}
 		case 0x72:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80D, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80D;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80D;
				}

			break;
		}
 		case 0x73:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80E, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80E;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80E;
				}

			break;
		}
 		case 0x74:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80H, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80H;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80H;
				}

			break;
		}
 		case 0x75:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80L, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80L;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80L;
				}

			break;
		}
 		case 0x76:
 		{
			sdwCyclesRemaining -= 19;
				InvalidInstruction(2);
			break;
		}
 		case 0x77:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemWrite->lowAddr) && (sdwAddr <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(sdwAddr, cpu.z80A, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (sdwAddr - psMemWrite->lowAddr)) = cpu.z80A;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[sdwAddr] = (UINT8) cpu.z80A;
				}

			break;
		}
 		case 0x78:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x79:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x7a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x7b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x7c:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80A = cpu.z80YH;
			break;
		}
 		case 0x7d:
 		{
			sdwCyclesRemaining -= 9;
			cpu.z80A = cpu.z80YL;
			break;
		}
 		case 0x7e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	// Get the offset
				sdwAddr = ((INT32) cpu.z80IY + sdwAddr) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((sdwAddr >= psMemRead->lowAddr) && (sdwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80A = psMemRead->memoryCall(sdwAddr, psMemRead);
						}
						else
						{
							cpu.z80A = *((UINT8 *) psMemRead->pUserArea + (sdwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80A = cpu.z80Base[sdwAddr];
				}

			break;
		}
 		case 0x7f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x80:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x81:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x82:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x83:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x84:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A + cpu.z80YH;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80YH];
				InvalidInstruction(2);
			break;
		}
 		case 0x85:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A + cpu.z80YL;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80YL];
				InvalidInstruction(2);
			break;
		}
 		case 0x86:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | bTemp];
				cpu.z80A += bTemp;
			break;
		}
 		case 0x87:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x88:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x89:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x8c:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A + cpu.z80YH + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80YH | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				InvalidInstruction(2);
			break;
		}
 		case 0x8d:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A + cpu.z80YL + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80YL | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				InvalidInstruction(2);
			break;
		}
 		case 0x8e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				bTemp2 = (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | bTemp | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A += bTemp + bTemp2;
			break;
		}
 		case 0x8f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x90:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x91:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x92:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x93:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x94:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A - cpu.z80YH;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80YH];
				InvalidInstruction(2);
			break;
		}
 		case 0x95:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A - cpu.z80YL;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80YL];
				InvalidInstruction(2);
			break;
		}
 		case 0x96:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp];
				cpu.z80A -= bTemp;
			break;
		}
 		case 0x97:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x98:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x99:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9a:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9b:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0x9c:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A - cpu.z80YH - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80YH | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				InvalidInstruction(2);
			break;
		}
 		case 0x9d:
 		{
			sdwCyclesRemaining -= 9;
				bTemp2 = cpu.z80A - cpu.z80YL - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80YL | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				InvalidInstruction(2);
			break;
		}
 		case 0x9e:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				bTemp2 = cpu.z80A;
				cpu.z80A = cpu.z80A - bTemp - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) bTemp2 << 8) | bTemp | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
			break;
		}
 		case 0x9f:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa4:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A &= cpu.z80YH;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xa5:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A &= cpu.z80YL;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xa6:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80A &= bTemp;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

			break;
		}
 		case 0xa7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xa9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xaa:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xab:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xac:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A ^= cpu.z80YH;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xad:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A ^= cpu.z80YL;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xae:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80A ^= bTemp;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

			break;
		}
 		case 0xaf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb4:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A |= cpu.z80YH;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xb5:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80A |= cpu.z80YL;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xb6:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80A |= bTemp;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

			break;
		}
 		case 0xb7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xb9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xba:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xbb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xbc:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xbd:
 		{
			sdwCyclesRemaining -= 9;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				InvalidInstruction(2);
			break;
		}
 		case 0xbe:
 		{
			sdwCyclesRemaining -= 19;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				dwAddr = (sdwAddr + (INT32) cpu.z80IY) & 0xffff;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(dwAddr, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[dwAddr];
				}

				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp];
			break;
		}
 		case 0xbf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xc9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xca:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcb:
 		{
				DDFDCBHandler(1);
			break;
		}
 		case 0xcc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xce:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xcf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xd9:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xda:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xde:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xdf:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe1:
 		{
			sdwCyclesRemaining -= 14;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemRead->lowAddr) && (cpu.z80sp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80IY = psMemRead->memoryCall(cpu.z80sp, psMemRead);
							cpu.z80IY |= (UINT32) ((UINT32) psMemRead->memoryCall(cpu.z80sp + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80IY = *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr));
							cpu.z80IY |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80IY = cpu.z80Base[cpu.z80sp];
					cpu.z80IY |= (UINT32) ((UINT32) cpu.z80Base[cpu.z80sp + 1] << 8);
				}

					cpu.z80sp += 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
			break;
		}
 		case 0xe2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe3:
 		{
			sdwCyclesRemaining -= 23;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemRead->lowAddr) && (cpu.z80sp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							dwAddr = psMemRead->memoryCall(cpu.z80sp, psMemRead);
							dwAddr |= (UINT32) ((UINT32) psMemRead->memoryCall(cpu.z80sp + 1, psMemRead) << 8);
						}
						else
						{
							dwAddr = *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr));
							dwAddr |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					dwAddr = cpu.z80Base[cpu.z80sp];
					dwAddr |= (UINT32) ((UINT32) cpu.z80Base[cpu.z80sp + 1] << 8);
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemWrite->lowAddr) && (cpu.z80sp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80sp, (cpu.z80IY & 0xff), psMemWrite);
							psMemWrite->memoryCall(cpu.z80sp + 1, (cpu.z80IY >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr)) = cpu.z80IY;
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr) + 1) = cpu.z80IY >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80sp] = (UINT8) cpu.z80IY;
					cpu.z80Base[cpu.z80sp + 1] = (UINT8) ((UINT32) cpu.z80IY >> 8);
				}

				cpu.z80IY = dwAddr;
			break;
		}
 		case 0xe4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe5:
 		{
			sdwCyclesRemaining -= 15;
					cpu.z80sp -= 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemWrite->lowAddr) && (cpu.z80sp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80sp, (cpu.z80IY & 0xff), psMemWrite);
							psMemWrite->memoryCall(cpu.z80sp + 1, (cpu.z80IY >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr)) = cpu.z80IY;
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr) + 1) = cpu.z80IY >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80sp] = (UINT8) cpu.z80IY;
					cpu.z80Base[cpu.z80sp + 1] = (UINT8) ((UINT32) cpu.z80IY >> 8);
				}

			break;
		}
 		case 0xe6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xe9:
 		{
			sdwCyclesRemaining -= 8;
				pbPC = cpu.z80Base + cpu.z80IY;
			break;
		}
 		case 0xea:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xeb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xec:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xed:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xee:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xef:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf0:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf1:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf2:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf3:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf4:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf5:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf6:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf7:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf8:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xf9:
 		{
			sdwCyclesRemaining -= 10;
				cpu.z80sp = cpu.z80IY;
			break;
		}
 		case 0xfa:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfb:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfc:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfd:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xfe:
 		{
				InvalidInstruction(2);
			break;
		}
 		case 0xff:
 		{
				InvalidInstruction(2);
			break;
		}
	}
}
/* Main execution entry point */

UINT32 mz80exec(UINT32 dwCycles)
{
	UINT8 bOpcode;

	dwReturnCode = 0x80000000; /* Assume it'll work */
	sdwCyclesRemaining = dwCycles;
	dwOriginalCycles = dwCycles;
		if (cpu.z80halted)
		{
		dwElapsedTicks += dwCycles;
		return(0x80000000);
		}

	pbPC = cpu.z80Base + cpu.z80pc;

	while (sdwCyclesRemaining > 0)
	{
		bOpcode = *pbPC++;
		switch (bOpcode)
		{
			case 0x00:
			{
				sdwCyclesRemaining -= 4;
				/* Intentionally not doing anything - NOP! */
				break;
			}
			case 0x01:
			{
				sdwCyclesRemaining -= 10;
				cpu.z80BC = *pbPC++;	/* LSB First */
				cpu.z80BC |= (((UINT32) *pbPC++ << 8));	/* Now the MSB */
				break;
			}
			case 0x02:
			{
				sdwCyclesRemaining -= 7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80BC >= psMemWrite->lowAddr) && (cpu.z80BC <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80BC, cpu.z80A, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80BC - psMemWrite->lowAddr)) = cpu.z80A;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80BC] = (UINT8) cpu.z80A;
				}

				break;
			}
			case 0x03:
			{
				sdwCyclesRemaining -= 6;
				cpu.z80BC++;
				cpu.z80BC &= 0xffff;
				break;
			}
			case 0x04:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80B++];
				break;
			}
			case 0x05:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= bPostDecFlags[cpu.z80B--];
				break;
			}
			case 0x06:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80B = *pbPC++;	/* Get immediate byte into register */
				break;
			}
			case 0x07:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
				cpu.z80F |= (cpu.z80A >> 7);
				cpu.z80A = (cpu.z80A << 1) | (cpu.z80A >> 7);
				break;
			}
			case 0x08:
			{
				sdwCyclesRemaining -= 4;
				dwAddr = (UINT32) cpu.z80AF;
				cpu.z80AF = cpu.z80afprime;
				cpu.z80afprime = dwAddr;
				break;
			}
			case 0x09:
			{
				sdwCyclesRemaining -= 11;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80HL + cpu.z80BC;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80HL ^ dwTemp ^ cpu.z80BC) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80HL = dwTemp & 0xffff;
				break;
			}
			case 0x0a:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80BC >= psMemRead->lowAddr) && (cpu.z80BC <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80A = psMemRead->memoryCall(cpu.z80BC, psMemRead);
						}
						else
						{
							cpu.z80A = *((UINT8 *) psMemRead->pUserArea + (cpu.z80BC - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80A = cpu.z80Base[cpu.z80BC];
				}

				break;
			}
			case 0x0b:
			{
				sdwCyclesRemaining -= 6;
				cpu.z80BC--;
				cpu.z80BC &= 0xffff;
				break;
			}
			case 0x0c:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80C++];
				break;
			}
			case 0x0d:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= bPostDecFlags[cpu.z80C--];
				break;
			}
			case 0x0e:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80C = *pbPC++;	/* Get immediate byte into register */
				break;
			}
			case 0x0f:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
				cpu.z80F |= (cpu.z80A & Z80_FLAG_CARRY);
				cpu.z80A = (cpu.z80A >> 1) | (cpu.z80A << 7);
				break;
			}
			case 0x10:
			{
				sdwCyclesRemaining -= 8;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				if (--cpu.z80B)
				{
					dwElapsedTicks += 5;	/* 5 More for jump taken */
					cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
					sdwAddr = (sdwAddr + (INT32) cpu.z80pc) & 0xffff;
					pbPC = cpu.z80Base + sdwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0x11:
			{
				sdwCyclesRemaining -= 10;
				cpu.z80DE = *pbPC++;	/* LSB First */
				cpu.z80DE |= (((UINT32) *pbPC++ << 8));	/* Now the MSB */
				break;
			}
			case 0x12:
			{
				sdwCyclesRemaining -= 7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80DE >= psMemWrite->lowAddr) && (cpu.z80DE <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80DE, cpu.z80A, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80DE - psMemWrite->lowAddr)) = cpu.z80A;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80DE] = (UINT8) cpu.z80A;
				}

				break;
			}
			case 0x13:
			{
				sdwCyclesRemaining -= 6;
				cpu.z80DE++;
				cpu.z80DE &= 0xffff;
				break;
			}
			case 0x14:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80D++];
				break;
			}
			case 0x15:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= bPostDecFlags[cpu.z80D--];
				break;
			}
			case 0x16:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80D = *pbPC++;	/* Get immediate byte into register */
				break;
			}
			case 0x17:
			{
				sdwCyclesRemaining -= 4;
				bTemp = cpu.z80A >> 7;
				cpu.z80A = (cpu.z80A << 1) | (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY)) | bTemp;
				break;
			}
			case 0x18:
			{
				sdwCyclesRemaining -= 12;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				sdwAddr = (sdwAddr + (INT32) cpu.z80pc) & 0xffff;
				{
				sdwCyclesRemaining -= 5;
					pbPC = cpu.z80Base + sdwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0x19:
			{
				sdwCyclesRemaining -= 11;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80HL + cpu.z80DE;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80HL ^ dwTemp ^ cpu.z80DE) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80HL = dwTemp & 0xffff;
				break;
			}
			case 0x1a:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80DE >= psMemRead->lowAddr) && (cpu.z80DE <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80A = psMemRead->memoryCall(cpu.z80DE, psMemRead);
						}
						else
						{
							cpu.z80A = *((UINT8 *) psMemRead->pUserArea + (cpu.z80DE - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80A = cpu.z80Base[cpu.z80DE];
				}

				break;
			}
			case 0x1b:
			{
				sdwCyclesRemaining -= 6;
				cpu.z80DE--;
				cpu.z80DE &= 0xffff;
				break;
			}
			case 0x1c:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80E++];
				break;
			}
			case 0x1d:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= bPostDecFlags[cpu.z80E--];
				break;
			}
			case 0x1e:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80E = *pbPC++;	/* Get immediate byte into register */
				break;
			}
			case 0x1f:
			{
				sdwCyclesRemaining -= 4;
				bTemp = (cpu.z80F & Z80_FLAG_CARRY) << 7;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY)) | (cpu.z80A & Z80_FLAG_CARRY);
				cpu.z80A = ((cpu.z80A >> 1) | bTemp);
				break;
			}
			case 0x20:
			{
				sdwCyclesRemaining -= 7;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				sdwAddr = (sdwAddr + (INT32) cpu.z80pc) & 0xffff;
				if (!(cpu.z80F & Z80_FLAG_ZERO))
				{
				sdwCyclesRemaining -= 5;
					pbPC = cpu.z80Base + sdwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0x21:
			{
				sdwCyclesRemaining -= 10;
				cpu.z80HL = *pbPC++;	/* LSB First */
				cpu.z80HL |= (((UINT32) *pbPC++ << 8));	/* Now the MSB */
				break;
			}
			case 0x22:
			{
				sdwCyclesRemaining -= 16;
				dwTemp = *pbPC++;
				dwTemp |= ((UINT32) *pbPC++ << 8);
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemWrite->lowAddr) && (dwTemp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwTemp, (cpu.z80HL & 0xff), psMemWrite);
							psMemWrite->memoryCall(dwTemp + 1, (cpu.z80HL >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr)) = cpu.z80HL;
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr) + 1) = cpu.z80HL >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwTemp] = (UINT8) cpu.z80HL;
					cpu.z80Base[dwTemp + 1] = (UINT8) ((UINT32) cpu.z80HL >> 8);
				}

				break;
			}
			case 0x23:
			{
				sdwCyclesRemaining -= 6;
				cpu.z80HL++;
				cpu.z80HL &= 0xffff;
				break;
			}
			case 0x24:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80H++];
				break;
			}
			case 0x25:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= bPostDecFlags[cpu.z80H--];
				break;
			}
			case 0x26:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80H = *pbPC++;	/* Get immediate byte into register */
				break;
			}
			case 0x27:
			{
				sdwCyclesRemaining -= 4;
				dwAddr = (((cpu.z80F & Z80_FLAG_CARRY) | 
						((cpu.z80F & Z80_FLAG_HALF_CARRY) >> 3) | 
						((cpu.z80F & Z80_FLAG_NEGATIVE) << 1)) << 8) | cpu.z80A;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= (wDAATable[dwAddr] >> 8);
				cpu.z80A = wDAATable[dwAddr] & 0xff;
				break;
			}
			case 0x28:
			{
				sdwCyclesRemaining -= 7;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				sdwAddr = (sdwAddr + (INT32) cpu.z80pc) & 0xffff;
				if (cpu.z80F & Z80_FLAG_ZERO)
				{
				sdwCyclesRemaining -= 5;
					pbPC = cpu.z80Base + sdwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0x29:
			{
				sdwCyclesRemaining -= 11;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80HL + cpu.z80HL;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80HL ^ dwTemp ^ cpu.z80HL) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80HL = dwTemp & 0xffff;
				break;
			}
			case 0x2a:
			{
				sdwCyclesRemaining -= 16;
				dwAddr = *pbPC++;
				dwAddr |= ((UINT32) *pbPC++ << 8);
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwAddr >= psMemRead->lowAddr) && (dwAddr <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80HL = psMemRead->memoryCall(dwAddr, psMemRead);
							cpu.z80HL |= (UINT32) ((UINT32) psMemRead->memoryCall(dwAddr + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80HL = *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr));
							cpu.z80HL |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (dwAddr - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80HL = cpu.z80Base[dwAddr];
					cpu.z80HL |= (UINT32) ((UINT32) cpu.z80Base[dwAddr + 1] << 8);
				}

				break;
			}
			case 0x2b:
			{
				sdwCyclesRemaining -= 6;
				cpu.z80HL--;
				cpu.z80HL &= 0xffff;
				break;
			}
			case 0x2c:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80L++];
				break;
			}
			case 0x2d:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= bPostDecFlags[cpu.z80L--];
				break;
			}
			case 0x2e:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80L = *pbPC++;	/* Get immediate byte into register */
				break;
			}
			case 0x2f:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A ^= 0xff;
				cpu.z80F |= (Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
				break;
			}
			case 0x30:
			{
				sdwCyclesRemaining -= 7;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				sdwAddr = (sdwAddr + (INT32) cpu.z80pc) & 0xffff;
				if (!(cpu.z80F & Z80_FLAG_CARRY))
				{
				sdwCyclesRemaining -= 5;
					pbPC = cpu.z80Base + sdwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0x31:
			{
				sdwCyclesRemaining -= 10;
				cpu.z80sp = *pbPC++;	/* LSB First */
				cpu.z80sp |= (((UINT32) *pbPC++ << 8));	/* Now the MSB */
				break;
			}
			case 0x32:
			{
				sdwCyclesRemaining -= 13;
				dwTemp = *pbPC++;
				dwTemp |= ((UINT32) *pbPC++ << 8);
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemWrite->lowAddr) && (dwTemp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(dwTemp, cpu.z80A, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (dwTemp - psMemWrite->lowAddr)) = cpu.z80A;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[dwTemp] = (UINT8) cpu.z80A;
				}

				break;
			}
			case 0x33:
			{
				sdwCyclesRemaining -= 6;
				cpu.z80sp++;
				cpu.z80sp &= 0xffff;
				break;
			}
			case 0x34:
			{
				sdwCyclesRemaining -= 11;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[bTemp];
				bTemp++;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

				break;
			}
			case 0x35:
			{
				sdwCyclesRemaining -= 11;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostDecFlags[bTemp];
				bTemp--;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, bTemp, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = bTemp;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) bTemp;
				}

				break;
			}
			case 0x36:
			{
				sdwCyclesRemaining -= 10;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, *pbPC++, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = *pbPC++;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) *pbPC++;
				}

				break;
			}
			case 0x37:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= Z80_FLAG_CARRY;
				break;
			}
			case 0x38:
			{
				sdwCyclesRemaining -= 7;
				sdwAddr = (INT8) *pbPC++;	/* Get LSB first */
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				sdwAddr = (sdwAddr + (INT32) cpu.z80pc) & 0xffff;
				if (cpu.z80F & Z80_FLAG_CARRY)
				{
				sdwCyclesRemaining -= 5;
					pbPC = cpu.z80Base + sdwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0x39:
			{
				sdwCyclesRemaining -= 11;
			cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_HALF_CARRY);
			dwTemp = cpu.z80HL + cpu.z80sp;
			cpu.z80F |= ((dwTemp >> 16) & Z80_FLAG_CARRY) | (((cpu.z80HL ^ dwTemp ^ cpu.z80sp) >> 8) & Z80_FLAG_HALF_CARRY);
			cpu.z80HL = dwTemp & 0xffff;
				break;
			}
			case 0x3a:
			{
				sdwCyclesRemaining -= 13;
				dwTemp = *pbPC++;
				dwTemp |= (((UINT32) *pbPC++) << 8);
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((dwTemp >= psMemRead->lowAddr) && (dwTemp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80A = psMemRead->memoryCall(dwTemp, psMemRead);
						}
						else
						{
							cpu.z80A = *((UINT8 *) psMemRead->pUserArea + (dwTemp - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80A = cpu.z80Base[dwTemp];
				}

				break;
			}
			case 0x3b:
			{
				sdwCyclesRemaining -= 6;
				cpu.z80sp--;
				cpu.z80sp &= 0xffff;
				break;
			}
			case 0x3c:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_NEGATIVE);
				cpu.z80F |= bPostIncFlags[cpu.z80A++];
				break;
			}
			case 0x3d:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F &= ~(Z80_FLAG_SIGN | Z80_FLAG_ZERO | Z80_FLAG_HALF_CARRY | Z80_FLAG_OVERFLOW_PARITY);
				cpu.z80F |= bPostDecFlags[cpu.z80A--];
				break;
			}
			case 0x3e:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80A = *pbPC++;	/* Get immediate byte into register */
				break;
			}
			case 0x3f:
			{
				sdwCyclesRemaining -= 4;
				bTemp = (cpu.z80F & Z80_FLAG_CARRY) << 4;
				cpu.z80F &= ~(Z80_FLAG_HALF_CARRY | Z80_FLAG_NEGATIVE);
				cpu.z80F ^= Z80_FLAG_CARRY;
				break;
			}
			case 0x40:
			{
				sdwCyclesRemaining -= 4;
				break;
			}
			case 0x41:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80B = cpu.z80C;
				break;
			}
			case 0x42:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80B = cpu.z80D;
				break;
			}
			case 0x43:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80B = cpu.z80E;
				break;
			}
			case 0x44:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80B = cpu.z80H;
				break;
			}
			case 0x45:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80B = cpu.z80L;
				break;
			}
			case 0x46:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80B = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							cpu.z80B = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80B = cpu.z80Base[cpu.z80HL];
				}

				break;
			}
			case 0x47:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80B = cpu.z80A;
				break;
			}
			case 0x48:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80C = cpu.z80B;
				break;
			}
			case 0x49:
			{
				sdwCyclesRemaining -= 4;
				break;
			}
			case 0x4a:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80C = cpu.z80D;
				break;
			}
			case 0x4b:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80C = cpu.z80E;
				break;
			}
			case 0x4c:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80C = cpu.z80H;
				break;
			}
			case 0x4d:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80C = cpu.z80L;
				break;
			}
			case 0x4e:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80C = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							cpu.z80C = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80C = cpu.z80Base[cpu.z80HL];
				}

				break;
			}
			case 0x4f:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80C = cpu.z80A;
				break;
			}
			case 0x50:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80D = cpu.z80B;
				break;
			}
			case 0x51:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80D = cpu.z80C;
				break;
			}
			case 0x52:
			{
				sdwCyclesRemaining -= 4;
				break;
			}
			case 0x53:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80D = cpu.z80E;
				break;
			}
			case 0x54:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80D = cpu.z80H;
				break;
			}
			case 0x55:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80D = cpu.z80L;
				break;
			}
			case 0x56:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80D = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							cpu.z80D = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80D = cpu.z80Base[cpu.z80HL];
				}

				break;
			}
			case 0x57:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80D = cpu.z80A;
				break;
			}
			case 0x58:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80E = cpu.z80B;
				break;
			}
			case 0x59:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80E = cpu.z80C;
				break;
			}
			case 0x5a:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80E = cpu.z80D;
				break;
			}
			case 0x5b:
			{
				sdwCyclesRemaining -= 4;
				break;
			}
			case 0x5c:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80E = cpu.z80H;
				break;
			}
			case 0x5d:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80E = cpu.z80L;
				break;
			}
			case 0x5e:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80E = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							cpu.z80E = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80E = cpu.z80Base[cpu.z80HL];
				}

				break;
			}
			case 0x5f:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80E = cpu.z80A;
				break;
			}
			case 0x60:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80H = cpu.z80B;
				break;
			}
			case 0x61:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80H = cpu.z80C;
				break;
			}
			case 0x62:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80H = cpu.z80D;
				break;
			}
			case 0x63:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80H = cpu.z80E;
				break;
			}
			case 0x64:
			{
				sdwCyclesRemaining -= 4;
				break;
			}
			case 0x65:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80H = cpu.z80L;
				break;
			}
			case 0x66:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80H = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							cpu.z80H = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80H = cpu.z80Base[cpu.z80HL];
				}

				break;
			}
			case 0x67:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80H = cpu.z80A;
				break;
			}
			case 0x68:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80L = cpu.z80B;
				break;
			}
			case 0x69:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80L = cpu.z80C;
				break;
			}
			case 0x6a:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80L = cpu.z80D;
				break;
			}
			case 0x6b:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80L = cpu.z80E;
				break;
			}
			case 0x6c:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80L = cpu.z80H;
				break;
			}
			case 0x6d:
			{
				sdwCyclesRemaining -= 4;
				break;
			}
			case 0x6e:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80L = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							cpu.z80L = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80L = cpu.z80Base[cpu.z80HL];
				}

				break;
			}
			case 0x6f:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80L = cpu.z80A;
				break;
			}
			case 0x70:
			{
				sdwCyclesRemaining -= 7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, cpu.z80B, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = cpu.z80B;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) cpu.z80B;
				}

				break;
			}
			case 0x71:
			{
				sdwCyclesRemaining -= 7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, cpu.z80C, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = cpu.z80C;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) cpu.z80C;
				}

				break;
			}
			case 0x72:
			{
				sdwCyclesRemaining -= 7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, cpu.z80D, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = cpu.z80D;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) cpu.z80D;
				}

				break;
			}
			case 0x73:
			{
				sdwCyclesRemaining -= 7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, cpu.z80E, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = cpu.z80E;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) cpu.z80E;
				}

				break;
			}
			case 0x74:
			{
				sdwCyclesRemaining -= 7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, cpu.z80H, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = cpu.z80H;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) cpu.z80H;
				}

				break;
			}
			case 0x75:
			{
				sdwCyclesRemaining -= 7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, cpu.z80L, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = cpu.z80L;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) cpu.z80L;
				}

				break;
			}
			case 0x76:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80halted = 1;
				dwElapsedTicks += sdwCyclesRemaining;
				sdwCyclesRemaining = 0;
				break;
			}
			case 0x77:
			{
				sdwCyclesRemaining -= 7;
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemWrite->lowAddr) && (cpu.z80HL <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80HL, cpu.z80A, psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80HL - psMemWrite->lowAddr)) = cpu.z80A;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80HL] = (UINT8) cpu.z80A;
				}

				break;
			}
			case 0x78:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A = cpu.z80B;
				break;
			}
			case 0x79:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A = cpu.z80C;
				break;
			}
			case 0x7a:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A = cpu.z80D;
				break;
			}
			case 0x7b:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A = cpu.z80E;
				break;
			}
			case 0x7c:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A = cpu.z80H;
				break;
			}
			case 0x7d:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A = cpu.z80L;
				break;
			}
			case 0x7e:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80A = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							cpu.z80A = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80A = cpu.z80Base[cpu.z80HL];
				}

				break;
			}
			case 0x7f:
			{
				sdwCyclesRemaining -= 4;
				break;
			}
			case 0x80:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80B;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80B];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x81:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80C;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80C];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x82:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80D;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80D];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x83:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80E;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80E];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x84:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80H;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80H];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x85:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80L;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80L];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x86:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp2 = cpu.z80A + bTemp;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | bTemp];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x87:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80A;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80A];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x88:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80B + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80B | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x89:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80C + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80C | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x8a:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80D + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80D | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x8b:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80E + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80E | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x8c:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80H + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80H | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x8d:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80L + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80L | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x8e:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp2 = cpu.z80A + bTemp + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | bTemp | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x8f:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A + cpu.z80A + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | cpu.z80A | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x90:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80B;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80B];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x91:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80C;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80C];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x92:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80D;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80D];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x93:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80E;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80E];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x94:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80H;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80H];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x95:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80L;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80L];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x96:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp2 = cpu.z80A - bTemp;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x97:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80A;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80A];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x98:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80B - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80B | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x99:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80C - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80C | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x9a:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80D - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80D | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x9b:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80E - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80E | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x9c:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80H - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80H | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x9d:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80L - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80L | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x9e:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				bTemp2 = cpu.z80A - bTemp - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0x9f:
			{
				sdwCyclesRemaining -= 4;
				bTemp2 = cpu.z80A - cpu.z80A - (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80A | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = bTemp2;
				break;
			}
			case 0xa0:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A &= cpu.z80B;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				break;
			}
			case 0xa1:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A &= cpu.z80C;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				break;
			}
			case 0xa2:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A &= cpu.z80D;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				break;
			}
			case 0xa3:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A &= cpu.z80E;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				break;
			}
			case 0xa4:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A &= cpu.z80H;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				break;
			}
			case 0xa5:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A &= cpu.z80L;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				break;
			}
			case 0xa6:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80A &= bTemp;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				break;
			}
			case 0xa7:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A &= cpu.z80A;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				break;
			}
			case 0xa8:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A ^= cpu.z80B;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xa9:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A ^= cpu.z80C;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xaa:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A ^= cpu.z80D;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xab:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A ^= cpu.z80E;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xac:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A ^= cpu.z80H;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xad:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A ^= cpu.z80L;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xae:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80A ^= bTemp;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xaf:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A ^= cpu.z80A;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xb0:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A |= cpu.z80B;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xb1:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A |= cpu.z80C;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xb2:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A |= cpu.z80D;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xb3:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A |= cpu.z80E;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xb4:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A |= cpu.z80H;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xb5:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A |= cpu.z80L;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xb6:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80A |= bTemp;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xb7:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80A |= cpu.z80A;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xb8:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80B];
				break;
			}
			case 0xb9:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80C];
				break;
			}
			case 0xba:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80D];
				break;
			}
			case 0xbb:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80E];
				break;
			}
			case 0xbc:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80H];
				break;
			}
			case 0xbd:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80L];
				break;
			}
			case 0xbe:
			{
				sdwCyclesRemaining -= 7;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80HL >= psMemRead->lowAddr) && (cpu.z80HL <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							bTemp = psMemRead->memoryCall(cpu.z80HL, psMemRead);
						}
						else
						{
							bTemp = *((UINT8 *) psMemRead->pUserArea + (cpu.z80HL - psMemRead->lowAddr));
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					bTemp = cpu.z80Base[cpu.z80HL];
				}

				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp];
				break;
			}
			case 0xbf:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | cpu.z80A];
				break;
			}
			case 0xc0:
			{
				sdwCyclesRemaining -= 5;
				if (!(cpu.z80F & Z80_FLAG_ZERO))
				{
					dwElapsedTicks += 6;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				}
				break;
			}
			case 0xc1:
			{
				sdwCyclesRemaining -= 10;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemRead->lowAddr) && (cpu.z80sp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80BC = psMemRead->memoryCall(cpu.z80sp, psMemRead);
							cpu.z80BC |= (UINT32) ((UINT32) psMemRead->memoryCall(cpu.z80sp + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80BC = *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr));
							cpu.z80BC |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80BC = cpu.z80Base[cpu.z80sp];
					cpu.z80BC |= (UINT32) ((UINT32) cpu.z80Base[cpu.z80sp + 1] << 8);
				}

					cpu.z80sp += 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				break;
			}
			case 0xc2:
			{
				sdwCyclesRemaining -= 10;
					dwAddr = *pbPC++;	/* Get LSB first */
					dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (!(cpu.z80F & Z80_FLAG_ZERO))
				{
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xc3:
			{
				sdwCyclesRemaining -= 10;
					dwAddr = *pbPC++;	/* Get LSB first */
					dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				break;
			}
			case 0xc4:
			{
				sdwCyclesRemaining -= 10;
				dwAddr = *pbPC++;	/* Get LSB first */
				dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (!(cpu.z80F & Z80_FLAG_ZERO))
				{
					cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
					pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
					*pbSP-- = cpu.z80pc >> 8;	/* MSB */
					*pbSP = (UINT8) cpu.z80pc;	/* LSB */
					cpu.z80sp -= 2;	/* Back our stack up */
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xc5:
			{
				sdwCyclesRemaining -= 11;
					cpu.z80sp -= 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemWrite->lowAddr) && (cpu.z80sp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80sp, (cpu.z80BC & 0xff), psMemWrite);
							psMemWrite->memoryCall(cpu.z80sp + 1, (cpu.z80BC >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr)) = cpu.z80BC;
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr) + 1) = cpu.z80BC >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80sp] = (UINT8) cpu.z80BC;
					cpu.z80Base[cpu.z80sp + 1] = (UINT8) ((UINT32) cpu.z80BC >> 8);
				}

				break;
			}
			case 0xc6:
			{
				sdwCyclesRemaining -= 7;
				bTemp = *pbPC++;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | bTemp];
				cpu.z80A += bTemp;
				break;
			}
			case 0xc7:
			{
				sdwCyclesRemaining -= 11;
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
				*pbSP-- = cpu.z80pc >> 8;	/* LSB */
				*pbSP = (UINT8) cpu.z80pc;	/* MSB */
				cpu.z80sp -= 2;	/* Back our stack up */
				pbPC = cpu.z80Base + 0x00;	/* Normalize the address */
				break;
			}
			case 0xc8:
			{
				sdwCyclesRemaining -= 5;
				if (cpu.z80F & Z80_FLAG_ZERO)
				{
					dwElapsedTicks += 6;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				}
				break;
			}
			case 0xc9:
			{
				sdwCyclesRemaining -= 10;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				break;
			}
			case 0xca:
			{
				sdwCyclesRemaining -= 10;
					dwAddr = *pbPC++;	/* Get LSB first */
					dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (cpu.z80F & Z80_FLAG_ZERO)
				{
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xcb:
			{
				CBHandler();
				break;
			}
			case 0xcc:
			{
				sdwCyclesRemaining -= 10;
				dwAddr = *pbPC++;	/* Get LSB first */
				dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (cpu.z80F & Z80_FLAG_ZERO)
				{
					cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
					pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
					*pbSP-- = cpu.z80pc >> 8;	/* MSB */
					*pbSP = (UINT8) cpu.z80pc;	/* LSB */
					cpu.z80sp -= 2;	/* Back our stack up */
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xcd:
			{
				sdwCyclesRemaining -= 17;
				dwAddr = *pbPC++;	/* Get LSB first */
				dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
				*pbSP-- = cpu.z80pc >> 8;	/* LSB */
				*pbSP = (UINT8) cpu.z80pc;	/* MSB */
				cpu.z80sp -= 2;	/* Back our stack up */
				pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				break;
			}
			case 0xce:
			{
				sdwCyclesRemaining -= 7;
				bTemp = *pbPC++ + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbAddAdcTable[((UINT32) cpu.z80A << 8) | bTemp | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A += bTemp;
				break;
			}
			case 0xcf:
			{
				sdwCyclesRemaining -= 11;
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
				*pbSP-- = cpu.z80pc >> 8;	/* LSB */
				*pbSP = (UINT8) cpu.z80pc;	/* MSB */
				cpu.z80sp -= 2;	/* Back our stack up */
				pbPC = cpu.z80Base + 0x08;	/* Normalize the address */
				break;
			}
			case 0xd0:
			{
				sdwCyclesRemaining -= 5;
				if (!(cpu.z80F & Z80_FLAG_CARRY))
				{
					dwElapsedTicks += 6;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				}
				break;
			}
			case 0xd1:
			{
				sdwCyclesRemaining -= 10;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemRead->lowAddr) && (cpu.z80sp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80DE = psMemRead->memoryCall(cpu.z80sp, psMemRead);
							cpu.z80DE |= (UINT32) ((UINT32) psMemRead->memoryCall(cpu.z80sp + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80DE = *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr));
							cpu.z80DE |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80DE = cpu.z80Base[cpu.z80sp];
					cpu.z80DE |= (UINT32) ((UINT32) cpu.z80Base[cpu.z80sp + 1] << 8);
				}

					cpu.z80sp += 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				break;
			}
			case 0xd2:
			{
				sdwCyclesRemaining -= 10;
					dwAddr = *pbPC++;	/* Get LSB first */
					dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (!(cpu.z80F & Z80_FLAG_CARRY))
				{
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xd3:
			{
				sdwCyclesRemaining -= 11;
			dwTemp = *pbPC++;
				psIoWrite = cpu.z80IoWrite;	/* Beginning of our handler */
				while (psIoWrite->lowIoAddr != 0xffff)
				{
					if ((dwTemp >= psIoWrite->lowIoAddr) && (dwTemp <= psIoWrite->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						psIoWrite->IOCall(dwTemp, cpu.z80A, psIoWrite);
						psIoWrite = NULL;
						break;
					}
					++psIoWrite;
				}

				break;
			}
			case 0xd4:
			{
				sdwCyclesRemaining -= 10;
				dwAddr = *pbPC++;	/* Get LSB first */
				dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (!(cpu.z80F & Z80_FLAG_CARRY))
				{
					cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
					pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
					*pbSP-- = cpu.z80pc >> 8;	/* MSB */
					*pbSP = (UINT8) cpu.z80pc;	/* LSB */
					cpu.z80sp -= 2;	/* Back our stack up */
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xd5:
			{
				sdwCyclesRemaining -= 11;
					cpu.z80sp -= 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemWrite->lowAddr) && (cpu.z80sp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80sp, (cpu.z80DE & 0xff), psMemWrite);
							psMemWrite->memoryCall(cpu.z80sp + 1, (cpu.z80DE >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr)) = cpu.z80DE;
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr) + 1) = cpu.z80DE >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80sp] = (UINT8) cpu.z80DE;
					cpu.z80Base[cpu.z80sp + 1] = (UINT8) ((UINT32) cpu.z80DE >> 8);
				}

				break;
			}
			case 0xd6:
			{
				sdwCyclesRemaining -= 7;
				bTemp = *pbPC++;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp];
				cpu.z80A -= bTemp;
				break;
			}
			case 0xd7:
			{
				sdwCyclesRemaining -= 11;
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
				*pbSP-- = cpu.z80pc >> 8;	/* LSB */
				*pbSP = (UINT8) cpu.z80pc;	/* MSB */
				cpu.z80sp -= 2;	/* Back our stack up */
				pbPC = cpu.z80Base + 0x10;	/* Normalize the address */
				break;
			}
			case 0xd8:
			{
				sdwCyclesRemaining -= 5;
				if (cpu.z80F & Z80_FLAG_CARRY)
				{
					dwElapsedTicks += 6;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				}
				break;
			}
			case 0xd9:
			{
				sdwCyclesRemaining -= 4;
				dwTemp = cpu.z80DE;
				cpu.z80DE = cpu.z80deprime;
				cpu.z80deprime = dwTemp;
				dwTemp = cpu.z80BC;
				cpu.z80BC = cpu.z80bcprime;
				cpu.z80bcprime = dwTemp;
				dwTemp = cpu.z80HL;
				cpu.z80HL = cpu.z80hlprime;
				cpu.z80hlprime = dwTemp;
				break;
			}
			case 0xda:
			{
				sdwCyclesRemaining -= 10;
					dwAddr = *pbPC++;	/* Get LSB first */
					dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (cpu.z80F & Z80_FLAG_CARRY)
				{
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xdb:
			{
				sdwCyclesRemaining -= 11;
			dwTemp = *pbPC++;
				psIoRead = cpu.z80IoRead;	/* Beginning of our handler */
				while (psIoRead->lowIoAddr != 0xffff)
				{
					if ((dwTemp >= psIoRead->lowIoAddr) && (dwTemp <= psIoRead->highIoAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						cpu.z80A = psIoRead->IOCall(dwTemp, psIoRead);
						psIoRead = NULL;
						break;
					}
					++psIoRead;
				}

				if (psIoRead)
				{
					cpu.z80A = 0xff; /* Unclaimed I/O read */
				}

				break;
			}
			case 0xdc:
			{
				sdwCyclesRemaining -= 10;
				dwAddr = *pbPC++;	/* Get LSB first */
				dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (cpu.z80F & Z80_FLAG_CARRY)
				{
					cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
					pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
					*pbSP-- = cpu.z80pc >> 8;	/* MSB */
					*pbSP = (UINT8) cpu.z80pc;	/* LSB */
					cpu.z80sp -= 2;	/* Back our stack up */
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xdd:
			{
				DDHandler();
				break;
			}
			case 0xde:
			{
				sdwCyclesRemaining -= 7;
				bTemp = *pbPC++ + (cpu.z80F & Z80_FLAG_CARRY);
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | bTemp | (((UINT32) cpu.z80F & Z80_FLAG_CARRY) << 16)];
				cpu.z80A = cpu.z80A - bTemp;
				break;
			}
			case 0xdf:
			{
				sdwCyclesRemaining -= 11;
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
				*pbSP-- = cpu.z80pc >> 8;	/* LSB */
				*pbSP = (UINT8) cpu.z80pc;	/* MSB */
				cpu.z80sp -= 2;	/* Back our stack up */
				pbPC = cpu.z80Base + 0x18;	/* Normalize the address */
				break;
			}
			case 0xe0:
			{
				sdwCyclesRemaining -= 5;
				if (!(cpu.z80F & Z80_FLAG_OVERFLOW_PARITY))
				{
					dwElapsedTicks += 6;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				}
				break;
			}
			case 0xe1:
			{
				sdwCyclesRemaining -= 10;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemRead->lowAddr) && (cpu.z80sp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80HL = psMemRead->memoryCall(cpu.z80sp, psMemRead);
							cpu.z80HL |= (UINT32) ((UINT32) psMemRead->memoryCall(cpu.z80sp + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80HL = *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr));
							cpu.z80HL |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80HL = cpu.z80Base[cpu.z80sp];
					cpu.z80HL |= (UINT32) ((UINT32) cpu.z80Base[cpu.z80sp + 1] << 8);
				}

					cpu.z80sp += 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				break;
			}
			case 0xe2:
			{
				sdwCyclesRemaining -= 10;
					dwAddr = *pbPC++;	/* Get LSB first */
					dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (!(cpu.z80F & Z80_FLAG_OVERFLOW_PARITY))
				{
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xe3:
			{
				sdwCyclesRemaining -= 19;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemRead->lowAddr) && (cpu.z80sp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							dwAddr = psMemRead->memoryCall(cpu.z80sp, psMemRead);
							dwAddr |= (UINT32) ((UINT32) psMemRead->memoryCall(cpu.z80sp + 1, psMemRead) << 8);
						}
						else
						{
							dwAddr = *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr));
							dwAddr |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					dwAddr = cpu.z80Base[cpu.z80sp];
					dwAddr |= (UINT32) ((UINT32) cpu.z80Base[cpu.z80sp + 1] << 8);
				}

				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemWrite->lowAddr) && (cpu.z80sp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80sp, (cpu.z80HL & 0xff), psMemWrite);
							psMemWrite->memoryCall(cpu.z80sp + 1, (cpu.z80HL >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr)) = cpu.z80HL;
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr) + 1) = cpu.z80HL >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80sp] = (UINT8) cpu.z80HL;
					cpu.z80Base[cpu.z80sp + 1] = (UINT8) ((UINT32) cpu.z80HL >> 8);
				}

				cpu.z80HL = dwAddr;
				break;
			}
			case 0xe4:
			{
				sdwCyclesRemaining -= 10;
				dwAddr = *pbPC++;	/* Get LSB first */
				dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (!(cpu.z80F & Z80_FLAG_OVERFLOW_PARITY))
				{
					cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
					pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
					*pbSP-- = cpu.z80pc >> 8;	/* MSB */
					*pbSP = (UINT8) cpu.z80pc;	/* LSB */
					cpu.z80sp -= 2;	/* Back our stack up */
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xe5:
			{
				sdwCyclesRemaining -= 11;
					cpu.z80sp -= 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemWrite->lowAddr) && (cpu.z80sp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80sp, (cpu.z80HL & 0xff), psMemWrite);
							psMemWrite->memoryCall(cpu.z80sp + 1, (cpu.z80HL >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr)) = cpu.z80HL;
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr) + 1) = cpu.z80HL >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80sp] = (UINT8) cpu.z80HL;
					cpu.z80Base[cpu.z80sp + 1] = (UINT8) ((UINT32) cpu.z80HL >> 8);
				}

				break;
			}
			case 0xe6:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80A &= *pbPC++;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostANDFlags[cpu.z80A];

				break;
			}
			case 0xe7:
			{
				sdwCyclesRemaining -= 11;
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
				*pbSP-- = cpu.z80pc >> 8;	/* LSB */
				*pbSP = (UINT8) cpu.z80pc;	/* MSB */
				cpu.z80sp -= 2;	/* Back our stack up */
				pbPC = cpu.z80Base + 0x20;	/* Normalize the address */
				break;
			}
			case 0xe8:
			{
				sdwCyclesRemaining -= 5;
				if (cpu.z80F & Z80_FLAG_OVERFLOW_PARITY)
				{
					dwElapsedTicks += 6;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				}
				break;
			}
			case 0xe9:
			{
				sdwCyclesRemaining -= 4;
				pbPC = cpu.z80Base + cpu.z80HL;
				break;
			}
			case 0xea:
			{
				sdwCyclesRemaining -= 10;
					dwAddr = *pbPC++;	/* Get LSB first */
					dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (cpu.z80F & Z80_FLAG_OVERFLOW_PARITY)
				{
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xeb:
			{
				sdwCyclesRemaining -= 4;
				dwAddr = cpu.z80DE;
				cpu.z80DE = cpu.z80HL;
				cpu.z80HL = dwAddr;
				break;
			}
			case 0xec:
			{
				sdwCyclesRemaining -= 10;
				dwAddr = *pbPC++;	/* Get LSB first */
				dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (cpu.z80F & Z80_FLAG_OVERFLOW_PARITY)
				{
					cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
					pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
					*pbSP-- = cpu.z80pc >> 8;	/* MSB */
					*pbSP = (UINT8) cpu.z80pc;	/* LSB */
					cpu.z80sp -= 2;	/* Back our stack up */
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xed:
			{
				EDHandler();
				break;
			}
			case 0xee:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80A ^= *pbPC++;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xef:
			{
				sdwCyclesRemaining -= 11;
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
				*pbSP-- = cpu.z80pc >> 8;	/* LSB */
				*pbSP = (UINT8) cpu.z80pc;	/* MSB */
				cpu.z80sp -= 2;	/* Back our stack up */
				pbPC = cpu.z80Base + 0x28;	/* Normalize the address */
				break;
			}
			case 0xf0:
			{
				sdwCyclesRemaining -= 5;
				if (!(cpu.z80F & Z80_FLAG_SIGN))
				{
					dwElapsedTicks += 6;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				}
				break;
			}
			case 0xf1:
			{
				sdwCyclesRemaining -= 10;
				psMemRead = cpu.z80MemRead;	/* Beginning of our handler */
				while (psMemRead->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemRead->lowAddr) && (cpu.z80sp <= psMemRead->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemRead->memoryCall)
						{
							cpu.z80AF = psMemRead->memoryCall(cpu.z80sp, psMemRead);
							cpu.z80AF |= (UINT32) ((UINT32) psMemRead->memoryCall(cpu.z80sp + 1, psMemRead) << 8);
						}
						else
						{
							cpu.z80AF = *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr));
							cpu.z80AF |= (UINT32) ((UINT32) *((UINT8 *) psMemRead->pUserArea + (cpu.z80sp - psMemRead->lowAddr + 1)) << 8);
						}
						psMemRead = NULL;
						break;
					}
					++psMemRead;
				}

				if (psMemRead)
				{
					cpu.z80AF = cpu.z80Base[cpu.z80sp];
					cpu.z80AF |= (UINT32) ((UINT32) cpu.z80Base[cpu.z80sp + 1] << 8);
				}

					cpu.z80sp += 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				break;
			}
			case 0xf2:
			{
				sdwCyclesRemaining -= 10;
					dwAddr = *pbPC++;	/* Get LSB first */
					dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (!(cpu.z80F & Z80_FLAG_SIGN))
				{
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xf3:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80iff &= (~IFF1);
				break;
			}
			case 0xf4:
			{
				sdwCyclesRemaining -= 10;
				dwAddr = *pbPC++;	/* Get LSB first */
				dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (!(cpu.z80F & Z80_FLAG_SIGN))
				{
					cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
					pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
					*pbSP-- = cpu.z80pc >> 8;	/* MSB */
					*pbSP = (UINT8) cpu.z80pc;	/* LSB */
					cpu.z80sp -= 2;	/* Back our stack up */
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xf5:
			{
				sdwCyclesRemaining -= 11;
					cpu.z80sp -= 2;
					pbSP = (cpu.z80Base + cpu.z80sp);	/* Normalize the stack pointer */
				psMemWrite = cpu.z80MemWrite;	/* Beginning of our handler */
				while (psMemWrite->lowAddr != 0xffffffff)
				{
					if ((cpu.z80sp >= psMemWrite->lowAddr) && (cpu.z80sp <= psMemWrite->highAddr))
					{
						cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
						if (psMemWrite->memoryCall)
						{
							psMemWrite->memoryCall(cpu.z80sp, (cpu.z80AF & 0xff), psMemWrite);
							psMemWrite->memoryCall(cpu.z80sp + 1, (cpu.z80AF >> 8), psMemWrite);
						}
						else
						{
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr)) = cpu.z80AF;
							*((UINT8 *) psMemWrite->pUserArea + (cpu.z80sp - psMemWrite->lowAddr) + 1) = cpu.z80AF >> 8;
						}
						psMemWrite = NULL;
						break;
					}
					++psMemWrite;
				}

				if (psMemWrite)
				{
					cpu.z80Base[cpu.z80sp] = (UINT8) cpu.z80AF;
					cpu.z80Base[cpu.z80sp + 1] = (UINT8) ((UINT32) cpu.z80AF >> 8);
				}

				break;
			}
			case 0xf6:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80A |= *pbPC++;
				cpu.z80F &= ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN);
				cpu.z80F |= bPostORFlags[cpu.z80A];

				break;
			}
			case 0xf7:
			{
				sdwCyclesRemaining -= 11;
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
				*pbSP-- = cpu.z80pc >> 8;	/* LSB */
				*pbSP = (UINT8) cpu.z80pc;	/* MSB */
				cpu.z80sp -= 2;	/* Back our stack up */
				pbPC = cpu.z80Base + 0x30;	/* Normalize the address */
				break;
			}
			case 0xf8:
			{
				sdwCyclesRemaining -= 5;
				if (cpu.z80F & Z80_FLAG_SIGN)
				{
					dwElapsedTicks += 6;
				pbSP = cpu.z80Base + cpu.z80sp;	/* Normalize our stack PTR */
				dwAddr = *pbSP++;	/* Pop LSB */
				dwAddr |= ((UINT32) *pbSP << 8);	/* Pop MSB */
				cpu.z80sp += 2;	/* Pop the word off */
				pbPC = (cpu.z80Base + dwAddr);	/* Point PC to our return address */
				}
				break;
			}
			case 0xf9:
			{
				sdwCyclesRemaining -= 6;
				cpu.z80sp = cpu.z80HL;
				break;
			}
			case 0xfa:
			{
				sdwCyclesRemaining -= 10;
					dwAddr = *pbPC++;	/* Get LSB first */
					dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (cpu.z80F & Z80_FLAG_SIGN)
				{
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xfb:
			{
				sdwCyclesRemaining -= 4;
				cpu.z80iff |= IFF1;
				break;
			}
			case 0xfc:
			{
				sdwCyclesRemaining -= 10;
				dwAddr = *pbPC++;	/* Get LSB first */
				dwAddr |= ((UINT32) *pbPC++ << 8); /* Get MSB last */
				if (cpu.z80F & Z80_FLAG_SIGN)
				{
					cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
					pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
					*pbSP-- = cpu.z80pc >> 8;	/* MSB */
					*pbSP = (UINT8) cpu.z80pc;	/* LSB */
					cpu.z80sp -= 2;	/* Back our stack up */
					pbPC = cpu.z80Base + dwAddr;	/* Normalize the address */
				}
				break;
			}
			case 0xfd:
			{
				FDHandler();
				break;
			}
			case 0xfe:
			{
				sdwCyclesRemaining -= 7;
				cpu.z80F = (cpu.z80F & ~(Z80_FLAG_CARRY | Z80_FLAG_NEGATIVE | Z80_FLAG_OVERFLOW_PARITY | 
							   Z80_FLAG_HALF_CARRY | Z80_FLAG_ZERO | Z80_FLAG_SIGN)) |
								pbSubSbcTable[((UINT32) cpu.z80A << 8) | *pbPC++];
				break;
			}
			case 0xff:
			{
				sdwCyclesRemaining -= 11;
				cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
				pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
				*pbSP-- = cpu.z80pc >> 8;	/* LSB */
				*pbSP = (UINT8) cpu.z80pc;	/* MSB */
				cpu.z80sp -= 2;	/* Back our stack up */
				pbPC = cpu.z80Base + 0x38;	/* Normalize the address */
				break;
			}
		}
	}

	dwElapsedTicks += (dwOriginalCycles - sdwCyclesRemaining);

	cpu.z80pc = (UINT32) pbPC - (UINT32) cpu.z80Base;
	return(dwReturnCode); /* Indicate success */
}

/* Get mz80's context */

void mz80GetContext(void *pData)
{
	memcpy(pData, &cpu, sizeof(CONTEXTMZ80));
}

/* Set mz80's context */

void mz80SetContext(void *pData)
{
	memcpy(&cpu, pData, sizeof(CONTEXTMZ80));
}

/* Get mz80's context size */

UINT32 mz80GetContextSize(void)
{
	return(sizeof(CONTEXTMZ80));
}

/* This will return the elapsed ticks */

UINT32 mz80GetElapsedTicks(UINT32 dwClear)
{
	UINT32 dwTemp = dwElapsedTicks;

	if (dwClear)
	{
		dwElapsedTicks = 0;
	}

	return(dwTemp);
}

/* Releases mz80 from its current timeslice */

void mz80ReleaseTimeslice(void)
{
	dwOriginalCycles -= sdwCyclesRemaining;
	sdwCyclesRemaining = 0;
}

/* This routine is mz80's reset handler */

void mz80reset(void)
{
	cpu.z80halted = 0;
	cpu.z80AF = 0;
	cpu.z80F = Z80_FLAG_ZERO;
	cpu.z80BC = 0;
	cpu.z80DE = 0;
	cpu.z80HL = 0;
	cpu.z80afprime = 0;
	cpu.z80bcprime = 0;
	cpu.z80deprime = 0;
	cpu.z80hlprime = 0;
	cpu.z80i = 0;
	cpu.z80r = 0;
	cpu.z80IX = 0xffff; /* Yes, this is intentional */
	cpu.z80IY = 0xffff; /* Yes, this is intentional */
	cpu.z80pc = 0;
	cpu.z80sp = 0;
	cpu.z80interruptMode = 0;
	cpu.z80intAddr = 0x38;
	cpu.z80nmiAddr = 0x66;
}

/* Interrupt handler */

UINT32 mz80int(UINT32 dwLowAddr)
{
	cpu.z80halted = 0;
	if (0 == (cpu.z80iff & IFF1))
		return(0xffffffff);
	cpu.z80iff &= ~(IFF1 | IFF2);
	pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
	*pbSP-- = cpu.z80pc >> 8;	/* LSB */
	*pbSP = (UINT8) cpu.z80pc;	/* MSB */
	cpu.z80sp -= 2;	/* Back our stack up */
	if (2 == cpu.z80interruptMode)
	{
		cpu.z80pc = ((UINT16) cpu.z80i << 8) | (dwLowAddr & 0xff);
		cpu.z80pc = ((UINT16) cpu.z80Base[cpu.z80pc + 1] << 8) | (cpu.z80Base[cpu.z80pc]);
	}
	else
	{
		cpu.z80pc = cpu.z80intAddr;
	}
	pbPC = cpu.z80Base + cpu.z80pc;	/* Normalize the address */
	return(0);
}

/* NMI Handler */

UINT32 mz80nmi(void)
{
	cpu.z80halted = 0;
	pbSP = (cpu.z80Base + cpu.z80sp - 1);	/* Normalize the stack pointer */
	*pbSP-- = cpu.z80pc >> 8;	/* LSB */
	*pbSP = (UINT8) cpu.z80pc;	/* MSB */
	cpu.z80sp -= 2;	/* Back our stack up */
	cpu.z80pc = cpu.z80nmiAddr;	/* Our NMI */
	return(0);
}

/* Initialize MZ80 for action */

void mz80init(void)
{
	UINT32 dwLoop;
	UINT8 *pbTempPtr;
	UINT8 *pbTempPtr2;
	UINT8 bNewAdd;
	UINT8 bNewSub;
	UINT8 bFlag;
	UINT8 bLow;
	UINT8 bHigh;
	UINT8 bCarry;

	if (NULL == pbAddAdcTable)
	{
		pbAddAdcTable = malloc(256*256*2);

		if (NULL == pbAddAdcTable)
		{
			return;
		}

		pbTempPtr = pbAddAdcTable;

		pbSubSbcTable = malloc(256*256*2);

		if (NULL == pbSubSbcTable)
		{
			return;
		}

		pbTempPtr2 = pbSubSbcTable;

		for (dwLoop = 0; dwLoop < (256*256*2); dwLoop++)
		{
			bLow = dwLoop & 0xff;
			bHigh = (dwLoop >> 8) & 0xff;
			bCarry = (dwLoop >> 16);

			bFlag = 0;
			bNewAdd = bHigh + bLow + bCarry;

			if (0 == bNewAdd)
			{
				bFlag |= Z80_FLAG_ZERO;
			}
			else
			{
				bFlag = bNewAdd & 0x80; /* Sign flag */
			}

			if (((UINT32) bLow + (UINT32) bHigh + (UINT32) bCarry) >= 0x100)
			{
				bFlag |= Z80_FLAG_CARRY;
			}

			if ( ((bLow ^ bHigh ^ 0x80) & (bLow ^ (bNewAdd & 0x80))) & 0x80)
			{
				bFlag |= Z80_FLAG_OVERFLOW_PARITY;
			}

			if (((bLow & 0x0f) + (bHigh & 0x0f) + bCarry) >= 0x10)
			{
				bFlag |= Z80_FLAG_HALF_CARRY;
			}

			*pbTempPtr++ = bFlag;	/* Store our new flag */

			// Now do subtract - Zero

			bFlag = Z80_FLAG_NEGATIVE;
			bNewSub = bHigh - bLow - bCarry;

			if (0 == bNewSub)
			{
				bFlag |= Z80_FLAG_ZERO;
			}
			else
			{
				bFlag |= bNewSub & 0x80; /* Sign flag */
			}

			if ( ((INT32) bHigh - (INT32) bLow - (INT32) bCarry) < 0)
			{
				bFlag |= Z80_FLAG_CARRY;
			}

			if ( ((INT32) (bHigh & 0xf) - (INT32) (bLow & 0x0f) - (INT32) bCarry) < 0)
			{
				bFlag |= Z80_FLAG_HALF_CARRY;
			}

			if ( ((bLow ^ bHigh) & (bHigh ^ bNewSub) & 0x80) )
			{
				bFlag |= Z80_FLAG_OVERFLOW_PARITY;
			}

			*pbTempPtr2++ = bFlag;	/* Store our sub flag */

		}
	}
}
/* Shut down MZ80 */

void mz80shutdown(void)
{
	// notaz: why weren't these here?
	free(pbAddAdcTable);
	pbAddAdcTable = 0;
	free(pbSubSbcTable);
	pbSubSbcTable = 0;
}

