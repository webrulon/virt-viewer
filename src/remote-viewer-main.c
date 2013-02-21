/*
 * Remote Viewer: A spice/vnc client based on virt-viewer
 *
 * Copyright (C) 2011-2012 Red Hat, Inc.
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
#include <locale.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#ifdef G_OS_WIN32
#include <windows.h>
#include <io.h>
#endif

#ifdef HAVE_GTK_VNC
#include <vncdisplay.h>
#endif
#ifdef HAVE_SPICE_GTK
#include <spice-option.h>
#endif

#include "remote-viewer.h"
#include "virt-viewer-app.h"
#include "virt-viewer-session.h"

static void
remote_viewer_version(void)
{
    g_print(_("remote-viewer version %s\n"), VERSION BUILDID);
    exit(EXIT_SUCCESS);
}

gboolean fullscreen = FALSE;
gboolean fullscreen_auto_conf = FALSE;

static gboolean
option_fullscreen(G_GNUC_UNUSED const gchar *option_name,
                  const gchar *value,
                  G_GNUC_UNUSED gpointer data, GError **error)
{
    fullscreen = TRUE;

    if (value == NULL)
        return TRUE;

    if (g_str_equal(value, "auto-conf")) {
        fullscreen_auto_conf = TRUE;
        return TRUE;
    }

    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED, _("Invalid full-screen argument: %s"), value);
    return FALSE;
}

static void recent_selection_changed_dialog_cb(GtkRecentChooser *chooser, gpointer data)
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

static void recent_item_activated_dialog_cb(GtkRecentChooser *chooser G_GNUC_UNUSED, gpointer data)
{
   gtk_dialog_response (GTK_DIALOG (data), GTK_RESPONSE_ACCEPT);
}

static gint connect_dialog(gchar **uri)
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
        retval = 0;
    } else {
        *uri = NULL;
        retval = -1;
    }
    gtk_widget_destroy(dialog);

    return retval;
}

static void
recent_add(gchar *uri, const gchar *mime_type)
{
    GtkRecentManager *recent;
    GtkRecentData meta = {
        .app_name     = (char*)"remote-viewer",
        .app_exec     = (char*)"remote-viewer %u",
        .mime_type    = (char*)mime_type,
    };

    if (uri == NULL)
        return;

    recent = gtk_recent_manager_get_default();
    meta.display_name = uri;
    if (!gtk_recent_manager_add_full(recent, uri, &meta))
        g_warning("Recent item couldn't be added");
}

static void connected(VirtViewerSession *session,
                      VirtViewerApp *self G_GNUC_UNUSED)
{
    gchar *uri = virt_viewer_session_get_uri(session);
    const gchar *mime = virt_viewer_session_mime_type(session);

    recent_add(uri, mime);
    g_free(uri);
}

int
main(int argc, char **argv)
{
    GOptionContext *context;
    GError *error = NULL;
    int ret = 1;
    int zoom = 100;
    gchar **args = NULL;
    gchar *uri = NULL;
    char *title = NULL;
    char *hotkeys = NULL;
    gboolean verbose = FALSE;
    gboolean debug = FALSE;
    gboolean direct = FALSE;
    RemoteViewer *viewer = NULL;
#ifdef HAVE_SPICE_GTK
    gboolean controller = FALSE;
#endif
    VirtViewerApp *app;
    const GOptionEntry options [] = {
        { "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          remote_viewer_version, N_("Display version information"), NULL },
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
          N_("Display verbose information"), NULL },
        { "title", 't', 0, G_OPTION_ARG_STRING, &title,
          N_("Set window title"), NULL },
        { "direct", 'd', 0, G_OPTION_ARG_NONE, &direct,
          N_("Direct connection with no automatic tunnels"), NULL },
        { "zoom", 'z', 0, G_OPTION_ARG_INT, &zoom,
          N_("Zoom level of window, in percentage"), "ZOOM" },
        { "debug", '\0', 0, G_OPTION_ARG_NONE, &debug,
          N_("Display debugging information"), NULL },
        { "full-screen", 'f', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, option_fullscreen,
          N_("Open in full screen mode (auto-conf adjusts guest resolution to fit the client's)."), N_("<auto-conf>") },
#ifdef HAVE_SPICE_GTK
        { "spice-controller", '\0', 0, G_OPTION_ARG_NONE, &controller,
          N_("Open connection using Spice controller communication"), NULL },
#endif
        { "hotkeys", 'h', 0, G_OPTION_ARG_STRING, &hotkeys,
          N_("Customise hotkeys"), NULL },
        { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &args,
          NULL, "URI" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };

    virt_viewer_util_init(_("Remote Viewer"));

    /* Setup command line options */
    context = g_option_context_new (_("- Remote viewer client"));
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
#ifdef HAVE_GTK_VNC
    g_option_context_add_group (context, vnc_display_get_option_group ());
#endif
#ifdef HAVE_SPICE_GTK
    g_option_context_add_group (context, spice_get_option_group ());
#endif
    g_option_context_parse (context, &argc, &argv, &error);
    if (error) {
        char *basename;
        basename = g_path_get_basename(argv[0]);
        g_printerr(_("%s\nRun '%s --help' to see a full list of available command line options\n"),
                   error->message, basename);
        g_free(basename);
        g_error_free(error);
        goto cleanup;
    }

    g_option_context_free(context);

#ifdef HAVE_SPICE_GTK
    if (controller) {
        if (args) {
            g_printerr(_("Error: extra arguments given while using Spice controller\n"));
            goto cleanup;
        }
    } else
#endif
    if (!args || g_strv_length(args) == 0) {
        if (connect_dialog(&uri) != 0)
            goto cleanup;
    } else if (g_strv_length(args) > 1) {
        g_printerr(_("Error: can't handle multiple URIs\n"));
        goto cleanup;
    } else {
        uri = g_strdup(args[0]);
    }

    if (zoom < 10 || zoom > 200) {
        g_printerr(_("Zoom level must be within 10-200\n"));
        goto cleanup;
    }

    gtk_window_set_default_icon_name("virt-viewer");

    virt_viewer_app_set_debug(debug);

#ifdef HAVE_SPICE_GTK
    if (controller) {
        viewer = remote_viewer_new_with_controller(verbose);
        g_object_set(viewer, "guest-name", "defined by Spice controller", NULL);
    } else {
#endif
        viewer = remote_viewer_new(uri, title, verbose);
        g_object_set(viewer, "guest-name", uri, NULL);
#ifdef HAVE_SPICE_GTK
    }
#endif
    if (viewer == NULL)
        goto cleanup;

    app = VIRT_VIEWER_APP(viewer);
    g_object_set(app,
                 "fullscreen", fullscreen,
                 "fullscreen-auto-conf", fullscreen_auto_conf,
                 NULL);
    virt_viewer_window_set_zoom_level(virt_viewer_app_get_main_window(app), zoom);
    virt_viewer_app_set_direct(app, direct);
    virt_viewer_app_set_hotkeys(app, hotkeys);

    if (!virt_viewer_app_start(app))
        goto cleanup;

    g_signal_connect(virt_viewer_app_get_session(app), "session-connected",
                     G_CALLBACK(connected), app);

    gtk_main();

    ret = 0;

 cleanup:
    g_free(uri);
    if (viewer)
        g_object_unref(viewer);
    g_strfreev(args);

    return ret;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
