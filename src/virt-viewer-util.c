/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2012 Red Hat, Inc.
 * Copyright (C) 2009-2012 Daniel P. Berrange
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
#include <string.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>

#include "virt-viewer-util.h"

GtkBuilder *virt_viewer_util_load_ui(const char *name)
{
    struct stat sb;
    GtkBuilder *builder;
    GError *error = NULL;

    builder = gtk_builder_new();
    if (stat(name, &sb) >= 0) {
        gtk_builder_add_from_file(builder, name, &error);
    } else {
        const gchar * const * dirs = g_get_system_data_dirs();
        g_return_val_if_fail(dirs != NULL, NULL);

        while (dirs[0] != NULL) {
            gchar *path = g_build_filename(dirs[0], PACKAGE, "ui", name, NULL);
            if (gtk_builder_add_from_file(builder, path, NULL) != 0) {
                g_free(path);
                break;
            }
            g_free(path);
            dirs++;
        }
        if (dirs[0] == NULL)
            goto failed;
    }

    if (error) {
        g_error("Cannot load UI description %s: %s", name,
                error->message);
        g_clear_error(&error);
        goto failed;
    }

    return builder;
 failed:
    g_error("failed to find UI description file");
    g_object_unref(builder);
    return NULL;
}

int
virt_viewer_util_extract_host(const char *uristr,
                              char **scheme,
                              char **host,
                              char **transport,
                              char **user,
                              int *port)
{
    xmlURIPtr uri;
    char *offset = NULL;

    if (uristr == NULL ||
        !g_ascii_strcasecmp(uristr, "xen"))
        uristr = "xen:///";

    uri = xmlParseURI(uristr);
    g_return_val_if_fail(uri != NULL, 1);

    if (host) {
        if (!uri || !uri->server) {
            *host = g_strdup("localhost");
        } else {
            if (uri->server[0] == '[') {
                gchar *tmp;
                *host = g_strdup(uri->server + 1);
                if ((tmp = strchr(*host, ']')))
                    *tmp = '\0';
            } else {
                *host = g_strdup(uri->server);
            }
        }
    }

    if (user) {
        if (uri->user)
            *user = g_strdup(uri->user);
        else
            *user = NULL;
    }

    if (port)
        *port = uri->port;

    if (uri->scheme)
        offset = strchr(uri->scheme, '+');

    if (transport) {
        if (offset)
            *transport = g_strdup(offset + 1);
        else
            *transport = NULL;
    }

    if (scheme && uri->scheme) {
        if (offset)
            *scheme = g_strndup(uri->scheme, offset - uri->scheme);
        else
            *scheme = g_strdup(uri->scheme);
    }

    xmlFreeURI(uri);
    return 0;
}

typedef struct {
    GObject *instance;
    GObject *observer;
    GClosure *closure;
    gulong handler_id;
} WeakHandlerCtx;

static WeakHandlerCtx *
whc_new(GObject *instance,
        GObject *observer)
{
    WeakHandlerCtx *ctx = g_slice_new0(WeakHandlerCtx);

    ctx->instance = instance;
    ctx->observer = observer;

    return ctx;
}

static void
whc_free(WeakHandlerCtx *ctx)
{
    g_slice_free(WeakHandlerCtx, ctx);
}

static void observer_destroyed_cb(gpointer, GObject *);
static void closure_invalidated_cb(gpointer, GClosure *);

/*
 * If signal handlers are removed before the object is destroyed, this
 * callback will never get triggered.
 */
static void
instance_destroyed_cb(gpointer ctx_,
                      GObject *where_the_instance_was G_GNUC_UNUSED)
{
    WeakHandlerCtx *ctx = ctx_;

    /* No need to disconnect the signal here, the instance has gone away. */
    g_object_weak_unref(ctx->observer, observer_destroyed_cb, ctx);
    g_closure_remove_invalidate_notifier(ctx->closure, ctx,
                                         closure_invalidated_cb);
    whc_free(ctx);
}

/* Triggered when the observer is destroyed. */
static void
observer_destroyed_cb(gpointer ctx_,
                      GObject *where_the_observer_was G_GNUC_UNUSED)
{
    WeakHandlerCtx *ctx = ctx_;

    g_closure_remove_invalidate_notifier(ctx->closure, ctx,
                                         closure_invalidated_cb);
    g_signal_handler_disconnect(ctx->instance, ctx->handler_id);
    g_object_weak_unref(ctx->instance, instance_destroyed_cb, ctx);
    whc_free(ctx);
}

/* Triggered when either object is destroyed or the handler is disconnected. */
static void
closure_invalidated_cb(gpointer ctx_,
                       GClosure *where_the_closure_was G_GNUC_UNUSED)
{
    WeakHandlerCtx *ctx = ctx_;

    g_object_weak_unref(ctx->instance, instance_destroyed_cb, ctx);
    g_object_weak_unref(ctx->observer, observer_destroyed_cb, ctx);
    whc_free(ctx);
}

/* Copied from tp_g_signal_connect_object. */
/**
  * virt_viewer_signal_connect_object: (skip)
  * @instance: the instance to connect to.
  * @detailed_signal: a string of the form "signal-name::detail".
  * @c_handler: the #GCallback to connect.
  * @gobject: the object to pass as data to @c_handler.
  * @connect_flags: a combination of #GConnectFlags.
  *
  * Similar to g_signal_connect_object() but will delete connection
  * when any of the objects is destroyed.
  *
  * Returns: the handler id.
  */
gulong virt_viewer_signal_connect_object(gpointer instance,
                                         const gchar *detailed_signal,
                                         GCallback c_handler,
                                         gpointer gobject,
                                         GConnectFlags connect_flags)
{
    GObject *instance_obj = G_OBJECT(instance);
    WeakHandlerCtx *ctx = whc_new(instance_obj, gobject);

    g_return_val_if_fail(G_TYPE_CHECK_INSTANCE (instance), 0);
    g_return_val_if_fail(detailed_signal != NULL, 0);
    g_return_val_if_fail(c_handler != NULL, 0);
    g_return_val_if_fail(G_IS_OBJECT (gobject), 0);
    g_return_val_if_fail((connect_flags & ~(G_CONNECT_AFTER|G_CONNECT_SWAPPED)) == 0, 0);

    if (connect_flags & G_CONNECT_SWAPPED)
        ctx->closure = g_cclosure_new_object_swap(c_handler, gobject);
    else
        ctx->closure = g_cclosure_new_object(c_handler, gobject);

    ctx->handler_id = g_signal_connect_closure(instance, detailed_signal,
                                               ctx->closure, (connect_flags & G_CONNECT_AFTER) ? TRUE : FALSE);

    g_object_weak_ref(instance_obj, instance_destroyed_cb, ctx);
    g_object_weak_ref(gobject, observer_destroyed_cb, ctx);
    g_closure_add_invalidate_notifier(ctx->closure, ctx,
                                      closure_invalidated_cb);

    return ctx->handler_id;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
