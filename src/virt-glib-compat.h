/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2009 Red Hat, Inc.
 * Copyright (C) 2009-2012 Daniel P. Berrange
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

#include <config.h>

#ifndef _VIRT_GLIB_COMPAT_H
# define _VIRT_GLIB_COMPAT_H 1

#include <glib-object.h>

G_BEGIN_DECLS

#ifndef g_clear_pointer
#define g_clear_pointer(pp, destroy) \
  G_STMT_START {                                                               \
    G_STATIC_ASSERT (sizeof *(pp) == sizeof (gpointer));                       \
    /* Only one access, please */                                              \
    gpointer *_pp = (gpointer *) (pp);                                         \
    gpointer _p;                                                               \
    /* This assignment is needed to avoid a gcc warning */                     \
    GDestroyNotify _destroy = (GDestroyNotify) (destroy);                      \
                                                                               \
    (void) (0 ? (gpointer) *(pp) : 0);                                         \
    do                                                                         \
      _p = g_atomic_pointer_get (_pp);                                         \
    while G_UNLIKELY (!g_atomic_pointer_compare_and_exchange (_pp, _p, NULL)); \
                                                                               \
    if (_p)                                                                    \
      _destroy (_p);                                                           \
  } G_STMT_END
#endif

#if !GLIB_CHECK_VERSION(2,28,0)
#define g_clear_object(object_ptr) \
  G_STMT_START {                                                             \
    /* Only one access, please */                                            \
    gpointer *_p = (gpointer) (object_ptr);                                  \
    gpointer _o;                                                             \
                                                                             \
    do                                                                       \
      _o = g_atomic_pointer_get (_p);                                        \
    while G_UNLIKELY (!g_atomic_pointer_compare_and_exchange (_p, _o, NULL));\
                                                                             \
    if (_o)                                                                  \
      g_object_unref (_o);                                                   \
  } G_STMT_END
#endif

#if !GLIB_CHECK_VERSION(2,32,0)
GByteArray *g_byte_array_new_take (guint8 *data, gsize len);
#endif

G_END_DECLS

#endif // _VIRT_GLIB_COMPAT_H
