/*
 * Copyright (C) 2017 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 or version 3 of the License.
 * See http://www.gnu.org/copyleft/lgpl.html the full text of the license.
 */

#include "snapd-get-changes.h"

#include "snapd-error.h"
#include "snapd-json.h"

struct _SnapdGetChanges
{
    SnapdRequest parent_instance;
    gchar *select;
    gchar *snap_name;
    GPtrArray *changes;
};

G_DEFINE_TYPE (SnapdGetChanges, snapd_get_changes, snapd_request_get_type ())

SnapdGetChanges *
_snapd_get_changes_new (const gchar *select, const gchar *snap_name, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
    SnapdGetChanges *request;

    request = SNAPD_GET_CHANGES (g_object_new (snapd_get_changes_get_type (),
                                               "cancellable", cancellable,
                                               "ready-callback", callback,
                                               "ready-callback-data", user_data,
                                               NULL));
    request->select = g_strdup (select);
    request->snap_name = g_strdup (snap_name);

    return request;
}

GPtrArray *
_snapd_get_changes_get_changes (SnapdGetChanges *request)
{
    return request->changes;
}

static SoupMessage *
generate_get_changes_request (SnapdRequest *request)
{
    SnapdGetChanges *r = SNAPD_GET_CHANGES (request);
    g_autoptr(GPtrArray) query_attributes = NULL;
    g_autoptr(GString) path = NULL;

    query_attributes = g_ptr_array_new_with_free_func (g_free);
    if (r->select != NULL) {
        g_autofree gchar *escaped = soup_uri_encode (r->select, NULL);
        g_ptr_array_add (query_attributes, g_strdup_printf ("select=%s", escaped));
    }
    if (r->snap_name != NULL) {
        g_autofree gchar *escaped = soup_uri_encode (r->snap_name, NULL);
        g_ptr_array_add (query_attributes, g_strdup_printf ("for=%s", escaped));
    }

    path = g_string_new ("http://snapd/v2/changes");
    if (query_attributes->len > 0) {
        guint i;

        g_string_append_c (path, '?');
        for (i = 0; i < query_attributes->len; i++) {
            if (i != 0)
                g_string_append_c (path, '&');
            g_string_append (path, (gchar *) query_attributes->pdata[i]);
        }
    }

    return soup_message_new ("GET", path->str);
}

static gboolean
parse_get_changes_response (SnapdRequest *request, SoupMessage *message, GError **error)
{
    SnapdGetChanges *r = SNAPD_GET_CHANGES (request);
    g_autoptr(JsonObject) response = NULL;
    g_autoptr(JsonArray) result = NULL;
    g_autoptr(GPtrArray) changes = NULL;
    guint i;

    response = _snapd_json_parse_response (message, error);
    if (response == NULL)
        return FALSE;
    result = _snapd_json_get_sync_result_a (response, error);
    if (result == NULL)
        return FALSE;

    changes = g_ptr_array_new_with_free_func (g_object_unref);
    for (i = 0; i < json_array_get_length (result); i++) {
        JsonNode *node = json_array_get_element (result, i);
        SnapdChange *change;

        if (json_node_get_value_type (node) != JSON_TYPE_OBJECT) {
            g_set_error (error, SNAPD_ERROR, SNAPD_ERROR_READ_FAILED, "Unexpected snap type");
            return FALSE;
        }

        change = _snapd_json_parse_change (json_node_get_object (node), error);
        if (change == NULL)
            return FALSE;
        g_ptr_array_add (changes, change);
    }

    r->changes = g_steal_pointer (&changes);

    return TRUE;
}

static void
snapd_get_changes_finalize (GObject *object)
{
    SnapdGetChanges *request = SNAPD_GET_CHANGES (object);

    g_clear_pointer (&request->select, g_free);
    g_clear_pointer (&request->snap_name, g_free);

    G_OBJECT_CLASS (snapd_get_changes_parent_class)->finalize (object);
}

static void
snapd_get_changes_class_init (SnapdGetChangesClass *klass)
{
   SnapdRequestClass *request_class = SNAPD_REQUEST_CLASS (klass);
   GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

   request_class->generate_request = generate_get_changes_request;
   request_class->parse_response = parse_get_changes_response;
   gobject_class->finalize = snapd_get_changes_finalize;
}

static void
snapd_get_changes_init (SnapdGetChanges *request)
{
}
