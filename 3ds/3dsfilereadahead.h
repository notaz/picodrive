
#ifndef _3DSFILEREADAHEAD_H_
#define _3DSFILEREADAHEAD_H_

#define CD_READ_AHEAD_BUFFER_SIZE (65536*2)

typedef struct
{
  FILE*             fptr;
  unsigned int      seek_pos;
  unsigned int      buffer_pos;
  unsigned int      buffer_length;
  char              buffer[CD_READ_AHEAD_BUFFER_SIZE];
} cd_read_ahead_struct;


//-----------------------------------------------------------------------------
// Initialize the read-ahead structure.
//-----------------------------------------------------------------------------
void read_ahead_init(cd_read_ahead_struct *cd_read_ahead);


//-----------------------------------------------------------------------------
// Call ftell
//-----------------------------------------------------------------------------
long read_ahead_ftell(cd_read_ahead_struct *cd_read_ahead, FILE *fp);


//-----------------------------------------------------------------------------
// Seek to a position in the file. If the new position is still
// within the size of the buffer loaded from disk, update all the 
// internal position pointers.
//-----------------------------------------------------------------------------
void read_ahead_fseek(cd_read_ahead_struct *, FILE *fp, int pos, int origin);


//-----------------------------------------------------------------------------
// Read data from the read-ahead buffer, if the full requested size of the
// data is still within the buffer. Otherwise, fseek to the new position
// and read-ahead to fill the full buffer and load the requested size.
//-----------------------------------------------------------------------------
int read_ahead_fread(cd_read_ahead_struct *, void *dest_buffer, int size, FILE *fp);

#endif

