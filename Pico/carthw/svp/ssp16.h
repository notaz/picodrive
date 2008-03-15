// basic, incomplete SSP160x (SSP1601?) interpreter

// (c) Copyright 2008, Grazvydas "notaz" Ignotas
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


// register names
enum {
	SSP_GR0, SSP_X,     SSP_Y,   SSP_A,
	SSP_ST,  SSP_STACK, SSP_PC,  SSP_P,
	SSP_PM0, SSP_PM1,   SSP_PM2, SSP_XST,
	SSP_PM4, SSP_gr13,  SSP_PMC, SSP_AL
};

typedef union
{
	unsigned int v;
	struct {
		unsigned short l;
		unsigned short h;
	};
} ssp_reg_t;

typedef struct
{
	union {
		unsigned short RAM[256*2];	// 000 2 internal RAM banks
		struct {
			unsigned short RAM0[256];
			unsigned short RAM1[256];
		};
	};
	ssp_reg_t gr[16];			// 400 general registers
	union {
		unsigned char r[8];		// 440 BANK pointers
		struct {
			unsigned char r0[4];
			unsigned char r1[4];
		};
	};
	unsigned short stack[6];		// 448
	unsigned int pmac_read[6];		// 454 read modes/addrs for PM0-PM5
	unsigned int pmac_write[6];		// 46c write ...
	//
	#define SSP_PMC_HAVE_ADDR	0x0001	// address written to PMAC, waiting for mode
	#define SSP_PMC_SET		0x0002	// PMAC is set
	#define SSP_WAIT_PM0		0x2000	// bit1 in PM0
	#define SSP_WAIT_30FE06		0x4000	// ssp tight loops on 30FE06 to become non-zero
	#define SSP_WAIT_30FE08		0x8000	// same for 30FE06
	#define SSP_WAIT_MASK		0xe000
	unsigned int emu_status;		// 484
	/* used by recompiler only: */
	struct {
		unsigned int ptr_rom;		// 488
		unsigned int ptr_iram_rom;	// 48c
		unsigned int ptr_dram;		// 490
		unsigned int iram_dirty;	// 494
		unsigned int iram_context;	// 498
		unsigned int ptr_btable;	// 49c
		unsigned int ptr_btable_iram;	// 4a0
		unsigned int tmp0;		// 4a4
		unsigned int tmp1;		// 4a8
		unsigned int tmp2;		// 4ac
	} drc;
} ssp1601_t;


void ssp1601_reset(ssp1601_t *ssp);
void ssp1601_run(int cycles);

