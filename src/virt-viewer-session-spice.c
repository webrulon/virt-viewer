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
#include "virt-viewer-session-spice.h"
#include "virt-viewer-display-spice.h"
#include "virt-viewer-auth.h"

G_DEFINE_TYPE (VirtViewerSessionSpice, virt_viewer_session_spice, VIRT_VIEWER_TYPE_SESSION)


struct _VirtViewerSessionSpicePrivate {
	SpiceSession *session;
	SpiceAudio *audio;
};

#define VIRT_VIEWER_SESSION_SPICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), VIRT_VIEWER_TYPE_SESSION_SPICE, VirtViewerSessionSpicePrivate))

static void virt_viewer_session_spice_close(VirtViewerSession *session);
static gboolean virt_viewer_session_spice_open_fd(VirtViewerSession *session, int fd);
static gboolean virt_viewer_session_spice_open_host(VirtViewerSession *session, char *host, char *port);
static gboolean virt_viewer_session_spice_channel_open_fd(VirtViewerSession *session, VirtViewerSessionChannel *channel, int fd);
static void virt_viewer_session_spice_channel_new(SpiceSession *s,
						  SpiceChannel *channel,
						  VirtViewerSession *session);
static void virt_viewer_session_spice_channel_destroy(SpiceSession *s,
						      SpiceChannel *channel,
						      VirtViewerSession *session);


static void
virt_viewer_session_spice_finalize(GObject *obj)
{
	VirtViewerSessionSpice *spice = VIRT_VIEWER_SESSION_SPICE(obj);

	if (spice->priv->session) {
		spice_session_disconnect(spice->priv->session);
		g_object_unref(spice->priv->session);
	}
	if (spice->priv->audio)
		g_object_unref(spice->priv->audio);

	G_OBJECT_CLASS(virt_viewer_session_spice_parent_class)->finalize(obj);
}


static void
virt_viewer_session_spice_class_init(VirtViewerSessionSpiceClass *klass)
{
	VirtViewerSessionClass *dclass = VIRT_VIEWER_SESSION_CLASS(klass);
	GObjectClass *oclass = G_OBJECT_CLASS(klass);

	oclass->finalize = virt_viewer_session_spice_finalize;

	dclass->close = virt_viewer_session_spice_close;
	dclass->open_fd = virt_viewer_session_spice_open_fd;
	dclass->open_host = virt_viewer_session_spice_open_host;
	dclass->channel_open_fd = virt_viewer_session_spice_channel_open_fd;

	g_type_class_add_private(oclass, sizeof(VirtViewerSessionSpicePrivate));
}

static void
virt_viewer_session_spice_init(VirtViewerSessionSpice *self G_GNUC_UNUSED)
{
	self->priv = VIRT_VIEWER_SESSION_SPICE_GET_PRIVATE(self);
}

static void
virt_viewer_session_spice_close(VirtViewerSession *session)
{
	VirtViewerSessionSpice *self = VIRT_VIEWER_SESSION_SPICE(session);

	g_return_if_fail(self != NULL);

	virt_viewer_session_clear_displays(session);

	if (self->priv->session) {
		spice_session_disconnect(self->priv->session);
		g_object_unref(self->priv->session);

		if (self->priv->audio)
			g_object_unref(self->priv->audio);
		self->priv->audio = NULL;
	}

	self->priv->session = spice_session_new();
	g_signal_connect(self->priv->session, "channel-new",
			 G_CALLBACK(virt_viewer_session_spice_channel_new), self);
	g_signal_connect(self->priv->session, "channel-destroy",
			 G_CALLBACK(virt_viewer_session_spice_channel_destroy), self);

}

static gboolean
virt_viewer_session_spice_open_host(VirtViewerSession *session,
				    char *host,
				    char *port)
{
	VirtViewerSessionSpice *self = VIRT_VIEWER_SESSION_SPICE(session);

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(self->priv->session != NULL, FALSE);

	g_object_set(self->priv->session,
		     "host", host,
		     "port", port,
		     NULL);

	return spice_session_connect(self->priv->session);
}

static gboolean
virt_viewer_session_spice_open_fd(VirtViewerSession *session,
				  int fd)
{
	VirtViewerSessionSpice *self = VIRT_VIEWER_SESSION_SPICE(session);

	g_return_val_if_fail(self != NULL, FALSE);

	return spice_session_open_fd(self->priv->session, fd);
}

static gboolean
virt_viewer_session_spice_channel_open_fd(VirtViewerSession *session,
					  VirtViewerSessionChannel *channel,
					  int fd)
{
	VirtViewerSessionSpice *self = VIRT_VIEWER_SESSION_SPICE(session);

	g_return_val_if_fail(self != NULL, FALSE);

	return spice_channel_open_fd(SPICE_CHANNEL(channel), fd);
}

static void
virt_viewer_session_spice_channel_open_fd_request(SpiceChannel *channel,
						  gint tls G_GNUC_UNUSED,
						  VirtViewerSession *session)
{
	g_signal_emit_by_name(session, "session-channel-open", channel);
}

static void
virt_viewer_session_spice_main_channel_event(SpiceChannel *channel G_GNUC_UNUSED,
					     SpiceChannelEvent event,
					     VirtViewerSession *session)
{
	VirtViewerSessionSpice *self = VIRT_VIEWER_SESSION_SPICE(session);
	char *password = NULL;

	g_return_if_fail(self != NULL);

	switch (event) {
	case SPICE_CHANNEL_OPENED:
		DEBUG_LOG("main channel: opened");
		break;
	case SPICE_CHANNEL_CLOSED:
		DEBUG_LOG("main channel: closed");
		g_signal_emit_by_name(session, "session-disconnected");
		break;
	case SPICE_CHANNEL_ERROR_CONNECT:
		DEBUG_LOG("main channel: failed to connect");
		g_signal_emit_by_name(session, "session-disconnected");
		break;
	case SPICE_CHANNEL_ERROR_AUTH:
		DEBUG_LOG("main channel: auth failure (wrong password?)");
		int ret = virt_viewer_auth_collect_credentials("SPICE",
							       NULL,
							       NULL, &password);
		if (ret < 0) {
			g_signal_emit_by_name(session, "session-auth-refused",
					      _("Unable to collect credentials"));
		} else {
			g_object_set(self->priv->session, "password", password, NULL);
			spice_session_connect(self->priv->session);
		}
		break;
	default:
		g_message("unhandled spice main channel event: %d", event);
		g_signal_emit_by_name(session, "session-disconnected");
		break;
	}

	g_free(password);
}


static void
virt_viewer_session_spice_channel_new(SpiceSession *s,
				      SpiceChannel *channel,
				      VirtViewerSession *session)
{
	VirtViewerSessionSpice *self = VIRT_VIEWER_SESSION_SPICE(session);
	int id;

	g_return_if_fail(self != NULL);

	g_signal_connect(channel, "open-fd",
			 G_CALLBACK(virt_viewer_session_spice_channel_open_fd_request), self);

	g_object_get(channel, "channel-id", &id, NULL);

	if (SPICE_IS_MAIN_CHANNEL(channel)) {
		g_signal_connect(channel, "channel-event",
				 G_CALLBACK(virt_viewer_session_spice_main_channel_event), self);
	}

	if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
		GtkWidget *display;

		g_signal_emit_by_name(session, "session-connected");

		DEBUG_LOG("new session channel (#%d)", id);
		display = virt_viewer_display_spice_new(channel,
							spice_display_new(s, id));

		virt_viewer_session_add_display(VIRT_VIEWER_SESSION(session),
						VIRT_VIEWER_DISPLAY(display));

		g_signal_emit_by_name(session, "session-initialized");
	}

	if (SPICE_IS_INPUTS_CHANNEL(channel)) {
		DEBUG_LOG("new inputs channel");
	}

	if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
		DEBUG_LOG("new audio channel");
		if (self->priv->audio != NULL)
			return;
		self->priv->audio = spice_audio_new(s, NULL, NULL);
	}
}

static void
virt_viewer_session_spice_channel_destroy(G_GNUC_UNUSED SpiceSession *s,
					  SpiceChannel *channel,
					  VirtViewerSession *session)
{
	VirtViewerSessionSpice *self = VIRT_VIEWER_SESSION_SPICE(session);
	int id;

	g_return_if_fail(self != NULL);

	g_object_get(channel, "channel-id", &id, NULL);
	if (SPICE_IS_MAIN_CHANNEL(channel)) {
		DEBUG_LOG("zap main channel");
	}

	if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
		DEBUG_LOG("zap session channel (#%d)", id);
	}

	if (SPICE_IS_PLAYBACK_CHANNEL(channel) && self->priv->audio) {
		DEBUG_LOG("zap audio channel");
		g_object_unref(self->priv->audio);
		self->priv->audio = NULL;
	}
}

VirtViewerSession *
virt_viewer_session_spice_new(void)
{
	VirtViewerSessionSpice *self;

	self = g_object_new(VIRT_VIEWER_TYPE_SESSION_SPICE, NULL);

	self->priv->session = spice_session_new();
	g_signal_connect(self->priv->session, "channel-new",
			 G_CALLBACK(virt_viewer_session_spice_channel_new), self);
	g_signal_connect(self->priv->session, "channel-destroy",
			 G_CALLBACK(virt_viewer_session_spice_channel_destroy), self);

	return VIRT_VIEWER_SESSION(self);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 *  indent-tabs-mode: t
 * End:
 */
