/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2011 Red Hat,
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

#include "virt-viewer-align.h"
#include "virt-viewer-util.h"


#define VIRT_VIEWER_ALIGN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), VIRT_VIEWER_TYPE_ALIGN, VirtViewerAlignPrivate))

struct _VirtViewerAlignPrivate
{
	gboolean dirty;
	guint preferred_width;
	guint preferred_height;
	guint zoom_level;
	gboolean zoom;
};

static void virt_viewer_align_size_request(GtkWidget *widget,
					   GtkRequisition *requisition);
#if GTK_CHECK_VERSION(3, 0, 0)
static void virt_viewer_align_get_preferred_width(GtkWidget *widget,
						  int *minwidth,
						  int *defwidth);
static void virt_viewer_align_get_preferred_height(GtkWidget *widget,
						   int *minheight,
						   int *defheight);
#endif
static void virt_viewer_align_size_allocate(GtkWidget *widget,
					    GtkAllocation *allocation);
static void virt_viewer_align_set_property(GObject *object,
					   guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec);
static void virt_viewer_align_get_property(GObject *object,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec);

G_DEFINE_TYPE(VirtViewerAlign, virt_viewer_align, GTK_TYPE_BIN)

enum {
	PROP_0,

	PROP_PREFERRED_WIDTH,
	PROP_PREFERRED_HEIGHT,
	PROP_ZOOM,
	PROP_ZOOM_LEVEL,
};

static void
virt_viewer_align_class_init(VirtViewerAlignClass *class)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;

	gobject_class->set_property = virt_viewer_align_set_property;
	gobject_class->get_property = virt_viewer_align_get_property;

#if GTK_CHECK_VERSION(3, 0, 0)
	widget_class->get_preferred_width = virt_viewer_align_get_preferred_width;
	widget_class->get_preferred_height = virt_viewer_align_get_preferred_height;
#else
	widget_class->size_request = virt_viewer_align_size_request;
#endif
	widget_class->size_allocate = virt_viewer_align_size_allocate;

	g_object_class_install_property(gobject_class,
					PROP_PREFERRED_WIDTH,
					g_param_spec_int("preferred-width",
							 "Width",
							 "Preferred width",
							 100,
							 G_MAXINT32,
							 100,
							 G_PARAM_READWRITE));
	g_object_class_install_property(gobject_class,
					PROP_PREFERRED_HEIGHT,
					g_param_spec_int("preferred-height",
							 "Height",
							 "Preferred height",
							 100,
							 G_MAXINT32,
							 100,
							 G_PARAM_READWRITE));
	g_object_class_install_property(gobject_class,
					PROP_ZOOM,
					g_param_spec_boolean("zoom",
							     "Zoom",
							     "Zoom",
							     TRUE,
							     G_PARAM_READWRITE));
	g_object_class_install_property(gobject_class,
					PROP_ZOOM_LEVEL,
					g_param_spec_int("zoom-level",
							 "Zoom",
							 "Zoom level",
							 10,
							 400,
							 100,
							 G_PARAM_READWRITE));

	g_type_class_add_private(gobject_class, sizeof(VirtViewerAlignPrivate));
}

static void
virt_viewer_align_init(VirtViewerAlign *align)
{
	gtk_widget_set_has_window(GTK_WIDGET(align), FALSE);
	gtk_widget_set_redraw_on_allocate(GTK_WIDGET(align), FALSE);

	align->priv = VIRT_VIEWER_ALIGN_GET_PRIVATE(align);

	align->priv->preferred_width = 100;
	align->priv->preferred_height = 100;
	align->priv->zoom_level = 100;
	align->priv->zoom = TRUE;
}

GtkWidget*
virt_viewer_align_new(void)
{
	return g_object_new (VIRT_VIEWER_TYPE_ALIGN, NULL);
}

static void
virt_viewer_align_set_property(GObject *object,
			       guint prop_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	VirtViewerAlign *align = VIRT_VIEWER_ALIGN(object);
	VirtViewerAlignPrivate *priv = align->priv;
   
	switch (prop_id) {
	case PROP_PREFERRED_WIDTH:
		virt_viewer_align_set_preferred_size(align,
						     g_value_get_int(value),
						     priv->preferred_height);
		break;
	case PROP_PREFERRED_HEIGHT:
		virt_viewer_align_set_preferred_size(align,
						     priv->preferred_width,
						     g_value_get_int(value));
		break;
    
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
virt_viewer_align_get_property(GObject *object,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	VirtViewerAlign *align = VIRT_VIEWER_ALIGN(object);
	VirtViewerAlignPrivate *priv = align->priv;
   
	switch (prop_id) {
	case PROP_PREFERRED_WIDTH:
		g_value_set_int(value, priv->preferred_width);
		break;
	case PROP_PREFERRED_HEIGHT:
		g_value_set_int(value, priv->preferred_height);
		break;
      
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


static gboolean
virt_viewer_align_idle(gpointer opaque)
{
	VirtViewerAlign *align = opaque;
	VirtViewerAlignPrivate *priv = align->priv;
	if (!priv->dirty)
		gtk_widget_queue_resize_no_redraw(GTK_WIDGET(align));
	return FALSE;
}


static void
virt_viewer_align_size_request(GtkWidget *widget,
			       GtkRequisition *requisition)
{
	VirtViewerAlign *align = VIRT_VIEWER_ALIGN(widget);
	VirtViewerAlignPrivate *priv = align->priv;
	int border_width = gtk_container_get_border_width(GTK_CONTAINER(widget));

	requisition->width = border_width * 2;
	requisition->height = border_width * 2;

	if (priv->dirty) {
		if (priv->zoom)
			requisition->width += priv->preferred_width * priv->zoom_level / 100;
		else
			requisition->width += priv->preferred_width;
	} else {
		requisition->width += 50;
	}
	if (priv->dirty) {
		if (priv->zoom)
			requisition->height += priv->preferred_height * priv->zoom_level / 100;
		else
			requisition->height += priv->preferred_height;
	} else {
		requisition->height += 50;
	}

	DEBUG_LOG("Align size request %dx%d (preferred %dx%d)",
		  requisition->width, requisition->height,
		  priv->preferred_width, priv->preferred_height);
}


#if GTK_CHECK_VERSION(3, 0, 0)
static void virt_viewer_align_get_preferred_width(GtkWidget *widget,
						  int *minwidth,
						  int *defwidth)
{
	GtkRequisition req;

	virt_viewer_align_size_request(widget, &req);

	*minwidth = *defwidth = req.width;
}


static void virt_viewer_align_get_preferred_height(GtkWidget *widget,
						   int *minheight,
						   int *defheight)
{
	GtkRequisition req;

	virt_viewer_align_size_request(widget, &req);

	*minheight = *defheight = req.height;
}
#endif


static void
virt_viewer_align_size_allocate(GtkWidget *widget,
				GtkAllocation *allocation)
{
	GtkBin *bin = GTK_BIN(widget);
	VirtViewerAlign *align = VIRT_VIEWER_ALIGN(widget);
	VirtViewerAlignPrivate *priv = align->priv;
	GtkAllocation child_allocation;
	gint width, height;
	gint border_width;
	double preferredAspect;
	double actualAspect;
	GtkWidget *child = gtk_bin_get_child(bin);

	DEBUG_LOG("Allocated %dx%d", allocation->width, allocation->height);
	gtk_widget_set_allocation(widget, allocation);

	preferredAspect = (double)priv->preferred_width / (double)priv->preferred_height;

	if (child && gtk_widget_get_visible(child)) {
		border_width = gtk_container_get_border_width(GTK_CONTAINER(align));

		width  = MAX(1, allocation->width - 2 * border_width);
		height = MAX(1, allocation->height - 2 * border_width);
		actualAspect = (double)width / (double)height;

		if (actualAspect > preferredAspect) {
			child_allocation.width = height * preferredAspect;
			child_allocation.height = height;
		} else {
			child_allocation.width = width;
			child_allocation.height = width / preferredAspect;
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
		g_idle_add(virt_viewer_align_idle, widget);
		priv->dirty = FALSE;
	}
}


void virt_viewer_align_set_preferred_size(VirtViewerAlign *align,
					  guint width,
					  guint height)
{
	VirtViewerAlignPrivate *priv = align->priv;

	priv->preferred_width = width;
	priv->preferred_height = height;
	priv->dirty = TRUE;

	gtk_widget_queue_resize(GTK_WIDGET(align));
}

void virt_viewer_align_set_zoom_level(VirtViewerAlign *align,
				      guint zoom)
{
	VirtViewerAlignPrivate *priv = align->priv;
	GtkWidget *child = gtk_bin_get_child(GTK_BIN(align));

	if (zoom < 10)
		zoom = 10;
	if (zoom > 400)
		zoom = 400;
	priv->zoom_level = zoom;

	if (child && gtk_widget_get_visible(child)) {
		priv->dirty = TRUE;
		gtk_widget_queue_resize(GTK_WIDGET(align));
	}
}


void virt_viewer_align_zoom_in(VirtViewerAlign *align)
{
	VirtViewerAlignPrivate *priv = align->priv;
	virt_viewer_align_set_zoom_level(align, priv->zoom_level + 10);
}

void virt_viewer_align_zoom_out(VirtViewerAlign *align)
{
	VirtViewerAlignPrivate *priv = align->priv;
	virt_viewer_align_set_zoom_level(align, priv->zoom_level - 10);
}

void virt_viewer_align_zoom_normal(VirtViewerAlign *align)
{
	virt_viewer_align_set_zoom_level(align, 100);
}


void virt_viewer_align_set_zoom(VirtViewerAlign *align,
				gboolean zoom)
{
	VirtViewerAlignPrivate *priv = align->priv;
	GtkWidget *child = gtk_bin_get_child(GTK_BIN(align));

	priv->zoom = zoom;
	if (child && gtk_widget_get_visible(child)) {
		priv->dirty = TRUE;
		gtk_widget_queue_resize(GTK_WIDGET(align));
	}
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
