#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void targetname(char *dest, char *src)
{
	char *p, *p1;

	if(strlen(src) < 5 || src[0] == '\t') return;

	// goto string end
	for(p=src; *p && *p != ' ' && *p != '\r'; p++);
	// goto start
	for(p1=p; p1 > src && *p1 != '\\'; p1--); p1++;
	if(p-p1 > 0) {
		strncpy(dest, p1, p-p1);
		dest[p-p1] = 0;
	}
}


int main(int argc, char *argv[])
{
	FILE *f = 0, *fo = 0;
	unsigned char buff[512], buff2[128], outname[512];
	buff2[0] = 0;

	if(argc != 2) {
		printf("usage: %s <makefile>\n\n", argv[0]);
		return 1;
	}
	
	f = fopen(argv[1], "r");
	if(!f) {
		printf("%s: couldn't open %s\n", argv[0], argv[1]);
		return 2;
	}

	strcpy(outname, argv[1]);
	strcat(outname, ".out");
	fo = fopen(outname, "w");
	if(!fo) {
		fclose(f);
		printf("%s: couldn't open %s for writing\n", argv[0], outname);
		return 3;
	}


	while(!feof(f)) {
		fgets(buff, 512, f);
		if(!strncmp(buff, "\t$(GCCUREL)", 11) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: gcc\n\t@$(GCCUREL)", buff2);
			fputs(buff+11, fo);
		} else if(!strncmp(buff, "\tperl -S ecopyfile.pl", 21) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: perl -S ecopyfile.pl\n\t@perl", buff2);
			fputs(buff+5, fo);
		} else if(!strncmp(buff, "\tperl -S epocrc.pl", 18) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: perl -S epocrc.pl\n\t@perl", buff2);
			fputs(buff+5, fo);
		} else if(!strncmp(buff, "\tperl -S epocaif.pl", 19) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: perl -S epocaif.pl\n\t@perl", buff2);
			fputs(buff+5, fo);
		} else if(!strncmp(buff, "\tperl -S emkdir.pl", 18) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: perl -S emkdir.pl\n\t@perl", buff2);
			fputs(buff+5, fo);
		} else if(!strncmp(buff, "\tperl -S makedef.pl", 18) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: perl -S makedef.pl\n\t@perl", buff2);
			fputs(buff+5, fo);
		} else if(!strncmp(buff, "\tld ", 4) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: ld\n\t@ld ", buff2);
			fputs(buff+4, fo);
		} else if(!strncmp(buff, "\tar ", 4) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: ar\n\t@ar ", buff2);
			fputs(buff+4, fo);
		} else if(!strncmp(buff, "\tif exist ", 10) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: if exist (del?)\n\t@if exist ", buff2);
			fputs(buff+10, fo);
		} else if(!strncmp(buff, "\tdlltool ", 9) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: dlltool\n\t@dlltool ", buff2);
			fputs(buff+9, fo);
		} else if(!strncmp(buff, "\tpetran ", 8) && !strchr(buff, '>')) {
			fprintf(fo, "\t@echo %s: petran\n\t@petran ", buff2);
			fputs(buff+8, fo);
		} else {
			// try to get new targetname
			targetname(buff2, buff);
			fputs(buff, fo);
		}
	}


	// done!
	fclose(f);
	fclose(fo);

	remove(argv[1]);
	rename(outname, argv[1]);

	return 0;
}
