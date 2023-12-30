#include <stdio.h>
#include <stdarg.h>

#include "../libpicofe/plat.h"

const char *renderer_names[] = { "Software", "Hardware" };
const char *renderer_names32x[] = { "Software", "Hardware", "Hardware (fast)" };

struct plat_target plat_target = {};

int plat_parse_arg(int argc, char *argv[], int *x) { return 1; }

void plat_early_init(void) {}

int  plat_target_init(void) { return 0; }

void plat_init(void) {}

void plat_video_menu_enter(int is_rom_loaded) {}

void plat_video_menu_leave(void) {}

void plat_finish(void) {}

void plat_target_finish(void) {}

void plat_video_menu_begin(void) {}

void plat_video_menu_end(void) {}

int  plat_get_root_dir(char *dst, int len) { return 0; }


unsigned int plat_get_ticks_ms(void) { return 0; }

unsigned int plat_get_ticks_us(void) { return 0; }

void plat_sleep_ms(int ms) {}

void plat_video_toggle_renderer(int change, int menu_call) {}

void plat_update_volume(int has_changed, int is_up) {}

int  plat_is_dir(const char *path) { return 0; }

void plat_status_msg_busy_first(const char *msg) {}

void pemu_prep_defconfig(void) {}

void pemu_validate_config(void) {}

void plat_status_msg_clear(void) {}

void plat_status_msg_busy_next(const char *msg) {}

void plat_video_loop_prepare(void) {}

int  plat_get_data_dir(char *dst, int len) { return 0; }

void plat_video_flip(void) {}

void plat_video_wait_vsync(void) {}

void plat_wait_till_us(unsigned int us) {}

int  plat_get_skin_dir(char *dst, int len) { return 0; }

void plat_debug_cat(char *str) {}

int  plat_wait_event(int *fds_hnds, int count, int timeout_ms) { return 0; }

void pemu_loop_prep(void) {}

void pemu_sound_start(void) {}

void pemu_loop_end(void) {}

void *plat_mem_get_for_drc(size_t size) { return NULL; }

void *plat_mmap(unsigned long addr, size_t size, int need_exec, int is_fixed) { return NULL; }

void *plat_mremap(void *ptr, size_t oldsize, size_t newsize) { return NULL; }

void  plat_munmap(void *ptr, size_t size) {}

int   plat_mem_set_exec(void *ptr, size_t size) { return 0; }

void emu_video_mode_change(int start_line, int line_count, int start_col, int col_count) {}

void pemu_forced_frame(int no_scale, int do_emu) {}

void pemu_finalize_frame(const char *fps, const char *notice_msg) {}

int _flush_cache (char *addr, const int size, const int op) { return 0; }

/* lprintf */
void lprintf(const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
#if defined(LOG_TO_FILE)
	vfprintf(logFile, fmt, vl);
#else
	vprintf(fmt, vl);
#endif
	va_end(vl);
}