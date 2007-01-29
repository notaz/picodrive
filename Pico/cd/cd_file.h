#ifndef _CD_FILE_H
#define _CD_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#define TYPE_ISO 1
#define TYPE_BIN 2
#define TYPE_MP3 3
//#define TYPE_WAV 4



void FILE_End(void);
int Load_ISO(const char *iso_name, int is_bin);
void Unload_ISO(void);
int FILE_Read_One_LBA_CDC(void);
int FILE_Play_CD_LBA(void);


#ifdef __cplusplus
};
#endif

#endif
