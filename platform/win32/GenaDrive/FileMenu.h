
// FileMenu.cpp
class FileMenu
{
public:
  FileMenu();
  int init();
  int scan();
  void exit();
  int render();
  int scroll(int amount);
  int getFilePath(char *name);

private:
  int nameReset();
  int nameFind(char *path);
  int nameAdd(char *entry);
  int nameSizeUp();
  int nameOffset(int index);

  char currentPath[260];
  char *nameList;
  int nameSize,nameMax;
  int nameCount;

  int choiceFocus;
};

extern class FileMenu FileMenu;
