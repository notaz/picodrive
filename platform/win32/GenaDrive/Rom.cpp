
#include "app.h"
#include "Unzip.h"

unsigned char *RomData=NULL;
int RomLen=0;
char RomName[260]="";


static int Byteswap(unsigned char *data,int len)
{
  int i=0;

  if (len<2) return 1; // Too short

  do
  {
    unsigned short *pd=(unsigned short *)(data+i);
    int word=*pd; // Get word

    word=(word<<8)|(word>>8); // Byteswap it
    *pd=(unsigned short)word; // Put word
    i+=2;
  }  
  while (i+2<=len);

  return 0;
}

// Interleve a 16k block and byteswap
static int InterleveBlock(unsigned char *dest,unsigned char *src)
{
  int i=0;
  for (i=0;i<0x2000;i++) dest[(i<<1)  ]=src[       i]; // Odd
  for (i=0;i<0x2000;i++) dest[(i<<1)+1]=src[0x2000+i]; // Even
  return 0;
}

// Decode a SMD file
static int DecodeSmd(unsigned char *data,int len)
{
  unsigned char *temp=NULL;
  int i=0;

  temp=(unsigned char *)malloc(0x4000);
  if (temp==NULL) return 1;
  memset(temp,0,0x4000);

  // Interleve each 16k block and shift down by 0x200:
  for (i=0; i+0x4200<=len; i+=0x4000)
  {
    InterleveBlock(temp,data+0x200+i); // Interleve 16k to temporary buffer
    memcpy(data+i,temp,0x4000); // Copy back in
  }

  free(temp);
  return 0;
}

int RomLoad()
{
  FILE *file=NULL;
  char *name=NULL;
  int nameLen=0;
  int fileLen=0,space=0;
  Unzip unzip;

  name=RomName;

  file=fopen(name,"rb"); if (file==NULL) return 1;

  nameLen=strlen(name);
  if (stricmp(name+nameLen-4,".zip")==0) unzip.file=file; // Open as zip file

  if (unzip.file)
  {
    int ret=0;

    ret=unzip.fileOpen(); // Get first entry
    if (ret==0)
    {
      fileLen=unzip.dataLen;  // Length of file
      // Switch to using the name in the zip file:
      name=unzip.name; nameLen=strlen(name);
    }
    else
    {
      unzip.file=NULL;
    }

  }
  else
  {
    // Find out the length of the file:
    fseek(file,0,SEEK_END); fileLen=ftell(file);
    fseek(file,0,SEEK_SET);
  }

  // Allocate space for it:
  space=(fileLen+0x3fff)&~0x3fff;

  RomData=(unsigned char *)malloc(space);
  if (RomData==NULL) { fclose(file); return 1; }
  memset(RomData,0,space);

  // Read in file:
  if (unzip.file) unzip.fileDecode(RomData);
  else fread(RomData,1,fileLen,file);

  unzip.fileClose();

  fclose(file);
  unzip.file=file=NULL;

  RomLen=fileLen;

  // Check for SMD:
  if ((fileLen&0x3fff)==0x200)
  {
    // Decode and byteswap:
    DecodeSmd(RomData,RomLen);
    RomLen-=0x200;
  }
  else
  {
    // Just byteswap:
    Byteswap(RomData,RomLen);
  }

  PicoCartInsert(RomData,RomLen);

  return 0;
}

void RomFree()
{
//  PicoCartInsert(NULL,0); // Unplug rom

  if (RomData) free(RomData);
  RomData=NULL; RomLen=0;
  memset(RomName,0,sizeof(RomName));
}

