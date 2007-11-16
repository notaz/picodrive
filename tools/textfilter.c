#include <stdio.h>
#include <string.h>


static int check_defines(const char **defs, int defcount, char *tdef)
{
	int i, len;

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
	static int counters[4] = { 1, 1, 1, 1 };
	char buff[1024];
	int counter;
	char *s = str;

	while ((s = strstr(s, "@@")))
	{
		if (s[2] < '0' || s[2] > '3') { s++; continue; }

		counter = s[2] - '0';
		snprintf(buff, sizeof(buff), "%i%s", counters[counter]++, s + 3);
		strcpy(s, buff);
	}
}


int main(int argc, char *argv[])
{
	char buff[1024];
	FILE *fi, *fo;
	int skip_mode = 0, ifdef_level = 0, line = 0;

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
				if (!check_defines((void *) &argv[3], argc-3, buff + 7)) skip_mode = 1;
				ifdef_level++;
			}
			else if (strncmp(buff, "#ifndef ", 8) == 0)
			{
				if ( check_defines((void *) &argv[3], argc-3, buff + 7)) skip_mode = 1;
				ifdef_level++;
			}
			else if (strncmp(buff, "#else", 5) == 0)
			{
				skip_mode ^= 1;
			}
			else if (strncmp(buff, "#endif", 6) == 0)
			{
				ifdef_level--;
				if (ifdef_level == 0) skip_mode = 0;
				if (ifdef_level < 0)
				{
					printf("%i: warning: #endif without #ifdef, ignoring\n", line);
					ifdef_level = 0;
				}
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

