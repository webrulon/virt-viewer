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

#include <glib/gi18n.h>

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
static void virt_viewer_display_spice_channel_new(SpiceSession *s,
						  SpiceChannel *channel,
						  VirtViewerDisplay *display);
static void virt_viewer_display_spice_channel_destroy(SpiceSession *s,
						      SpiceChannel *channel,
						      VirtViewerDisplay *display);

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

	if (self->session) {
		spice_session_disconnect(self->session);
		g_object_unref(self->session);

		if (self->audio)
			g_object_unref(self->audio);
		self->audio = NULL;

		if (self->display)
			gtk_container_remove(GTK_CONTAINER(self), GTK_WIDGET(self->display));
		self->display = NULL;
	}

	self->session = spice_session_new();
	g_signal_connect(self->session, "channel-new",
			 G_CALLBACK(virt_viewer_display_spice_channel_new), self);
	g_signal_connect(self->session, "channel-destroy",
			 G_CALLBACK(virt_viewer_display_spice_channel_destroy), self);

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
	g_signal_emit_by_name(display, "display-channel-open", channel);
}

static void
virt_viewer_display_spice_main_channel_event(SpiceChannel *channel G_GNUC_UNUSED,
					     SpiceChannelEvent event,
					     VirtViewerDisplay *display)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);
	char *password = NULL;

	g_return_if_fail(self != NULL);

	switch (event) {
	case SPICE_CHANNEL_OPENED:
		DEBUG_LOG("main channel: opened");
		break;
	case SPICE_CHANNEL_CLOSED:
		DEBUG_LOG("main channel: closed");
		g_signal_emit_by_name(display, "display-disconnected");
		break;
	case SPICE_CHANNEL_ERROR_CONNECT:
		DEBUG_LOG("main channel: failed to connect");
		g_signal_emit_by_name(display, "display-disconnected");
		break;
	case SPICE_CHANNEL_ERROR_AUTH:
		DEBUG_LOG("main channel: auth failure (wrong password?)");
		int ret = virt_viewer_auth_collect_credentials("SPICE",
							       NULL,
							       NULL, &password);
		if (ret < 0) {
			g_signal_emit_by_name(display, "display-auth-refused",
					      _("Unable to collect credentials"));
		} else {
			g_object_set(self->session, "password", password, NULL);
			spice_session_connect(self->session);
		}
		break;
	default:
		g_warning("unknown main channel event: %d", event);
		g_signal_emit_by_name(display, "display-disconnected");
		break;
	}

	g_free(password);
}


/*
 * Called when desktop size changes.
 *
 * It either tries to resize the main window, or it triggers
 * recalculation of the display within existing window size
 */
static void
virt_viewer_display_spice_primary_create(SpiceChannel *channel G_GNUC_UNUSED,
					 gint format G_GNUC_UNUSED,
					 gint width,
					 gint height,
					 gint stride G_GNUC_UNUSED,
					 gint shmid G_GNUC_UNUSED,
					 gpointer imgdata G_GNUC_UNUSED,
					 VirtViewerDisplay *display)
{
	DEBUG_LOG("desktop resize %dx%d", width, height);

	virt_viewer_display_set_desktop_size(display, width, height);
	g_signal_emit_by_name(display, "display-initialized");
	g_signal_emit_by_name(display, "display-desktop-resize");
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
		if (id != 0)
			return;

		DEBUG_LOG("new display channel (#%d)", id);
		g_signal_connect(channel, "display-primary-create",
				 G_CALLBACK(virt_viewer_display_spice_primary_create), display);

		self->display = spice_display_new(s, id);
		gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(self->display));
		gtk_widget_show(GTK_WIDGET(self->display));
		g_object_set(self->display,
			     "grab-keyboard", TRUE,
			     "grab-mouse", TRUE,
			     "resize-guest", FALSE,
			     "scaling", TRUE,
			     "auto-clipboard", TRUE,
			     NULL);

		g_signal_emit_by_name(display, "display-connected");
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

	if (SPICE_IS_PLAYBACK_CHANNEL(channel) && self->audio) {
		DEBUG_LOG("zap audio channel");
		g_object_unref(self->audio);
		self->audio = NULL;
	}
}

GtkWidget *
virt_viewer_display_spice_new(void)
{
	VirtViewerDisplaySpice *self;

	self = g_object_new(VIRT_VIEWER_TYPE_DISPLAY_SPICE, NULL);

	self->session = spice_session_new();
	g_signal_connect(self->session, "channel-new",
			 G_CALLBACK(virt_viewer_display_spice_channel_new), self);
	g_signal_connect(self->session, "channel-destroy",
			 G_CALLBACK(virt_viewer_display_spice_channel_destroy), self);

	return GTK_WIDGET(self);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 *  indent-tabs-mode: t
 * End:
 */
