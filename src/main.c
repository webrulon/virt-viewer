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

#include <vncdisplay.h>
#include <gtk/gtk.h>

#include "viewer.h"

static void viewer_version(FILE *out)
{
	fprintf(out, "%s version %s\n", PACKAGE, VERSION);
}


int main(int argc, char **argv)
{
	GOptionContext *context;
	GError *error = NULL;
	int ret;
	char *uri = NULL;
	gchar **args = NULL;
	gboolean print_version = FALSE;
	gboolean verbose = FALSE;
	gboolean debug = FALSE;
	gboolean direct = FALSE;
	gboolean waitvm = FALSE;
	gboolean reconnect = FALSE;
	const char *help_msg = "Run '" PACKAGE " --help' to see a full list of available command line options";
	const GOptionEntry options [] = {
		{ "version", 'V', 0, G_OPTION_ARG_NONE, &print_version,
		  "display version information", NULL },
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
		  "display verbose information", NULL },
		{ "direct", 'd', 0, G_OPTION_ARG_NONE, &direct,
		  "direct connection with no automatic tunnels", NULL },
		{ "connect", 'c', 0, G_OPTION_ARG_STRING, &uri,
		  "connect to hypervisor", "URI"},
		{ "wait", 'w', 0, G_OPTION_ARG_NONE, &waitvm,
		  "wait for domain to start", NULL },
		{ "reconnect", 'r', 0, G_OPTION_ARG_NONE, &reconnect,
		  "reconnect to domain upon restart", NULL },
		{ "debug", '\0', 0, G_OPTION_ARG_NONE, &debug,
		  "display debugging information", NULL },
  	     	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &args,
		  NULL, "DOMAIN-NAME|ID|UUID" },
  		{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
	};

	/* Setup command line options */
	context = g_option_context_new ("- Virtual machine graphical console");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_group (context, vnc_display_get_option_group ());
	g_option_context_parse (context, &argc, &argv, &error);
	if (error) {
		g_print ("%s\n%s\n",
			 error->message,
			 help_msg);
		g_error_free (error);
		return 1;
	}
	if (print_version) {
		viewer_version(stdout);
		return 0;
	}

	if (!args || (g_strv_length(args) != 1)) {
		fprintf(stderr, "\nUsage: %s [OPTIONS] DOMAIN-NAME|ID|UUID\n\n%s\n\n", argv[0], help_msg);
		return 1;
	}

	ret = viewer_start (uri, args[0], direct, waitvm, reconnect, verbose, debug, NULL);
	if (ret != 0)
		return ret;

	gtk_main();

	return 0;
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
