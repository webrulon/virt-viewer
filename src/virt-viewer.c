/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2009 Red Hat,
 * Copyright (C) 2009 Daniel P. Berrange
 * Copyright (C) 2010 Marc-André Lureau
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
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

#include "virt-viewer.h"
#include "virt-viewer-events.h"
#include "virt-viewer-auth.h"
#include "virt-viewer-session-vnc.h"

#ifdef HAVE_SPICE_GTK
#include "virt-viewer-session-spice.h"
#endif

#include "view/autoDrawer.h"

gboolean doDebug = FALSE;

/* Signal handlers for main window */
void virt_viewer_menu_view_zoom_out(GtkWidget *menu, VirtViewer *viewer);
void virt_viewer_menu_view_zoom_in(GtkWidget *menu, VirtViewer *viewer);
void virt_viewer_menu_view_zoom_reset(GtkWidget *menu, VirtViewer *viewer);
void virt_viewer_delete(GtkWidget *src, void *dummy, VirtViewer *viewer);
void virt_viewer_menu_file_quit(GtkWidget *src, VirtViewer *viewer);
void virt_viewer_menu_help_about(GtkWidget *menu, VirtViewer *viewer);
void virt_viewer_menu_view_fullscreen(GtkWidget *menu, VirtViewer *viewer);
void virt_viewer_menu_view_resize(GtkWidget *menu, VirtViewer *viewer);
void virt_viewer_menu_send(GtkWidget *menu, VirtViewer *viewer);
void virt_viewer_menu_file_screenshot(GtkWidget *menu, VirtViewer *viewer);

/* Signal handlers for about dialog */
void virt_viewer_about_close(GtkWidget *dialog, VirtViewer *viewer);
void virt_viewer_about_delete(GtkWidget *dialog, void *dummy, VirtViewer *viewer);


/* Internal methods */
static void virt_viewer_connected(VirtViewerSession *session,
				  VirtViewer *viewer);
static void virt_viewer_initialized(VirtViewerSession *session,
				  VirtViewer *viewer);
static void virt_viewer_disconnected(VirtViewerSession *session,
				  VirtViewer *viewer);
static void virt_viewer_auth_refused(VirtViewerSession *session,
				     const char *msg,
				     VirtViewer *viewer);
static void virt_viewer_auth_failed(VirtViewerSession *session,
				    const char *msg,
				    VirtViewer *viewer);

static void virt_viewer_server_cut_text(VirtViewerSession *session,
					const gchar *text,
					VirtViewer *viewer);
static void virt_viewer_bell(VirtViewerSession *session,
			     VirtViewer *viewer);
static void virt_viewer_enable_modifiers(VirtViewer *viewer);
static void virt_viewer_disable_modifiers(VirtViewer *viewer);
static void virt_viewer_resize_main_window(VirtViewer *viewer);

static void virt_viewer_set_status(VirtViewer *viewer, const char *text);
static void virt_viewer_set_title(VirtViewer *viewer, gboolean grabbed);
static void virt_viewer_channel_open(VirtViewerSession *session,
				     VirtViewerSessionChannel *channel,
				     VirtViewer *viewer);
static void virt_viewer_quit(VirtViewer *viewer);

static void virt_viewer_simple_message_dialog(GtkWidget *window, const char *fmt, ...);


#if GTK_CHECK_VERSION(3, 0, 0)
#define GDK_Control_L GDK_KEY_Control_L
#define GDK_Alt_L GDK_KEY_Alt_L
#define GDK_Delete GDK_KEY_Delete
#define GDK_BackSpace GDK_KEY_BackSpace
#define GDK_Print GDK_KEY_Print
#define GDK_F1 GDK_KEY_F1
#define GDK_F2 GDK_KEY_F2
#define GDK_F3 GDK_KEY_F3
#define GDK_F4 GDK_KEY_F4
#define GDK_F5 GDK_KEY_F5
#define GDK_F6 GDK_KEY_F6
#define GDK_F7 GDK_KEY_F7
#define GDK_F8 GDK_KEY_F8
#define GDK_F9 GDK_KEY_F9
#define GDK_F10 GDK_KEY_F10
#define GDK_F11 GDK_KEY_F11
#define GDK_F12 GDK_KEY_F12
#endif

enum menuNums {
	FILE_MENU,
	VIEW_MENU,
	SEND_KEY_MENU,
	HELP_MENU,
	LAST_MENU // sentinel
};

struct _VirtViewer {
	char *uri;
	virConnectPtr conn;
	char *domkey;
	char *domtitle;

	GtkBuilder *builder;
	GtkWidget *window;
	GtkWidget *container;

	GtkWidget *notebook;
	GtkWidget *status;

	GtkWidget *toolbar;
	GtkWidget *layout;

	char *pretty_address;

	gboolean autoResize;
	gboolean fullscreen;
	gboolean withEvents;

	gboolean active;

	gboolean accelEnabled;
	GValue accelSetting;
	GSList *accelList;
	int accelMenuSig[LAST_MENU];

	gboolean waitvm;
	gboolean reconnect;
	gboolean direct;
	gboolean verbose;
	gboolean authretry;
	gboolean connected;

	gchar *clipboard;

	GtkWidget *display;
	VirtViewerSession *session;

	gint zoomlevel;

	char *unixsock;
	char *ghost;
	char *gport;
	char *host;
	char *transport;
	char *user;
        int port;
};

static const char * const menuNames[LAST_MENU] = {
	"menu-file", "menu-view", "menu-send", "menu-help"
};


#define MAX_KEY_COMBO 3
struct	keyComboDef {
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


static gboolean virt_viewer_connect_timer(void *opaque);
static int virt_viewer_initial_connect(VirtViewer *viewer);


void
virt_viewer_simple_message_dialog(GtkWidget *window,
				  const char *fmt, ...)
{
	GtkWidget *dialog;
	char *msg;
	va_list vargs;

	va_start(vargs, fmt);

	msg = g_strdup_vprintf(fmt, vargs);

	va_end(vargs);

	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
					GTK_DIALOG_MODAL |
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_OK,
					"%s",
					msg);

	gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	g_free(msg);
}


/*
 * This code attempts to resize the top level window to be large enough
 * to contain the entire display desktop at 1:1 ratio. If the local desktop
 * isn't large enough that it goes as large as possible and lets the display
 * scale down to fit, maintaining aspect ratio
 */
static void
virt_viewer_resize_main_window(VirtViewer *viewer)
{
	GdkRectangle fullscreen;
	GdkScreen *screen;
	int width, height;
	double desktopAspect;
	double screenAspect;
	guint desktopWidth;
	guint desktopHeight;

	DEBUG_LOG("Preparing main window resize");
	if (!viewer->active) {
		DEBUG_LOG("Skipping inactive resize");
		return;
	}

	gtk_window_resize(GTK_WINDOW(viewer->window), 1, 1);

	virt_viewer_display_get_desktop_size(VIRT_VIEWER_DISPLAY(viewer->display),
					     &desktopWidth, &desktopHeight);

	screen = gtk_widget_get_screen(viewer->window);
	gdk_screen_get_monitor_geometry(screen,
					gdk_screen_get_monitor_at_window
					(screen, gtk_widget_get_window(viewer->window)),
					&fullscreen);

	desktopAspect = (double)desktopWidth / (double)desktopHeight;
	screenAspect = (double)(fullscreen.width - 128) / (double)(fullscreen.height - 128);

	if ((desktopWidth > (fullscreen.width - 128)) ||
	    (desktopHeight > (fullscreen.height - 128))) {
		/* Doesn't fit native res, so go as large as possible
		   maintaining aspect ratio */
		if (screenAspect > desktopAspect) {
			width = desktopHeight * desktopAspect;
			height = desktopHeight;
		} else {
			width = desktopWidth;
			height = desktopWidth / desktopAspect;
		}
	} else {
		width = desktopWidth;
		height = desktopHeight;
	}

	DEBUG_LOG("Decided todo %dx%d (desktop is %dx%d, fullscreen is %dx%d",
		  width, height, desktopWidth, desktopHeight,
		  fullscreen.width, fullscreen.height);

	virt_viewer_display_set_desktop_size(VIRT_VIEWER_DISPLAY(viewer->display),
					     width, height);
}

static void
virt_viewer_desktop_resize(VirtViewerDisplay *display G_GNUC_UNUSED,
			   VirtViewer *viewer)
{
	if (viewer->autoResize && viewer->window && !viewer->fullscreen)
		virt_viewer_resize_main_window(viewer);
}


void
virt_viewer_menu_view_zoom_out(GtkWidget *menu G_GNUC_UNUSED,
			       VirtViewer *viewer)
{
	gtk_window_resize(GTK_WINDOW(viewer->window), 1, 1);
	if (viewer->zoomlevel > 10)
		viewer->zoomlevel -= 10;

	if (viewer->display)
		virt_viewer_display_set_zoom_level(VIRT_VIEWER_DISPLAY(viewer->display), viewer->zoomlevel);
}

void
virt_viewer_menu_view_zoom_in(GtkWidget *menu G_GNUC_UNUSED,
			      VirtViewer *viewer)
{
	gtk_window_resize(GTK_WINDOW(viewer->window), 1, 1);
	if (viewer->zoomlevel < 400)
		viewer->zoomlevel += 10;

	if (viewer->display)
		virt_viewer_display_set_zoom_level(VIRT_VIEWER_DISPLAY(viewer->display), viewer->zoomlevel);
}

void
virt_viewer_menu_view_zoom_reset(GtkWidget *menu G_GNUC_UNUSED,
				 VirtViewer *viewer)
{
	gtk_window_resize(GTK_WINDOW(viewer->window), 1, 1);
	viewer->zoomlevel = 100;

	if (viewer->display)
		virt_viewer_display_set_zoom_level(VIRT_VIEWER_DISPLAY(viewer->display), viewer->zoomlevel);
}

void
virt_viewer_set_title(VirtViewer *viewer,
		      gboolean grabbed)
{
	char *title;
	const char *subtitle;

	if (!viewer->window)
		return;

	if (grabbed)
		subtitle = "(Press Ctrl+Alt to release pointer) ";
	else
		subtitle = "";

	if (viewer->domtitle)
		title = g_strdup_printf("%s%s - Virt Viewer",
					subtitle, viewer->domtitle);
	else
		title = g_strdup("Virt Viewer");

	gtk_window_set_title(GTK_WINDOW(viewer->window), title);

	g_free(title);
}

static gboolean
virt_viewer_ignore_accel(GtkWidget *menu G_GNUC_UNUSED,
			 VirtViewer *viewer G_GNUC_UNUSED)
{
	/* ignore accelerator */
	return TRUE;
}


void
virt_viewer_disable_modifiers(VirtViewer *viewer)
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
		GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(viewer->builder, menuNames[i]));
		viewer->accelMenuSig[i] =
			g_signal_connect(menu, "mnemonic-activate",
					 G_CALLBACK(virt_viewer_ignore_accel), viewer);
	}

	viewer->accelEnabled = FALSE;
}


void
virt_viewer_enable_modifiers(VirtViewer *viewer)
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
		GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(viewer->builder, menuNames[i]));
		g_signal_handler_disconnect(menu, viewer->accelMenuSig[i]);
	}

	viewer->accelEnabled = TRUE;
}

void
virt_viewer_quit(VirtViewer *viewer)
{
	g_return_if_fail(viewer != NULL);

	if (viewer->session)
		virt_viewer_session_close(VIRT_VIEWER_SESSION(viewer->session));
	gtk_main_quit();
}

void
virt_viewer_delete(GtkWidget *src G_GNUC_UNUSED,
		   void *dummy G_GNUC_UNUSED,
		   VirtViewer *viewer)
{
	virt_viewer_quit(viewer);
}

void
virt_viewer_menu_file_quit(GtkWidget *src G_GNUC_UNUSED,
			   VirtViewer *viewer)
{
	virt_viewer_quit(viewer);
}


static void
virt_viewer_leave_fullscreen(VirtViewer *viewer)
{
	GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(viewer->builder, "top-menu"));
	if (!viewer->fullscreen)
		return;
	viewer->fullscreen = FALSE;
	ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(viewer->layout), FALSE);
	gtk_widget_show(menu);
	gtk_widget_hide(viewer->toolbar);
	gtk_window_unfullscreen(GTK_WINDOW(viewer->window));
	if (viewer->autoResize)
		virt_viewer_resize_main_window(viewer);
}

static void
virt_viewer_enter_fullscreen(VirtViewer *viewer)
{
	GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(viewer->builder, "top-menu"));
	if (viewer->fullscreen)
		return;
	viewer->fullscreen = TRUE;
	gtk_widget_hide(menu);
	gtk_window_fullscreen(GTK_WINDOW(viewer->window));
	gtk_widget_show(viewer->toolbar);
	ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(viewer->layout), TRUE);
}

static gboolean
window_state_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEventWindowState *event,
		gpointer data)
{
	VirtViewer *viewer = data;

	if (!(event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN))
		return TRUE;

	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
		virt_viewer_enter_fullscreen(viewer);
	else
		virt_viewer_leave_fullscreen(viewer);

	return TRUE;
}

static void
virt_viewer_toolbar_leave_fullscreen(GtkWidget *button G_GNUC_UNUSED,
				     VirtViewer *viewer)
{
	GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(viewer->builder, "menu-view-fullscreen"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu), FALSE);
	virt_viewer_leave_fullscreen(viewer);
}


void
virt_viewer_menu_view_fullscreen(GtkWidget *menu,
				 VirtViewer *viewer)
{
	if (!viewer->window)
		return;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu))) {
		virt_viewer_enter_fullscreen(viewer);
	} else {
		virt_viewer_leave_fullscreen(viewer);
	}
}

void
virt_viewer_menu_view_resize(GtkWidget *menu,
			     VirtViewer *viewer)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu))) {
		viewer->autoResize = TRUE;
		if (!viewer->fullscreen)
			virt_viewer_resize_main_window(viewer);
	} else {
		viewer->autoResize = FALSE;
	}
}

void
virt_viewer_menu_send(GtkWidget *menu G_GNUC_UNUSED,
		      VirtViewer *viewer)
{
	int i;
	GtkWidget *label = gtk_bin_get_child(GTK_BIN(menu));
	const char *text = gtk_label_get_label(GTK_LABEL(label));

	for (i = 0 ; i < G_N_ELEMENTS(keyCombos) ; i++) {
		if (!strcmp(text, keyCombos[i].label)) {
			DEBUG_LOG("Sending key combo %s", gtk_label_get_text(GTK_LABEL(label)));
			virt_viewer_display_send_keys(VIRT_VIEWER_DISPLAY(viewer->display),
						      keyCombos[i].keys,
						      keyCombos[i].nkeys);
			return;
		}
	}
	DEBUG_LOG("Failed to find key combo %s", gtk_label_get_text(GTK_LABEL(label)));
}


static void
virt_viewer_save_screenshot(VirtViewer *viewer,
			    const char *file)
{
	GdkPixbuf *pix = virt_viewer_display_get_pixbuf(VIRT_VIEWER_DISPLAY(viewer->display));
	gdk_pixbuf_save(pix, file, "png", NULL,
			"tEXt::Generator App", PACKAGE, NULL);
	gdk_pixbuf_unref(pix);
}


void
virt_viewer_menu_file_screenshot(GtkWidget *menu G_GNUC_UNUSED,
				 VirtViewer *viewer)
{
	GtkWidget *dialog;

	g_return_if_fail(viewer->display != NULL);

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
		virt_viewer_save_screenshot(viewer, filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}

void
virt_viewer_about_close(GtkWidget *dialog,
			VirtViewer *viewer G_GNUC_UNUSED)
{
	gtk_widget_hide(dialog);
	gtk_widget_destroy(dialog);
}

void
virt_viewer_about_delete(GtkWidget *dialog,
			 void *dummy G_GNUC_UNUSED,
			 VirtViewer *viewer G_GNUC_UNUSED)
{
	gtk_widget_hide(dialog);
	gtk_widget_destroy(dialog);
}

void
virt_viewer_menu_help_about(GtkWidget *menu G_GNUC_UNUSED,
			    VirtViewer *viewer)
{
	GtkBuilder *about = virt_viewer_util_load_ui("virt-viewer-about.xml");

	GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(about, "about"));
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), VERSION);

	gtk_builder_connect_signals(about, viewer);

	gtk_widget_show_all(dialog);

	g_object_unref(G_OBJECT(about));
}


static int
virt_viewer_parse_uuid(const char *name,
		       unsigned char *uuid)
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


static virDomainPtr
virt_viewer_lookup_domain(VirtViewer *viewer)
{
	char *end;
	int id = strtol(viewer->domkey, &end, 10);
	virDomainPtr dom = NULL;
	unsigned char uuid[16];

	if (id >= 0 && end && !*end) {
		dom = virDomainLookupByID(viewer->conn, id);
	}
	if (!dom && virt_viewer_parse_uuid(viewer->domkey, uuid) == 0) {
		dom = virDomainLookupByUUID(viewer->conn, uuid);
	}
	if (!dom) {
		dom = virDomainLookupByName(viewer->conn, viewer->domkey);
	}
	return dom;
}

static int
virt_viewer_matches_domain(VirtViewer *viewer,
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
	if (virt_viewer_parse_uuid(viewer->domkey, wantuuid) == 0) {
		virDomainGetUUID(dom, domuuid);
		if (memcmp(wantuuid, domuuid, VIR_UUID_BUFLEN) == 0)
			return 1;
	}

	name = virDomainGetName(dom);
	if (strcmp(name, viewer->domkey) == 0)
		return 1;

	return 0;
}

static char *
virt_viewer_extract_xpath_string(const gchar *xmldesc,
				 const gchar *xpath)
{
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
	if (!xml)
		goto error;

	ctxt = xmlXPathNewContext(xml);
	if (!ctxt)
		goto error;

	obj = xmlXPathEval((const xmlChar *)xpath, ctxt);
	if (!obj || obj->type != XPATH_STRING || !obj->stringval || !obj->stringval[0])
		goto error;
	if (!strcmp((const char*)obj->stringval, "-1"))
		goto error;

	port = g_strdup((const char*)obj->stringval);
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


static int
virt_viewer_extract_host(const char *uristr,
			 char **host,
			 char **transport,
			 char **user,
			 int *port)
{
	xmlURIPtr uri;
	char *offset;

	*host = NULL;
	*transport = NULL;
	*user = NULL;

	if (uristr == NULL ||
	    !g_strcasecmp(uristr, "xen"))
		uristr = "xen:///";

	uri = xmlParseURI(uristr);
	if (!uri || !uri->server) {
		*host = g_strdup("localhost");
	} else {
		*host = g_strdup(uri->server);
	}

	if (uri->user)
		*user = g_strdup(uri->user);
	*port = uri->port;

	offset = strchr(uri->scheme, '+');
	if (offset)
		*transport = g_strdup(offset+1);

	xmlFreeURI(uri);
	return 0;
}

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)

static int
virt_viewer_open_tunnel(const char **cmd)
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


static int
virt_viewer_open_tunnel_ssh(const char *sshhost,
			    int sshport,
			    const char *sshuser,
			    const char *host,
			    const char *port,
			    const char *unixsock)
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
	if (port) {
		cmd[n++] = host;
		cmd[n++] = port;
	} else {
		cmd[n++] = "-U";
		cmd[n++] = unixsock;
	}
	cmd[n++] = NULL;

	return virt_viewer_open_tunnel(cmd);
}

static int
virt_viewer_open_unix_sock(const char *unixsock)
{
	struct sockaddr_un addr;
	int fd;

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, unixsock);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;

	if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

#endif /* defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK) */

static void
virt_viewer_trace(VirtViewer *viewer,
		  const char *fmt, ...)
{
	va_list ap;

	if (doDebug) {
		va_start(ap, fmt);
		g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, fmt, ap);
		va_end(ap);
	}

	if (viewer->verbose) {
		va_start(ap, fmt);
		g_vprintf(fmt, ap);
		va_end(ap);
	}
}


void
virt_viewer_set_status(VirtViewer *viewer,
		       const char *text)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(viewer->notebook), 0);
	gtk_label_set_text(GTK_LABEL(viewer->status), text);
}


static void
virt_viewer_show_display(VirtViewer *viewer)
{
	g_return_if_fail(viewer != NULL);
	g_return_if_fail(viewer->display != NULL);

	gtk_widget_grab_focus(gtk_bin_get_child(GTK_BIN(viewer->display)));
	gtk_notebook_set_current_page(GTK_NOTEBOOK(viewer->notebook), 1);
}

static void
virt_viewer_pointer_grab(VirtViewerDisplay *display G_GNUC_UNUSED,
			 VirtViewer *viewer)
{
	virt_viewer_set_title(viewer, TRUE);
}

static void
virt_viewer_pointer_ungrab(VirtViewerDisplay *display G_GNUC_UNUSED,
			   VirtViewer *viewer)
{
	virt_viewer_set_title(viewer, FALSE);
}

static void
virt_viewer_keyboard_grab(VirtViewerDisplay *display G_GNUC_UNUSED,
			  VirtViewer *viewer)
{
	virt_viewer_disable_modifiers(viewer);
}

static void
virt_viewer_keyboard_ungrab(VirtViewerDisplay *display G_GNUC_UNUSED,
			    VirtViewer *viewer)
{
	virt_viewer_enable_modifiers(viewer);
}


static void
virt_viewer_display_added(VirtViewerSession *session G_GNUC_UNUSED,
			  VirtViewerDisplay *display,
			  VirtViewer *viewer)
{
	/* XXX multihead */
	if (viewer->display)
		return;
	viewer->display = GTK_WIDGET(display);

	g_signal_connect(viewer->display, "display-pointer-grab",
			 G_CALLBACK(virt_viewer_pointer_grab), viewer);
	g_signal_connect(viewer->display, "display-pointer-ungrab",
			 G_CALLBACK(virt_viewer_pointer_ungrab), viewer);
	g_signal_connect(viewer->display, "display-keyboard-grab",
			 G_CALLBACK(virt_viewer_keyboard_grab), viewer);
	g_signal_connect(viewer->display, "display-keyboard-ungrab",
			 G_CALLBACK(virt_viewer_keyboard_ungrab), viewer);

	g_signal_connect(viewer->display, "display-desktop-resize",
			 G_CALLBACK(virt_viewer_desktop_resize), viewer);

	gtk_notebook_append_page(GTK_NOTEBOOK(viewer->notebook), viewer->display, NULL);
	if (gtk_bin_get_child(GTK_BIN(viewer->display)))
		gtk_widget_realize(GTK_WIDGET(gtk_bin_get_child(GTK_BIN(viewer->display))));
	gtk_widget_show_all(viewer->display);
}

static void
virt_viewer_display_removed(VirtViewerSession *session G_GNUC_UNUSED,
			    VirtViewerDisplay *display,
			    VirtViewer *viewer)
{
	/* XXX multihead */
	if (viewer->display != GTK_WIDGET(display))
		return;

	gtk_widget_hide(GTK_WIDGET(display));
	gtk_notebook_remove_page(GTK_NOTEBOOK(viewer->notebook), 1);
	viewer->display = NULL;
}

static void
virt_viewer_connect_info_free(VirtViewer *viewer)
{
	free(viewer->host);
	free(viewer->ghost);
	free(viewer->gport);
	free(viewer->transport);
	free(viewer->user);
	free(viewer->unixsock);

	viewer->host = NULL;
	viewer->ghost = NULL;
	viewer->gport = NULL;
	viewer->transport = NULL;
	viewer->user = NULL;
	viewer->unixsock = NULL;
	viewer->port = 0;
}

static gboolean
virt_viewer_extract_connect_info(VirtViewer *viewer,
				 virDomainPtr dom)
{
	char *type = NULL;
	char *xpath = NULL;
	gboolean retval = FALSE;
	char *xmldesc = virDomainGetXMLDesc(dom, 0);

	virt_viewer_connect_info_free(viewer);

	if ((type = virt_viewer_extract_xpath_string(xmldesc, "string(/domain/devices/graphics/@type)")) == NULL) {
		virt_viewer_simple_message_dialog(viewer->window, _("Cannot determine the graphic type for the guest %s"),
					     viewer->domkey);
		goto cleanup;
	}

	if (g_strcasecmp(type, "vnc") == 0) {
		virt_viewer_trace(viewer, "Guest %s has a %s display\n",
			     viewer->domkey, type);
		viewer->session = virt_viewer_session_vnc_new();
#ifdef HAVE_SPICE_GTK
	} else if (g_strcasecmp(type, "spice") == 0) {
		virt_viewer_trace(viewer, "Guest %s has a %s display\n",
			     viewer->domkey, type);
		viewer->session = virt_viewer_session_spice_new();
#endif
	} else {
		virt_viewer_trace(viewer, "Guest %s has unsupported %s display type\n",
			     viewer->domkey, type);
		virt_viewer_simple_message_dialog(viewer->window, _("Unknown graphic type for the guest %s"),
					     viewer->domkey);
		goto cleanup;
	}

	g_signal_connect(viewer->session, "session-initialized",
			 G_CALLBACK(virt_viewer_initialized), viewer);
	g_signal_connect(viewer->session, "session-connected",
			 G_CALLBACK(virt_viewer_connected), viewer);
	g_signal_connect(viewer->session, "session-disconnected",
			 G_CALLBACK(virt_viewer_disconnected), viewer);
	g_signal_connect(viewer->session, "session-channel-open",
			 G_CALLBACK(virt_viewer_channel_open), viewer);
	g_signal_connect(viewer->session, "session-auth-refused",
			 G_CALLBACK(virt_viewer_auth_refused), viewer);
	g_signal_connect(viewer->session, "session-auth-failed",
			 G_CALLBACK(virt_viewer_auth_failed), viewer);
	g_signal_connect(viewer->session, "session-display-added",
			 G_CALLBACK(virt_viewer_display_added), viewer);
	g_signal_connect(viewer->session, "session-display-removed",
			 G_CALLBACK(virt_viewer_display_removed), viewer);

	g_signal_connect(viewer->session, "session-cut-text",
			 G_CALLBACK(virt_viewer_server_cut_text), viewer);
	g_signal_connect(viewer->session, "session-bell",
			 G_CALLBACK(virt_viewer_bell), viewer);

	xpath = g_strdup_printf("string(/domain/devices/graphics[@type='%s']/@port)", type);
	if ((viewer->gport = virt_viewer_extract_xpath_string(xmldesc, xpath)) == NULL) {
		free(xpath);
		xpath = g_strdup_printf("string(/domain/devices/graphics[@type='%s']/@socket)", type);
		if ((viewer->unixsock = virt_viewer_extract_xpath_string(xmldesc, xpath)) == NULL) {
			virt_viewer_simple_message_dialog(viewer->window, _("Cannot determine the graphic address for the guest %s"),
						     viewer->domkey);
			goto cleanup;
		}
	} else {
		free(xpath);
		xpath = g_strdup_printf("string(/domain/devices/graphics[@type='%s']/@listen)", type);
		viewer->ghost = virt_viewer_extract_xpath_string(xmldesc, xpath);
		if (viewer->ghost == NULL)
			viewer->ghost = g_strdup("localhost");
	}

	if (viewer->gport)
		DEBUG_LOG("Guest graphics address is %s:%s", viewer->ghost, viewer->gport);
	else
		DEBUG_LOG("Guest graphics address is %s", viewer->unixsock);

	if (virt_viewer_extract_host(viewer->uri, &viewer->host, &viewer->transport, &viewer->user, &viewer->port) < 0) {
		virt_viewer_simple_message_dialog(viewer->window, _("Cannot determine the host for the guest %s"),
					     viewer->domkey);
		goto cleanup;
	}

	retval = TRUE;

cleanup:
	g_free(type);
	g_free(xpath);
	g_free(xmldesc);
	return retval;
}

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)
static void
virt_viewer_channel_open(VirtViewerSession *session,
			 VirtViewerSessionChannel *channel,
			 VirtViewer *viewer)
{
	int fd = -1;

	g_return_if_fail(viewer != NULL);

	if (viewer->transport && g_strcasecmp(viewer->transport, "ssh") == 0 &&
	    !viewer->direct) {
		if ((fd = virt_viewer_open_tunnel_ssh(viewer->host, viewer->port, viewer->user,
						      viewer->ghost, viewer->gport, NULL)) < 0)
			virt_viewer_simple_message_dialog(viewer->window, _("Connect to ssh failed."));
	} else {
		virt_viewer_simple_message_dialog(viewer->window, _("Can't connect to channel, SSH only supported."));
	}

	if (fd >= 0)
		virt_viewer_session_channel_open_fd(session, channel, fd);
}
#else
static void
virt_viewer_channel_open(VirtViewerSession *session G_GNUC_UNUSED,
			 VirtViewerSessionChannel *channel G_GNUC_UNUSED,
			 VirtViewer *viewer)
{
	virt_viewer_simple_message_dialog(viewer->window, _("Connect to channel unsupported."));
}
#endif

static int
virt_viewer_activate(VirtViewer *viewer,
		     virDomainPtr dom)
{
	int fd = -1;
	int ret = -1;

	if (viewer->active)
		goto cleanup;

	virt_viewer_trace(viewer, "Guest %s is running, determining display\n",
			  viewer->domkey);
	if (viewer->session == NULL) {
		if (!virt_viewer_extract_connect_info(viewer, dom))
			goto cleanup;

		if (viewer->gport)
			viewer->pretty_address = g_strdup_printf("%s:%s", viewer->ghost, viewer->gport);
		else
			viewer->pretty_address = g_strdup_printf("%s:%s", viewer->host, viewer->unixsock);
	}

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)
	if (viewer->transport &&
	    g_strcasecmp(viewer->transport, "ssh") == 0 &&
	    !viewer->direct) {
		if (viewer->gport) {
			virt_viewer_trace(viewer, "Opening indirect TCP connection to display at %s:%s\n",
				     viewer->ghost, viewer->gport);
		} else {
			virt_viewer_trace(viewer, "Opening indirect UNIX connection to display at %s\n",
				     viewer->unixsock);
		}
		virt_viewer_trace(viewer, "Setting up SSH tunnel via %s@%s:%d\n",
			     viewer->user, viewer->host, viewer->port ? viewer->port : 22);

		if ((fd = virt_viewer_open_tunnel_ssh(viewer->host, viewer->port,
						 viewer->user, viewer->ghost,
						 viewer->gport, viewer->unixsock)) < 0)
			return -1;
	} else if (viewer->unixsock) {
		virt_viewer_trace(viewer, "Opening direct UNIX connection to display at %s",
			     viewer->unixsock);
		if ((fd = virt_viewer_open_unix_sock(viewer->unixsock)) < 0)
			return -1;
	}
#endif

	if (fd >= 0) {
		ret = virt_viewer_session_open_fd(VIRT_VIEWER_SESSION(viewer->session), fd);
	} else {
		virt_viewer_trace(viewer, "Opening direct TCP connection to display at %s:%s\n",
			     viewer->ghost, viewer->gport);
		ret = virt_viewer_session_open_host(VIRT_VIEWER_SESSION(viewer->session),
						    viewer->ghost, viewer->gport);
	}

	virt_viewer_set_status(viewer, "Connecting to graphic server");

	viewer->connected = FALSE;
	viewer->active = TRUE;
	virt_viewer_set_title(viewer, FALSE);

cleanup:
	return ret;
}

/* text was actually requested */
static void
virt_viewer_clipboard_copy(GtkClipboard *clipboard G_GNUC_UNUSED,
			   GtkSelectionData *data,
			   guint info G_GNUC_UNUSED,
			   VirtViewer *viewer)
{
	gtk_selection_data_set_text(data, viewer->clipboard, -1);
}

static void
virt_viewer_server_cut_text(VirtViewerSession *session G_GNUC_UNUSED,
			    const gchar *text,
			    VirtViewer *viewer)
{
	GtkClipboard *cb;
	gsize a, b;
	GtkTargetEntry targets[] = {
		{g_strdup("UTF8_STRING"), 0, 0},
		{g_strdup("COMPOUND_TEXT"), 0, 0},
		{g_strdup("TEXT"), 0, 0},
		{g_strdup("STRING"), 0, 0},
	};

	if (!text)
		return;

	g_free (viewer->clipboard);
	viewer->clipboard = g_convert (text, -1, "utf-8", "iso8859-1", &a, &b, NULL);

	if (viewer->clipboard) {
		cb = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

		gtk_clipboard_set_with_owner (cb,
					      targets,
					      G_N_ELEMENTS(targets),
					      (GtkClipboardGetFunc)virt_viewer_clipboard_copy,
					      NULL,
					      G_OBJECT (viewer));
	}
}


static void virt_viewer_bell(VirtViewerSession *session G_GNUC_UNUSED,
			     VirtViewer *viewer)
{
	gdk_window_beep(gtk_widget_get_window(GTK_WIDGET(viewer->window)));
}


static gboolean
virt_viewer_retryauth(gpointer opaque)
{
	VirtViewer *viewer = opaque;
	virt_viewer_initial_connect(viewer);

	return FALSE;
}

static void
virt_viewer_deactivate(VirtViewer *viewer)
{
	if (!viewer->active)
		return;

	if (viewer->session)
		virt_viewer_session_close(VIRT_VIEWER_SESSION(viewer->session));

	viewer->connected = FALSE;
	viewer->active = FALSE;
#if 0
	g_free(viewer->pretty_address);
	viewer->pretty_address = NULL;
#endif
	virt_viewer_set_title(viewer, FALSE);

	if (viewer->authretry) {
		viewer->authretry = FALSE;
		g_idle_add(virt_viewer_retryauth, viewer);
	} else if (viewer->reconnect) {
		if (!viewer->withEvents) {
			DEBUG_LOG("No domain events, falling back to polling");
			g_timeout_add(500,
				      virt_viewer_connect_timer,
				      viewer);
		}

		virt_viewer_set_status(viewer, "Waiting for guest domain to re-start");
		virt_viewer_trace(viewer, "Guest %s display has disconnected, waiting to reconnect",
			     viewer->domkey);
	} else {
		virt_viewer_set_status(viewer, "Guest domain has shutdown");
		virt_viewer_trace(viewer, "Guest %s display has disconnected, shutting down",
			     viewer->domkey);
		gtk_main_quit();
	}
}

static void
virt_viewer_connected(VirtViewerSession *session G_GNUC_UNUSED,
		      VirtViewer *viewer)
{
	viewer->connected = TRUE;
	virt_viewer_set_status(viewer, "Connected to graphic server");
}

static void
virt_viewer_initialized(VirtViewerSession *session G_GNUC_UNUSED,
			VirtViewer *viewer)
{
	virt_viewer_show_display(viewer);
	virt_viewer_set_title(viewer, FALSE);
}

static void
virt_viewer_disconnected(VirtViewerSession *session G_GNUC_UNUSED,
			 VirtViewer *viewer)
{
	if (!viewer->connected) {
		virt_viewer_simple_message_dialog(viewer->window,
						  _("Unable to connect to the graphic server %s"),
						  viewer->pretty_address);
	}
	virt_viewer_deactivate(viewer);
}


static void virt_viewer_auth_refused(VirtViewerSession *session G_GNUC_UNUSED,
				     const char *msg,
				     VirtViewer *viewer)
{
	GtkWidget *dialog;
	int ret;

	dialog = gtk_message_dialog_new(GTK_WINDOW(viewer->window),
					GTK_DIALOG_MODAL |
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_YES_NO,
					_("Unable to authenticate with remote desktop server at %s: %s\n"
					  "Retry connection again?"),
					viewer->pretty_address, msg);

	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	if (ret == GTK_RESPONSE_YES)
		viewer->authretry = TRUE;
	else
		viewer->authretry = FALSE;
}


static void virt_viewer_auth_failed(VirtViewerSession *session G_GNUC_UNUSED,
				    const char *msg,
				    VirtViewer *viewer)
{
	virt_viewer_simple_message_dialog(viewer->window,
					  _("Unable to authenticate with remote desktop server at %s"),
					  viewer->pretty_address, msg);
}


static int
virt_viewer_domain_event(virConnectPtr conn G_GNUC_UNUSED,
			 virDomainPtr dom,
			 int event,
			 int detail G_GNUC_UNUSED,
			 void *opaque)
{
	VirtViewer *viewer = opaque;

	DEBUG_LOG("Got domain event %d %d", event, detail);

	if (!virt_viewer_matches_domain(viewer, dom))
		return 0;
	
	switch (event) {
	case VIR_DOMAIN_EVENT_STOPPED:
		//virt_viewer_deactivate(viewer);
		break;

	case VIR_DOMAIN_EVENT_STARTED:
		virt_viewer_activate(viewer, dom);
		break;
	}

	return 0;
}


static int
virt_viewer_initial_connect(VirtViewer *viewer)
{
	virDomainPtr dom = NULL;
	virDomainInfo info;
	int ret = -1;

	virt_viewer_set_status(viewer, "Finding guest domain");
	dom = virt_viewer_lookup_domain(viewer);
	if (!dom) {
		if (viewer->waitvm) {
			virt_viewer_set_status(viewer, "Waiting for guest domain to be created");
			virt_viewer_trace(viewer, "Guest %s does not yet exist, waiting for it to be created\n",
				     viewer->domkey);
			goto done;
		} else {
			virt_viewer_simple_message_dialog(viewer->window, _("Cannot find guest domain %s"),
						     viewer->domkey);
			DEBUG_LOG("Cannot find guest %s", viewer->domkey);
			goto cleanup;
		}
	}

	free(viewer->domtitle);
	viewer->domtitle = g_strdup(virDomainGetName(dom));

	virt_viewer_set_status(viewer, "Checking guest domain status");
	if (virDomainGetInfo(dom, &info) < 0) {
		DEBUG_LOG("Cannot get guest state");
		goto cleanup;
	}

	if (info.state == VIR_DOMAIN_SHUTOFF) {
		virt_viewer_set_status(viewer, "Waiting for guest domain to start");
	} else {
		ret = virt_viewer_activate(viewer, dom);
		if (ret < 0) {
			if (viewer->waitvm) {
				virt_viewer_set_status(viewer, "Waiting for guest domain to start server");
				virt_viewer_trace(viewer, "Guest %s has not activated its display yet, waiting for it to start\n",
					     viewer->domkey);
			} else {
				DEBUG_LOG("Failed to activate viewer");
				goto cleanup;
			}
		} else if (ret == 0) {
			DEBUG_LOG("Failed to activate viewer");
			ret = -1;
			goto cleanup;
		}
	}

 done:
	ret = 0;
 cleanup:
	if (dom)
		virDomainFree(dom);
	return ret;
}

static gboolean
virt_viewer_connect_timer(void *opaque)
{
	VirtViewer *viewer = opaque;

	DEBUG_LOG("Connect timer fired");

	if (!viewer->active &&
	    virt_viewer_initial_connect(viewer) < 0)
		gtk_main_quit();

	if (viewer->active)
		return FALSE;

	return TRUE;
}

static void
virt_viewer_toolbar_setup(VirtViewer *viewer)
{
	GtkWidget *button;

	viewer->toolbar = gtk_toolbar_new();
	gtk_toolbar_set_show_arrow(GTK_TOOLBAR(viewer->toolbar), FALSE);
	gtk_widget_set_no_show_all(viewer->toolbar, TRUE);
	gtk_toolbar_set_style(GTK_TOOLBAR(viewer->toolbar), GTK_TOOLBAR_BOTH_HORIZ);

	/* Close connection */
	button = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_CLOSE));
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(button), _("Disconnect"));
	gtk_widget_show(GTK_WIDGET(button));
	gtk_toolbar_insert(GTK_TOOLBAR(viewer->toolbar), GTK_TOOL_ITEM (button), 0);
	g_signal_connect(button, "clicked", G_CALLBACK(gtk_main_quit), NULL);

	/* Leave fullscreen */
	button = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_LEAVE_FULLSCREEN));
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(button), _("Leave fullscreen"));
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(button), _("Leave fullscreen"));
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(button), TRUE);
	gtk_widget_show(GTK_WIDGET(button));
	gtk_toolbar_insert(GTK_TOOLBAR(viewer->toolbar), GTK_TOOL_ITEM(button), 0);
	g_signal_connect(button, "clicked", G_CALLBACK(virt_viewer_toolbar_leave_fullscreen), viewer);

	viewer->layout = ViewAutoDrawer_New();

	ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(viewer->layout), FALSE);
	ViewOvBox_SetOver(VIEW_OV_BOX(viewer->layout), viewer->toolbar);
	ViewOvBox_SetUnder(VIEW_OV_BOX(viewer->layout), viewer->notebook);
	ViewAutoDrawer_SetOffset(VIEW_AUTODRAWER(viewer->layout), -1);
	ViewAutoDrawer_SetFill(VIEW_AUTODRAWER(viewer->layout), FALSE);
	ViewAutoDrawer_SetOverlapPixels(VIEW_AUTODRAWER(viewer->layout), 1);
	ViewAutoDrawer_SetNoOverlapPixels(VIEW_AUTODRAWER(viewer->layout), 0);
}


static void
virt_viewer_error_func (void *data G_GNUC_UNUSED,
			virErrorPtr error G_GNUC_UNUSED)
{
	/* nada */
}

int
virt_viewer_start(const char *uri,
		  const char *name,
		  gint zoom,
		  gboolean direct,
		  gboolean waitvm,
		  gboolean reconnect,
		  gboolean verbose,
		  gboolean debug,
		  gboolean fullscreen,
		  GtkWidget *container)
{
	VirtViewer *viewer;
	GtkWidget *menu;
	int cred_types[] =
		{ VIR_CRED_AUTHNAME, VIR_CRED_PASSPHRASE };
	virConnectAuth auth_libvirt = {
		.credtype = cred_types,
		.ncredtype = ARRAY_CARDINALITY(cred_types),
		.cb = virt_viewer_auth_libvirt_credentials,
		.cbdata = (void *)uri,
	};
	GdkColor color;
	doDebug = debug;

	viewer = g_new0(VirtViewer, 1);

	viewer->active = FALSE;
	viewer->autoResize = TRUE;
	viewer->direct = direct;
	viewer->waitvm = waitvm;
	viewer->reconnect = reconnect;
	viewer->verbose = verbose;
	viewer->domkey = g_strdup(name);
	viewer->uri = g_strdup(uri);

	g_value_init(&viewer->accelSetting, G_TYPE_STRING);

	virt_viewer_events_register();

	virSetErrorFunc(NULL, virt_viewer_error_func);

	virt_viewer_trace(viewer, "Opening connection to libvirt with URI %s\n",
		     uri ? uri : "<null>");
	viewer->conn = virConnectOpenAuth(uri,
					  //virConnectAuthPtrDefault,
					  &auth_libvirt,
					  VIR_CONNECT_RO);
	if (!viewer->conn) {
		virt_viewer_simple_message_dialog(NULL, _("Unable to connect to libvirt with URI %s"),
					     uri ? uri : _("[none]"));
		return -1;
	}

	if (!container) {
		viewer->builder = virt_viewer_util_load_ui("virt-viewer.xml");

		menu = GTK_WIDGET(gtk_builder_get_object(viewer->builder, "menu-view-resize"));
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu), TRUE);

		gtk_builder_connect_signals(viewer->builder, viewer);
	}

	viewer->status = gtk_label_new("");

	viewer->zoomlevel = zoom;

	viewer->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(viewer->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(viewer->notebook), FALSE);

	gtk_notebook_append_page(GTK_NOTEBOOK(viewer->notebook), viewer->status, NULL);

	if (container) {
		viewer->container = container;

		gtk_box_pack_end(GTK_BOX(container), viewer->notebook, TRUE, TRUE, 0);
		gtk_widget_show_all(GTK_WIDGET(container));
	} else {
		GtkWidget *vbox = GTK_WIDGET(gtk_builder_get_object(viewer->builder, "viewer-box"));
		virt_viewer_toolbar_setup(viewer);

		gtk_box_pack_end(GTK_BOX(vbox), viewer->layout, TRUE, TRUE, 0);
		gdk_color_parse("black", &color);
		gtk_widget_modify_bg(viewer->layout, GTK_STATE_NORMAL, &color);
		gdk_color_parse("white", &color);
		gtk_widget_modify_fg(viewer->status, GTK_STATE_NORMAL, &color);

		GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(viewer->builder, "viewer"));
		GSList *accels;
		viewer->container = window;
		viewer->window = window;
		virt_viewer_set_title(viewer, FALSE);
		gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
		gtk_window_set_has_resize_grip(GTK_WINDOW(window), FALSE);
#endif
		viewer->accelEnabled = TRUE;
		accels = gtk_accel_groups_from_object(G_OBJECT(window));
		for ( ; accels ; accels = accels->next) {
			viewer->accelList = g_slist_append(viewer->accelList, accels->data);
			g_object_ref(G_OBJECT(accels->data));
		}
		g_signal_connect(G_OBJECT(window), "window-state-event", G_CALLBACK(window_state_cb), viewer);
		if (fullscreen)
			gtk_window_fullscreen(GTK_WINDOW(window));
		gtk_widget_show_all(viewer->window);
	}

	if (virt_viewer_initial_connect(viewer) < 0)
		return -1;

	if (virConnectDomainEventRegister(viewer->conn,
					  virt_viewer_domain_event,
					  viewer,
					  NULL) < 0)
		viewer->withEvents = FALSE;
	else
		viewer->withEvents = TRUE;

	if (!viewer->withEvents &&
	    !viewer->active) {
		DEBUG_LOG("No domain events, falling back to polling");
		g_timeout_add(500,
			      virt_viewer_connect_timer,
			      viewer);
	}

	return 0;
}


/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 *  indent-tabs-mode: t
 * End:
 */
