

extern u16 msd_command, msd_result;
extern u16 msd_data[0x800/2];

extern void msd_update(void);
extern void msd_process(u16 d);
extern void msd_reset(void);

extern void msd_write8(u32 a, u32 d);
extern void msd_write16(u32 a, u32 d);
