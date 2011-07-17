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

#ifndef VIRT_VIEWER_APP_H
#define VIRT_VIEWER_APP_H

#include <glib-object.h>
#include "virt-viewer-util.h"
#include "virt-viewer-window.h"

G_BEGIN_DECLS

#define VIRT_VIEWER_TYPE_APP virt_viewer_app_get_type()
#define VIRT_VIEWER_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIRT_VIEWER_TYPE_APP, VirtViewerApp))
#define VIRT_VIEWER_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), VIRT_VIEWER_TYPE_APP, VirtViewerAppClass))
#define VIRT_VIEWER_IS_APP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIRT_VIEWER_TYPE_APP))
#define VIRT_VIEWER_IS_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIRT_VIEWER_TYPE_APP))
#define VIRT_VIEWER_APP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VIRT_VIEWER_TYPE_APP, VirtViewerAppClass))

typedef struct _VirtViewerAppPrivate VirtViewerAppPrivate;

typedef struct {
        GObject parent;
        VirtViewerAppPrivate *priv;
} VirtViewerApp;

typedef struct {
        GObjectClass parent_class;

        gboolean (*start) (VirtViewerApp *self);
        int (*initial_connect) (VirtViewerApp *self);
        void (*deactivated) (VirtViewerApp *self);
} VirtViewerAppClass;

GType virt_viewer_app_get_type (void);

void virt_viewer_app_set_debug(gboolean debug);
gboolean virt_viewer_app_start(VirtViewerApp *app);
void virt_viewer_app_quit(VirtViewerApp *self);
VirtViewerWindow* virt_viewer_app_get_main_window(VirtViewerApp *self);
void virt_viewer_app_trace(VirtViewerApp *self, const char *fmt, ...);
void virt_viewer_app_simple_message_dialog(VirtViewerApp *self, const char *fmt, ...);
gboolean virt_viewer_app_is_active(VirtViewerApp *app);
void virt_viewer_app_free_connect_info(VirtViewerApp *self);
int virt_viewer_app_create_session(VirtViewerApp *self, const gchar *type);
int virt_viewer_app_activate(VirtViewerApp *self);
int virt_viewer_app_initial_connect(VirtViewerApp *self);
void virt_viewer_app_start_reconnect_poll(VirtViewerApp *self);
void virt_viewer_app_set_zoom_level(VirtViewerApp *self, gint zoom_level);
void virt_viewer_app_set_direct(VirtViewerApp *self, gboolean direct);
gboolean virt_viewer_app_has_session(VirtViewerApp *self);
void virt_viewer_app_set_connect_info(VirtViewerApp *self,
                                      const gchar *host,
                                      const gchar *ghost,
                                      const gchar *gport,
                                      const gchar *transport,
                                      const gchar *unixsock,
                                      const gchar *user,
                                      gint port);
gboolean virt_viewer_app_window_set_visible(VirtViewerApp *self, VirtViewerWindow *window, gboolean visible);
void virt_viewer_app_show_status(VirtViewerApp *self, const gchar *fmt, ...);

G_END_DECLS

#endif /* VIRT_VIEWER_APP_H */
