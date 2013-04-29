/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include <config.h>

#include "virt-glib-compat.h"

#if !GLIB_CHECK_VERSION(2,32,0)
GByteArray *g_byte_array_new_take (guint8 *data, gsize len)
{
  GByteArray *array;

  array = g_byte_array_new ();
  g_assert (array->data == NULL);
  g_assert (array->len == 0);

  array->data = data;
  array->len = len;

  return array;
}
#endif
