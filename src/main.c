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

#define _GNU_SOURCE

#include <vncdisplay.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <libvirt/libvirt.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <sys/socket.h>
#include <sys/un.h>

// #define DEBUG 1
#ifdef DEBUG
#define DEBUG_LOG(s, ...) fprintf(stderr, (s), ## __VA_ARGS__)
#else
#define DEBUG_LOG(s, ...) do {} while (0)
#endif

static int verbose = 0;
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
	{ {}, 0, "" },
	{ { GDK_Print }, 1, "_PrintScreen"},
};


static void viewer_set_title(VncDisplay *vnc, GtkWidget *window, gboolean grabbed)
{
	const char *name;
	char title[1024];
	const char *subtitle;

	if (grabbed)
		subtitle = "(Press Ctrl+Alt to release pointer) ";
	else
		subtitle = "";

	name = vnc_display_get_name(VNC_DISPLAY(vnc));
	snprintf(title, sizeof(title), "%s%s - Virt Viewer",
		 subtitle, name);

	gtk_window_set_title(GTK_WINDOW(window), title);
}

static void viewer_grab(GtkWidget *vnc, GtkWidget *window)
{
	viewer_set_title(VNC_DISPLAY(vnc), window, TRUE);
}

static void viewer_ungrab(GtkWidget *vnc, GtkWidget *window)
{
	viewer_set_title(VNC_DISPLAY(vnc), window, FALSE);
}

static void viewer_shutdown(GtkWidget *src G_GNUC_UNUSED, void *dummy G_GNUC_UNUSED, GtkWidget *vnc)
{
	vnc_display_close(VNC_DISPLAY(vnc));
	gtk_main_quit();
}

static void viewer_quit(GtkWidget *src G_GNUC_UNUSED, GtkWidget *vnc)
{
	viewer_shutdown(src, NULL, vnc);
}

static void viewer_connected(GtkWidget *vnc G_GNUC_UNUSED)
{
	DEBUG_LOG("Connected to server\n");
}

static void viewer_initialized(GtkWidget *vnc, GtkWidget *window)
{
	DEBUG_LOG("Connection initialized\n");
	gtk_widget_show_all(window);
	viewer_set_title(VNC_DISPLAY(vnc), window, FALSE);
}

static void viewer_disconnected(GtkWidget *vnc G_GNUC_UNUSED)
{
	DEBUG_LOG("Disconnected from server\n");
	gtk_main_quit();
}

static void viewer_send_key(GtkWidget *menu, GtkWidget *vnc)
{
	int i;
	GtkWidget *label = gtk_bin_get_child(GTK_BIN(menu));
	const char *text = gtk_label_get_label(GTK_LABEL(label));

	for (i = 0 ; i < (sizeof(keyCombos)/sizeof(keyCombos[0])) ; i++) {
		if (!strcmp(text, keyCombos[i].label)) {
			DEBUG_LOG("Sending key combo %s\n", gtk_label_get_text(GTK_LABEL(label)));
			vnc_display_send_keys(VNC_DISPLAY(vnc),
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

static void viewer_screenshot(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vnc)
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
		viewer_save_screenshot(vnc, filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}


static void viewer_credential(GtkWidget *vnc, GValueArray *credList)
{
        GtkWidget *dialog = NULL;
        int response;
        unsigned int i, prompt = 0;
        const char **data;

        DEBUG_LOG("Got credential request for %d credential(s)\n", credList->n_values);

        data = g_new0(const char *, credList->n_values);

        for (i = 0 ; i < credList->n_values ; i++) {
                GValue *cred = g_value_array_get_nth(credList, i);
                switch (g_value_get_enum(cred)) {
                case VNC_DISPLAY_CREDENTIAL_USERNAME:
                case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                        prompt++;
                        break;
                case VNC_DISPLAY_CREDENTIAL_CLIENTNAME:
                        data[i] = "libvirt";
                default:
                        break;
                }
        }

        if (prompt) {
                GtkWidget **label, **entry, *box, *vbox;
                int row;
                dialog = gtk_dialog_new_with_buttons("Authentication required",
                                                     NULL,
                                                     0,
                                                     GTK_STOCK_CANCEL,
                                                     GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OK,
                                                     GTK_RESPONSE_OK,
                                                     NULL);
                gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

                box = gtk_table_new(credList->n_values, 2, FALSE);
                label = g_new(GtkWidget *, prompt);
                entry = g_new(GtkWidget *, prompt);

                for (i = 0, row =0 ; i < credList->n_values ; i++) {
                        GValue *cred = g_value_array_get_nth(credList, i);
                        switch (g_value_get_enum(cred)) {
                        case VNC_DISPLAY_CREDENTIAL_USERNAME:
                                label[row] = gtk_label_new("Username:");
                                break;
                        case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                                label[row] = gtk_label_new("Password:");
                                break;
                        default:
                                continue;
                        }
                        entry[row] = gtk_entry_new();

                        gtk_table_attach(GTK_TABLE(box), label[i], 0, 1, row, row+1, GTK_SHRINK, GTK_SHRINK, 3, 3);
                        gtk_table_attach(GTK_TABLE(box), entry[i], 1, 2, row, row+1, GTK_SHRINK, GTK_SHRINK, 3, 3);
                        row++;
                }

                vbox = gtk_bin_get_child(GTK_BIN(dialog));
                gtk_container_add(GTK_CONTAINER(vbox), box);

                gtk_widget_show_all(dialog);
                response = gtk_dialog_run(GTK_DIALOG(dialog));
                gtk_widget_hide(GTK_WIDGET(dialog));

                if (response == GTK_RESPONSE_OK) {
                        for (i = 0, row = 0 ; i < credList->n_values ; i++) {
                                GValue *cred = g_value_array_get_nth(credList, i);
                                switch (g_value_get_enum(cred)) {
                                case VNC_DISPLAY_CREDENTIAL_USERNAME:
                                case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                                        data[i] = gtk_entry_get_text(GTK_ENTRY(entry[row]));
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

static void viewer_about(GtkWidget *menu G_GNUC_UNUSED)
{
	GtkWidget *about;
	const char *authors[] = {
		"Daniel P. Berrange <berrange@redhat.com>",
		NULL
	};

	about = gtk_about_dialog_new();

	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(about), "Virtual Machine Viewer");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), VERSION);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about), "http://virt-manager.org/");
	gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(about), "http://virt-manager.org/");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about), authors);
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about), 
 "This program is free software; you can redistribute it and/or modify\n" \
 "it under the terms of the GNU General Public License as published by\n" \
 "the Free Software Foundation; either version 2 of the License, or\n" \
 "(at your option) any later version.\n" \
 "\n" \
 "This program is distributed in the hope that it will be useful,\n" \
 "but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
 "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" \
 "GNU General Public License for more details.\n" \
 "\n" \
 "You should have received a copy of the GNU General Public License\n" \
 "along with this program; if not, write to the Free Software\n" \
 "Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n");


	gtk_dialog_run(GTK_DIALOG(about));

	gtk_widget_destroy(about);
}

static GtkWidget *viewer_build_file_menu(VncDisplay *vnc)
{
	GtkWidget *file;
	GtkWidget *filemenu;
	GtkWidget *quit;
	GtkWidget *screenshot;

	file = gtk_menu_item_new_with_mnemonic("_File");

	filemenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), filemenu);

	screenshot = gtk_menu_item_new_with_mnemonic("_Screenshot");
	gtk_menu_append(GTK_MENU(filemenu), screenshot);
	g_signal_connect(screenshot, "activate", GTK_SIGNAL_FUNC(viewer_screenshot), vnc);

	gtk_menu_append(GTK_MENU(filemenu), gtk_separator_menu_item_new());


	quit = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
	gtk_menu_append(GTK_MENU(filemenu), quit);
	g_signal_connect(quit, "activate", GTK_SIGNAL_FUNC(viewer_quit), vnc);

	return file;
}

static GtkWidget *viewer_build_sendkey_menu(VncDisplay *vnc)
{
	GtkWidget *sendkey;
	GtkWidget *sendkeymenu;
	int i;

	sendkey = gtk_menu_item_new_with_mnemonic("_Send Key");

	sendkeymenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(sendkey), sendkeymenu);

	for (i = 0 ; i < (sizeof(keyCombos)/sizeof(keyCombos[0])) ; i++) {
		GtkWidget *key;

		if (keyCombos[i].nkeys) {
			key = gtk_menu_item_new_with_mnemonic(keyCombos[i].label);
			gtk_menu_append(GTK_MENU(sendkeymenu), key);
			g_signal_connect(key, "activate", GTK_SIGNAL_FUNC(viewer_send_key), vnc);
		} else {
			gtk_menu_append(GTK_MENU(sendkeymenu), gtk_separator_menu_item_new());
		}
	}

	return sendkey;
}

static GtkWidget *viewer_build_help_menu(void)
{
	GtkWidget *help;
	GtkWidget *helpmenu;
	GtkWidget *about;

	help = gtk_menu_item_new_with_mnemonic("_Help");

	helpmenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(help), helpmenu);

	about = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
	gtk_menu_append(GTK_MENU(helpmenu), about);
	g_signal_connect(about, "activate", GTK_SIGNAL_FUNC(viewer_about), NULL);

	return help;
}

static GtkWidget *viewer_build_menu(VncDisplay *vnc)
{
	GtkWidget *menubar;
	GtkWidget *file;
	GtkWidget *sendkey;
	GtkWidget *help;

	menubar = gtk_menu_bar_new();

	file = viewer_build_file_menu(vnc);
	sendkey = viewer_build_sendkey_menu(vnc);
	help = viewer_build_help_menu();

	gtk_menu_bar_append(GTK_MENU_BAR(menubar), file);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), sendkey);
	gtk_menu_bar_append(GTK_MENU_BAR(menubar), help);


	return menubar;
}

static GtkWidget *viewer_build_window(VncDisplay *vnc)
{
	GtkWidget *window;
	GtkWidget *menubar;
	GtkWidget *layout;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	layout = gtk_vbox_new(FALSE, 3);
	menubar = viewer_build_menu(vnc);

	gtk_container_add(GTK_CONTAINER(window), layout);
	gtk_container_add(GTK_CONTAINER(layout), menubar);
	gtk_container_add(GTK_CONTAINER(layout), GTK_WIDGET(vnc));
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

	gtk_signal_connect(GTK_OBJECT(vnc), "vnc-pointer-grab",
			   GTK_SIGNAL_FUNC(viewer_grab), window);
	gtk_signal_connect(GTK_OBJECT(vnc), "vnc-pointer-ungrab",
			   GTK_SIGNAL_FUNC(viewer_ungrab), window);

	gtk_signal_connect(GTK_OBJECT(window), "delete-event",
			   GTK_SIGNAL_FUNC(viewer_shutdown), vnc);

	gtk_signal_connect(GTK_OBJECT(vnc), "vnc-connected",
			   GTK_SIGNAL_FUNC(viewer_connected), NULL);
	gtk_signal_connect(GTK_OBJECT(vnc), "vnc-initialized",
			   GTK_SIGNAL_FUNC(viewer_initialized), window);
	gtk_signal_connect(GTK_OBJECT(vnc), "vnc-disconnected",
			   GTK_SIGNAL_FUNC(viewer_disconnected), NULL);

	g_signal_connect(GTK_OBJECT(vnc), "vnc-auth-credential",
			 GTK_SIGNAL_FUNC(viewer_credential), NULL);

	return window;
}


static void viewer_version(FILE *out)
{
	fprintf(out, "%s version %s\n", PACKAGE, VERSION);
}

static void viewer_help(FILE *out, const char *app)
{
	fprintf(out, "\n");
	fprintf(out, "syntax: %s [OPTIONS] DOMAIN-NAME|ID|UUID\n", app);
	fprintf(out, "\n");
	viewer_version(out);
	fprintf(out, "\n");
	fprintf(out, "Options:");
	fprintf(out, "  -h, --help              display command line help\n");
	fprintf(out, "  -v, --verbose           display verbose information\n");
	fprintf(out, "  -V, --version           display verion informaton\n");
	fprintf(out, "  -d, --direct            direct connection with no automatic tunnels\n");
	fprintf(out, "  -c URI, --connect URI   connect to hypervisor URI\n");
	fprintf(out, "  -w, --wait              wait for domain to start\n");
	fprintf(out, "\n");
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


static virDomainPtr viewer_lookup_domain(virConnectPtr conn, const char *name)
{
	char *end;
	int id = strtol(name, &end, 10);
	virDomainPtr dom = NULL;
	unsigned char uuid[16];

	if (id >= 0 && end && !*end) {
		dom = virDomainLookupByID(conn, id);
	}
	if (!dom && viewer_parse_uuid(name, uuid) == 0) {
		dom = virDomainLookupByUUID(conn, uuid);
	}
	if (!dom) {
		dom = virDomainLookupByName(conn, name);
	}
	return dom;
}

static int viewer_extract_vnc_graphics(virDomainPtr dom, char **port)
{
	char *xmldesc = virDomainGetXMLDesc(dom, 0);
	xmlDocPtr xml = NULL;
	xmlParserCtxtPtr pctxt = NULL;
	xmlXPathContextPtr ctxt = NULL;
	xmlXPathObjectPtr obj = NULL;
	int ret = -1;

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
		goto missing;

	*port = strdup((const char*)obj->stringval);
	xmlXPathFreeObject(obj);
	obj = NULL;

 missing:
	ret = 0;

 error:
	if (obj)
		xmlXPathFreeObject(obj);
	if (ctxt)
		xmlXPathFreeContext(ctxt);
	if (xml)
		xmlFreeDoc(xml);
	if (pctxt)
		xmlFreeParserCtxt(pctxt);
	return ret;
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


int main(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *vnc;
	char *uri = NULL;
	int opt_ind;
	const char *sopts = "hVc:";
	static const struct option lopts[] = {
		{ "help", 0, 0, 'h' },
		{ "version", 0, 0, 'V' },
		{ "verbose", 0, 0, 'v' },
		{ "connect", 1, 0, 'c' },
		{ "wait", 0, 0, 'w' },
		{ "direct", 0, 0, 'd' },
		{ 0, 0, 0, 0 }
	};
	int ch;
	int waitvnc = 0;
	virConnectPtr conn = NULL;
	virDomainPtr dom = NULL;
	char *host = NULL;
	char *vncport = NULL;
	char *transport = NULL;
	char *user = NULL;
	int port = 0;
	int fd = -1;
	int direct = 0;

	while ((ch = getopt_long(argc, argv, sopts, lopts, &opt_ind)) != -1) {
		switch (ch) {
		case 'h':
			viewer_help(stdout, argv[0]);
			return 0;
		case 'V':
			viewer_version(stdout);
			return 0;
		case 'v':
			verbose = 1;
			break;
		case 'c':
			uri = strdup(optarg);
			break;
		case 'w':
			waitvnc = 1;
			break;
		case 'd':
			direct = 1;
			break;
		case '?':
			viewer_help(stderr, argv[0]);
			return 1;
		}
	}


	if (argc != (optind+1)) {
		viewer_help(stderr, argv[0]);
		return 1;
	}

	gtk_init(&argc, &argv);

	conn = virConnectOpenReadOnly(uri);
	if (!conn) {
		fprintf(stderr, "unable to connect to libvirt %s\n",
			uri ? uri : "xen");
		return 2;
	}

	do {
		dom = viewer_lookup_domain(conn, argv[optind]);
		if (!dom && !waitvnc) {
			fprintf(stderr, "unable to lookup domain %s\n", argv[optind]);
			return 3;
		}
		if (!dom)
			usleep(500*1000);
	} while (!dom);

	do {
		viewer_extract_vnc_graphics(dom, &vncport);
		if (!vncport && !waitvnc) {
			fprintf(stderr, "unable to find vnc graphics for %s\n", argv[optind]);
			return 4;
		}
		if (!vncport)
			usleep(300*1000);
	} while (!vncport);
	virDomainFree(dom);
	virConnectClose(conn);

	if (viewer_extract_host(uri, &host, &transport, &user, &port) < 0) {
		fprintf(stderr, "unable to determine hostname for URI %s\n", uri);
		return 5;
	}
	DEBUG_LOG("Remote host is %s and transport %s user %s\n", host, transport ? transport : "", user ? user : "");

	if (transport && strcasecmp(transport, "ssh") == 0 && !direct)
		fd = viewer_open_tunnel_ssh(host, port, user, vncport);

	vnc = vnc_display_new();
	window = viewer_build_window(VNC_DISPLAY(vnc));
	gtk_widget_realize(vnc);

	vnc_display_set_keyboard_grab(VNC_DISPLAY(vnc), TRUE);
	vnc_display_set_pointer_grab(VNC_DISPLAY(vnc), TRUE);

	if (fd >= 0)
		vnc_display_open_fd(VNC_DISPLAY(vnc), fd);
	else
		vnc_display_open_host(VNC_DISPLAY(vnc), host, vncport);

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
