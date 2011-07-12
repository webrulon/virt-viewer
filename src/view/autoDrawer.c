/* *************************************************************************
 * Copyright (c) 2005 VMware Inc.
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
 * autoDrawer.c -
 *
 *    Subclass of ViewDrawer that encapsulates the behaviour typically required
 *    when using the drawer to implement a menu/toolbar that auto-opens when
 *    moused-over and auto-closes when the mouse leaves.
 */


#include "autoDrawer.h"


struct _ViewAutoDrawerPrivate
{
   gboolean active;
   gboolean pinned;
   gboolean inputUngrabbed;

   gboolean opened;
   gboolean forceClosing;

   gboolean fill;
   gint offset;

   guint closeConnection;
   guint delayConnection;
   guint delayValue;
   guint overlapPixels;
   guint noOverlapPixels;

   GtkWidget *over;
   GtkWidget *evBox;
};

#define VIEW_AUTODRAWER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), VIEW_TYPE_AUTODRAWER, ViewAutoDrawerPrivate))

/* The unaltered parent class. */
static ViewDrawerClass *parentClass;


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerEnforce --
 *
 *      Enforce an AutoDrawer's goal now.
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
ViewAutoDrawerEnforce(ViewAutoDrawer *that, // IN
                      gboolean animate)     // IN
{
   double fraction;
   GtkAllocation allocation;
   ViewAutoDrawerPrivate *priv = that->priv;

   if (!priv->active) {
      ViewOvBox_SetMin(VIEW_OV_BOX(that), -1);
      ViewOvBox_SetFraction(VIEW_OV_BOX(that), 0);
      return;
   }

   g_assert(priv->over != NULL);
   g_assert(GTK_IS_WIDGET(priv->over));

   ViewOvBox_SetMin(VIEW_OV_BOX(that), priv->noOverlapPixels);

   // The forceClosing flag overrides the opened flag.
   if (priv->opened && !priv->forceClosing) {
      fraction = 1;
   } else {
      gtk_widget_get_allocation (priv->over, &allocation);
      fraction = ((double)priv->overlapPixels / allocation.height);
   }

   if (!animate) {
      ViewOvBox_SetFraction(VIEW_OV_BOX(that), fraction);
   }
   ViewDrawer_SetGoal(VIEW_DRAWER(that), fraction);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerOnEnforceDelay --
 *
 *      Callback fired when a delayed update happens to update the drawer state.
 *
 * Results:
 *      FALSE to indicate timer should not repeat.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
ViewAutoDrawerOnEnforceDelay(ViewAutoDrawer *that) // IN
{
   that->priv->delayConnection = 0;
   ViewAutoDrawerEnforce(that, TRUE);

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerOnCloseDelay --
 *
 *      Callback fired when the drawer is closed manually. This prevents the
 *      drawer from reopening right away.
 *
 * Results:
 *      FALSE to indicate timer should not repeat.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
ViewAutoDrawerOnCloseDelay(ViewAutoDrawer *that) // IN
{
   that->priv->closeConnection = 0;
   that->priv->forceClosing = FALSE;

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerUpdate --
 *
 *      Decide whether an AutoDrawer should be opened or closed, and enforce
 *      that decision now or later.
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
ViewAutoDrawerUpdate(ViewAutoDrawer *that, // IN
                     gboolean immediate)   // IN
{
   ViewAutoDrawerPrivate *priv = that->priv;
   GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(that));
   GtkWindow *window;
   GtkAllocation allocation;

   if (!toplevel || !gtk_widget_is_toplevel(toplevel)) {
      // The autoDrawer cannot function properly without a toplevel.
      return;
   }
   window = GTK_WINDOW(toplevel);

   /*
    * We decide to open the drawer by OR'ing several conditions. Evaluating a
    * condition can have the side-effect of setting 'immediate' to TRUE, so we
    * cannot stop evaluating the conditions after we have found one to be TRUE.
    */

   priv->opened = FALSE;

   /* Is the AutoDrawer pinned? */

   if (priv->pinned) {
      immediate = TRUE;

      priv->opened = TRUE;
   }

   /* Is the mouse cursor inside the event box? */

   {
      int x;
      int y;

      gtk_widget_get_pointer(priv->evBox, &x, &y);
      gtk_widget_get_allocation(priv->evBox, &allocation);
      g_assert(gtk_container_get_border_width(   GTK_CONTAINER(priv->evBox))
                                              == 0);
      if (   (guint)x < (guint)allocation.width
          && (guint)y < (guint)allocation.height) {
         priv->opened = TRUE;
      }
   }

   /* If there is a focused widget, is it inside the event box? */

   {
      GtkWidget *focus;

      focus = gtk_window_get_focus(window);
      if (focus && gtk_widget_is_ancestor(focus, priv->evBox)) {
         /*
          * Override the default 'immediate' to make sure the 'over' widget
          * immediately appears along with the widget the focused widget.
          */
         immediate = TRUE;

         priv->opened = TRUE;
      }
   }

   /* If input is grabbed, is it on behalf of a widget inside the event box? */

   if (!priv->inputUngrabbed) {
      GtkWidget *grabbed = NULL;

#if GTK_CHECK_VERSION(3, 0, 0)
      if (gtk_window_has_group (window)) {
        GtkWindowGroup *group = gtk_window_get_group (window);
        grabbed = gtk_window_group_get_current_grab (group);
      }
#else
      if (window->group && window->group->grabs) {
	grabbed = GTK_WIDGET(window->group->grabs->data);
      }
#endif
      if (!grabbed) {
         grabbed = gtk_grab_get_current();
      }

      if (grabbed && GTK_IS_MENU(grabbed)) {
         /*
          * With cascading menus, the deepest menu owns the grab. Traverse the
          * menu hierarchy up until we reach the attach widget for the whole
          * hierarchy.
          */

         for (;;) {
            GtkWidget *menuAttach;
            GtkWidget *menuItemParent;

            menuAttach = gtk_menu_get_attach_widget(GTK_MENU(grabbed));
            if (!menuAttach) {
               /*
                * It is unfortunately not mandatory for a menu to have a proper
                * attach widget set.
                */
               break;
            }

            grabbed = menuAttach;
            if (!GTK_IS_MENU_ITEM(grabbed)) {
               break;
            }

            menuItemParent = gtk_widget_get_parent(grabbed);
            g_return_if_fail(menuItemParent);
            if (!GTK_IS_MENU(menuItemParent)) {
               break;
            }

            grabbed = menuItemParent;
         }
      }

      if (grabbed && gtk_widget_is_ancestor(grabbed, priv->evBox)) {
         /*
          * Override the default 'immediate' to make sure the 'over' widget
          * immediately appears along with the widget the grab happens on
          * behalf of.
          */
         immediate = TRUE;

         priv->opened = TRUE;
      }
   }

   if (priv->delayConnection) {
      g_source_remove(priv->delayConnection);
   }

   if (priv->forceClosing) {
      ViewAutoDrawerEnforce(that, TRUE);
   } else if (immediate) {
      ViewAutoDrawerEnforce(that, FALSE);
   } else {
      priv->delayConnection = g_timeout_add(priv->delayValue,
         (GSourceFunc)ViewAutoDrawerOnEnforceDelay, that);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerOnOverEnterLeave --
 *
 *      Respond to enter/leave events by doing a delayed update of the drawer
 *      state.
 *
 * Results:
 *      FALSE to indicate event was not handled.
 *
 * Side effects:
 *      Will queue delayed update.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
ViewAutoDrawerOnOverEnterLeave(GtkWidget *evBox G_GNUC_UNUSED,        // IN: Unused
                               GdkEventCrossing *event G_GNUC_UNUSED, // IN
                               ViewAutoDrawer *that)    // IN
{
   /*
    * This change happens in response to user input. By default, give the user
    * some time to correct his input before reacting to the change.
    */
   ViewAutoDrawerUpdate(that, FALSE);

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerOnGrabNotify --
 *
 *      Respond to grab notifications by updating the drawer state.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Might queue delayed update.
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewAutoDrawerOnGrabNotify(GtkWidget *evBox G_GNUC_UNUSED,     // IN: Unused
                           gboolean ungrabbed,   // IN
                           ViewAutoDrawer *that) // IN
{
   ViewAutoDrawerPrivate *priv = that->priv;

   priv->inputUngrabbed = ungrabbed;

   /*
    * This change happens in response to user input. By default, give the user
    * some time to correct his input before reacting to the change.
    */
   ViewAutoDrawerUpdate(that, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerOnSetFocus --
 *
 *      Respond to changes in the focus widget of the autoDrawer's toplevel
 *      by recalculating the state.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated.
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewAutoDrawerOnSetFocus(GtkWindow *window G_GNUC_UNUSED,    // IN
                         GtkWidget *widget G_GNUC_UNUSED,    // IN
                         ViewAutoDrawer *that) // IN
{
   /*
    * This change happens in response to user input. By default, give the user
    * some time to correct his input before reacting to the change.
    */
   ViewAutoDrawerUpdate(that, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerOnHierarchyChanged --
 *
 *      Respond to changes in the toplevel for the AutoDrawer. A toplevel is
 *      required for the AutoDrawer to calculate its state.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated.
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewAutoDrawerOnHierarchyChanged(ViewAutoDrawer *that,   // IN
				 GtkWidget *oldToplevel) // IN
{
   GtkWidget *newToplevel = gtk_widget_get_toplevel(GTK_WIDGET(that));

   if (oldToplevel && gtk_widget_is_toplevel(oldToplevel)) {
      g_signal_handlers_disconnect_by_func(oldToplevel,
                                           G_CALLBACK(ViewAutoDrawerOnSetFocus),
                                           that);
   }

   if (newToplevel && gtk_widget_is_toplevel(newToplevel)) {
      g_signal_connect_after(newToplevel, "set-focus",
                             G_CALLBACK(ViewAutoDrawerOnSetFocus), that);
   }

   /* This change happens programmatically. Always react to it immediately. */
   ViewAutoDrawerUpdate(that, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerSetOver --
 *
 *      Virtual method override so that the user's over widget is placed
 *      inside the AutoDrawer's event box.
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
ViewAutoDrawerSetOver(ViewOvBox *ovBox,  // IN
                      GtkWidget *widget) // IN
{
   ViewAutoDrawer *that = VIEW_AUTODRAWER(ovBox);
   ViewAutoDrawerPrivate *priv = that->priv;
   GtkWidget *oldChild = gtk_bin_get_child(GTK_BIN(priv->evBox));

   if (oldChild) {
      g_object_ref(oldChild);
      gtk_container_remove(GTK_CONTAINER(priv->evBox), oldChild);
   }

   if (widget) {
      gtk_container_add(GTK_CONTAINER(priv->evBox), widget);
   }

   if (oldChild) {
      g_object_unref(oldChild);
   }

   priv->over = widget;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerRefreshPacking --
 *
 *      Sets the actual packing values for fill, expand, and packing
 *      given internal settings.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated.
 *
 *-----------------------------------------------------------------------------
 */

static void
ViewAutoDrawerRefreshPacking(ViewAutoDrawer *that) // IN
{
   gboolean expand;
   gboolean fill;
   guint padding;

   expand = (that->priv->fill || (that->priv->offset < 0));
   fill = that->priv->fill;
   padding = (expand || fill) ? 0 : that->priv->offset;

   gtk_box_set_child_packing(GTK_BOX(that), that->priv->evBox,
                             expand, fill, padding, GTK_PACK_START);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerInit --
 *
 *      Initialize a ViewAutoDrawer.
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
ViewAutoDrawerInit(GTypeInstance *instance, // IN
                   gpointer klass G_GNUC_UNUSED)          // Unused
{
   ViewAutoDrawer *that;
   ViewAutoDrawerPrivate *priv;

   that = VIEW_AUTODRAWER(instance);
   that->priv = VIEW_AUTODRAWER_GET_PRIVATE(that);
   priv = that->priv;

   priv->active = TRUE;
   priv->pinned = FALSE;
   priv->forceClosing = FALSE;
   priv->inputUngrabbed = TRUE;
   priv->delayConnection = 0;
   priv->delayValue = 250;
   priv->overlapPixels = 0;
   priv->noOverlapPixels = 1;

   priv->fill = TRUE;
   priv->offset = -1;

   priv->evBox = gtk_event_box_new();
   gtk_widget_show(priv->evBox);
   VIEW_OV_BOX_CLASS(parentClass)->set_over(VIEW_OV_BOX(that), priv->evBox);

   g_signal_connect(priv->evBox, "enter-notify-event",
                    G_CALLBACK(ViewAutoDrawerOnOverEnterLeave), that);
   g_signal_connect(priv->evBox, "leave-notify-event",
                    G_CALLBACK(ViewAutoDrawerOnOverEnterLeave), that);
   g_signal_connect(priv->evBox, "grab-notify",
                    G_CALLBACK(ViewAutoDrawerOnGrabNotify), that);

   g_signal_connect(that, "hierarchy-changed",
                    G_CALLBACK(ViewAutoDrawerOnHierarchyChanged), NULL);

   /* This change happens programmatically. Always react to it immediately. */
   ViewAutoDrawerUpdate(that, TRUE);

   ViewAutoDrawerRefreshPacking(that);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerFinalize --
 *
 *      "finalize" method of a ViewAutoDrawer.
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
ViewAutoDrawerFinalize(GObject *object) // IN
{
   ViewAutoDrawer *that;

   that = VIEW_AUTODRAWER(object);
   if (that->priv->delayConnection) {
      g_source_remove(that->priv->delayConnection);
   }

   G_OBJECT_CLASS(parentClass)->finalize(object);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawerClassInit --
 *
 *      Initialize the ViewAutoDrawerClass.
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
ViewAutoDrawerClassInit(gpointer klass) // IN
{
   GObjectClass *objectClass = G_OBJECT_CLASS(klass);
   ViewOvBoxClass *ovBoxClass = VIEW_OV_BOX_CLASS(klass);

   parentClass = g_type_class_peek_parent(klass);

   objectClass->finalize = ViewAutoDrawerFinalize;

   ovBoxClass->set_over = ViewAutoDrawerSetOver;

   g_type_class_add_private(objectClass, sizeof(ViewAutoDrawerPrivate));
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_GetType --
 *
 *      Get the (memoized) GType of the ViewAutoDrawer GTK+ object.
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
ViewAutoDrawer_GetType(void)
{
   static GType type = 0;

   if (type == 0) {
      static const GTypeInfo info = {
         sizeof (ViewAutoDrawerClass),
         NULL, /* BaseInit */
         NULL, /* BaseFinalize */
         (GClassInitFunc)ViewAutoDrawerClassInit,
         NULL,
         NULL, /* Class Data */
         sizeof (ViewAutoDrawer),
         0, /* n_preallocs */
         (GInstanceInitFunc)ViewAutoDrawerInit,
	 NULL,
      };

      type = g_type_register_static(VIEW_TYPE_DRAWER, "ViewAutoDrawer", &info, 0);
   }

   return type;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_New --
 *
 *      Create a new ViewAutoDrawer GTK+ widget.
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
ViewAutoDrawer_New(void)
{
   return GTK_WIDGET(g_object_new(VIEW_TYPE_AUTODRAWER, NULL));
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_SetSlideDelay --
 *
 *      Set the response time of an AutoDrawer in ms., i.e. the time that
 *      elapses between:
 *      - when the AutoDrawer notices a change that can impact the outcome of
 *        the decision to open or close the drawer,
 *      and
 *      - when the AutoDrawer makes such decision.
 *
 *      Users move the mouse inaccurately. If they temporarily move the mouse in
 *      or out of the AutoDrawer for less than the reponse time, their move will
 *      be ignored.
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
ViewAutoDrawer_SetSlideDelay(ViewAutoDrawer *that, // IN
                             guint delay)          // IN
{
   g_return_if_fail(VIEW_IS_AUTODRAWER(that));

   that->priv->delayValue = delay;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_SetOverlapPixels --
 *
 *      Set the number of pixels that the over widget overlaps the under widget
 *      when not open.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated.
 *
 *-----------------------------------------------------------------------------
 */

void
ViewAutoDrawer_SetOverlapPixels(ViewAutoDrawer *that, // IN
                                guint overlapPixels)  // IN
{
   g_return_if_fail(VIEW_IS_AUTODRAWER(that));

   that->priv->overlapPixels = overlapPixels;

   /* This change happens programmatically. Always react to it immediately. */
   ViewAutoDrawerUpdate(that, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_SetNoOverlapPixels --
 *
 *      Set the number of pixels that the drawer reserves when not open. The
 *      over widget does not overlap the under widget over these pixels.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated.
 *
 *-----------------------------------------------------------------------------
 */

void
ViewAutoDrawer_SetNoOverlapPixels(ViewAutoDrawer *that,  // IN
                                  guint noOverlapPixels) // IN
{
   g_return_if_fail(VIEW_IS_AUTODRAWER(that));

   that->priv->noOverlapPixels = noOverlapPixels;

   /* This change happens programmatically. Always react to it immediately. */
   ViewAutoDrawerUpdate(that, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_SetActive --
 *
 *      Set whether the AutoDrawer is active or not. That is to say, whether
 *      it is acting as a drawer or not. When inactive, the over and under
 *      widget do not overlap and the net result is very much like a vbox.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated.
 *
 *-----------------------------------------------------------------------------
 */

void
ViewAutoDrawer_SetActive(ViewAutoDrawer *that, // IN
                         gboolean active)      // IN
{
   g_return_if_fail(VIEW_IS_AUTODRAWER(that));

   that->priv->active = active;

   /* This change happens programmatically. Always react to it immediately. */
   ViewAutoDrawerUpdate(that, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_SetPinned --
 *
 *      Set whether the AutoDrawer is pinned or not. When pinned, the
 *      AutoDrawer will stay open regardless of the state of any other inputs.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated.
 *
 *-----------------------------------------------------------------------------
 */

void
ViewAutoDrawer_SetPinned(ViewAutoDrawer *that, // IN
                         gboolean pinned)      // IN
{
   g_return_if_fail(VIEW_IS_AUTODRAWER(that));

   that->priv->pinned = pinned;

   /*
    * This change happens in response to user input. By default, give the user
    * some time to correct his input before reacting to the change.
    */
   ViewAutoDrawerUpdate(that, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_SetFill --
 *
 *      Set whether the Over widget of the AutoDrawer should fill the full
 *      width of the AutoDrawer or just occupy the minimum space it needs.
 *      A value of TRUE overrides offset settings.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated.
 *
 *-----------------------------------------------------------------------------
 */

void
ViewAutoDrawer_SetFill(ViewAutoDrawer *that, // IN
                       gboolean fill)        // IN
{
   g_return_if_fail(VIEW_IS_AUTODRAWER(that));

   that->priv->fill = fill;
   ViewAutoDrawerRefreshPacking(that);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_SetOffset --
 *
 *      Set the drawer's X offset, or distance in pixels from the left side.
 *      If offset is -1, the drawer will be centered.  If fill has been set
 *      TRUE by SetFill, these settings will have no effect.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated.
 *
 *-----------------------------------------------------------------------------
 */

void
ViewAutoDrawer_SetOffset(ViewAutoDrawer *that, // IN
                         gint offset)          // IN
{
   g_return_if_fail(VIEW_IS_AUTODRAWER(that));

   that->priv->offset = offset;
   ViewAutoDrawerRefreshPacking(that);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewAutoDrawer_Close --
 *
 *      Closes the drawer. This will not unset the pinned state.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Drawer state is updated. If there is a focused widget inside the
 *      drawer, unfocus it.
 *
 *-----------------------------------------------------------------------------
 */

void
ViewAutoDrawer_Close(ViewAutoDrawer *that)   // IN
{
   GtkWindow *window;
   GtkWidget *focus;
   GtkWidget *toplevel;

   g_return_if_fail(VIEW_IS_AUTODRAWER(that));
   toplevel = gtk_widget_get_toplevel(GTK_WIDGET(that));

   if (!toplevel || !gtk_widget_is_toplevel(toplevel)) {
      // The autoDrawer cannot function properly without a toplevel.
      return;
   }
   window = GTK_WINDOW(toplevel);

   focus = gtk_window_get_focus(window);
   if (focus && gtk_widget_is_ancestor(focus, that->priv->evBox)) {
      gtk_window_set_focus(window, NULL);
   }

   that->priv->forceClosing = TRUE;
   that->priv->closeConnection =
      g_timeout_add(ViewDrawer_GetCloseTime(&that->parent) +
                    that->priv->delayValue,
      (GSourceFunc)ViewAutoDrawerOnCloseDelay, that);

   /* This change happens programmatically. Always react to it immediately. */
   ViewAutoDrawerUpdate(that, TRUE);
}
