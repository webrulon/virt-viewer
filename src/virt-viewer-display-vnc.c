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

#include "virt-viewer-auth.h"
#include "virt-viewer-display-vnc.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE(VirtViewerDisplayVnc, virt_viewer_display_vnc, VIRT_VIEWER_TYPE_DISPLAY)

static void virt_viewer_display_vnc_close(VirtViewerDisplay* display);
static void virt_viewer_display_vnc_send_keys(VirtViewerDisplay* display, const guint *keyvals, int nkeyvals);
static GdkPixbuf* virt_viewer_display_vnc_get_pixbuf(VirtViewerDisplay* display);
static gboolean virt_viewer_display_vnc_open_fd(VirtViewerDisplay* display, int fd);
static gboolean virt_viewer_display_vnc_open_host(VirtViewerDisplay* display, char *host, char *port);
static gboolean virt_viewer_display_vnc_channel_open_fd(VirtViewerDisplay* display,
							VirtViewerDisplayChannel* channel, int fd);

static void
virt_viewer_display_vnc_class_init(VirtViewerDisplayVncClass *klass)
{
	VirtViewerDisplayClass *dclass = VIRT_VIEWER_DISPLAY_CLASS(klass);

	dclass->close = virt_viewer_display_vnc_close;
	dclass->send_keys = virt_viewer_display_vnc_send_keys;
	dclass->get_pixbuf = virt_viewer_display_vnc_get_pixbuf;
	dclass->open_fd = virt_viewer_display_vnc_open_fd;
	dclass->open_host = virt_viewer_display_vnc_open_host;
	dclass->channel_open_fd = virt_viewer_display_vnc_channel_open_fd;
}

static void
virt_viewer_display_vnc_init(VirtViewerDisplayVnc *self G_GNUC_UNUSED)
{
}

static void
virt_viewer_display_vnc_connected(VncDisplay *vnc G_GNUC_UNUSED,
				  VirtViewerDisplay *display)
{
	g_signal_emit_by_name(display, "display-connected");
}

static void
virt_viewer_display_vnc_disconnected(VncDisplay *vnc G_GNUC_UNUSED,
				     VirtViewerDisplay *display)
{
	g_signal_emit_by_name(display, "display-disconnected");
}

static void
virt_viewer_display_vnc_initialized(VncDisplay *vnc G_GNUC_UNUSED,
				    VirtViewerDisplay *display)
{
	g_signal_emit_by_name(display, "display-initialized");
}

static void
virt_viewer_display_vnc_cut_text(VncDisplay *vnc G_GNUC_UNUSED,
				 const char *text,
				 VirtViewerDisplay *display)
{
	g_signal_emit_by_name(display, "display-cut-text", text);
}

static void
virt_viewer_display_vnc_mouse_grab(VncDisplay *vnc G_GNUC_UNUSED,
				   VirtViewerDisplay *display)
{
	g_signal_emit_by_name(display, "display-pointer-grab");
}


static void
virt_viewer_display_vnc_mouse_ungrab(VncDisplay *vnc G_GNUC_UNUSED,
				     VirtViewerDisplay *display)
{
	g_signal_emit_by_name(display, "display-pointer-ungrab");
}

static void
virt_viewer_display_vnc_key_grab(VncDisplay *vnc G_GNUC_UNUSED,
				 VirtViewerDisplay *display)
{
	g_signal_emit_by_name(display, "display-keyboard-grab");
}

static void
virt_viewer_display_vnc_key_ungrab(VncDisplay *vnc G_GNUC_UNUSED,
				   VirtViewerDisplay *display)
{
	g_signal_emit_by_name(display, "display-keyboard-ungrab");
}

static void
virt_viewer_display_vnc_send_keys(VirtViewerDisplay* display,
				  const guint *keyvals,
				  int nkeyvals)
{
	VirtViewerDisplayVnc *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_if_fail(self != NULL);
	g_return_if_fail(keyvals != NULL);
	g_return_if_fail(self->vnc != NULL);

	vnc_display_send_keys(self->vnc, keyvals, nkeyvals);
}

static GdkPixbuf*
virt_viewer_display_vnc_get_pixbuf(VirtViewerDisplay* display)
{
	VirtViewerDisplayVnc *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(self->vnc != NULL, NULL);

	return vnc_display_get_pixbuf(self->vnc);
}

static gboolean
virt_viewer_display_vnc_open_fd(VirtViewerDisplay* display,
				int fd)
{
	VirtViewerDisplayVnc *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(self->vnc != NULL, FALSE);

	return vnc_display_open_fd(self->vnc, fd);
}

static gboolean
virt_viewer_display_vnc_channel_open_fd(VirtViewerDisplay* display G_GNUC_UNUSED,
					VirtViewerDisplayChannel* channel G_GNUC_UNUSED,
					int fd G_GNUC_UNUSED)
{
	g_warning("channel_open_fd is not supported by VNC");
	return FALSE;
}

static gboolean
virt_viewer_display_vnc_open_host(VirtViewerDisplay* display,
				  char *host,
				  char *port)
{
	VirtViewerDisplayVnc *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(self->vnc != NULL, FALSE);

	return vnc_display_open_host(self->vnc, host, port);
}

static void
virt_viewer_display_vnc_close(VirtViewerDisplay* display)
{
	VirtViewerDisplayVnc *self = VIRT_VIEWER_DISPLAY_VNC(display);

	g_return_if_fail(self != NULL);

	if (self->vnc != NULL)
		vnc_display_close(self->vnc);
 }

static void
virt_viewer_display_vnc_bell(VncDisplay *vnc G_GNUC_UNUSED,
			     VirtViewerDisplay *display)
{
	g_signal_emit_by_name(display, "display-bell");
}

static void
virt_viewer_display_vnc_auth_unsupported(VncDisplay *vnc G_GNUC_UNUSED,
					 unsigned int authType,
					 VirtViewerDisplay *display)
{
	char *msg = g_strdup_printf(_("Unsupported authentication type %d"),
				    authType);
	g_signal_emit_by_name(display, "display-auth-failed", msg);
	g_free(msg);
}

static void
virt_viewer_display_vnc_auth_failure(VncDisplay *vnc G_GNUC_UNUSED,
				     const char *reason,
				     VirtViewerDisplay *display)
{

	g_signal_emit_by_name(display, "display-auth-refused", reason);
}

/*
 * Called when desktop size changes.
 *
 * It either tries to resize the main window, or it triggers
 * recalculation of the display within existing window size
 */
static void
virt_viewer_display_vnc_resize_desktop(VncDisplay *vnc G_GNUC_UNUSED,
				       int width, int height,
				       VirtViewerDisplay *display)
{
	DEBUG_LOG("desktop resize %dx%d", width, height);

	virt_viewer_display_set_desktop_size(display, width, height);
	g_signal_emit_by_name(display, "display-desktop-resize");
}


GtkWidget *
virt_viewer_display_vnc_new(void)
{
	VirtViewerDisplayVnc *display;

	display = g_object_new(VIRT_VIEWER_TYPE_DISPLAY_VNC, NULL);

	display->vnc = VNC_DISPLAY(vnc_display_new());
	gtk_container_add(GTK_CONTAINER(display), GTK_WIDGET(display->vnc));
	vnc_display_set_keyboard_grab(display->vnc, TRUE);
	vnc_display_set_pointer_grab(display->vnc, TRUE);

	/*
	 * In auto-resize mode we have things setup so that we always
	 * automatically resize the top level window to be exactly the
	 * same size as the VNC desktop, except when it won't fit on
	 * the local screen, at which point we let it scale down.
	 * The upshot is, we always want scaling enabled.
	 * We disable force_size because we want to allow user to
	 * manually size the widget smaller too
	 */
	vnc_display_set_force_size(display->vnc, FALSE);
	vnc_display_set_scaling(display->vnc, TRUE);

	g_signal_connect(display->vnc, "vnc-connected",
			 G_CALLBACK(virt_viewer_display_vnc_connected), display);
	g_signal_connect(display->vnc, "vnc-initialized",
			 G_CALLBACK(virt_viewer_display_vnc_initialized), display);
	g_signal_connect(display->vnc, "vnc-disconnected",
			 G_CALLBACK(virt_viewer_display_vnc_disconnected), display);

	/* When VNC desktop resizes, we have to resize the containing widget */
	g_signal_connect(display->vnc, "vnc-desktop-resize",
			 G_CALLBACK(virt_viewer_display_vnc_resize_desktop), display);
	g_signal_connect(display->vnc, "vnc-bell",
			 G_CALLBACK(virt_viewer_display_vnc_bell), display);
	g_signal_connect(display->vnc, "vnc-auth-failure",
			 G_CALLBACK(virt_viewer_display_vnc_auth_failure), display);
	g_signal_connect(display->vnc, "vnc-auth-unsupported",
			 G_CALLBACK(virt_viewer_display_vnc_auth_unsupported), display);
	g_signal_connect(display->vnc, "vnc-server-cut-text",
			 G_CALLBACK(virt_viewer_display_vnc_cut_text), display);

	g_signal_connect(display->vnc, "vnc-pointer-grab",
			 G_CALLBACK(virt_viewer_display_vnc_mouse_grab), display);
	g_signal_connect(display->vnc, "vnc-pointer-ungrab",
			 G_CALLBACK(virt_viewer_display_vnc_mouse_ungrab), display);
	g_signal_connect(display->vnc, "vnc-keyboard-grab",
			 G_CALLBACK(virt_viewer_display_vnc_key_grab), display);
	g_signal_connect(display->vnc, "vnc-keyboard-ungrab",
			 G_CALLBACK(virt_viewer_display_vnc_key_ungrab), display);

	g_signal_connect(display->vnc, "vnc-auth-credential",
			 G_CALLBACK(virt_viewer_auth_vnc_credentials), NULL);

	return GTK_WIDGET(display);
}




/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
