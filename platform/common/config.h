
#ifdef __cplusplus
extern "C" {
#endif

int config_writesect(const char *fname, const char *section);
int config_writelrom(const char *fname);
int config_readsect(const char *fname, const char *section);
int config_readlrom(const char *fname);
int config_havesect(const char *fname, const char *section);
int config_get_var_val(void *file, char *line, int lsize, char **rvar, char **rval);

#ifdef __cplusplus
}
#endif

