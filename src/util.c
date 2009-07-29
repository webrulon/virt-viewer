/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2009 Red Hat,
 * Copyright (C) 2009 Daniel P. Berrange
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

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

GladeXML *viewer_load_glade(const char *name, const char *widget)
{
	char *path;
	struct stat sb;
	GladeXML *xml;

	if (stat(name, &sb) >= 0)
		return glade_xml_new(name, widget, NULL);

	path = g_strdup_printf("%s/%s", GLADE_DIR, name);

	xml = glade_xml_new(path, widget, NULL);
	g_free(path);
	return xml;
}
