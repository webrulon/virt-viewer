/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2009 Red Hat, Inc.
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

#include <config.h>

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
static void virt_viewer_display_spice_release_cursor(VirtViewerDisplay *display);

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
    dclass->release_cursor = virt_viewer_display_spice_release_cursor;

    g_type_class_add_private(klass, sizeof(VirtViewerDisplaySpicePrivate));
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
display_mark(SpiceChannel *channel G_GNUC_UNUSED,
             gint mark,
             VirtViewerDisplay *display)
{
    DEBUG_LOG("display mark %d", mark);

    virt_viewer_display_set_show_hint(display, mark);
}

static void
primary_create(SpiceChannel *channel G_GNUC_UNUSED,
               gint format G_GNUC_UNUSED,
               gint width,
               gint height,
               gint stride G_GNUC_UNUSED,
               gint shmid G_GNUC_UNUSED,
               gpointer imgdata G_GNUC_UNUSED,
               VirtViewerDisplay *display)
{
    DEBUG_LOG("spice desktop resize %dx%d", width, height);

    virt_viewer_display_set_desktop_size(display, width, height);
}


static void
virt_viewer_display_spice_keyboard_grab(SpiceDisplay *display G_GNUC_UNUSED,
                                        int grabbed,
                                        VirtViewerDisplaySpice *self)
{
    if (grabbed)
        g_signal_emit_by_name(self, "display-keyboard-grab");
    else
        g_signal_emit_by_name(self, "display-keyboard-ungrab");
}


static void
virt_viewer_display_spice_mouse_grab(SpiceDisplay *display G_GNUC_UNUSED,
                                     int grabbed,
                                     VirtViewerDisplaySpice *self)
{
    if (grabbed)
        g_signal_emit_by_name(self, "display-pointer-grab");
    else
        g_signal_emit_by_name(self, "display-pointer-ungrab");
}


static void
virt_viewer_display_spice_size_allocate(VirtViewerDisplaySpice *self,
                                        GtkAllocation *allocation,
                                        gpointer data G_GNUC_UNUSED)
{
    gdouble dw = allocation->width, dh = allocation->height;
    guint zoom = 100;
    guint channelid;

    if (virt_viewer_display_get_auto_resize(VIRT_VIEWER_DISPLAY(self)) == FALSE)
        return;

    if (virt_viewer_display_get_zoom(VIRT_VIEWER_DISPLAY(self))) {
        zoom = virt_viewer_display_get_zoom_level(VIRT_VIEWER_DISPLAY(self));

        dw /= ((double)zoom / 100.0);
        dh /= ((double)zoom / 100.0);
    }

    g_object_get(self->priv->channel, "channel-id", &channelid, NULL);

    SpiceMainChannel *main_channel = virt_viewer_session_spice_get_main_channel(
        VIRT_VIEWER_SESSION_SPICE(virt_viewer_display_get_session(VIRT_VIEWER_DISPLAY(self))));
    spice_main_set_display(main_channel,
                           channelid,
                           0, 0, dw, dh);
}


GtkWidget *
virt_viewer_display_spice_new(VirtViewerSessionSpice *session,
                              SpiceChannel *channel,
                              SpiceDisplay *display)
{
    VirtViewerDisplaySpice *self;
    gint channelid;

    g_return_val_if_fail(SPICE_IS_DISPLAY_CHANNEL(channel), NULL);
    g_return_val_if_fail(SPICE_IS_DISPLAY(display), NULL);

    g_object_get(channel, "channel-id", &channelid, NULL);

    self = g_object_new(VIRT_VIEWER_TYPE_DISPLAY_SPICE,
                        "session", session,
                        "nth-display", channelid,
                        NULL);
    self->priv->channel = g_object_ref(channel);
    self->priv->display = g_object_ref(display);

    g_signal_connect(channel, "display-primary-create",
                     G_CALLBACK(primary_create), self);
    g_signal_connect(channel, "display-mark",
                     G_CALLBACK(display_mark), self);

    gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(self->priv->display));
    gtk_widget_show(GTK_WIDGET(self->priv->display));
    g_object_set(self->priv->display,
                 "grab-keyboard", TRUE,
                 "grab-mouse", TRUE,
                 "resize-guest", FALSE,
                 "scaling", TRUE,
                 NULL);

    g_signal_connect(self->priv->display,
                     "keyboard-grab",
                     G_CALLBACK(virt_viewer_display_spice_keyboard_grab), self);
    g_signal_connect(self->priv->display,
                     "mouse-grab",
                     G_CALLBACK(virt_viewer_display_spice_mouse_grab), self);
    g_signal_connect(self,
                     "size-allocate",
                     G_CALLBACK(virt_viewer_display_spice_size_allocate), self);


    return GTK_WIDGET(self);
}

static void
virt_viewer_display_spice_release_cursor(VirtViewerDisplay *display)
{
    VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

    spice_display_mouse_ungrab(self->priv->display);
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
