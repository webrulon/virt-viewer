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

#include "virt-viewer-notebook.h"
#include "virt-viewer-util.h"

G_DEFINE_TYPE (VirtViewerNotebook, virt_viewer_notebook, GTK_TYPE_NOTEBOOK)

#define GET_PRIVATE(o)                                                        \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIRT_VIEWER_TYPE_NOTEBOOK, VirtViewerNotebookPrivate))

struct _VirtViewerNotebookPrivate {
    GtkWidget *status;
};

static void
virt_viewer_notebook_get_property (GObject *object, guint property_id,
                                   GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
virt_viewer_notebook_set_property (GObject *object, guint property_id,
                                   const GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
virt_viewer_notebook_dispose (GObject *object)
{
    G_OBJECT_CLASS (virt_viewer_notebook_parent_class)->dispose (object);
}

static void
virt_viewer_notebook_class_init (VirtViewerNotebookClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (VirtViewerNotebookPrivate));

    object_class->get_property = virt_viewer_notebook_get_property;
    object_class->set_property = virt_viewer_notebook_set_property;
    object_class->dispose = virt_viewer_notebook_dispose;
}

static void
virt_viewer_notebook_init (VirtViewerNotebook *self)
{
    VirtViewerNotebookPrivate *priv;
    GdkColor color;

    self->priv = GET_PRIVATE(self);
    priv = self->priv;

    priv->status = gtk_label_new("");
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(self), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(self), FALSE);
    gtk_widget_show_all(priv->status);
    gtk_notebook_append_page(GTK_NOTEBOOK(self), priv->status, NULL);
    gdk_color_parse("white", &color);
    gtk_widget_modify_fg(priv->status, GTK_STATE_NORMAL, &color);
}

void
virt_viewer_notebook_show_status_va(VirtViewerNotebook *self, const gchar *fmt, va_list args)
{
    VirtViewerNotebookPrivate *priv;
    gchar *text;

    DEBUG_LOG("notebook show status %p", self);
    g_return_if_fail(VIRT_VIEWER_IS_NOTEBOOK(self));

    text = g_strdup_vprintf(fmt, args);
    priv = self->priv;
    gtk_label_set_text(GTK_LABEL(priv->status), text);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(self), 0);
    gtk_widget_show_all(GTK_WIDGET(self));
    g_free(text);
}

void
virt_viewer_notebook_show_status(VirtViewerNotebook *self, const gchar *fmt, ...)
{
    va_list args;

    g_return_if_fail(VIRT_VIEWER_IS_NOTEBOOK(self));

    va_start(args, fmt);
    virt_viewer_notebook_show_status_va(self, fmt, args);
    va_end(args);
}

void
virt_viewer_notebook_show_display(VirtViewerNotebook *self)
{
    GtkWidget *display;

    DEBUG_LOG("notebook show display %p", self);
    g_return_if_fail(VIRT_VIEWER_IS_NOTEBOOK(self));

    display = gtk_notebook_get_nth_page(GTK_NOTEBOOK(self), 1);
    if (display == NULL)
        DEBUG_LOG("FIXME: showing display although it's not ready yet");
    else
        gtk_widget_grab_focus(display);

    gtk_notebook_set_current_page(GTK_NOTEBOOK(self), 1);
    gtk_widget_show_all(GTK_WIDGET(self));
}

VirtViewerNotebook*
virt_viewer_notebook_new (void)
{
    return g_object_new (VIRT_VIEWER_TYPE_NOTEBOOK, NULL);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
