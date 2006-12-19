#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#define symbols 2

int main(int argc, char *argv[])
{
	FILE *f = 0;
	unsigned char pattern[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56 };
	unsigned char *buff, *p;
	unsigned long patt_offset; // pattern offset in .text section
	unsigned long size = 0, i, insert_pos, *handler;//, symbols;
	unsigned short reloc_entry;
	IMAGE_BASE_RELOCATION *reloc_page;
	IMAGE_DOS_HEADER  *dos_header;
	IMAGE_FILE_HEADER *file_header;
	IMAGE_SECTION_HEADER *sect_header, *relocsect_header = 0, *codesect_header = 0;

	if(argc != 2) {
		printf("usage: %s <pe_exe_or_app_before_petran>\n\n", argv[0]);
		printf("note: this was written to fix a problem related to Cyclone and as v.2.9-psion-98r2 and shouldn't be used for anything else. See Readme.\n", argv[0]);
		return 1;
	}
	
	f = fopen(argv[1], "rb+");
	if(!f) {
		printf("%s: couldn't open %s\n", argv[0], argv[1]);
		return 2;
	}

	//symbols = atoi(argv[2]);

	// read the file
	fseek(f,0,SEEK_END); size=ftell(f); fseek(f,0,SEEK_SET);
	buff = (unsigned char *) malloc(size);
	fread(buff,1,size,f);

	dos_header = (IMAGE_DOS_HEADER *)      buff;
	file_header= (IMAGE_FILE_HEADER *)    (buff+dos_header->e_lfanew+4);
	sect_header= (IMAGE_SECTION_HEADER *) (buff+dos_header->e_lfanew+4+sizeof(IMAGE_FILE_HEADER)+sizeof(IMAGE_OPTIONAL_HEADER32));

	if(size < 0x500 || dos_header->e_magic != IMAGE_DOS_SIGNATURE ||
	   *(DWORD *)(buff+dos_header->e_lfanew) != IMAGE_NT_SIGNATURE || file_header->Machine != 0x0A00) {
		printf("%s: not a PE executable image for ARM target.\n", argv[0]);
		fclose(f);
		free(buff);
		return 2;
	}

	// scan all sections for data and reloc sections
	for(i = 0; i < file_header->NumberOfSections; i++, sect_header++) {
		     if(strncmp(sect_header->Name, ".text",  5) == 0) codesect_header  = sect_header;
		else if(strncmp(sect_header->Name, ".reloc", 6) == 0) relocsect_header = sect_header;
	}
	
	if(!codesect_header || !relocsect_header) {
		printf("%s: failed to find reloc and/or data section.\n", argv[0]);
		fclose(f);
		free(buff);
		return 3;
	}

	if(relocsect_header != sect_header-1) {
		printf("%s: bug: reloc section is not last, this is unexpected and not supported.\n", argv[0]);
		fclose(f);
		free(buff);
		return 4;
	}

	// find the pattern
	for(i = codesect_header->PointerToRawData; i < size-8; i+=2)
		if(memcmp(buff+i, pattern, 8) == 0) break;
	if(i == size-8 || i < 4) {
		printf("%s: failed to find the pattern.\n", argv[0]);
		fclose(f);
		free(buff);
		return 5;
	}

	// calculate pattern offset in RVA (relative virtual address)
	patt_offset = i - codesect_header->PointerToRawData + codesect_header->VirtualAddress;

	// replace the placeholders themselves
	handler = (unsigned long *) (buff + i - 4);
	for(i = 1; i <= symbols; i++)
		*(handler+i) = *handler;

	// find suitable reloc section
	for(i = 0, p = buff+relocsect_header->PointerToRawData; i < relocsect_header->SizeOfRawData; ) {
		reloc_page = (IMAGE_BASE_RELOCATION *) p;
		if(patt_offset - reloc_page->VirtualAddress >= 0 && patt_offset - reloc_page->VirtualAddress < 0x1000 - symbols*2) break;
		i += reloc_page->SizeOfBlock;
		p += reloc_page->SizeOfBlock;
	}

	if(i >= relocsect_header->SizeOfRawData) {
		printf("%s: suitable reloc section not found.\n", argv[0]);
		fclose(f);
		free(buff);
		return 6;
	}

	// now find the insert pos and update everything
	insert_pos = p + reloc_page->SizeOfBlock - buff;
	reloc_page->SizeOfBlock += symbols*2;
	relocsect_header->SizeOfRawData += symbols*2;

	// check for possible padding
	if(!*(buff+insert_pos-1)) insert_pos -= 2;

	// write all this joy
	fseek(f,0,SEEK_SET);
	fwrite(buff, 1, insert_pos, f);

	// write new reloc entries
	for(i = 0; i < symbols; i++) {
		handler++;
		reloc_entry = (unsigned short)(((unsigned char *) handler - buff) - reloc_page->VirtualAddress - codesect_header->PointerToRawData + codesect_header->VirtualAddress) | 0x3000; // quite nasty
		fwrite(&reloc_entry, 1, 2, f);
	}

	// write the remaining data
	fwrite(buff+insert_pos, 1, size-insert_pos, f);

	// done at last!
	fclose(f);
	free(buff);

	return 0;
}
