

#define DEBUG_WAIT_L_KEY 	\
    { \
        uint32 prevkey = 0; \
        while (aptMainLoop()) \ 
        {  \
            hidScanInput(); \ 
            uint32 key = hidKeysHeld(); \
            if (key == KEY_L) break; \
            if (key == KEY_TOUCH) break; \
            if (key == KEY_SELECT) { emulator.enableDebug ^= 1; break; } \
            if (prevkey != 0 && key == 0) \
                break;  \
            prevkey = key; \
        } \ 
    }
