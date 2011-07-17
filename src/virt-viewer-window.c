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

#include "virt-viewer-window.h"
#include "virt-viewer-app.h"
#include "virt-viewer-util.h"
#include "view/autoDrawer.h"

/* Signal handlers for main window (move in a VirtViewerMainWindow?) */
void virt_viewer_window_menu_view_zoom_out(GtkWidget *menu, VirtViewerWindow *self);
void virt_viewer_window_menu_view_zoom_in(GtkWidget *menu, VirtViewerWindow *self);
void virt_viewer_window_menu_view_zoom_reset(GtkWidget *menu, VirtViewerWindow *self);
gboolean virt_viewer_window_delete(GtkWidget *src, void *dummy, VirtViewerWindow *self);
void virt_viewer_window_menu_file_quit(GtkWidget *src, VirtViewerWindow *self);
void virt_viewer_window_menu_help_about(GtkWidget *menu, VirtViewerWindow *self);
void virt_viewer_window_menu_view_fullscreen(GtkWidget *menu, VirtViewerWindow *self);
void virt_viewer_window_menu_view_resize(GtkWidget *menu, VirtViewerWindow *self);
void virt_viewer_window_menu_send(GtkWidget *menu, VirtViewerWindow *self);
void virt_viewer_window_menu_file_screenshot(GtkWidget *menu, VirtViewerWindow *self);

/* Internal methods */
static void virt_viewer_window_enable_modifiers(VirtViewerWindow *self);
static void virt_viewer_window_disable_modifiers(VirtViewerWindow *self);
static void virt_viewer_window_resize(VirtViewerWindow *self);
static void virt_viewer_window_toolbar_setup(VirtViewerWindow *self);

G_DEFINE_TYPE (VirtViewerWindow, virt_viewer_window, G_TYPE_OBJECT)

#define GET_PRIVATE(o)							\
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), VIRT_VIEWER_TYPE_WINDOW, VirtViewerWindowPrivate))

enum {
	PROP_0,
	PROP_WINDOW,
	PROP_DISPLAY,
	PROP_SUBTITLE,
	PROP_CONTAINER,
	PROP_APP,
};

enum menuNums {
	FILE_MENU,
	VIEW_MENU,
	SEND_KEY_MENU,
	HELP_MENU,
	LAST_MENU // sentinel
};

struct _VirtViewerWindowPrivate {
	VirtViewerApp *app;
	GtkContainer *container; /* if any, then there is no window */

	GtkBuilder *builder;
	GtkWidget *window;
	GtkWidget *layout;
	GtkWidget *toolbar;
	VirtViewerNotebook *notebook;
	VirtViewerDisplay *display;

	gboolean accel_enabled;
	GValue accel_setting;
	GSList *accel_list;
	int accel_menu_sig[LAST_MENU];
	gboolean grabbed;
	gboolean before_saved;
	GdkRectangle before_fullscreen;

	gint zoomlevel;
	gboolean auto_resize;
	gboolean fullscreen;
	gchar *subtitle;
};

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

static void
virt_viewer_window_get_property (GObject *object, guint property_id,
				 GValue *value, GParamSpec *pspec)
{
	VirtViewerWindowPrivate *priv = VIRT_VIEWER_WINDOW(object)->priv;

	switch (property_id) {
	case PROP_SUBTITLE:
		g_value_set_string(value, priv->subtitle);
		break;

	case PROP_WINDOW:
		g_value_set_object(value, priv->window);
		break;

	case PROP_DISPLAY:
		g_value_set_object(value, priv->display);
		break;

	case PROP_CONTAINER:
		g_value_set_object(value, priv->container);
		break;

	case PROP_APP:
		g_value_set_object(value, priv->app);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
virt_viewer_window_set_property (GObject *object, guint property_id,
				 const GValue *value, GParamSpec *pspec)
{
	VirtViewerWindowPrivate *priv = VIRT_VIEWER_WINDOW(object)->priv;

	switch (property_id) {
	case PROP_SUBTITLE:
		g_free(priv->subtitle);
		priv->subtitle = g_value_dup_string(value);
		break;

	case PROP_CONTAINER:
		g_return_if_fail(priv->container == NULL);
		priv->container = g_value_dup_object(value);
		break;

	case PROP_APP:
		g_return_if_fail(priv->app == NULL);
		priv->app = g_value_dup_object(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
virt_viewer_window_dispose (GObject *object)
{
	VirtViewerWindowPrivate *priv = VIRT_VIEWER_WINDOW(object)->priv;
	G_OBJECT_CLASS (virt_viewer_window_parent_class)->dispose (object);

	if (priv->display) {
		g_object_unref(priv->display);
		priv->display = NULL;
	}

	if (priv->app) {
		g_object_unref(priv->app);
		priv->app = NULL;
	}

	g_free(priv->subtitle);
	priv->subtitle = NULL;
}

static void
virt_viewer_window_class_init (VirtViewerWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (VirtViewerWindowPrivate));

	object_class->get_property = virt_viewer_window_get_property;
	object_class->set_property = virt_viewer_window_set_property;
	object_class->dispose = virt_viewer_window_dispose;

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
					PROP_WINDOW,
					g_param_spec_object("window",
							    "Window",
							    "GtkWindow",
							    GTK_TYPE_WIDGET,
							    G_PARAM_READABLE |
							    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object_class,
					PROP_DISPLAY,
					g_param_spec_object("display",
							    "Display",
							    "VirtDisplay",
							    VIRT_VIEWER_TYPE_DISPLAY,
							    G_PARAM_READABLE |
							    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object_class,
					PROP_CONTAINER,
					g_param_spec_object("container",
							    "Container",
							    "Container widget",
							    VIRT_VIEWER_TYPE_DISPLAY,
							    G_PARAM_READABLE |
							    G_PARAM_WRITABLE |
							    G_PARAM_CONSTRUCT_ONLY |
							    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(object_class,
					PROP_APP,
					g_param_spec_object("app",
							    "App",
							    "VirtViewerApp",
							    VIRT_VIEWER_TYPE_APP,
							    G_PARAM_READABLE |
							    G_PARAM_WRITABLE |
							    G_PARAM_CONSTRUCT_ONLY |
							    G_PARAM_STATIC_STRINGS));
}

static void
virt_viewer_window_init (VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv;
	GtkWidget *vbox;
	GtkWidget *menu;
	GdkColor color;
	GSList *accels;

	self->priv = GET_PRIVATE(self);
	priv = self->priv;

	priv->auto_resize = TRUE;
	g_value_init(&priv->accel_setting, G_TYPE_STRING);

	priv->notebook = virt_viewer_notebook_new();
	priv->builder = virt_viewer_util_load_ui("virt-viewer.xml");

	menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, "menu-view-resize"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu), TRUE);

	gtk_builder_connect_signals(priv->builder, self);

	vbox = GTK_WIDGET(gtk_builder_get_object(priv->builder, "viewer-box"));
	virt_viewer_window_toolbar_setup(self);

	gtk_box_pack_end(GTK_BOX(vbox), priv->layout, TRUE, TRUE, 0);
	gdk_color_parse("black", &color);
	gtk_widget_modify_bg(priv->layout, GTK_STATE_NORMAL, &color);

	priv->window = GTK_WIDGET(gtk_builder_get_object(priv->builder, "viewer"));

	virt_viewer_window_update_title(self);
	gtk_window_set_resizable(GTK_WINDOW(priv->window), TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_window_set_has_resize_grip(GTK_WINDOW(priv->window), FALSE);
#endif
	priv->accel_enabled = TRUE;

	accels = gtk_accel_groups_from_object(G_OBJECT(priv->window));
	for ( ; accels ; accels = accels->next) {
		priv->accel_list = g_slist_append(priv->accel_list, accels->data);
		g_object_ref(G_OBJECT(accels->data));
	}
}

static void
virt_viewer_window_desktop_resize(VirtViewerDisplay *display G_GNUC_UNUSED,
				  VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv = self->priv;
	if (priv->auto_resize && priv->window && !priv->fullscreen)
		virt_viewer_window_resize(self);
}


void
virt_viewer_window_menu_view_zoom_out(GtkWidget *menu G_GNUC_UNUSED,
				      VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv = self->priv;

	if (priv->zoomlevel > 10)
		priv->zoomlevel -= 10;

	if (!priv->display)
		return;

	gtk_window_resize(GTK_WINDOW(priv->window), 1, 1);
	if (priv->display)
		virt_viewer_display_set_zoom_level(VIRT_VIEWER_DISPLAY(priv->display), priv->zoomlevel);
}

void
virt_viewer_window_menu_view_zoom_in(GtkWidget *menu G_GNUC_UNUSED,
				     VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv = self->priv;

	if (priv->zoomlevel < 400)
		priv->zoomlevel += 10;

	if (!priv->display)
		return;

	gtk_window_resize(GTK_WINDOW(priv->window), 1, 1);
	if (priv->display)
		virt_viewer_display_set_zoom_level(VIRT_VIEWER_DISPLAY(priv->display), priv->zoomlevel);
}

void
virt_viewer_window_menu_view_zoom_reset(GtkWidget *menu G_GNUC_UNUSED,
					VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv = self->priv;
	gtk_window_resize(GTK_WINDOW(priv->window), 1, 1);
	priv->zoomlevel = 100;

	if (priv->display)
		virt_viewer_display_set_zoom_level(VIRT_VIEWER_DISPLAY(priv->display), priv->zoomlevel);
}

/*
 * This code attempts to resize the top level window to be large enough
 * to contain the entire display desktop at 1:1 ratio. If the local desktop
 * isn't large enough that it goes as large as possible and lets the display
 * scale down to fit, maintaining aspect ratio
 */
static void
virt_viewer_window_resize(VirtViewerWindow *self)
{
	GdkRectangle fullscreen;
	GdkScreen *screen;
	int width, height;
	double desktopAspect;
	double screenAspect;
	guint desktopWidth;
	guint desktopHeight;
	VirtViewerWindowPrivate *priv = self->priv;

	DEBUG_LOG("Preparing main window resize");
	if (!priv->display) {
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

void
virt_viewer_window_leave_fullscreen(VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv = self->priv;
	GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, "top-menu"));
	GtkCheckMenuItem *check = GTK_CHECK_MENU_ITEM(gtk_builder_get_object(priv->builder, "menu-view-fullscreen"));

	if (!priv->fullscreen)
		return;

	gtk_check_menu_item_set_active(check, FALSE);
	priv->fullscreen = FALSE;
	ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(priv->layout), FALSE);
	gtk_widget_show(menu);
	gtk_widget_hide(priv->toolbar);
	gtk_window_unfullscreen(GTK_WINDOW(priv->window));

	if (priv->before_saved) {
		gtk_window_move(GTK_WINDOW(priv->window),
				priv->before_fullscreen.x,
				priv->before_fullscreen.y);
		gtk_window_resize(GTK_WINDOW(priv->window),
				  priv->before_fullscreen.width,
				  priv->before_fullscreen.height);
		priv->before_saved = FALSE;
	}
}

void
virt_viewer_window_enter_fullscreen(VirtViewerWindow *self, gboolean move, gint x, gint y)
{
	VirtViewerWindowPrivate *priv = self->priv;
	GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, "top-menu"));
	GtkCheckMenuItem *check = GTK_CHECK_MENU_ITEM(gtk_builder_get_object(priv->builder, "menu-view-fullscreen"));

	if (!priv->before_saved) {
		gtk_window_get_position(GTK_WINDOW(priv->window),
					&priv->before_fullscreen.x,
					&priv->before_fullscreen.y);
		gtk_window_get_size(GTK_WINDOW(priv->window),
				    &priv->before_fullscreen.width,
				    &priv->before_fullscreen.height);
		priv->before_saved = TRUE;
	}

	if (!priv->fullscreen) {
		gtk_check_menu_item_set_active(check, TRUE);
		priv->fullscreen = TRUE;
		gtk_widget_hide(menu);
		gtk_widget_show(priv->toolbar);
		ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(priv->layout), TRUE);
		ViewAutoDrawer_Close(VIEW_AUTODRAWER(priv->layout));
	}

	if (move)
		gtk_window_move(GTK_WINDOW(priv->window), x, y);

	gtk_window_fullscreen(GTK_WINDOW(priv->window));
}

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

void
virt_viewer_window_menu_send(GtkWidget *menu G_GNUC_UNUSED,
			     VirtViewerWindow *self)
{
	int i;
	GtkWidget *label = gtk_bin_get_child(GTK_BIN(menu));
	const char *text = gtk_label_get_label(GTK_LABEL(label));
	VirtViewerWindowPrivate *priv = self->priv;

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

static gboolean
virt_viewer_window_ignore_accel(GtkWidget *menu G_GNUC_UNUSED,
				VirtViewerWindow *self G_GNUC_UNUSED)
{
	/* ignore accelerator */
	return TRUE;
}

static const char * const menuNames[LAST_MENU] = {
	"menu-file", "menu-view", "menu-send", "menu-help"
};

void
virt_viewer_window_disable_modifiers(VirtViewerWindow *self)
{
	GtkSettings *settings = gtk_settings_get_default();
	VirtViewerWindowPrivate *priv = self->priv;
	GValue empty;
	GSList *accels;
	int i;

	if (!priv->window)
		return;

	if (!priv->accel_enabled)
		return;

	/* This stops F10 activating menu bar */
	memset(&empty, 0, sizeof empty);
	g_value_init(&empty, G_TYPE_STRING);
	g_object_get_property(G_OBJECT(settings), "gtk-menu-bar-accel", &priv->accel_setting);
	g_object_set_property(G_OBJECT(settings), "gtk-menu-bar-accel", &empty);

	/* This stops global accelerators like Ctrl+Q == Quit */
	for (accels = priv->accel_list ; accels ; accels = accels->next) {
		gtk_window_remove_accel_group(GTK_WINDOW(priv->window), accels->data);
	}

	/* This stops menu bar shortcuts like Alt+F == File */
	for (i = 0 ; i < LAST_MENU ; i++) {
		GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, menuNames[i]));
		priv->accel_menu_sig[i] =
			g_signal_connect(menu, "mnemonic-activate",
					 G_CALLBACK(virt_viewer_window_ignore_accel), self);
	}

	priv->accel_enabled = FALSE;
}

void
virt_viewer_window_enable_modifiers(VirtViewerWindow *self)
{
	GtkSettings *settings = gtk_settings_get_default();
	VirtViewerWindowPrivate *priv = self->priv;
	GSList *accels;
	int i;

	if (!priv->window)
		return;

	if (priv->accel_enabled)
		return;

	/* This allows F10 activating menu bar */
	g_object_set_property(G_OBJECT(settings), "gtk-menu-bar-accel", &priv->accel_setting);

	/* This allows global accelerators like Ctrl+Q == Quit */
	for (accels = priv->accel_list ; accels ; accels = accels->next) {
		gtk_window_add_accel_group(GTK_WINDOW(priv->window), accels->data);
	}

	/* This allows menu bar shortcuts like Alt+F == File */
	for (i = 0 ; i < LAST_MENU ; i++) {
		GtkWidget *menu = GTK_WIDGET(gtk_builder_get_object(priv->builder, menuNames[i]));
		g_signal_handler_disconnect(menu, priv->accel_menu_sig[i]);
	}

	priv->accel_enabled = TRUE;
}


gboolean
virt_viewer_window_delete(GtkWidget *src G_GNUC_UNUSED,
			  void *dummy G_GNUC_UNUSED,
			  VirtViewerWindow *self)
{
	virt_viewer_app_window_set_visible(self->priv->app, self, FALSE);
	return TRUE;
}


void
virt_viewer_window_menu_file_quit(GtkWidget *src G_GNUC_UNUSED,
				  VirtViewerWindow *self)
{
	virt_viewer_app_quit(self->priv->app);
}


static void
virt_viewer_window_toolbar_leave_fullscreen(GtkWidget *button G_GNUC_UNUSED,
					    VirtViewerWindow *self)
{
	g_object_set(self->priv->app, "fullscreen", FALSE, NULL);
}


void
virt_viewer_window_menu_view_fullscreen(GtkWidget *menu,
					VirtViewerWindow *self)
{
	gboolean fullscreen = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu));

	g_object_set(self->priv->app, "fullscreen", fullscreen, NULL);
}

void
virt_viewer_window_menu_view_resize(GtkWidget *menu,
				    VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv = self->priv;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu))) {
		priv->auto_resize = TRUE;
		if (!priv->fullscreen)
			virt_viewer_window_resize(self);
	} else {
		priv->auto_resize = FALSE;
	}
}

static void
virt_viewer_window_save_screenshot(VirtViewerWindow *self,
				   const char *file)
{
	VirtViewerWindowPrivate *priv = self->priv;
	GdkPixbuf *pix = virt_viewer_display_get_pixbuf(VIRT_VIEWER_DISPLAY(priv->display));

	gdk_pixbuf_save(pix, file, "png", NULL,
			"tEXt::Generator App", PACKAGE, NULL);
	gdk_pixbuf_unref(pix);
}

void
virt_viewer_window_menu_file_screenshot(GtkWidget *menu G_GNUC_UNUSED,
					VirtViewerWindow *self)
{
	GtkWidget *dialog;
	VirtViewerWindowPrivate *priv = self->priv;

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
		virt_viewer_window_save_screenshot(self, filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}

void
virt_viewer_window_menu_help_about(GtkWidget *menu G_GNUC_UNUSED,
				   VirtViewerWindow *self)
{
	GtkBuilder *about = virt_viewer_util_load_ui("virt-viewer-about.xml");

	GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(about, "about"));
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), VERSION);

	gtk_builder_connect_signals(about, self);

	gtk_widget_show_all(dialog);

	g_object_unref(G_OBJECT(about));
}


static void
virt_viewer_window_toolbar_setup(VirtViewerWindow *self)
{
	GtkWidget *button;
	VirtViewerWindowPrivate *priv = self->priv;

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
	g_signal_connect(button, "clicked", G_CALLBACK(virt_viewer_window_toolbar_leave_fullscreen), self);

	priv->layout = ViewAutoDrawer_New();

	ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(priv->layout), FALSE);
	ViewOvBox_SetOver(VIEW_OV_BOX(priv->layout), priv->toolbar);
	ViewOvBox_SetUnder(VIEW_OV_BOX(priv->layout), GTK_WIDGET(priv->notebook));
	ViewAutoDrawer_SetOffset(VIEW_AUTODRAWER(priv->layout), -1);
	ViewAutoDrawer_SetFill(VIEW_AUTODRAWER(priv->layout), FALSE);
	ViewAutoDrawer_SetOverlapPixels(VIEW_AUTODRAWER(priv->layout), 1);
	ViewAutoDrawer_SetNoOverlapPixels(VIEW_AUTODRAWER(priv->layout), 0);
	gtk_widget_show(priv->layout);
}

VirtViewerNotebook*
virt_viewer_window_get_notebook (VirtViewerWindow *self)
{
	return VIRT_VIEWER_NOTEBOOK(self->priv->notebook);
}

GtkWindow*
virt_viewer_window_get_window (VirtViewerWindow *self)
{
	return GTK_WINDOW(self->priv->window);
}

static void
virt_viewer_window_pointer_grab(VirtViewerDisplay *display G_GNUC_UNUSED,
				VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv = self->priv;

	priv->grabbed = TRUE;
	virt_viewer_window_update_title(self);
}

static void
virt_viewer_window_pointer_ungrab(VirtViewerDisplay *display G_GNUC_UNUSED,
				  VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv = self->priv;

	priv->grabbed = FALSE;
	virt_viewer_window_update_title(self);
}

static void
virt_viewer_window_keyboard_grab(VirtViewerDisplay *display G_GNUC_UNUSED,
				 VirtViewerWindow *self)
{
	virt_viewer_window_disable_modifiers(self);
}

static void
virt_viewer_window_keyboard_ungrab(VirtViewerDisplay *display G_GNUC_UNUSED,
				   VirtViewerWindow *self)
{
	virt_viewer_window_enable_modifiers(self);
}

void
virt_viewer_window_update_title(VirtViewerWindow *self)
{
	VirtViewerWindowPrivate *priv = self->priv;
	char *title;
	const char *subtitle;

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

void
virt_viewer_window_set_display(VirtViewerWindow *self, VirtViewerDisplay *display)
{
	VirtViewerWindowPrivate *priv;

	g_return_if_fail(VIRT_VIEWER_IS_WINDOW(self));
	g_return_if_fail(display == NULL || VIRT_VIEWER_IS_DISPLAY(display));

	priv = self->priv;
	if (priv->display) {
		gtk_notebook_remove_page(GTK_NOTEBOOK(priv->notebook), 1);
		g_object_unref(priv->display);
		priv->display = NULL;
	}

	if (display != NULL) {
		priv->display = g_object_ref(display);

		gtk_notebook_append_page(GTK_NOTEBOOK(priv->notebook), GTK_WIDGET(display), NULL);
		if (gtk_bin_get_child(GTK_BIN(display)))
			gtk_widget_realize(GTK_WIDGET(gtk_bin_get_child(GTK_BIN(display))));
		gtk_widget_show_all(GTK_WIDGET(display));

		g_signal_connect(display, "display-pointer-grab",
				 G_CALLBACK(virt_viewer_window_pointer_grab), self);
		g_signal_connect(display, "display-pointer-ungrab",
				 G_CALLBACK(virt_viewer_window_pointer_ungrab), self);
		g_signal_connect(display, "display-keyboard-grab",
				 G_CALLBACK(virt_viewer_window_keyboard_grab), self);
		g_signal_connect(display, "display-keyboard-ungrab",
				 G_CALLBACK(virt_viewer_window_keyboard_ungrab), self);
		g_signal_connect(display, "display-desktop-resize",
				 G_CALLBACK(virt_viewer_window_desktop_resize), self);
	}
}

void
virt_viewer_window_set_zoom_level(VirtViewerWindow *self, gint zoom_level)
{
	g_return_if_fail(VIRT_VIEWER_IS_WINDOW(self));

	/* FIXME: turn into a dynamic property */
	self->priv->zoomlevel = zoom_level;
}

GtkMenuItem*
virt_viewer_window_get_menu_displays(VirtViewerWindow *self)
{
	g_return_val_if_fail(VIRT_VIEWER_IS_WINDOW(self), NULL);
	return GTK_MENU_ITEM(gtk_builder_get_object(self->priv->builder, "menu-displays"));
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 *  indent-tabs-mode: t
 * End:
 */
