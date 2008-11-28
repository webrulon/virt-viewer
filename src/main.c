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
#include <gdk/gdkkeysyms.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <glade/glade.h>
#include <libvirt/libvirt.h>
#include <libvirt-glib/libvirt-glib.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include "viewer.h"

// #define DEBUG 1
#ifdef DEBUG
#define DEBUG_LOG(s, ...) g_debug((s), ## __VA_ARGS__)
#else
#define DEBUG_LOG(s, ...) do {} while (0)
#endif

enum menuNums {
        FILE_MENU,
        VIEW_MENU,
        SEND_KEY_MENU,
        HELP_MENU,
        LAST_MENU // sentinel
};

static const char * const menuNames[LAST_MENU] = {
	"menu-file", "menu-view", "menu-send", "menu-help"
};


#define MAX_KEY_COMBO 3
struct  keyComboDef {
	guint keys[MAX_KEY_COMBO];
	guint nkeys;
	const char *label;
};

static const struct keyComboDef keyCombos[] = {
        { { GDK_Control_L, GDK_Alt_L, GDK_Delete }, 3, "Ctrl+Alt+_Del"},
        { { GDK_Control_L, GDK_Alt_L, GDK_BackSpace }, 3, "Ctrl+Alt+_Backspace"},
        { {}, 0, "" },
        { { GDK_Control_L, GDK_Alt_L, GDK_F1 }, 3, "Ctrl+Alt+F_1"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F2 }, 3, "Ctrl+Alt+F_2"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F3 }, 3, "Ctrl+Alt+F_3"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F4 }, 3, "Ctrl+Alt+F_4"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F5 }, 3, "Ctrl+Alt+F_5"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F6 }, 3, "Ctrl+Alt+F_6"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F7 }, 3, "Ctrl+Alt+F_7"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F8 }, 3, "Ctrl+Alt+F_8"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F5 }, 3, "Ctrl+Alt+F_9"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F6 }, 3, "Ctrl+Alt+F1_0"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F7 }, 3, "Ctrl+Alt+F11"},
        { { GDK_Control_L, GDK_Alt_L, GDK_F8 }, 3, "Ctrl+Alt+F12"},
        { {}, 0, "" },
        { { GDK_Print }, 1, "_PrintScreen"},
};



typedef struct VirtViewer {
	char *uri;
	virConnectPtr conn;
	char *domkey;
	char *domtitle;

	GladeXML *glade;
	GtkWidget *window;
	GtkWidget *vnc;
	int active;

	gboolean accelEnabled;
	GValue accelSetting;
	GSList *accelList;
	int accelMenuSig[LAST_MENU];

	int waitvm;
	int reconnect;
	int direct;
	int verbose;
} VirtViewer;

typedef struct VirtViewerSize {
	gint width, height;
	gulong sig_id;
} VirtViewerSize;


static GladeXML *
viewer_load_glade(const char *name, const char *widget)
{
	char *path;
	struct stat sb;
	GladeXML *xml;

	if (stat(name, &sb) >= 0)
		return glade_xml_new(name, widget, NULL);

	if (asprintf(&path, "%s/%s", GLADE_DIR, name) < 0)
		abort();

	xml = glade_xml_new(path, widget, NULL);
	free(path);
	return xml;
}

/* Now that the size is set to our preferred sizing, this
 * triggers another resize calculation but without our
 * scrolled window callback active. This is the key that
 * allows us to set the fixed size, but then allow the user
 * to later resize it smaller again
 */
static gboolean
viewer_unset_scroll_size (gpointer data)
{
	GtkWidget *widget = data;

	gtk_widget_queue_resize_no_redraw (widget);

	return FALSE;
}

/*
 * This sets the actual size of the scrolled window, and then
 * sets an idle callback to resize again, without constraints
 * activated
 */
static void
viewer_set_preferred_scroll_size (GtkWidget *widget,
				  GtkRequisition *req,
				  gpointer data)
{
	VirtViewerSize *size = data;
	DEBUG_LOG("Scroll resize to preferred %d %d\n", size->width, size->height);

	req->width = size->width;
	req->height = size->height;

	g_signal_handler_disconnect (widget, size->sig_id);
	g_free (size);
	g_idle_add (viewer_unset_scroll_size, widget);
}


/*
 * Called when the scroll widgets actual size has been set.
 * We now update the VNC widget's size
 */
static void viewer_resize_vnc(GtkWidget *scroll G_GNUC_UNUSED,
			      GtkAllocation *alloc,
			      VirtViewer *viewer)
{
	int vncw = vnc_display_get_width(VNC_DISPLAY(viewer->vnc));
	int vnch = vnc_display_get_height(VNC_DISPLAY(viewer->vnc));
	VirtViewerSize *size = g_new (VirtViewerSize, 1);

	if (vnc_display_get_scaling(VNC_DISPLAY(viewer->vnc))) {
		double vncaspect = (double)vncw / (double)vnch;
		double scrollaspect = (double)alloc->width / (double)alloc->height;

		/* When scaling, we set VNC widget size to maximum possible
		 * scaled which fits inside teh scrolled window (no scrollbarS)
		 * while maintaining the aspect ratio */
		if (scrollaspect > vncaspect) {
			size->width = alloc->height * vncaspect;
			size->height = alloc->height;
		} else {
			size->width = alloc->width;
			size->height = alloc->width / vncaspect;
		}
	} else {
		/* When scrollling, the VNC widget is always at its native size */
		size->width = vncw;
		size->height = vnch;
	}

	DEBUG_LOG("Scroll resize is %d %d, desktop is %d %d, VNC is %d %d\n",
		  alloc->width, alloc->height,
		  vncw, vnch, size->width, size->height);

	size->sig_id = g_signal_connect
		(viewer->vnc, "size-request",
		 G_CALLBACK (viewer_set_preferred_scroll_size),
		 size);

	gtk_widget_queue_resize (viewer->vnc);
}


/*
 * Called when VNC desktop size changes.
 * We figure out 'best' size for the containing scrolled window
 */
static void viewer_resize_desktop(GtkWidget *vnc G_GNUC_UNUSED, gint width, gint height, VirtViewer *viewer)
{
	GtkWidget *scroll;
	VirtViewerSize *size = g_new (VirtViewerSize, 1);

	DEBUG_LOG("Resized VNC event %d %d\n", width, height);

	scroll = glade_xml_get_widget(viewer->glade, "vnc-scroll");

	if (viewer->window)
		gtk_window_resize(GTK_WINDOW (viewer->window), 1, 1);

	size->width = width;
	size->height = height;
	size->sig_id = g_signal_connect
		(scroll, "size-request",
		 G_CALLBACK (viewer_set_preferred_scroll_size),
		 size);

	gtk_widget_queue_resize (scroll);
}



static void viewer_set_title(VirtViewer *viewer, gboolean grabbed)
{
	char title[1024];
	const char *subtitle;

	if (!viewer->window)
		return;

	if (grabbed)
		subtitle = "(Press Ctrl+Alt to release pointer) ";
	else
		subtitle = "";

	snprintf(title, sizeof(title), "%s%s - Virt Viewer",
		 subtitle, viewer->domtitle);

	gtk_window_set_title(GTK_WINDOW(viewer->window), title);
}

static void viewer_ignore_accel(GtkWidget *menu G_GNUC_UNUSED,
				VirtViewer *viewer G_GNUC_UNUSED)
{
	/* ignore accelerator */
}


static void viewer_disable_modifiers(VirtViewer *viewer)
{
	GtkSettings *settings = gtk_settings_get_default();
	GValue empty;
	GSList *accels;
	int i;

	if (!viewer->window)
		return;

	if (!viewer->accelEnabled)
		return;

	/* This stops F10 activating menu bar */
	memset(&empty, 0, sizeof empty);
	g_value_init(&empty, G_TYPE_STRING);
	g_object_get_property(G_OBJECT(settings), "gtk-menu-bar-accel", &viewer->accelSetting);
	g_object_set_property(G_OBJECT(settings), "gtk-menu-bar-accel", &empty);

	/* This stops global accelerators like Ctrl+Q == Quit */
	for (accels = viewer->accelList ; accels ; accels = accels->next) {
		gtk_window_remove_accel_group(GTK_WINDOW(viewer->window), accels->data);
	}

	/* This stops menu bar shortcuts like Alt+F == File */
	for (i = 0 ; i < LAST_MENU ; i++) {
		GtkWidget *menu = glade_xml_get_widget(viewer->glade, menuNames[i]);
		viewer->accelMenuSig[i] =
			g_signal_connect(GTK_OBJECT(menu), "mnemonic-activate",
					 GTK_SIGNAL_FUNC(viewer_ignore_accel), viewer);
	}

	viewer->accelEnabled = FALSE;
}


static void viewer_enable_modifiers(VirtViewer *viewer)
{
	GtkSettings *settings = gtk_settings_get_default();
        GSList *accels;
	int i;

	if (!viewer->window)
		return;

	if (viewer->accelEnabled)
		return;

	/* This allows F10 activating menu bar */
	g_object_set_property(G_OBJECT(settings), "gtk-menu-bar-accel", &viewer->accelSetting);

	/* This allows global accelerators like Ctrl+Q == Quit */
	for (accels = viewer->accelList ; accels ; accels = accels->next) {
		gtk_window_add_accel_group(GTK_WINDOW(viewer->window), accels->data);
	}

	/* This allows menu bar shortcuts like Alt+F == File */
	for (i = 0 ; i < LAST_MENU ; i++) {
		GtkWidget *menu = glade_xml_get_widget(viewer->glade, menuNames[i]);
		g_signal_handler_disconnect(GTK_OBJECT(menu),
					    viewer->accelMenuSig[i]);
	}

	viewer->accelEnabled = TRUE;
}



static void viewer_grab(GtkWidget *vnc G_GNUC_UNUSED, VirtViewer *viewer)
{
	viewer_set_title(viewer, TRUE);
	viewer_disable_modifiers(viewer);
}

static void viewer_ungrab(GtkWidget *vnc G_GNUC_UNUSED, VirtViewer *viewer)
{
	viewer_set_title(viewer, FALSE);
	viewer_enable_modifiers(viewer);
}


static void viewer_shutdown(GtkWidget *src G_GNUC_UNUSED, void *dummy G_GNUC_UNUSED, VirtViewer *viewer)
{
	vnc_display_close(VNC_DISPLAY(viewer->vnc));
	gtk_main_quit();
}

static void viewer_menu_file_quit(GtkWidget *src G_GNUC_UNUSED, VirtViewer *viewer)
{
	viewer_shutdown(src, NULL, viewer);
}

static void viewer_menu_view_fullscreen(GtkWidget *menu, VirtViewer *viewer)
{
	if (!viewer->window)
		return;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu))) {
		gtk_window_fullscreen(GTK_WINDOW(viewer->window));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(viewer->window));
	}
}

static void viewer_menu_view_scale(GtkWidget *menu, VirtViewer *viewer)
{
	GtkWidget *scroll = glade_xml_get_widget(viewer->glade, "vnc-scroll");

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu))) {
		vnc_display_set_scaling(VNC_DISPLAY(viewer->vnc), TRUE);
	} else {
		vnc_display_set_scaling(VNC_DISPLAY(viewer->vnc), FALSE);
	}

	gtk_widget_queue_resize (scroll);
}

static void viewer_menu_send(GtkWidget *menu G_GNUC_UNUSED, VirtViewer *viewer)
{
        int i;
        GtkWidget *label = gtk_bin_get_child(GTK_BIN(menu));
        const char *text = gtk_label_get_label(GTK_LABEL(label));
	DEBUG_LOG("Woo\n");
        for (i = 0 ; i < (sizeof(keyCombos)/sizeof(keyCombos[0])) ; i++) {
                if (!strcmp(text, keyCombos[i].label)) {
                        DEBUG_LOG("Sending key combo %s\n", gtk_label_get_text(GTK_LABEL(label)));
                        vnc_display_send_keys(VNC_DISPLAY(viewer->vnc),
                                              keyCombos[i].keys,
                                              keyCombos[i].nkeys);
                        return;
                }
        }
	DEBUG_LOG("Failed to find key combo %s\n", gtk_label_get_text(GTK_LABEL(label)));
}


static void viewer_save_screenshot(GtkWidget *vnc, const char *file)
{
	GdkPixbuf *pix = vnc_display_get_pixbuf(VNC_DISPLAY(vnc));
	gdk_pixbuf_save(pix, file, "png", NULL,
			"tEXt::Generator App", PACKAGE, NULL);
	gdk_pixbuf_unref(pix);
}

static void viewer_menu_file_screenshot(GtkWidget *menu G_GNUC_UNUSED, VirtViewer *viewer)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new ("Save screenshot",
					      NULL,
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

	//gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), default_folder_for_saving);
	//gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "Screenshot");

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		viewer_save_screenshot(viewer->vnc, filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}

static void viewer_about_close(GtkWidget *dialog, VirtViewer *viewer G_GNUC_UNUSED)
{
	gtk_widget_hide(dialog);
	gtk_widget_destroy(dialog);
}

static void viewer_about_delete(GtkWidget *dialog, void *dummy G_GNUC_UNUSED, VirtViewer *viewer G_GNUC_UNUSED)
{
	gtk_widget_hide(dialog);
	gtk_widget_destroy(dialog);
}

static void viewer_menu_help_about(GtkWidget *menu G_GNUC_UNUSED, VirtViewer *viewer)
{
	GladeXML *about;
	GtkWidget *dialog;

	about = viewer_load_glade("about.glade", "about");

	dialog = glade_xml_get_widget(about, "about");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), VERSION);
	glade_xml_signal_connect_data(about, "about_delete",
				      G_CALLBACK(viewer_about_delete), viewer);
	glade_xml_signal_connect_data(about, "about_close",
				      G_CALLBACK(viewer_about_close), viewer);

	gtk_widget_show_all(dialog);

	g_object_unref(G_OBJECT(about));
}


static void viewer_credential(GtkWidget *vnc, GValueArray *credList)
{
	GtkWidget *dialog = NULL;
        const char **data;
	gboolean wantPassword = FALSE, wantUsername = FALSE;
	int i;

        DEBUG_LOG("Got credential request for %d credential(s)\n", credList->n_values);

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
                                DEBUG_LOG("Failed to set credential type %d\n", g_value_get_enum(cred));
                                vnc_display_close(VNC_DISPLAY(vnc));
                        }
                } else {
                        DEBUG_LOG("Unsupported credential type %d\n", g_value_get_enum(cred));
                        vnc_display_close(VNC_DISPLAY(vnc));
                }
        }

        g_free(data);
        if (dialog)
                gtk_widget_destroy(GTK_WIDGET(dialog));
}



static int viewer_parse_uuid(const char *name, unsigned char *uuid)
{
	int i;

	const char *cur = name;
	for (i = 0;i < 16;) {
		uuid[i] = 0;
		if (*cur == 0)
			return -1;
		if ((*cur == '-') || (*cur == ' ')) {
			cur++;
			continue;
		}
		if ((*cur >= '0') && (*cur <= '9'))
			uuid[i] = *cur - '0';
		else if ((*cur >= 'a') && (*cur <= 'f'))
			uuid[i] = *cur - 'a' + 10;
		else if ((*cur >= 'A') && (*cur <= 'F'))
			uuid[i] = *cur - 'A' + 10;
		else
			return -1;
		uuid[i] *= 16;
		cur++;
		if (*cur == 0)
			return -1;
		if ((*cur >= '0') && (*cur <= '9'))
			uuid[i] += *cur - '0';
		else if ((*cur >= 'a') && (*cur <= 'f'))
			uuid[i] += *cur - 'a' + 10;
		else if ((*cur >= 'A') && (*cur <= 'F'))
			uuid[i] += *cur - 'A' + 10;
		else
			return -1;
		i++;
		cur++;
	}

	return 0;
}


static virDomainPtr viewer_lookup_domain(VirtViewer *viewer)
{
	char *end;
	int id = strtol(viewer->domkey, &end, 10);
	virDomainPtr dom = NULL;
	unsigned char uuid[16];

	if (id >= 0 && end && !*end) {
		dom = virDomainLookupByID(viewer->conn, id);
	}
	if (!dom && viewer_parse_uuid(viewer->domkey, uuid) == 0) {
		dom = virDomainLookupByUUID(viewer->conn, uuid);
	}
	if (!dom) {
		dom = virDomainLookupByName(viewer->conn, viewer->domkey);
	}
	return dom;
}

static int viewer_matches_domain(VirtViewer *viewer,
				 virDomainPtr dom)
{
	char *end;
	const char *name;
	int id = strtol(viewer->domkey, &end, 10);
	unsigned char wantuuid[16];
	unsigned char domuuid[16];

	if (id >= 0 && end && !*end) {
		if (virDomainGetID(dom) == id)
			return 1;
	}
	if (!dom && viewer_parse_uuid(viewer->domkey, wantuuid) == 0) {
		virDomainGetUUID(dom, domuuid);
		if (memcmp(wantuuid, domuuid, VIR_UUID_BUFLEN) == 0)
			return 1;
	}

	name = virDomainGetName(dom);
	if (strcmp(name, viewer->domkey) == 0)
		return 1;

	return 0;
}

static char * viewer_extract_vnc_port(virDomainPtr dom)
{
	char *xmldesc = virDomainGetXMLDesc(dom, 0);
	xmlDocPtr xml = NULL;
	xmlParserCtxtPtr pctxt = NULL;
	xmlXPathContextPtr ctxt = NULL;
	xmlXPathObjectPtr obj = NULL;
	char *port = NULL;

	pctxt = xmlNewParserCtxt();
	if (!pctxt || !pctxt->sax)
		goto error;

	xml = xmlCtxtReadDoc(pctxt, (const xmlChar *)xmldesc, "domain.xml", NULL,
			     XML_PARSE_NOENT | XML_PARSE_NONET |
			     XML_PARSE_NOWARNING);
	free(xmldesc);
	if (!xml)
		goto error;

	ctxt = xmlXPathNewContext(xml);
	if (!ctxt)
		goto error;

	obj = xmlXPathEval((const xmlChar *)"string(/domain/devices/graphics[@type='vnc']/@port)", ctxt);
	if (!obj || obj->type != XPATH_STRING || !obj->stringval || !obj->stringval[0])
		goto error;
	if (!strcmp((const char*)obj->stringval, "-1"))
		goto error;

	port = strdup((const char*)obj->stringval);
	xmlXPathFreeObject(obj);
	obj = NULL;

 error:
	if (obj)
		xmlXPathFreeObject(obj);
	if (ctxt)
		xmlXPathFreeContext(ctxt);
	if (xml)
		xmlFreeDoc(xml);
	if (pctxt)
		xmlFreeParserCtxt(pctxt);
	return port;
}


static int viewer_extract_host(const char *uristr, char **host, char **transport, char **user, int *port)
{
	xmlURIPtr uri;
	char *offset;

	*host = NULL;
	*transport = NULL;
	*user = NULL;

	if (uristr == NULL ||
	    !strcasecmp(uristr, "xen"))
		uristr = "xen:///";

	uri = xmlParseURI(uristr);
	if (!uri || !uri->server) {
		*host = strdup("localhost");
	} else {
		*host = strdup(uri->server);
	}
	if (!*host) {
		xmlFreeURI(uri);
		return -1;
	}
	if (uri->user) {
		*user = strdup(uri->user);
		if (!*user) {
			xmlFreeURI(uri);
			free(*host);
			*host =NULL;
			return -1;
		}
	}
	*port = uri->port;

	offset = strchr(uri->scheme, '+');
	if (offset) {
		*transport = strdup(offset+1);
		if (!*transport) {
			free(*host);
			*host = NULL;
			free(*user);
			*user = NULL;
			xmlFreeURI(uri);
			return -1;
		}
	}
	xmlFreeURI(uri);
	return 0;
}

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)

static int viewer_open_tunnel(const char **cmd)
{
        int fd[2];
        pid_t pid;

        if (socketpair(PF_UNIX, SOCK_STREAM, 0, fd) < 0)
                return -1;

        pid = fork();
        if (pid == -1) {
		close(fd[0]);
		close(fd[1]);
		return -1;
	}

        if (pid == 0) { /* child */
                close(fd[0]);
                close(0);
                close(1);
                if (dup(fd[1]) < 0)
                        _exit(1);
                if (dup(fd[1]) < 0)
                        _exit(1);
                close(fd[1]);
                execvp("ssh", (char *const*)cmd);
                _exit(1);
        }
        close(fd[1]);
	return fd[0];
}


static int viewer_open_tunnel_ssh(const char *sshhost, int sshport, const char *sshuser, const char *vncport)
{
	const char *cmd[10];
	char portstr[50];
	int n = 0;

	if (!sshport)
		sshport = 22;

	sprintf(portstr, "%d", sshport);

	cmd[n++] = "ssh";
	cmd[n++] = "-p";
	cmd[n++] = portstr;
	if (sshuser) {
		cmd[n++] = "-l";
		cmd[n++] = sshuser;
	}
	cmd[n++] = sshhost;
	cmd[n++] = "nc";
	cmd[n++] = "localhost";
	cmd[n++] = vncport;
	cmd[n++] = NULL;

	return viewer_open_tunnel(cmd);
}

#endif /* defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK) */

static void viewer_set_status(VirtViewer *viewer, const char *text)
{
	GtkWidget *status, *notebook;

	notebook = glade_xml_get_widget(viewer->glade, "notebook");
	status = glade_xml_get_widget(viewer->glade, "status");

	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
	gtk_label_set_text(GTK_LABEL(status), text);
}


static void viewer_set_vnc(VirtViewer *viewer)
{
	GtkWidget *notebook;

	notebook = glade_xml_get_widget(viewer->glade, "notebook");
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 1);

	gtk_widget_show(viewer->vnc);
}


static int viewer_activate(VirtViewer *viewer,
			   virDomainPtr dom)
{
	char *vncport = NULL;
	char *host = NULL;
	char *transport = NULL;
	char *user = NULL;
	int port, fd = -1;
	int ret = -1;

	if (viewer->active)
		goto cleanup;

	if ((vncport = viewer_extract_vnc_port(dom)) == NULL)
		goto cleanup;

        if (viewer_extract_host(viewer->uri, &host, &transport, &user, &port) < 0)
		goto cleanup;

        DEBUG_LOG("Remote host is %s and transport %s user %s\n",
		  host, transport ? transport : "", user ? user : "");

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)
        if (transport && strcasecmp(transport, "ssh") == 0 &&
	    !viewer->direct)
                if ((fd = viewer_open_tunnel_ssh(host, port, user, vncport)) < 0)
			return -1;
#endif

	if (fd >= 0)
		vnc_display_open_fd(VNC_DISPLAY(viewer->vnc), fd);
	else
		vnc_display_open_host(VNC_DISPLAY(viewer->vnc), host, vncport);

	viewer_set_status(viewer, "Connecting to VNC server");

	free(viewer->domtitle);
	viewer->domtitle = strdup(virDomainGetName(dom));

	viewer->active = 1;
	viewer_set_title(viewer, FALSE);

	ret = 0;
 cleanup:
	free(host);
	free(transport);
	free(user);
	free(vncport);
	return ret;

}

static void viewer_deactivate(VirtViewer *viewer)
{
	if (!viewer->active)
		return;

	vnc_display_close(VNC_DISPLAY(viewer->vnc));
	free(viewer->domtitle);
	viewer->domtitle = NULL;

	if (viewer->reconnect) {
		viewer_set_status(viewer, "Waiting for guest domain to re-start");
	} else {
		viewer_set_status(viewer, "Guest domain has shutdown");
		gtk_main_quit();
	}
	viewer->active = 0;
	viewer_set_title(viewer, FALSE);
}

static void viewer_connected(GtkWidget *vnc G_GNUC_UNUSED, VirtViewer *viewer)
{
	viewer_set_status(viewer, "Connected to VNC server");
}

static void viewer_initialized(GtkWidget *vnc G_GNUC_UNUSED, VirtViewer *viewer)
{
	viewer_set_vnc(viewer);
        viewer_set_title(viewer, FALSE);
}


static void viewer_disconnected(GtkWidget *vnc G_GNUC_UNUSED, VirtViewer *viewer)
{
	viewer_deactivate(viewer);
}


static int viewer_domain_event(virConnectPtr conn G_GNUC_UNUSED,
			       virDomainPtr dom,
			       int event,
			       int detail G_GNUC_UNUSED,
			       void *opaque)
{
	VirtViewer *viewer = opaque;

	if (!viewer_matches_domain(viewer, dom))
		return 0;

	switch (event) {
	case VIR_DOMAIN_EVENT_STOPPED:
		viewer_deactivate(viewer);
		break;

	case VIR_DOMAIN_EVENT_STARTED:
		viewer_activate(viewer, dom);
		break;
	}

	return 0;
}


static int viewer_initial_connect(VirtViewer *viewer)
{
	virDomainPtr dom = NULL;
	virDomainInfo info;
	int ret = -1;

	viewer_set_status(viewer, "Finding guest domain");
	dom = viewer_lookup_domain(viewer);
	if (!dom)
		goto cleanup;

	viewer_set_status(viewer, "Checking guest domain");
	if (virDomainGetInfo(dom, &info) < 0)
		goto cleanup;

	if (info.state == VIR_DOMAIN_SHUTOFF) {
		viewer_set_status(viewer, "Waiting for guest domain to start");
	} else {
		if (viewer_activate(viewer, dom) < 0) {
			if (viewer->waitvm) {
				viewer_set_status(viewer, "Waiting for guest domain to start VNC");
			} else {
				goto cleanup;
			}
		}
	}

	ret = 0;
 cleanup:
	if (dom)
		virDomainFree(dom);
	return ret;
}

int
viewer_start (const char *uri,
	      const char *name,
	      gboolean direct,
	      gboolean waitvm,
	      gboolean reconnect,
	      gboolean verbose,
	      GtkWidget *container)
{
	VirtViewer *viewer;
	GtkWidget *notebook;
	GtkWidget *scroll;
	GtkWidget *align;

	viewer = g_new0(VirtViewer, 1);

	viewer->active = 0;
	viewer->direct = direct;
	viewer->waitvm = waitvm;
	viewer->reconnect = reconnect;
	viewer->verbose = verbose;
	viewer->domkey = g_strdup(name);
	viewer->uri = g_strdup(uri);

	g_value_init(&viewer->accelSetting, G_TYPE_STRING);

	virEventRegisterGLib();

	viewer->conn = virConnectOpenReadOnly(uri);
	if (!viewer->conn) {
		fprintf(stderr, "unable to connect to libvirt %s\n",
			uri ? uri : "xen");
		return -1;
	}

	if (!(viewer->glade = viewer_load_glade("viewer.glade",
						container ? "notebook" : "viewer")))
		return -1;

	glade_xml_signal_connect_data(viewer->glade, "viewer_menu_file_quit",
				      G_CALLBACK(viewer_menu_file_quit), viewer);
	glade_xml_signal_connect_data(viewer->glade, "viewer_menu_file_screenshot",
				      G_CALLBACK(viewer_menu_file_screenshot), viewer);
	glade_xml_signal_connect_data(viewer->glade, "viewer_menu_view_fullscreen",
				      G_CALLBACK(viewer_menu_view_fullscreen), viewer);
	glade_xml_signal_connect_data(viewer->glade, "viewer_menu_view_scale",
				      G_CALLBACK(viewer_menu_view_scale), viewer);
	glade_xml_signal_connect_data(viewer->glade, "viewer_menu_send",
				      G_CALLBACK(viewer_menu_send), viewer);
	glade_xml_signal_connect_data(viewer->glade, "viewer_menu_help_about",
				      G_CALLBACK(viewer_menu_help_about), viewer);


	viewer->vnc = vnc_display_new();
        vnc_display_set_keyboard_grab(VNC_DISPLAY(viewer->vnc), TRUE);
        vnc_display_set_pointer_grab(VNC_DISPLAY(viewer->vnc), TRUE);
	vnc_display_set_force_size(VNC_DISPLAY(viewer->vnc), FALSE);
	//vnc_display_set_scaling(VNC_DISPLAY(viewer->vnc), TRUE);

        g_signal_connect(GTK_OBJECT(viewer->vnc), "vnc-connected",
			 GTK_SIGNAL_FUNC(viewer_connected), viewer);
        g_signal_connect(GTK_OBJECT(viewer->vnc), "vnc-initialized",
			 GTK_SIGNAL_FUNC(viewer_initialized), viewer);
        g_signal_connect(GTK_OBJECT(viewer->vnc), "vnc-disconnected",
			 GTK_SIGNAL_FUNC(viewer_disconnected), viewer);
	g_signal_connect(GTK_OBJECT(viewer->vnc), "vnc-desktop-resize",
			 GTK_SIGNAL_FUNC(viewer_resize_desktop), viewer);
        g_signal_connect(GTK_OBJECT(viewer->vnc), "vnc-pointer-grab",
			 GTK_SIGNAL_FUNC(viewer_grab), viewer);
        g_signal_connect(GTK_OBJECT(viewer->vnc), "vnc-pointer-ungrab",
			 GTK_SIGNAL_FUNC(viewer_ungrab), viewer);
        g_signal_connect(GTK_OBJECT(viewer->vnc), "vnc-auth-credential",
                         GTK_SIGNAL_FUNC(viewer_credential), NULL);

	notebook = glade_xml_get_widget(viewer->glade, "notebook");

	if (!notebook)
		return -1;

	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
	//gtk_notebook_append_page(GTK_NOTEBOOK(notebook), viewer->vnc, NULL);
	scroll = glade_xml_get_widget(viewer->glade, "vnc-scroll");
	align = glade_xml_get_widget(viewer->glade, "vnc-align");
	gtk_container_add(GTK_CONTAINER(align), viewer->vnc);

	g_signal_connect(GTK_OBJECT(scroll), "size-allocate",
			   GTK_SIGNAL_FUNC(viewer_resize_vnc), viewer);

	if (container) {
		gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(notebook));
		if (GTK_IS_WINDOW(container))
			gtk_window_set_resizable(GTK_WINDOW(container), TRUE);
		gtk_widget_show_all(container);
	} else {
		GtkWidget *window = glade_xml_get_widget(viewer->glade, "viewer");
		GSList *accels;
		viewer->window = window;
		g_signal_connect(GTK_OBJECT(window), "delete-event",
				 GTK_SIGNAL_FUNC(viewer_shutdown), viewer);
		gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
		viewer->accelEnabled = TRUE;
		accels = gtk_accel_groups_from_object(G_OBJECT(window));
		for ( ; accels ; accels = accels->next) {
			viewer->accelList = g_slist_append(viewer->accelList, accels->data);
			g_object_ref(G_OBJECT(accels->data));
		}
		gtk_widget_show_all(window);
	}

	gtk_widget_realize(viewer->vnc);


	if (viewer_initial_connect(viewer) < 0)
		return -1;

	virConnectDomainEventRegister(viewer->conn,
				      viewer_domain_event,
				      viewer,
				      NULL);

	return 0;
}

#ifndef PLUGIN
/* Standalone program. */

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

	ret = viewer_start (uri, args[0], direct, waitvm, reconnect, verbose, NULL);
	if (ret != 0)
		return ret;

	gtk_main();

	return 0;
}
#endif /* !PLUGIN */

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
