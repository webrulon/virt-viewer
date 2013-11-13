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

#include <math.h>
#include <spice-audio.h>

#include <glib/gi18n.h>

#include "virt-viewer-util.h"
#include "virt-viewer-display-spice.h"
#include "virt-viewer-auth.h"

G_DEFINE_TYPE (VirtViewerDisplaySpice, virt_viewer_display_spice, VIRT_VIEWER_TYPE_DISPLAY)

typedef enum {
    AUTO_RESIZE_ALWAYS,
    AUTO_RESIZE_FULLSCREEN,
    AUTO_RESIZE_NEVER,
} AutoResizeState;

struct _VirtViewerDisplaySpicePrivate {
    SpiceChannel *channel; /* weak reference */
    SpiceDisplay *display;
    AutoResizeState auto_resize;
};

#define VIRT_VIEWER_DISPLAY_SPICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), VIRT_VIEWER_TYPE_DISPLAY_SPICE, VirtViewerDisplaySpicePrivate))

static void virt_viewer_display_spice_send_keys(VirtViewerDisplay *display,
                                                const guint *keyvals,
                                                int nkeyvals);
static GdkPixbuf *virt_viewer_display_spice_get_pixbuf(VirtViewerDisplay *display);
static void virt_viewer_display_spice_release_cursor(VirtViewerDisplay *display);
static void virt_viewer_display_spice_close(VirtViewerDisplay *display G_GNUC_UNUSED);
static gboolean virt_viewer_display_spice_selectable(VirtViewerDisplay *display);

static void
virt_viewer_display_spice_finalize(GObject *obj)
{
    VirtViewerDisplaySpice *spice = VIRT_VIEWER_DISPLAY_SPICE(obj);

    g_object_unref(spice->priv->display);

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
    dclass->close = virt_viewer_display_spice_close;
    dclass->selectable = virt_viewer_display_spice_selectable;

    g_type_class_add_private(klass, sizeof(VirtViewerDisplaySpicePrivate));
}

static SpiceMainChannel*
get_main(VirtViewerDisplay *self)
{
    VirtViewerSessionSpice *session;

    session = VIRT_VIEWER_SESSION_SPICE(virt_viewer_display_get_session(self));

    return virt_viewer_session_spice_get_main_channel(session);
}

static void
virt_viewer_display_spice_monitor_geometry_changed(VirtViewerDisplaySpice *self)
{

    if (virt_viewer_display_get_auto_resize(VIRT_VIEWER_DISPLAY(self)) == FALSE)
        return;

    g_signal_emit_by_name(self, "monitor-geometry-changed", NULL);

}

static void
show_hint_changed(VirtViewerDisplay *self)
{
    SpiceMainChannel *main_channel = get_main(self);
    guint enabled = virt_viewer_display_get_enabled(self);
    guint nth;

    /* this may happen when finalizing */
    if (!main_channel)
        return;

    g_object_get(self, "nth-display", &nth, NULL);
    spice_main_set_display_enabled(main_channel, nth, enabled);
}

static void
virt_viewer_display_spice_init(VirtViewerDisplaySpice *self G_GNUC_UNUSED)
{
    self->priv = VIRT_VIEWER_DISPLAY_SPICE_GET_PRIVATE(self);
    self->priv->auto_resize = AUTO_RESIZE_ALWAYS;

    g_signal_connect(self, "notify::show-hint", G_CALLBACK(show_hint_changed), NULL);
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
update_display_ready(VirtViewerDisplaySpice *self)
{
    gboolean ready;

    g_object_get(self->priv->display, "ready", &ready, NULL);

    virt_viewer_display_set_show_hint(VIRT_VIEWER_DISPLAY(self),
                                      VIRT_VIEWER_DISPLAY_SHOW_HINT_READY, ready);
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
                                        GtkAllocation *allocation G_GNUC_UNUSED,
                                        gpointer data G_GNUC_UNUSED)
{
    if (self->priv->auto_resize != AUTO_RESIZE_NEVER)
        virt_viewer_display_spice_monitor_geometry_changed(self);

    if (self->priv->auto_resize == AUTO_RESIZE_FULLSCREEN)
        self->priv->auto_resize = AUTO_RESIZE_NEVER;
}

static void
zoom_level_changed(VirtViewerDisplaySpice *self,
                   GParamSpec *pspec G_GNUC_UNUSED,
                   VirtViewerApp *app G_GNUC_UNUSED)
{
    if (self->priv->auto_resize != AUTO_RESIZE_NEVER)
        return;

    virt_viewer_display_spice_monitor_geometry_changed(self);
}

static void
enable_accel_changed(VirtViewerApp *app,
                     GParamSpec *pspec G_GNUC_UNUSED,
                     VirtViewerDisplaySpice *self)
{
    if (virt_viewer_app_get_enable_accel(app)
            && gtk_accel_map_lookup_entry("<virt-viewer>/view/release-cursor", NULL)) {
        SpiceGrabSequence *seq = spice_grab_sequence_new(0, NULL);
        /* disable default grab sequence */
        spice_display_set_grab_keys(self->priv->display, seq);
        spice_grab_sequence_free(seq);
    } else {
        spice_display_set_grab_keys(self->priv->display, NULL);
    }
}

static void
fullscreen_changed(VirtViewerDisplaySpice *self,
                   GParamSpec *pspec G_GNUC_UNUSED,
                   VirtViewerApp *app)
{
    if (virt_viewer_display_get_fullscreen(VIRT_VIEWER_DISPLAY(self))) {
        gboolean auto_conf;
        g_object_get(app, "fullscreen", &auto_conf, NULL);
        if (auto_conf)
            self->priv->auto_resize = AUTO_RESIZE_NEVER;
        else
            self->priv->auto_resize = AUTO_RESIZE_FULLSCREEN;
    } else
        self->priv->auto_resize = AUTO_RESIZE_ALWAYS;
}

GtkWidget *
virt_viewer_display_spice_new(VirtViewerSessionSpice *session,
                              SpiceChannel *channel,
                              gint monitorid)
{
    VirtViewerDisplaySpice *self;
    VirtViewerApp *app;
    gint channelid;
    SpiceSession *s;

    g_return_val_if_fail(SPICE_IS_DISPLAY_CHANNEL(channel), NULL);

    g_object_get(channel, "channel-id", &channelid, NULL);
    // We don't allow monitorid != 0 && channelid != 0
    g_return_val_if_fail(channelid == 0 || monitorid == 0, NULL);

    self = g_object_new(VIRT_VIEWER_TYPE_DISPLAY_SPICE,
                        "session", session,
                        // either monitorid is always 0 or channelid
                        // is, we can't have display (0, 2) and (2, 0)
                        // for example
                        "nth-display", channelid + monitorid,
                        NULL);
    self->priv->channel = channel;

    g_object_get(session, "spice-session", &s, NULL);
    self->priv->display = spice_display_new_with_monitor(s, channelid, monitorid);
    g_object_unref(s);

    virt_viewer_signal_connect_object(self->priv->display, "notify::ready",
                                      G_CALLBACK(update_display_ready), self,
                                      G_CONNECT_SWAPPED);
    update_display_ready(self);

    gtk_container_add(GTK_CONTAINER(self), g_object_ref(self->priv->display));
    gtk_widget_show(GTK_WIDGET(self->priv->display));
    g_object_set(self->priv->display,
                 "grab-keyboard", TRUE,
                 "grab-mouse", TRUE,
                 "resize-guest", FALSE,
                 "scaling", TRUE,
                 NULL);

    virt_viewer_signal_connect_object(self->priv->display, "keyboard-grab",
                                      G_CALLBACK(virt_viewer_display_spice_keyboard_grab), self, 0);
    virt_viewer_signal_connect_object(self->priv->display, "mouse-grab",
                                      G_CALLBACK(virt_viewer_display_spice_mouse_grab), self, 0);
    virt_viewer_signal_connect_object(self, "size-allocate",
                                      G_CALLBACK(virt_viewer_display_spice_size_allocate), self, 0);


    app = virt_viewer_session_get_app(VIRT_VIEWER_SESSION(session));
    virt_viewer_signal_connect_object(app, "notify::enable-accel",
                                      G_CALLBACK(enable_accel_changed), self, 0);
    virt_viewer_signal_connect_object(self, "notify::fullscreen",
                                      G_CALLBACK(fullscreen_changed), app, 0);
    virt_viewer_signal_connect_object(self, "notify::zoom-level",
                                      G_CALLBACK(zoom_level_changed), app, 0);
    fullscreen_changed(self, NULL, app);
    enable_accel_changed(app, NULL, self);

    return GTK_WIDGET(self);
}

static void
virt_viewer_display_spice_release_cursor(VirtViewerDisplay *display)
{
    VirtViewerDisplaySpice *self = VIRT_VIEWER_DISPLAY_SPICE(display);

    spice_display_mouse_ungrab(self->priv->display);
}


static void
virt_viewer_display_spice_close(VirtViewerDisplay *display G_GNUC_UNUSED)
{
}

static gboolean
virt_viewer_display_spice_selectable(VirtViewerDisplay *self)
{
    gboolean agent_connected;
    SpiceMainChannel *mainc;

    mainc = get_main(self);
    g_object_get(mainc,
                 "agent-connected", &agent_connected,
                 NULL);

    return agent_connected;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
