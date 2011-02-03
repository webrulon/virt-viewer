/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2009 Red Hat,
 * Copyright (C) 2009 Daniel P. Berrange
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
#ifndef _VIRT_VIEWER_PRIV_H
# define _VIRT_VIEWER_PRIV_H

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>

typedef struct _VirtViewerDisplay VirtViewerDisplay;
typedef struct _VirtViewerDisplayClass VirtViewerDisplayClass;
typedef struct _VirtViewer VirtViewer;
typedef struct _VirtViewerSize VirtViewerSize;
typedef struct _VirtViewerDisplayChanne VirtViewerDisplayChannel;

enum menuNums {
	FILE_MENU,
	VIEW_MENU,
	SEND_KEY_MENU,
	HELP_MENU,
	LAST_MENU // sentinel
};

struct _VirtViewer {
	char *uri;
	virConnectPtr conn;
	char *domkey;
	char *domtitle;

	GladeXML *glade;
	GtkWidget *window;
	GtkWidget *container;

	char *pretty_address;

	int zoomlevel;

	int desktopWidth;
	int desktopHeight;
	gboolean autoResize;
	gboolean fullscreen;
	gboolean withEvents;

	gboolean active;

	gboolean accelEnabled;
	GValue accelSetting;
	GSList *accelList;
	int accelMenuSig[LAST_MENU];

	gboolean waitvm;
	gboolean reconnect;
	gboolean direct;
	gboolean verbose;
	gboolean authretry;
	gboolean connected;

	gchar *clipboard;

	VirtViewerDisplay *display;

	char *unixsock;
	char *gport;
	char *host;
	char *transport;
	char *user;
        int port;
};

struct _VirtViewerSize {
	VirtViewer *viewer;
	gint width, height;
	gulong sig_id;
};

void viewer_connected(VirtViewer *viewer);
void viewer_initialized(VirtViewer *viewer);
void viewer_disconnected(VirtViewer *viewer);
void viewer_set_status(VirtViewer *viewer, const char *text);
void viewer_set_title(VirtViewer *viewer, gboolean grabbed);
void viewer_enable_modifiers(VirtViewer *viewer);
void viewer_disable_modifiers(VirtViewer *viewer);
void viewer_add_display_and_realize(VirtViewer *viewer);
void viewer_server_cut_text(VirtViewer *viewer, const gchar *text);
void viewer_resize_main_window(VirtViewer *viewer);
void viewer_channel_open_fd(VirtViewer *viewer, VirtViewerDisplayChannel *channel);
void viewer_quit(VirtViewer *viewer);

void viewer_simple_message_dialog(GtkWidget *window, const char *fmt, ...);

#endif // _VIRT_VIEWER_PRIV_H
/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
