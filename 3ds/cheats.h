
#include "3ds.h"

extern "C" 
{
u32 decode_cheat(char *string, int index);
u32 enable_cheat(int index, u8 enable);
void apply_cheats(void);
void clear_cheats(void);
void ROMCheatUpdate(void);
void RAMCheatUpdate(void);
}