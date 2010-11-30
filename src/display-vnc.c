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

#include "auth.h"
#include "display-vnc.h"

G_DEFINE_TYPE(VirtViewerDisplayVNC, virt_viewer_display_vnc, VIRT_VIEWER_TYPE_DISPLAY)

static void _vnc_close(VirtViewerDisplay* display);
static void _vnc_send_keys(VirtViewerDisplay* display, const guint *keyvals, int nkeyvals);
static GdkPixbuf* _vnc_get_pixbuf(VirtViewerDisplay* display);
static gboolean _vnc_open_fd(VirtViewerDisplay* display, int fd);
static gboolean _vnc_open_host(VirtViewerDisplay* display, char *host, char *port);
static gboolean _vnc_channel_open_fd(VirtViewerDisplay* display,
				     VirtViewerDisplayChannel* channel, int fd);

static void virt_viewer_display_vnc_class_init(VirtViewerDisplayVNCClass *klass)
{
	VirtViewerDisplayClass *dclass = VIRT_VIEWER_DISPLAY_CLASS(klass);

	dclass->close = _vnc_close;
	dclass->send_keys = _vnc_send_keys;
	dclass->get_pixbuf = _vnc_get_pixbuf;
	dclass->open_fd = _vnc_open_fd;
	dclass->open_host = _vnc_open_host;
	dclass->channel_open_fd = _vnc_channel_open_fd;
}

static void virt_viewer_display_vnc_init(VirtViewerDisplayVNC *self G_GNUC_UNUSED)
{
}

static void _vnc_mouse_grab(GtkWidget *vnc G_GNUC_UNUSED, VirtViewerDisplayVNC *self)
{
	viewer_set_title(VIRT_VIEWER_DISPLAY(self)->viewer, TRUE);
}

static void _vnc_mouse_ungrab(GtkWidget *vnc G_GNUC_UNUSED, VirtViewerDisplayVNC *self)
{
	viewer_set_title(VIRT_VIEWER_DISPLAY(self)->viewer, FALSE);
}

static void _vnc_key_grab(GtkWidget *vnc G_GNUC_UNUSED, VirtViewerDisplayVNC *self)
{
	viewer_disable_modifiers(VIRT_VIEWER_DISPLAY(self)->viewer);
}

static void _vnc_key_ungrab(GtkWidget *vnc G_GNUC_UNUSED, VirtViewerDisplayVNC *self)
{
	viewer_enable_modifiers(VIRT_VIEWER_DISPLAY(self)->viewer);
}

static void _vnc_send_keys(VirtViewerDisplay* display, const guint *keyvals, int nkeyvals)
{
	VirtViewerDisplayVNC *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_if_fail(self != NULL);
	g_return_if_fail(keyvals != NULL);
	g_return_if_fail(self->vnc != NULL);

	vnc_display_send_keys(self->vnc, keyvals, nkeyvals);
}

static GdkPixbuf* _vnc_get_pixbuf(VirtViewerDisplay* display)
{
	VirtViewerDisplayVNC *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(self->vnc != NULL, NULL);

	return vnc_display_get_pixbuf(self->vnc);
}

static gboolean _vnc_open_fd(VirtViewerDisplay* display, int fd)
{
	VirtViewerDisplayVNC *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(self->vnc != NULL, FALSE);

	return vnc_display_open_fd(self->vnc, fd);
}

static gboolean _vnc_channel_open_fd(VirtViewerDisplay* display G_GNUC_UNUSED,
				     VirtViewerDisplayChannel* channel G_GNUC_UNUSED,
				     int fd G_GNUC_UNUSED)
{
	g_warning("channel_open_fd is not supported by VNC");
	return FALSE;
}

static gboolean _vnc_open_host(VirtViewerDisplay* display, char *host, char *port)
{
	VirtViewerDisplayVNC *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(self->vnc != NULL, FALSE);

	return vnc_display_open_host(self->vnc, host, port);
}

static void _vnc_close(VirtViewerDisplay* display)
{
	VirtViewerDisplayVNC *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_if_fail(self != NULL);

	if (self->vnc != NULL)
		vnc_display_close(self->vnc);
 }

static void viewer_bell(VirtViewer *viewer, gpointer data G_GNUC_UNUSED)
{
	gdk_window_beep(GTK_WIDGET(viewer->window)->window);
}

static void viewer_vnc_auth_unsupported(VirtViewer *viewer,
					unsigned int authType, gpointer data G_GNUC_UNUSED)
{
	viewer_simple_message_dialog(viewer->window,
				     _("Unable to authenticate with VNC server at %s\n"
				       "Unsupported authentication type %d"),
				     viewer->pretty_address, authType);
}

static void viewer_vnc_auth_failure(VirtViewer *viewer,
				    const char *reason, gpointer data G_GNUC_UNUSED)
{
	GtkWidget *dialog;
	int ret;

	dialog = gtk_message_dialog_new(GTK_WINDOW(viewer->window),
					GTK_DIALOG_MODAL |
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_YES_NO,
					_("Unable to authenticate with VNC server at %s: %s\n"
					  "Retry connection again?"),
					viewer->pretty_address, reason);

	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	if (ret == GTK_RESPONSE_YES)
		viewer->authretry = TRUE;
	else
		viewer->authretry = FALSE;
}

/*
 * Triggers a resize of the main container to indirectly cause
 * the display widget to be resized to fit the available space
 */
static void
viewer_resize_display_widget(VirtViewer *viewer)
{
	GtkWidget *align;
	align = glade_xml_get_widget(viewer->glade, "display-align");
	gtk_widget_queue_resize(align);
}


/*
 * Called when desktop size changes.
 *
 * It either tries to resize the main window, or it triggers
 * recalculation of the display within existing window size
 */
static void viewer_resize_desktop(VirtViewer *viewer, gint width, gint height)
{
	DEBUG_LOG("desktop resize %dx%d", width, height);
	viewer->desktopWidth = width;
	viewer->desktopHeight = height;

	if (viewer->autoResize && viewer->window && !viewer->fullscreen) {
		viewer_resize_main_window(viewer);
	} else {
		viewer_resize_display_widget(viewer);
	}
}


/*
 * Called when the main container widget's size has been set.
 * It attempts to fit the display widget into this space while
 * maintaining aspect ratio
 */
static gboolean viewer_resize_align(GtkWidget *widget,
				    GtkAllocation *alloc,
				    VirtViewer *viewer)
{
	double desktopAspect;
	double scrollAspect;
	int height, width;
	GtkAllocation child;
	int dx = 0, dy = 0;

	if (!viewer->active) {
		DEBUG_LOG("Skipping inactive resize");
		return TRUE;
	}

	desktopAspect = (double)viewer->desktopWidth / (double)viewer->desktopHeight;
	scrollAspect = (double)alloc->width / (double)alloc->height;

	if (scrollAspect > desktopAspect) {
		width = alloc->height * desktopAspect;
		dx = (alloc->width - width) / 2;
		height = alloc->height;
	} else {
		width = alloc->width;
		height = alloc->width / desktopAspect;
		dy = (alloc->height - height) / 2;
	}

	DEBUG_LOG("Align widget=%p is %dx%d, desktop is %dx%d, setting display to %dx%d",
		  widget,
		  alloc->width, alloc->height,
		  viewer->desktopWidth, viewer->desktopHeight,
		  width, height);

	child.x = alloc->x + dx;
	child.y = alloc->y + dy;
	child.width = width;
	child.height = height;
	if (viewer->display && viewer->display->widget)
		gtk_widget_size_allocate(viewer->display->widget, &child);

	return FALSE;
}

VirtViewerDisplayVNC* virt_viewer_display_vnc_new(VirtViewer *viewer)
{
	VirtViewerDisplayVNC *self;
	VirtViewerDisplay *d;
	GtkWidget *align;

	g_return_val_if_fail(viewer != NULL, NULL);

	self = g_object_new(VIRT_VIEWER_TYPE_DISPLAY_VNC, NULL);
	d = VIRT_VIEWER_DISPLAY(self);
	d->viewer = viewer;
	viewer->display = d;

	d->need_align = TRUE;
	d->widget = vnc_display_new();
	self->vnc = VNC_DISPLAY(d->widget);
	vnc_display_set_keyboard_grab(self->vnc, TRUE);
	vnc_display_set_pointer_grab(self->vnc, TRUE);

	/*
	 * In auto-resize mode we have things setup so that we always
	 * automatically resize the top level window to be exactly the
	 * same size as the VNC desktop, except when it won't fit on
	 * the local screen, at which point we let it scale down.
	 * The upshot is, we always want scaling enabled.
	 * We disable force_size because we want to allow user to
	 * manually size the widget smaller too
	 */
	vnc_display_set_force_size(self->vnc, FALSE);
	vnc_display_set_scaling(self->vnc, TRUE);

	g_signal_connect_swapped(self->vnc, "vnc-connected",
				 G_CALLBACK(viewer_connected), viewer);
	g_signal_connect_swapped(self->vnc, "vnc-initialized",
				 G_CALLBACK(viewer_initialized), viewer);
	g_signal_connect_swapped(self->vnc, "vnc-disconnected",
				 G_CALLBACK(viewer_disconnected), viewer);

	/* When VNC desktop resizes, we have to resize the containing widget */
	g_signal_connect_swapped(self->vnc, "vnc-desktop-resize",
				 G_CALLBACK(viewer_resize_desktop), viewer);
	g_signal_connect_swapped(self->vnc, "vnc-bell",
				 G_CALLBACK(viewer_bell), NULL);
	g_signal_connect_swapped(self->vnc, "vnc-auth-failure",
				 G_CALLBACK(viewer_vnc_auth_failure), viewer);
	g_signal_connect_swapped(self->vnc, "vnc-auth-unsupported",
				 G_CALLBACK(viewer_vnc_auth_unsupported), viewer);
	g_signal_connect_swapped(self->vnc, "vnc-server-cut-text",
				 G_CALLBACK(viewer_server_cut_text), viewer);

	g_signal_connect(self->vnc, "vnc-pointer-grab",
			 G_CALLBACK(_vnc_mouse_grab), self);
	g_signal_connect(self->vnc, "vnc-pointer-ungrab",
			 G_CALLBACK(_vnc_mouse_ungrab), self);
	g_signal_connect(self->vnc, "vnc-keyboard-grab",
			 G_CALLBACK(_vnc_key_grab), self);
	g_signal_connect(self->vnc, "vnc-keyboard-ungrab",
			 G_CALLBACK(_vnc_key_ungrab), self);

	g_signal_connect(self->vnc, "vnc-auth-credential",
			 G_CALLBACK(viewer_auth_vnc_credentials), &viewer->pretty_address);

	viewer_add_display_and_realize(viewer);

	align = glade_xml_get_widget(viewer->glade, "display-align");
	g_signal_connect(align, "size-allocate",
			 G_CALLBACK(viewer_resize_align), viewer);

	return self;
}




/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
