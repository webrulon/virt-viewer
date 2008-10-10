/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2008 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Richard W.M. Jones <rjones@redhat.com>
 */

/* Replacement usleep function, only used on platforms which lack
 * this system or library function.
 */

#include <config.h>

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#ifdef WIN32

int
usleep (unsigned int usecs)
{
  unsigned int msecs = usecs / 1000;
  if (msecs < 1)
    Sleep (1);
  else
    Sleep (msecs);
}

#else

int
usleep (unsigned int usecs)
{
  /* This sucks, but everything has sleep and it's not
   * really that critical how long we sleep for in the
   * main code.
   */
  unsigned int secs = usecs / 1000000;
  if (secs < 1)
    sleep (1);
  else
    sleep (secs);
}

#endif
