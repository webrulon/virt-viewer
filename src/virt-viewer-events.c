/*
 * events.c: event loop integration
 *
 * Copyright (C) 2008-2012 Daniel P. Berrange
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 * Author: Daniel P. Berrange <berrange@redhat.com>
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <libvirt/libvirt.h>

#include "virt-viewer-events.h"

struct virt_viewer_events_handle
{
    int watch;
    int fd;
    int events;
    int enabled;
    GIOChannel *channel;
    guint source;
    virEventHandleCallback cb;
    void *opaque;
    virFreeCallback ff;
};

static int nextwatch = 1;
static unsigned int nhandles = 0;
static struct virt_viewer_events_handle **handles = NULL;

static gboolean
virt_viewer_events_dispatch_handle(GIOChannel *source G_GNUC_UNUSED,
                                   GIOCondition condition,
                                   gpointer opaque)
{
    struct virt_viewer_events_handle *data = opaque;
    int events = 0;

    if (condition & G_IO_IN)
        events |= VIR_EVENT_HANDLE_READABLE;
    if (condition & G_IO_OUT)
        events |= VIR_EVENT_HANDLE_WRITABLE;
    if (condition & G_IO_HUP)
        events |= VIR_EVENT_HANDLE_HANGUP;
    if (condition & G_IO_ERR)
        events |= VIR_EVENT_HANDLE_ERROR;

    DEBUG_LOG("Dispatch handler %d %d %p", data->fd, events, data->opaque);

    (data->cb)(data->watch, data->fd, events, data->opaque);

    return TRUE;
}


static
int virt_viewer_events_add_handle(int fd,
                                  int events,
                                  virEventHandleCallback cb,
                                  void *opaque,
                                  virFreeCallback ff)
{
    struct virt_viewer_events_handle *data;
    GIOCondition cond = 0;

    handles = g_realloc(handles, sizeof(*handles)*(nhandles+1));
    data = g_malloc(sizeof(*data));
    memset(data, 0, sizeof(*data));

    if (events & VIR_EVENT_HANDLE_READABLE)
        cond |= G_IO_IN;
    if (events & VIR_EVENT_HANDLE_WRITABLE)
        cond |= G_IO_OUT;

    data->watch = nextwatch++;
    data->fd = fd;
    data->events = events;
    data->cb = cb;
    data->opaque = opaque;
    data->channel = g_io_channel_unix_new(fd);
    data->ff = ff;

    DEBUG_LOG("Add handle %d %d %p", data->fd, events, data->opaque);

    data->source = g_io_add_watch(data->channel,
                                  cond,
                                  virt_viewer_events_dispatch_handle,
                                  data);

    handles[nhandles++] = data;

    return data->watch;
}

static struct virt_viewer_events_handle *
virt_viewer_events_find_handle(int watch)
{
    unsigned int i;
    for (i = 0 ; i < nhandles ; i++)
        if (handles[i]->watch == watch)
            return handles[i];

    return NULL;
}

static void
virt_viewer_events_update_handle(int watch,
                                 int events)
{
    struct virt_viewer_events_handle *data = virt_viewer_events_find_handle(watch);

    if (!data) {
        DEBUG_LOG("Update for missing handle watch %d", watch);
        return;
    }

    if (events) {
        GIOCondition cond = 0;
        if (events == data->events)
            return;

        if (data->source)
            g_source_remove(data->source);

        cond |= G_IO_HUP;
        if (events & VIR_EVENT_HANDLE_READABLE)
            cond |= G_IO_IN;
        if (events & VIR_EVENT_HANDLE_WRITABLE)
            cond |= G_IO_OUT;
        data->source = g_io_add_watch(data->channel,
                                      cond,
                                      virt_viewer_events_dispatch_handle,
                                      data);
        data->events = events;
    } else {
        if (!data->source)
            return;

        g_source_remove(data->source);
        data->source = 0;
        data->events = 0;
    }
}


static gboolean
virt_viewer_events_cleanup_handle(gpointer user_data)
{
    struct virt_viewer_events_handle *data = user_data;

    DEBUG_LOG("Cleanup of handle %p", data);
    g_return_val_if_fail(data != NULL, FALSE);

    if (data->ff)
        (data->ff)(data->opaque);

    free(data);
    return FALSE;
}


static int
virt_viewer_events_remove_handle(int watch)
{
    struct virt_viewer_events_handle *data = virt_viewer_events_find_handle(watch);

    if (!data) {
        DEBUG_LOG("Remove of missing watch %d", watch);
        return -1;
    }

    DEBUG_LOG("Remove handle %d %d", watch, data->fd);

    if (!data->source)
        return -1;

    g_source_remove(data->source);
    data->source = 0;
    data->events = 0;

    g_idle_add(virt_viewer_events_cleanup_handle, data);
    return 0;
}

struct virt_viewer_events_timeout
{
    int timer;
    int interval;
    guint source;
    virEventTimeoutCallback cb;
    void *opaque;
    virFreeCallback ff;
};


static int nexttimer = 1;
static unsigned int ntimeouts = 0;
static struct virt_viewer_events_timeout **timeouts = NULL;

static gboolean
virt_viewer_events_dispatch_timeout(void *opaque)
{
    struct virt_viewer_events_timeout *data = opaque;
    DEBUG_LOG("Dispatch timeout %p %p %d %p", data, data->cb, data->timer, data->opaque);
    (data->cb)(data->timer, data->opaque);

    return TRUE;
}

static int
virt_viewer_events_add_timeout(int interval,
                               virEventTimeoutCallback cb,
                               void *opaque,
                               virFreeCallback ff)
{
    struct virt_viewer_events_timeout *data;

    timeouts = g_realloc(timeouts, sizeof(*timeouts)*(ntimeouts+1));
    data = g_malloc(sizeof(*data));
    memset(data, 0, sizeof(*data));

    data->timer = nexttimer++;
    data->interval = interval;
    data->cb = cb;
    data->opaque = opaque;
    data->ff = ff;
    if (interval >= 0)
        data->source = g_timeout_add(interval,
                                     virt_viewer_events_dispatch_timeout,
                                     data);

    timeouts[ntimeouts++] = data;

    DEBUG_LOG("Add timeout %p %d %p %p %d", data, interval, cb, opaque, data->timer);

    return data->timer;
}


static struct virt_viewer_events_timeout *
virt_viewer_events_find_timeout(int timer)
{
    unsigned int i;
    for (i = 0 ; i < ntimeouts ; i++)
        if (timeouts[i]->timer == timer)
            return timeouts[i];

    return NULL;
}


static void
virt_viewer_events_update_timeout(int timer,
                                  int interval)
{
    struct virt_viewer_events_timeout *data = virt_viewer_events_find_timeout(timer);

    if (!data) {
        DEBUG_LOG("Update of missing timer %d", timer);
        return;
    }

    DEBUG_LOG("Update timeout %p %d %d", data, timer, interval);

    if (interval >= 0) {
        if (data->source)
            return;

        data->interval = interval;
        data->source = g_timeout_add(data->interval,
                                     virt_viewer_events_dispatch_timeout,
                                     data);
    } else {
        if (!data->source)
            return;

        g_source_remove(data->source);
        data->source = 0;
    }
}


static gboolean
virt_viewer_events_cleanup_timeout(gpointer user_data)
{
    struct virt_viewer_events_timeout *data = user_data;

    DEBUG_LOG("Cleanup of timeout %p", data);
    g_return_val_if_fail(data != NULL, FALSE);

    if (data->ff)
        (data->ff)(data->opaque);

    free(data);
    return FALSE;
}


static int
virt_viewer_events_remove_timeout(int timer)
{
    struct virt_viewer_events_timeout *data = virt_viewer_events_find_timeout(timer);

    if (!data) {
        DEBUG_LOG("Remove of missing timer %d", timer);
        return -1;
    }

    DEBUG_LOG("Remove timeout %p %d", data, timer);

    if (!data->source)
        return -1;

    g_source_remove(data->source);
    data->source = 0;

    g_idle_add(virt_viewer_events_cleanup_timeout, data);
    return 0;
}


void virt_viewer_events_register(void) {
    virEventRegisterImpl(virt_viewer_events_add_handle,
                         virt_viewer_events_update_handle,
                         virt_viewer_events_remove_handle,
                         virt_viewer_events_add_timeout,
                         virt_viewer_events_update_timeout,
                         virt_viewer_events_remove_timeout);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
