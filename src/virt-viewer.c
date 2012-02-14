/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2012 Red Hat, Inc.
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>

#if defined(HAVE_SOCKETPAIR)
#include <sys/socket.h>
#endif

#include "virt-viewer.h"
#include "virt-viewer-app.h"
#include "virt-viewer-events.h"
#include "virt-viewer-auth.h"

struct _VirtViewerPrivate {
    char *uri;
    virConnectPtr conn;
    virDomainPtr dom;
    char *domkey;
    gboolean withEvents;
    gboolean waitvm;
    gboolean reconnect;
};

G_DEFINE_TYPE (VirtViewer, virt_viewer, VIRT_VIEWER_TYPE_APP)
#define GET_PRIVATE(o)                                                        \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), VIRT_VIEWER_TYPE, VirtViewerPrivate))

static int virt_viewer_initial_connect(VirtViewerApp *self);
static gboolean virt_viewer_open_connection(VirtViewerApp *self, int *fd);
static void virt_viewer_deactivated(VirtViewerApp *self);
static gboolean virt_viewer_start(VirtViewerApp *self);

static void
virt_viewer_get_property (GObject *object, guint property_id,
                          GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
virt_viewer_set_property (GObject *object, guint property_id,
                          const GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
virt_viewer_dispose (GObject *object)
{
    VirtViewer *self = VIRT_VIEWER(object);
    VirtViewerPrivate *priv = self->priv;
    if (priv->dom)
        virDomainFree(priv->dom);
    if (priv->conn)
        virConnectClose(priv->conn);
    G_OBJECT_CLASS(virt_viewer_parent_class)->dispose (object);
}

static void
virt_viewer_class_init (VirtViewerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    VirtViewerAppClass *app_class = VIRT_VIEWER_APP_CLASS (klass);

    g_type_class_add_private (klass, sizeof (VirtViewerPrivate));

    object_class->get_property = virt_viewer_get_property;
    object_class->set_property = virt_viewer_set_property;
    object_class->dispose = virt_viewer_dispose;

    app_class->initial_connect = virt_viewer_initial_connect;
    app_class->deactivated = virt_viewer_deactivated;
    app_class->open_connection = virt_viewer_open_connection;
    app_class->start = virt_viewer_start;
}

static void
virt_viewer_init(VirtViewer *self)
{
    self->priv = GET_PRIVATE(self);
}

static void
virt_viewer_deactivated(VirtViewerApp *app)
{
    VirtViewer *self = VIRT_VIEWER(app);
    VirtViewerPrivate *priv = self->priv;

    if (priv->dom) {
        virDomainFree(priv->dom);
        priv->dom = NULL;
    }

    if (priv->reconnect) {
        if (!priv->withEvents) {
            DEBUG_LOG("No domain events, falling back to polling");
            virt_viewer_app_start_reconnect_poll(app);
        }

        virt_viewer_app_show_status(app, _("Waiting for guest domain to re-start"));
        virt_viewer_app_trace(app, "Guest %s display has disconnected, waiting to reconnect", priv->domkey);
    } else {
        VIRT_VIEWER_APP_CLASS(virt_viewer_parent_class)->deactivated(app);
    }
}

static int
virt_viewer_parse_uuid(const char *name,
                       unsigned char *uuid)
{
    int i;

    const char *cur = name;
    for (i = 0;i < 16;) {
        uuid[i] = 0;
        if (*cur == 0)
            return -1;
        if ((*cur == '-') || (*cur == ' ')) {
            cur++;
            continue;
        }
        if ((*cur >= '0') && (*cur <= '9'))
            uuid[i] = *cur - '0';
        else if ((*cur >= 'a') && (*cur <= 'f'))
            uuid[i] = *cur - 'a' + 10;
        else if ((*cur >= 'A') && (*cur <= 'F'))
            uuid[i] = *cur - 'A' + 10;
        else
            return -1;
        uuid[i] *= 16;
        cur++;
        if (*cur == 0)
            return -1;
        if ((*cur >= '0') && (*cur <= '9'))
            uuid[i] += *cur - '0';
        else if ((*cur >= 'a') && (*cur <= 'f'))
            uuid[i] += *cur - 'a' + 10;
        else if ((*cur >= 'A') && (*cur <= 'F'))
            uuid[i] += *cur - 'A' + 10;
        else
            return -1;
        i++;
        cur++;
    }

    return 0;
}


static virDomainPtr
virt_viewer_lookup_domain(VirtViewer *self)
{
    char *end;
    VirtViewerPrivate *priv = self->priv;
    int id = strtol(priv->domkey, &end, 10);
    virDomainPtr dom = NULL;
    unsigned char uuid[16];

    if (id >= 0 && end && !*end) {
        dom = virDomainLookupByID(priv->conn, id);
    }
    if (!dom && virt_viewer_parse_uuid(priv->domkey, uuid) == 0) {
        dom = virDomainLookupByUUID(priv->conn, uuid);
    }
    if (!dom) {
        dom = virDomainLookupByName(priv->conn, priv->domkey);
    }
    return dom;
}

static int
virt_viewer_matches_domain(VirtViewer *self,
                           virDomainPtr dom)
{
    char *end;
    const char *name;
    VirtViewerPrivate *priv = self->priv;
    int id = strtol(priv->domkey, &end, 10);
    unsigned char wantuuid[16];
    unsigned char domuuid[16];

    if (id >= 0 && end && !*end) {
        if (virDomainGetID(dom) == id)
            return 1;
    }
    if (virt_viewer_parse_uuid(priv->domkey, wantuuid) == 0) {
        virDomainGetUUID(dom, domuuid);
        if (memcmp(wantuuid, domuuid, VIR_UUID_BUFLEN) == 0)
            return 1;
    }

    name = virDomainGetName(dom);
    if (strcmp(name, priv->domkey) == 0)
        return 1;

    return 0;
}

static char *
virt_viewer_extract_xpath_string(const gchar *xmldesc,
                                 const gchar *xpath)
{
    xmlDocPtr xml = NULL;
    xmlParserCtxtPtr pctxt = NULL;
    xmlXPathContextPtr ctxt = NULL;
    xmlXPathObjectPtr obj = NULL;
    char *port = NULL;

    pctxt = xmlNewParserCtxt();
    if (!pctxt || !pctxt->sax)
        goto error;

    xml = xmlCtxtReadDoc(pctxt, (const xmlChar *)xmldesc, "domain.xml", NULL,
                         XML_PARSE_NOENT | XML_PARSE_NONET |
                         XML_PARSE_NOWARNING);
    if (!xml)
        goto error;

    ctxt = xmlXPathNewContext(xml);
    if (!ctxt)
        goto error;

    obj = xmlXPathEval((const xmlChar *)xpath, ctxt);
    if (!obj || obj->type != XPATH_STRING || !obj->stringval || !obj->stringval[0])
        goto error;
    if (!strcmp((const char*)obj->stringval, "-1"))
        goto error;

    port = g_strdup((const char*)obj->stringval);
    xmlXPathFreeObject(obj);
    obj = NULL;

 error:
    xmlXPathFreeObject(obj);
    xmlXPathFreeContext(ctxt);
    xmlFreeDoc(xml);
    xmlFreeParserCtxt(pctxt);
    return port;
}

static gboolean
virt_viewer_extract_connect_info(VirtViewer *self,
                                 virDomainPtr dom)
{
    char *type = NULL;
    char *xpath = NULL;
    gboolean retval = FALSE;
    char *xmldesc = virDomainGetXMLDesc(dom, 0);
    VirtViewerPrivate *priv = self->priv;
    VirtViewerApp *app = VIRT_VIEWER_APP(self);
    gchar *gport = NULL;
    gchar *gtlsport = NULL;
    gchar *ghost = NULL;
    gchar *unixsock = NULL;
    gchar *host = NULL;
    gchar *transport = NULL;
    gchar *user = NULL;
    gint port = 0;
    gchar *uri = NULL;

    virt_viewer_app_free_connect_info(app);

    if ((type = virt_viewer_extract_xpath_string(xmldesc, "string(/domain/devices/graphics/@type)")) == NULL) {
        virt_viewer_app_simple_message_dialog(app, _("Cannot determine the graphic type for the guest %s"),
                                              priv->domkey);
        goto cleanup;
    }

    if (virt_viewer_app_create_session(app, type) < 0)
        goto cleanup;

    xpath = g_strdup_printf("string(/domain/devices/graphics[@type='%s']/@port)", type);
    if ((gport = virt_viewer_extract_xpath_string(xmldesc, xpath)) == NULL) {
        free(xpath);
        xpath = g_strdup_printf("string(/domain/devices/graphics[@type='%s']/@socket)", type);
        if ((unixsock = virt_viewer_extract_xpath_string(xmldesc, xpath)) == NULL) {
            virt_viewer_app_simple_message_dialog(app, _("Cannot determine the graphic address for the guest %s"),
                                                  priv->domkey);
            goto cleanup;
        }
    } else {
        if (g_str_equal(type, "spice")) {
            free(xpath);
            xpath = g_strdup_printf("string(/domain/devices/graphics[@type='%s']/@tlsPort)", type);
            gtlsport = virt_viewer_extract_xpath_string(xmldesc, xpath);
        }

        free(xpath);
        xpath = g_strdup_printf("string(/domain/devices/graphics[@type='%s']/@listen)", type);
        ghost = virt_viewer_extract_xpath_string(xmldesc, xpath);
    }

    if (ghost && gport)
        DEBUG_LOG("Guest graphics address is %s:%s", ghost, gport);
    else if (unixsock)
        DEBUG_LOG("Guest graphics address is %s", unixsock);

    uri = virConnectGetURI(priv->conn);
    if (virt_viewer_util_extract_host(uri, NULL, &host, &transport, &user, &port) < 0) {
        virt_viewer_app_simple_message_dialog(app, _("Cannot determine the host for the guest %s"),
                                              priv->domkey);
        goto cleanup;
    }

    /* If the XML listen attribute shows a wildcard address, we need to
     * throw that away since you obviously can't 'connect(2)' to that
     * from a remote host. Instead we fallback to the hostname used in
     * the libvirt URI. This isn't perfect but it is better than nothing
     */
    if (!ghost ||
        (strcmp(ghost, "0.0.0.0") == 0 ||
         strcmp(ghost, "::") == 0)) {
        DEBUG_LOG("Guest graphics listen '%s' is NULL or a wildcard, replacing with '%s'",
                  ghost ? ghost : "", host);
        g_free(ghost);
        ghost = g_strdup(host);
    }

    virt_viewer_app_set_connect_info(app, host, ghost, gport, gtlsport,transport, unixsock, user, port, NULL);

    retval = TRUE;

 cleanup:
    g_free(gport);
    g_free(gtlsport);
    g_free(ghost);
    g_free(unixsock);
    g_free(host);
    g_free(transport);
    g_free(user);
    g_free(type);
    g_free(xpath);
    g_free(xmldesc);
    g_free(uri);
    return retval;
}

static int
virt_viewer_update_display(VirtViewer *self, virDomainPtr dom)
{
    VirtViewerPrivate *priv = self->priv;
    VirtViewerApp *app = VIRT_VIEWER_APP(self);

    if (priv->dom)
        virDomainFree(priv->dom);
    priv->dom = dom;
    virDomainRef(priv->dom);

    virt_viewer_app_trace(app, "Guest %s is running, determining display\n",
                          priv->domkey);

    g_object_set(app, "title", virDomainGetName(dom), NULL);

    if (!virt_viewer_app_has_session(app)) {
        if (!virt_viewer_extract_connect_info(self, dom))
            return -1;
    }

    return 0;
}

static gboolean
virt_viewer_open_connection(VirtViewerApp *self G_GNUC_UNUSED, int *fd)
{
#if defined(HAVE_SOCKETPAIR)
    VirtViewer *viewer = VIRT_VIEWER(self);
    VirtViewerPrivate *priv = viewer->priv;
    int pair[2];
#endif
    *fd = -1;
#if defined(HAVE_SOCKETPAIR)
    if (!priv->dom)
        return TRUE;

    if (socketpair(PF_UNIX, SOCK_STREAM, 0, pair) < 0)
        return FALSE;

    if (virDomainOpenGraphics(priv->dom, 0, pair[0],
                              VIR_DOMAIN_OPEN_GRAPHICS_SKIPAUTH) < 0) {
        virErrorPtr err = virGetLastError();
        DEBUG_LOG("Error %s", err && err->message ? err->message : "Unknown");
        close(pair[0]);
        close(pair[1]);
        return TRUE;
    }
    close(pair[0]);
    *fd = pair[1];
#endif
    return TRUE;
}

static int
virt_viewer_domain_event(virConnectPtr conn G_GNUC_UNUSED,
                         virDomainPtr dom,
                         int event,
                         int detail G_GNUC_UNUSED,
                         void *opaque)
{
    VirtViewer *self = opaque;
    VirtViewerApp *app = VIRT_VIEWER_APP(self);

    DEBUG_LOG("Got domain event %d %d", event, detail);

    if (!virt_viewer_matches_domain(self, dom))
        return 0;

    switch (event) {
    case VIR_DOMAIN_EVENT_STOPPED:
        //virt_viewer_deactivate(self);
        break;

    case VIR_DOMAIN_EVENT_STARTED:
        virt_viewer_update_display(self, dom);
        virt_viewer_app_activate(app);
        break;
    }

    return 0;
}

static int
virt_viewer_initial_connect(VirtViewerApp *app)
{
    virDomainPtr dom = NULL;
    virDomainInfo info;
    int ret = -1;
    VirtViewer *self = VIRT_VIEWER(app);
    VirtViewerPrivate *priv = self->priv;

    virt_viewer_app_show_status(app, _("Finding guest domain"));
    dom = virt_viewer_lookup_domain(self);
    if (!dom) {
        if (priv->waitvm) {
            virt_viewer_app_show_status(app, _("Waiting for guest domain to be created"));
            virt_viewer_app_trace(app, "Guest %s does not yet exist, waiting for it to be created\n",
                                  priv->domkey);
            goto done;
        } else {
            virt_viewer_app_simple_message_dialog(app, _("Cannot find guest domain %s"),
                                                  priv->domkey);
            DEBUG_LOG("Cannot find guest %s", priv->domkey);
            goto cleanup;
        }
    }

    virt_viewer_app_show_status(app, _("Checking guest domain status"));
    if (virDomainGetInfo(dom, &info) < 0) {
        DEBUG_LOG("Cannot get guest state");
        goto cleanup;
    }

    if (info.state == VIR_DOMAIN_SHUTOFF) {
        virt_viewer_app_show_status(app, _("Waiting for guest domain to start"));
    } else {
        ret = virt_viewer_update_display(self, dom);
        if (ret >= 0)
            ret = VIRT_VIEWER_APP_CLASS(virt_viewer_parent_class)->initial_connect(app);
        if (ret < 0) {
            if (priv->waitvm) {
                virt_viewer_app_show_status(app, _("Waiting for guest domain to start server"));
                virt_viewer_app_trace(app, "Guest %s has not activated its display yet, waiting for it to start\n",
                                      priv->domkey);
            } else {
                DEBUG_LOG("Failed to activate viewer");
                goto cleanup;
            }
        } else if (ret == 0) {
            DEBUG_LOG("Failed to activate viewer");
            ret = -1;
            goto cleanup;
        }
    }

 done:
    ret = 0;
 cleanup:
    if (dom)
        virDomainFree(dom);
    return ret;
}

static void
virt_viewer_error_func (void *data G_GNUC_UNUSED,
                        virErrorPtr error G_GNUC_UNUSED)
{
    /* nada */
}



static int
virt_viewer_auth_libvirt_credentials(virConnectCredentialPtr cred,
                                     unsigned int ncred,
                                     void *cbdata)
{
    char **username = NULL, **password = NULL;
    VirtViewer *app = cbdata;
    int i;
    int ret = -1;

    DEBUG_LOG("Got libvirt credential request for %d credential(s)", ncred);

    for (i = 0 ; i < ncred ; i++) {
        switch (cred[i].type) {
        case VIR_CRED_USERNAME:
        case VIR_CRED_AUTHNAME:
            username = &cred[i].result;
            break;
        case VIR_CRED_PASSPHRASE:
            password = &cred[i].result;
            break;
        default:
            DEBUG_LOG("Unsupported libvirt credential %d", cred[i].type);
            return -1;
        }
    }

    if (username || password) {
        VirtViewerWindow *vwin = virt_viewer_app_get_main_window(VIRT_VIEWER_APP(app));
        GtkWindow *win = virt_viewer_window_get_window(vwin);
        ret = virt_viewer_auth_collect_credentials(win,
                                                   "libvirt",
                                                   app->priv->uri,
                                                   username, password);
        if (ret < 0)
            goto cleanup;
    } else {
        ret = 0;
    }

    for (i = 0 ; i < ncred ; i++) {
        switch (cred[i].type) {
        case VIR_CRED_AUTHNAME:
        case VIR_CRED_USERNAME:
        case VIR_CRED_PASSPHRASE:
            if (cred[i].result)
                cred[i].resultlen = strlen(cred[i].result);
            else
                cred[i].resultlen = 0;
            DEBUG_LOG("Got '%s' %d %d", cred[i].result, cred[i].resultlen, cred[i].type);
            break;
        }
    }

 cleanup:
    DEBUG_LOG("Return %d", ret);
    return ret;
}


static gboolean
virt_viewer_start(VirtViewerApp *app)
{
    VirtViewer *self = VIRT_VIEWER(app);
    VirtViewerPrivate *priv = self->priv;
    int cred_types[] =
        { VIR_CRED_AUTHNAME, VIR_CRED_PASSPHRASE };
    virConnectAuth auth_libvirt = {
        .credtype = cred_types,
        .ncredtype = ARRAY_CARDINALITY(cred_types),
        .cb = virt_viewer_auth_libvirt_credentials,
        .cbdata = app,
    };
    int oflags = 0;

    if (!virt_viewer_app_get_attach(app))
        oflags |= VIR_CONNECT_RO;

    virt_viewer_events_register();

    virSetErrorFunc(NULL, virt_viewer_error_func);

    virt_viewer_app_trace(app, "Opening connection to libvirt with URI %s\n",
                          priv->uri ? priv->uri : "<null>");
    priv->conn = virConnectOpenAuth(priv->uri,
                                    //virConnectAuthPtrDefault,
                                    &auth_libvirt,
                                    oflags);
    if (!priv->conn) {
        virt_viewer_app_simple_message_dialog(app, _("Unable to connect to libvirt with URI %s"),
                                              priv->uri ? priv->uri : _("[none]"));
        return FALSE;
    }

    if (virt_viewer_app_initial_connect(app) < 0)
        return FALSE;

    if (virConnectDomainEventRegister(priv->conn,
                                      virt_viewer_domain_event,
                                      self,
                                      NULL) < 0)
        priv->withEvents = FALSE;
    else
        priv->withEvents = TRUE;

    if (!priv->withEvents &&
        !virt_viewer_app_is_active(app)) {
        DEBUG_LOG("No domain events, falling back to polling");
        virt_viewer_app_start_reconnect_poll(app);
    }

    return VIRT_VIEWER_APP_CLASS(virt_viewer_parent_class)->start(app);
}

VirtViewer *
virt_viewer_new(const char *uri,
                const char *name,
                gint zoom,
                gboolean direct,
                gboolean attach,
                gboolean waitvm,
                gboolean reconnect,
                gboolean verbose,
                GtkWidget *container)
{
    VirtViewer *self;
    VirtViewerApp *app;
    VirtViewerPrivate *priv;

    self = g_object_new(VIRT_VIEWER_TYPE,
                        "container", container,
                        "verbose", verbose,
                        "guest-name", name,
                        NULL);
    app = VIRT_VIEWER_APP(self);
    priv = self->priv;

    /* Set initial title based on guest name arg, which can be a ID,
     * UUID, or NAME string. To be replaced with the real guest name later
     */
    g_object_set(app, "title", name, NULL);
    virt_viewer_window_set_zoom_level(virt_viewer_app_get_main_window(app), zoom);
    virt_viewer_app_set_direct(app, direct);
    virt_viewer_app_set_attach(app, attach);

    /* should probably be properties instead */
    priv->uri = g_strdup(uri);
    priv->domkey = g_strdup(name);
    priv->waitvm = waitvm;
    priv->reconnect = reconnect;

    return self;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
