
#include "stdio.h"
#include "3dsfilereadahead.h"
#include "3dsdbg.h"

extern void debugWait();


//-----------------------------------------------------------------------------
// Initialize the read-ahead structure.
//-----------------------------------------------------------------------------
void read_ahead_init(cd_read_ahead_struct *cd_read_ahead)
{
  cd_read_ahead->fptr = NULL;
  cd_read_ahead->seek_pos = 0;
  cd_read_ahead->buffer_pos = 0;
  cd_read_ahead->buffer_length = 0;
  memset(cd_read_ahead->buffer, 0, CD_READ_AHEAD_BUFFER_SIZE);
}


//-----------------------------------------------------------------------------
// Call ftell
//-----------------------------------------------------------------------------
long read_ahead_ftell(cd_read_ahead_struct *cd_read_ahead, FILE *fp) 
{
  if (fp == NULL)
    return 0;
  
  if (cd_read_ahead->fptr != fp)
  {
    return ftell(fp);
  }  
  else
  {
    return cd_read_ahead->seek_pos + cd_read_ahead->buffer_pos;
  }
}


//-----------------------------------------------------------------------------
// Seek to a position in the file. If the new position is still
// within the size of the buffer loaded from disk, update all the 
// internal position pointers.
//-----------------------------------------------------------------------------
void read_ahead_fseek(cd_read_ahead_struct *cd_read_ahead, FILE *fp, int pos, int origin) 
{
  if (fp == NULL)
    return;

  fseek(fp, pos, origin);
  pos = ftell(fp);

  if (cd_read_ahead->fptr != fp)
  {
    cd_read_ahead->fptr = fp;
    cd_read_ahead->seek_pos = pos;
    cd_read_ahead->buffer_pos = 0;
    cd_read_ahead->buffer_length = 0;
  }
  else
  {
    if (cd_read_ahead->seek_pos <= pos && 
      pos < (cd_read_ahead->seek_pos + cd_read_ahead->buffer_length))
    {
      cd_read_ahead->buffer_pos = pos - cd_read_ahead->seek_pos;
    }
    else
    {
      cd_read_ahead->seek_pos = pos;
      cd_read_ahead->buffer_pos = 0;
      cd_read_ahead->buffer_length = 0;
    }
  }

}

//-----------------------------------------------------------------------------
// Read data from the read-ahead buffer, if the full requested size of the
// data is still within the buffer. Otherwise, fseek to the new position
// and read-ahead to fill the full buffer and load the requested size.
//-----------------------------------------------------------------------------
int read_ahead_fread(cd_read_ahead_struct *cd_read_ahead, void *dest_buffer, int size, FILE *fp) 
{
  if (fp == NULL)
    return 0;
  
  int total_size = size;  
  if (cd_read_ahead->fptr != fp || 
      (cd_read_ahead->buffer_pos + total_size) > cd_read_ahead->buffer_length) 
  { 
    fseek(fp, cd_read_ahead->buffer_pos + cd_read_ahead->seek_pos, SEEK_SET);
    cd_read_ahead->seek_pos = cd_read_ahead->buffer_pos + cd_read_ahead->seek_pos;
    cd_read_ahead->buffer_length = fread(cd_read_ahead->buffer, 1, CD_READ_AHEAD_BUFFER_SIZE, fp); 
    cd_read_ahead->buffer_pos = 0; 
    cd_read_ahead->fptr = fp; 
  } 
  if (cd_read_ahead->buffer_pos + total_size > cd_read_ahead->buffer_length) 
  {
    total_size = cd_read_ahead->buffer_length - cd_read_ahead->buffer_pos; 
  }
  memcpy(dest_buffer, &cd_read_ahead->buffer[cd_read_ahead->buffer_pos], total_size); 
  cd_read_ahead->buffer_pos += total_size; 

  return total_size;
}
