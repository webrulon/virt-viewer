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

#ifdef HAVE_GTK_VNC
#include <vncdisplay.h>
#endif
#ifdef HAVE_SPICE_GTK
#include <spice-option.h>
#endif

#include "remote-viewer.h"
#include "virt-viewer-app.h"

static void
remote_viewer_version(void)
{
    g_print(_("remote-viewer version %s\n"), VERSION);
    exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
    GOptionContext *context;
    GError *error = NULL;
    int ret = 1;
    int zoom = 100;
    gchar **args = NULL;
    gboolean verbose = FALSE;
    gboolean debug = FALSE;
    gboolean direct = FALSE;
    gboolean fullscreen = FALSE;
    RemoteViewer *viewer = NULL;
#if HAVE_SPICE_GTK
    gboolean controller = FALSE;
#endif
    VirtViewerApp *app;
    const char *help_msg = N_("Run '" PACKAGE " --help' to see a full list of available command line options");
    const GOptionEntry options [] = {
        { "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          remote_viewer_version, N_("Display version information"), NULL },
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
          N_("Display verbose information"), NULL },
        { "direct", 'd', 0, G_OPTION_ARG_NONE, &direct,
          N_("Direct connection with no automatic tunnels"), NULL },
        { "zoom", 'z', 0, G_OPTION_ARG_INT, &zoom,
          N_("Zoom level of window, in percentage"), "ZOOM" },
        { "debug", '\0', 0, G_OPTION_ARG_NONE, &debug,
          N_("Display debugging information"), NULL },
        { "full-screen", 'f', 0, G_OPTION_ARG_NONE, &fullscreen,
          N_("Open in full screen mode"), NULL },
#if HAVE_SPICE_GTK
        { "spice-controller", '\0', 0, G_OPTION_ARG_NONE, &controller,
          N_("Open connection using Spice controller communication"), NULL },
#endif
        { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &args,
          NULL, "URI" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };

    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

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
        g_printerr("%s\n%s\n",
                   error->message,
                   gettext(help_msg));
        g_error_free(error);
        goto cleanup;
    }

    g_option_context_free(context);

    if ((!args || (g_strv_length(args) != 1))
#if HAVE_SPICE_GTK
        && !controller
#endif
        ) {
        g_printerr(_("\nUsage: %s [OPTIONS] URI\n\n%s\n\n"), argv[0], help_msg);
        goto cleanup;
    }

    if (zoom < 10 || zoom > 200) {
        g_printerr(_("Zoom level must be within 10-200\n"));
        goto cleanup;
    }

    gtk_window_set_default_icon_name("virt-viewer");

    virt_viewer_app_set_debug(debug);

#if HAVE_SPICE_GTK
    if (controller) {
        viewer = remote_viewer_new_with_controller(verbose);
        g_object_set(viewer, "guest-name", "defined by Spice controller", NULL);
    } else {
#endif
        viewer = remote_viewer_new(args[0], verbose);
        g_object_set(viewer, "guest-name", args[0], NULL);
#if HAVE_SPICE_GTK
    }
#endif
    if (viewer == NULL)
        goto cleanup;

    app = VIRT_VIEWER_APP(viewer);
    g_object_set(app, "fullscreen", fullscreen, NULL);
    virt_viewer_window_set_zoom_level(virt_viewer_app_get_main_window(app), zoom);
    virt_viewer_app_set_direct(app, direct);

    if (!virt_viewer_app_start(app))
        goto cleanup;

    gtk_main();

    ret = 0;

 cleanup:
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
