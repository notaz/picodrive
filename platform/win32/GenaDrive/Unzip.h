

class Unzip
{
public:
  Unzip();
  FILE *file; // Zip file current open
  unsigned char head[0x1e]; // Zip entry header
  int dataLen; // Zip entry dest (uncompressed) size

  char *name; // Name of entry

  int gotoFirstFile();
  int fileOpen();
  int fileClose();
  int fileDecode(unsigned char *data);

private:
  int srcLen; // Zip entry source (compressed) size
  int nameLen,extraLen; // Length of name field and extra fields
  int headerPos; // Position of file entry header (PK... etc)
  int compPos; // Position of compressed data
};
