
#include "app.h"
#include "FileMenu.h"

class FileMenu FileMenu;

FileMenu::FileMenu()
{
  memset(this,0,sizeof(*this));
}

int FileMenu::init()
{
  memset(this,0,sizeof(*this));
  strcpy(currentPath,HOME "roms");

  return 0;
}

int FileMenu::scan()
{
  char path[260];

  memset(path,0,sizeof(path));

  // Scan for all the roms in the current directory:
  nameReset();

  sprintf(path,"%.240s\\*.bin", currentPath); nameFind(path);
  sprintf(path,"%.240s\\*.smd", currentPath); nameFind(path);
  sprintf(path,"%.240s\\*.zip",currentPath); nameFind(path);

  return 0;
}

void FileMenu::exit()
{
  free(nameList);
  memset(this,0,sizeof(*this));
}

int FileMenu::render()
{
  int x=0,y=0;
  int pos=0,index=0;
  WCHAR text[64];
  int height=24;

  memset(text,0,sizeof(text));

  x=120; y=224;
  y-=(choiceFocus*height)>>8;

  while (pos<nameSize)
  {
    char *name=NULL;

    name=nameList+pos;

    if (y>-height && y<MainHeight)
    {
      unsigned int colour=0xffffff;

      // If this line is visible:
      wsprintfW(text,L"%.42S",name);
      if (index==(choiceFocus>>8)) colour=0x00ff40;

      FontSetColour(colour);
      FontText(text,x,y);
    }
        
    y+=height;
    pos+=strlen(name)+1; // Skip to next string
    index++;
  }

  return 0;
}

int FileMenu::scroll(int amount)
{
  int max=0;

  choiceFocus+=amount;

  max=nameCount<<8;
  if (choiceFocus<0) choiceFocus=0;
  if (choiceFocus>=max) choiceFocus=max-1;

  return 0;
}

// Get the currently highlighted filename
int FileMenu::getFilePath(char *path)
{
  int focus=0;
  int pos=0;
  char *name=NULL;

  // Find where the user is focused
  focus=choiceFocus>>8;
  pos=nameOffset(focus); if (pos<0) return 1;

  name=nameList+pos;

  // Return path and name:
  sprintf(path,"%.128s\\%.128s",currentPath,name);
  return 0;
}

// ----------------------------------------------------------------------
int FileMenu::nameReset()
{
  free(nameList); nameList=NULL;
  nameSize=nameMax=nameCount=0;

  return 0;
}

int FileMenu::nameFind(char *path)
{
  HANDLE find=NULL;
  WIN32_FIND_DATA wfd;
 
  memset(&wfd,0,sizeof(wfd));

  find=FindFirstFile(path,&wfd);
  if (find==INVALID_HANDLE_VALUE) return 1;
  
  for (;;)
  {
    nameAdd(wfd.cFileName); // Add the name to the list

    if (FindNextFile(find,&wfd)==0) break;
  }

  FindClose(find);
  return 0;
}

int FileMenu::nameAdd(char *entry)
{
  int len=0;

  len=strlen(entry);
  // Check we have room for this entry:
  if (nameSize+len+1>nameMax) nameSizeUp();
  if (nameSize+len+1>nameMax) return 1;

  // Add entry with zero at the end:
  memcpy(nameList+nameSize,entry,len);
  nameSize+=len+1;
  nameCount++;

  return 0;
}

int FileMenu::nameSizeUp()
{

  void *mem=NULL;
  int add=256;

  // Allocate more memory for the list:
  mem=realloc(nameList,nameMax+add); if (mem==NULL) return 1;

  nameList=(char *)mem;
  memset(nameList+nameMax,0,add); // Blank new memory
  nameMax+=add;
  return 0;
}

int FileMenu::nameOffset(int index)
{
  int pos=0,i=0;

  while (pos<nameSize)
  {
    char *name=nameList+pos;

    if (i==index) return pos;

    pos+=strlen(name)+1; // Skip to next string
    i++;
  }

  return -1; // Unknown index
}
