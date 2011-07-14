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

#include "virt-viewer-app.h"
#include "virt-viewer-auth.h"
#include "virt-viewer-session.h"
#include "virt-viewer-session-vnc.h"
#ifdef HAVE_SPICE_GTK
#include "virt-viewer-session-spice.h"
#endif

#include "view/autoDrawer.h"

gboolean doDebug = FALSE;

/* Signal handlers for main window */
void virt_viewer_app_menu_view_zoom_out(GtkWidget *menu, VirtViewerApp *self);
void virt_viewer_app_menu_view_zoom_in(GtkWidget *menu, VirtViewerApp *self);
void virt_viewer_app_menu_view_zoom_reset(GtkWidget *menu, VirtViewerApp *self);
void virt_viewer_app_delete(GtkWidget *src, void *dummy, VirtViewerApp *self);
void virt_viewer_app_menu_file_quit(GtkWidget *src, VirtViewerApp *self);
void virt_viewer_app_menu_help_about(GtkWidget *menu, VirtViewerApp *self);
void virt_viewer_app_menu_view_fullscreen(GtkWidget *menu, VirtViewerApp *self);
void virt_viewer_app_menu_view_resize(GtkWidget *menu, VirtViewerApp *self);
void virt_viewer_app_menu_send(GtkWidget *menu, VirtViewerApp *self);
void virt_viewer_app_menu_file_screenshot(GtkWidget *menu, VirtViewerApp *self);

/* Signal handlers for about dialog */
void virt_viewer_app_about_close(GtkWidget *dialog, VirtViewerApp *self);
void virt_viewer_app_about_delete(GtkWidget *dialog, void *dummy, VirtViewerApp *self);


/* Internal methods */
static void virt_viewer_app_connected(VirtViewerSession *session,
				      VirtViewerApp *self);
static void virt_viewer_app_initialized(VirtViewerSession *session,
					VirtViewerApp *self);
static void virt_viewer_app_disconnected(VirtViewerSession *session,
					 VirtViewerApp *self);
static void virt_viewer_app_auth_refused(VirtViewerSession *session,
					 const char *msg,
					 VirtViewerApp *self);
static void virt_viewer_app_auth_failed(VirtViewerSession *session,
					const char *msg,
					VirtViewerApp *self);

static void virt_viewer_app_server_cut_text(VirtViewerSession *session,
					    const gchar *text,
					    VirtViewerApp *self);
static void virt_viewer_app_bell(VirtViewerSession *session,
				 VirtViewerApp *self);
static void virt_viewer_app_enable_modifiers(VirtViewerApp *self);
static void virt_viewer_app_disable_modifiers(VirtViewerApp *self);
static void virt_viewer_app_resize_main_window(VirtViewerApp *self);

static void virt_viewer_app_update_title(VirtViewerApp *self);
static void virt_viewer_app_channel_open(VirtViewerSession *session,
					 VirtViewerSessionChannel *channel,
					 VirtViewerApp *self);
static void virt_viewer_app_quit(VirtViewerApp *self);
static void virt_viewer_app_update_pretty_address(VirtViewerApp *self);


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

struct _VirtViewerAppPrivate {
	GtkBuilder *builder;
	GtkWidget *window;
	GtkWidget *container;
	GtkWidget *notebook;
	GtkWidget *status;
	GtkWidget *toolbar;
	GtkWidget *layout;
	GtkWidget *display;

	gboolean accelEnabled;
	GValue accelSetting;
	GSList *accelList;
	int accelMenuSig[LAST_MENU];
	gchar *clipboard;

	gint zoomlevel;
	gboolean autoResize;
	gboolean fullscreen;
	gboolean direct;
	gboolean verbose;
	gboolean authretry;
	gboolean started;

	VirtViewerSession *session;
	gboolean active;
	gboolean connected;
	guint reconnect_poll; /* source id */
	char *unixsock;
	char *ghost;
	char *gport;
	char *host; /* ssh */
        int port;/* ssh */
	char *user; /* ssh */
	char *transport;
	char *pretty_address;
	gchar *subtitle;
	gchar *guest_name;
	gboolean grabbed;
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


G_DEFINE_ABSTRACT_TYPE(VirtViewerApp, virt_viewer_app, G_TYPE_OBJECT)
#define GET_PRIVATE(o)							\
        (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIRT_VIEWER_TYPE_APP, VirtViewerAppPrivate))

enum {
	PROP_0,
	PROP_SUBTITLE,
	PROP_VERBOSE,
	PROP_CONTAINER,
	PROP_SESSION,
	PROP_GUEST_NAME,
};

void
virt_viewer_app_set_debug(gboolean debug)
{
	doDebug = debug;
}

void
virt_viewer_app_simple_message_dialog(VirtViewerApp *self,
				      const char *fmt, ...)
{
	g_return_if_fail(VIRT_VIEWER_IS_APP(self));
	GtkWindow *window = GTK_WINDOW(self->priv->window);
	GtkWidget *dialog;
	char *msg;
	va_list vargs;

	va_start(vargs, fmt);

	msg = g_strdup_vprintf(fmt, vargs);

	va_end(vargs);

	dialog = gtk_message_dialog_new(window,
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
virt_viewer_app_resize_main_window(VirtViewerApp *self)
{
	GdkRectangle fullscreen;
	GdkScreen *screen;
	int width, height;
	double desktopAspect;
	double screenAspect;
	guint desktopWidth;
	guint desktopHeight;
	VirtViewerAppPrivate *priv = self->priv;

	DEBUG_LOG("Preparing main window resize");
	if (!priv->active) {
		DEBUG_LOG("Skipping inactive resize");
		return;
	}

	gtk_window_resize(GTK_WINDOW(priv->window), 1, 1);

	virt_viewer_display_get_desktop_size(VIRT_VIEWER_DISPLAY(priv->display),
					     &desktopWidth, &desktopHeight);

	screen = gtk_widget_get_screen(priv->window);
	gdk_screen_get_monitor_geometry(screen,
					gdk_screen_get_monitor_at_window
					(screen, gtk_widget_get_window(priv->window)),
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

	virt_viewer_display_set_desktop_size(VIRT_VIEWER_DISPLAY(priv->display),
					     width, height);
}

static void
virt_viewer_app_desktop_resize(VirtViewerDisplay *display G_GNUC_UNUSED,
			       VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;
	if (priv->autoResize && priv->window && !priv->fullscreen)
		virt_viewer_app_resize_main_window(self);
}


void
virt_viewer_app_menu_view_zoom_out(GtkWidget *menu G_GNUC_UNUSED,
				   VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;
	gtk_window_resize(GTK_WINDOW(priv->window), 1, 1);
	if (priv->zoomlevel > 10)
		priv->zoomlevel -= 10;

	if (priv->display)
		virt_viewer_display_set_zoom_level(VIRT_VIEWER_DISPLAY(priv->display), priv->zoomlevel);
}

void
virt_viewer_app_menu_view_zoom_in(GtkWidget *menu G_GNUC_UNUSED,
				  VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;
	gtk_window_resize(GTK_WINDOW(priv->window), 1, 1);
	if (priv->zoomlevel < 400)
		priv->zoomlevel += 10;

	if (priv->display)
		virt_viewer_display_set_zoom_level(VIRT_VIEWER_DISPLAY(priv->display), priv->zoomlevel);
}

void
virt_viewer_app_menu_view_zoom_reset(GtkWidget *menu G_GNUC_UNUSED,
				     VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;
	gtk_window_resize(GTK_WINDOW(priv->window), 1, 1);
	priv->zoomlevel = 100;

	if (priv->display)
		virt_viewer_display_set_zoom_level(VIRT_VIEWER_DISPLAY(priv->display), priv->zoomlevel);
}

void
virt_viewer_app_update_title(VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;
	char *title;
	const char *subtitle;

	if (!priv->window)
		return;

	if (priv->grabbed)
		subtitle = "(Press Ctrl+Alt to release pointer) ";
	else
		subtitle = "";

	if (priv->subtitle)
		title = g_strdup_printf("%s%s - Virt Viewer",
					subtitle, priv->subtitle);
	else
		title = g_strdup("Virt Viewer");

	gtk_window_set_title(GTK_WINDOW(priv->window), title);

	g_free(title);
}

static gboolean
virt_viewer_app_ignore_accel(GtkWidget *menu G_GNUC_UNUSED,
			     VirtViewerApp *self G_GNUC_UNUSED)
{
	/* ignore accelerator */
	return TRUE;
}


void
virt_viewer_app_disable_modifiers(VirtViewerApp *self)
{
	GtkSettings *settings = gtk_settings_get_default();
	VirtViewerAppPrivate *priv = self->priv;
	GValue empty;
	GSList *accels;
	int i;

	if (!priv->window)
		return;

	if (!priv->accelEnabled)
		return;

	/* This stops F10 activating menu bar */
	memset(&empty, 0, sizeof empty);
	g_value_init(&empty, G_TYPE_STRING);
	g_object_get_property(G_OBJECT(settings), "gtk-menu-bar-accel", &priv->accelSetting);
	g_object_set_property(G_OBJECT(settings), "gtk-menu-bar-accel", &empty);

	/* This stops global accelerators like Ctrl+Q == Quit */
	for (accels = priv->accelList ; accels ; accels = accels->next) {
		gtk_window_remove_accel_group(GTK_WINDOW(priv->window), accels->data);
	}

	/* This stops menu bar shortcuts like Alt+F == File */
	for (i = 0 ; i < LAST_MENU ; i++) {
		GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, menuNames[i]));
		priv->accelMenuSig[i] =
			g_signal_connect(menu, "mnemonic-activate",
					 G_CALLBACK(virt_viewer_app_ignore_accel), self);
	}

	priv->accelEnabled = FALSE;
}


void
virt_viewer_app_enable_modifiers(VirtViewerApp *self)
{
	GtkSettings *settings = gtk_settings_get_default();
	VirtViewerAppPrivate *priv = self->priv;
	GSList *accels;
	int i;

	if (!priv->window)
		return;

	if (priv->accelEnabled)
		return;

	/* This allows F10 activating menu bar */
	g_object_set_property(G_OBJECT(settings), "gtk-menu-bar-accel", &priv->accelSetting);

	/* This allows global accelerators like Ctrl+Q == Quit */
	for (accels = priv->accelList ; accels ; accels = accels->next) {
		gtk_window_add_accel_group(GTK_WINDOW(priv->window), accels->data);
	}

	/* This allows menu bar shortcuts like Alt+F == File */
	for (i = 0 ; i < LAST_MENU ; i++) {
		GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, menuNames[i]));
		g_signal_handler_disconnect(menu, priv->accelMenuSig[i]);
	}

	priv->accelEnabled = TRUE;
}

void
virt_viewer_app_quit(VirtViewerApp *self)
{
	g_return_if_fail(self != NULL);
	VirtViewerAppPrivate *priv = self->priv;

	if (priv->session)
		virt_viewer_session_close(VIRT_VIEWER_SESSION(priv->session));
	gtk_main_quit();
}

void
virt_viewer_app_delete(GtkWidget *src G_GNUC_UNUSED,
		       void *dummy G_GNUC_UNUSED,
		       VirtViewerApp *self)
{
	virt_viewer_app_quit(self);
}

void
virt_viewer_app_menu_file_quit(GtkWidget *src G_GNUC_UNUSED,
			       VirtViewerApp *self)
{
	virt_viewer_app_quit(self);
}


static void
virt_viewer_app_leave_fullscreen(VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;
	GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, "top-menu"));

	if (!priv->fullscreen)
		return;
	priv->fullscreen = FALSE;
	ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(priv->layout), FALSE);
	gtk_widget_show(menu);
	gtk_widget_hide(priv->toolbar);
	gtk_window_unfullscreen(GTK_WINDOW(priv->window));
	if (priv->autoResize)
		virt_viewer_app_resize_main_window(self);
}

static void
virt_viewer_app_enter_fullscreen(VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;
	GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, "top-menu"));

	if (priv->fullscreen)
		return;
	priv->fullscreen = TRUE;
	gtk_widget_hide(menu);
	gtk_window_fullscreen(GTK_WINDOW(priv->window));
	gtk_widget_show(priv->toolbar);
	ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(priv->layout), TRUE);
	ViewAutoDrawer_Close(VIEW_AUTODRAWER(priv->layout));
}

static gboolean
window_state_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEventWindowState *event,
		gpointer data)
{
	VirtViewerApp *self = data;

	if (!(event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN))
		return TRUE;

	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
		virt_viewer_app_enter_fullscreen(self);
	else
		virt_viewer_app_leave_fullscreen(self);

	return TRUE;
}

static void
virt_viewer_app_toolbar_leave_fullscreen(GtkWidget *button G_GNUC_UNUSED,
					 VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;
	GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, "menu-view-fullscreen"));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu), FALSE);
	virt_viewer_app_leave_fullscreen(self);
}


void
virt_viewer_app_menu_view_fullscreen(GtkWidget *menu,
				     VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;
	if (!priv->window)
		return;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu))) {
		virt_viewer_app_enter_fullscreen(self);
	} else {
		virt_viewer_app_leave_fullscreen(self);
	}
}

void
virt_viewer_app_menu_view_resize(GtkWidget *menu,
				 VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu))) {
		priv->autoResize = TRUE;
		if (!priv->fullscreen)
			virt_viewer_app_resize_main_window(self);
	} else {
		priv->autoResize = FALSE;
	}
}

void
virt_viewer_app_menu_send(GtkWidget *menu G_GNUC_UNUSED,
			  VirtViewerApp *self)
{
	int i;
	GtkWidget *label = gtk_bin_get_child(GTK_BIN(menu));
	const char *text = gtk_label_get_label(GTK_LABEL(label));
	VirtViewerAppPrivate *priv = self->priv;

	for (i = 0 ; i < G_N_ELEMENTS(keyCombos) ; i++) {
		if (!strcmp(text, keyCombos[i].label)) {
			DEBUG_LOG("Sending key combo %s", gtk_label_get_text(GTK_LABEL(label)));
			virt_viewer_display_send_keys(VIRT_VIEWER_DISPLAY(priv->display),
						      keyCombos[i].keys,
						      keyCombos[i].nkeys);
			return;
		}
	}
	DEBUG_LOG("Failed to find key combo %s", gtk_label_get_text(GTK_LABEL(label)));
}


static void
virt_viewer_app_save_screenshot(VirtViewerApp *self,
				const char *file)
{
	VirtViewerAppPrivate *priv = self->priv;
	GdkPixbuf *pix = virt_viewer_display_get_pixbuf(VIRT_VIEWER_DISPLAY(priv->display));

	gdk_pixbuf_save(pix, file, "png", NULL,
			"tEXt::Generator App", PACKAGE, NULL);
	gdk_pixbuf_unref(pix);
}


void
virt_viewer_app_menu_file_screenshot(GtkWidget *menu G_GNUC_UNUSED,
				     VirtViewerApp *self)
{
	GtkWidget *dialog;
	VirtViewerAppPrivate *priv = self->priv;

	g_return_if_fail(priv->display != NULL);

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
		virt_viewer_app_save_screenshot(self, filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}

void
virt_viewer_app_about_close(GtkWidget *dialog,
			    VirtViewerApp *self G_GNUC_UNUSED)
{
	gtk_widget_hide(dialog);
	gtk_widget_destroy(dialog);
}

void
virt_viewer_app_about_delete(GtkWidget *dialog,
			     void *dummy G_GNUC_UNUSED,
			     VirtViewerApp *self G_GNUC_UNUSED)
{
	gtk_widget_hide(dialog);
	gtk_widget_destroy(dialog);
}

void
virt_viewer_app_menu_help_about(GtkWidget *menu G_GNUC_UNUSED,
				VirtViewerApp *self)
{
	GtkBuilder *about = virt_viewer_util_load_ui("virt-viewer-about.xml");

	GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(about, "about"));
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), VERSION);

	gtk_builder_connect_signals(about, self);

	gtk_widget_show_all(dialog);

	g_object_unref(G_OBJECT(about));
}

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)

static int
virt_viewer_app_open_tunnel(const char **cmd)
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
virt_viewer_app_open_tunnel_ssh(const char *sshhost,
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

	return virt_viewer_app_open_tunnel(cmd);
}

static int
virt_viewer_app_open_unix_sock(const char *unixsock)
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

void
virt_viewer_app_trace(VirtViewerApp *self,
		      const char *fmt, ...)
{
	g_return_if_fail(VIRT_VIEWER_IS_APP(self));
	va_list ap;
	VirtViewerAppPrivate *priv = self->priv;

	if (doDebug) {
		va_start(ap, fmt);
		g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, fmt, ap);
		va_end(ap);
	}

	if (priv->verbose) {
		va_start(ap, fmt);
		g_vprintf(fmt, ap);
		va_end(ap);
	}
}


void
virt_viewer_app_set_status(VirtViewerApp *self,
			   const char *text)
{
	g_return_if_fail(VIRT_VIEWER_IS_APP(self));
	VirtViewerAppPrivate *priv = self->priv;

	gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook), 0);
	gtk_label_set_text(GTK_LABEL(priv->status), text);
}


static void
virt_viewer_app_show_display(VirtViewerApp *self)
{
	g_return_if_fail(self != NULL);

	VirtViewerAppPrivate *priv = self->priv;
	g_return_if_fail(priv->display != NULL);

	gtk_widget_grab_focus(gtk_bin_get_child(GTK_BIN(priv->display)));
	gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->notebook), 1);
}

static void
virt_viewer_app_pointer_grab(VirtViewerDisplay *display G_GNUC_UNUSED,
			     VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	priv->grabbed = TRUE;
	virt_viewer_app_update_title(self);
}

static void
virt_viewer_app_pointer_ungrab(VirtViewerDisplay *display G_GNUC_UNUSED,
			       VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	priv->grabbed = FALSE;
	virt_viewer_app_update_title(self);
}

static void
virt_viewer_app_keyboard_grab(VirtViewerDisplay *display G_GNUC_UNUSED,
			      VirtViewerApp *self)
{
	virt_viewer_app_disable_modifiers(self);
}

static void
virt_viewer_app_keyboard_ungrab(VirtViewerDisplay *display G_GNUC_UNUSED,
				VirtViewerApp *self)
{
	virt_viewer_app_enable_modifiers(self);
}


static void
virt_viewer_app_display_added(VirtViewerSession *session G_GNUC_UNUSED,
			      VirtViewerDisplay *display,
			      VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	/* XXX multihead */
	if (priv->display)
		return;
	priv->display = GTK_WIDGET(display);

	g_signal_connect(priv->display, "display-pointer-grab",
			 G_CALLBACK(virt_viewer_app_pointer_grab), self);
	g_signal_connect(priv->display, "display-pointer-ungrab",
			 G_CALLBACK(virt_viewer_app_pointer_ungrab), self);
	g_signal_connect(priv->display, "display-keyboard-grab",
			 G_CALLBACK(virt_viewer_app_keyboard_grab), self);
	g_signal_connect(priv->display, "display-keyboard-ungrab",
			 G_CALLBACK(virt_viewer_app_keyboard_ungrab), self);

	g_signal_connect(priv->display, "display-desktop-resize",
			 G_CALLBACK(virt_viewer_app_desktop_resize), self);

	gtk_notebook_append_page(GTK_NOTEBOOK(priv->notebook), priv->display, NULL);
	if (gtk_bin_get_child(GTK_BIN(priv->display)))
		gtk_widget_realize(GTK_WIDGET(gtk_bin_get_child(GTK_BIN(priv->display))));
	gtk_widget_show_all(priv->display);
}

static void
virt_viewer_app_display_removed(VirtViewerSession *session G_GNUC_UNUSED,
				VirtViewerDisplay *display,
				VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	/* XXX multihead */
	if (priv->display != GTK_WIDGET(display))
		return;

	gtk_widget_hide(GTK_WIDGET(display));
	gtk_notebook_remove_page(GTK_NOTEBOOK(priv->notebook), 1);
	priv->display = NULL;
}

int
virt_viewer_app_create_session(VirtViewerApp *self, const gchar *type)
{
	g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), -1);
	VirtViewerAppPrivate *priv = self->priv;
	g_return_val_if_fail(priv->session == NULL, -1);

	if (g_strcasecmp(type, "vnc") == 0) {
		virt_viewer_app_trace(self, "Guest %s has a %s display\n",
				      priv->guest_name, type);
		priv->session = virt_viewer_session_vnc_new();
	} else
#ifdef HAVE_SPICE_GTK
	if (g_strcasecmp(type, "spice") == 0) {
		virt_viewer_app_trace(self, "Guest %s has a %s display\n",
				      priv->guest_name, type);
		priv->session = virt_viewer_session_spice_new();
	} else
#endif
	{
		virt_viewer_app_trace(self, "Guest %s has unsupported %s display type\n",
				      priv->guest_name, type);
		virt_viewer_app_simple_message_dialog(self, _("Unknown graphic type for the guest %s"),
						      priv->guest_name);
		return -1;
	}

	g_signal_connect(priv->session, "session-initialized",
			 G_CALLBACK(virt_viewer_app_initialized), self);
	g_signal_connect(priv->session, "session-connected",
			 G_CALLBACK(virt_viewer_app_connected), self);
	g_signal_connect(priv->session, "session-disconnected",
			 G_CALLBACK(virt_viewer_app_disconnected), self);
	g_signal_connect(priv->session, "session-channel-open",
			 G_CALLBACK(virt_viewer_app_channel_open), self);
	g_signal_connect(priv->session, "session-auth-refused",
			 G_CALLBACK(virt_viewer_app_auth_refused), self);
	g_signal_connect(priv->session, "session-auth-failed",
			 G_CALLBACK(virt_viewer_app_auth_failed), self);
	g_signal_connect(priv->session, "session-display-added",
			 G_CALLBACK(virt_viewer_app_display_added), self);
	g_signal_connect(priv->session, "session-display-removed",
			 G_CALLBACK(virt_viewer_app_display_removed), self);

	g_signal_connect(priv->session, "session-cut-text",
			 G_CALLBACK(virt_viewer_app_server_cut_text), self);
	g_signal_connect(priv->session, "session-bell",
			 G_CALLBACK(virt_viewer_app_bell), self);

	return 0;
}

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)
static void
virt_viewer_app_channel_open(VirtViewerSession *session,
			     VirtViewerSessionChannel *channel,
			     VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv;
	int fd = -1;

	g_return_if_fail(self != NULL);

	priv = self->priv;
	if (priv->transport && g_strcasecmp(priv->transport, "ssh") == 0 &&
	    !priv->direct) {
		if ((fd = virt_viewer_app_open_tunnel_ssh(priv->host, priv->port, priv->user,
							  priv->ghost, priv->gport, NULL)) < 0)
			virt_viewer_app_simple_message_dialog(self, _("Connect to ssh failed."));
	} else {
		virt_viewer_app_simple_message_dialog(self, _("Can't connect to channel, SSH only supported."));
	}

	if (fd >= 0)
		virt_viewer_session_channel_open_fd(session, channel, fd);
}
#else
static void
virt_viewer_app_channel_open(VirtViewerSession *session G_GNUC_UNUSED,
			     VirtViewerSessionChannel *channel G_GNUC_UNUSED,
			     VirtViewerApp *self)
{
	virt_viewer_app_simple_message_dialog(self, _("Connect to channel unsupported."));
}
#endif

int
virt_viewer_app_activate(VirtViewerApp *self)
{
	g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), -1);
	VirtViewerAppPrivate *priv = self->priv;
	int fd = -1;
	int ret = -1;

	if (priv->active)
		goto cleanup;

#if defined(HAVE_SOCKETPAIR) && defined(HAVE_FORK)
	if (priv->transport &&
	    g_strcasecmp(priv->transport, "ssh") == 0 &&
	    !priv->direct) {
		if (priv->gport) {
			virt_viewer_app_trace(self, "Opening indirect TCP connection to display at %s:%s\n",
					      priv->ghost, priv->gport);
		} else {
			virt_viewer_app_trace(self, "Opening indirect UNIX connection to display at %s\n",
					      priv->unixsock);
		}
		virt_viewer_app_trace(self, "Setting up SSH tunnel via %s@%s:%d\n",
				      priv->user, priv->host, priv->port ? priv->port : 22);

		if ((fd = virt_viewer_app_open_tunnel_ssh(priv->host, priv->port,
							  priv->user, priv->ghost,
							  priv->gport, priv->unixsock)) < 0)
			return -1;
	} else if (priv->unixsock) {
		virt_viewer_app_trace(self, "Opening direct UNIX connection to display at %s",
				      priv->unixsock);
		if ((fd = virt_viewer_app_open_unix_sock(priv->unixsock)) < 0)
			return -1;
	}
#endif

	if (fd >= 0) {
		ret = virt_viewer_session_open_fd(VIRT_VIEWER_SESSION(priv->session), fd);
	} else {
		virt_viewer_app_trace(self, "Opening direct TCP connection to display at %s:%s\n",
				      priv->ghost, priv->gport);
		ret = virt_viewer_session_open_host(VIRT_VIEWER_SESSION(priv->session),
						    priv->ghost, priv->gport);
	}

	virt_viewer_app_set_status(self, "Connecting to graphic server");

	priv->connected = FALSE;
	priv->active = TRUE;
	priv->grabbed = FALSE;
	virt_viewer_app_update_title(self);

 cleanup:
	return ret;
}

/* text was actually requested */
static void
virt_viewer_app_clipboard_copy(GtkClipboard *clipboard G_GNUC_UNUSED,
			       GtkSelectionData *data,
			       guint info G_GNUC_UNUSED,
			       VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	gtk_selection_data_set_text(data, priv->clipboard, -1);
}

static void
virt_viewer_app_server_cut_text(VirtViewerSession *session G_GNUC_UNUSED,
				const gchar *text,
				VirtViewerApp *self)
{
	GtkClipboard *cb;
	gsize a, b;
	VirtViewerAppPrivate *priv = self->priv;
	GtkTargetEntry targets[] = {
		{g_strdup("UTF8_STRING"), 0, 0},
		{g_strdup("COMPOUND_TEXT"), 0, 0},
		{g_strdup("TEXT"), 0, 0},
		{g_strdup("STRING"), 0, 0},
	};

	if (!text)
		return;

	g_free (priv->clipboard);
	priv->clipboard = g_convert (text, -1, "utf-8", "iso8859-1", &a, &b, NULL);

	if (priv->clipboard) {
		cb = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

		gtk_clipboard_set_with_owner (cb,
					      targets,
					      G_N_ELEMENTS(targets),
					      (GtkClipboardGetFunc)virt_viewer_app_clipboard_copy,
					      NULL,
					      G_OBJECT (self));
	}
}


static void virt_viewer_app_bell(VirtViewerSession *session G_GNUC_UNUSED,
				 VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	gdk_window_beep(gtk_widget_get_window(GTK_WIDGET(priv->window)));
}


static int
virt_viewer_app_default_initial_connect(VirtViewerApp *self)
{
	return virt_viewer_app_activate(self);
}

int
virt_viewer_app_initial_connect(VirtViewerApp *self)
{
	VirtViewerAppClass *klass;

	g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), -1);
	klass = VIRT_VIEWER_APP_GET_CLASS(self);

	return klass->initial_connect(self);
}

static gboolean
virt_viewer_app_retryauth(gpointer opaque)
{
	VirtViewerApp *self = opaque;

	virt_viewer_app_initial_connect(self);

	return FALSE;
}

static gboolean
virt_viewer_app_connect_timer(void *opaque)
{
	VirtViewerApp *self = opaque;
	VirtViewerAppPrivate *priv = self->priv;

	DEBUG_LOG("Connect timer fired");

	if (!priv->active &&
	    virt_viewer_app_initial_connect(self) < 0)
		gtk_main_quit();

	if (priv->active) {
		priv->reconnect_poll = 0;
		return FALSE;
	}

	return TRUE;
}

void
virt_viewer_app_start_reconnect_poll(VirtViewerApp *self)
{
	g_return_if_fail(VIRT_VIEWER_IS_APP(self));
	VirtViewerAppPrivate *priv = self->priv;

	if (priv->reconnect_poll != 0)
		return;

	priv->reconnect_poll = g_timeout_add(500, virt_viewer_app_connect_timer, self);
}

static void
virt_viewer_app_default_deactivated(VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	virt_viewer_app_set_status(self, "Guest domain has shutdown");
	virt_viewer_app_trace(self, "Guest %s display has disconnected, shutting down",
			      priv->guest_name);
	gtk_main_quit();
}

static void
virt_viewer_app_deactivated(VirtViewerApp *self)
{
	VirtViewerAppClass *klass;
	klass = VIRT_VIEWER_APP_GET_CLASS(self);

	klass->deactivated(self);
}

static void
virt_viewer_app_deactivate(VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	if (!priv->active)
		return;

	if (priv->session)
		virt_viewer_session_close(VIRT_VIEWER_SESSION(priv->session));

	priv->connected = FALSE;
	priv->active = FALSE;
#if 0
	g_free(priv->pretty_address);
	priv->pretty_address = NULL;
#endif
	priv->grabbed = FALSE;
	virt_viewer_app_update_title(self);

	if (priv->authretry) {
		priv->authretry = FALSE;
		g_idle_add(virt_viewer_app_retryauth, self);
	} else
		virt_viewer_app_deactivated(self);

}

static void
virt_viewer_app_connected(VirtViewerSession *session G_GNUC_UNUSED,
			  VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	priv->connected = TRUE;
	virt_viewer_app_set_status(self, "Connected to graphic server");
}

static void
virt_viewer_app_initialized(VirtViewerSession *session G_GNUC_UNUSED,
			    VirtViewerApp *self)
{
	virt_viewer_app_show_display(self);
	virt_viewer_app_update_title(self);
}

static void
virt_viewer_app_disconnected(VirtViewerSession *session G_GNUC_UNUSED,
			     VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	if (!priv->connected) {
		virt_viewer_app_simple_message_dialog(self,
						      _("Unable to connect to the graphic server %s"),
						      priv->pretty_address);
	}
	virt_viewer_app_deactivate(self);
}


static void virt_viewer_app_auth_refused(VirtViewerSession *session G_GNUC_UNUSED,
					 const char *msg,
					 VirtViewerApp *self)
{
	GtkWidget *dialog;
	int ret;
	VirtViewerAppPrivate *priv = self->priv;

	dialog = gtk_message_dialog_new(GTK_WINDOW(priv->window),
					GTK_DIALOG_MODAL |
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_YES_NO,
					_("Unable to authenticate with remote desktop server at %s: %s\n"
					  "Retry connection again?"),
					priv->pretty_address, msg);

	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	if (ret == GTK_RESPONSE_YES)
		priv->authretry = TRUE;
	else
		priv->authretry = FALSE;
}


static void virt_viewer_app_auth_failed(VirtViewerSession *session G_GNUC_UNUSED,
					const char *msg,
					VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv = self->priv;

	virt_viewer_app_simple_message_dialog(self,
					      _("Unable to authenticate with remote desktop server at %s"),
					      priv->pretty_address, msg);
}

static void
virt_viewer_app_toolbar_setup(VirtViewerApp *self)
{
	GtkWidget *button;
	VirtViewerAppPrivate *priv = self->priv;

	priv->toolbar = gtk_toolbar_new();
	gtk_toolbar_set_show_arrow(GTK_TOOLBAR(priv->toolbar), FALSE);
	gtk_widget_set_no_show_all(priv->toolbar, TRUE);
	gtk_toolbar_set_style(GTK_TOOLBAR(priv->toolbar), GTK_TOOLBAR_BOTH_HORIZ);

	/* Close connection */
	button = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_CLOSE));
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(button), _("Disconnect"));
	gtk_widget_show(GTK_WIDGET(button));
	gtk_toolbar_insert(GTK_TOOLBAR(priv->toolbar), GTK_TOOL_ITEM (button), 0);
	g_signal_connect(button, "clicked", G_CALLBACK(gtk_main_quit), NULL);

	/* Leave fullscreen */
	button = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_LEAVE_FULLSCREEN));
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(button), _("Leave fullscreen"));
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(button), _("Leave fullscreen"));
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(button), TRUE);
	gtk_widget_show(GTK_WIDGET(button));
	gtk_toolbar_insert(GTK_TOOLBAR(priv->toolbar), GTK_TOOL_ITEM(button), 0);
	g_signal_connect(button, "clicked", G_CALLBACK(virt_viewer_app_toolbar_leave_fullscreen), self);

	priv->layout = ViewAutoDrawer_New();

	ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(priv->layout), FALSE);
	ViewOvBox_SetOver(VIEW_OV_BOX(priv->layout), priv->toolbar);
	ViewOvBox_SetUnder(VIEW_OV_BOX(priv->layout), priv->notebook);
	ViewAutoDrawer_SetOffset(VIEW_AUTODRAWER(priv->layout), -1);
	ViewAutoDrawer_SetFill(VIEW_AUTODRAWER(priv->layout), FALSE);
	ViewAutoDrawer_SetOverlapPixels(VIEW_AUTODRAWER(priv->layout), 1);
	ViewAutoDrawer_SetNoOverlapPixels(VIEW_AUTODRAWER(priv->layout), 0);
}


static void
virt_viewer_app_get_property (GObject *object, guint property_id,
			      GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
	g_return_if_fail(VIRT_VIEWER_IS_APP(object));
	VirtViewerApp *self = VIRT_VIEWER_APP(object);
	VirtViewerAppPrivate *priv = self->priv;

        switch (property_id) {
	case PROP_SUBTITLE:
		g_value_set_string(value, priv->subtitle);
		break;

	case PROP_VERBOSE:
		g_value_set_boolean(value, priv->verbose);
		break;

	case PROP_CONTAINER:
		g_value_set_object(value, priv->container);
		break;

	case PROP_SESSION:
		g_value_set_object(value, priv->session);
		break;

	case PROP_GUEST_NAME:
		g_value_set_string(value, priv->guest_name);
		break;

        default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
virt_viewer_app_set_property (GObject *object, guint property_id,
			      const GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
	g_return_if_fail(VIRT_VIEWER_IS_APP(object));
	VirtViewerApp *self = VIRT_VIEWER_APP(object);
	VirtViewerAppPrivate *priv = self->priv;

        switch (property_id) {
	case PROP_SUBTITLE:
		g_free(priv->subtitle);
		priv->subtitle = g_value_dup_string(value);
		break;

	case PROP_VERBOSE:
		priv->verbose = g_value_get_boolean(value);
		break;

	case PROP_CONTAINER:
		g_return_if_fail(priv->container == NULL);
		priv->container = g_value_dup_object(value);
		break;

	case PROP_GUEST_NAME:
		g_free(priv->guest_name);
		priv->guest_name = g_value_dup_string(value);
		break;

        default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
virt_viewer_app_dispose (GObject *object)
{
	VirtViewerApp *self = VIRT_VIEWER_APP(object);
	VirtViewerAppPrivate *priv = self->priv;

	if (priv->container) {
		g_object_unref(priv->container);
		priv->container = NULL;
	}

	g_free(priv->subtitle);
	priv->subtitle = NULL;

	virt_viewer_app_free_connect_info(self);

        G_OBJECT_CLASS (virt_viewer_app_parent_class)->dispose (object);
}

static gboolean
virt_viewer_app_default_start(VirtViewerApp *self, gboolean fullscreen)
{
	VirtViewerAppPrivate *priv;
	GtkWidget *menu;
	GdkColor color;

	priv = self->priv;

	if (!priv->container) {
		priv->builder = virt_viewer_util_load_ui("virt-viewer.xml");

		menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, "menu-view-resize"));
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu), TRUE);

		gtk_builder_connect_signals(priv->builder, self);
	}

	if (priv->container) {
		gtk_box_pack_end(GTK_BOX(priv->container), priv->notebook, TRUE, TRUE, 0);
		gtk_widget_show_all(GTK_WIDGET(priv->container));
	} else {
		GtkWidget *vbox = GTK_WIDGET(gtk_builder_get_object(priv->builder, "viewer-box"));
		virt_viewer_app_toolbar_setup(self);

		gtk_box_pack_end(GTK_BOX(vbox), priv->layout, TRUE, TRUE, 0);
		gdk_color_parse("black", &color);
		gtk_widget_modify_bg(priv->layout, GTK_STATE_NORMAL, &color);
		gdk_color_parse("white", &color);
		gtk_widget_modify_fg(priv->status, GTK_STATE_NORMAL, &color);

		GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(priv->builder, "viewer"));
		GSList *accels;
		priv->container = window;
		priv->window = window;
		virt_viewer_app_update_title(self);
		gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
		gtk_window_set_has_resize_grip(GTK_WINDOW(window), FALSE);
#endif
		priv->accelEnabled = TRUE;
		accels = gtk_accel_groups_from_object(G_OBJECT(window));
		for ( ; accels ; accels = accels->next) {
			priv->accelList = g_slist_append(priv->accelList, accels->data);
			g_object_ref(G_OBJECT(accels->data));
		}
		g_signal_connect(G_OBJECT(window), "window-state-event", G_CALLBACK(window_state_cb), self);
		if (fullscreen)
			gtk_window_fullscreen(GTK_WINDOW(window));
		gtk_widget_show_all(priv->window);
	}

	return TRUE;
}

gboolean virt_viewer_app_start(VirtViewerApp *self, gboolean fullscreen)
{
	VirtViewerAppClass *klass;

	g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);
	klass = VIRT_VIEWER_APP_GET_CLASS(self);

	g_return_val_if_fail(!self->priv->started, TRUE);

	self->priv->started = klass->start(self, fullscreen);
	return self->priv->started;
}

static void
virt_viewer_app_class_init (VirtViewerAppClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        g_type_class_add_private (klass, sizeof (VirtViewerAppPrivate));

        object_class->get_property = virt_viewer_app_get_property;
        object_class->set_property = virt_viewer_app_set_property;
        object_class->dispose = virt_viewer_app_dispose;

	klass->start = virt_viewer_app_default_start;
	klass->initial_connect = virt_viewer_app_default_initial_connect;
	klass->deactivated = virt_viewer_app_default_deactivated;

	g_object_class_install_property(object_class,
					PROP_SUBTITLE,
					g_param_spec_string("subtitle",
							    "Subtitle",
							    "Window subtitle",
							    "",
							    G_PARAM_READABLE |
							    G_PARAM_WRITABLE |
							    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object_class,
					PROP_VERBOSE,
					g_param_spec_boolean("verbose",
							     "Verbose",
							     "Verbose trace",
							     FALSE,
							     G_PARAM_READABLE |
							     G_PARAM_WRITABLE |
							     G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object_class,
					PROP_CONTAINER,
					g_param_spec_object("container",
							    "Container",
							    "Widget container",
							    GTK_TYPE_WIDGET,
							    G_PARAM_READABLE |
							    G_PARAM_WRITABLE |
							    G_PARAM_CONSTRUCT_ONLY |
							    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object_class,
					PROP_SESSION,
					g_param_spec_object("session",
							    "Session",
							    "ViewerSession",
							    VIRT_VIEWER_TYPE_SESSION,
							    G_PARAM_READABLE |
							    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object_class,
					PROP_GUEST_NAME,
					g_param_spec_string("guest-name",
							    "Guest name",
							    "Guest name",
							    "",
							    G_PARAM_READABLE |
							    G_PARAM_WRITABLE |
							    G_PARAM_STATIC_STRINGS));

}

static void
virt_viewer_app_init (VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv;

	self->priv = GET_PRIVATE(self);
	priv = self->priv;

	priv->autoResize = TRUE;
	g_value_init(&priv->accelSetting, G_TYPE_STRING);
	priv->status = gtk_label_new("");
	priv->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(priv->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(priv->notebook), FALSE);

	gtk_notebook_append_page(GTK_NOTEBOOK(priv->notebook), priv->status, NULL);

}

void
virt_viewer_app_set_zoom_level(VirtViewerApp *self, gint zoom_level)
{
	g_return_if_fail(VIRT_VIEWER_IS_APP(self));

	/* FIXME: turn into a dynamic property */
	self->priv->zoomlevel = zoom_level;
}

void
virt_viewer_app_set_direct(VirtViewerApp *self, gboolean direct)
{
	g_return_if_fail(VIRT_VIEWER_IS_APP(self));

	self->priv->direct = direct;
}

gboolean
virt_viewer_app_is_active(VirtViewerApp *self)
{
	g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);

	return self->priv->active;
}

gboolean
virt_viewer_app_has_session(VirtViewerApp *self)
{
	g_return_val_if_fail(VIRT_VIEWER_IS_APP(self), FALSE);

	return self->priv->session != NULL;
}

static void
virt_viewer_app_update_pretty_address(VirtViewerApp *self)
{
	VirtViewerAppPrivate *priv;

	priv = self->priv;
	g_free(priv->pretty_address);
	if (priv->gport)
		priv->pretty_address = g_strdup_printf("%s:%s", priv->ghost, priv->gport);
	else
		priv->pretty_address = g_strdup_printf("%s:%s", priv->host, priv->unixsock);
}

void
virt_viewer_app_set_connect_info(VirtViewerApp *self,
				 const gchar *host,
				 const gchar *ghost,
				 const gchar *gport,
				 const gchar *transport,
				 const gchar *unixsock,
				 const gchar *user,
				 gint port)
{
	g_return_if_fail(VIRT_VIEWER_IS_APP(self));
	VirtViewerAppPrivate *priv = self->priv;

	DEBUG_LOG("Set connect info: %s,%s,%s,%s,%s,%s,%d",
		  host, ghost, gport, transport, unixsock, user, port);

	g_free(priv->host);
	g_free(priv->ghost);
	g_free(priv->gport);
	g_free(priv->transport);
	g_free(priv->unixsock);
	g_free(priv->user);

	priv->host = g_strdup(host);
	priv->ghost = g_strdup(ghost);
	priv->gport = g_strdup(gport);
	priv->transport = g_strdup(transport);
	priv->unixsock = g_strdup(unixsock);
	priv->user = g_strdup(user);
	priv->port = 0;

	virt_viewer_app_update_pretty_address(self);
}

void
virt_viewer_app_free_connect_info(VirtViewerApp *self)
{
	virt_viewer_app_set_connect_info(self, NULL, NULL, NULL, NULL, NULL, NULL, 0);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 *  indent-tabs-mode: t
 * End:
 */
