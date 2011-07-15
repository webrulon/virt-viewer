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

#include <locale.h>

#include "virt-viewer-display.h"
#include "virt-viewer-util.h"

#define VIRT_VIEWER_DISPLAY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), VIRT_VIEWER_TYPE_DISPLAY, VirtViewerDisplayPrivate))

struct _VirtViewerDisplayPrivate
{
	gboolean dirty;
	guint desktopWidth;
	guint desktopHeight;
	guint zoom_level;
	gboolean zoom;
        gint nth_display;
        gint show_hint;
};

static void virt_viewer_display_size_request(GtkWidget *widget,
					     GtkRequisition *requisition);
#if GTK_CHECK_VERSION(3, 0, 0)
static void virt_viewer_display_get_preferred_width(GtkWidget *widget,
						    int *minwidth,
						    int *defwidth);
static void virt_viewer_display_get_preferred_height(GtkWidget *widget,
						     int *minheight,
						     int *defheight);
#endif
static void virt_viewer_display_size_allocate(GtkWidget *widget,
					      GtkAllocation *allocation);
static void virt_viewer_display_set_property(GObject *object,
					     guint prop_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void virt_viewer_display_get_property(GObject *object,
					     guint prop_id,
					     GValue *value,
					     GParamSpec *pspec);

G_DEFINE_ABSTRACT_TYPE(VirtViewerDisplay, virt_viewer_display, GTK_TYPE_BIN)

enum {
	PROP_0,

	PROP_DESKTOP_WIDTH,
	PROP_DESKTOP_HEIGHT,
	PROP_NTH_DISPLAY,
	PROP_ZOOM,
	PROP_ZOOM_LEVEL,
	PROP_SHOW_HINT,
};

static void
virt_viewer_display_class_init(VirtViewerDisplayClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

	object_class->set_property = virt_viewer_display_set_property;
	object_class->get_property = virt_viewer_display_get_property;

#if GTK_CHECK_VERSION(3, 0, 0)
	widget_class->get_preferred_width = virt_viewer_display_get_preferred_width;
	widget_class->get_preferred_height = virt_viewer_display_get_preferred_height;
#else
	widget_class->size_request = virt_viewer_display_size_request;
#endif
	widget_class->size_allocate = virt_viewer_display_size_allocate;

	g_object_class_install_property(object_class,
					PROP_DESKTOP_WIDTH,
					g_param_spec_int("desktop-width",
							 "Width",
							 "Desktop width",
							 100,
							 G_MAXINT32,
							 100,
							 G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
					PROP_DESKTOP_HEIGHT,
					g_param_spec_int("desktop-height",
							 "Height",
							 "Desktop height",
							 100,
							 G_MAXINT32,
							 100,
							 G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
					PROP_ZOOM,
					g_param_spec_boolean("zoom",
							     "Zoom",
							     "Zoom",
							     TRUE,
							     G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
					PROP_ZOOM_LEVEL,
					g_param_spec_int("zoom-level",
							 "Zoom",
							 "Zoom level",
							 10,
							 400,
							 100,
							 G_PARAM_READWRITE));

	g_object_class_install_property(object_class,
					PROP_NTH_DISPLAY,
					g_param_spec_int("nth-display",
							 "Nth display",
							 "Nth display",
							 0,
							 G_MAXINT32,
							 0,
							 G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(object_class,
					PROP_SHOW_HINT,
					g_param_spec_int("show-hint",
							 "Show hint",
							 "Show state hint",
							 0,
							 G_MAXINT32,
							 0,
							 G_PARAM_READABLE));


	g_signal_new("display-pointer-grab",
		     G_OBJECT_CLASS_TYPE(object_class),
		     G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
		     G_STRUCT_OFFSET(VirtViewerDisplayClass, display_pointer_grab),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE,
		     0);

	g_signal_new("display-pointer-ungrab",
		     G_OBJECT_CLASS_TYPE(object_class),
		     G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
		     G_STRUCT_OFFSET(VirtViewerDisplayClass, display_pointer_ungrab),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE,
		     0);

	g_signal_new("display-keyboard-grab",
		     G_OBJECT_CLASS_TYPE(object_class),
		     G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
		     G_STRUCT_OFFSET(VirtViewerDisplayClass, display_keyboard_grab),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE,
		     0);

	g_signal_new("display-keyboard-ungrab",
		     G_OBJECT_CLASS_TYPE(object_class),
		     G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
		     G_STRUCT_OFFSET(VirtViewerDisplayClass, display_keyboard_ungrab),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE,
		     0);

	g_signal_new("display-desktop-resize",
		     G_OBJECT_CLASS_TYPE(object_class),
		     G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
		     G_STRUCT_OFFSET(VirtViewerDisplayClass, display_desktop_resize),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE,
		     0);

	g_type_class_add_private(object_class, sizeof(VirtViewerDisplayPrivate));
}

static void
virt_viewer_display_init(VirtViewerDisplay *display)
{
	gtk_widget_set_has_window(GTK_WIDGET(display), FALSE);
	gtk_widget_set_redraw_on_allocate(GTK_WIDGET(display), FALSE);

	display->priv = VIRT_VIEWER_DISPLAY_GET_PRIVATE(display);

	display->priv->desktopWidth = 100;
	display->priv->desktopHeight = 100;
	display->priv->zoom_level = 100;
	display->priv->zoom = TRUE;
}

GtkWidget*
virt_viewer_display_new(void)
{
	return g_object_new(VIRT_VIEWER_TYPE_DISPLAY, NULL);
}

static void
virt_viewer_display_set_property(GObject *object,
				 guint prop_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	VirtViewerDisplay *display = VIRT_VIEWER_DISPLAY(object);
	VirtViewerDisplayPrivate *priv = display->priv;

	switch (prop_id) {
	case PROP_DESKTOP_WIDTH:
		virt_viewer_display_set_desktop_size(display,
						     g_value_get_int(value),
						     priv->desktopHeight);
		break;
	case PROP_DESKTOP_HEIGHT:
		virt_viewer_display_set_desktop_size(display,
						     priv->desktopWidth,
						     g_value_get_int(value));
		break;
        case PROP_NTH_DISPLAY:
                priv->nth_display = g_value_get_int(value);
                break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
virt_viewer_display_get_property(GObject *object,
				 guint prop_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	VirtViewerDisplay *display = VIRT_VIEWER_DISPLAY(object);
	VirtViewerDisplayPrivate *priv = display->priv;

	switch (prop_id) {
	case PROP_DESKTOP_WIDTH:
		g_value_set_int(value, priv->desktopWidth);
		break;
	case PROP_DESKTOP_HEIGHT:
		g_value_set_int(value, priv->desktopHeight);
		break;
	case PROP_NTH_DISPLAY:
		g_value_set_int(value, priv->nth_display);
		break;
	case PROP_SHOW_HINT:
		g_value_set_int(value, priv->show_hint);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


static gboolean
virt_viewer_display_idle(gpointer opaque)
{
	VirtViewerDisplay *display = opaque;
	VirtViewerDisplayPrivate *priv = display->priv;
	if (!priv->dirty)
		gtk_widget_queue_resize_no_redraw(GTK_WIDGET(display));
	return FALSE;
}


static void
virt_viewer_display_size_request(GtkWidget *widget,
				 GtkRequisition *requisition)
{
	VirtViewerDisplay *display = VIRT_VIEWER_DISPLAY(widget);
	VirtViewerDisplayPrivate *priv = display->priv;
	int border_width = gtk_container_get_border_width(GTK_CONTAINER(widget));

	requisition->width = border_width * 2;
	requisition->height = border_width * 2;

	if (priv->dirty) {
		if (priv->zoom)
			requisition->width += priv->desktopWidth * priv->zoom_level / 100;
		else
			requisition->width += priv->desktopWidth;
	} else {
		requisition->width += 50;
	}
	if (priv->dirty) {
		if (priv->zoom)
			requisition->height += priv->desktopHeight * priv->zoom_level / 100;
		else
			requisition->height += priv->desktopHeight;
	} else {
		requisition->height += 50;
	}

	DEBUG_LOG("Display size request %dx%d (desktop %dx%d)",
		  requisition->width, requisition->height,
		  priv->desktopWidth, priv->desktopHeight);
}


#if GTK_CHECK_VERSION(3, 0, 0)
static void virt_viewer_display_get_preferred_width(GtkWidget *widget,
						    int *minwidth,
						    int *defwidth)
{
	GtkRequisition req;

	virt_viewer_display_size_request(widget, &req);

	*minwidth = *defwidth = req.width;
}


static void virt_viewer_display_get_preferred_height(GtkWidget *widget,
						     int *minheight,
						     int *defheight)
{
	GtkRequisition req;

	virt_viewer_display_size_request(widget, &req);

	*minheight = *defheight = req.height;
}
#endif


static void
virt_viewer_display_size_allocate(GtkWidget *widget,
				  GtkAllocation *allocation)
{
	GtkBin *bin = GTK_BIN(widget);
	VirtViewerDisplay *display = VIRT_VIEWER_DISPLAY(widget);
	VirtViewerDisplayPrivate *priv = display->priv;
	GtkAllocation child_allocation;
	gint width, height;
	gint border_width;
	double desktopAspect;
	double actualAspect;
	GtkWidget *child = gtk_bin_get_child(bin);

	DEBUG_LOG("Allocated %dx%d", allocation->width, allocation->height);
	gtk_widget_set_allocation(widget, allocation);

	desktopAspect = (double)priv->desktopWidth / (double)priv->desktopHeight;

	if (child && gtk_widget_get_visible(child)) {
		border_width = gtk_container_get_border_width(GTK_CONTAINER(display));

		width  = MAX(1, allocation->width - 2 * border_width);
		height = MAX(1, allocation->height - 2 * border_width);
		actualAspect = (double)width / (double)height;

		if (actualAspect > desktopAspect) {
			child_allocation.width = height * desktopAspect;
			child_allocation.height = height;
		} else {
			child_allocation.width = width;
			child_allocation.height = width / desktopAspect;
		}

		child_allocation.x = 0.5 * (width - child_allocation.width) + allocation->x + border_width;
		child_allocation.y = 0.5 * (height - child_allocation.height) + allocation->y + border_width;

		DEBUG_LOG("Child allocate %dx%d", child_allocation.width, child_allocation.height);
		gtk_widget_size_allocate(child, &child_allocation);
	}


	/* This unsets the size request, so that the user can
	 * manually resize the window smaller again
	 */
	if (priv->dirty) {
		g_idle_add(virt_viewer_display_idle, widget);
		priv->dirty = FALSE;
	}
}


void virt_viewer_display_set_desktop_size(VirtViewerDisplay *display,
					  guint width,
					  guint height)
{
	VirtViewerDisplayPrivate *priv = display->priv;

	priv->desktopWidth = width;
	priv->desktopHeight = height;
	priv->dirty = TRUE;

	gtk_widget_queue_resize(GTK_WIDGET(display));
}


void virt_viewer_display_get_desktop_size(VirtViewerDisplay *display,
					  guint *width,
					  guint *height)
{
	VirtViewerDisplayPrivate *priv = display->priv;

	*width = priv->desktopWidth;
	*height = priv->desktopHeight;
}


void virt_viewer_display_set_zoom_level(VirtViewerDisplay *display,
					guint zoom)
{
	VirtViewerDisplayPrivate *priv = display->priv;
	GtkWidget *child = gtk_bin_get_child(GTK_BIN(display));

	if (zoom < 10)
		zoom = 10;
	if (zoom > 400)
		zoom = 400;
	priv->zoom_level = zoom;

	if (child && gtk_widget_get_visible(child)) {
		priv->dirty = TRUE;
		gtk_widget_queue_resize(GTK_WIDGET(display));
	}
}


void virt_viewer_display_set_zoom(VirtViewerDisplay *display,
				  gboolean zoom)
{
	VirtViewerDisplayPrivate *priv = display->priv;
	GtkWidget *child = gtk_bin_get_child(GTK_BIN(display));

	priv->zoom = zoom;
	if (child && gtk_widget_get_visible(child)) {
		priv->dirty = TRUE;
		gtk_widget_queue_resize(GTK_WIDGET(display));
	}
}

void virt_viewer_display_send_keys(VirtViewerDisplay *display,
				   const guint *keyvals, int nkeyvals)
{
	g_return_if_fail(VIRT_VIEWER_IS_DISPLAY(display));

	VIRT_VIEWER_DISPLAY_GET_CLASS(display)->send_keys(display, keyvals, nkeyvals);
}

GdkPixbuf* virt_viewer_display_get_pixbuf(VirtViewerDisplay *display)
{
	g_return_val_if_fail(VIRT_VIEWER_IS_DISPLAY(display), NULL);

	return VIRT_VIEWER_DISPLAY_GET_CLASS(display)->get_pixbuf(display);
}

void virt_viewer_display_set_show_hint(VirtViewerDisplay *self, gint hint)
{
	VirtViewerDisplayPrivate *priv;
	g_return_if_fail(VIRT_VIEWER_IS_DISPLAY(self));

        priv = self->priv;
        if (priv->show_hint == hint)
                return;

        priv->show_hint = hint;
        g_object_notify(G_OBJECT(self), "show-hint");
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
