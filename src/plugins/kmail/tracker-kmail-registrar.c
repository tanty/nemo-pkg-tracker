/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Authors:
 *  Philip Van Hoof <philip@codeminded.be>
 */

#include "config.h"

#include <string.h>

#include <glib-object.h>
#include <dbus/dbus-glib-bindings.h>

#include <libtracker-data/tracker-data-manager.h>

#include <tracker-store/tracker-push-registrar.h>
#include <tracker-store/tracker-store.h>

#define __TRACKER_KMAIL_REGISTRAR_C__

#include "tracker-kmail-registrar.h"
#include "tracker-kmail-registrar-glue.h"

#define TRACKER_KMAIL_REGISTRAR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TRACKER_TYPE_KMAIL_REGISTRAR, TrackerKMailRegistrarPrivate))

#define TRACKER_TYPE_KMAIL_PUSH_REGISTRAR    (tracker_kmail_push_registrar_get_type ())
#define TRACKER_KMAIL_PUSH_REGISTRAR(module) (G_TYPE_CHECK_INSTANCE_CAST ((module), TRACKER_TYPE_KMAIL_PUSH_REGISTRAR, TrackerKMailPushRegistrar))

#define RDF_PREFIX	TRACKER_RDF_PREFIX
#define NMO_PREFIX	TRACKER_NMO_PREFIX
#define NCO_PREFIX	TRACKER_NCO_PREFIX
#define NAO_PREFIX	TRACKER_NAO_PREFIX

#define NIE_DATASOURCE 			       TRACKER_NIE_PREFIX "DataSource"
#define NIE_DATASOURCE_P 		       TRACKER_NIE_PREFIX "dataSource"

#define DATASOURCE_URN			       "urn:nepomuk:datasource:4a157cf0-1241-11de-8c30-0800200c9a66"

typedef struct TrackerKMailPushRegistrar TrackerKMailPushRegistrar;
typedef struct TrackerKMailPushRegistrarClass TrackerKMailPushRegistrarClass;

struct TrackerKMailPushRegistrar {
	TrackerPushRegistrar parent_instance;
};

struct TrackerKMailPushRegistrarClass {
	TrackerPushRegistrarClass parent_class;
};

typedef struct {
	gpointer dummy;
} TrackerKMailRegistrarPrivate;

enum {
	PROP_0,
};


typedef struct {
	guint modseq;
	gchar *subject;
	GStrv predicates;
	GStrv values;
} QueuedSet;

static GQueue *many_queue = NULL;
#define QUEUED_SETS_PER_MAINLOOP 2

static GType tracker_kmail_push_registrar_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (TrackerKMailRegistrar, tracker_kmail_registrar, G_TYPE_OBJECT)
G_DEFINE_TYPE (TrackerKMailPushRegistrar, tracker_kmail_push_registrar, TRACKER_TYPE_PUSH_REGISTRAR);

static void
tracker_kmail_registrar_finalize (GObject *object)
{
	G_OBJECT_CLASS (tracker_kmail_registrar_parent_class)->finalize (object);
}

static void
tracker_kmail_registrar_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_kmail_registrar_get_property (GObject    *object,
					  guint       prop_id,
					  GValue     *value,
					  GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tracker_kmail_registrar_class_init (TrackerKMailRegistrarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tracker_kmail_registrar_finalize;
	object_class->set_property = tracker_kmail_registrar_set_property;
	object_class->get_property = tracker_kmail_registrar_get_property;

	g_type_class_add_private (object_class, sizeof (TrackerKMailRegistrarPrivate));
}

static void
tracker_kmail_registrar_init (TrackerKMailRegistrar *object)
{
}


static void
get_email_and_fullname (const gchar *line, gchar **email, gchar **fullname)
{
	gchar *ptr = g_utf8_strchr (line, -1, '<');

	if (ptr) {
		gchar *holder;

		holder = g_strdup (line);
		ptr = g_utf8_strchr (holder, -1, '<');
		*ptr = '\0';
		ptr++;
		*fullname = holder;
		holder = ptr;
		ptr = g_utf8_strchr (ptr, -1, '>');
		if (ptr) {
			*ptr = '\0';
		}
		*email = g_strdup (holder);

	} else {
		*email = g_strdup (line);
		*fullname = NULL;
	}
}

static void
perform_set (TrackerKMailRegistrar *object, 
	     const gchar *subject, 
	     const GStrv predicates, 
	     const GStrv values)
{
	guint i = 0;
	TrackerSparqlBuilder *sparql;

	sparql = tracker_sparql_builder_new_update ();

	tracker_sparql_builder_insert_open (sparql);

	tracker_sparql_builder_subject_iri (sparql, DATASOURCE_URN);
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object_iri (sparql, NIE_DATASOURCE);

	tracker_sparql_builder_subject_iri (sparql, subject);
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object (sparql, "nmo:Email");

	tracker_sparql_builder_subject_iri (sparql, subject);
	tracker_sparql_builder_predicate (sparql, "rdf:type");
	tracker_sparql_builder_object (sparql, "nmo:MailboxDataObject");

	tracker_sparql_builder_subject_iri (sparql, subject);
	tracker_sparql_builder_predicate_iri (sparql, NIE_DATASOURCE_P);
	tracker_sparql_builder_object_iri (sparql, DATASOURCE_URN);

	while (predicates [i] != NULL && values[i] != NULL) {

		/* TODO: TRACKER_KMAIL_PREDICATE_IDMD5
		 *       TRACKER_KMAIL_PREDICATE_UID
		 *       TRACKER_KMAIL_PREDICATE_SERNUM
		 *       TRACKER_KMAIL_PREDICATE_SPAM
		 *       TRACKER_KMAIL_PREDICATE_HAM
		 *
		 * I don't have predicates in Tracker's ontology for these. In
		 * Jürg's vstore branch we are working with Nepomuk as ontology-
		 * set. Perhaps when we merge this to that branch that we can 
		 * improve this situation. */

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_TAG) == 0) {

			tracker_sparql_builder_predicate (sparql, "nao:hasTag");

			tracker_sparql_builder_object_blank_open (sparql);

			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nao:Tag");

			tracker_sparql_builder_predicate (sparql, "nao:prefLabel");
			tracker_sparql_builder_object_string (sparql, values[i]);

		}

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_SUBJECT) == 0) {
			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:messageSubject");
			tracker_sparql_builder_object_string (sparql, values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_SENT) == 0) {
			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:receivedDate");
			tracker_sparql_builder_object_string (sparql, values[i]);
		}

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_FROM) == 0) {
			gchar *email_uri, *email = NULL, *fullname = NULL;

			get_email_and_fullname (values[i], &email, &fullname);

			email_uri = tracker_uri_printf_escaped ("mailto:%s", email); 

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:EmailAddress");

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "nco:emailAddress");
			tracker_sparql_builder_object_string (sparql, email);

			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:from");

			tracker_sparql_builder_object_blank_open (sparql);

			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:Contact");

			if (fullname) {
				tracker_sparql_builder_predicate (sparql, "nco:fullname");
				tracker_sparql_builder_object_string (sparql, fullname);
				g_free (fullname);
			}

			tracker_sparql_builder_predicate (sparql, "nco:hasEmailAddress");
			tracker_sparql_builder_object_iri (sparql, email_uri);

			tracker_sparql_builder_object_blank_close (sparql);

			g_free (email_uri);
			g_free (email);
		}


		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_TO) == 0) {
			gchar *email_uri, *email = NULL, *fullname = NULL;

			get_email_and_fullname (values[i], &email, &fullname);

			email_uri = tracker_uri_printf_escaped ("mailto:%s", email); 

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:EmailAddress");

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "nco:emailAddress");
			tracker_sparql_builder_object_string (sparql, email);

			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:to");

			tracker_sparql_builder_object_blank_open (sparql);

			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:Contact");

			if (fullname) {
				tracker_sparql_builder_predicate (sparql, "nco:fullname");
				tracker_sparql_builder_object_string (sparql, fullname);
				g_free (fullname);
			}

			tracker_sparql_builder_predicate (sparql, "nco:hasEmailAddress");
			tracker_sparql_builder_object_iri (sparql, email_uri);

			tracker_sparql_builder_object_blank_close (sparql);

			g_free (email_uri);
			g_free (email);
		}

		if (g_strcmp0 (predicates[i], TRACKER_KMAIL_PREDICATE_CC) == 0) {
			gchar *email_uri, *email = NULL, *fullname = NULL;

			get_email_and_fullname (values[i], &email, &fullname);

			email_uri = tracker_uri_printf_escaped ("mailto:%s", email); 

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:EmailAddress");

			tracker_sparql_builder_subject_iri (sparql, email_uri);
			tracker_sparql_builder_predicate (sparql, "nco:emailAddress");
			tracker_sparql_builder_object_string (sparql, email);

			tracker_sparql_builder_subject_iri (sparql, subject);
			tracker_sparql_builder_predicate (sparql, "nmo:cc");

			tracker_sparql_builder_object_blank_open (sparql);

			tracker_sparql_builder_predicate (sparql, "rdf:type");
			tracker_sparql_builder_object (sparql, "nco:Contact");

			if (fullname) {
				tracker_sparql_builder_predicate (sparql, "nco:fullname");
				tracker_sparql_builder_object_string (sparql, fullname);
				g_free (fullname);
			}

			tracker_sparql_builder_predicate (sparql, "nco:hasEmailAddress");
			tracker_sparql_builder_object_iri (sparql, email_uri);

			tracker_sparql_builder_object_blank_close (sparql);

			g_free (email_uri);
			g_free (email);
		}


		i++;
	}

	tracker_sparql_builder_insert_close (sparql);

	tracker_store_queue_sparql_update (tracker_sparql_builder_get_result (sparql),
	                                   NULL, NULL, NULL);

	g_object_unref (sparql);
}

static void 
perform_unset (TrackerKMailRegistrar *object, 
	       const gchar *subject, 
	       gboolean batch)
{
	gchar *sparql = g_strdup_printf ("DELETE { <%s> a rdfs:Resource }", subject);

	if (!batch) {
		tracker_store_sparql_update (sparql, NULL);
	} else {
		tracker_store_queue_sparql_update (sparql, NULL, NULL, NULL);
	}

	g_free (sparql);
}

static void
perform_cleanup (TrackerKMailRegistrar *object)
{
	tracker_store_sparql_update ("DELETE { ?s a rdfs:Resource } WHERE { ?s nie:dataSource <" DATASOURCE_URN "> }", NULL);
	/* tracker_store_sparql_update ("DELETE { ?s ?p ?o } WHERE { ?s nie:dataSource <" DATASOURCE_URN "> }", NULL); */
}

static void
set_stored_last_modseq (guint last_modseq)
{
	tracker_data_manager_set_db_option_int ("KMailLastModseq", (gint) last_modseq);
}


void
tracker_kmail_registrar_set (TrackerKMailRegistrar *object, 
				 const gchar *subject, 
				 const GStrv predicates,
				 const GStrv values,
				 const guint modseq,
				 DBusGMethodInvocation *context,
				 GError *derror)
{
	dbus_async_return_if_fail (subject != NULL, context);

	if (predicates && values) {

		dbus_async_return_if_fail (g_strv_length (predicates) == 
					   g_strv_length (values), context);

		perform_set (object, subject, predicates, values);
	}

	set_stored_last_modseq (modseq);

	tracker_store_queue_commit (NULL, NULL, NULL);

	dbus_g_method_return (context);
}

static void
on_commit (gpointer user_data)
{
	set_stored_last_modseq (GPOINTER_TO_UINT (user_data));
}

static void
queued_set_free (QueuedSet *queued_set)
{
	g_free (queued_set->subject);
	g_strfreev (queued_set->values);
	g_strfreev (queued_set->predicates);
	g_slice_free (QueuedSet, queued_set);
}

static gboolean 
many_idle_handler (gpointer user_data)
{
	guint i, last_modseq = 0;
	QueuedSet *queued_set = GUINT_TO_POINTER (1);
	TrackerKMailRegistrar *object = user_data;

	for (i = 0; i < QUEUED_SETS_PER_MAINLOOP && queued_set ; i++) {
		queued_set = g_queue_pop_head (many_queue);

		if (queued_set) {
			perform_set (object,
			             queued_set->subject,
			             queued_set->predicates,
			             queued_set->values);

			if (last_modseq != queued_set->modseq) {
				tracker_store_queue_commit (on_commit, 
				                            GUINT_TO_POINTER (queued_set->modseq), 
				                            NULL);
			}

			last_modseq = queued_set->modseq;

			queued_set_free (queued_set);
		} 
	}

	return (gboolean) queued_set;
}

static void
many_idle_destroy (gpointer user_data)
{
	g_queue_free (many_queue);
	many_queue = NULL;
	g_object_unref (user_data);
}

static void
start_many_handler (TrackerKMailRegistrar *object)
{
	g_idle_add_full (G_PRIORITY_LOW,
	                 many_idle_handler,
	                 g_object_ref (object),
	                 many_idle_destroy);
}

void
tracker_kmail_registrar_set_many (TrackerKMailRegistrar *object, 
				  const GStrv subjects, 
				  const GPtrArray *predicates,
				  const GPtrArray *values,
				  const guint modseq,
				  DBusGMethodInvocation *context,
				  GError *derror)
{
	guint len, i = 0;
	gboolean start_handler = FALSE;

	dbus_async_return_if_fail (subjects != NULL, context);
	dbus_async_return_if_fail (predicates != NULL, context);
	dbus_async_return_if_fail (values != NULL, context);

	len = g_strv_length (subjects);

	dbus_async_return_if_fail (len == predicates->len, context);
	dbus_async_return_if_fail (len == values->len, context);

	if (!many_queue) {
		many_queue = g_queue_new ();
		start_handler = TRUE;
	}

	while (subjects[i] != NULL) {
		QueuedSet *queued_set = g_slice_new (QueuedSet);

		queued_set->subject = g_strdup (subjects[1]);
		queued_set->predicates = g_strdupv (g_ptr_array_index (predicates, i));
		queued_set->values = g_strdupv (g_ptr_array_index (values, i));
		queued_set->modseq = modseq;

		g_queue_push_tail (many_queue, queued_set);

		i++;
	}

	if (start_handler) {
		start_many_handler (object);
	}

	dbus_g_method_return (context);
}




void
tracker_kmail_registrar_unset_many (TrackerKMailRegistrar *object, 
				    const GStrv subjects, 
				    const guint modseq,
				    DBusGMethodInvocation *context,
				    GError *derror)
{
	guint i = 0;

	dbus_async_return_if_fail (subjects != NULL, context);

	while (subjects[i] != NULL) {
		perform_unset (object, subjects[i], TRUE);
		i++;
	}

	tracker_store_queue_commit (on_commit, GUINT_TO_POINTER (modseq), NULL);

	dbus_g_method_return (context);
}

void
tracker_kmail_registrar_unset (TrackerKMailRegistrar *object, 
			       const gchar *subject, 
			       const guint modseq,
			       DBusGMethodInvocation *context,
			       GError *derror)
{
	dbus_async_return_if_fail (subject != NULL, context);

	perform_unset (object, subject, FALSE);

	dbus_g_method_return (context);
}

void
tracker_kmail_registrar_cleanup (TrackerKMailRegistrar *object, 
				 const guint modseq,
				 DBusGMethodInvocation *context,
				 GError *derror)
{
	perform_cleanup (object);

	set_stored_last_modseq (modseq);

	dbus_g_method_return (context);
}


static void
on_manager_destroy (DBusGProxy *proxy, gpointer user_data)
{
	return;
}

static void
tracker_kmail_push_registrar_enable (TrackerPushRegistrar *registrar, 
				     DBusGConnection      *connection,
				     DBusGProxy           *dbus_proxy, 
				     GError              **error)
{
	GError *nerror = NULL;
	guint result;
	DBusGProxy *manager_proxy;
	GObject *object;

	tracker_push_registrar_set_object (registrar, NULL);
	tracker_push_registrar_set_manager (registrar, NULL);

	manager_proxy = dbus_g_proxy_new_for_name (connection,
						   TRACKER_KMAIL_MANAGER_SERVICE,
						   TRACKER_KMAIL_MANAGER_PATH,
						   TRACKER_KMAIL_MANAGER_INTERFACE);

	/* Creation of the registrar */
	if (!org_freedesktop_DBus_request_name (dbus_proxy, 
						TRACKER_KMAIL_REGISTRAR_SERVICE,
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&result, &nerror)) {

		g_critical ("Could not setup DBus, %s in use\n", 
			    TRACKER_KMAIL_REGISTRAR_SERVICE);

		if (nerror) {
			g_propagate_error (error, nerror);
			return;
		}
	}

	if (nerror) {
		g_propagate_error (error, nerror);
		return;
	}

	object = g_object_new (TRACKER_TYPE_KMAIL_REGISTRAR, 
			       "connection", connection, NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_tracker_kmail_registrar_object_info);

	dbus_g_connection_register_g_object (connection, 
					     TRACKER_KMAIL_REGISTRAR_PATH, 
					     object);

	/* Registration of the registrar to the manager */
	dbus_g_proxy_call_no_reply (manager_proxy, "Register",
				    G_TYPE_OBJECT, object, 
				    G_TYPE_UINT, (guint) tracker_data_manager_get_db_option_int ("KMailLastModseq"),
				    G_TYPE_INVALID,
				    G_TYPE_INVALID);

	/* If while we had a proxy for the manager the manager shut itself down,
	 * then we'll get rid of our registrar too, in on_manager_destroy */

	g_signal_connect (manager_proxy, "destroy",
			  G_CALLBACK (on_manager_destroy), registrar);

	tracker_push_registrar_set_object (registrar, object);
	tracker_push_registrar_set_manager (registrar, manager_proxy);

	g_object_unref (object); /* sink own */
	g_object_unref (manager_proxy);  /* sink own */
}

static void
tracker_kmail_push_registrar_disable (TrackerPushRegistrar *registrar)
{
	tracker_push_registrar_set_object (registrar, NULL);
	tracker_push_registrar_set_manager (registrar, NULL);
}

static void
tracker_kmail_push_registrar_class_init (TrackerKMailPushRegistrarClass *klass)
{
	TrackerPushRegistrarClass *p_class = TRACKER_PUSH_REGISTRAR_CLASS (klass);

	p_class->enable = tracker_kmail_push_registrar_enable;
	p_class->disable = tracker_kmail_push_registrar_disable;
}

static void
tracker_kmail_push_registrar_init (TrackerKMailPushRegistrar *registrar)
{
	return;
}

TrackerPushRegistrar *
tracker_push_module_init (void)
{
	GObject *object;

	object = g_object_new (TRACKER_TYPE_KMAIL_PUSH_REGISTRAR, NULL);

	tracker_push_registrar_set_service (TRACKER_PUSH_REGISTRAR (object),
					    TRACKER_KMAIL_MANAGER_SERVICE);

	return TRACKER_PUSH_REGISTRAR (object);
}

void
tracker_push_module_shutdown (TrackerPushRegistrar *registrar)
{
	tracker_kmail_push_registrar_disable (registrar);
}
