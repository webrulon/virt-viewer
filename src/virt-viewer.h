/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007 Red Hat,
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
#include "virt-viewer-util.h"

G_BEGIN_DECLS

#define VIRT_TYPE_VIEWER_APP virt_viewer_app_get_type()
#define VIRT_VIEWER_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIRT_TYPE_VIEWER_APP, VirtViewerApp))
#define VIRT_VIEWER_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), VIRT_TYPE_VIEWER_APP, VirtViewerAppClass))
#define VIRT_IS_VIEWER_APP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIRT_TYPE_VIEWER_APP))
#define VIRT_IS_VIEWER_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIRT_TYPE_VIEWER_APP))
#define VIRT_VIEWER_APP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VIRT_TYPE_VIEWER_APP, VirtViewerAppClass))

typedef struct _VirtViewerAppPrivate VirtViewerAppPrivate;

typedef struct {
        GObject parent;
        VirtViewerAppPrivate *priv;
} VirtViewerApp;

typedef struct {
        GObjectClass parent_class;
} VirtViewerAppClass;

GType virt_viewer_app_get_type (void);

VirtViewerApp *
virt_viewer_app_new(gint zoom,
                    gboolean direct,
                    gboolean verbose,
                    gboolean fullscreen,
                    GtkWidget *container);

VirtViewerApp *
virt_viewer_start(const char *uri,
		  const char *name,
		  gint zoom,
		  gboolean direct,
		  gboolean waitvm,
		  gboolean reconnect,
		  gboolean verbose,
		  gboolean debug,
		  gboolean fullscreen,
		  GtkWidget *container);

G_END_DECLS

#endif /* VIRT_VIEWER_H */
