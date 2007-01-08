#ifndef _CD_FILE_H
#define _CD_FILE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TYPE_ISO 1
#define TYPE_BIN 2
#define TYPE_MP3 3
//#define TYPE_WAV 4


struct _file_track {
	FILE *F;
	int Lenght;
	int Type;
};

extern struct _file_track Tracks[100];
extern char Track_Played;


int FILE_Init(void);
void FILE_End(void);
int Load_ISO(const char *iso_name, int is_bin);
void Unload_ISO(void);
int FILE_Read_One_LBA_CDC(void);
int FILE_Play_CD_LBA(void);


#ifdef __cplusplus
};
#endif

#endif
