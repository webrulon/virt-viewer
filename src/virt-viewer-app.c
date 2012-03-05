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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <libxml/xpath.h>
#include <libxml/uri.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include "virt-gtk-compat.h"
#include "virt-viewer-app.h"
#include "virt-viewer-auth.h"
#include "virt-viewer-window.h"
#include "virt-viewer-session.h"
#ifdef HAVE_GTK_VNC
#include "virt-viewer-session-vnc.h"
#endif
#ifdef HAVE_SPICE_GTK
#include "virt-viewer-session-spice.h"
#endif

gboolean doDebug = FALSE;

/* Signal handlers for about dialog */
void virt_viewer_app_about_close(GtkWidget *dialog, VirtViewerApp *self);
void virt_viewer_app_about_delete(GtkWidget *dialog, void *dummy, VirtViewerApp *self);


/* Internal methods */
static void virt_viewer_app_connected(VirtViewerSession *session,
                                      VirtViewerApp *self);
static void virt_viewer_app_initialized(VirtViewerSession *session,
                                        VirtViewerApp *self);
static void virt_viewer_app_disconnected(VirtViewerSession *session,
                                         VirtViewerApp *self);
static void virt_viewer_app_auth_refused(VirtViewerSession *session,
                                         const char *msg,
                                         VirtViewerApp *self);
static void virt_viewer_app_auth_failed(VirtViewerSession *session,
                                        const char *msg,
                                        VirtViewerApp *self);
static void virt_viewer_app_usb_failed(VirtViewerSession *session,
                                       const char *msg,
                                       VirtViewerApp *self);

static void virt_viewer_app_server_cut_text(VirtViewerSession *session,
                                            const gchar *text,
                                            VirtViewerApp *self);
static void virt_viewer_app_bell(VirtViewerSession *session,
                                 VirtViewerApp *self);

static void virt_viewer_app_cancelled(VirtViewerSession *session,
                                      VirtViewerApp *self);

static void virt_viewer_app_channel_open(VirtViewerSession *session,
                                         VirtViewerSessionChannel *channel,
                                         VirtViewerApp *self);
static void virt_viewer_app_update_pretty_address(VirtViewerApp *self);
static void virt_viewer_app_set_fullscreen(VirtViewerApp *self, gboolean fullscreen);
static void virt_viewer_app_update_menu_displays(VirtViewerApp *self);


struct _VirtViewerAppPrivate {
    VirtViewerWindow *main_window;
    GtkWidget *main_notebook;
    GtkWidget *container;
    GHashTable *windows;
    gchar *clipboard;

    gboolean direct;
    gboolean verbose;
    gboolean enable_accel;
    gboolean authretry;
    gboolean started;
    gboolean fullscreen;
    gboolean attach;
    gboolean quiting;

    VirtViewerSession *session;
    gboolean active;
    gboolean connected;
    gboolean cancelled;
    guint reconnect_poll; /* source id */
    char *unixsock;
    char *guri; /* prefered over ghost:gport */
    char *ghost;
    char *gport;
    char *gtlsport;
    char *host; /* ssh */
    int port;/* ssh */
    char *user; /* ssh */
    char *transport;
    char *pretty_address;
    gchar *guest_name;
    gboolean grabbed;
    char *title;

    gint focused;
};


G_DEFINE_ABSTRACT_TYPE(VirtViewerApp, virt_viewer_app, G_TYPE_OBJECT)
#define GET_PRIVATE(o)                                                        \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIRT_VIEWER_TYPE_APP, VirtViewerAppPrivate))

enum {
    PROP_0,
    PROP_VERBOSE,
    PROP_CONTAINER,
    PROP_SESSION,
    PROP_GUEST_NAME,
    PROP_GURI,
    PROP_FULLSCREEN,
    PROP_TITLE,
    PROP_ENABLE_ACCEL,
    PROP_HAS_FOCUS,
};

enum {
    SIGNAL_WINDOW_ADDED,
    SIGNAL_WINDOW_REMOVED,
    SIGNAL_LAST,
};

static guint signals[SIGNAL_LAST];

void
virt_viewer_app_set_debug(gboolean debug)
{
    doDebug = debug;
}

void
virt_viewer_app_simple_message_dialog(VirtViewerApp *self,
                                      const char *fmt, ...)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(self));
    GtkWindow *window = GTK_WINDOW(virt_viewer_window_get_window(self->priv->main_window));
    GtkWidget *dialog;
    char *msg;
    va_list vargs;

    va_start(vargs, fmt);

    msg = g_strdup_vprintf(fmt, vargs);

    va_end(vargs);

    dialog = gtk_message_dialog_new(window,
                                    GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    "%s",
                                    msg);

    gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);

    g_free(msg);
}

void
virt_viewer_app_quit(VirtViewerApp *self)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(self));
    VirtViewerAppPrivate *priv = self->priv;

    if (priv->session) {
        virt_viewer_session_close(VIRT_VIEWER_SESSION(priv->session));
        if (priv->connected) {
            priv->quiting = TRUE;
            return;
        }
    }

    gtk_main_quit();
}

static void count_window_visible(gpointer key G_GNUC_UNUSED,
                                 gpointer value,
                                 gpointer user_data)
{
    GtkWindow *win = virt_viewer_window_get_window(VIRT_VIEWER_WINDOW(value));
    guint *n = (guint*)user_data;

    if (gtk_widget_get_visible(GTK_WIDGET(win)))
        *n += 1;
}

static guint
virt_viewer_app_get_n_windows_visible(VirtViewerApp *self)
{
    guint n = 0;
    g_hash_table_foreach(self->priv->windows, count_window_visible, &n);
    return n;
}

static guint
virt_viewer_app_get_n_windows(VirtViewerApp *self)
{
    return g_hash_table_size(self->priv->windows);
}

gboolean
virt_viewer_app_window_set_visible(VirtViewerApp *self,
                                   VirtViewerWindow *window,
                                   gboolean visible)
{
    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);
    g_return_val_if_fail(VIRT_VIEWER_IS_WINDOW(window), FALSE);

    if (visible) {
        virt_viewer_window_show(window);
        return TRUE;
    } else {
        if (virt_viewer_app_get_n_windows_visible(self) > 1) {
            virt_viewer_window_hide(window);
            return FALSE;
        } else if (virt_viewer_app_get_n_windows(self) > 1) {
            GtkWidget *dialog =
                gtk_message_dialog_new (virt_viewer_window_get_window(window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_OK_CANCEL,
                                        _("This is the last visible display. Do you want to quit?"));
            gint result = gtk_dialog_run (GTK_DIALOG (dialog));
            switch (result) {
            case GTK_RESPONSE_OK:
                virt_viewer_app_quit(self);
                break;
            default:
                break;
            }
            gtk_widget_destroy(dialog);
            return FALSE;
        } else {
            virt_viewer_app_quit(self);
            return FALSE;
        }
    }

    g_warn_if_reached();
    return FALSE;
}

G_MODULE_EXPORT void
virt_viewer_app_about_close(GtkWidget *dialog,
                            VirtViewerApp *self G_GNUC_UNUSED)
{
    gtk_widget_hide(dialog);
    gtk_widget_destroy(dialog);
}

G_MODULE_EXPORT void
virt_viewer_app_about_delete(GtkWidget *dialog,
                             void *dummy G_GNUC_UNUSED,
                             VirtViewerApp *self G_GNUC_UNUSED)
{
    gtk_widget_hide(dialog);
    gtk_widget_destroy(dialog);
}

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)

static int
virt_viewer_app_open_tunnel(const char **cmd)
{
    int fd[2];
    pid_t pid;

    if (socketpair(PF_UNIX, SOCK_STREAM, 0, fd) < 0)
        return -1;

    pid = fork();
    if (pid == -1) {
        close(fd[0]);
        close(fd[1]);
        return -1;
    }

    if (pid == 0) { /* child */
        close(fd[0]);
        close(0);
        close(1);
        if (dup(fd[1]) < 0)
            _exit(1);
        if (dup(fd[1]) < 0)
            _exit(1);
        close(fd[1]);
        execvp("ssh", (char *const*)cmd);
        _exit(1);
    }
    close(fd[1]);
    return fd[0];
}


static int
virt_viewer_app_open_tunnel_ssh(const char *sshhost,
                                int sshport,
                                const char *sshuser,
                                const char *host,
                                const char *port,
                                const char *unixsock)
{
    const char *cmd[10];
    char portstr[50];
    int n = 0;

    cmd[n++] = "ssh";
    if (sshport) {
        cmd[n++] = "-p";
        sprintf(portstr, "%d", sshport);
        cmd[n++] = portstr;
    }
    if (sshuser) {
        cmd[n++] = "-l";
        cmd[n++] = sshuser;
    }
    cmd[n++] = sshhost;
    cmd[n++] = "nc";
    if (port) {
        cmd[n++] = host;
        cmd[n++] = port;
    } else {
        cmd[n++] = "-U";
        cmd[n++] = unixsock;
    }
    cmd[n++] = NULL;

    return virt_viewer_app_open_tunnel(cmd);
}

static int
virt_viewer_app_open_unix_sock(const char *unixsock)
{
    struct sockaddr_un addr;
    int fd;

    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, unixsock);

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return -1;

    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

#endif /* defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK) */

void
virt_viewer_app_trace(VirtViewerApp *self,
                      const char *fmt, ...)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(self));
    va_list ap;
    VirtViewerAppPrivate *priv = self->priv;

    if (doDebug) {
        va_start(ap, fmt);
        g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, fmt, ap);
        va_end(ap);
    }

    if (priv->verbose) {
        va_start(ap, fmt);
        g_vprintf(fmt, ap);
        va_end(ap);
    }
}

static void
virt_viewer_app_set_window_subtitle(VirtViewerApp *app,
                                    VirtViewerWindow *window,
                                    int nth)
{
    gchar *subtitle = NULL;

    if (app->priv->title != NULL) {
        gchar *d = strstr(app->priv->title, "%d");
        if (d != NULL) {
            *d = '\0';
            subtitle = g_strdup_printf("%s%d%s", app->priv->title, nth + 1, d + 2);
            *d = '%';
        } else
            subtitle = g_strdup_printf("%s (%d)", app->priv->title, nth + 1);
    }

    g_object_set(window, "subtitle", subtitle, NULL);
    g_free(subtitle);
}

static void
set_title(gpointer key,
          gpointer value,
          gpointer user_data)
{
    gint *nth = key;
    VirtViewerApp *app = user_data;
    VirtViewerWindow *window = value;
    virt_viewer_app_set_window_subtitle(app, window, *nth);
}

static void
virt_viewer_app_set_all_window_subtitles(VirtViewerApp *app)
{
    g_hash_table_foreach(app->priv->windows, set_title, app);
}

static void update_title(gpointer key G_GNUC_UNUSED,
                         gpointer value,
                         gpointer user_data G_GNUC_UNUSED)
{
    virt_viewer_window_update_title(VIRT_VIEWER_WINDOW(value));
}

static void
virt_viewer_app_update_title(VirtViewerApp *self)
{
    g_hash_table_foreach(self->priv->windows, update_title, NULL);
}

static void set_usb_options_sensitive(gpointer key G_GNUC_UNUSED,
                                      gpointer value,
                                      gpointer user_data)
{
    virt_viewer_window_set_usb_options_sensitive(VIRT_VIEWER_WINDOW(value),
                                                 GPOINTER_TO_INT(user_data));
}

static void
virt_viewer_app_set_usb_options_sensitive(VirtViewerApp *self, gboolean sensitive)
{
    g_hash_table_foreach(self->priv->windows, set_usb_options_sensitive,
                         GINT_TO_POINTER(sensitive));
}

static VirtViewerWindow *
virt_viewer_app_get_nth_window(VirtViewerApp *self, gint nth)
{
    return g_hash_table_lookup(self->priv->windows, &nth);
}

static gboolean
virt_viewer_app_remove_nth_window(VirtViewerApp *self, gint nth)
{
    VirtViewerWindow *win;
    gboolean removed;

    g_return_val_if_fail(nth != 0, FALSE);

    win = virt_viewer_app_get_nth_window(self, nth);
    g_return_val_if_fail(win != NULL, FALSE);

    removed = g_hash_table_steal(self->priv->windows, &nth);
    g_warn_if_fail(removed);

    if (removed)
        g_signal_emit(self, signals[SIGNAL_WINDOW_REMOVED], 0, win);

    g_object_unref(win);

    return removed;
}

static void
virt_viewer_app_set_nth_window(VirtViewerApp *self, gint nth, VirtViewerWindow *win)
{
    gint *key;

    g_return_if_fail(virt_viewer_app_get_nth_window(self, nth) == NULL);
    key = g_malloc(sizeof(gint));
    *key = nth;
    g_hash_table_insert(self->priv->windows, key, win);
    virt_viewer_app_set_window_subtitle(self, win, nth);

    g_signal_emit(self, signals[SIGNAL_WINDOW_ADDED], 0, win);
}

static void
viewer_window_visible_cb(GtkWidget *widget G_GNUC_UNUSED,
                         gpointer user_data)
{
    virt_viewer_app_update_menu_displays(VIRT_VIEWER_APP(user_data));
}

static gboolean
viewer_window_focus_in_cb(GtkWindow *window G_GNUC_UNUSED,
                          GdkEvent *event G_GNUC_UNUSED,
                          VirtViewerApp *self)
{
    self->priv->focused += 1;

    if (self->priv->focused == 1)
        g_object_notify(G_OBJECT(self), "has-focus");

    return FALSE;
}

static gboolean
viewer_window_focus_out_cb(GtkWindow *window G_GNUC_UNUSED,
                           GdkEvent *event G_GNUC_UNUSED,
                           VirtViewerApp *self)
{
    self->priv->focused -= 1;
    g_warn_if_fail(self->priv->focused >= 0);

    if (self->priv->focused <= 0)
        g_object_notify(G_OBJECT(self), "has-focus");

    return FALSE;
}

static VirtViewerWindow*
virt_viewer_app_window_new(VirtViewerApp *self, GtkWidget *container, gint nth)
{
    VirtViewerWindow* window;
    GtkWindow *w;

    window = g_object_new(VIRT_VIEWER_TYPE_WINDOW,
                          "app", self,
                          "container", container,
                          NULL);
    if (self->priv->main_window)
        virt_viewer_window_set_zoom_level(window, virt_viewer_window_get_zoom_level(self->priv->main_window));
    virt_viewer_app_set_nth_window(self, nth, window);
    w = virt_viewer_window_get_window(window);

    /* this will set new window to fullscreen if necessary */
    virt_viewer_app_set_fullscreen(self, self->priv->fullscreen);
    g_signal_connect(w, "hide", G_CALLBACK(viewer_window_visible_cb), self);
    g_signal_connect(w, "show", G_CALLBACK(viewer_window_visible_cb), self);
    g_signal_connect(w, "focus-in-event", G_CALLBACK(viewer_window_focus_in_cb), self);
    g_signal_connect(w, "focus-out-event", G_CALLBACK(viewer_window_focus_out_cb), self);
    return window;
}

static void
display_show_hint(VirtViewerDisplay *display,
                  GParamSpec *pspec G_GNUC_UNUSED,
                  VirtViewerWindow *win)
{
    VirtViewerApp *self;
    VirtViewerNotebook *nb = virt_viewer_window_get_notebook(win);
    gint nth, hint;

    g_object_get(win,
                 "app", &self,
                 NULL);
    g_object_get(display,
                 "nth-display", &nth,
                 "show-hint", &hint,
                 NULL);

    if (hint == VIRT_VIEWER_DISPLAY_SHOW_HINT_HIDE) {
        if (win != self->priv->main_window &&
            g_getenv("VIRT_VIEWER_HIDE"))
            virt_viewer_window_hide(win);
        virt_viewer_notebook_show_status(nb, _("Waiting for display %d..."), nth + 1);
    } else {
        virt_viewer_notebook_show_display(nb);
        virt_viewer_window_show(win);
        gtk_window_present(virt_viewer_window_get_window(win));
    }

    g_object_unref(self);
}

static void
virt_viewer_app_display_added(VirtViewerSession *session G_GNUC_UNUSED,
                              VirtViewerDisplay *display,
                              VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv = self->priv;
    VirtViewerWindow *window;
    gint nth;

    g_object_get(display, "nth-display", &nth, NULL);
    if (nth == 0) {
        window = priv->main_window;
    } else {
        if (priv->container) {
            g_warning("multi-head not yet supported within container");
            return;
        }

        g_return_if_fail(virt_viewer_app_get_nth_window(self, nth) == NULL);
        window = virt_viewer_app_window_new(self, NULL, nth);
    }

    virt_viewer_window_set_display(window, display);
    g_signal_connect(display, "notify::show-hint",
                     G_CALLBACK(display_show_hint), window);
    g_object_notify(G_OBJECT(display), "show-hint"); /* call display_show_hint */
}


static void
virt_viewer_app_display_removed(VirtViewerSession *session G_GNUC_UNUSED,
                                VirtViewerDisplay *display,
                                VirtViewerApp *self)
{
    VirtViewerWindow *win = NULL;
    gint nth;

    gtk_widget_hide(GTK_WIDGET(display));
    g_object_get(display, "nth-display", &nth, NULL);
    win = virt_viewer_app_get_nth_window(self, nth);
    virt_viewer_window_set_display(win, NULL);

    if (nth != 0)
        virt_viewer_app_remove_nth_window(self, nth);
}

int
virt_viewer_app_create_session(VirtViewerApp *self, const gchar *type)
{
    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), -1);
    VirtViewerAppPrivate *priv = self->priv;
    g_return_val_if_fail(priv->session == NULL, -1);

#ifdef HAVE_GTK_VNC
    if (g_ascii_strcasecmp(type, "vnc") == 0) {
        GtkWindow *window = virt_viewer_window_get_window(priv->main_window);
        virt_viewer_app_trace(self, "Guest %s has a %s display\n",
                              priv->guest_name, type);
        priv->session = virt_viewer_session_vnc_new(window);
    } else
#endif
#ifdef HAVE_SPICE_GTK
        if (g_ascii_strcasecmp(type, "spice") == 0) {
            GtkWindow *window = virt_viewer_window_get_window(priv->main_window);
            virt_viewer_app_trace(self, "Guest %s has a %s display\n",
                                  priv->guest_name, type);
            priv->session = virt_viewer_session_spice_new(window);
        } else
#endif
            {
                virt_viewer_app_trace(self, "Guest %s has unsupported %s display type\n",
                                      priv->guest_name, type);
                virt_viewer_app_simple_message_dialog(self, _("Unknown graphic type for the guest %s"),
                                                      priv->guest_name);
                return -1;
            }

    g_signal_connect(priv->session, "session-initialized",
                     G_CALLBACK(virt_viewer_app_initialized), self);
    g_signal_connect(priv->session, "session-connected",
                     G_CALLBACK(virt_viewer_app_connected), self);
    g_signal_connect(priv->session, "session-disconnected",
                     G_CALLBACK(virt_viewer_app_disconnected), self);
    g_signal_connect(priv->session, "session-channel-open",
                     G_CALLBACK(virt_viewer_app_channel_open), self);
    g_signal_connect(priv->session, "session-auth-refused",
                     G_CALLBACK(virt_viewer_app_auth_refused), self);
    g_signal_connect(priv->session, "session-auth-failed",
                     G_CALLBACK(virt_viewer_app_auth_failed), self);
    g_signal_connect(priv->session, "session-usb-failed",
                     G_CALLBACK(virt_viewer_app_usb_failed), self);
    g_signal_connect(priv->session, "session-display-added",
                     G_CALLBACK(virt_viewer_app_display_added), self);
    g_signal_connect(priv->session, "session-display-removed",
                     G_CALLBACK(virt_viewer_app_display_removed), self);

    g_signal_connect(priv->session, "session-cut-text",
                     G_CALLBACK(virt_viewer_app_server_cut_text), self);
    g_signal_connect(priv->session, "session-bell",
                     G_CALLBACK(virt_viewer_app_bell), self);
    g_signal_connect(priv->session, "session-cancelled",
                     G_CALLBACK(virt_viewer_app_cancelled), self);

    return 0;
}

static gboolean
virt_viewer_app_default_open_connection(VirtViewerApp *self G_GNUC_UNUSED, int *fd)
{
    *fd = -1;
    return TRUE;
}


static int
virt_viewer_app_open_connection(VirtViewerApp *self, int *fd)
{
    VirtViewerAppClass *klass;

    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), -1);
    klass = VIRT_VIEWER_APP_GET_CLASS(self);

    return klass->open_connection(self, fd);
}


#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)
static void
virt_viewer_app_channel_open(VirtViewerSession *session,
                             VirtViewerSessionChannel *channel,
                             VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv;
    int fd = -1;

    g_return_if_fail(self != NULL);

    if (!virt_viewer_app_open_connection(self, &fd))
        return;

    DEBUG_LOG("After open connection callback fd=%d", fd);

    priv = self->priv;
    if (priv->transport && g_ascii_strcasecmp(priv->transport, "ssh") == 0 &&
        !priv->direct && fd == -1) {
        if ((fd = virt_viewer_app_open_tunnel_ssh(priv->host, priv->port, priv->user,
                                                  priv->ghost, priv->gport, NULL)) < 0)
            virt_viewer_app_simple_message_dialog(self, _("Connect to ssh failed."));
    } else if (fd == -1) {
        virt_viewer_app_simple_message_dialog(self, _("Can't connect to channel, SSH only supported."));
    }

    if (fd >= 0)
        virt_viewer_session_channel_open_fd(session, channel, fd);
}
#else
static void
virt_viewer_app_channel_open(VirtViewerSession *session G_GNUC_UNUSED,
                             VirtViewerSessionChannel *channel G_GNUC_UNUSED,
                             VirtViewerApp *self)
{
    virt_viewer_app_simple_message_dialog(self, _("Connect to channel unsupported."));
}
#endif

static int
virt_viewer_app_default_activate(VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv = self->priv;
    int fd = -1;

    if (!virt_viewer_app_open_connection(self, &fd))
        return -1;

    DEBUG_LOG("After open connection callback fd=%d", fd);

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)
    if (priv->transport &&
        g_ascii_strcasecmp(priv->transport, "ssh") == 0 &&
        !priv->direct &&
        fd == -1) {
        gchar *p = NULL;

        if (priv->gport) {
            virt_viewer_app_trace(self, "Opening indirect TCP connection to display at %s:%s\n",
                                  priv->ghost, priv->gport);
        } else {
            virt_viewer_app_trace(self, "Opening indirect UNIX connection to display at %s\n",
                                  priv->unixsock);
        }
        if (priv->port)
            p = g_strdup_printf(":%d", priv->port);

        virt_viewer_app_trace(self, "Setting up SSH tunnel via %s%s%s%s\n",
                              priv->user ? priv->user : "",
                              priv->user ? "@" : "",
                              priv->host, p ? p : "");
        g_free(p);

        if ((fd = virt_viewer_app_open_tunnel_ssh(priv->host, priv->port,
                                                  priv->user, priv->ghost,
                                                  priv->gport, priv->unixsock)) < 0)
            return -1;
    } else if (priv->unixsock && fd == -1) {
        virt_viewer_app_trace(self, "Opening direct UNIX connection to display at %s",
                              priv->unixsock);
        if ((fd = virt_viewer_app_open_unix_sock(priv->unixsock)) < 0)
            return -1;
    }
#endif

    if (fd >= 0) {
        return virt_viewer_session_open_fd(VIRT_VIEWER_SESSION(priv->session), fd);
    } else if (priv->guri) {
        virt_viewer_app_trace(self, "Opening connection to display at %s\n", priv->guri);
        return virt_viewer_session_open_uri(VIRT_VIEWER_SESSION(priv->session), priv->guri);
    } else {
        virt_viewer_app_trace(self, "Opening direct TCP connection to display at %s:%s:%s\n",
                              priv->ghost, priv->gport, priv->gtlsport ? priv->gtlsport : "-1");
        return virt_viewer_session_open_host(VIRT_VIEWER_SESSION(priv->session),
                                             priv->ghost, priv->gport, priv->gtlsport);
    }

    return -1;
}

int
virt_viewer_app_activate(VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv;
    int ret;

    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), -1);

    priv = self->priv;
    if (priv->active)
        return -1;

    ret = VIRT_VIEWER_APP_GET_CLASS(self)->activate(self);

    if (ret != -1) {
        virt_viewer_app_show_status(self, _("Connecting to graphic server"));
        priv->connected = FALSE;
        priv->cancelled = FALSE;
        priv->active = TRUE;
        priv->grabbed = FALSE;
        virt_viewer_app_update_title(self);
    }

    return ret;
}

/* text was actually requested */
static void
virt_viewer_app_clipboard_copy(GtkClipboard *clipboard G_GNUC_UNUSED,
                               GtkSelectionData *data,
                               guint info G_GNUC_UNUSED,
                               VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv = self->priv;

    gtk_selection_data_set_text(data, priv->clipboard, -1);
}

static void
virt_viewer_app_server_cut_text(VirtViewerSession *session G_GNUC_UNUSED,
                                const gchar *text,
                                VirtViewerApp *self)
{
    GtkClipboard *cb;
    gsize a, b;
    VirtViewerAppPrivate *priv = self->priv;
    GtkTargetEntry targets[] = {
        {g_strdup("UTF8_STRING"), 0, 0},
        {g_strdup("COMPOUND_TEXT"), 0, 0},
        {g_strdup("TEXT"), 0, 0},
        {g_strdup("STRING"), 0, 0},
    };

    if (!text)
        return;

    g_free (priv->clipboard);
    priv->clipboard = g_convert (text, -1, "utf-8", "iso8859-1", &a, &b, NULL);

    if (priv->clipboard) {
        cb = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

        gtk_clipboard_set_with_owner (cb,
                                      targets,
                                      G_N_ELEMENTS(targets),
                                      (GtkClipboardGetFunc)virt_viewer_app_clipboard_copy,
                                      NULL,
                                      G_OBJECT (self));
    }
}


static void virt_viewer_app_bell(VirtViewerSession *session G_GNUC_UNUSED,
                                 VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv = self->priv;

    gdk_window_beep(gtk_widget_get_window(GTK_WIDGET(virt_viewer_window_get_window(priv->main_window))));
}


static int
virt_viewer_app_default_initial_connect(VirtViewerApp *self)
{
    return virt_viewer_app_activate(self);
}

int
virt_viewer_app_initial_connect(VirtViewerApp *self)
{
    VirtViewerAppClass *klass;

    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), -1);
    klass = VIRT_VIEWER_APP_GET_CLASS(self);

    return klass->initial_connect(self);
}

static gboolean
virt_viewer_app_retryauth(gpointer opaque)
{
    VirtViewerApp *self = opaque;

    virt_viewer_app_initial_connect(self);

    return FALSE;
}

static gboolean
virt_viewer_app_connect_timer(void *opaque)
{
    VirtViewerApp *self = opaque;
    VirtViewerAppPrivate *priv = self->priv;

    DEBUG_LOG("Connect timer fired");

    if (!priv->active &&
        virt_viewer_app_initial_connect(self) < 0)
        gtk_main_quit();

    if (priv->active) {
        priv->reconnect_poll = 0;
        return FALSE;
    }

    return TRUE;
}

void
virt_viewer_app_start_reconnect_poll(VirtViewerApp *self)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(self));
    VirtViewerAppPrivate *priv = self->priv;

    if (priv->reconnect_poll != 0)
        return;

    priv->reconnect_poll = g_timeout_add(500, virt_viewer_app_connect_timer, self);
}

static void
virt_viewer_app_default_deactivated(VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv = self->priv;

    virt_viewer_app_show_status(self, _("Guest domain has shutdown"));
    virt_viewer_app_trace(self, "Guest %s display has disconnected, shutting down",
                          priv->guest_name);
    gtk_main_quit();
}

static void
virt_viewer_app_deactivated(VirtViewerApp *self)
{
    VirtViewerAppClass *klass;
    klass = VIRT_VIEWER_APP_GET_CLASS(self);

    klass->deactivated(self);
}

static void
virt_viewer_app_deactivate(VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv = self->priv;

    if (!priv->active)
        return;

    if (priv->session)
        virt_viewer_session_close(VIRT_VIEWER_SESSION(priv->session));

    priv->connected = FALSE;
    priv->active = FALSE;
#if 0
    g_free(priv->pretty_address);
    priv->pretty_address = NULL;
#endif
    priv->grabbed = FALSE;
    virt_viewer_app_update_title(self);

    if (priv->authretry) {
        priv->authretry = FALSE;
        g_idle_add(virt_viewer_app_retryauth, self);
    } else
        virt_viewer_app_deactivated(self);

}

static void
virt_viewer_app_connected(VirtViewerSession *session G_GNUC_UNUSED,
                          VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv = self->priv;

    priv->connected = TRUE;
    virt_viewer_app_show_status(self, _("Connected to graphic server"));
}



static void
virt_viewer_app_initialized(VirtViewerSession *session G_GNUC_UNUSED,
                            VirtViewerApp *self)
{
    gboolean has_usb = virt_viewer_session_has_usb(self->priv->session);

    virt_viewer_app_update_title(self);
    virt_viewer_app_set_usb_options_sensitive(self, has_usb);
}

static void
virt_viewer_app_disconnected(VirtViewerSession *session G_GNUC_UNUSED,
                             VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv = self->priv;

    if (priv->quiting)
        gtk_main_quit();

    if (!priv->connected && !priv->cancelled) {
        virt_viewer_app_simple_message_dialog(self,
                                              _("Unable to connect to the graphic server %s"),
                                              priv->pretty_address);
    }
    virt_viewer_app_set_usb_options_sensitive(self, FALSE);
    virt_viewer_app_deactivate(self);
}

static void virt_viewer_app_cancelled(VirtViewerSession *session,
                                      VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv = self->priv;
    priv->cancelled = TRUE;
    virt_viewer_app_disconnected(session, self);
}


static void virt_viewer_app_auth_refused(VirtViewerSession *session G_GNUC_UNUSED,
                                         const char *msg,
                                         VirtViewerApp *self)
{
    GtkWidget *dialog;
    int ret;
    VirtViewerAppPrivate *priv = self->priv;

    dialog = gtk_message_dialog_new(virt_viewer_window_get_window(priv->main_window),
                                    GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_YES_NO,
                                    _("Unable to authenticate with remote desktop server at %s: %s\n"
                                      "Retry connection again?"),
                                    priv->pretty_address, msg);

    ret = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);

    if (ret == GTK_RESPONSE_YES)
        priv->authretry = TRUE;
    else
        priv->authretry = FALSE;
}


static void virt_viewer_app_auth_failed(VirtViewerSession *session G_GNUC_UNUSED,
                                        const char *msg,
                                        VirtViewerApp *self)
{
    virt_viewer_app_simple_message_dialog(self,
                                          _("Unable to authenticate with remote desktop server: %s"),
                                          msg);
}

static void virt_viewer_app_usb_failed(VirtViewerSession *session G_GNUC_UNUSED,
                                       const gchar *msg,
                                       VirtViewerApp *self)
{
    virt_viewer_app_simple_message_dialog(self, _("USB redirection error: %s"), msg);
}

static void
virt_viewer_app_get_property (GObject *object, guint property_id,
                              GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(object));
    VirtViewerApp *self = VIRT_VIEWER_APP(object);
    VirtViewerAppPrivate *priv = self->priv;

    switch (property_id) {
    case PROP_VERBOSE:
        g_value_set_boolean(value, priv->verbose);
        break;

    case PROP_CONTAINER:
        g_value_set_object(value, priv->container);
        break;

    case PROP_SESSION:
        g_value_set_object(value, priv->session);
        break;

    case PROP_GUEST_NAME:
        g_value_set_string(value, priv->guest_name);
        break;

    case PROP_GURI:
        g_value_set_string(value, priv->guri);
        break;

    case PROP_FULLSCREEN:
        g_value_set_boolean(value, priv->fullscreen);
        break;

    case PROP_TITLE:
        g_value_set_string(value, priv->title);
        break;

    case PROP_ENABLE_ACCEL:
        g_value_set_boolean(value, virt_viewer_app_get_enable_accel(self));
        break;

    case PROP_HAS_FOCUS:
        g_value_set_boolean(value, priv->focused > 0);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
virt_viewer_app_set_property (GObject *object, guint property_id,
                              const GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(object));
    VirtViewerApp *self = VIRT_VIEWER_APP(object);
    VirtViewerAppPrivate *priv = self->priv;

    switch (property_id) {
    case PROP_VERBOSE:
        priv->verbose = g_value_get_boolean(value);
        break;

    case PROP_CONTAINER:
        g_return_if_fail(priv->container == NULL);
        priv->container = g_value_dup_object(value);
        break;

    case PROP_GUEST_NAME:
        g_free(priv->guest_name);
        priv->guest_name = g_value_dup_string(value);
        break;

    case PROP_GURI:
        g_free(priv->guri);
        priv->guri = g_value_dup_string(value);
        virt_viewer_app_update_pretty_address(self);
        break;

    case PROP_FULLSCREEN:
        virt_viewer_app_set_fullscreen(self, g_value_get_boolean(value));
        break;

    case PROP_TITLE:
        g_free(priv->title);
        priv->title = g_value_dup_string(value);
        virt_viewer_app_set_all_window_subtitles(self);
        break;

    case PROP_ENABLE_ACCEL:
        priv->enable_accel = g_value_get_boolean(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
virt_viewer_app_dispose (GObject *object)
{
    VirtViewerApp *self = VIRT_VIEWER_APP(object);
    VirtViewerAppPrivate *priv = self->priv;

    if (priv->windows) {
        g_hash_table_unref(priv->windows);
        priv->windows = NULL;
        priv->main_window = NULL;
    }

    if (priv->container) {
        g_object_unref(priv->container);
        priv->container = NULL;
    }

    if (priv->session) {
        g_object_unref(priv->session);
        priv->session = NULL;
    }
    g_free(priv->title);

    virt_viewer_app_free_connect_info(self);

    G_OBJECT_CLASS (virt_viewer_app_parent_class)->dispose (object);
}

static gboolean
virt_viewer_app_default_start(VirtViewerApp *self)
{
    virt_viewer_window_show(self->priv->main_window);
    return TRUE;
}

gboolean virt_viewer_app_start(VirtViewerApp *self)
{
    VirtViewerAppClass *klass;

    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);
    klass = VIRT_VIEWER_APP_GET_CLASS(self);

    g_return_val_if_fail(!self->priv->started, TRUE);

    self->priv->started = klass->start(self);
    return self->priv->started;
}

static void
virt_viewer_app_init (VirtViewerApp *self)
{
    self->priv = GET_PRIVATE(self);
    self->priv->windows = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, g_object_unref);
}

static GObject *
virt_viewer_app_constructor (GType gtype,
                             guint n_properties,
                             GObjectConstructParam *properties)
{
    GObject *obj;
    VirtViewerApp *self;
    VirtViewerAppPrivate *priv;

    obj = G_OBJECT_CLASS (virt_viewer_app_parent_class)->constructor (gtype, n_properties, properties);
    self = VIRT_VIEWER_APP(obj);
    priv = self->priv;

    priv->main_window = virt_viewer_app_window_new(self, priv->container, 0);
    priv->main_notebook = GTK_WIDGET(virt_viewer_window_get_notebook(priv->main_window));

    gtk_accel_map_add_entry("<virt-viewer>/file/smartcard-insert", GDK_F8, GDK_SHIFT_MASK);
    gtk_accel_map_add_entry("<virt-viewer>/file/smartcard-remove", GDK_F9, GDK_SHIFT_MASK);
    gtk_accel_map_add_entry("<virt-viewer>/view/fullscreen", GDK_F11, 0);
    gtk_accel_map_add_entry("<virt-viewer>/view/release-cursor", GDK_F12, GDK_SHIFT_MASK);

    return obj;
}

static void
virt_viewer_app_class_init (VirtViewerAppClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (VirtViewerAppPrivate));

    object_class->constructor = virt_viewer_app_constructor;
    object_class->get_property = virt_viewer_app_get_property;
    object_class->set_property = virt_viewer_app_set_property;
    object_class->dispose = virt_viewer_app_dispose;

    klass->start = virt_viewer_app_default_start;
    klass->initial_connect = virt_viewer_app_default_initial_connect;
    klass->activate = virt_viewer_app_default_activate;
    klass->deactivated = virt_viewer_app_default_deactivated;
    klass->open_connection = virt_viewer_app_default_open_connection;

    g_object_class_install_property(object_class,
                                    PROP_VERBOSE,
                                    g_param_spec_boolean("verbose",
                                                         "Verbose",
                                                         "Verbose trace",
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class,
                                    PROP_CONTAINER,
                                    g_param_spec_object("container",
                                                        "Container",
                                                        "Widget container",
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class,
                                    PROP_SESSION,
                                    g_param_spec_object("session",
                                                        "Session",
                                                        "ViewerSession",
                                                        VIRT_VIEWER_TYPE_SESSION,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class,
                                    PROP_GUEST_NAME,
                                    g_param_spec_string("guest-name",
                                                        "Guest name",
                                                        "Guest name",
                                                        "",
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class,
                                    PROP_GURI,
                                    g_param_spec_string("guri",
                                                        "guri",
                                                        "Remote graphical URI",
                                                        "",
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class,
                                    PROP_FULLSCREEN,
                                    g_param_spec_boolean("fullscreen",
                                                         "Fullscreen",
                                                         "Fullscreen",
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class,
                                    PROP_TITLE,
                                    g_param_spec_string("title",
                                                        "Title",
                                                        "Title",
                                                        "",
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class,
                                    PROP_ENABLE_ACCEL,
                                    g_param_spec_boolean("enable-accel",
                                                         "Enable Accel",
                                                         "Enable accelerators",
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT |
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class,
                                    PROP_HAS_FOCUS,
                                    g_param_spec_boolean("has-focus",
                                                         "Has Focus",
                                                         "Application has focus",
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

    signals[SIGNAL_WINDOW_ADDED] =
        g_signal_new("window-added",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(VirtViewerAppClass, window_added),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_OBJECT);

    signals[SIGNAL_WINDOW_REMOVED] =
        g_signal_new("window-removed",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(VirtViewerAppClass, window_removed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_OBJECT);
}

void
virt_viewer_app_set_direct(VirtViewerApp *self, gboolean direct)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(self));

    self->priv->direct = direct;
}

void
virt_viewer_app_set_attach(VirtViewerApp *self, gboolean attach)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(self));

    self->priv->attach = attach;
}

gboolean
virt_viewer_app_get_attach(VirtViewerApp *self)
{
    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);

    return self->priv->attach;
}

gboolean
virt_viewer_app_is_active(VirtViewerApp *self)
{
    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);

    return self->priv->active;
}

gboolean
virt_viewer_app_has_session(VirtViewerApp *self)
{
    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);

    return self->priv->session != NULL;
}

static void
virt_viewer_app_update_pretty_address(VirtViewerApp *self)
{
    VirtViewerAppPrivate *priv;

    priv = self->priv;
    g_free(priv->pretty_address);
    if (priv->guri)
        priv->pretty_address = g_strdup(priv->guri);
    else if (priv->gport)
        priv->pretty_address = g_strdup_printf("%s:%s", priv->ghost, priv->gport);
    else
        priv->pretty_address = g_strdup_printf("%s:%s", priv->host, priv->unixsock);
}

typedef struct {
    gboolean fullscreen;
    gboolean move;
} FullscreenOptions;

static void fullscreen_cb(gpointer key,
                          gpointer value,
                          gpointer user_data)
{
    gint nth = *(gint*)key;
    FullscreenOptions *options = (FullscreenOptions *)user_data;
    VirtViewerWindow *vwin = VIRT_VIEWER_WINDOW(value);

    DEBUG_LOG("fullscreen display %d: %d", nth, options->fullscreen);
    if (options->fullscreen) {
        GdkScreen *screen = gdk_screen_get_default ();
        GdkRectangle mon;

        if (nth >= gdk_screen_get_n_monitors(screen)) {
            DEBUG_LOG("skipping display %d", nth);
            return;
        }
        gdk_screen_get_monitor_geometry(screen, nth, &mon);
        virt_viewer_window_enter_fullscreen(vwin, options->move, mon.x, mon.y);
    } else
        virt_viewer_window_leave_fullscreen(vwin);
}

static void
virt_viewer_app_set_fullscreen(VirtViewerApp *self, gboolean fullscreen)
{
    VirtViewerAppPrivate *priv = self->priv;
    FullscreenOptions options  = {
        .fullscreen = fullscreen,
        .move = virt_viewer_app_get_n_windows_visible(self) > 1,
    };

    /* we iterate unconditionnaly, even if it was set before to update new windows */
    priv->fullscreen = fullscreen;
    g_hash_table_foreach(priv->windows, fullscreen_cb, &options);
}

static void
menu_display_visible_toggled_cb(GtkCheckMenuItem *checkmenuitem,
                                VirtViewerWindow *vwin)
{
    VirtViewerApp *self;
    gboolean visible;
    static gboolean reentering = FALSE;

    if (reentering) /* do not reenter if I switch you back */
        return;

    reentering = TRUE;
    g_object_get(vwin, "app", &self, NULL);
    visible = virt_viewer_app_window_set_visible(self, vwin,
                                                 gtk_check_menu_item_get_active(checkmenuitem));
    gtk_check_menu_item_set_active(checkmenuitem, /* will be toggled again */ !visible);
    g_object_unref(self);
    reentering = FALSE;
}

static gint
update_menu_displays_sort(gconstpointer a, gconstpointer b)
{
    const int *ai = a;
    const int *bi = b;

    if (*ai > *bi)
        return 1;
    else if (*ai < *bi)
        return -1;
    else
        return 0;
}

static void
window_update_menu_displays_cb(gpointer key G_GNUC_UNUSED,
                               gpointer value,
                               gpointer user_data)
{
    VirtViewerApp *self = VIRT_VIEWER_APP(user_data);
    VirtViewerWindow *window = VIRT_VIEWER_WINDOW(value);
    GtkMenuShell *submenu = GTK_MENU_SHELL(gtk_menu_new());
    GtkMenuItem *menu = virt_viewer_window_get_menu_displays(window);
    GList *keys = g_hash_table_get_keys(self->priv->windows);
    GList *tmp;

    keys = g_list_sort(keys, update_menu_displays_sort);

    tmp = keys;
    while (tmp) {
        int *nth = tmp->data;
        VirtViewerWindow *vwin = VIRT_VIEWER_WINDOW(g_hash_table_lookup(self->priv->windows, nth));
        GtkWidget *item;
        gboolean visible;

        item = gtk_check_menu_item_new_with_label(g_strdup_printf("Display %d", *nth));
        visible = gtk_widget_get_visible(GTK_WIDGET(virt_viewer_window_get_window(vwin)));
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), visible);
        g_signal_connect(G_OBJECT(item),
                         "toggled", G_CALLBACK(menu_display_visible_toggled_cb), vwin);
        gtk_menu_shell_append(submenu, item);
        tmp = tmp->next;
    }

    gtk_widget_show_all(GTK_WIDGET(submenu));
    gtk_menu_item_set_submenu(menu, GTK_WIDGET(submenu));
    g_list_free(keys);
}

static void
virt_viewer_app_update_menu_displays(VirtViewerApp *self)
{
    g_hash_table_foreach(self->priv->windows, window_update_menu_displays_cb, self);
}

void
virt_viewer_app_set_connect_info(VirtViewerApp *self,
                                 const gchar *host,
                                 const gchar *ghost,
                                 const gchar *gport,
                                 const gchar *gtlsport,
                                 const gchar *transport,
                                 const gchar *unixsock,
                                 const gchar *user,
                                 gint port,
                                 const gchar *guri)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(self));
    VirtViewerAppPrivate *priv = self->priv;

    DEBUG_LOG("Set connect info: %s,%s,%s,%s,%s,%s,%s,%d",
              host, ghost, gport, gtlsport ? gtlsport : "-1", transport, unixsock, user, port);

    g_free(priv->host);
    g_free(priv->ghost);
    g_free(priv->gport);
    g_free(priv->gtlsport);
    g_free(priv->transport);
    g_free(priv->unixsock);
    g_free(priv->user);
    g_free(priv->guri);

    priv->host = g_strdup(host);
    priv->ghost = g_strdup(ghost);
    priv->gport = g_strdup(gport);
    priv->gtlsport = gtlsport ? g_strdup(gtlsport) : NULL;
    priv->transport = g_strdup(transport);
    priv->unixsock = g_strdup(unixsock);
    priv->user = g_strdup(user);
    priv->guri = g_strdup(guri);
    priv->port = port;

    virt_viewer_app_update_pretty_address(self);
}

void
virt_viewer_app_free_connect_info(VirtViewerApp *self)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(self));

    virt_viewer_app_set_connect_info(self, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL);
}

VirtViewerWindow*
virt_viewer_app_get_main_window(VirtViewerApp *self)
{
    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), NULL);

    return self->priv->main_window;
}

static void
show_status_cb(gpointer key G_GNUC_UNUSED,
               gpointer value,
               gpointer user_data)
{
    VirtViewerNotebook *nb = virt_viewer_window_get_notebook(VIRT_VIEWER_WINDOW(value));
    gchar *text = (gchar*)user_data;

    virt_viewer_notebook_show_status(nb, text);
}

void
virt_viewer_app_show_status(VirtViewerApp *self, const gchar *fmt, ...)
{
    va_list args;
    gchar *text;

    g_return_if_fail(VIRT_VIEWER_IS_APP(self));
    g_return_if_fail(fmt != NULL);

    va_start(args, fmt);
    text = g_strdup_vprintf(fmt, args);
    va_end(args);

    g_hash_table_foreach(self->priv->windows, show_status_cb, text);
    g_free(text);
}

static void
show_display_cb(gpointer key G_GNUC_UNUSED,
                gpointer value,
                gpointer user_data G_GNUC_UNUSED)
{
    VirtViewerNotebook *nb = virt_viewer_window_get_notebook(VIRT_VIEWER_WINDOW(value));

    virt_viewer_notebook_show_display(nb);
}

void
virt_viewer_app_show_display(VirtViewerApp *self)
{
    g_return_if_fail(VIRT_VIEWER_IS_APP(self));
    g_hash_table_foreach(self->priv->windows, show_display_cb, self);
}

gboolean
virt_viewer_app_get_enable_accel(VirtViewerApp *self)
{
    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);

    return self->priv->enable_accel;
}

VirtViewerSession*
virt_viewer_app_get_session(VirtViewerApp *self)
{
    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);

    return self->priv->session;
}

GHashTable*
virt_viewer_app_get_windows(VirtViewerApp *self)
{
    g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), NULL);
    return self->priv->windows;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
