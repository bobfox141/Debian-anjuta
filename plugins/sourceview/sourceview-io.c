/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta-trunk
 * Copyright (C) Johannes Schmid 2008 <jhs@gnome.org>
 *
 * anjuta-trunk is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * anjuta-trunk is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sourceview-io.h"
#include "sourceview-private.h"
#include <libanjuta/interfaces/ianjuta-editor.h>
#include <libanjuta/anjuta-convert.h>
#include <libanjuta/anjuta-encodings.h>

#define READ_SIZE 4096
#define RATE_LIMIT 5000 /* Use a big rate limit to avoid duplicates */

enum
{
	SAVE_STATUS,
	SAVE_FINISHED,
	OPEN_STATUS,
	OPEN_FINISHED,
	OPEN_FAILED,
	SAVE_FAILED,
	FILE_DELETED,

	LAST_SIGNAL
};
static guint io_signals[LAST_SIGNAL] = { 0 };

#define IO_ERROR_QUARK g_quark_from_string ("SourceviewIO-Error")

#define IO_PRIORITY G_PRIORITY_DEFAULT

G_DEFINE_TYPE (SourceviewIO, sourceview_io, G_TYPE_OBJECT);

static void
on_sourceview_finalized (gpointer data, GObject* where_the_object_was)
{
	SourceviewIO* sio = SOURCEVIEW_IO (data);

	sio->sv = NULL;
	/* Cancel all open operations */
	g_cancellable_cancel (sio->open_cancel);
}

static void
sourceview_io_init (SourceviewIO *object)
{
	object->file = NULL;
	object->filename = NULL;
	object->read_buffer = NULL;
	object->write_buffer = NULL;
	object->open_cancel = g_cancellable_new();
	object->monitor = NULL;
	object->last_encoding = NULL;
	object->bytes_read = 0;
}

static void
sourceview_io_finalize (GObject *object)
{
	SourceviewIO* sio = SOURCEVIEW_IO(object);

	if (sio->sv)
		g_object_weak_unref (G_OBJECT (sio->sv), on_sourceview_finalized, sio);

	if (sio->file)
		g_object_unref (sio->file);
	g_free (sio->etag);
	g_free(sio->filename);
	g_free(sio->read_buffer);
	g_free(sio->write_buffer);
	g_object_unref (sio->open_cancel);
	if (sio->monitor)
		g_object_unref (sio->monitor);

	G_OBJECT_CLASS (sourceview_io_parent_class)->finalize (object);
}

static void
sourceview_io_class_init (SourceviewIOClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = sourceview_io_finalize;

	klass->changed = NULL;
	klass->deleted = NULL;
	klass->save_finished = NULL;
	klass->open_finished = NULL;
	klass->open_failed = NULL;
	klass->save_failed = NULL;

	io_signals[SAVE_STATUS] =
		g_signal_new ("changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              0,
		              G_STRUCT_OFFSET (SourceviewIOClass, changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              NULL);

	io_signals[SAVE_FINISHED] =
		g_signal_new ("save-finished",
		              G_OBJECT_CLASS_TYPE (klass),
		              0,
		              G_STRUCT_OFFSET (SourceviewIOClass, save_finished),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              NULL);

	io_signals[OPEN_FINISHED] =
		g_signal_new ("open-finished",
		              G_OBJECT_CLASS_TYPE (klass),
		              0,
		              G_STRUCT_OFFSET (SourceviewIOClass, open_finished),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              NULL);

	io_signals[OPEN_FAILED] =
		g_signal_new ("open-failed",
		              G_OBJECT_CLASS_TYPE (klass),
		              0,
		              G_STRUCT_OFFSET (SourceviewIOClass, open_failed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1,
		              G_TYPE_POINTER);

	io_signals[SAVE_FAILED] =
		g_signal_new ("save-failed",
		              G_OBJECT_CLASS_TYPE (klass),
		              0,
		              G_STRUCT_OFFSET (SourceviewIOClass, save_failed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1,
		              G_TYPE_POINTER);

	io_signals[FILE_DELETED] =
		g_signal_new ("deleted",
		              G_OBJECT_CLASS_TYPE (klass),
		              0,
		              G_STRUCT_OFFSET (SourceviewIOClass, deleted),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              NULL);
}

static void on_file_changed (GFileMonitor* monitor,
							 GFile* file,
							 GFile* other_file,
							 GFileMonitorEvent event_type,
							 gpointer data)
{
	SourceviewIO* sio = SOURCEVIEW_IO(data);

	switch (event_type)
	{
		case G_FILE_MONITOR_EVENT_CREATED:
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		{
			GFileInfo* info;
			const gchar* etag;

			info = g_file_query_info (file, G_FILE_ATTRIBUTE_ETAG_VALUE,
			                          G_FILE_QUERY_INFO_NONE, NULL, NULL);
			if (!info)
				break;

			etag = g_file_info_get_etag (info);

			/* Only emit "changed" if the etag has changed. */
			if (g_strcmp0 (sio->etag, etag))
				g_signal_emit_by_name (sio, "changed");

			g_object_unref (info);
			break;
		}
		case G_FILE_MONITOR_EVENT_DELETED:
			g_signal_emit_by_name (sio, "deleted");
			break;
		default:
			break;
	}
}

static void
setup_monitor(SourceviewIO* sio)
{
	if (sio->monitor != NULL)
		g_object_unref (sio->monitor);
	sio->monitor = g_file_monitor_file (sio->file,
	                                    G_FILE_MONITOR_NONE,
	                                    NULL,
	                                    NULL);
	if (sio->monitor)
	{
		g_signal_connect (sio->monitor, "changed",
		                  G_CALLBACK(on_file_changed), sio);
		g_file_monitor_set_rate_limit (sio->monitor, RATE_LIMIT);
	}
}

static void
sourceview_io_unset_current_file (SourceviewIO* sio)
{
	g_clear_object (&sio->file);
	g_clear_object (&sio->monitor);

	g_free (sio->etag);
	sio->etag = NULL;

	g_free (sio->filename);
	sio->filename = NULL;
}

static void
set_display_name (SourceviewIO* sio)
{
	GFileInfo* file_info = g_file_query_info (sio->file,
											  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
											  G_FILE_QUERY_INFO_NONE,
											  NULL,
											  NULL);
	if (file_info)
	{
		g_free (sio->filename);
		sio->filename = g_strdup(g_file_info_get_display_name (file_info));
	}
	else
	{
		g_free (sio->filename);
		sio->filename = NULL;
	}
	g_object_unref (file_info);
}

/*
 * This function may be called after the corresponding Sourceview (sio->sv)
 * has been destroyed.
 */
static void
on_save_finished (GObject* file, GAsyncResult* result, gpointer data)
{
	SourceviewIO* sio = SOURCEVIEW_IO(data);
	
	GError* err = NULL;
	gchar* etag;

	g_file_replace_contents_finish (G_FILE (file),
	                                result,
	                                &etag,
	                                &err);
	g_free (sio->write_buffer);
	sio->write_buffer = NULL;
	if (err)
	{
		g_signal_emit_by_name (sio, "save-failed", err);
		g_error_free (err);
	}
	else
	{
		set_display_name (sio);
		if (!sio->monitor)
			setup_monitor (sio);

		g_free (sio->etag);
		sio->etag = etag;

		g_signal_emit_by_name (sio, "save-finished");
	}

	if (sio->shell)
		anjuta_shell_saving_pop (sio->shell);

	g_object_unref (sio);
}

void
sourceview_io_save (SourceviewIO* sio)
{
	g_return_if_fail (SOURCEVIEW_IS_IO (sio));
	g_return_if_fail (sio->sv != NULL);

	if (!sio->file)
	{
		GError* error = NULL;
		g_set_error (&error, IO_ERROR_QUARK, 0,
					 _("Could not save file because filename not yet specified"));
		g_signal_emit_by_name (sio, "save-failed", error);
		g_error_free(error);
	}
	else
		sourceview_io_save_as (sio, sio->file);
}

void
sourceview_io_save_as (SourceviewIO* sio, GFile* file)
{
	gboolean backup = TRUE;
	gsize len;

	g_return_if_fail (SOURCEVIEW_IS_IO (sio));
	g_return_if_fail (sio->sv != NULL);
	g_return_if_fail (G_IS_FILE (file));

	if (sio->file != file)
	{
		sourceview_io_unset_current_file (sio);

		sio->file = g_object_ref (file);
	}

	backup = g_settings_get_boolean (sio->sv->priv->settings,
	                                 "backup");

	if (sio->last_encoding == NULL)
	{
		sio->write_buffer = ianjuta_editor_get_text_all (IANJUTA_EDITOR(sio->sv),
														 NULL);
		len = strlen (sio->write_buffer);
	}
	else
	{
		GError* err = NULL;
		gchar* buffer_text = ianjuta_editor_get_text_all (IANJUTA_EDITOR(sio->sv),
														  NULL);
		sio->write_buffer = anjuta_convert_from_utf8 (buffer_text,
													  -1,
													  sio->last_encoding,
													  &len,
													  &err);
		g_free (buffer_text);
		if (err != NULL)
		{
			g_signal_emit_by_name (sio, "save-failed", err);
			g_error_free(err);
			return;
		}
	}
	g_file_replace_contents_async (file,
	                               sio->write_buffer,
	                               len,
	                               NULL,
	                               backup,
	                               G_FILE_CREATE_NONE,
	                               NULL,
	                               on_save_finished,
	                               sio);
	anjuta_shell_saving_push (sio->shell);

	g_object_ref (sio);
}

static void insert_text_in_document(SourceviewIO* sio, const gchar* text, gsize len)
{
	GtkSourceBuffer* document = GTK_SOURCE_BUFFER (sio->sv->priv->document);
	gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (sio->sv->priv->document));

	/* Insert text in the buffer */
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (document),
							  text,
							  len);

	gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (document),
				      FALSE);

	gtk_source_buffer_end_not_undoable_action (document);
}

static gboolean
append_buffer (SourceviewIO* sio, gsize size)
{
	/* Text is utf-8 - good */
	if (g_utf8_validate (sio->read_buffer, size, NULL))
	{
		insert_text_in_document (sio, sio->read_buffer, size);
	}
	else
	{
		/* Text is not utf-8 */
		GError *conv_error = NULL;
		gchar *converted_text = NULL;
		gsize new_len = size;
		const AnjutaEncoding* enc = NULL;

		converted_text = anjuta_convert_to_utf8 (sio->read_buffer,
												 size,
												 &enc,
												 &new_len,
												 &conv_error);
		if  (converted_text == NULL)
		{
			/* Last chance, let's try 8859-15 */
			enc = anjuta_encoding_get_from_charset( "ISO-8859-15");
			conv_error = NULL;
			converted_text = anjuta_convert_to_utf8 (sio->read_buffer,
													 size,
													 &enc,
													 &new_len,
													 &conv_error);
		}
		if (converted_text == NULL)
		{
			g_return_val_if_fail (conv_error != NULL, FALSE);

			g_signal_emit_by_name (sio, "open-failed", conv_error);
			g_error_free (conv_error);
			return FALSE;
		}
		sio->last_encoding = enc;
		insert_text_in_document (sio, converted_text, new_len);
		g_free (converted_text);
	}
	return TRUE;
}

static void
on_read_finished (GObject* input, GAsyncResult* result, gpointer data)
{
	SourceviewIO* sio = SOURCEVIEW_IO(data);
	GInputStream* input_stream = G_INPUT_STREAM(input);
	gsize current_bytes = 0;
	GError* err = NULL;

	if (!g_cancellable_set_error_if_cancelled (sio->open_cancel, &err))
		current_bytes = g_input_stream_read_finish (input_stream, result, &err);
	if (err)
	{
		g_signal_emit_by_name (sio, "open-failed", err);
		g_error_free (err);
	}
	else
	{
		/* Check that the parent Sourceview is still alive. */
		if (sio->sv == NULL)
		{
			g_warning ("Sourceview was destroyed without canceling SourceviewIO open operation");
			goto out;
		}

		sio->bytes_read += current_bytes;
		if (current_bytes != 0)
		{
			sio->read_buffer = g_realloc (sio->read_buffer, sio->bytes_read + READ_SIZE);
			g_input_stream_read_async (G_INPUT_STREAM (input_stream),
									   sio->read_buffer + sio->bytes_read,
									   READ_SIZE,
									   IO_PRIORITY,
									   sio->open_cancel,
									   on_read_finished,
									   sio);
			return;
		}
		else
		{
			GFileInfo* info;

			info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM (input_stream),
			                                       G_FILE_ATTRIBUTE_ETAG_VALUE,
			                                       NULL, &err);
			if (!info)
			{
				g_signal_emit_by_name (sio, "open-failed", err);
				g_error_free (err);
			}
			else
			{
				g_free (sio->etag);
				sio->etag = g_strdup (g_file_info_get_etag (info));
				g_object_unref (info);

				if (append_buffer (sio, sio->bytes_read))
					g_signal_emit_by_name (sio, "open-finished");
				setup_monitor (sio);
			}
		}
	}

out:
	g_object_unref (input_stream);
	g_free (sio->read_buffer);
	sio->read_buffer = NULL;
	sio->bytes_read = 0;
	g_object_unref (sio);
}

void
sourceview_io_open (SourceviewIO* sio, GFile* file)
{
	GFileInputStream* input_stream;
	GError* err = NULL;

	g_return_if_fail (SOURCEVIEW_IS_IO (sio));
	g_return_if_fail (sio->sv != NULL);
	g_return_if_fail (G_IS_FILE (file));

	if (sio->file != file)
	{
		sourceview_io_unset_current_file (sio);

		sio->file = g_object_ref (file);
		set_display_name(sio);
	}

	input_stream = g_file_read (file, NULL, &err);
	if (!input_stream)
	{
		g_signal_emit_by_name (sio, "open-failed", err);
		g_error_free (err);
		return;
	}
	sio->read_buffer = g_realloc (sio->read_buffer, READ_SIZE);
	g_input_stream_read_async (G_INPUT_STREAM (input_stream),
							   sio->read_buffer,
							   READ_SIZE,
							   IO_PRIORITY,
							   sio->open_cancel,
							   on_read_finished,
							   g_object_ref (sio));
}

GFile*
sourceview_io_get_file (SourceviewIO* sio)
{
	if (sio->file)
		g_object_ref (sio->file);
	return sio->file;
}

const gchar*
sourceview_io_get_filename (SourceviewIO* sio)
{
	static gint new_file_count = 1;
	if (sio->filename == NULL) /* new file */
	{
		sio->filename = g_strdup_printf (_("New file %d"), new_file_count++);
	}
	return sio->filename;
}

void
sourceview_io_set_filename (SourceviewIO* sio, const gchar* filename)
{
	g_free (sio->filename);
	sio->filename = g_strdup(filename);
}

gchar*
sourceview_io_get_mime_type (SourceviewIO* sio)
{
	GFileInfo* file_info;

	if (!sio->file)
		return NULL;

	file_info = g_file_query_info (sio->file,
								   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
								   G_FILE_QUERY_INFO_NONE,
								   NULL,
								   NULL);
	if (file_info)
	{
		gchar* mime_type = g_strdup (g_file_info_get_content_type (file_info));
		g_object_unref (file_info);
		return mime_type;
	}
	else
		return NULL;

}

gboolean
sourceview_io_get_read_only (SourceviewIO* sio)
{
	GFileInfo* file_info;
	gboolean retval;

	if (!sio->file)
		return FALSE;

	file_info = g_file_query_info (sio->file,
								   G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
								   G_FILE_QUERY_INFO_NONE,
								   NULL,
								   NULL);
	if (!file_info)
		return FALSE;

	retval = !g_file_info_get_attribute_boolean (file_info,
												G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
	g_object_unref (file_info);
	return retval;
}

SourceviewIO*
sourceview_io_new (Sourceview* sv)
{
	SourceviewIO* sio;

	g_return_val_if_fail (ANJUTA_IS_SOURCEVIEW(sv), NULL);

	sio = SOURCEVIEW_IO(g_object_new (SOURCEVIEW_TYPE_IO, NULL));

	sio->sv = sv;
	g_object_weak_ref (G_OBJECT (sv), on_sourceview_finalized, sio);

	/* Store a separate pointer to the shell since we want to have access
	 * to it even though the parent Sourceview has been destroyed .*/
	sio->shell = ANJUTA_PLUGIN (sv->priv->plugin)->shell;
	g_object_add_weak_pointer (G_OBJECT (sio->shell), (gpointer*)&sio->shell);

	return sio;
}
