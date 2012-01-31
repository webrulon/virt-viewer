/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2012 Red Hat, Inc.
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

#ifndef VIRT_VIEWER_H
#define VIRT_VIEWER_H

#include <glib-object.h>
#include "virt-viewer-app.h"

G_BEGIN_DECLS

#define VIRT_VIEWER_TYPE virt_viewer_get_type()
#define VIRT_VIEWER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIRT_VIEWER_TYPE, VirtViewer))
#define VIRT_VIEWER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), VIRT_VIEWER_TYPE, VirtViewerClass))
#define VIRT_VIEWER_IS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIRT_VIEWER_TYPE))
#define VIRT_VIEWER_IS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIRT_VIEWER_TYPE))
#define VIRT_VIEWER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VIRT_VIEWER_TYPE, VirtViewerClass))

typedef struct _VirtViewerPrivate VirtViewerPrivate;

typedef struct {
    VirtViewerApp parent;
    VirtViewerPrivate *priv;
} VirtViewer;

typedef struct {
    VirtViewerAppClass parent_class;
} VirtViewerClass;

GType virt_viewer_get_type (void);

VirtViewer *
virt_viewer_new(const char *uri,
                const char *name,
                gint zoom,
                gboolean direct,
                gboolean attach,
                gboolean waitvm,
                gboolean reconnect,
                gboolean verbose,
                GtkWidget *container);

G_END_DECLS

#endif /* VIRT_VIEWER_H */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
