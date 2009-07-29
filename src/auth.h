/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2009 Red Hat,
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

#include <libvirt/libvirt.h>

#include "util.h"

void viewer_auth_vnc_credentials(GtkWidget *vnc, GValueArray *credList, char **message);

int viewer_auth_libvirt_credentials(virConnectCredentialPtr cred,
				    unsigned int ncred,
				    void *cbdata);

#endif
