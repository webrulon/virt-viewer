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
 * Author: Marc-André Lureau <marcandre.lureau@redhat.com>
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#ifdef HAVE_SPICE_GTK
#include <spice-controller/spice-controller.h>
#endif

#ifdef HAVE_SPICE_GTK
#include "virt-viewer-session-spice.h"
#endif
#include "virt-viewer-app.h"
#include "remote-viewer.h"

struct _RemoteViewerPrivate {
#ifdef HAVE_SPICE_GTK
    SpiceCtrlController *controller;
#endif
    GtkWidget *controller_menu;
};

G_DEFINE_TYPE (RemoteViewer, remote_viewer, VIRT_VIEWER_TYPE_APP)
#define GET_PRIVATE(o)                                                        \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), REMOTE_VIEWER_TYPE, RemoteViewerPrivate))

#if HAVE_SPICE_GTK
enum {
    PROP_0,
    PROP_CONTROLLER,
};
#endif

static gboolean remote_viewer_start(VirtViewerApp *self);
#if HAVE_SPICE_GTK
static int remote_viewer_activate(VirtViewerApp *self);
#endif
static void remote_viewer_window_added(VirtViewerApp *self, VirtViewerWindow *win);

#if HAVE_SPICE_GTK
static void
remote_viewer_get_property (GObject *object, guint property_id,
                            GValue *value, GParamSpec *pspec)
{
    RemoteViewer *self = REMOTE_VIEWER(object);
    RemoteViewerPrivate *priv = self->priv;

    switch (property_id) {
    case PROP_CONTROLLER:
        g_value_set_object(value, priv->controller);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
remote_viewer_set_property (GObject *object, guint property_id,
                            const GValue *value, GParamSpec *pspec)
{
    RemoteViewer *self = REMOTE_VIEWER(object);
    RemoteViewerPrivate *priv = self->priv;

    switch (property_id) {
    case PROP_CONTROLLER:
        g_return_if_fail(priv->controller == NULL);
        priv->controller = g_value_dup_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
remote_viewer_dispose (GObject *object)
{
    RemoteViewer *self = REMOTE_VIEWER(object);
    RemoteViewerPrivate *priv = self->priv;

    if (priv->controller) {
        g_object_unref(priv->controller);
        priv->controller = NULL;
    }

    G_OBJECT_CLASS(remote_viewer_parent_class)->dispose (object);
}
#endif


static void
remote_viewer_class_init (RemoteViewerClass *klass)
{
#if HAVE_SPICE_GTK
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
#endif
    VirtViewerAppClass *app_class = VIRT_VIEWER_APP_CLASS (klass);

    g_type_class_add_private (klass, sizeof (RemoteViewerPrivate));

#if HAVE_SPICE_GTK
    object_class->get_property = remote_viewer_get_property;
    object_class->set_property = remote_viewer_set_property;
    object_class->dispose = remote_viewer_dispose;
#endif

    app_class->start = remote_viewer_start;
#if HAVE_SPICE_GTK
    app_class->activate = remote_viewer_activate;
#endif
    app_class->window_added = remote_viewer_window_added;

#if HAVE_SPICE_GTK
    g_object_class_install_property(object_class,
                                    PROP_CONTROLLER,
                                    g_param_spec_object("controller",
                                                        "Controller",
                                                        "Spice controller",
                                                        SPICE_CTRL_TYPE_CONTROLLER,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
#endif
}

static void
remote_viewer_init(RemoteViewer *self)
{
    self->priv = GET_PRIVATE(self);
}

RemoteViewer *
remote_viewer_new(const gchar *uri, gboolean verbose)
{
    return g_object_new(REMOTE_VIEWER_TYPE,
                        "guri", uri,
                        "verbose", verbose,
                        NULL);
}

#if HAVE_SPICE_GTK
RemoteViewer *
remote_viewer_new_with_controller(gboolean verbose)
{
    RemoteViewer *self;
    SpiceCtrlController *ctrl = spice_ctrl_controller_new();

    self =  g_object_new(REMOTE_VIEWER_TYPE,
                         "controller", ctrl,
                         "verbose", verbose,
                         NULL);
    g_object_unref(ctrl);

    return self;
}

static void
spice_ctrl_do_connect(SpiceCtrlController *ctrl G_GNUC_UNUSED,
                      VirtViewerApp *self)
{
    if (virt_viewer_app_initial_connect(self) < 0) {
        virt_viewer_app_simple_message_dialog(self, _("Failed to initiate connection"));
    }
}

static void
spice_ctrl_show(SpiceCtrlController *ctrl G_GNUC_UNUSED, RemoteViewer *self)
{
    virt_viewer_app_show_display(VIRT_VIEWER_APP(self));
}

static void
spice_ctrl_hide(SpiceCtrlController *ctrl G_GNUC_UNUSED, RemoteViewer *self)
{
    virt_viewer_app_show_status(VIRT_VIEWER_APP(self), _("Display disabled by controller"));
}

static void
spice_menuitem_activate_cb(GtkMenuItem *mi, RemoteViewer *self)
{
    SpiceCtrlMenuItem *menuitem = g_object_get_data(G_OBJECT(mi), "spice-menuitem");

    g_return_if_fail(menuitem != NULL);
    if (gtk_menu_item_get_submenu(mi))
        return;

    spice_ctrl_controller_menu_item_click_msg(self->priv->controller, menuitem->id);
}

static GtkWidget *
ctrlmenu_to_gtkmenu (RemoteViewer *self, SpiceCtrlMenu *ctrlmenu)
{
    GList *l;
    GtkWidget *menu = gtk_menu_new();
    guint n = 0;

    for (l = ctrlmenu->items; l != NULL; l = l->next) {
        SpiceCtrlMenuItem *menuitem = l->data;
        GtkWidget *item;
        char *s;
        if (menuitem->text == NULL) {
            g_warn_if_reached();
            continue;
        }

        for (s = menuitem->text; *s; s++)
            if (*s == '&')
                *s = '_';

        if (g_str_equal(menuitem->text, "-")){
            item = gtk_separator_menu_item_new();
        } else {
            item = gtk_menu_item_new_with_mnemonic(menuitem->text);
        }

        g_object_set_data_full(G_OBJECT(item), "spice-menuitem",
                               g_object_ref(menuitem), g_object_unref);
        g_signal_connect(item, "activate", G_CALLBACK(spice_menuitem_activate_cb), self);
        gtk_menu_attach(GTK_MENU (menu), item, 0, 1, n, n + 1);
        n += 1;

        if (menuitem->submenu) {
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
                                      ctrlmenu_to_gtkmenu(self, menuitem->submenu));
        }
    }

    if (n == 0) {
        g_object_ref_sink(menu);
        g_object_unref(menu);
        menu = NULL;
    }

    gtk_widget_show_all(menu);
    return menu;
}

static void
spice_menu_set_visible(gpointer key G_GNUC_UNUSED,
                       gpointer value,
                       gpointer user_data)
{
    gboolean visible = GPOINTER_TO_INT(user_data);
    GtkWidget *menu = g_object_get_data(value, "spice-menu");

    gtk_widget_set_visible(menu, visible);
}

static void
remote_viewer_window_spice_menu_set_visible(RemoteViewer *self,
                                            gboolean visible)
{
    GHashTable *windows = virt_viewer_app_get_windows(VIRT_VIEWER_APP(self));

    g_hash_table_foreach(windows, spice_menu_set_visible, GINT_TO_POINTER(visible));
}

static void
spice_menu_update(gpointer key G_GNUC_UNUSED,
                  gpointer value,
                  gpointer user_data)
{
    RemoteViewer *self = REMOTE_VIEWER(user_data);
    GtkWidget *menuitem = g_object_get_data(value, "spice-menu");
    SpiceCtrlMenu *menu;

    g_object_get(self->priv->controller, "menu", &menu, NULL);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), ctrlmenu_to_gtkmenu(self, menu));
    g_object_unref(menu);
}

static void
spice_ctrl_menu_updated(RemoteViewer *self,
                        SpiceCtrlMenu *menu)
{
    GHashTable *windows = virt_viewer_app_get_windows(VIRT_VIEWER_APP(self));
    RemoteViewerPrivate *priv = self->priv;
    gboolean visible;

    DEBUG_LOG("Spice controller menu updated");

    if (priv->controller_menu != NULL) {
        g_object_unref (priv->controller_menu);
        priv->controller_menu = NULL;
    }

    if (menu && g_list_length(menu->items) > 0) {
        priv->controller_menu = ctrlmenu_to_gtkmenu(self, menu);
        g_hash_table_foreach(windows, spice_menu_update, self);
    }

    visible = priv->controller_menu != NULL;

    remote_viewer_window_spice_menu_set_visible(self, visible);
}

static SpiceSession *
remote_viewer_get_spice_session(RemoteViewer *self)
{
    VirtViewerSession *vsession = NULL;
    SpiceSession *session = NULL;

    g_object_get(self, "session", &vsession, NULL);
    g_return_val_if_fail(vsession != NULL, NULL);

    g_object_get(vsession, "spice-session", &session, NULL);

    g_object_unref(vsession);

    return session;
}

#ifndef G_VALUE_INIT /* see bug https://bugzilla.gnome.org/show_bug.cgi?id=654793 */
#define G_VALUE_INIT  { 0, { { 0 } } }
#endif

static void
spice_ctrl_notified(SpiceCtrlController *ctrl,
                    GParamSpec *pspec,
                    RemoteViewer *self)
{
    SpiceSession *session = remote_viewer_get_spice_session(self);
    GValue value = G_VALUE_INIT;
    VirtViewerApp *app = VIRT_VIEWER_APP(self);

    g_return_if_fail(session != NULL);

    g_value_init(&value, pspec->value_type);
    g_object_get_property(G_OBJECT(ctrl), pspec->name, &value);

    if (g_str_equal(pspec->name, "host") ||
        g_str_equal(pspec->name, "port") ||
        g_str_equal(pspec->name, "password") ||
        g_str_equal(pspec->name, "ca-file")) {
        g_object_set_property(G_OBJECT(session), pspec->name, &value);
    } else if (g_str_equal(pspec->name, "sport")) {
        g_object_set_property(G_OBJECT(session), "tls-port", &value);
    } else if (g_str_equal(pspec->name, "tls-ciphers")) {
        g_object_set_property(G_OBJECT(session), "ciphers", &value);
    } else if (g_str_equal(pspec->name, "host-subject")) {
        g_object_set_property(G_OBJECT(session), "cert-subject", &value);
    } else if (g_str_equal(pspec->name, "title")) {
        g_object_set_property(G_OBJECT(app), "title", &value);
    } else if (g_str_equal(pspec->name, "display-flags")) {
        guint flags = g_value_get_uint(&value);
        gboolean fullscreen = flags & CONTROLLER_SET_FULL_SCREEN;
        gboolean auto_res = flags & CONTROLLER_AUTO_DISPLAY_RES;
        g_object_set(G_OBJECT(self), "fullscreen", fullscreen, NULL);
        g_debug("unimplemented resize-guest %d", auto_res);
        /* g_object_set(G_OBJECT(self), "resize-guest", auto_res, NULL); */
    } else if (g_str_equal(pspec->name, "menu")) {
        spice_ctrl_menu_updated(self, g_value_get_object(&value));
    } else {
        gchar *content = g_strdup_value_contents(&value);

        g_debug("unimplemented property: %s=%s", pspec->name, content);
        g_free(content);
    }

    g_object_unref(session);
    g_value_unset(&value);
}

static void
spice_ctrl_listen_async_cb(GObject *object,
                           GAsyncResult *res,
                           gpointer user_data)
{
    GError *error = NULL;

    spice_ctrl_controller_listen_finish(SPICE_CTRL_CONTROLLER(object), res, &error);

    if (error != NULL) {
        virt_viewer_app_simple_message_dialog(VIRT_VIEWER_APP(user_data),
                                              _("Controller connection failed: %s"),
                                              error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE); /* TODO: make start async? */
    }
}


static int
remote_viewer_activate(VirtViewerApp *app)
{
    g_return_val_if_fail(REMOTE_VIEWER_IS(app), -1);
    RemoteViewer *self = REMOTE_VIEWER(app);
    int ret = -1;

    if (self->priv->controller) {
        SpiceSession *session = remote_viewer_get_spice_session(self);
        ret = spice_session_connect(session);
        g_object_unref(session);
    } else {
        ret = VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->activate(app);
    }

    return ret;
}
#endif

static void
remote_viewer_window_added(VirtViewerApp *self G_GNUC_UNUSED,
                           VirtViewerWindow *win)
{
    GtkMenuShell *shell = GTK_MENU_SHELL(gtk_builder_get_object(virt_viewer_window_get_builder(win), "top-menu"));
    GtkWidget *spice = gtk_menu_item_new_with_label("Spice");

    gtk_menu_shell_append(shell, spice);
    g_object_set_data(G_OBJECT(win), "spice-menu", spice);
}

static gboolean
remote_viewer_start(VirtViewerApp *app)
{
    g_return_val_if_fail(REMOTE_VIEWER_IS(app), FALSE);

#if HAVE_SPICE_GTK
    RemoteViewer *self = REMOTE_VIEWER(app);
    RemoteViewerPrivate *priv = self->priv;
#endif
    gboolean ret = FALSE;
    gchar *guri = NULL;
    gchar *type = NULL;

#if HAVE_SPICE_GTK
    if (priv->controller) {
        if (virt_viewer_app_create_session(app, "spice") < 0) {
            virt_viewer_app_simple_message_dialog(app, _("Couldn't create a Spice session"));
            goto cleanup;
        }

        g_signal_connect(priv->controller, "notify", G_CALLBACK(spice_ctrl_notified), self);
        g_signal_connect(priv->controller, "do_connect", G_CALLBACK(spice_ctrl_do_connect), self);
        g_signal_connect(priv->controller, "show", G_CALLBACK(spice_ctrl_show), self);
        g_signal_connect(priv->controller, "hide", G_CALLBACK(spice_ctrl_hide), self);

        spice_ctrl_controller_listen(priv->controller, NULL, spice_ctrl_listen_async_cb, self);
        virt_viewer_app_show_status(VIRT_VIEWER_APP(self), _("Setting up Spice session..."));
    } else {
#endif
        g_object_get(app, "guri", &guri, NULL);
        g_return_val_if_fail(guri != NULL, FALSE);

        DEBUG_LOG("Opening display to %s", guri);
        g_object_set(app, "title", guri, NULL);

        if (virt_viewer_util_extract_host(guri, &type, NULL, NULL, NULL, NULL) < 0) {
            virt_viewer_app_simple_message_dialog(app, _("Cannot determine the connection type from URI"));
            goto cleanup;
        }

        if (virt_viewer_app_create_session(app, type) < 0) {
            virt_viewer_app_simple_message_dialog(app, _("Couldn't create a session for this type: %s"), type);
            goto cleanup;
        }

        if (virt_viewer_app_initial_connect(app) < 0) {
            virt_viewer_app_simple_message_dialog(app, _("Failed to initiate connection"));
            goto cleanup;
        }
#if HAVE_SPICE_GTK
    }
#endif

    ret = VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->start(app);

 cleanup:
    g_free(guri);
    g_free(type);
    return ret;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
