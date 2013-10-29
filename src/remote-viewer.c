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
#include <libxml/uri.h>

#ifdef HAVE_OVIRT
#include <govirt/govirt.h>
#endif

#ifdef HAVE_SPICE_GTK
#include <spice-controller.h>
#endif

#ifdef HAVE_SPICE_GTK
#include "virt-viewer-session-spice.h"
#endif
#include "virt-viewer-app.h"
#include "virt-viewer-auth.h"
#include "virt-viewer-file.h"
#include "virt-viewer-session.h"
#include "remote-viewer.h"

#ifndef G_VALUE_INIT /* see bug https://bugzilla.gnome.org/show_bug.cgi?id=654793 */
#define G_VALUE_INIT  { 0, { { 0 } } }
#endif

struct _RemoteViewerPrivate {
#ifdef HAVE_SPICE_GTK
    SpiceCtrlController *controller;
    SpiceCtrlForeignMenu *ctrl_foreign_menu;
#endif
    GtkWidget *controller_menu;
    GtkWidget *foreign_menu;
    gboolean open_recent_dialog;

    gboolean default_title; /* Whether the window title was set by the user, or
                               is the default one (URI we are connecting to) */

};

G_DEFINE_TYPE (RemoteViewer, remote_viewer, VIRT_VIEWER_TYPE_APP)
#define GET_PRIVATE(o)                                                        \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), REMOTE_VIEWER_TYPE, RemoteViewerPrivate))

enum {
    PROP_0,
#ifdef HAVE_SPICE_GTK
    PROP_CONTROLLER,
    PROP_CTRL_FOREIGN_MENU,
#endif
    PROP_OPEN_RECENT_DIALOG
};

static gboolean remote_viewer_start(VirtViewerApp *self);
#ifdef HAVE_SPICE_GTK
static gboolean remote_viewer_activate(VirtViewerApp *self, GError **error);
static void remote_viewer_window_added(VirtViewerApp *self, VirtViewerWindow *win);
static void spice_foreign_menu_updated(RemoteViewer *self);
static gint connect_dialog(gchar **uri);

static void
remote_viewer_dispose (GObject *object)
{
    RemoteViewer *self = REMOTE_VIEWER(object);
    RemoteViewerPrivate *priv = self->priv;

    if (priv->controller) {
        g_object_unref(priv->controller);
        priv->controller = NULL;
    }

    if (priv->ctrl_foreign_menu) {
        g_object_unref(priv->ctrl_foreign_menu);
        priv->ctrl_foreign_menu = NULL;
    }

    G_OBJECT_CLASS(remote_viewer_parent_class)->dispose (object);
}
#endif

static void
remote_viewer_get_property (GObject *object, guint property_id,
                            GValue *value, GParamSpec *pspec)
{
    RemoteViewer *self = REMOTE_VIEWER(object);
    RemoteViewerPrivate *priv = self->priv;

    switch (property_id) {
#ifdef HAVE_SPICE_GTK
    case PROP_CONTROLLER:
        g_value_set_object(value, priv->controller);
        break;
    case PROP_CTRL_FOREIGN_MENU:
        g_value_set_object(value, priv->ctrl_foreign_menu);
        break;
#endif
    case PROP_OPEN_RECENT_DIALOG:
        g_value_set_boolean(value, priv->open_recent_dialog);
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
#ifdef HAVE_SPICE_GTK
    case PROP_CONTROLLER:
        g_return_if_fail(priv->controller == NULL);
        priv->controller = g_value_dup_object(value);
        break;
    case PROP_CTRL_FOREIGN_MENU:
        g_return_if_fail(priv->ctrl_foreign_menu == NULL);
        priv->ctrl_foreign_menu = g_value_dup_object(value);
        break;
#endif
    case PROP_OPEN_RECENT_DIALOG:
        priv->open_recent_dialog = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
remote_viewer_deactivated(VirtViewerApp *app, gboolean connect_error)
{
    RemoteViewer *self = REMOTE_VIEWER(app);
    RemoteViewerPrivate *priv = self->priv;

    if (connect_error && priv->open_recent_dialog) {
        if (virt_viewer_app_start(app)) {
            return;
        }
    }

    VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->deactivated(app, connect_error);
}

static void
remote_viewer_class_init (RemoteViewerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    VirtViewerAppClass *app_class = VIRT_VIEWER_APP_CLASS (klass);

    g_type_class_add_private (klass, sizeof (RemoteViewerPrivate));

    object_class->get_property = remote_viewer_get_property;
    object_class->set_property = remote_viewer_set_property;

    app_class->start = remote_viewer_start;
    app_class->deactivated = remote_viewer_deactivated;
#ifdef HAVE_SPICE_GTK
    object_class->dispose = remote_viewer_dispose;
    app_class->activate = remote_viewer_activate;
    app_class->window_added = remote_viewer_window_added;
#endif

#ifdef HAVE_SPICE_GTK
    g_object_class_install_property(object_class,
                                    PROP_CONTROLLER,
                                    g_param_spec_object("controller",
                                                        "Controller",
                                                        "Spice controller",
                                                        SPICE_CTRL_TYPE_CONTROLLER,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_CTRL_FOREIGN_MENU,
                                    g_param_spec_object("foreign-menu",
                                                        "Foreign Menu",
                                                        "Spice foreign menu",
                                                        SPICE_CTRL_TYPE_FOREIGN_MENU,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
#endif
    g_object_class_install_property(object_class,
                                    PROP_OPEN_RECENT_DIALOG,
                                    g_param_spec_boolean("open-recent-dialog",
                                                         "Open recent dialog",
                                                         "Open recent dialog",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));
}

static void
remote_viewer_init(RemoteViewer *self)
{
    self->priv = GET_PRIVATE(self);
}

RemoteViewer *
remote_viewer_new(const gchar *uri, const gchar *title)
{
    return g_object_new(REMOTE_VIEWER_TYPE,
                        "guri", uri,
                        "title", title,
                        "open-recent-dialog", uri == NULL,
                        NULL);
}

#ifdef HAVE_SPICE_GTK
static void
foreign_menu_title_changed(SpiceCtrlForeignMenu *menu G_GNUC_UNUSED,
                           GParamSpec *pspec G_GNUC_UNUSED,
                           RemoteViewer *self)
{
    gboolean has_focus;

    g_object_get(G_OBJECT(self), "has-focus", &has_focus, NULL);
    /* FIXME: use a proper "new client connected" event
    ** a foreign menu client set the title when connecting,
    ** inform of focus state at that time.
    */
    spice_ctrl_foreign_menu_app_activated_msg(self->priv->ctrl_foreign_menu, has_focus);

    /* update menu title */
    spice_foreign_menu_updated(self);
}

RemoteViewer *
remote_viewer_new_with_controller(void)
{
    RemoteViewer *self;
    SpiceCtrlController *ctrl = spice_ctrl_controller_new();
    SpiceCtrlForeignMenu *menu = spice_ctrl_foreign_menu_new();

    self =  g_object_new(REMOTE_VIEWER_TYPE,
                         "controller", ctrl,
                         "foreign-menu", menu,
                         NULL);
    g_signal_connect(menu, "notify::title",
                     G_CALLBACK(foreign_menu_title_changed),
                     self);
    g_object_unref(ctrl);
    g_object_unref(menu);

    return self;
}

static void
spice_ctrl_do_connect(SpiceCtrlController *ctrl G_GNUC_UNUSED,
                      VirtViewerApp *self)
{
    GError *error = NULL;

    if (!virt_viewer_app_initial_connect(self, &error)) {
        const gchar *msg = error ? error->message :
            _("Failed to initiate connection");
        virt_viewer_app_simple_message_dialog(self, msg);
        g_clear_error(&error);
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
spice_menuitem_activate_cb(GtkMenuItem *mi, GObject *ctrl)
{
    SpiceCtrlMenuItem *menuitem = g_object_get_data(G_OBJECT(mi), "spice-menuitem");

    g_return_if_fail(menuitem != NULL);
    if (gtk_menu_item_get_submenu(mi))
        return;

    if (SPICE_CTRL_IS_CONTROLLER(ctrl))
        spice_ctrl_controller_menu_item_click_msg(SPICE_CTRL_CONTROLLER(ctrl), menuitem->id);
    else if (SPICE_CTRL_IS_FOREIGN_MENU(ctrl))
        spice_ctrl_foreign_menu_menu_item_click_msg(SPICE_CTRL_FOREIGN_MENU(ctrl), menuitem->id);
}

static GtkWidget *
ctrlmenu_to_gtkmenu (RemoteViewer *self, SpiceCtrlMenu *ctrlmenu, GObject *ctrl)
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

        if (g_str_equal(menuitem->text, "-")) {
            item = gtk_separator_menu_item_new();
        } else if (menuitem->flags & CONTROLLER_MENU_FLAGS_CHECKED) {
            item = gtk_check_menu_item_new_with_mnemonic(menuitem->text);
            g_object_set(item, "active", TRUE, NULL);
        } else {
            item = gtk_menu_item_new_with_mnemonic(menuitem->text);
        }

        if (menuitem->flags & (CONTROLLER_MENU_FLAGS_GRAYED | CONTROLLER_MENU_FLAGS_DISABLED))
            gtk_widget_set_sensitive(item, FALSE);

        g_object_set_data_full(G_OBJECT(item), "spice-menuitem",
                               g_object_ref(menuitem), g_object_unref);
        g_signal_connect(item, "activate", G_CALLBACK(spice_menuitem_activate_cb), ctrl);
        gtk_menu_attach(GTK_MENU (menu), item, 0, 1, n, n + 1);
        n += 1;

        if (menuitem->submenu) {
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
                                      ctrlmenu_to_gtkmenu(self, menuitem->submenu, ctrl));
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
spice_menu_update(RemoteViewer *self, VirtViewerWindow *win)
{
    GtkWidget *menuitem = g_object_get_data(G_OBJECT(win), "spice-menu");
    SpiceCtrlMenu *menu;

    if (self->priv->controller == NULL)
        return;

    if (menuitem != NULL)
        gtk_widget_destroy(menuitem);

    {
        GtkMenuShell *shell = GTK_MENU_SHELL(gtk_builder_get_object(virt_viewer_window_get_builder(win), "top-menu"));
        menuitem = gtk_menu_item_new_with_label("Spice");
        gtk_menu_shell_append(shell, menuitem);
        g_object_set_data(G_OBJECT(win), "spice-menu", menuitem);
    }

    g_object_get(self->priv->controller, "menu", &menu, NULL);
    if (menu == NULL || g_list_length(menu->items) == 0) {
        gtk_widget_set_visible(menuitem, FALSE);
    } else {
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem),
            ctrlmenu_to_gtkmenu(self, menu, G_OBJECT(self->priv->controller)));
        gtk_widget_set_visible(menuitem, TRUE);
    }

    if (menu != NULL)
        g_object_unref(menu);
}

static void
spice_menu_update_each(gpointer key G_GNUC_UNUSED,
                       gpointer value,
                       gpointer user_data)
{
    spice_menu_update(REMOTE_VIEWER(user_data), VIRT_VIEWER_WINDOW(value));
}

static void
spice_ctrl_menu_updated(RemoteViewer *self)
{
    GHashTable *windows = virt_viewer_app_get_windows(VIRT_VIEWER_APP(self));

    DEBUG_LOG("Spice controller menu updated");

    g_hash_table_foreach(windows, spice_menu_update_each, self);
}

static void
foreign_menu_update(RemoteViewer *self, VirtViewerWindow *win)
{
    GtkWidget *menuitem = g_object_get_data(G_OBJECT(win), "foreign-menu");
    SpiceCtrlMenu *menu;

    if (self->priv->ctrl_foreign_menu == NULL)
        return;

    if (menuitem != NULL)
        gtk_widget_destroy(menuitem);

    {
        GtkMenuShell *shell = GTK_MENU_SHELL(gtk_builder_get_object(virt_viewer_window_get_builder(win), "top-menu"));
        const gchar *title = spice_ctrl_foreign_menu_get_title(self->priv->ctrl_foreign_menu);
        menuitem = gtk_menu_item_new_with_label(title);
        gtk_menu_shell_append(shell, menuitem);
        g_object_set_data(G_OBJECT(win), "foreign-menu", menuitem);
    }

    g_object_get(self->priv->ctrl_foreign_menu, "menu", &menu, NULL);
    if (menu == NULL || g_list_length(menu->items) == 0) {
        gtk_widget_set_visible(menuitem, FALSE);
    } else {
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem),
            ctrlmenu_to_gtkmenu(self, menu, G_OBJECT(self->priv->ctrl_foreign_menu)));
        gtk_widget_set_visible(menuitem, TRUE);
    }
    g_object_unref(menu);
}

static void
foreign_menu_update_each(gpointer key G_GNUC_UNUSED,
                         gpointer value,
                         gpointer user_data)
{
    foreign_menu_update(REMOTE_VIEWER(user_data), VIRT_VIEWER_WINDOW(value));
}

static void
spice_foreign_menu_updated(RemoteViewer *self)
{
    GHashTable *windows = virt_viewer_app_get_windows(VIRT_VIEWER_APP(self));

    DEBUG_LOG("Spice foreign menu updated");

    g_hash_table_foreach(windows, foreign_menu_update_each, self);
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

static void
app_notified(VirtViewerApp *app,
             GParamSpec *pspec,
             RemoteViewer *self)
{
    GValue value = G_VALUE_INIT;

    g_value_init(&value, pspec->value_type);
    g_object_get_property(G_OBJECT(app), pspec->name, &value);

    if (g_str_equal(pspec->name, "has-focus")) {
        if (self->priv->ctrl_foreign_menu)
            spice_ctrl_foreign_menu_app_activated_msg(self->priv->ctrl_foreign_menu, g_value_get_boolean(&value));
    }

    g_value_unset(&value);
}

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
        g_str_equal(pspec->name, "ca-file") ||
        g_str_equal(pspec->name, "enable-smartcard") ||
        g_str_equal(pspec->name, "color-depth") ||
        g_str_equal(pspec->name, "disable-effects") ||
        g_str_equal(pspec->name, "enable-usbredir") ||
        g_str_equal(pspec->name, "secure-channels") ||
        g_str_equal(pspec->name, "proxy")) {
        g_object_set_property(G_OBJECT(session), pspec->name, &value);
    } else if (g_str_equal(pspec->name, "sport")) {
        g_object_set_property(G_OBJECT(session), "tls-port", &value);
    } else if (g_str_equal(pspec->name, "tls-ciphers")) {
        g_object_set_property(G_OBJECT(session), "ciphers", &value);
    } else if (g_str_equal(pspec->name, "host-subject")) {
        g_object_set_property(G_OBJECT(session), "cert-subject", &value);
    } else if (g_str_equal(pspec->name, "enable-usb-autoshare")) {
        VirtViewerSession *vsession = NULL;

        g_object_get(self, "session", &vsession, NULL);
        g_object_set_property(G_OBJECT(vsession), "auto-usbredir", &value);
        g_object_unref(G_OBJECT(vsession));
    } else if (g_str_equal(pspec->name, "usb-filter")) {
        SpiceUsbDeviceManager *manager;
        manager = spice_usb_device_manager_get(session, NULL);
        if (manager != NULL) {
            g_object_set_property(G_OBJECT(manager),
                                  "auto-connect-filter",
                                  &value);
        }
    } else if (g_str_equal(pspec->name, "title")) {
        virt_viewer_app_set_title(app, g_value_get_string(&value));
    } else if (g_str_equal(pspec->name, "display-flags")) {
        guint flags = g_value_get_uint(&value);
        gboolean fullscreen = !!(flags & (CONTROLLER_SET_FULL_SCREEN | CONTROLLER_AUTO_DISPLAY_RES));
        g_object_set(G_OBJECT(self), "fullscreen", fullscreen, NULL);
    } else if (g_str_equal(pspec->name, "menu")) {
        spice_ctrl_menu_updated(self);
    } else if (g_str_equal(pspec->name, "hotkeys")) {
        virt_viewer_app_set_hotkeys(app, g_value_get_string(&value));
    } else {
        gchar *content = g_strdup_value_contents(&value);

        g_debug("unimplemented property: %s=%s", pspec->name, content);
        g_free(content);
    }

    g_object_unref(session);
    g_value_unset(&value);
}

static void
spice_ctrl_foreign_menu_notified(SpiceCtrlForeignMenu *ctrl_foreign_menu G_GNUC_UNUSED,
                                 GParamSpec *pspec,
                                 RemoteViewer *self)
{
    if (g_str_equal(pspec->name, "menu")) {
        spice_foreign_menu_updated(self);
    }
}

static void
spice_ctrl_listen_async_cb(GObject *object,
                           GAsyncResult *res,
                           gpointer user_data)
{
    GError *error = NULL;
    VirtViewerApp *app = VIRT_VIEWER_APP(user_data);

    if (SPICE_CTRL_IS_CONTROLLER(object))
        spice_ctrl_controller_listen_finish(SPICE_CTRL_CONTROLLER(object), res, &error);
    else if (SPICE_CTRL_IS_FOREIGN_MENU(object)) {
        spice_ctrl_foreign_menu_listen_finish(SPICE_CTRL_FOREIGN_MENU(object), res, &error);
    } else
        g_warn_if_reached();

    if (error != NULL) {
        virt_viewer_app_simple_message_dialog(app,
                                              _("Controller connection failed: %s"),
                                              error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE); /* TODO: make start async? */
    }
}


static gboolean
remote_viewer_activate(VirtViewerApp *app, GError **error)
{
    g_return_val_if_fail(REMOTE_VIEWER_IS(app), FALSE);
    RemoteViewer *self = REMOTE_VIEWER(app);
    gboolean ret = FALSE;

    if (self->priv->controller) {
        SpiceSession *session = remote_viewer_get_spice_session(self);
        ret = spice_session_connect(session);
        g_object_unref(session);
    } else {
        ret = VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->activate(app, error);
    }

    return ret;
}

static void
remote_viewer_window_added(VirtViewerApp *app G_GNUC_UNUSED,
                           VirtViewerWindow *win)
{
    spice_menu_update(REMOTE_VIEWER(app), win);
    foreign_menu_update(REMOTE_VIEWER(app), win);
}
#endif

#ifdef HAVE_OVIRT
static gboolean
parse_ovirt_uri(const gchar *uri_str, char **rest_uri, char **name)
{
    char *vm_name = NULL;
    char *rel_path;
    xmlURIPtr uri;
    gchar **path_elements;
    guint element_count;

    g_return_val_if_fail(uri_str != NULL, FALSE);
    g_return_val_if_fail(rest_uri != NULL, FALSE);
    g_return_val_if_fail(name != NULL, FALSE);

    uri = xmlParseURI(uri_str);
    if (uri == NULL)
        return FALSE;

    if (g_strcmp0(uri->scheme, "ovirt") != 0) {
        xmlFreeURI(uri);
        return FALSE;
    }

    if (uri->path == NULL) {
        xmlFreeURI(uri);
        return FALSE;
    }

    /* extract VM name from path */
    path_elements = g_strsplit(uri->path, "/", -1);

    element_count = g_strv_length(path_elements);
    if (element_count == 0) {
        g_strfreev(path_elements);
        xmlFreeURI(uri);
        return FALSE;
    }
    vm_name = path_elements[element_count-1];
    path_elements[element_count-1] = NULL;

    /* build final URI */
    rel_path = g_strjoinv("/", path_elements);
    /* FIXME: how to decide between http and https? */
    *rest_uri = g_strdup_printf("https://%s/%s/api/", uri->server, rel_path);
    *name = vm_name;
    g_free(rel_path);
    g_strfreev(path_elements);
    xmlFreeURI(uri);

    g_debug("oVirt base URI: %s", *rest_uri);
    g_debug("oVirt VM name: %s", *name);

    return TRUE;
}

static gboolean
authenticate_cb(RestProxy *proxy, G_GNUC_UNUSED RestProxyAuth *auth,
                G_GNUC_UNUSED gboolean retrying, gpointer user_data)
{
    gchar *username;
    gchar *password;
    VirtViewerWindow *window;

    window = virt_viewer_app_get_main_window(VIRT_VIEWER_APP(user_data));
    int ret = virt_viewer_auth_collect_credentials(virt_viewer_window_get_window(window),
                                                   "oVirt",
                                                   NULL,
                                                   &username, &password);
    if (ret < 0) {
        return FALSE;
    } else {
        g_object_set(G_OBJECT(proxy),
                     "username", username,
                     "password", password,
                     NULL);
        g_free(username);
        g_free(password);
        return TRUE;
    }
}


static gboolean
create_ovirt_session(VirtViewerApp *app, const char *uri)
{
    OvirtProxy *proxy = NULL;
    OvirtApi *api = NULL;
    OvirtCollection *vms;
    OvirtVm *vm = NULL;
    OvirtVmDisplay *display = NULL;
    OvirtVmState state;
    GError *error = NULL;
    char *rest_uri = NULL;
    char *vm_name = NULL;
    gboolean success = FALSE;
    guint port;
    guint secure_port;
    OvirtVmDisplayType type;
    const char *session_type;

    gchar *gport = NULL;
    gchar *gtlsport = NULL;
    gchar *ghost = NULL;
    gchar *ticket = NULL;
    gchar *host_subject = NULL;

    g_return_val_if_fail(VIRT_VIEWER_IS_APP(app), FALSE);

    if (!parse_ovirt_uri(uri, &rest_uri, &vm_name))
        goto error;
    proxy = ovirt_proxy_new(rest_uri);
    if (proxy == NULL)
        goto error;
    ovirt_set_proxy_options(proxy);
    g_signal_connect(G_OBJECT(proxy), "authenticate",
                     G_CALLBACK(authenticate_cb), app);

    api = ovirt_proxy_fetch_api(proxy, &error);
    if (error != NULL) {
        g_debug("failed to get oVirt 'api' collection: %s", error->message);
        goto error;
    }
    vms = ovirt_api_get_vms(api);
    ovirt_collection_fetch(vms, proxy, &error);
    if (error != NULL) {
        g_debug("failed to lookup %s: %s", vm_name, error->message);
        goto error;
    }
    vm = OVIRT_VM(ovirt_collection_lookup_resource(vms, vm_name));
    g_return_val_if_fail(vm != NULL, FALSE);
    g_object_get(G_OBJECT(vm), "state", &state, NULL);
    if (state != OVIRT_VM_STATE_UP) {
        g_debug("oVirt VM %s is not running", vm_name);
        goto error;
    }

    if (!ovirt_vm_get_ticket(vm, proxy, &error)) {
        g_debug("failed to get ticket for %s: %s", vm_name, error->message);
        goto error;
    }

    g_object_get(G_OBJECT(vm), "display", &display, NULL);
    if (display == NULL) {
        goto error;
    }

    g_object_get(G_OBJECT(display),
                 "type", &type,
                 "address", &ghost,
                 "port", &port,
                 "secure-port", &secure_port,
                 "ticket", &ticket,
                 "host-subject", &host_subject,
                 NULL);
    gport = g_strdup_printf("%d", port);
    gtlsport = g_strdup_printf("%d", secure_port);

    if (type == OVIRT_VM_DISPLAY_SPICE) {
        session_type = "spice";
    } else if (type == OVIRT_VM_DISPLAY_VNC) {
        session_type = "vnc";
    } else {
        g_debug("Unknown display type: %d", type);
        goto error;
    }

    virt_viewer_app_set_connect_info(app, NULL, ghost, gport, gtlsport,
                                     session_type, NULL, NULL, 0, NULL);

    if (virt_viewer_app_create_session(app, session_type) < 0)
        goto error;

#ifdef HAVE_SPICE_GTK
    if (type == OVIRT_VM_DISPLAY_SPICE) {
        SpiceSession *session;
        GByteArray *ca_cert;

        session = remote_viewer_get_spice_session(REMOTE_VIEWER(app));
        g_object_set(G_OBJECT(session),
                     "password", ticket,
                     "cert-subject", host_subject,
                     NULL);
        g_object_get(G_OBJECT(proxy), "ca-cert", &ca_cert, NULL);
        if (ca_cert != NULL) {
            g_object_set(G_OBJECT(session),
                    "ca", ca_cert,
                    NULL);
            g_byte_array_unref(ca_cert);
        }
    }
#endif

    success = TRUE;

error:
    g_free(rest_uri);
    g_free(vm_name);
    g_free(ticket);
    g_free(gport);
    g_free(gtlsport);
    g_free(ghost);
    g_free(host_subject);

    if (error != NULL)
        g_error_free(error);
    if (display != NULL)
        g_object_unref(display);
    if (vm != NULL)
        g_object_unref(vm);
    if (api != NULL)
        g_object_unref(api);
    if (proxy != NULL)
        g_object_unref(proxy);

    return success;
}

#endif

static void
recent_selection_changed_dialog_cb(GtkRecentChooser *chooser, gpointer data)
{
    GtkRecentInfo *info;
    GtkWidget *entry = data;
    const gchar *uri;

    info = gtk_recent_chooser_get_current_item(chooser);
    if (info == NULL)
        return;

    uri = gtk_recent_info_get_uri(info);
    g_return_if_fail(uri != NULL);

    gtk_entry_set_text(GTK_ENTRY(entry), uri);

    gtk_recent_info_unref(info);
}

static void
recent_item_activated_dialog_cb(GtkRecentChooser *chooser G_GNUC_UNUSED, gpointer data)
{
   gtk_dialog_response(GTK_DIALOG (data), GTK_RESPONSE_ACCEPT);
}

static gint
connect_dialog(gchar **uri)
{
    GtkWidget *dialog, *area, *label, *entry, *recent;
    GtkRecentFilter *rfilter;
    GtkTable *table;
    gint retval;

    /* Create the widgets */
    dialog = gtk_dialog_new_with_buttons(_("Connection details"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_REJECT,
                                         GTK_STOCK_CONNECT,
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    table = GTK_TABLE(gtk_table_new(1, 2, 0));
    gtk_box_pack_start(GTK_BOX(area), GTK_WIDGET(table), TRUE, TRUE, 0);
    gtk_table_set_row_spacings(table, 5);
    gtk_table_set_col_spacings(table, 5);

    label = gtk_label_new(_("URL:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach_defaults(table, label, 0, 1, 0, 1);
    entry = GTK_WIDGET(gtk_entry_new());
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    g_object_set(entry, "width-request", 200, NULL);
    gtk_table_attach_defaults(table, entry, 1, 2, 0, 1);

    label = gtk_label_new(_("Recent connections:"));
    gtk_box_pack_start(GTK_BOX(area), label, TRUE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    recent = GTK_WIDGET(gtk_recent_chooser_widget_new());
    gtk_recent_chooser_set_show_icons(GTK_RECENT_CHOOSER(recent), FALSE);
    gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER(recent), GTK_RECENT_SORT_MRU);
    gtk_box_pack_start(GTK_BOX(area), recent, TRUE, TRUE, 0);

    rfilter = gtk_recent_filter_new();
    gtk_recent_filter_add_mime_type(rfilter, "application/x-spice");
    gtk_recent_filter_add_mime_type(rfilter, "application/x-vnc");
    gtk_recent_filter_add_mime_type(rfilter, "application/x-virt-viewer");
    gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER(recent), rfilter);
    gtk_recent_chooser_set_local_only(GTK_RECENT_CHOOSER(recent), FALSE);
    g_signal_connect(recent, "selection-changed",
                     G_CALLBACK(recent_selection_changed_dialog_cb), entry);
    g_signal_connect(recent, "item-activated",
                     G_CALLBACK(recent_item_activated_dialog_cb), dialog);

    /* show and wait for response */
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        *uri = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
        g_strstrip(*uri);
        retval = 0;
    } else {
        *uri = NULL;
        retval = -1;
    }
    gtk_widget_destroy(dialog);

    return retval;
}

static gboolean
remote_viewer_start(VirtViewerApp *app)
{
    g_return_val_if_fail(REMOTE_VIEWER_IS(app), FALSE);

    RemoteViewer *self = REMOTE_VIEWER(app);
    RemoteViewerPrivate *priv = self->priv;
    GFile *file = NULL;
    VirtViewerFile *vvfile = NULL;
    gboolean ret = FALSE;
    gchar *guri = NULL;
    gchar *type = NULL;
    GError *error = NULL;

#ifdef HAVE_SPICE_GTK
    g_signal_connect(app, "notify", G_CALLBACK(app_notified), self);

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

        g_signal_connect(priv->ctrl_foreign_menu, "notify", G_CALLBACK(spice_ctrl_foreign_menu_notified), self);
        spice_ctrl_foreign_menu_listen(priv->ctrl_foreign_menu, NULL, spice_ctrl_listen_async_cb, self);

        virt_viewer_app_show_status(VIRT_VIEWER_APP(self), _("Setting up Spice session..."));
    } else {
#endif
retry_dialog:
        if (priv->open_recent_dialog) {
            if (connect_dialog(&guri) != 0)
                return FALSE;
            g_object_set(app, "guri", guri, NULL);
        } else
            g_object_get(app, "guri", &guri, NULL);

        g_return_val_if_fail(guri != NULL, FALSE);

        DEBUG_LOG("Opening display to %s", guri);
        if ((virt_viewer_app_get_title(app) == NULL) || priv->default_title) {
            priv->default_title = TRUE;
            virt_viewer_app_set_title(app, guri);
        }

        file = g_file_new_for_commandline_arg(guri);
        if (g_file_query_exists(file, NULL)) {
            gchar *path = g_file_get_path(file);
            vvfile = virt_viewer_file_new(path, &error);
            g_free(path);
            if (error) {
                virt_viewer_app_simple_message_dialog(app, _("Invalid file %s"), guri);
                g_warning("%s", error->message);
                g_clear_error(&error);
                goto cleanup;
            }
            g_object_get(G_OBJECT(vvfile), "type", &type, NULL);
        } else if (virt_viewer_util_extract_host(guri, &type, NULL, NULL, NULL, NULL) < 0 || type == NULL) {
            virt_viewer_app_simple_message_dialog(app, _("Cannot determine the connection type from URI"));
            goto cleanup;
        }
#ifdef HAVE_OVIRT
        if (g_strcmp0(type, "ovirt") == 0) {
            if (!create_ovirt_session(app, guri)) {
                virt_viewer_app_simple_message_dialog(app, _("Couldn't open oVirt session"));
                goto cleanup;
            }
        } else
#endif
        {
            if (virt_viewer_app_create_session(app, type) < 0) {
                virt_viewer_app_simple_message_dialog(app, _("Couldn't create a session for this type: %s"), type);
                goto cleanup;
            }
        }

        virt_viewer_session_set_file(virt_viewer_app_get_session(app), vvfile);

        if (!virt_viewer_app_initial_connect(app, &error)) {
            const gchar *msg = error ? error->message :
                _("Failed to initiate connection");

            virt_viewer_app_simple_message_dialog(app, msg);
            g_clear_error(&error);
            goto cleanup;
        }
#ifdef HAVE_SPICE_GTK
    }
#endif

    ret = VIRT_VIEWER_APP_CLASS(remote_viewer_parent_class)->start(app);

cleanup:
    g_clear_object(&file);
    g_clear_object(&vvfile);
    g_free(guri);
    g_free(type);

    if (!ret && priv->open_recent_dialog) {
        goto retry_dialog;
    }

    return ret;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
