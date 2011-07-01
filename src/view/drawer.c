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
 * drawer.c -
 *
 *      Implementation of a GTK+ drawer, i.e. a widget that opens and closes by
 *      sliding smoothly, at constant speed, over another one.
 */


#include "drawer.h"


struct _ViewDrawerPrivate
{
   unsigned int period;
   double step;
   double goal;
   struct {
      gboolean pending;
      guint id;
   } timer;
};

#define VIEW_DRAWER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), VIEW_TYPE_DRAWER, ViewDrawerPrivate))

/* The unaltered parent class. */
static ViewOvBoxClass *parentClass;


/*
 *-----------------------------------------------------------------------------
 *
 * ViewDrawerInit --
 *
 *      Initialize a ViewDrawer.
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
ViewDrawerInit(GTypeInstance *instance, // IN
               gpointer klass)          // Unused
{
   ViewDrawer *that;

   that = VIEW_DRAWER(instance);
   that->priv = VIEW_DRAWER_GET_PRIVATE(that);

   that->priv->period = 10;
   that->priv->step = 0.2;
   that->priv->timer.pending = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewDrawerFinalize --
 *
 *      "finalize" method of a ViewOvBox.
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
ViewDrawerFinalize(GObject *object) // IN
{
   ViewDrawer *that;
   ViewDrawerPrivate *priv;

   that = VIEW_DRAWER(object);
   priv = that->priv;

   if (priv->timer.pending) {
      g_source_remove(priv->timer.id);
      priv->timer.pending = FALSE;
   }

   G_OBJECT_CLASS(parentClass)->finalize(object);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewDrawerClassInit --
 *
 *      Initialize the ViewDrawerClass.
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
ViewDrawerClassInit(gpointer klass) // IN
{
   GObjectClass *objectClass = G_OBJECT_CLASS(klass);

   parentClass = g_type_class_peek_parent(klass);

   objectClass->finalize = ViewDrawerFinalize;

   g_type_class_add_private(objectClass, sizeof(ViewDrawerPrivate));
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewDrawer_GetType --
 *
 *      Get the (memoized) GType of the ViewDrawer GTK+ object.
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
ViewDrawer_GetType(void)
{
   static GType type = 0;

   if (type == 0) {
      static const GTypeInfo info = {
         sizeof (ViewDrawerClass),
         NULL, /* BaseInit */
         NULL, /* BaseFinalize */
         (GClassInitFunc)ViewDrawerClassInit,
         NULL,
         NULL, /* Class Data */
         sizeof (ViewDrawer),
         0, /* n_preallocs */
         (GInstanceInitFunc)ViewDrawerInit,
      };

      type = g_type_register_static(VIEW_TYPE_OV_BOX, "ViewDrawer", &info, 0);
   }

   return type;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewDrawer_New --
 *
 *      Create a new ViewDrawer GTK+ widget.
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
ViewDrawer_New(void)
{
   ViewDrawer *that;

   that = VIEW_DRAWER(g_object_new(VIEW_TYPE_DRAWER, NULL));

   return GTK_WIDGET(that);
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewDrawerOnTimer --
 *
 *      Timer callback of a ViewDrawer. If we have reached the goal, deschedule
 *      the timer. Otherwise make progress towards the goal, and keep the timer
 *      scheduled.
 *
 * Results:
 *      TRUE if the timer must be rescheduled.
 *      FALSE if the timer must not be rescheduled.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static gint
ViewDrawerOnTimer(gpointer data) // IN
{
   ViewDrawer *that;
   ViewDrawerPrivate *priv;
   double fraction;

   that = VIEW_DRAWER(data);
   priv = that->priv;

   fraction = ViewOvBox_GetFraction(VIEW_OV_BOX(that));
   /*
    * Comparing double values with '==' is most of the time a bad idea, due to
    * the inexact representation of values in binary (see
    * http://www2.hursley.ibm.com/decimal/decifaq1.html and http://boost.org/libs/test/doc/components/test_tools/floating_point_comparison.html).
    * But in this particular case it is legitimate. --hpreg
    */
   if (priv->goal == fraction) {
      return priv->timer.pending = FALSE;
   }

   ViewOvBox_SetFraction(VIEW_OV_BOX(that),
                         priv->goal > fraction
                            ? MIN(fraction + priv->step, priv->goal)
                            : MAX(fraction - priv->step, priv->goal));
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewDrawer_SetSpeed --
 *
 *      Set the 'period' (in ms.) and 'step' properties of a ViewDrawer, which
 *      determine the speed and smoothness of the drawer's motion.
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
ViewDrawer_SetSpeed(ViewDrawer *that,    // IN
                    unsigned int period, // IN
                    double step)         // IN
{
   ViewDrawerPrivate *priv;

   g_return_if_fail(that != NULL);

   priv = that->priv;

   priv->period = period;
   if (priv->timer.pending) {
      g_source_remove(priv->timer.id);
      priv->timer.id = g_timeout_add(priv->period, ViewDrawerOnTimer, that);
   }
   priv->step = step;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewDrawer_SetGoal --
 *
 *      Set the 'goal' property of a ViewDrawer, i.e. how much the drawer should
 *      be opened when it is done sliding.
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
ViewDrawer_SetGoal(ViewDrawer *that, // IN
                   double goal)      // IN
{
   ViewDrawerPrivate *priv;

   g_return_if_fail(that != NULL);
   g_return_if_fail(goal >= 0 && goal <= 1);

   priv = that->priv;

   priv->goal = goal;
   if (priv->timer.pending == FALSE) {
      priv->timer.id = g_timeout_add(priv->period, ViewDrawerOnTimer, that);
      priv->timer.pending = TRUE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * ViewDrawer_GetCloseTime --
 *
 *    Get the approximate amount of time it will take for this drawer to
 *    open and close, in ms.
 *
 * Results:
 *      The time it takes to open or close the drawer.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
ViewDrawer_GetCloseTime(ViewDrawer *that)
{
   ViewDrawerPrivate *priv;

   if (that == NULL) {
      return 0;
   }

   priv = that->priv;

   return priv->period * ((int)(1/priv->step) + 1);
}
