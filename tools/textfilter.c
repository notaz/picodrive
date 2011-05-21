#include <stdio.h>
#include <string.h>
#include <ctype.h>


static int check_defines(const char **defs, int defcount, char *tdef)
{
	int i, len;

	while (isspace(*tdef)) tdef++;
	len = strlen(tdef);
	for (i = 0; i < len; i++)
		if (tdef[i] == ' ' || tdef[i] == '\r' || tdef[i] == '\n') break;
	tdef[i] = 0;

	for (i = 0; i < defcount; i++)
	{
		if (strcmp(defs[i], tdef) == 0)
			return 1;
	}

	return 0;
}


static void do_counters(char *str)
{
	static int counter_id = -1, counter;
	char buff[1024];
	char *s = str;

	while ((s = strstr(s, "@@")))
	{
		if (s[2] < '0' || s[2] > '9') { s++; continue; }

		if (counter_id != s[2] - '0') {
			counter_id = s[2] - '0';
			counter = 1;
		}
		snprintf(buff, sizeof(buff), "%i%s", counter++, s + 3);
		strcpy(s, buff);
	}
}


int main(int argc, char *argv[])
{
	char buff[1024];
	FILE *fi, *fo;
	int skip_mode = 0, ifdef_level = 0, skip_level = 0, line = 0;

	if (argc < 3)
	{
		printf("usage:\n%s <file_in> <file_out> [defines...]\n", argv[0]);
		return 1;
	}

	fi = fopen(argv[1], "r");
	if (fi == NULL)
	{
		printf("failed to open: %s\n", argv[1]);
		return 2;
	}

	fo = fopen(argv[2], "w");
	if (fo == NULL)
	{
		printf("failed to open: %s\n", argv[2]);
		return 3;
	}

	for (++line; !feof(fi); line++)
	{
		char *fgs;

		fgs = fgets(buff, sizeof(buff), fi);
		if (fgs == NULL) break;

		if (buff[0] == '#')
		{
			/* control char */
			if (strncmp(buff, "#ifdef ", 7) == 0)
			{
				ifdef_level++;
				if (!skip_mode && !check_defines((void *) &argv[3], argc-3, buff + 7))
					skip_mode = 1, skip_level = ifdef_level;
			}
			else if (strncmp(buff, "#ifndef ", 8) == 0)
			{
				ifdef_level++;
				if (!skip_mode &&  check_defines((void *) &argv[3], argc-3, buff + 8))
					skip_mode = 1, skip_level = ifdef_level;
			}
			else if (strncmp(buff, "#else", 5) == 0)
			{
				if (!skip_mode || skip_level == ifdef_level)
					skip_mode ^= 1, skip_level = ifdef_level;
			}
			else if (strncmp(buff, "#endif", 6) == 0)
			{
				if (skip_level == ifdef_level)
					skip_mode = 0;
				ifdef_level--;
				if (ifdef_level == 0) skip_mode = 0;
				if (ifdef_level < 0)
				{
					printf("%i: warning: #endif without #ifdef, ignoring\n", line);
					ifdef_level = 0;
				}
			}
			else if (strncmp(buff, "#include ", 9) == 0)
			{
				char *pe, *p = buff + 9;
				FILE *ftmp;
				if (skip_mode) continue;
				while (*p && (*p == ' ' || *p == '\"')) p++;
				for (pe = p + strlen(p) - 1; pe > p; pe--)
					if (isspace(*pe) || *pe == '\"') *pe = 0;
					else break;
				ftmp = fopen(p, "r");
				if (ftmp == NULL) {
					printf("%i: error: failed to include \"%s\"\n", line, p);
					return 1;
				}
				while (!feof(ftmp))
				{
					fgs = fgets(buff, sizeof(buff), ftmp);
					if (fgs == NULL) break;
					fputs(buff, fo);
				}
				fclose(ftmp);
				continue;
			}

			/* skip line */
			continue;
		}
		if (!skip_mode)
		{
			do_counters(buff);
			fputs(buff, fo);
		}
	}

	fclose(fi);
	fclose(fo);

	return 0;
}

