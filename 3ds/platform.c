
#include "stdio.h"
#include "../pico/pico.h"
#include "../pico/pico_int.h"

int emu_save_load_sram(char *saveFname, int load)
{
	int ret = 0;

    FILE *sramFile;
    int sram_size;
    unsigned char *sram_data;
    int truncate = 1;
    if (PicoIn.AHW & PAHW_MCD)
    {
        if (PicoIn.opt & POPT_EN_MCD_RAMCART) {
            sram_size = 0x12000;
            sram_data = Pico.sv.data;
            if (sram_data)
                memcpy(sram_data, Pico_mcd->bram, 0x2000);
        } else {
            sram_size = 0x2000;
            sram_data = Pico_mcd->bram;
            truncate  = 0; // the .brm may contain RAM cart data after normal brm
        }
    } else {
        sram_size = Pico.sv.size;
        sram_data = Pico.sv.data;
    }
    if (sram_data == NULL)
        return 0; // cart saves forcefully disabled for this game

    if (load)
    {
        sramFile = fopen(saveFname, "rb");
        if (!sramFile)
            return -1;
        ret = fread(sram_data, 1, sram_size, sramFile);
        ret = ret > 0 ? 0 : -1;
        fclose(sramFile);
        if ((PicoIn.AHW & PAHW_MCD) && (PicoIn.opt&POPT_EN_MCD_RAMCART))
            memcpy(Pico_mcd->bram, sram_data, 0x2000);
    } else {
        // sram save needs some special processing
        // see if we have anything to save
        for (; sram_size > 0; sram_size--)
            if (sram_data[sram_size-1]) break;

        if (sram_size) {
            sramFile = fopen(saveFname, truncate ? "wb" : "r+b");
            if (!sramFile) sramFile = fopen(saveFname, "wb"); // retry
            if (!sramFile) return -1;
            ret = fwrite(sram_data, 1, sram_size, sramFile);
            ret = (ret != sram_size) ? -1 : 0;
            fclose(sramFile);
#ifdef __GP2X__
            sync();
#endif
        }
    }
    return ret;
}