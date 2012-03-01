/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2012 Red Hat, Inc.
 * Copyright (C) 2009-2012 Daniel P. Berrange
 * Copyright (C) 2010 Marc-André Lureau
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
 * Author: Daniel P. Berrange <berrange@redhat.com>
 */
#ifndef _VIRT_GTK_COMPAT
# define _VIRT_GTK_COMPAT

#include <gdk/gdk.h>

G_BEGIN_DECLS

#if GTK_CHECK_VERSION(3, 0, 0)
#define GDK_Control_L GDK_KEY_Control_L
#define GDK_Alt_L GDK_KEY_Alt_L
#define GDK_Delete GDK_KEY_Delete
#define GDK_BackSpace GDK_KEY_BackSpace
#define GDK_Print GDK_KEY_Print
#define GDK_F1 GDK_KEY_F1
#define GDK_F2 GDK_KEY_F2
#define GDK_F3 GDK_KEY_F3
#define GDK_F4 GDK_KEY_F4
#define GDK_F5 GDK_KEY_F5
#define GDK_F6 GDK_KEY_F6
#define GDK_F7 GDK_KEY_F7
#define GDK_F8 GDK_KEY_F8
#define GDK_F9 GDK_KEY_F9
#define GDK_F10 GDK_KEY_F10
#define GDK_F11 GDK_KEY_F11
#define GDK_F12 GDK_KEY_F12
#endif

G_END_DECLS

#endif /* _VIRT_GTK_COMPAT */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
