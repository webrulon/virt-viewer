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
 * drawer.h --
 *
 *      Declarations for the ViewDrawer GTK+ widget.
 */


#ifndef LIBVIEW_DRAWER_H
#define LIBVIEW_DRAWER_H


#include "ovBox.h"


#define VIEW_TYPE_DRAWER            (ViewDrawer_GetType())
#define VIEW_DRAWER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), VIEW_TYPE_DRAWER, ViewDrawer))
#define VIEW_DRAWER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), VIEW_TYPE_DRAWER, ViewDrawerClass))
#define VIEW_IS_DRAWER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIEW_TYPE_DRAWER))
#define VIEW_IS_DRAWER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), VIEW_TYPE_DRAWER))
#define VIEW_DRAWER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), VIEW_TYPE_DRAWER, ViewDrawerClass))

typedef struct _ViewDrawerPrivate ViewDrawerPrivate;

typedef struct _ViewDrawer {
   /* Must come first. */
   ViewOvBox parent;

   /* Private. */
   ViewDrawerPrivate *priv;
} ViewDrawer;


typedef struct _ViewDrawerClass {
   /* Must come first. */
   ViewOvBoxClass parent;

   /* Padding for future expansion */
   void (*_view_reserved0)(void);
   void (*_view_reserved1)(void);
   void (*_view_reserved2)(void);
   void (*_view_reserved3)(void);
} ViewDrawerClass;


G_BEGIN_DECLS


GType ViewDrawer_GetType(void);

GtkWidget *ViewDrawer_New(void);

void ViewDrawer_SetSpeed(ViewDrawer *that, unsigned int period, double step);
void ViewDrawer_SetGoal(ViewDrawer *that, double fraction);
int ViewDrawer_GetCloseTime(ViewDrawer *that);


G_END_DECLS


#endif /* LIBVIEW_DRAWER_H */
