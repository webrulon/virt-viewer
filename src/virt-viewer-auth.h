/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2012 Red Hat, Inc.
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

#ifndef VIRT_VIEWER_AUTH_H
#define VIRT_VIEWER_AUTH_H

#include "config.h"

#ifdef HAVE_LIBVIRT
#include <libvirt/libvirt.h>
#endif

#include "virt-viewer-util.h"

void virt_viewer_auth_vnc_credentials(GtkWindow *window,
                                      GtkWidget *vnc,
                                      GValueArray *credList,
                                      char *vncAddress);

int virt_viewer_auth_collect_credentials(GtkWindow *window,
                                         const char *type,
                                         const char *address,
                                         char **username,
                                         char **password);

#endif
/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
