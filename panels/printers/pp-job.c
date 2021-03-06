/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2015  Red Hat, Inc,
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Felipe Borges <feborges@redhat.com>
 */

#include "pp-job.h"

#include <gio/gio.h>
#include <cups/cups.h>

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetBoolean(attr, element) attr->values[element].boolean
#define ippGetCount(attr)     attr->num_values
#define ippGetInteger(attr, element) attr->values[element].integer
#define ippGetString(attr, element, language) attr->values[element].string.text
#define ippGetValueTag(attr)  attr->value_tag
static int
ippGetRange (ipp_attribute_t *attr,
             int element,
             int *upper)
{
  *upper = attr->values[element].range.upper;
  return (attr->values[element].range.lower);
}
#endif

typedef struct
{
  GObject parent;

  gint    id;
  gchar  *title;
  gint    state;
  gchar **auth_info_required;
} PpJobPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PpJob, pp_job, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_ID,
  PROP_TITLE,
  PROP_STATE,
  PROP_AUTH_INFO_REQUIRED,
  LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY];

static void
pp_job_cancel_purge_async_dbus_cb (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  GVariant *output;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          NULL);
  g_object_unref (source_object);

  if (output != NULL)
    {
      g_variant_unref (output);
    }
}

void
pp_job_cancel_purge_async (PpJob        *job,
                           gboolean      job_purge)
{
  GDBusConnection *bus;
  GError          *error = NULL;
  gint            *job_id;

  g_object_get (job, "id", &job_id, NULL);

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("Failed to get session bus: %s", error->message);
      g_error_free (error);
      return;
    }

  g_dbus_connection_call (bus,
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          "JobCancelPurge",
                          g_variant_new ("(ib)",
                                         job_id,
                                         job_purge),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          pp_job_cancel_purge_async_dbus_cb,
                          NULL);
}

static void
pp_job_set_hold_until_async_dbus_cb (GObject      *source_object,
                                     GAsyncResult *res,
                                     gpointer      user_data)
{
  GVariant *output;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          NULL);
  g_object_unref (source_object);

  if (output)
    {
      g_variant_unref (output);
    }
}

void
pp_job_set_hold_until_async (PpJob        *job,
                             const gchar  *job_hold_until)
{
  GDBusConnection *bus;
  GError          *error = NULL;
  gint            *job_id;

  g_object_get (job, "id", &job_id, NULL);

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("Failed to get session bus: %s", error->message);
      g_error_free (error);
      return;
    }

  g_dbus_connection_call (bus,
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          "JobSetHoldUntil",
                          g_variant_new ("(is)",
                                         job_id,
                                         job_hold_until),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          pp_job_set_hold_until_async_dbus_cb,
                          NULL);
}

static void
pp_job_init (PpJob *obj)
{
}

static void
pp_job_get_property (GObject    *object,
                     guint       property_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  PpJobPrivate *priv;

  priv = pp_job_get_instance_private (PP_JOB (object));

  switch (property_id)
    {
      case PROP_ID:
        g_value_set_int (value, priv->id);
        break;
      case PROP_TITLE:
        g_value_set_string (value, priv->title);
        break;
      case PROP_STATE:
        g_value_set_int (value, priv->state);
        break;
      case PROP_AUTH_INFO_REQUIRED:
        g_value_set_pointer (value, g_strdupv (priv->auth_info_required));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
pp_job_set_property (GObject      *object,
                     guint         property_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
  PpJobPrivate *priv;

  priv = pp_job_get_instance_private (PP_JOB (object));

  switch (property_id)
    {
      case PROP_ID:
        priv->id = g_value_get_int (value);
        break;
      case PROP_TITLE:
        g_free (priv->title);
        priv->title = g_value_dup_string (value);
        break;
      case PROP_STATE:
        priv->state = g_value_get_int (value);
        break;
      case PROP_AUTH_INFO_REQUIRED:
        priv->auth_info_required = g_strdupv (g_value_get_pointer (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
pp_job_finalize (GObject *object)
{
  PpJobPrivate *priv;

  priv = pp_job_get_instance_private (PP_JOB (object));

  g_free (priv->title);
  g_strfreev (priv->auth_info_required);

  G_OBJECT_CLASS (pp_job_parent_class)->finalize (object);
}

static void
pp_job_class_init (PpJobClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = pp_job_get_property;
  object_class->set_property = pp_job_set_property;
  object_class->finalize = pp_job_finalize;

  properties[PROP_ID] = g_param_spec_int ("id",
                                          "Id",
                                          "Job id",
                                          0,
                                          G_MAXINT,
                                          0,
                                          G_PARAM_READWRITE);
  properties[PROP_TITLE] = g_param_spec_string ("title",
                                                "Title",
                                                "Title of this print job",
                                                NULL,
                                                G_PARAM_READWRITE);
  properties[PROP_STATE] = g_param_spec_int ("state",
                                             "State",
                                             "State of this print job (Paused, Completed, Cancelled,...)",
                                             0,
                                             G_MAXINT,
                                             0,
                                             G_PARAM_READWRITE);
  properties[PROP_AUTH_INFO_REQUIRED] = g_param_spec_pointer ("auth-info-required",
                                                              "Authentication info required",
                                                              "Which authentication info is required for this print job",
                                                              G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROPERTY, properties);
}

static void
_pp_job_get_attributes_thread (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  ipp_attribute_t *attr = NULL;
  GVariantBuilder  builder;
  GVariant        *attributes = NULL;
  gchar          **attributes_names = task_data;
  PpJobPrivate    *priv;
  ipp_t           *request;
  ipp_t           *response = NULL;
  gchar           *job_uri;
  gint             i, j, length = 0, n_attrs = 0;

  priv = pp_job_get_instance_private (source_object);

  job_uri = g_strdup_printf ("ipp://localhost/jobs/%d", priv->id);

  if (attributes_names != NULL)
    {
      length = g_strv_length (attributes_names);

      request = ippNewRequest (IPP_GET_JOB_ATTRIBUTES);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "job-uri", NULL, job_uri);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                    "requesting-user-name", NULL, cupsUser ());
      ippAddStrings (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                     "requested-attributes", length, NULL, (const char **) attributes_names);
      response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");
    }

  if (response != NULL)
    {
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

      for (j = 0; j < length; j++)
        {
          attr = ippFindAttribute (response, attributes_names[j], IPP_TAG_ZERO);
          n_attrs = ippGetCount (attr);
          if (attr != NULL && n_attrs > 0 && ippGetValueTag (attr) != IPP_TAG_NOVALUE)
            {
              const GVariantType  *type = NULL;
              GVariant           **values;
              GVariant            *range[2];
              gint                 range_uppervalue;

              values = g_new (GVariant*, n_attrs);

              switch (ippGetValueTag (attr))
                {
                  case IPP_TAG_INTEGER:
                  case IPP_TAG_ENUM:
                    type = G_VARIANT_TYPE_INT32;

                    for (i = 0; i < n_attrs; i++)
                      values[i] = g_variant_new_int32 (ippGetInteger (attr, i));
                    break;

                  case IPP_TAG_NAME:
                  case IPP_TAG_STRING:
                  case IPP_TAG_TEXT:
                  case IPP_TAG_URI:
                  case IPP_TAG_KEYWORD:
                  case IPP_TAG_URISCHEME:
                    type = G_VARIANT_TYPE_STRING;

                    for (i = 0; i < n_attrs; i++)
                      values[i] = g_variant_new_string (ippGetString (attr, i, NULL));
                    break;

                  case IPP_TAG_RANGE:
                    type = G_VARIANT_TYPE_TUPLE;

                    for (i = 0; i < n_attrs; i++)
                      {
                        range[0] = g_variant_new_int32 (ippGetRange (attr, i, &(range_uppervalue)));
                        range[1] = g_variant_new_int32 (range_uppervalue);

                        values[i] = g_variant_new_tuple (range, 2);
                      }
                    break;

                  case IPP_TAG_BOOLEAN:
                    type = G_VARIANT_TYPE_BOOLEAN;

                    for (i = 0; i < n_attrs; i++)
                      values[i] = g_variant_new_boolean (ippGetBoolean (attr, i));
                    break;

                  default:
                    /* do nothing (switch w/ enumeration type) */
                    break;
                }

              if (type != NULL)
                {
                  g_variant_builder_add (&builder, "{sv}",
                                         attributes_names[j],
                                         g_variant_new_array (type, values, n_attrs));
                }

              g_free (values);
            }
        }

      attributes = g_variant_builder_end (&builder);
    }
  g_free (job_uri);

  g_task_return_pointer (task, attributes, (GDestroyNotify) g_variant_unref);
}

void
pp_job_get_attributes_async (PpJob                *job,
                             gchar               **attributes_names,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data)
{
  GTask *task;

  task = g_task_new (job, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdupv (attributes_names), (GDestroyNotify) g_strfreev);
  g_task_run_in_thread (task, _pp_job_get_attributes_thread);

  g_object_unref (task);
}

GVariant *
pp_job_get_attributes_finish (PpJob         *job,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, job), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
_pp_job_authenticate_thread (GTask        *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
  PpJobPrivate  *priv;
  gboolean       result = FALSE;
  gchar        **auth_info = task_data;
  ipp_t         *request;
  ipp_t         *response = NULL;
  gchar         *job_uri;
  gint           length;

  priv = pp_job_get_instance_private (source_object);

  if (auth_info != NULL)
    {
      job_uri = g_strdup_printf ("ipp://localhost/jobs/%d", priv->id);

      length = g_strv_length (auth_info);

      request = ippNewRequest (IPP_OP_CUPS_AUTHENTICATE_JOB);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "job-uri", NULL, job_uri);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                    "requesting-user-name", NULL, cupsUser ());
      ippAddStrings (request, IPP_TAG_OPERATION, IPP_TAG_TEXT,
                     "auth-info", length, NULL, (const char **) auth_info);
      response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");

      result = response != NULL && ippGetStatusCode (response) <= IPP_OK;

      if (response != NULL)
        ippDelete (response);

      g_free (job_uri);
    }

  g_task_return_boolean (task, result);
}

void
pp_job_authenticate_async (PpJob                *job,
                           gchar               **auth_info,
                           GCancellable         *cancellable,
                           GAsyncReadyCallback   callback,
                           gpointer              user_data)
{
  GTask *task;

  task = g_task_new (job, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdupv (auth_info), (GDestroyNotify) g_strfreev);
  g_task_run_in_thread (task, _pp_job_authenticate_thread);

  g_object_unref (task);
}

gboolean
pp_job_authenticate_finish (PpJob         *job,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, job), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
