/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2009 Red Hat,
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
#include <glade/glade.h>
#include <string.h>

#include "auth.h"


int viewer_auth_collect_credentials(const char *type, const char *address,
                                    char **username, char **password)
{
	GtkWidget *dialog = NULL;
	GladeXML *creds = viewer_load_glade("auth.glade", "auth");
	GtkWidget *credUsername;
	GtkWidget *credPassword;
	GtkWidget *promptUsername;
	GtkWidget *promptPassword;
	GtkWidget *labelMessage;
	int response;
	char *message;

	dialog = glade_xml_get_widget(creds, "auth");
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	labelMessage = glade_xml_get_widget(creds, "message");
	credUsername = glade_xml_get_widget(creds, "cred-username");
	promptUsername = glade_xml_get_widget(creds, "prompt-username");
	credPassword = glade_xml_get_widget(creds, "cred-password");
	promptPassword = glade_xml_get_widget(creds, "prompt-password");

	gtk_widget_set_sensitive(credUsername, username != NULL);
	gtk_widget_set_sensitive(promptUsername, username != NULL);
	gtk_widget_set_sensitive(credPassword, password != NULL);
	gtk_widget_set_sensitive(promptPassword, password != NULL);

	if (address) {
		message = g_strdup_printf("Authentication is required for the %s connection to:\n\n"
					  "<b>%s</b>\n\n",
					  type,
					  address ? address : "[unknown]");
	} else {
		message = g_strdup_printf("Authentication is required for the %s connection:\n",
					  type);
	}

	gtk_label_set_markup(GTK_LABEL(labelMessage), message);
	g_free(message);

	gtk_widget_show_all(dialog);
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_hide(dialog);

	if (response == GTK_RESPONSE_OK) {
		if (username)
			*username = g_strdup(gtk_entry_get_text(GTK_ENTRY(credUsername)));
		if (password)
			*password = g_strdup(gtk_entry_get_text(GTK_ENTRY(credPassword)));
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));

	return response == GTK_RESPONSE_OK ? 0 : -1;
}

void viewer_auth_vnc_credentials(GtkWidget *vnc, GValueArray *credList, char **vncAddress)
{
	char *username = NULL, *password = NULL;
	gboolean wantPassword = FALSE, wantUsername = FALSE;
	int i;

        DEBUG_LOG("Got VNC credential request for %d credential(s)", credList->n_values);

        for (i = 0 ; i < credList->n_values ; i++) {
                GValue *cred = g_value_array_get_nth(credList, i);
                switch (g_value_get_enum(cred)) {
                case VNC_DISPLAY_CREDENTIAL_USERNAME:
			wantUsername = TRUE;
			break;
                case VNC_DISPLAY_CREDENTIAL_PASSWORD:
			wantPassword = TRUE;
                        break;
                case VNC_DISPLAY_CREDENTIAL_CLIENTNAME:
			break;
                default:
                        DEBUG_LOG("Unsupported credential type %d", g_value_get_enum(cred));
			vnc_display_close(VNC_DISPLAY(vnc));
			goto cleanup;
                }
        }

        if (wantUsername || wantPassword) {
		int ret = viewer_auth_collect_credentials("VNC", vncAddress ? *vncAddress : NULL,
							  wantUsername ? &username : NULL,
							  wantPassword ? &password : NULL);

		if (ret < 0) {
			vnc_display_close(VNC_DISPLAY(vnc));
			goto cleanup;
		}
        }

        for (i = 0 ; i < credList->n_values ; i++) {
                GValue *cred = g_value_array_get_nth(credList, i);
                switch (g_value_get_enum(cred)) {
                case VNC_DISPLAY_CREDENTIAL_USERNAME:
                        if (!username ||
			    vnc_display_set_credential(VNC_DISPLAY(vnc),
                                                       g_value_get_enum(cred),
                                                       username)) {
                                DEBUG_LOG("Failed to set credential type %d", g_value_get_enum(cred));
                                vnc_display_close(VNC_DISPLAY(vnc));
                        }
			break;
                case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                        if (!password ||
			    vnc_display_set_credential(VNC_DISPLAY(vnc),
                                                       g_value_get_enum(cred),
                                                       password)) {
                                DEBUG_LOG("Failed to set credential type %d", g_value_get_enum(cred));
                                vnc_display_close(VNC_DISPLAY(vnc));
                        }
                        break;
                case VNC_DISPLAY_CREDENTIAL_CLIENTNAME:
                        if (vnc_display_set_credential(VNC_DISPLAY(vnc),
                                                       g_value_get_enum(cred),
                                                       "libvirt")) {
                                DEBUG_LOG("Failed to set credential type %d", g_value_get_enum(cred));
                                vnc_display_close(VNC_DISPLAY(vnc));
                        }
			break;
		default:
                        DEBUG_LOG("Unsupported credential type %d", g_value_get_enum(cred));
                        vnc_display_close(VNC_DISPLAY(vnc));
                }
        }

 cleanup:
        g_free(username);
	g_free(password);
}



int
viewer_auth_libvirt_credentials(virConnectCredentialPtr cred,
				unsigned int ncred,
				void *cbdata)
{
	char **username = NULL, **password = NULL;
	const char *uri = cbdata;
	int i;
	int ret = -1;

        DEBUG_LOG("Got libvirt credential request for %d credential(s)", ncred);

        for (i = 0 ; i < ncred ; i++) {
		switch (cred[i].type) {
		case VIR_CRED_USERNAME:
		case VIR_CRED_AUTHNAME:
			username = &cred[i].result;
			break;
                case VIR_CRED_PASSPHRASE:
			password = &cred[i].result;
                        break;
                default:
			DEBUG_LOG("Unsupported libvirt credential %d", cred[i].type);
			return -1;
                }
        }

        if (username || password) {
		ret = viewer_auth_collect_credentials("libvirt", uri,
						      username, password);
		if (ret < 0)
			goto cleanup;
        } else {
		ret = 0;
	}

        for (i = 0 ; i < ncred ; i++) {
		switch (cred[i].type) {
		case VIR_CRED_AUTHNAME:
		case VIR_CRED_USERNAME:
                case VIR_CRED_PASSPHRASE:
			if (cred[i].result)
				cred[i].resultlen = strlen(cred[i].result);
			else
				cred[i].resultlen = 0;
			DEBUG_LOG("Got '%s' %d %d", cred[i].result, cred[i].resultlen, cred[i].type);
			break;
		}
        }

 cleanup:
	DEBUG_LOG("Return %d", ret);
	return ret;
}




/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
