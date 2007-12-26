
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
		unsigned short RAM[256*2];	// 2 internal RAM banks
		struct {
			unsigned short RAM0[256];
			unsigned short RAM1[256];
		};
	};
	ssp_reg_t gr[16];	// general registers
	union {
		unsigned char r[8];	// BANK pointers
		struct {
			unsigned char r0[4];
			unsigned char r1[4];
		};
	};
	unsigned short stack[6];
	unsigned int pmac_read[6];	// read modes/addrs for PM0-PM5
	unsigned int pmac_write[6];	// write ...
	//
	#define SSP_PMC_HAVE_ADDR 1	// address written to PMAC, waiting for mode
	#define SSP_PMC_SET 2		// PMAC is set
	unsigned int emu_status;
	unsigned int pad[30];
} ssp1601_t;


void ssp1601_reset(ssp1601_t *ssp);
void ssp1601_run(int cycles);

