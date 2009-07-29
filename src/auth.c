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

#include "auth.h"

void viewer_auth_vnc_credentials(GtkWidget *vnc, GValueArray *credList)
{
	GtkWidget *dialog = NULL;
        const char **data;
	gboolean wantPassword = FALSE, wantUsername = FALSE;
	int i;

        DEBUG_LOG("Got credential request for %d credential(s)", credList->n_values);

        data = g_new0(const char *, credList->n_values);

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
                        data[i] = "libvirt";
                default:
                        break;
                }
        }

        if (wantUsername || wantPassword) {
		GladeXML *creds = viewer_load_glade("auth.glade", "auth");
		GtkWidget *credUsername;
		GtkWidget *credPassword;
		GtkWidget *promptUsername;
		GtkWidget *promptPassword;
		int response;

		dialog = glade_xml_get_widget(creds, "auth");
                gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

		credUsername = glade_xml_get_widget(creds, "cred-username");
		promptUsername = glade_xml_get_widget(creds, "prompt-username");
		credPassword = glade_xml_get_widget(creds, "cred-password");
		promptPassword = glade_xml_get_widget(creds, "prompt-password");

		gtk_widget_set_sensitive(credUsername, wantUsername);
		gtk_widget_set_sensitive(promptUsername, wantUsername);
		gtk_widget_set_sensitive(credPassword, wantPassword);
		gtk_widget_set_sensitive(promptPassword, wantPassword);

		gtk_widget_show_all(dialog);
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_hide(dialog);

                if (response == GTK_RESPONSE_OK) {
                        for (i = 0 ; i < credList->n_values ; i++) {
                                GValue *cred = g_value_array_get_nth(credList, i);
                                switch (g_value_get_enum(cred)) {
                                case VNC_DISPLAY_CREDENTIAL_USERNAME:
					data[i] = gtk_entry_get_text(GTK_ENTRY(credUsername));
					break;
                                case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                                        data[i] = gtk_entry_get_text(GTK_ENTRY(credPassword));
                                        break;
                                }
                        }
                }
        }

        for (i = 0 ; i < credList->n_values ; i++) {
                GValue *cred = g_value_array_get_nth(credList, i);
                if (data[i]) {
                        if (vnc_display_set_credential(VNC_DISPLAY(vnc),
                                                       g_value_get_enum(cred),
                                                       data[i])) {
                                DEBUG_LOG("Failed to set credential type %d", g_value_get_enum(cred));
                                vnc_display_close(VNC_DISPLAY(vnc));
                        }
                } else {
                        DEBUG_LOG("Unsupported credential type %d", g_value_get_enum(cred));
                        vnc_display_close(VNC_DISPLAY(vnc));
                }
        }

        g_free(data);
        if (dialog)
                gtk_widget_destroy(GTK_WIDGET(dialog));
}


