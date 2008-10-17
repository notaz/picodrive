/* Title: USB Joystick library
   Version 0.2
   Written by Puck2099 (puck2099@gmail.com), (c) 2006.
   <http://www.gp32wip.com>

   If you use this library or a part of it, please, let it know.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdlib.h>
#include <stdio.h>		/* For the definition of NULL */
#include <sys/types.h>	        // For Device open
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>		// For Device read

#include <string.h>
#include <limits.h>		/* For the definition of PATH_MAX */
#include <linux/joystick.h>

#include "usbjoy.h"

/* This is a try to support analog joys. Untested. */
#define DEAD_ZONE (8*1024)

/*
  Function: joy_open

  Opens a USB joystick and fills its information.

  Parameters:

  joynumber - Joystick's identifier (0 reserved for GP2X's builtin Joystick).

  Returns:

  Filled usbjoy structure.

*/
struct usbjoy *joy_open(int joynumber)
{
	int fd;
	char path [128];
	struct usbjoy * joy = NULL;
	struct js_event event;
#ifdef __GP2X__
	static char insmod_done = 0;
	// notaz: on my system I get unresolved input_* symbols, so have to 'insmod input' too
	// also we should insmod only once, not on every joy_open() call.
	if (!insmod_done) {
		system ("insmod input");
		system ("insmod joydev"); // Loads joydev module
		insmod_done = 1;
	}
#endif

	if (joynumber == 0) {
	}
	else if (joynumber > 0) {
		sprintf (path, "/dev/input/js%d", joynumber-1);
		fd = open(path, O_RDONLY, 0);
		if (fd == -1) {
			sprintf (path, "/dev/js%d", joynumber-1);
			fd = open(path, O_RDONLY, 0);
		}
		if (fd != -1) {
			joy = (struct usbjoy *) malloc(sizeof(*joy));
			if (joy == NULL) { close(fd); return NULL; }
			memset(joy, 0, sizeof(*joy));

			// Set the joystick to non-blocking read mode
			fcntl(fd, F_SETFL, O_NONBLOCK);

			// notaz: maybe we should flush init events now.
			// My pad returns axis as active when I plug it in, which is kind of annoying.
			while (read(fd, &event, sizeof(event)) > 0);

			// Joystick's file descriptor
			joy->fd = fd;

			// Joystick's name
			ioctl(joy->fd, JSIOCGNAME(128*sizeof(char)), joy->name);

			// Joystick's device
			strcpy(joy->device, path);

			// Joystick's buttons
			ioctl(joy->fd, JSIOCGBUTTONS, &joy->numbuttons);

			// Joystick's axes
			ioctl(joy->fd, JSIOCGAXES, &joy->numaxes);

			// Joystick's type (derived from name)
			if (strncasecmp(joy->name, "logitech", strlen("logitech")) == 0)
			     joy->type = JOY_TYPE_LOGITECH;
			else joy->type = JOY_TYPE_GENERIC;
		} else {
			// printf ("ERROR: No Joystick found\n");
		}
	}
	return joy;
}

/*
  Function: joy_name

  Returns Joystick's name.

  Parameters:

  joy - Selected joystick.

  Returns:

  Joystick's name or NULL if <usbjoy> struct is empty.
*/
char * joy_name (struct usbjoy * joy) {
  if (joy != NULL)  return joy->name;
  else return NULL;
}


/*
  Function: joy_device

  Returns Joystick's device.

  Parameters:

  joy - Selected joystick.

  Returns:

  Joystick's device or NULL if <usbjoy> struct is empty.
*/
char * joy_device (struct usbjoy * joy) {
  if (joy != NULL)  return joy->device;
  else return NULL;
}


/*
  Function: joy_buttons

  Returns Joystick's buttons number.

  Parameters:

  joy - Selected joystick.

  Returns:

  Joystick's buttons or 0 if <usbjoy> struct is empty.
*/
int joy_buttons (struct usbjoy * joy) {
  if (joy != NULL) return joy->numbuttons;
  else return 0;
}


/*
  Function: joy_axes

  Returns Joystick's axes number.

  Parameters:

  joy - Selected joystick.

  Returns:

  Joystick's axes or 0 if <usbjoy> struct is empty.
*/
int joy_axes (struct usbjoy * joy) {
  if (joy != NULL) return joy->numaxes;
  else return 0;
}


/*
  Function: joy_update

  Updates Joystick's internal information (<statebuttons> and <stateaxes> fields).

  Parameters:

  joy - Selected joystick.

  Returns:

  0 - No events registered (no need to update).
  1 - Events registered (a button or axe has been pushed).
  -1 - Error: <usbjoy> struct is empty.
*/
int joy_update (struct usbjoy * joy) {
  struct js_event events[0xff];
  int i, len;
  int event = 0;
  if (joy != NULL) {
    if ((len=read(joy->fd, events, (sizeof events))) >0) {
      len /= sizeof(events[0]);
      for ( i=0; i<len; ++i ) {
	switch (events[i].type & ~JS_EVENT_INIT) {
	case JS_EVENT_AXIS:
	  if (events[i].number == 0) {
	    joy->stateaxes[JOYLEFT] = joy->stateaxes[JOYRIGHT] = 0;
	    if      (events[i].value < -DEAD_ZONE) joy->stateaxes[JOYLEFT] = 1;
	    else if (events[i].value >  DEAD_ZONE) joy->stateaxes[JOYRIGHT] = 1;
	    joy->axevals[0] = events[i].value;
	  }
	  else if (events[i].number == 1) {
	    joy->stateaxes[JOYUP] = joy->stateaxes[JOYDOWN] = 0;
	    if      (events[i].value < -DEAD_ZONE) joy->stateaxes[JOYUP] = 1;
	    else if (events[i].value >  DEAD_ZONE) joy->stateaxes[JOYDOWN] = 1;
	    joy->axevals[1] = events[i].value;
	  }
	  event = 1;
	  break;
	case JS_EVENT_BUTTON:
	  joy->statebuttons[events[i].number] = events[i].value;
	  event = 1;
	  break;
	default:
	  break;
	}
      }
    }
  }
  else {
    event = -1;
  }
  return event;
}


/*
  Function: joy_getbutton

  Returns Joystick's button information.

  Parameters:

  button - Button which value you want to know (from 0 to 31).
  joy - Selected joystick.

  Returns:

  0 - Button NOT pushed.
  1 - Button pushed.
  -1 - Error: <usbjoy> struct is empty.
*/
int joy_getbutton (int button, struct usbjoy * joy) {
  if (joy != NULL) {
    if (button < joy_buttons(joy)) return joy->statebuttons[button];
    else return 0;
  }
  else return -1;
}


/*
  Function: joy_getaxe

  Returns Joystick's axes information.

  Parameters:

  axe - Axe which value you want to know (see <Axes values>).
  joy - Selected joystick.

  Returns:

  0 - Direction NOT pushed.
  1 - Direction pushed.
  -1 - Error: <usbjoy> struct is empty.
*/
int joy_getaxe (int axe, struct usbjoy * joy) {
  if (joy != NULL) {
    if (axe < 4) return joy->stateaxes[axe];
    else return 0;
  }
  else return -1;
}


/*
  Function: joy_close

  Closes selected joystick's file descriptor and detroys it's fields.

  Parameters:

  joy - Selected joystick.

  Returns:

  0 - Joystick successfully closed.
  -1 - Error: <usbjoy> struct is empty.
*/
int joy_close (struct usbjoy * joy) {
  if (joy != NULL) {
    close (joy->fd);
    free (joy);
    return 0;
  }
  else return -1;
}


/*********************************************************************/

#include "../common/common.h"

int num_of_joys = 0;
struct usbjoy *joys[4];

void usbjoy_init (void)
{
	/* Open available joysticks -GnoStiC */
	int i, n = 0;

	printf("\n");
	for (i = 0; i < 4; i++) {
		joys[n] = joy_open(i+1);
		if (joys[n] && joy_buttons(joys[n]) > 0) {
			printf ("+-Joystick %d: \"%s\", buttons = %i\n", i+1, joy_name(joys[n]), joy_buttons(joys[n]));
			n++;
		}
	}
	num_of_joys = n;

	printf("Found %d Joystick(s)\n",num_of_joys);
}

void usbjoy_update (void)
{
	/* Update Joystick Event Cache */
	int q, foo;
	for (q=0; q < num_of_joys; q++) {
		foo = joy_update (joys[q]);
	}
}

int usbjoy_check (int joyno)
{
	/* Check Joystick */
	int q, joyExKey = 0;
	struct usbjoy *joy = joys[joyno];

	if (joy != NULL) {
		if (joy_getaxe(JOYUP, joy))    { joyExKey |= PBTN_UP; }
		if (joy_getaxe(JOYDOWN, joy))  { joyExKey |= PBTN_DOWN; }
		if (joy_getaxe(JOYLEFT, joy))  { joyExKey |= PBTN_LEFT; }
		if (joy_getaxe(JOYRIGHT, joy)) { joyExKey |= PBTN_RIGHT; }

		/* loop through joy buttons to check if they are pushed */
		for (q=0; q<joy_buttons (joy); q++) {
			if (joy_getbutton (q, joy)) {
				if (joy->type == JOY_TYPE_LOGITECH) {
					switch (q) {
						case 0: joyExKey |= PBTN_WEST;  break;
						case 1: joyExKey |= PBTN_SOUTH; break;
						case 2: joyExKey |= PBTN_EAST;  break;
						case 3: joyExKey |= PBTN_NORTH; break;
					}
				} else {
					switch (q) {
						case 0: joyExKey |= PBTN_NORTH; break;
						case 1: joyExKey |= PBTN_EAST;  break;
						case 2: joyExKey |= PBTN_SOUTH; break;
						case 3: joyExKey |= PBTN_WEST;  break;
					}
				}

				switch (q) {
					case  4: joyExKey |= PBTN_L; break;
					case  5: joyExKey |= PBTN_R; break;
					case  6: joyExKey |= PBTN_L; break; /* left shoulder button 2 */
					case  7: joyExKey |= PBTN_R; break; /* right shoulder button 2 */
/*
					case  8: joyExKey |= GP2X_SELECT;break;
					case  9: joyExKey |= GP2X_START; break;
					case 10: joyExKey |= GP2X_PUSH;  break;
					case 11: joyExKey |= GP2X_PUSH;  break;
*/
				}
			}
		}
	}
	return joyExKey;
}

int usbjoy_check2 (int joyno)
{
	/* Check Joystick, don't map to gp2x joy */
	int q, to, joyExKey = 0;
	struct usbjoy *joy = joys[joyno];

	if (joy != NULL) {
		if (joy_getaxe(JOYUP, joy))    { joyExKey |= 1 << 0; }
		if (joy_getaxe(JOYDOWN, joy))  { joyExKey |= 1 << 1; }
		if (joy_getaxe(JOYLEFT, joy))  { joyExKey |= 1 << 2; }
		if (joy_getaxe(JOYRIGHT, joy)) { joyExKey |= 1 << 3; }

		/* loop through joy buttons to check if they are pushed */
		to = joy->numbuttons;
		if (to > 32-4) to = 32-4;
		for (q=0; q < to; q++)
			if (joy->statebuttons[q]) joyExKey |= 1 << (q+4);
	}
	return joyExKey;
}



void usbjoy_deinit (void)
{
	int i;
	for (i=0; i<num_of_joys; i++) {
		joy_close (joys[i]);
		joys[i] = NULL;
	}
	num_of_joys = 0;
}

