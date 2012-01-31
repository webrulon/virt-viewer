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
#ifndef _VIRT_VIEWER_NOTEBOOK
#define _VIRT_VIEWER_NOTEBOOK

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VIRT_VIEWER_TYPE_NOTEBOOK virt_viewer_notebook_get_type()

#define VIRT_VIEWER_NOTEBOOK(obj)                                        \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIRT_VIEWER_TYPE_NOTEBOOK, VirtViewerNotebook))

#define VIRT_VIEWER_NOTEBOOK_CLASS(klass)                                \
    (G_TYPE_CHECK_CLASS_CAST ((klass), VIRT_VIEWER_TYPE_NOTEBOOK, VirtViewerNotebookClass))

#define VIRT_VIEWER_IS_NOTEBOOK(obj)                                        \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIRT_VIEWER_TYPE_NOTEBOOK))

#define VIRT_VIEWER_IS_NOTEBOOK_CLASS(klass)                                \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), VIRT_VIEWER_TYPE_NOTEBOOK))

#define VIRT_VIEWER_NOTEBOOK_GET_CLASS(obj)                                \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), VIRT_VIEWER_TYPE_NOTEBOOK, VirtViewerNotebookClass))

typedef struct _VirtViewerNotebookPrivate VirtViewerNotebookPrivate;

typedef struct {
    GtkNotebook parent;
    VirtViewerNotebookPrivate *priv;
} VirtViewerNotebook;

typedef struct {
    GtkNotebookClass parent_class;
} VirtViewerNotebookClass;

GType virt_viewer_notebook_get_type (void);

VirtViewerNotebook* virt_viewer_notebook_new (void);
void virt_viewer_notebook_show_status_va(VirtViewerNotebook *self, const gchar *fmt, va_list args);
void virt_viewer_notebook_show_status(VirtViewerNotebook *nb, const gchar *fmt, ...);
void virt_viewer_notebook_show_display(VirtViewerNotebook *nb);

G_END_DECLS

#endif /* _VIRT_VIEWER_NOTEBOOK */
/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
