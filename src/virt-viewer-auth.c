/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2012 Red Hat, Inc.
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

#include <gtk/gtk.h>
#include <string.h>

#ifdef HAVE_GTK_VNC
#include <vncdisplay.h>
#endif

#include "virt-viewer-auth.h"


int
virt_viewer_auth_collect_credentials(GtkWindow *window,
                                     const char *type,
                                     const char *address,
                                     char **username,
                                     char **password)
{
    GtkWidget *dialog = NULL;
    GtkBuilder *creds = virt_viewer_util_load_ui("virt-viewer-auth.xml");
    GtkWidget *credUsername;
    GtkWidget *credPassword;
    GtkWidget *promptUsername;
    GtkWidget *promptPassword;
    GtkWidget *labelMessage;
    int response;
    char *message;

    dialog = GTK_WIDGET(gtk_builder_get_object(creds, "auth"));
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), window);

    labelMessage = GTK_WIDGET(gtk_builder_get_object(creds, "message"));
    credUsername = GTK_WIDGET(gtk_builder_get_object(creds, "cred-username"));
    promptUsername = GTK_WIDGET(gtk_builder_get_object(creds, "prompt-username"));
    credPassword = GTK_WIDGET(gtk_builder_get_object(creds, "cred-password"));
    promptPassword = GTK_WIDGET(gtk_builder_get_object(creds, "prompt-password"));

    gtk_widget_set_sensitive(credUsername, username != NULL);
    gtk_widget_set_sensitive(promptUsername, username != NULL);
    gtk_widget_set_sensitive(credPassword, password != NULL);
    gtk_widget_set_sensitive(promptPassword, password != NULL);

    if (address) {
        message = g_strdup_printf("Authentication is required for the %s connection to:\n\n"
                                  "<b>%s</b>\n\n",
                                  type,
                                  address);
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

#ifdef HAVE_GTK_VNC
void
virt_viewer_auth_vnc_credentials(GtkWindow *window,
                                 GtkWidget *vnc,
                                 GValueArray *credList,
                                 char *vncAddress)
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
        int ret = virt_viewer_auth_collect_credentials(window,
                                                       "VNC", vncAddress,
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
#endif

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
