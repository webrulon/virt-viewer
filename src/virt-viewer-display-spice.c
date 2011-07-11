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

struct _VirtViewerDisplaySpicePrivate {
	SpiceChannel *channel;
	SpiceDisplay *display;
};

#define VIRT_VIEWER_DISPLAY_SPICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), VIRT_VIEWER_TYPE_DISPLAY_SPICE, VirtViewerDisplaySpicePrivate))

static void virt_viewer_display_spice_send_keys(VirtViewerDisplay *display,
						const guint *keyvals,
						int nkeyvals);
static GdkPixbuf *virt_viewer_display_spice_get_pixbuf(VirtViewerDisplay *display);

static void
virt_viewer_display_spice_finalize(GObject *obj)
{
	VirtViewerDisplaySpice *spice = VIRT_VIEWER_DISPLAY_SPICE(obj);

	g_object_unref(spice->priv->display);
	g_object_unref(spice->priv->channel);

	G_OBJECT_CLASS(virt_viewer_display_spice_parent_class)->finalize(obj);
}

static void
virt_viewer_display_spice_class_init(VirtViewerDisplaySpiceClass *klass)
{
	VirtViewerDisplayClass *dclass = VIRT_VIEWER_DISPLAY_CLASS(klass);
	GObjectClass *oclass = G_OBJECT_CLASS(klass);

	oclass->finalize = virt_viewer_display_spice_finalize;

	dclass->send_keys = virt_viewer_display_spice_send_keys;
	dclass->get_pixbuf = virt_viewer_display_spice_get_pixbuf;

	g_type_class_add_private(oclass, sizeof(VirtViewerDisplaySpicePrivate));
}

static void
virt_viewer_display_spice_init(VirtViewerDisplaySpice *self G_GNUC_UNUSED)
{
	self->priv = VIRT_VIEWER_DISPLAY_SPICE_GET_PRIVATE(self);
}

static void
virt_viewer_display_spice_send_keys(VirtViewerDisplay *display,
				    const guint *keyvals,
				    int nkeyvals)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

	g_return_if_fail(self != NULL);
	g_return_if_fail(self->priv->display != NULL);

	spice_display_send_keys(self->priv->display, keyvals, nkeyvals, SPICE_DISPLAY_KEY_EVENT_CLICK);
}

static GdkPixbuf *
virt_viewer_display_spice_get_pixbuf(VirtViewerDisplay *display)
{
	VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(self->priv->display != NULL, NULL);

	return spice_display_get_pixbuf(self->priv->display);
}

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
       g_signal_emit_by_name(display, "display-desktop-resize");
}


GtkWidget *
virt_viewer_display_spice_new(SpiceChannel *channel,
			      SpiceDisplay *display)
{
	VirtViewerDisplaySpice *self;

	self = g_object_new(VIRT_VIEWER_TYPE_DISPLAY_SPICE, NULL);

	self->priv->channel = channel;
	self->priv->display = display;

	g_object_ref(channel);
	g_object_ref(display);

	g_signal_connect(channel, "display-primary-create",
			 G_CALLBACK(virt_viewer_display_spice_primary_create), self);

	gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(self->priv->display));
	gtk_widget_show(GTK_WIDGET(self->priv->display));
	g_object_set(self->priv->display,
		     "grab-keyboard", TRUE,
		     "grab-mouse", TRUE,
		     "resize-guest", FALSE,
		     "scaling", TRUE,
		     "auto-clipboard", TRUE,
		     NULL);

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
