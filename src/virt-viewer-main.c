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

#include <config.h>
#include <locale.h>
#include <vncdisplay.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "virt-viewer.h"

static void virt_viewer_version(void)
{
	g_print(_("%s version %s\n"), PACKAGE, VERSION);

	exit(0);
}


int main(int argc, char **argv)
{
	GOptionContext *context;
	GError *error = NULL;
	int ret = 1;
	char *uri = NULL;
	int zoom = 100;
	gchar **args = NULL;
	gboolean verbose = FALSE;
	gboolean debug = FALSE;
	gboolean direct = FALSE;
	gboolean waitvm = FALSE;
	gboolean reconnect = FALSE;
	gboolean fullscreen = FALSE;
	const char *help_msg = N_("Run '" PACKAGE " --help' to see a full list of available command line options");
	const GOptionEntry options [] = {
		{ "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
		  virt_viewer_version, N_("display version information"), NULL },
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
		  N_("display verbose information"), NULL },
		{ "direct", 'd', 0, G_OPTION_ARG_NONE, &direct,
		  N_("direct connection with no automatic tunnels"), NULL },
		{ "connect", 'c', 0, G_OPTION_ARG_STRING, &uri,
		  N_("connect to hypervisor"), "URI"},
		{ "wait", 'w', 0, G_OPTION_ARG_NONE, &waitvm,
		  N_("wait for domain to start"), NULL },
		{ "reconnect", 'r', 0, G_OPTION_ARG_NONE, &reconnect,
		  N_("reconnect to domain upon restart"), NULL },
		{ "zoom", 'z', 0, G_OPTION_ARG_INT, &zoom,
		  N_("Zoom level of window, in percentage"), "ZOOM" },
		{ "debug", '\0', 0, G_OPTION_ARG_NONE, &debug,
		  N_("display debugging information"), NULL },
		{ "full-screen", 'f', 0, G_OPTION_ARG_NONE, &fullscreen,
		  N_("Open in full screen mode"), NULL },
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &args,
		  NULL, "DOMAIN-NAME|ID|UUID" },
		{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	/* Setup command line options */
	context = g_option_context_new (_("- Virtual machine graphical console"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_group (context, vnc_display_get_option_group ());
	g_option_context_parse (context, &argc, &argv, &error);
	if (error) {
		g_printerr("%s\n%s\n",
			   error->message,
			   gettext(help_msg));
		g_error_free(error);
		goto cleanup;
	}

	g_option_context_free(context);

	if (!args || (g_strv_length(args) != 1)) {
		fprintf(stderr, _("\nUsage: %s [OPTIONS] DOMAIN-NAME|ID|UUID\n\n%s\n\n"), argv[0], help_msg);
		goto cleanup;
	}

	if (zoom < 10 || zoom > 200) {
		fprintf(stderr, "Zoom level must be within 10-200\n");
		goto cleanup;
	}

	ret = virt_viewer_start(uri, args[0], zoom, direct, waitvm, reconnect, verbose, debug, fullscreen, NULL);
	if (ret != 0)
		return ret;

	gtk_main();

 cleanup:
	g_free(uri);
	g_strfreev(args);

	return ret;
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 *  indent-tabs-mode: t
 * End:
 */
