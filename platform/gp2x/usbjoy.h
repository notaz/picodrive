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

#ifndef USBJOY_H
#define USBJOY_H

/* notaz: my Logitech has different button layout, and I want it to match gp2x's */
typedef enum {
	JOY_TYPE_GENERIC,
	JOY_TYPE_LOGITECH
} joy_type;

/*
  Enumeration: Axes values
  This enumeration contains shortcuts to the values used on axes.

  Constants:
  JOYUP    - Joystick Up
  JOYDOWN  - Joystick Down
  JOYLEFT  - Joystick Left
  JOYRIGHT - Joystick Right

  See also:
  <joy_getaxe>
*/
#define JOYUP    (0)
#define JOYDOWN  (1)
#define JOYLEFT  (2)
#define JOYRIGHT (3)


/*
  Struct: usbjoy

  Contains all Joystick needed information.

  Fields:
  fd - File descriptor used.
  name - Joystick's name.
  device - /dev/input/jsX device.
  numbuttons - Joystick's buttons.
  numaxes - Joystick's axes.
  numhats - Joystick's hats.
  statebuttons - Current state of each button.
  stateaxes - Current state of each direction.
*/
struct usbjoy {
  int fd;
  char name [128];
  char device [128];
  int numbuttons;
  int numaxes;
  int numhats;
  int statebuttons[32];
  int stateaxes[4];
  int axevals[2];
  joy_type type;
};


/*
  Function: joy_open

  Opens a USB joystick and fills its information.

  Parameters:

  joynumber - Joystick's identifier (0 reserved for GP2X's builtin Joystick).

  Returns:

  Filled usbjoy structure.
*/
struct usbjoy * joy_open (int joynumber);


/*
  Function: joy_name

  Returns Joystick's name.

  Parameters:

  joy - Selected joystick.

  Returns:

  Joystick's name or NULL if <usbjoy> struct is empty.
*/
char * joy_name (struct usbjoy * joy);


/*
  Function: joy_device

  Returns Joystick's device.

  Parameters:

  joy - Selected joystick.

  Returns:

  Joystick's device or NULL if <usbjoy> struct is empty.
*/
char * joy_device (struct usbjoy * joy);

/*
  Function: joy_buttons

  Returns Joystick's buttons number.

  Parameters:

  joy - Selected joystick.

  Returns:

  Joystick's buttons or 0 if <usbjoy> struct is empty.
*/
int joy_buttons (struct usbjoy * joy);

/*
  Function: joy_axes

  Returns Joystick's axes number.

  Parameters:

  joy - Selected joystick.

  Returns:

  Joystick's axes or 0 if <usbjoy> struct is empty.
*/
int joy_axes (struct usbjoy * joy);


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
int joy_update (struct usbjoy * joy);


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
int joy_getbutton (int button, struct usbjoy * joy);


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
int joy_getaxe (int axe, struct usbjoy * joy);

/*
  Function: joy_close

  Closes selected joystick's file descriptor and detroys it's fields.

  Parameters:

  joy - Selected joystick.

  Returns:

  0 - Joystick successfully closed.
  -1 - Error: <usbjoy> struct is empty.
*/
int joy_close (struct usbjoy * joy);



/* gp2x stuff */
extern int num_of_joys;
extern struct usbjoy *joys[4];

void gp2x_usbjoy_update(void);
void gp2x_usbjoy_init(void);
int  gp2x_usbjoy_check(int joyno);
int  gp2x_usbjoy_check2(int joyno);
void gp2x_usbjoy_deinit(void);


#endif // USBJOY_H
