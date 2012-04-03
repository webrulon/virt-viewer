/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2012 Red Hat, Inc.
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

#include "virt-viewer-auth.h"
#include "virt-viewer-display-vnc.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE(VirtViewerDisplayVnc, virt_viewer_display_vnc, VIRT_VIEWER_TYPE_DISPLAY)

struct _VirtViewerDisplayVncPrivate {
    VncDisplay *vnc;
};

#define VIRT_VIEWER_DISPLAY_VNC_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), VIRT_VIEWER_TYPE_DISPLAY_VNC, VirtViewerDisplayVncPrivate))

static void virt_viewer_display_vnc_send_keys(VirtViewerDisplay* display, const guint *keyvals, int nkeyvals);
static GdkPixbuf *virt_viewer_display_vnc_get_pixbuf(VirtViewerDisplay* display);
static void virt_viewer_display_vnc_close(VirtViewerDisplay *display);

static void
virt_viewer_display_vnc_finalize(GObject *obj)
{
    VirtViewerDisplayVnc *vnc = VIRT_VIEWER_DISPLAY_VNC(obj);

    g_object_unref(vnc->priv->vnc);

    G_OBJECT_CLASS(virt_viewer_display_vnc_parent_class)->finalize(obj);
}


static void
virt_viewer_display_vnc_class_init(VirtViewerDisplayVncClass *klass)
{
    VirtViewerDisplayClass *dclass = VIRT_VIEWER_DISPLAY_CLASS(klass);
    GObjectClass *oclass = G_OBJECT_CLASS(klass);

    oclass->finalize = virt_viewer_display_vnc_finalize;

    dclass->send_keys = virt_viewer_display_vnc_send_keys;
    dclass->get_pixbuf = virt_viewer_display_vnc_get_pixbuf;
    dclass->close = virt_viewer_display_vnc_close;

    g_type_class_add_private(klass, sizeof(VirtViewerDisplayVncPrivate));
}

static void
virt_viewer_display_vnc_init(VirtViewerDisplayVnc *self G_GNUC_UNUSED)
{
    self->priv = VIRT_VIEWER_DISPLAY_VNC_GET_PRIVATE(self);
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
    g_return_if_fail(self->priv->vnc != NULL);

    vnc_display_send_keys(self->priv->vnc, keyvals, nkeyvals);
}


static GdkPixbuf *
virt_viewer_display_vnc_get_pixbuf(VirtViewerDisplay* display)
{
    VirtViewerDisplayVnc *self = VIRT_VIEWER_DISPLAY_VNC(display);

    g_return_val_if_fail(self != NULL, NULL);
    g_return_val_if_fail(self->priv->vnc != NULL, NULL);

    return vnc_display_get_pixbuf(self->priv->vnc);
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
}


GtkWidget *
virt_viewer_display_vnc_new(VncDisplay *vnc)
{
    VirtViewerDisplayVnc *display;

    display = g_object_new(VIRT_VIEWER_TYPE_DISPLAY_VNC, NULL);

    g_object_ref(vnc);
    display->priv->vnc = vnc;

    gtk_container_add(GTK_CONTAINER(display), GTK_WIDGET(display->priv->vnc));
    vnc_display_set_keyboard_grab(display->priv->vnc, TRUE);
    vnc_display_set_pointer_grab(display->priv->vnc, TRUE);

    /*
     * In auto-resize mode we have things setup so that we always
     * automatically resize the top level window to be exactly the
     * same size as the VNC desktop, except when it won't fit on
     * the local screen, at which point we let it scale down.
     * The upshot is, we always want scaling enabled.
     * We disable force_size because we want to allow user to
     * manually size the widget smaller too
     */
    vnc_display_set_force_size(display->priv->vnc, FALSE);
    vnc_display_set_scaling(display->priv->vnc, TRUE);

    /* When VNC desktop resizes, we have to resize the containing widget */
    g_signal_connect(display->priv->vnc, "vnc-desktop-resize",
                     G_CALLBACK(virt_viewer_display_vnc_resize_desktop), display);

    g_signal_connect(display->priv->vnc, "vnc-pointer-grab",
                     G_CALLBACK(virt_viewer_display_vnc_mouse_grab), display);
    g_signal_connect(display->priv->vnc, "vnc-pointer-ungrab",
                     G_CALLBACK(virt_viewer_display_vnc_mouse_ungrab), display);
    g_signal_connect(display->priv->vnc, "vnc-keyboard-grab",
                     G_CALLBACK(virt_viewer_display_vnc_key_grab), display);
    g_signal_connect(display->priv->vnc, "vnc-keyboard-ungrab",
                     G_CALLBACK(virt_viewer_display_vnc_key_ungrab), display);

    return GTK_WIDGET(display);
}


static void
virt_viewer_display_vnc_close(VirtViewerDisplay *display)
{
    VirtViewerDisplayVnc *vnc = VIRT_VIEWER_DISPLAY_VNC(display);

    /* We're not the real owner, so we shouldn't be letting the container
     * destroy the widget. There are still signals that need to be
     * propagated to the VirtViewerSession
     */
    gtk_container_remove(GTK_CONTAINER(display), GTK_WIDGET(vnc->priv->vnc));
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
