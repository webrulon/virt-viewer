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
 * Author: Marc-André Lureau <marcandre.lureau@redhat.com>
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "virt-viewer-app.h"
#include "remote-viewer.h"

struct _RemoteViewerPrivate {
	int _dummy;
};

G_DEFINE_TYPE (RemoteViewer, remote_viewer, VIRT_VIEWER_TYPE_APP)
#define GET_PRIVATE(o)							\
        (G_TYPE_INSTANCE_GET_PRIVATE ((o), REMOTE_VIEWER_TYPE, RemoteViewerPrivate))

static gboolean remote_viewer_start(VirtViewerApp *self);

static void
remote_viewer_get_property (GObject *object, guint property_id,
			    GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
remote_viewer_set_property (GObject *object, guint property_id,
			    const GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
remote_viewer_dispose (GObject *object)
{
	G_OBJECT_CLASS(remote_viewer_parent_class)->dispose (object);
}

static void
remote_viewer_class_init (RemoteViewerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	VirtViewerAppClass *app_class = VIRT_VIEWER_APP_CLASS (klass);

	g_type_class_add_private (klass, sizeof (RemoteViewerPrivate));

	object_class->get_property = remote_viewer_get_property;
	object_class->set_property = remote_viewer_set_property;
	object_class->dispose = remote_viewer_dispose;

	app_class->start = remote_viewer_start;
}

static void
remote_viewer_init(RemoteViewer *self)
{
	self->priv = GET_PRIVATE(self);
}

RemoteViewer *
remote_viewer_new(const gchar *uri, gboolean verbose)
{
	return g_object_new(REMOTE_VIEWER_TYPE,
			    "guri", uri,
			    "verbose", verbose,
			    NULL);
}

static gboolean
remote_viewer_start(VirtViewerApp *app)
{
	gchar *guri;
	gchar *type;
	gboolean ret = FALSE;

	g_object_get(app, "guri", &guri, NULL);
	g_return_val_if_fail(guri != NULL, FALSE);

	DEBUG_LOG("Opening display to %s", guri);

	if (virt_viewer_util_extract_host(guri, &type, NULL, NULL, NULL, NULL) < 0) {
		virt_viewer_app_simple_message_dialog(app, _("Cannot determine the connection type from URI"));
		goto cleanup;
	}

	if (virt_viewer_app_create_session(app, type) < 0) {
		virt_viewer_app_simple_message_dialog(app, _("Couldn't create a session for this type: %s"), type);
		goto cleanup;
	}

	if (virt_viewer_app_activate(app) < 0) {
		virt_viewer_app_simple_message_dialog(app, _("Failed to initiate connection"));
		goto cleanup;
	}

	ret = VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->start(app);

 cleanup:
	g_free(guri);
	g_free(type);
	return ret;
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 *  indent-tabs-mode: t
 * End:
 */
