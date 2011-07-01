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
 * autoDrawer.h --
 *
 *      Declarations for the ViewAutoDrawer GTK+ widget.
 */


#ifndef LIBVIEW_AUTODRAWER_H
#define LIBVIEW_AUTODRAWER_H


#include "drawer.h"


#define VIEW_TYPE_AUTODRAWER            (ViewAutoDrawer_GetType())
#define VIEW_AUTODRAWER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), VIEW_TYPE_AUTODRAWER, ViewAutoDrawer))
#define VIEW_AUTODRAWER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), VIEW_TYPE_AUTODRAWER, ViewAutoDrawerClass))
#define VIEW_IS_AUTODRAWER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIEW_TYPE_AUTODRAWER))
#define VIEW_IS_AUTODRAWER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), VIEW_TYPE_AUTODRAWER))
#define VIEW_AUTODRAWER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), VIEW_TYPE_AUTODRAWER, ViewAutoDrawerClass))

typedef struct _ViewAutoDrawerPrivate ViewAutoDrawerPrivate;

typedef struct _ViewAutoDrawer {
   /* Must come first. */
   ViewDrawer parent;

   /* Private. */
   ViewAutoDrawerPrivate *priv;
} ViewAutoDrawer;

typedef struct _ViewAutoDrawerClass {
   /* Must come first. */
   ViewDrawerClass parent;

   /* Padding for future expansion */
   void (*_view_reserved0)(void);
   void (*_view_reserved1)(void);
   void (*_view_reserved2)(void);
   void (*_view_reserved3)(void);
} ViewAutoDrawerClass;


G_BEGIN_DECLS


GType ViewAutoDrawer_GetType(void);

GtkWidget *ViewAutoDrawer_New(void);

void ViewAutoDrawer_SetSlideDelay(ViewAutoDrawer *that, guint delay);
void ViewAutoDrawer_SetOverlapPixels(ViewAutoDrawer *that, guint overlapPixels);
void ViewAutoDrawer_SetNoOverlapPixels(ViewAutoDrawer *that, guint noOverlapPixels);

void ViewAutoDrawer_SetActive(ViewAutoDrawer *that, gboolean active);

void ViewAutoDrawer_SetPinned(ViewAutoDrawer *that, gboolean pinned);

void ViewAutoDrawer_SetFill(ViewAutoDrawer *that, gboolean fill);

void ViewAutoDrawer_SetOffset(ViewAutoDrawer *that, gint offset);

void ViewAutoDrawer_Close(ViewAutoDrawer *that);

G_END_DECLS


#endif /* LIBVIEW_AUTODRAWER_H */
