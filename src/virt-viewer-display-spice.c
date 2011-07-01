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

#include <spice-audio.h>

#include "virt-viewer-util.h"
#include "virt-viewer-display-spice.h"
#include "virt-viewer-auth.h"

G_DEFINE_TYPE (VirtViewerDisplaySpice, virt_viewer_display_spice, VIRT_VIEWER_TYPE_DISPLAY)


static void virt_viewer_display_spice_close(VirtViewerDisplay *display);
static void virt_viewer_display_spice_send_keys(VirtViewerDisplay *display, const guint *keyvals, int nkeyvals);
static GdkPixbuf *virt_viewer_display_spice_get_pixbuf(VirtViewerDisplay *display);
static gboolean virt_viewer_display_spice_open_fd(VirtViewerDisplay *display, int fd);
static gboolean virt_viewer_display_spice_open_host(VirtViewerDisplay *display, char *host, char *port);
static gboolean virt_viewer_display_spice_channel_open_fd(VirtViewerDisplay *display, VirtViewerDisplayChannel *channel, int fd);


static void
virt_viewer_display_spice_class_init(VirtViewerDisplaySpiceClass *klass)
{
	VirtViewerDisplayClass *dclass = VIRT_VIEWER_DISPLAY_CLASS(klass);

	dclass->close = virt_viewer_display_spice_close;
	dclass->send_keys = virt_viewer_display_spice_send_keys;
	dclass->get_pixbuf = virt_viewer_display_spice_get_pixbuf;
	dclass->open_fd = virt_viewer_display_spice_open_fd;
	dclass->open_host = virt_viewer_display_spice_open_host;
	dclass->channel_open_fd = virt_viewer_display_spice_channel_open_fd;
}

static void
virt_viewer_display_spice_init(VirtViewerDisplaySpice *self G_GNUC_UNUSED)
{
}

static void
virt_viewer_display_spice_send_keys(VirtViewerDisplay *display,
				    const guint *keyvals,
				    int nkeyvals)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

	g_return_if_fail(self != NULL);
	g_return_if_fail(self->display != NULL);

	spice_display_send_keys(self->display, keyvals, nkeyvals, SPICE_DISPLAY_KEY_EVENT_CLICK);
}

static GdkPixbuf *
virt_viewer_display_spice_get_pixbuf(VirtViewerDisplay *display)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(self->display != NULL, NULL);

	return spice_display_get_pixbuf(self->display);
}

static void
virt_viewer_display_spice_close(VirtViewerDisplay *display)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

	g_return_if_fail(self != NULL);

	if (self->session == NULL)
		return;

	spice_session_disconnect(self->session);

	if (self->session) /* let viewer_quit() be reentrant */
		g_object_unref(self->session);
	self->session = NULL;

	if (self->audio)
		g_object_unref(self->audio);
	self->audio = NULL;
}

static gboolean
virt_viewer_display_spice_open_host(VirtViewerDisplay *display,
				    char *host,
				    char *port)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(self->session != NULL, FALSE);

	g_object_set(self->session,
		     "host", host,
		     "port", port,
		     NULL);

	return spice_session_connect(self->session);
}

static gboolean
virt_viewer_display_spice_open_fd(VirtViewerDisplay *display,
				  int fd)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

	g_return_val_if_fail(self != NULL, FALSE);

	return spice_session_open_fd(self->session, fd);
}

static gboolean
virt_viewer_display_spice_channel_open_fd(VirtViewerDisplay *display,
					  VirtViewerDisplayChannel *channel,
					  int fd)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

	g_return_val_if_fail(self != NULL, FALSE);

	return spice_channel_open_fd(SPICE_CHANNEL(channel), fd);
}

static void
virt_viewer_display_spice_channel_open_fd_request(SpiceChannel *channel,
						  gint tls G_GNUC_UNUSED,
						  VirtViewerDisplay *display)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

	g_return_if_fail(self != NULL);

	virt_viewer_channel_open_fd(display->viewer, (VirtViewerDisplayChannel *)channel);
}

static void
virt_viewer_display_spice_main_channel_event(SpiceChannel *channel G_GNUC_UNUSED,
					     SpiceChannelEvent event,
					     VirtViewerDisplay *display)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);
	char *password = NULL;

	g_return_if_fail(self != NULL);
	g_return_if_fail(display->viewer != NULL);

	switch (event) {
	case SPICE_CHANNEL_OPENED:
		DEBUG_LOG("main channel: opened");
		break;
	case SPICE_CHANNEL_CLOSED:
		DEBUG_LOG("main channel: closed");
		virt_viewer_quit(display->viewer);
		break;
	case SPICE_CHANNEL_ERROR_CONNECT:
		DEBUG_LOG("main channel: failed to connect");
		virt_viewer_disconnected(display->viewer);
		break;
	case SPICE_CHANNEL_ERROR_AUTH:
		DEBUG_LOG("main channel: auth failure (wrong password?)");
		int ret = virt_viewer_auth_collect_credentials("SPICE",
							       display->viewer->pretty_address,
							       NULL, &password);
		if (ret < 0) {
			virt_viewer_quit(display->viewer);
		} else {
			g_object_set(self->session, "password", password, NULL);
			spice_session_connect(self->session);
		}
		break;
	default:
		g_warning("unknown main channel event: %d", event);
		virt_viewer_disconnected(display->viewer);
		break;
	}

	g_free(password);
}


/*
 * Triggers a resize of the main container to indirectly cause
 * the display widget to be resized to fit the available space
 */
static void
virt_viewer_display_spice_resize_widget(VirtViewer *viewer)
{
	gtk_widget_queue_resize(viewer->align);
}


/*
 * Called when desktop size changes.
 *
 * It either tries to resize the main window, or it triggers
 * recalculation of the display within existing window size
 */
static void
virt_viewer_display_spice_resize_desktop(SpiceChannel *channel G_GNUC_UNUSED,
					 gint format G_GNUC_UNUSED,
					 gint width,
					 gint height,
					 gint stride G_GNUC_UNUSED,
					 gint shmid G_GNUC_UNUSED,
					 gpointer imgdata G_GNUC_UNUSED,
					 VirtViewer *viewer)
{
	DEBUG_LOG("desktop resize %dx%d", width, height);
	viewer->desktopWidth = width;
	viewer->desktopHeight = height;

	if (viewer->autoResize && viewer->window && !viewer->fullscreen) {
		virt_viewer_resize_main_window(viewer);
	} else {
		virt_viewer_display_spice_resize_widget(viewer);
	}
}


/*
 * Called when the main container widget's size has been set.
 * It attempts to fit the display widget into this space while
 * maintaining aspect ratio
 */
static gboolean
virt_viewer_display_spice_resize_align(GtkWidget *widget,
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

	if (viewer->desktopWidth == 0 || viewer->desktopHeight == 0)
		desktopAspect = 1;
	else
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

static void
virt_viewer_display_spice_channel_new(SpiceSession *s,
				      SpiceChannel *channel,
				      VirtViewerDisplay *display)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);
	int id;

	g_return_if_fail(self != NULL);

	g_signal_connect(channel, "open-fd",
			 G_CALLBACK(virt_viewer_display_spice_channel_open_fd_request), self);

	g_object_get(channel, "channel-id", &id, NULL);

	if (SPICE_IS_MAIN_CHANNEL(channel)) {
		g_signal_connect(channel, "channel-event",
				 G_CALLBACK(virt_viewer_display_spice_main_channel_event), self);
	}

	if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
		DEBUG_LOG("new display channel (#%d)", id);
		if (display->widget != NULL)
			return;

		g_signal_connect(channel, "display-primary-create",
				 G_CALLBACK(virt_viewer_display_spice_resize_desktop), display->viewer);

		self->display = spice_display_new(s, id);
		display->widget = GTK_WIDGET(self->display);
		g_object_set(self->display,
			     "grab-keyboard", TRUE,
			     "grab-mouse", TRUE,
			     "resize-guest", FALSE,
			     "scaling", TRUE,
			     "auto-clipboard", TRUE,
			     NULL);
		virt_viewer_add_display_and_realize(display->viewer);

		g_signal_connect(display->viewer->align, "size-allocate",
				 G_CALLBACK(virt_viewer_display_spice_resize_align), display->viewer);

		virt_viewer_initialized(display->viewer);
	}

	if (SPICE_IS_INPUTS_CHANNEL(channel)) {
		DEBUG_LOG("new inputs channel");
	}

	if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
		DEBUG_LOG("new audio channel");
		if (self->audio != NULL)
			return;
		self->audio = spice_audio_new(s, NULL, NULL);
	}
}

static void
virt_viewer_display_spice_channel_destroy(G_GNUC_UNUSED SpiceSession *s,
					  SpiceChannel *channel,
					  VirtViewerDisplay *display)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);
	int id;

	g_return_if_fail(self != NULL);

	g_object_get(channel, "channel-id", &id, NULL);
	if (SPICE_IS_MAIN_CHANNEL(channel)) {
		DEBUG_LOG("zap main channel");
	}

	if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
		DEBUG_LOG("zap display channel (#%d)", id);
	}

	if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
		if (self->audio == NULL)
			return;
		DEBUG_LOG("zap audio channel");
	}
}

VirtViewerDisplaySpice *
virt_viewer_display_spice_new(VirtViewer *viewer)
{
	VirtViewerDisplaySpice *self;
	VirtViewerDisplay *d;

	g_return_val_if_fail(viewer != NULL, NULL);

	self = g_object_new(VIRT_VIEWER_TYPE_DISPLAY_SPICE, NULL);
	d = VIRT_VIEWER_DISPLAY(self);
	d->viewer = viewer;

	self->session = spice_session_new();
	g_signal_connect(self->session, "channel-new",
			 G_CALLBACK(virt_viewer_display_spice_channel_new), self);
	g_signal_connect(self->session, "channel-destroy",
			 G_CALLBACK(virt_viewer_display_spice_channel_destroy), self);

	return self;
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
