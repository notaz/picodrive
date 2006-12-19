

#include "app.h"
#include "Unzip.h"
#include "zlib.h"

// Decompress a 'deflate' compressed buffer
static int Inflate(unsigned char *dest,int destLen, unsigned char *src,int srcLen)
{
  z_stream stream;

  memset(&stream,0,sizeof(stream));

  stream.next_in  =src;
  stream.avail_in =srcLen;
  stream.next_out =dest;
  stream.avail_out=destLen;
  inflateInit2(&stream,-15);
  inflate(&stream,Z_FINISH);
  inflateEnd(&stream);

  return 0;
}

static int Get32(unsigned char *src)
{
  return src[0] | (src[1]<<8) | (src[2]<<16) | (src[3]<<24);
}

// --------------------------------------------------------------
Unzip::Unzip()
{
  memset(this,0,sizeof(*this));
}

int Unzip::gotoFirstFile()
{
  headerPos=0;
  return 0;
}

int Unzip::fileOpen()
{
  int ret=0,okay=0;

  fseek(file,headerPos,SEEK_SET);

  // Read in file entry header:
  ret=fread(head,1,sizeof(head),file);
  if (ret!=sizeof(head)) return 1;

  // Check header:
  if (head[0]=='P' && head[1]=='K' && head[2]==3 && head[3]==4) okay=1;
  if (okay==0) return 1;

  // Get compressed and uncompressed sizes:
  srcLen =Get32(head+0x12);
  dataLen=Get32(head+0x16);

  // Get size of name and extra fields:
  nameLen=Get32(head+0x1a);
  extraLen=nameLen>>16; nameLen&=0xffff;

  // Read in name:
  name=(char *)malloc(nameLen+1); if (name==NULL) return 1;
  memset(name,0,nameLen+1);
  fread(name,1,nameLen,file);

  // Find position of compressed data in the file
  compPos=headerPos+sizeof(head);
  compPos+=nameLen+extraLen;

  return 0;
}

int Unzip::fileClose()
{
  free(name); name=NULL;

  // Go to next header:
  headerPos=compPos+srcLen;

  srcLen=dataLen=0;
  nameLen=extraLen=0;

  return 0;
}

int Unzip::fileDecode(unsigned char *data)
{
  unsigned char *src=NULL;

  // Go to compressed data:
  fseek(file,compPos,SEEK_SET);

  // Allocate memory:
  src=(unsigned char *)malloc(srcLen);
  if (src==NULL) { fclose(file); return 1; }
  memset(src,0,srcLen);

  // Read in compressed version and decompress
  fread(src,1,srcLen,file);

  Inflate(data,dataLen, src,srcLen);

  free(src); src=NULL; srcLen=0;

  return 0;
}
