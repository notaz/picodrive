extern void *giz_screen;

void giz_init();
void giz_deinit(void);
void lprintf_al(const char *fmt, ...);

#define lprintf lprintf_al

// button mappings, include kgsdk/Framework.h to use
#define BTN_LEFT  (1 << FRAMEWORK_BUTTON_DPAD_LEFT)
#define BTN_RIGHT (1 << FRAMEWORK_BUTTON_DPAD_RIGHT)
#define BTN_UP    (1 << FRAMEWORK_BUTTON_DPAD_UP)
#define BTN_DOWN  (1 << FRAMEWORK_BUTTON_DPAD_DOWN)
#define BTN_STOP  (1 << FRAMEWORK_BUTTON_STOP)
#define BTN_PLAY  (1 << FRAMEWORK_BUTTON_PLAY)
#define BTN_FWD   (1 << FRAMEWORK_BUTTON_FORWARD)
#define BTN_REW   (1 << FRAMEWORK_BUTTON_REWIND)
#define BTN_L     (1 << FRAMEWORK_BUTTON_LEFT_SHOULDER)
#define BTN_R     (1 << FRAMEWORK_BUTTON_RIGHT_SHOULDER)

#define BTN_HOME       (1 << FRAMEWORK_BUTTON_HOME)
#define BTN_VOLUME     (1 << FRAMEWORK_BUTTON_VOLUME)
#define BTN_BRIGHTNESS (1 << FRAMEWORK_BUTTON_BRIGHTNESS)
#define BTN_ALARM      (1 << FRAMEWORK_BUTTON_ALARM)
#define BTN_POWER      (1 << FRAMEWORK_BUTTON_POWER)

