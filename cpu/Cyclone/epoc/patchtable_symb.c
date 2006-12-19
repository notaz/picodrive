#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
	unsigned long _dontcare1[4];
	char signature[4];						// 'EPOC'
	unsigned long iCpu;						// 0x1000 = X86, 0x2000 = ARM, 0x4000 = M*Core
	unsigned long iCheckSumCode;			// sum of all 32 bit words in .text
	unsigned long _dontcare3[5];
	unsigned long iCodeSize;				// size of code, import address table, constant data and export dir |+30
	unsigned long _dontcare4[12];
	unsigned long iCodeOffset;				// file offset to code section    |+64
	unsigned long _dontcare5[2];
	unsigned long iCodeRelocOffset;			// relocations for code and const |+70
	unsigned long iDataRelocOffset;			// relocations for data
	unsigned long iPriority;		// priority of this process (EPriorityHigh=450)
} E32ImageHeader;


typedef struct {
	unsigned long iSize;				// size of this relocation section
	unsigned long iNumberOfRelocs;		// number of relocations in this section
} E32RelocSection;


typedef struct {
	unsigned long base;
	unsigned long size;
} reloc_page_header;


// E32Image relocation section consists of a number of pages
// every page has 8 byte header and a number or 16bit relocation entries
// entry format:
// 0x3000 | <12bit_reloc_offset>
//
// if we have page_header.base == 0x1000 and a reloc entry 0x3110,
// it means that 32bit value at offset 0x1110 of .text section
// is relocatable

int main(int argc, char *argv[])
{
	FILE *f = 0;
	unsigned char pattern[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56 };
	unsigned char *buff, *p;
	unsigned long patt_offset; // pattern offset in .text section
	unsigned long size = 0, i, symbols, insert_pos, *handler;
	unsigned short reloc_entry;
	E32ImageHeader  *header;
	E32RelocSection *reloc_section;
	reloc_page_header *reloc_page;

	if(argc != 3) {
		printf("usage: %s <e32_exe> <nsymbols>\n\n", argv[0]);
		printf("note: this was written to fix a problem caused by as v.2.9-psion-98r2 and shouldn't be used for anything else.\n", argv[0]);
		return 1;
	}
	
	f = fopen(argv[1], "rb+");
	if(!f) {
		printf("%s: couldn't open %s\n", argv[0], argv[1]);
		return 2;
	}

	symbols = atoi(argv[2]);

	// read the file
	fseek(f,0,SEEK_END); size=ftell(f); fseek(f,0,SEEK_SET);
	buff = (unsigned char *) malloc(size);
	fread(buff,1,size,f);

	header = (E32ImageHeader *) buff;

	if(strncmp(header->signature, "EPOC", 4) || header->iCpu != 0x2000) {
		printf("%s: not a E32 executable image for ARM target.\n", argv[0]);
		fclose(f);
		free(buff);
		return 2;
	}

	// find the pattern
	for(i = 0; i < size-8; i++)
		if(memcmp(buff+i, pattern, 8) == 0) break;
	if(i == size-8 || i < 4) {
		printf("%s: failed to find the pattern.\n", argv[0]);
		fclose(f);
		free(buff);
		return 3;
	}
	patt_offset = i - header->iCodeOffset;

	// find suitable reloc section
	reloc_section = (E32RelocSection *) (buff + header->iCodeRelocOffset);
	for(i = 0, p = buff+header->iCodeRelocOffset+8; i < reloc_section->iSize; ) {
		reloc_page = (reloc_page_header *) p;
		if(patt_offset - reloc_page->base >= 0 && patt_offset - reloc_page->base < 0x1000 - symbols*4) break;
		i += reloc_page->size;
		p += reloc_page->size;
	}

	if(i >= reloc_section->iSize) {
		printf("%s: suitable reloc section not found.\n", argv[0]);
		fclose(f);
		free(buff);
		return 4;
	}

	// now find the insert pos and update everything
	insert_pos = p + reloc_page->size - buff;
	reloc_page->size     += symbols*2;
	reloc_section->iSize += symbols*2;
	reloc_section->iNumberOfRelocs += symbols;
	header->iDataRelocOffset += symbols*2; // data reloc section is now also pushed a little
	header->iPriority = 450; // let's boost our priority :)

	// replace the placeholders themselves
	handler = (unsigned long *) (buff + patt_offset + header->iCodeOffset - 4);
	for(i = 1; i <= symbols; i++)
		*(handler+i) = *handler;
	
	// recalculate checksum
	header->iCheckSumCode = 0;
	for(i = 0, p = buff+header->iCodeOffset; i < header->iCodeSize; i+=4, p+=4)
		header->iCheckSumCode += *(unsigned long *) p;

	// check for possible padding
	if(!*(buff+insert_pos-1)) insert_pos -= 2;

	// write all this joy
	fseek(f,0,SEEK_SET);
	fwrite(buff, 1, insert_pos, f);

	// write new reloc entries
	for(i = 0; i < symbols; i++) {
		handler++;
		reloc_entry = ((unsigned char *) handler - buff - reloc_page->base - header->iCodeOffset) | 0x3000;
		fwrite(&reloc_entry, 1, 2, f);
	}

	// write the remaining data
	fwrite(buff+insert_pos, 1, size-insert_pos, f);

	// done at last!
	fclose(f);
	free(buff);

	return 0;
}
