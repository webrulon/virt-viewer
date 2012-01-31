/*
  VIRT_VIEWER-PLUGIN

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

#ifndef VIRT_VIEWER_PLUGIN_H
#define VIRT_VIEWER_PLUGIN_H

#include <npapi.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include "virt-viewer-app.h"

#define PLUGIN_NAME         "Virt-viewer browser plugin"
#define MIME_TYPES_HANDLED  "application/x-virt-viewer:virt-viewer:Virt viewer"
#define PLUGIN_DESCRIPTION  "Virtual machine console viewer plugin"

typedef struct {
  uint16 mode;
  NPWindow *window;
  int32 x, y;
  uint32 width, height;

  NPP instance;
  NPBool pluginsHidden;

  GtkWidget *container;

  char *uri, *name;
  gboolean direct, waitvm, debug, reconnect;
} PluginInstance;

extern NPError VirtViewerXSetWindow (NPP instance, NPWindow* window);
extern NPError VirtViewerDestroyWindow (NPP instance);
extern int16 VirtViewerXHandleEvent (NPP instance, void* event);

#ifdef ENABLE_DEBUG
static void
debug (const char *msg, ...)
{
  va_list args;

  va_start (args, msg);
  vfprintf (stderr, msg, args);
  va_end (args);
  fprintf (stderr, "\n");
  fflush (stderr);
}
#else
#define debug(msg...)
#endif

#endif /* VIRT_VIEWER_PLUGIN_H */
