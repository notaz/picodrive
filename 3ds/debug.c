


#include "3ds.h"
#include "3dsdbg.h"

void debugWait()
{                                                                       
    hidScanInput();                                                     
    uint32 prevkey = hidKeysHeld();                                     
    while (aptMainLoop())                                              
    {                                                                   
        hidScanInput();                                                 
        uint32 key = hidKeysHeld();                                     
        if (key == KEY_L) break;                                        
        if (key == KEY_TOUCH) break;                                    
        if (prevkey == 0 && key != 0)                                   
            break;                                                      
        if (key == KEY_SELECT)                                          
            { emulator.enableDebug = !emulator.enableDebug; break; }    
        prevkey = key;                                                  
    }                                                                    
}

void clearBottomScreen()
{
    gfxSetDoubleBuffering(GFX_BOTTOM,false); 
    gfxSwapBuffers(); 
    consoleInit(GFX_BOTTOM, NULL); 
    consoleClear(); 
}