/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2009 Red Hat,
 * Copyright (C) 2009 Daniel P. Berrange
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

#ifndef VIRT_VIEWER_UTIL_H
#define VIRT_VIEWER_UTIL_H

#include <gtk/gtk.h>
#include <glade/glade.h>

#if (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION <= 12)
#define gtk_widget_get_window(widget) ((widget)->window)
#endif

extern gboolean doDebug;

#define DEBUG_LOG(s, ...) do { if (doDebug) g_debug((s), ## __VA_ARGS__); } while (0)
#define ARRAY_CARDINALITY(Array) (sizeof (Array) / sizeof *(Array))


GladeXML *viewer_load_glade(const char *name, const char *widget);

#endif
