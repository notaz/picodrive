
int  xenv_init(int *have_mouse_events, const char *window_title);

/* read events from X, calling key_cb for key, mouseb_cb for mouse button
 * and mousem_cb for mouse motion events */
int  xenv_update(int (*key_cb)(void *cb_arg, int kc, int is_pressed),
		 int (*mouseb_cb)(void *cb_arg, int x, int y, int button, int is_pressed),
		 int (*mousem_cb)(void *cb_arg, int x, int y),
		 void *cb_arg);

int  xenv_minimize(void);
void xenv_finish(void);

