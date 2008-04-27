#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cue.h"

//#include "../PicoInt.h"
#define elprintf(w,f,...) printf(f "\n",##__VA_ARGS__);

static char *mystrip(char *str)
{
	int i, len;

	len = strlen(str);
	for (i = 0; i < len; i++)
		if (str[i] != ' ') break;
	if (i > 0) memmove(str, str + i, len - i + 1);

	len = strlen(str);
	for (i = len - 1; i >= 0; i--)
		if (str[i] != ' ' && str[i] != '\r' && str[i] != '\n') break;
	str[i+1] = 0;

	return str;
}

static int get_token(const char *buff, char *dest, int len)
{
	const char *p = buff;
	char sep = ' ';
	int d = 0, skip = 0;

	while (*p && *p == ' ') {
		skip++;
		p++;
	}

	if (*p == '\"') {
		sep = '\"';
		p++;
	}
	while (*p && *p != sep && d < len-1)
		dest[d++] = *p++;
	dest[d] = 0;

	if (*p != sep)
		elprintf(EL_STATUS, "cue: bad token: \"%s\"", buff);

	return d + skip;
}

static char *get_ext(char *fname)
{
	int len = strlen(fname);
	return (len >= 3) ? (fname - 3) : (fname - len);
}


#define BEGINS(buff,str) (strncmp(buff,str,sizeof(str)-1) == 0)

/* note: tracks[0] is not used */
cue_data *cue_parse(const char *fname)
{
	char buff[256], current_file[256], buff2[32];
	FILE *f, *tmpf;
	int ret, count = 0, count_alloc = 2;
	cue_data *data;
	void *tmp;

	f = fopen(fname, "r");
	if (f == NULL) return NULL;

	current_file[0] = 0;
	data = calloc(1, sizeof(*data) + count_alloc * sizeof(cue_track));

	while (!feof(f))
	{
		tmp = fgets(buff, sizeof(buff), f);
		if (tmp == NULL) break;

		mystrip(buff);
		if (buff[0] == 0) continue;
		if      (BEGINS(buff, "TITLE ") || BEGINS(buff, "PERFORMER "))
			continue;	/* who would put those here? Ignore! */
		else if (BEGINS(buff, "FILE "))
		{
			get_token(buff+5, current_file, sizeof(current_file));
		}
		else if (BEGINS(buff, "TRACK "))
		{
			count++;
			if (count >= count_alloc) {
				count_alloc *= 2;
				tmp = realloc(data, sizeof(*data) + count_alloc * sizeof(cue_track));
				if (tmp == NULL) { count--; break; }
			}
			memset(&data->tracks[count], 0, sizeof(data->tracks[0]));
			if (count == 1 || strcmp(data->tracks[1].fname, current_file) != 0)
			{
				data->tracks[count].fname = strdup(current_file);
				if (data->tracks[count].fname == NULL) break;

				tmpf = fopen(current_file, "rb");
				if (tmpf == NULL) {
					elprintf(EL_STATUS, "cue: bad/missing file: \"%s\"", current_file);
					count--; break;
				}
				fclose(tmpf);
			}
			// track number
			ret = get_token(buff+6, buff2, sizeof(buff2));
			if (count != atoi(buff2))
				elprintf(EL_STATUS, "cue: track index mismatch: track %i is track %i in cue",
					count, atoi(buff2));
			// check type
			get_token(buff+6+ret, buff2, sizeof(buff2));
			if      (strcmp(buff2, "MODE1/2352"))
				data->tracks[count].type = CT_BIN;
			else if (strcmp(buff2, "MODE1/2048"))
				data->tracks[count].type = CT_ISO;
			else if (strcmp(buff2, "AUDIO"))
			{
				if (data->tracks[count].fname != NULL)
				{
					// rely on extension, not type in cue..
					char *ext = get_ext(data->tracks[count].fname);
					if      (strcasecmp(ext, "mp3") == 0)
						data->tracks[count].type = CT_MP3;
					else if (strcasecmp(ext, "wav") == 0)
						data->tracks[count].type = CT_WAV;
					else {
						elprintf(EL_STATUS, "unhandled audio format: \"%s\"",
							data->tracks[count].fname);
					}
				}
			}
			else {
				elprintf(EL_STATUS, "unhandled track type: \"%s\"", buff2);
			}
		}
		else if (BEGINS(buff, "INDEX "))
		{
			int m, s, f;
			// type
			ret = get_token(buff+6, buff2, sizeof(buff2));
			if (atoi(buff2) == 0) continue;
			if (atoi(buff2) != 1) {
				elprintf(EL_STATUS, "cue: don't know how to handle: \"%s\"", buff);
				count--; break;
			}
			// offset in file
			get_token(buff+6+ret, buff2, sizeof(buff2));
			ret = sscanf(buff2, "%i:%i:%i", &m, &s, &f);
			if (ret != 3) {
				elprintf(EL_STATUS, "cue: failed to parse: \"%s\"", buff);
				count--; break;
			}
			data->tracks[count].sector_offset = m*60*75 + s*75 + f;
		}
		else if (BEGINS(buff, "PREGAP "))
		{
			int m, s, f;
			get_token(buff+7, buff2, sizeof(buff2));
			ret = sscanf(buff2, "%i:%i:%i", &m, &s, &f);
			if (ret != 3) {
				elprintf(EL_STATUS, "cue: failed to parse: \"%s\"", buff);
				continue;
			}
			data->tracks[count].pregap = m*60*75 + s*75 + f;
		}
		else
		{
			elprintf(EL_STATUS, "cue: failed to parse: \"%s\"", buff);
		}
	}

	if (count < 1 || data->tracks[1].fname == NULL) {
		// failed..
		for (; count > 0; count--)
			if (data->tracks[count].fname != NULL)
				free(data->tracks[count].fname);
		free(data);
		return NULL;
	}

	data->track_count = count;
	return data;
}


void cue_destroy(cue_data *data)
{
	int c;

	if (data == NULL) return;

	for (c = data->track_count; c > 0; c--)
		if (data->tracks[c].fname != NULL)
			free(data->tracks[c].fname);
	free(data);
}


int main(int argc, char *argv[])
{
	cue_data *data = cue_parse(argv[1]);
	int c;

	if (data == NULL) return 1;

	for (c = 1; c <= data->track_count; c++)
		printf("%2i: %i %9i %9i %s\n", c, data->tracks[c].type, data->tracks[c].sector_offset,
			data->tracks[c].pregap, data->tracks[c].fname);

	cue_destroy(data);

	return 0;
}

