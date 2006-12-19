#include <e32base.h>

// let's do it in more c-like way
int  vidInit(int displayMode, void *vidmem, int p800, int reinit=0);
void vidFree();
void vidDrawFrame(char *noticeStr, char *fpsStr, int num);
void vidKeyConfigFrame(const TUint whichAction, TInt flipClosed);
void vidDrawFCconfigDone();
void vidDrawNotice(const char *txt); // safe to call anytime, draws text for 1 frame
