/*
  VIRT-VIEWER-PLUGIN

  By Richard W.M. Jones <rjones@redhat.com>
  Copyright (C) 2008-2012 Red Hat, Inc.

  Largely based on DiamondX (http://multimedia.cx/diamondx/), which itself
  is based on Mozilla sources.

  DiamondX copyright notice:

  Example XEmbed-aware Mozilla browser plugin by Adobe.

  Copyright (c) 2007 Adobe Systems Incorporated

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <vncdisplay.h>

#include "virt-viewer-plugin.h"


NPError
VirtViewerXSetWindow (NPP instance, NPWindow *window)
{
  PluginInstance *This;
  NPSetWindowCallbackStruct *ws_info;
  int r;

  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  This = (PluginInstance*) instance->pdata;

  debug ("ViewViewerXSetWindow, This=%p", This);

  if (This == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  ws_info = (NPSetWindowCallbackStruct *)window->ws_info;

  /* Mozilla likes to re-run its greatest hits */
  if (window == This->window &&
      window->x == This->x &&
      window->y == This->y &&
      window->width == This->width &&
      window->height == This->height) {
    debug ("virt-viewer-plugin: window re-run; returning");
    return NPERR_NO_ERROR;
  }

  This->window = window;
  This->x = window->x;
  This->y = window->y;
  This->width = window->width;
  This->height = window->height;

  /* Create a GtkPlug container and a Gtk-VNC widget inside it. */
  This->container = gtk_plug_new ((GdkNativeWindow)(long)window->window);

  /* Make the VNC widget. */
  if (This->uri && This->name) {
    debug ("calling viewer_start uri=%s name=%s direct=%d waitvm=%d reconnect=%d container=%p",
           This->uri, This->name, This->direct, This->waitvm, This->reconnect, This->container);
    r = viewer_start (This->uri, This->name, This->direct, This->waitvm, This->reconnect, 1, This->debug, This->container);
    if (r != 0)
      fprintf (stderr, "viewer_start returned %d != 0\n", r);
  }

  gtk_widget_show_all (This->container);

  return NPERR_NO_ERROR;
}

NPError
VirtViewerDestroyWindow (NPP instance)
{
  PluginInstance *This = (PluginInstance*) instance->pdata;

  debug ("VirtViewerDestroyWindow, This=%p", This);

  if (This && This->container) {
    gtk_widget_destroy (This->container);
    This->container = NULL;
  }

  return NPERR_NO_ERROR;
}

static NPWindow windowlessWindow;

int16
VirtViewerXHandleEvent(NPP instance, void *event)
{
  XGraphicsExposeEvent exposeEvent;
  XEvent *nsEvent;

  debug ("VirtViewerXHandleEvent");

  nsEvent = (XEvent *) event;
  exposeEvent = nsEvent->xgraphicsexpose;

  /*printf(" event: x, y, w, h = %d, %d, %d, %d; display @ %p, window/drawable = %d\n",
    exposeEvent.x,
    exposeEvent.y,
    exposeEvent.width,
    exposeEvent.height,
    exposeEvent.display,
    exposeEvent.drawable);*/

  windowlessWindow.window = exposeEvent.display;
  windowlessWindow.x = exposeEvent.x;
  windowlessWindow.y = exposeEvent.y;
  windowlessWindow.width = exposeEvent.width;
  windowlessWindow.height = exposeEvent.height;
  windowlessWindow.ws_info = (void *)exposeEvent.drawable;

  NPP_SetWindow(instance, &windowlessWindow);

  return 0;
}
