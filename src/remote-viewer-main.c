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
#ifdef HAVE_OVIRT
#include <govirt/ovirt-options.h>
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
    gchar **args = NULL;
    gchar *uri = NULL;
    char *title = NULL;
    RemoteViewer *viewer = NULL;
#ifdef HAVE_SPICE_GTK
    gboolean controller = FALSE;
#endif
    VirtViewerApp *app;
    const GOptionEntry options [] = {
        { "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          remote_viewer_version, N_("Display version information"), NULL },
        { "title", 't', 0, G_OPTION_ARG_STRING, &title,
          N_("Set window title"), NULL },
#ifdef HAVE_SPICE_GTK
        { "spice-controller", '\0', 0, G_OPTION_ARG_NONE, &controller,
          N_("Open connection using Spice controller communication"), NULL },
#endif
        { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &args,
          NULL, "-- URI" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };

    virt_viewer_util_init(_("Remote Viewer"));

    /* Setup command line options */
    context = g_option_context_new (_("- Remote viewer client"));
    g_option_context_add_main_entries (context, virt_viewer_app_get_options(), NULL);
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
#ifdef HAVE_GTK_VNC
    g_option_context_add_group (context, vnc_display_get_option_group ());
#endif
#ifdef HAVE_SPICE_GTK
    g_option_context_add_group (context, spice_get_option_group ());
#endif
#ifdef HAVE_OVIRT
    g_option_context_add_group (context, ovirt_get_option_group ());
#endif
    g_option_context_parse (context, &argc, &argv, &error);
    if (error) {
        char *base_name;
        base_name = g_path_get_basename(argv[0]);
        g_printerr(_("%s\nRun '%s --help' to see a full list of available command line options\n"),
                   error->message, base_name);
        g_free(base_name);
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
    if (args) {
        if (g_strv_length(args) > 1) {
            g_printerr(_("Error: can't handle multiple URIs\n"));
            goto cleanup;
        } else if (g_strv_length(args) == 1) {
            uri = g_strdup(args[0]);
        }
    }

#ifdef HAVE_SPICE_GTK
    if (controller) {
        viewer = remote_viewer_new_with_controller();
        g_object_set(viewer, "guest-name", "defined by Spice controller", NULL);
    } else {
#endif
        viewer = remote_viewer_new(uri, title);
        g_object_set(viewer, "guest-name", uri, NULL);
#ifdef HAVE_SPICE_GTK
    }
#endif
    if (viewer == NULL)
        goto cleanup;

    app = VIRT_VIEWER_APP(viewer);

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
