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
#ifndef _VIRT_VIEWER_SESSION_H
#define _VIRT_VIEWER_SESSION_H

#include <gtk/gtk.h>

#include "virt-viewer-app.h"
#include "virt-viewer-display.h"

G_BEGIN_DECLS

#define VIRT_VIEWER_TYPE_SESSION virt_viewer_session_get_type()

#define VIRT_VIEWER_SESSION(obj)                                        \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIRT_VIEWER_TYPE_SESSION, VirtViewerSession))

#define VIRT_VIEWER_SESSION_CLASS(klass)                                \
    (G_TYPE_CHECK_CLASS_CAST ((klass), VIRT_VIEWER_TYPE_SESSION, VirtViewerSessionClass))

#define VIRT_VIEWER_IS_SESSION(obj)                                     \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIRT_VIEWER_TYPE_SESSION))

#define VIRT_VIEWER_IS_SESSION_CLASS(klass)                             \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), VIRT_VIEWER_TYPE_SESSION))

#define VIRT_VIEWER_SESSION_GET_CLASS(obj)                                \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), VIRT_VIEWER_TYPE_SESSION, VirtViewerSessionClass))

typedef struct _VirtViewerSessionPrivate VirtViewerSessionPrivate;

typedef struct _VirtViewerSessionChannel VirtViewerSessionChannel;


/* perhaps this become an interface, and be pushed in gtkvnc and spice? */
struct _VirtViewerSession {
    GObject parent;

    VirtViewerSessionPrivate *priv;
};

struct _VirtViewerSessionClass {
    GObjectClass parent_class;

    /* virtual methods */
    void (* close) (VirtViewerSession* session);
    gboolean (* open_fd) (VirtViewerSession* session, int fd);
    gboolean (* open_host) (VirtViewerSession* session, const gchar *host, const gchar *port, const gchar *tlsport);
    gboolean (* open_uri) (VirtViewerSession* session, const gchar *uri);
    gboolean (* channel_open_fd) (VirtViewerSession* session, VirtViewerSessionChannel *channel, int fd);
    gboolean (* has_usb) (VirtViewerSession* session);
    void (* usb_device_selection) (VirtViewerSession* session, GtkWindow *parent);
    void (* smartcard_insert) (VirtViewerSession* session);
    void (* smartcard_remove) (VirtViewerSession* session);

    /* signals */
    void (*session_connected)(VirtViewerSession *session);
    void (*session_initialized)(VirtViewerSession *session);
    void (*session_disconnected)(VirtViewerSession *session);
    void (*session_auth_refused)(VirtViewerSession *session, const gchar *msg);
    void (*session_auth_failed)(VirtViewerSession *session, const gchar *msg);
    void (*session_usb_failed)(VirtViewerSession *session, const gchar *msg);

    void (*session_channel_open)(VirtViewerSession *session, VirtViewerSessionChannel *channel);

    void (*session_display_added)(VirtViewerSession *session,
                                  VirtViewerDisplay *display);
    void (*session_display_removed)(VirtViewerSession *session,
                                    VirtViewerDisplay *display);

    void (*session_cut_text)(VirtViewerSession *session, const gchar *str);
    void (*session_bell)(VirtViewerSession *session);
    void (*session_cancelled)(VirtViewerSession *session);
};

GType virt_viewer_session_get_type(void);

GtkWidget *virt_viewer_session_new(void);

void virt_viewer_session_add_display(VirtViewerSession *session,
                                     VirtViewerDisplay *display);
void virt_viewer_session_remove_display(VirtViewerSession *session,
                                        VirtViewerDisplay *display);
void virt_viewer_session_clear_displays(VirtViewerSession *session);

void virt_viewer_session_close(VirtViewerSession* session);
gboolean virt_viewer_session_open_fd(VirtViewerSession* session, int fd);
gboolean virt_viewer_session_open_host(VirtViewerSession* session, const gchar *host, const gchar *port, const gchar *tlsport);
GObject* virt_viewer_session_get(VirtViewerSession* session);
gboolean virt_viewer_session_channel_open_fd(VirtViewerSession* session,
                                             VirtViewerSessionChannel* channel, int fd);
gboolean virt_viewer_session_open_uri(VirtViewerSession *session, const gchar *uri);

void virt_viewer_session_set_auto_usbredir(VirtViewerSession* session, gboolean auto_usbredir);
gboolean virt_viewer_session_get_auto_usbredir(VirtViewerSession* session);

gboolean virt_viewer_session_has_usb(VirtViewerSession *self);
void virt_viewer_session_usb_device_selection(VirtViewerSession *self, GtkWindow *parent);
void virt_viewer_session_smartcard_insert(VirtViewerSession *self);
void virt_viewer_session_smartcard_remove(VirtViewerSession *self);
VirtViewerApp* virt_viewer_session_get_app(VirtViewerSession *self);
gchar* virt_viewer_session_get_uri(VirtViewerSession *self);

G_END_DECLS

#endif /* _VIRT_VIEWER_SESSION_H */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
