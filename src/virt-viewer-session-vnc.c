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
#include "virt-viewer-session-vnc.h"
#include "virt-viewer-display-vnc.h"

#include <glib/gi18n.h>
#include <libxml/uri.h>

G_DEFINE_TYPE(VirtViewerSessionVnc, virt_viewer_session_vnc, VIRT_VIEWER_TYPE_SESSION)

struct _VirtViewerSessionVncPrivate {
    GtkWindow *main_window;
    /* XXX we should really just have a VncConnection */
    VncDisplay *vnc;
};

#define VIRT_VIEWER_SESSION_VNC_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), VIRT_VIEWER_TYPE_SESSION_VNC, VirtViewerSessionVncPrivate))

static void virt_viewer_session_vnc_close(VirtViewerSession* session);
static gboolean virt_viewer_session_vnc_open_fd(VirtViewerSession* session, int fd);
static gboolean virt_viewer_session_vnc_open_host(VirtViewerSession* session, const gchar *host, const gchar *port, const gchar *tlsport);
static gboolean virt_viewer_session_vnc_open_uri(VirtViewerSession* session, const gchar *uri);
static gboolean virt_viewer_session_vnc_channel_open_fd(VirtViewerSession* session,
                                                        VirtViewerSessionChannel* channel, int fd);


static void
virt_viewer_session_vnc_finalize(GObject *obj)
{
    VirtViewerSessionVnc *vnc = VIRT_VIEWER_SESSION_VNC(obj);

    if (vnc->priv->vnc) {
        vnc_display_close(vnc->priv->vnc);
        g_object_unref(vnc->priv->vnc);
    }
    if (vnc->priv->main_window)
        g_object_unref(vnc->priv->main_window);

    G_OBJECT_CLASS(virt_viewer_session_vnc_parent_class)->finalize(obj);
}


static void
virt_viewer_session_vnc_class_init(VirtViewerSessionVncClass *klass)
{
    VirtViewerSessionClass *dclass = VIRT_VIEWER_SESSION_CLASS(klass);
    GObjectClass *oclass = G_OBJECT_CLASS(klass);

    oclass->finalize = virt_viewer_session_vnc_finalize;

    dclass->close = virt_viewer_session_vnc_close;
    dclass->open_fd = virt_viewer_session_vnc_open_fd;
    dclass->open_host = virt_viewer_session_vnc_open_host;
    dclass->open_uri = virt_viewer_session_vnc_open_uri;
    dclass->channel_open_fd = virt_viewer_session_vnc_channel_open_fd;

    g_type_class_add_private(klass, sizeof(VirtViewerSessionVncPrivate));
}

static void
virt_viewer_session_vnc_init(VirtViewerSessionVnc *self G_GNUC_UNUSED)
{
    self->priv = VIRT_VIEWER_SESSION_VNC_GET_PRIVATE(self);
}

static void
virt_viewer_session_vnc_connected(VncDisplay *vnc G_GNUC_UNUSED,
                                  VirtViewerSessionVnc *session)
{
    GtkWidget *display = virt_viewer_display_vnc_new(session->priv->vnc);
    g_signal_emit_by_name(session, "session-connected");
    virt_viewer_display_set_show_hint(VIRT_VIEWER_DISPLAY(display),
                                      VIRT_VIEWER_DISPLAY_SHOW_HINT_READY);
    virt_viewer_session_add_display(VIRT_VIEWER_SESSION(session),
                                    VIRT_VIEWER_DISPLAY(display));
}

static void
virt_viewer_session_vnc_disconnected(VncDisplay *vnc G_GNUC_UNUSED,
                                     VirtViewerSessionVnc *session)
{
    DEBUG_LOG("Disconnected");
    g_signal_emit_by_name(session, "session-disconnected");
    /* TODO perhaps? */
    /* virt_viewer_display_set_show_hint(VIRT_VIEWER_DISPLAY(session->priv->vnc), */
    /*                                   VIRT_VIEWER_DISPLAY_SHOW_HINT_HIDE); */
}

static void
virt_viewer_session_vnc_initialized(VncDisplay *vnc G_GNUC_UNUSED,
                                    VirtViewerSessionVnc *session)
{
    g_signal_emit_by_name(session, "session-initialized");
}

static void
virt_viewer_session_vnc_cut_text(VncDisplay *vnc G_GNUC_UNUSED,
                                 const gchar *text,
                                 VirtViewerSession *session)
{
    g_signal_emit_by_name(session, "session-cut-text", text);
}

static void
virt_viewer_session_vnc_bell(VncDisplay *vnc G_GNUC_UNUSED,
                             VirtViewerSession *session)
{
    g_signal_emit_by_name(session, "session-bell");
}

static void
virt_viewer_session_vnc_auth_unsupported(VncDisplay *vnc G_GNUC_UNUSED,
                                         unsigned int authType,
                                         VirtViewerSession *session)
{
    gchar *msg = g_strdup_printf(_("Unsupported authentication type %d"),
                                 authType);
    g_signal_emit_by_name(session, "session-auth-failed", msg);
    g_free(msg);
}

static void
virt_viewer_session_vnc_auth_failure(VncDisplay *vnc G_GNUC_UNUSED,
                                     const gchar *reason,
                                     VirtViewerSession *session)
{

    g_signal_emit_by_name(session, "session-auth-refused", reason);
}



static gboolean
virt_viewer_session_vnc_open_fd(VirtViewerSession* session,
                                int fd)
{
    VirtViewerSessionVnc *self = VIRT_VIEWER_SESSION_VNC(session);

    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(self->priv->vnc != NULL, FALSE);

    return vnc_display_open_fd(self->priv->vnc, fd);
}

static gboolean
virt_viewer_session_vnc_channel_open_fd(VirtViewerSession* session G_GNUC_UNUSED,
                                        VirtViewerSessionChannel* channel G_GNUC_UNUSED,
                                        int fd G_GNUC_UNUSED)
{
    g_warning("channel_open_fd is not supported by VNC");
    return FALSE;
}

static gboolean
virt_viewer_session_vnc_open_host(VirtViewerSession* session,
                                  const gchar *host,
                                  const gchar *port,
                                  const gchar *tlsport G_GNUC_UNUSED)
{
    VirtViewerSessionVnc *self = VIRT_VIEWER_SESSION_VNC(session);

    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(self->priv->vnc != NULL, FALSE);

    return vnc_display_open_host(self->priv->vnc, host, port);
}

static gboolean
virt_viewer_session_vnc_open_uri(VirtViewerSession* session,
                                 const gchar *uristr)
{
    VirtViewerSessionVnc *self = VIRT_VIEWER_SESSION_VNC(session);
    xmlURIPtr uri = NULL;
    gchar *portstr;
    gchar *hoststr = NULL;
    gboolean ret;

    g_return_val_if_fail(self != NULL, FALSE);
    g_return_val_if_fail(self->priv->vnc != NULL, FALSE);

    if (!(uri = xmlParseURI(uristr)))
        return FALSE;

    portstr = g_strdup_printf("%d", uri->port);

    if (uri->server) {
        if (uri->server[0] == '[') {
            gchar *tmp;
            hoststr = g_strdup(uri->server + 1);
            if ((tmp = strchr(hoststr, ']')))
                *tmp = '\0';
        } else {
            hoststr = g_strdup(uri->server);
        }
    }

    ret = vnc_display_open_host(self->priv->vnc,
                                hoststr,
                                portstr);
    g_free(portstr);
    g_free(hoststr);
    xmlFreeURI(uri);
    return ret;
}


static void
virt_viewer_session_vnc_auth_credential(GtkWidget *src,
                                        GValueArray *credList,
                                        VirtViewerSession *session)
{
    VirtViewerSessionVnc *self = VIRT_VIEWER_SESSION_VNC(session);

    virt_viewer_auth_vnc_credentials(self->priv->main_window,
                                     src,
                                     credList,
                                     NULL);
}


static void
virt_viewer_session_vnc_close(VirtViewerSession* session)
{
    VirtViewerSessionVnc *self = VIRT_VIEWER_SESSION_VNC(session);

    g_return_if_fail(self != NULL);

    DEBUG_LOG("close vnc=%p", self->priv->vnc);
    if (self->priv->vnc != NULL) {
        virt_viewer_session_clear_displays(session);
        vnc_display_close(self->priv->vnc);
        g_object_unref(self->priv->vnc);
    }

    self->priv->vnc = VNC_DISPLAY(vnc_display_new());
    g_object_ref_sink(self->priv->vnc);

    g_signal_connect(self->priv->vnc, "vnc-connected",
                     G_CALLBACK(virt_viewer_session_vnc_connected), session);
    g_signal_connect(self->priv->vnc, "vnc-initialized",
                     G_CALLBACK(virt_viewer_session_vnc_initialized), session);
    g_signal_connect(self->priv->vnc, "vnc-disconnected",
                     G_CALLBACK(virt_viewer_session_vnc_disconnected), session);

    g_signal_connect(self->priv->vnc, "vnc-bell",
                     G_CALLBACK(virt_viewer_session_vnc_bell), session);
    g_signal_connect(self->priv->vnc, "vnc-auth-failure",
                     G_CALLBACK(virt_viewer_session_vnc_auth_failure), session);
    g_signal_connect(self->priv->vnc, "vnc-auth-unsupported",
                     G_CALLBACK(virt_viewer_session_vnc_auth_unsupported), session);
    g_signal_connect(self->priv->vnc, "vnc-server-cut-text",
                     G_CALLBACK(virt_viewer_session_vnc_cut_text), session);

    g_signal_connect(self->priv->vnc, "vnc-auth-credential",
                     G_CALLBACK(virt_viewer_session_vnc_auth_credential), session);

}

VirtViewerSession *
virt_viewer_session_vnc_new(GtkWindow *main_window)
{
    VirtViewerSessionVnc *session;

    session = g_object_new(VIRT_VIEWER_TYPE_SESSION_VNC, NULL);

    session->priv->vnc = VNC_DISPLAY(vnc_display_new());
    g_object_ref_sink(session->priv->vnc);
    session->priv->main_window = g_object_ref(main_window);

    g_signal_connect(session->priv->vnc, "vnc-connected",
                     G_CALLBACK(virt_viewer_session_vnc_connected), session);
    g_signal_connect(session->priv->vnc, "vnc-initialized",
                     G_CALLBACK(virt_viewer_session_vnc_initialized), session);
    g_signal_connect(session->priv->vnc, "vnc-disconnected",
                     G_CALLBACK(virt_viewer_session_vnc_disconnected), session);

    g_signal_connect(session->priv->vnc, "vnc-bell",
                     G_CALLBACK(virt_viewer_session_vnc_bell), session);
    g_signal_connect(session->priv->vnc, "vnc-auth-failure",
                     G_CALLBACK(virt_viewer_session_vnc_auth_failure), session);
    g_signal_connect(session->priv->vnc, "vnc-auth-unsupported",
                     G_CALLBACK(virt_viewer_session_vnc_auth_unsupported), session);
    g_signal_connect(session->priv->vnc, "vnc-server-cut-text",
                     G_CALLBACK(virt_viewer_session_vnc_cut_text), session);

    g_signal_connect(session->priv->vnc, "vnc-auth-credential",
                     G_CALLBACK(virt_viewer_session_vnc_auth_credential), session);

    return VIRT_VIEWER_SESSION(session);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
