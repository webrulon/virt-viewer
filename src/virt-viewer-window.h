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
#ifndef _VIRT_VIEWER_WINDOW
#define _VIRT_VIEWER_WINDOW

#include <glib-object.h>
#include "virt-viewer-notebook.h"
#include "virt-viewer-display.h"

G_BEGIN_DECLS

#define VIRT_VIEWER_TYPE_WINDOW virt_viewer_window_get_type()

#define VIRT_VIEWER_WINDOW(obj)                                                \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIRT_VIEWER_TYPE_WINDOW, VirtViewerWindow))

#define VIRT_VIEWER_WINDOW_CLASS(klass)                                        \
    (G_TYPE_CHECK_CLASS_CAST ((klass), VIRT_VIEWER_TYPE_WINDOW, VirtViewerWindowClass))

#define VIRT_VIEWER_IS_WINDOW(obj)                                        \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIRT_VIEWER_TYPE_WINDOW))

#define VIRT_VIEWER_IS_WINDOW_CLASS(klass)                                \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), VIRT_VIEWER_TYPE_WINDOW))

#define VIRT_VIEWER_WINDOW_GET_CLASS(obj)                                \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), VIRT_VIEWER_TYPE_WINDOW, VirtViewerWindowClass))

typedef struct _VirtViewerWindowPrivate VirtViewerWindowPrivate;

typedef struct {
    GObject parent;
    VirtViewerWindowPrivate *priv;
} VirtViewerWindow;

typedef struct {
    GObjectClass parent_class;
} VirtViewerWindowClass;

GType virt_viewer_window_get_type (void);

GtkWindow* virt_viewer_window_get_window (VirtViewerWindow* window);
VirtViewerNotebook* virt_viewer_window_get_notebook (VirtViewerWindow* window);
void virt_viewer_window_set_display(VirtViewerWindow *self, VirtViewerDisplay *display);
void virt_viewer_window_set_usb_options_sensitive(VirtViewerWindow *self, gboolean sensitive);
void virt_viewer_window_update_title(VirtViewerWindow *self);
void virt_viewer_window_show(VirtViewerWindow *self);
void virt_viewer_window_hide(VirtViewerWindow *self);
void virt_viewer_window_set_zoom_level(VirtViewerWindow *self, gint zoom_level);
gint virt_viewer_window_get_zoom_level(VirtViewerWindow *self);
void virt_viewer_window_leave_fullscreen(VirtViewerWindow *self);
void virt_viewer_window_enter_fullscreen(VirtViewerWindow *self, gboolean move, gint x, gint y);
GtkMenuItem *virt_viewer_window_get_menu_displays(VirtViewerWindow *self);
GtkBuilder* virt_viewer_window_get_builder(VirtViewerWindow *window);

G_END_DECLS

#endif /* _VIRT_VIEWER_WINDOW */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
