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
#include "display.h"

G_DEFINE_ABSTRACT_TYPE(VirtViewerDisplay, virt_viewer_display, G_TYPE_OBJECT)


static void virt_viewer_display_class_init(VirtViewerDisplayClass *klass G_GNUC_UNUSED)
{
}

static void virt_viewer_display_init(VirtViewerDisplay *self G_GNUC_UNUSED)
{
}

void virt_viewer_display_close(VirtViewerDisplay *self)
{
	g_return_if_fail(VIRT_VIEWER_IS_DISPLAY(self));

	VIRT_VIEWER_DISPLAY_GET_CLASS(self)->close(self);
}

void virt_viewer_display_send_keys(VirtViewerDisplay *self,
				   const guint *keyvals, int nkeyvals)
{
	g_return_if_fail(VIRT_VIEWER_IS_DISPLAY(self));

	VIRT_VIEWER_DISPLAY_GET_CLASS(self)->send_keys(self, keyvals, nkeyvals);
}

GdkPixbuf* virt_viewer_display_get_pixbuf(VirtViewerDisplay *self)
{
	g_return_val_if_fail(VIRT_VIEWER_IS_DISPLAY(self), NULL);

	return VIRT_VIEWER_DISPLAY_GET_CLASS(self)->get_pixbuf(self);
}

gboolean virt_viewer_display_open_fd(VirtViewerDisplay *self, int fd)
{
	g_return_val_if_fail(VIRT_VIEWER_IS_DISPLAY(self), FALSE);

	return VIRT_VIEWER_DISPLAY_GET_CLASS(self)->open_fd(self, fd);
}

gboolean virt_viewer_display_open_host(VirtViewerDisplay *self, char *host, char *port)
{
        VirtViewerDisplayClass *klass;

	g_return_val_if_fail(VIRT_VIEWER_IS_DISPLAY(self), FALSE);

	klass = VIRT_VIEWER_DISPLAY_GET_CLASS(self);
        return klass->open_host(self, host, port);
}

gboolean virt_viewer_display_channel_open_fd(VirtViewerDisplay *self,
					     VirtViewerDisplayChannel *channel, int fd)
{
	g_return_val_if_fail(VIRT_VIEWER_IS_DISPLAY(self), FALSE);

	return VIRT_VIEWER_DISPLAY_GET_CLASS(self)->channel_open_fd(self, channel, fd);
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
