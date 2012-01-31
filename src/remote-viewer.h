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

#ifndef REMOTE_VIEWER_H
#define REMOTE_VIEWER_H

#include <glib-object.h>
#include "virt-viewer-app.h"

G_BEGIN_DECLS

#define REMOTE_VIEWER_TYPE remote_viewer_get_type()
#define REMOTE_VIEWER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), REMOTE_VIEWER_TYPE, RemoteViewer))
#define REMOTE_VIEWER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), REMOTE_VIEWER_TYPE, RemoteViewerClass))
#define REMOTE_VIEWER_IS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REMOTE_VIEWER_TYPE))
#define REMOTE_VIEWER_IS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), REMOTE_VIEWER_TYPE))
#define REMOTE_VIEWER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), REMOTE_VIEWER_TYPE, RemoteViewerClass))

typedef struct _RemoteViewerPrivate RemoteViewerPrivate;

typedef struct {
    VirtViewerApp parent;
    RemoteViewerPrivate *priv;
} RemoteViewer;

typedef struct {
    VirtViewerAppClass parent_class;
} RemoteViewerClass;

GType remote_viewer_get_type (void);

RemoteViewer* remote_viewer_new(const gchar *uri, gboolean verbose);
RemoteViewer* remote_viewer_new_with_controller(gboolean verbose);

G_END_DECLS

#endif /* REMOTE_VIEWER_H */
/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
