#include <e32base.h>

// let's do it in more c-like way
int  vidInit(void *vidmem, int reinit);
void vidFree();
void vidDrawFrame(char *noticeStr, char *fpsStr, int num);
void vidKeyConfigFrame(const TUint whichAction);
void vidDrawFCconfigDone();
void vidDrawNotice(const char *txt); // safe to call anytime, draws text for 1 frame
