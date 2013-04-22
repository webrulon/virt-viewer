/* *************************************************************************
 * Copyright (c) 2005 VMware, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * *************************************************************************/

/*
 * ovBox.c --
 *
 *      Implementation of a GTK+ overlapping box. Allows you to display and
 *      quickly move a child that overlaps another child.
 *
 *      Implementation notes
 *      --------------------
 *
 *      Changing 'fraction' is fast (we just move the 'overWin' X window, which
 *      ultimately copies a rectangle on the X server side), and does not
 *      flicker (the 'under' and 'over' GTK children are not re-drawn, except
 *      for parts of them that become exposed).
 *
 *      o Initially, we thought it could be done with only 2 X windows
 *
 *        Layout                  Hierarchy
 *        ------                  ---------
 *
 *          /- overWin --\        underWin
 *          |            |           overWin
 *        /-+- underWin -+-\
 *        | |            | |
 *        | \------------/ |
 *        |                |
 *        \----------------/
 *
 *        But the 'under' GTK child could create other X windows inside
 *        'underWin', which makes it impossible to guarantee that 'overWin'
 *        will stay stacked on top.
 *
 *      o So we are forced to use 3 X windows
 *
 *        Layout                  Hierarchy
 *        ------                  ---------
 *
 *            /- overWin --\      window
 *            |            |         overWin
 *        /---+- window ---+---\     underWin
 *        |   |            |   |
 *        | /-+- underWin -+-\ |
 *        | | |            | | |
 *        | | \------------/ | |
 *        | |                | |
 *        | \----------------/ |
 *        |                    |
 *        \--------------------/
 *
 *  --hpreg
 */

#include <config.h>

#include "ovBox.h"

#if ! GTK_CHECK_VERSION(3, 0, 0)
#define gtk_widget_set_realized(widget, val)        \
  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED)
#define gtk_widget_get_realized(widget)                \
  GTK_WIDGET_REALIZED(widget)
#endif

struct _ViewOvBoxPrivate
{
   GdkWindow *underWin;
   GtkWidget *under;
   GdkWindow *overWin;
   GtkWidget *over;
   GtkRequisition overR;
   unsigned int min;
   double fraction;
   gint verticalOffset;
};

#define VIEW_OV_BOX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), VIEW_TYPE_OV_BOX, ViewOvBoxPrivate))

/* The unaltered parent class. */
static GtkBoxClass *parentClass;


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxInit --
 *
 *      Initialize a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxInit(GTypeInstance *instance, // IN
              gpointer klass G_GNUC_UNUSED)          // Unused
{
   ViewOvBox *that;
   ViewOvBoxPrivate *priv;

   that = VIEW_OV_BOX(instance);
   that->priv = VIEW_OV_BOX_GET_PRIVATE(that);
   priv = that->priv;

   gtk_widget_set_has_window (GTK_WIDGET (that), TRUE);

   priv->underWin = NULL;
   priv->under = NULL;
   priv->overWin = NULL;
   priv->over = NULL;
   priv->overR.height = -1;
   priv->overR.width = -1;
   priv->min = 0;
   priv->fraction = 0;
   priv->verticalOffset = 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxMap --
 *
 *      "map" method of a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxMap(GtkWidget *widget) // IN
{
   gdk_window_show(gtk_widget_get_window (widget));
   GTK_WIDGET_CLASS(parentClass)->map(widget);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxUnmap --
 *
 *      "unmap" method of a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxUnmap(GtkWidget *widget) // IN
{
   gdk_window_hide(gtk_widget_get_window (widget));
   GTK_WIDGET_CLASS(parentClass)->unmap(widget);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxGetActualMin --
 *
 *      Retrieve the actual 'min' value, i.e. a value that is guaranteed not to
 *      exceed the height of the 'over' child.
 *
 * Results:
 *      The actual 'min' value.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static inline unsigned int
ViewOvBoxGetActualMin(ViewOvBox *that) // IN
{
   return MIN(that->priv->min, that->priv->overR.height);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxGetUnderGeometry --
 *
 *      Retrieve the geometry to apply to 'that->underWin'.
 *
 * Results:
 *      The geometry
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxGetUnderGeometry(ViewOvBox *that, // IN
                          int *x,          // OUT
                          int *y,          // OUT
                          int *width,      // OUT
                          int *height)     // OUT
{
   unsigned int min;
   GtkAllocation allocation;

   min = ViewOvBoxGetActualMin(that);
   gtk_widget_get_allocation (GTK_WIDGET(that), &allocation);

   *x = 0;
   *y = min;
   *width = allocation.width;
   *height = allocation.height - min;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxGetOverGeometry --
 *
 *      Retrieve the geometry to apply to 'that->overWin'.
 *
 * Results:
 *      The geometry
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxGetOverGeometry(ViewOvBox *that, // IN
                         int *x,          // OUT
                         int *y,          // OUT
                         int *width,      // OUT
                         int *height)     // OUT
{
   ViewOvBoxPrivate *priv;
   gboolean expand;
   gboolean fill;
   guint padding;
   unsigned int boxWidth;
   GtkAllocation allocation;

   priv = that->priv;

   if (priv->over) {
      /*
       * When a child's expand or fill property changes, GtkBox queues
       * a resize for the child.
       */
      gtk_container_child_get(GTK_CONTAINER(that), priv->over,
                              "expand", &expand,
                              "fill", &fill,
                              "padding", &padding,
                              NULL);
   } else {
      /* Default values used by GtkBox. */
      expand = TRUE;
      fill = TRUE;
      padding = 0;
   }

   gtk_widget_get_allocation(GTK_WIDGET(that), &allocation);
   boxWidth = allocation.width;
   if (!expand) {
      *width = MIN(priv->overR.width, boxWidth - padding);
      *x = padding;
   } else if (!fill) {
      *width = MIN(priv->overR.width, boxWidth);
      *x = (boxWidth - *width) / 2;
   } else {
      *width = boxWidth;
      *x = 0;
   }

   *y =   (priv->overR.height - ViewOvBoxGetActualMin(that))
        * (priv->fraction - 1) + priv->verticalOffset;
   *height = priv->overR.height;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxSetBackground --
 *
 *      Set the background color of the 'underWin' and 'overWin' X windows.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxSetBackground(ViewOvBox *that) // IN
{
   GtkWidget *widget = GTK_WIDGET(that);

#if GTK_CHECK_VERSION(3, 0, 0)
   GtkStyleContext *stylecontext;

   stylecontext = gtk_widget_get_style_context(widget);
   gtk_style_context_set_background(stylecontext, gtk_widget_get_window(widget));
   gtk_style_context_set_background(stylecontext, that->priv->underWin);
   gtk_style_context_set_background(stylecontext, that->priv->overWin);
#else
   GtkStyle *style;

   style = gtk_widget_get_style (widget);
   gtk_style_set_background(style, gtk_widget_get_window(widget), GTK_STATE_NORMAL);
   gtk_style_set_background(style, that->priv->underWin, GTK_STATE_NORMAL);
   gtk_style_set_background(style, that->priv->overWin, GTK_STATE_NORMAL);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxRealize --
 *
 *      "realize" method of a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxRealize(GtkWidget *widget) // IN
{
   ViewOvBox *that;
   ViewOvBoxPrivate *priv;
   GdkWindowAttr attributes;
   gint mask;
   GtkAllocation allocation;
   GdkWindow *window;

   gtk_widget_set_realized (widget, TRUE);

   that = VIEW_OV_BOX(widget);
   priv = that->priv;

   attributes.window_type = GDK_WINDOW_CHILD;
   attributes.wclass = GDK_INPUT_OUTPUT;
   attributes.visual = gtk_widget_get_visual(widget);
   attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;
   mask = GDK_WA_VISUAL | GDK_WA_X | GDK_WA_Y;

   gtk_widget_get_allocation(widget, &allocation);
   attributes.x = allocation.x;
   attributes.y = allocation.y;
   attributes.width = allocation.width;
   attributes.height = allocation.height;
   window = gdk_window_new(gtk_widget_get_parent_window(widget),
                           &attributes, mask);
   gtk_widget_set_window(widget, window);
   gdk_window_set_user_data(window, that);
#if !GTK_CHECK_VERSION(3, 0, 0)
   gtk_widget_set_style(widget, gtk_style_attach(gtk_widget_get_style(widget), window));
#endif

   /*
    * The order in which we create the children X window matters: the child
    * created last is stacked on top. --hpreg
    */

   ViewOvBoxGetUnderGeometry(that, &attributes.x, &attributes.y,
                             &attributes.width, &attributes.height);
   priv->underWin = gdk_window_new(window, &attributes, mask);
   gdk_window_set_user_data(priv->underWin, that);
   if (priv->under) {
      gtk_widget_set_parent_window(priv->under, priv->underWin);
   }
   gdk_window_show(priv->underWin);

   ViewOvBoxGetOverGeometry(that, &attributes.x, &attributes.y,
                            &attributes.width, &attributes.height);
   priv->overWin = gdk_window_new(window, &attributes, mask);
   gdk_window_set_user_data(priv->overWin, that);
   if (priv->over) {
      gtk_widget_set_parent_window(priv->over, priv->overWin);
   }
   gdk_window_show(priv->overWin);

   ViewOvBoxSetBackground(that);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxUnrealize --
 *
 *      "unrealize" method of a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxUnrealize(GtkWidget *widget) // IN
{
   ViewOvBox *that;
   ViewOvBoxPrivate *priv;

   that = VIEW_OV_BOX(widget);
   priv = that->priv;

   /*
    * Unrealize the parent before destroying the windows so that we end up
    * unrealizing all the child widgets before destroying the child windows,
    * giving them a chance to reparent their windows before we clobber them.
    */
   GTK_WIDGET_CLASS(parentClass)->unrealize(widget);


   gdk_window_set_user_data(priv->underWin, NULL);
   gdk_window_destroy(priv->underWin);
   priv->underWin = NULL;

   gdk_window_set_user_data(priv->overWin, NULL);
   gdk_window_destroy(priv->overWin);
   priv->overWin = NULL;

}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxRealSizeRequest --
 *
 *      "size_request" method, generalized to work with both gtk-2 and 3.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
static void
ViewOvBoxRealSizeRequest(GtkWidget *widget,           // IN
   GtkRequisition *min_in, GtkRequisition *nat_in,      // IN
   GtkRequisition *min_out, GtkRequisition *nat_out)    // OUT
{
   ViewOvBox *that;
   ViewOvBoxPrivate *priv;
   gboolean expand;
   gboolean fill;
   guint padding;
   unsigned int min;

   that = VIEW_OV_BOX(widget);
   priv = that->priv;

#if GTK_CHECK_VERSION(3, 0, 0)
   gtk_widget_get_preferred_size(priv->over, NULL, &priv->overR);
#else
   gtk_widget_size_request(priv->over, &priv->overR);
#endif

   gtk_container_child_get(GTK_CONTAINER(that), priv->over,
                           "expand", &expand,
                           "fill", &fill,
                           "padding", &padding,
                           NULL);

   min = ViewOvBoxGetActualMin(that);

   if (min_out) {
      min_out->width = MAX(min_in->width, priv->overR.width +
                                            ((expand || fill) ? 0 : padding));
      min_out->height = MAX(min_in->height + min, priv->overR.height);
   }
   if (nat_out) {
      nat_out->width = MAX(nat_in->width, priv->overR.width +
                                            ((expand || fill) ? 0 : padding));
      nat_out->height = MAX(nat_in->height + min, priv->overR.height);
   }
}

#if GTK_CHECK_VERSION(3, 0, 0)
static void
ViewOvBox_get_preferred_width (GtkWidget *widget,
                               gint      *minimal_width,
                               gint      *natural_width)
{
   ViewOvBoxPrivate *priv = VIEW_OV_BOX(widget)->priv;
   GtkRequisition min_in, nat_in, min_out, nat_out;

   gtk_widget_get_preferred_size(priv->under, &min_in, &nat_in);

   ViewOvBoxRealSizeRequest(widget, &min_in, &nat_in, &min_out, &nat_out);

   *minimal_width = min_out.width;
   *natural_width = nat_out.width;
}

static void
ViewOvBox_get_preferred_height (GtkWidget *widget,
                                gint      *minimal_height,
                                gint      *natural_height)
{
   ViewOvBoxPrivate *priv = VIEW_OV_BOX(widget)->priv;
   GtkRequisition min_in, nat_in, min_out, nat_out;

   gtk_widget_get_preferred_size(priv->under, &min_in, &nat_in);

   ViewOvBoxRealSizeRequest(widget, &min_in, &nat_in, &min_out, &nat_out);

   *minimal_height = min_out.height;
   *natural_height = nat_out.height;
}

#else

static void
ViewOvBoxSizeRequest(GtkWidget *widget,           // IN
                     GtkRequisition *requisition) // OUT
{
   ViewOvBoxPrivate *priv = VIEW_OV_BOX(widget)->priv;
   GtkRequisition min;

   gtk_widget_size_request(priv->under, &min);

   ViewOvBoxRealSizeRequest(widget, &min, NULL, requisition, NULL);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxSizeAllocate --
 *
 *      "size_allocate" method of a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxSizeAllocate(GtkWidget *widget,         // IN
                      GtkAllocation *allocation) // IN
{
   ViewOvBox *that;
   ViewOvBoxPrivate *priv;
   GtkAllocation under;
   GtkAllocation over;

   gtk_widget_set_allocation (widget, allocation);

   that = VIEW_OV_BOX(widget);
   priv = that->priv;

   ViewOvBoxGetUnderGeometry(that, &under.x, &under.y, &under.width,
                             &under.height);
   ViewOvBoxGetOverGeometry(that, &over.x, &over.y, &over.width, &over.height);

   if (gtk_widget_get_realized(widget)) {
      gdk_window_move_resize(gtk_widget_get_window(widget),
                             allocation->x, allocation->y,
                             allocation->width, allocation->height);
      gdk_window_move_resize(priv->underWin, under.x, under.y, under.width,
                             under.height);
      gdk_window_move_resize(priv->overWin, over.x, over.y, over.width,
                             over.height);
   }

   under.x = 0;
   under.y = 0;
   gtk_widget_size_allocate(priv->under, &under);
   over.x = 0;
   over.y = 0;
   gtk_widget_size_allocate(priv->over, &over);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxStyleSet --
 *
 *      "style_set" method of a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxStyleSet(GtkWidget *widget,       // IN
                  GtkStyle *previousStyle) // IN: Unused
{
   ViewOvBox *that;

   that = VIEW_OV_BOX(widget);

   if (gtk_widget_get_realized(widget)) {
      ViewOvBoxSetBackground(that);
   }

   GTK_WIDGET_CLASS(parentClass)->style_set(widget, previousStyle);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxSetChild --
 *
 *      Set a child of a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxSetChild(ViewOvBox *that,     // IN
                  GtkWidget **child,   // IN
                  GdkWindow *childWin, // IN
                  GtkWidget *widget)   // IN
{
   GtkWidget *oldChild = *child;

   if (oldChild) {
      g_object_ref(oldChild);
      gtk_container_remove(GTK_CONTAINER(that), oldChild);
   }

   *child = widget;
   if (*child) {
      gtk_widget_set_parent_window(widget, childWin);
      gtk_container_add(GTK_CONTAINER(that), *child);
   }

   if (oldChild) {
      g_object_unref(oldChild);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxSetOver --
 *
 *      Base implementation of ViewOvBox_SetOver.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxSetOver(ViewOvBox *that,   // IN
                 GtkWidget *widget) // IN
{
   ViewOvBoxSetChild(that, &that->priv->over, that->priv->overWin, widget);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBoxClassInit --
 *
 *      Initialize the ViewOvBoxClass.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewOvBoxClassInit(ViewOvBoxClass *klass) // IN
{
   GtkWidgetClass *widgetClass;

   widgetClass = GTK_WIDGET_CLASS(klass);

   widgetClass->map = ViewOvBoxMap;
   widgetClass->unmap = ViewOvBoxUnmap;
   widgetClass->realize = ViewOvBoxRealize;
   widgetClass->unrealize = ViewOvBoxUnrealize;
#if GTK_CHECK_VERSION(3, 0, 0)
   widgetClass->get_preferred_width = ViewOvBox_get_preferred_width;
   widgetClass->get_preferred_height = ViewOvBox_get_preferred_height;
#else
   widgetClass->size_request = ViewOvBoxSizeRequest;
#endif
   widgetClass->size_allocate = ViewOvBoxSizeAllocate;
   widgetClass->style_set = ViewOvBoxStyleSet;

   klass->set_over = ViewOvBoxSetOver;

   parentClass = g_type_class_peek_parent(klass);

   g_type_class_add_private(klass, sizeof(ViewOvBoxPrivate));
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBox_GetType --
 *
 *      Get the (memoized) GType of the ViewOvBox GTK+ object.
 *
 * Results:
 *      The GType
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

GType
ViewOvBox_GetType(void)
{
   static GType type = 0;

   if (type == 0) {
      static const GTypeInfo info = {
         sizeof (ViewOvBoxClass),
         NULL, /* BaseInit */
         NULL, /* BaseFinalize */
         (GClassInitFunc)ViewOvBoxClassInit,
         NULL,
         NULL, /* Class Data */
         sizeof (ViewOvBox),
         0, /* n_preallocs */
         (GInstanceInitFunc)ViewOvBoxInit,
         NULL,
      };

      type = g_type_register_static(GTK_TYPE_BOX, "ViewOvBox", &info, 0);
   }

   return type;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBox_New --
 *
 *      Create a new ViewOvBox GTK+ widget.
 *
 * Results:
 *      The widget
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

GtkWidget *
ViewOvBox_New(void)
{
   ViewOvBox *that;

   that = VIEW_OV_BOX(g_object_new(VIEW_TYPE_OV_BOX, NULL));

   return GTK_WIDGET(that);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBox_SetUnder --
 *
 *      Set the under widget of a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
ViewOvBox_SetUnder(ViewOvBox *that,   // IN
                   GtkWidget *widget) // IN
{
   g_return_if_fail(that != NULL);

   ViewOvBoxSetChild(that, &that->priv->under, that->priv->underWin, widget);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBox_SetOver --
 *
 *      Set the over widget of a ViewOvBox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
ViewOvBox_SetOver(ViewOvBox *that,   // IN
                  GtkWidget *widget) // IN
{
   g_return_if_fail(that != NULL);

   VIEW_OV_BOX_GET_CLASS(that)->set_over(that, widget);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBox_SetMin --
 *
 *      Set the 'min' property of a ViewOvBox, i.e. the number of pixel of the
 *      'over' child that should always be displayed without overlapping on the
 *      'under' child.
 *
 *      Using a value of -1 displays the 'over' child entirely.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
ViewOvBox_SetMin(ViewOvBox *that,  // IN
                 unsigned int min) // IN
{
   g_return_if_fail(that != NULL);

   that->priv->min = min;
   gtk_widget_queue_resize(GTK_WIDGET(that));
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBox_SetFraction --
 *
 *      Set the 'fraction' property of a ViewOvBox, i.e. how much of the 'over'
 *      child should overlap on the 'under' child.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
ViewOvBox_SetFraction(ViewOvBox *that, // IN
                      double fraction) // IN
{
   g_return_if_fail(that != NULL);
   g_return_if_fail(fraction >=0 && fraction <= 1);

   that->priv->fraction = fraction;
   if (gtk_widget_get_realized(GTK_WIDGET (that))) {
      int x;
      int y;
      int width;
      int height;

      ViewOvBoxGetOverGeometry(that, &x, &y, &width, &height);
      gdk_window_move(that->priv->overWin, x, y);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewOvBox_GetFraction --
 *
 *      Retrieve the 'fraction' property of a ViewOvBox.
 *
 * Results:
 *      The value
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

double
ViewOvBox_GetFraction(ViewOvBox *that)
{
   g_return_val_if_fail(that != NULL, 0);

   return that->priv->fraction;
}
