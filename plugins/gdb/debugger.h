/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
    debugger.h
    Copyright (C) Naba Kumar <naba@gnome.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef __DEBUGGER_H__
#define __DEBUGGER_H__

#include <sys/types.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "gdbmi.h"

#include "preferences.h"

#include <libanjuta/interfaces/ianjuta-message-view.h>
#include <libanjuta/interfaces/ianjuta-debugger.h>
#include <libanjuta/interfaces/ianjuta-debugger-breakpoint.h>
#include <libanjuta/interfaces/ianjuta-debugger-memory.h>
#include <libanjuta/interfaces/ianjuta-debugger-instruction.h>
#include <libanjuta/interfaces/ianjuta-debugger-variable.h>

G_BEGIN_DECLS

typedef struct _Debugger        Debugger;
typedef struct _DebuggerClass   DebuggerClass;
typedef struct _DebuggerPriv    DebuggerPriv;
typedef struct _DebuggerCommand DebuggerCommand;

#define DEBUGGER_TYPE            (debugger_get_type ())
#define DEBUGGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), DEBUGGER_TYPE, Debugger))
#define DEBUGGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), DEBUGGER_TYPE, DebuggerClass))
#define IS_DEBUGGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DEBUGGER_TYPE))
#define IS_DEBUGGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DEBUGGER_TYPE))

typedef enum
{
	DEBUGGER_BREAKPOINT,
} DebuggerBreakpointType;

typedef void (*DebuggerParserFunc) (Debugger *debugger,
									const GDBMIValue *mi_result,
									const GList *cli_result,
									GError* error);

typedef enum
{
	DEBUGGER_COMMAND_NO_ERROR = 1 << 0,
	DEBUGGER_COMMAND_KEEP_RESULT = 1 << 1,
	DEBUGGER_COMMAND_PREPEND = 1 << 2,
} DebuggerCommandFlags;


struct _DebuggerCommand
{
	gchar *cmd;
	DebuggerCommandFlags flags;
	DebuggerParserFunc parser;
	IAnjutaDebuggerCallback callback;
	gpointer user_data;
};

struct _Debugger
{
	GObject parent;
	DebuggerPriv *priv;
};

struct _DebuggerClass
{
	GObjectClass parent_class;
};

GType debugger_get_type (void);

Debugger* debugger_new (GtkWindow *parent_win, GObject* instance);
void debugger_free (Debugger *debugger);

gboolean debugger_set_pretty_printers (Debugger *debugger,
									const GList *pretty_printers);

gboolean debugger_start (Debugger *debugger,
						const GList *search_dirs,
						const gchar *prog,
						gboolean is_libtool_prog);

gboolean debugger_stop (Debugger *debugger);
gboolean debugger_abort (Debugger *debugger);

void debugger_set_output_callback (Debugger *debugger, IAnjutaDebuggerOutputCallback callback, gpointer user_data);

/* Status */
gboolean debugger_is_ready (Debugger *debugger);
gboolean debugger_program_is_running (Debugger *debugger);
gboolean debugger_program_is_attached (Debugger *debugger);
gboolean debugger_program_is_loaded (Debugger *debugger);
IAnjutaDebuggerState debugger_get_state (Debugger *debugger);

/* Send standard gdb MI2 or CLI commands */
void debugger_command (Debugger *debugger, const gchar *command,
					   gboolean suppress_error, DebuggerParserFunc parser,
					   gpointer user_data);

void debugger_program_moved (Debugger *debugger, const gchar *file,
							   gint line, gulong address);
gchar* debugger_get_source_path (Debugger *debugger, const gchar *file);

/* Program loading */
void debugger_load_executable (Debugger *debugger, const gchar *prog);
void debugger_load_core (Debugger *debugger, const gchar *core);
void debugger_attach_process (Debugger *debugger, pid_t pid);
void debugger_detach_process (Debugger *debugger);

/* Environment */
gboolean debugger_set_working_directory (Debugger *debugger, const gchar *directory);
gboolean debugger_set_environment (Debugger *debugger, gchar **variables);

/* Execution */
void debugger_start_program (Debugger *debugger, const gchar *server, const gchar* args, const gchar* tty, gboolean stop);
void debugger_stop_program (Debugger *debugger);
void debugger_restart_program (Debugger *debugger);
void debugger_interrupt (Debugger *debugger);
void debugger_run (Debugger *debugger);
void debugger_step_in (Debugger *debugger);
void debugger_step_over (Debugger *debugger);
void debugger_step_out (Debugger *debugger);
void debugger_stepi_in (Debugger *debugger);
void debugger_stepi_over (Debugger *debugger);
void debugger_run_to_location (Debugger *debugger, const gchar *loc);
void debugger_run_to_position (Debugger *debugger, const gchar *file, guint line);
void debugger_run_from_position (Debugger *debugger, const gchar *file, guint line);
void debugger_run_to_address (Debugger *debugger, gulong address);
void debugger_run_from_address (Debugger *debugger, gulong address);

/* Breakpoint */
void debugger_add_breakpoint_at_line (Debugger *debugger, const gchar* file, guint line, IAnjutaDebuggerBreakpointCallback callback, gpointer user_data);
void debugger_add_breakpoint_at_function (Debugger *debugger, const gchar* file, const gchar* function, IAnjutaDebuggerBreakpointCallback callback, gpointer user_data);
void debugger_add_breakpoint_at_address (Debugger *debugger, gulong address, IAnjutaDebuggerBreakpointCallback callback, gpointer user_data);
void debugger_remove_breakpoint (Debugger *debugger, guint id, IAnjutaDebuggerBreakpointCallback callback, gpointer user_data);
void debugger_list_breakpoint (Debugger *debugger, IAnjutaDebuggerGListCallback callback, gpointer user_data);
void debugger_enable_breakpoint (Debugger *debugger, guint id, gboolean enable, IAnjutaDebuggerBreakpointCallback callback, gpointer user_data);
void debugger_ignore_breakpoint (Debugger *debugger, guint id, guint ignore, IAnjutaDebuggerBreakpointCallback callback, gpointer user_data);
void debugger_condition_breakpoint (Debugger *debugger, guint id, const gchar* condition, IAnjutaDebuggerBreakpointCallback callback, gpointer user_data);

/* Variable */
void debugger_print (Debugger *debugger, const gchar* variable, IAnjutaDebuggerGCharCallback callback, gpointer user_data);
void debugger_evaluate (Debugger *debugger, const gchar* name, IAnjutaDebuggerGCharCallback callback, gpointer user_data);

/* Info */
void debugger_list_local (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);
void debugger_list_argument (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);
void debugger_info_signal (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);
void debugger_info_sharedlib (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);
void debugger_inspect_memory (Debugger *debugger, gulong address, guint length, IAnjutaDebuggerMemoryCallback func, gpointer user_data);
void debugger_disassemble (Debugger *debugger, gulong address, guint length, IAnjutaDebuggerInstructionCallback func, gpointer user_data);

/* Register */

void debugger_list_register (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);
void debugger_update_register (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);
void debugger_write_register (Debugger *debugger, const gchar *name, const gchar *value);

/* Stack */
void debugger_list_argument (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);
void debugger_list_frame (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);
void debugger_set_frame (Debugger *debugger, gsize frame);
void debugger_dump_stack_trace (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);

/* Thread */
void debugger_list_thread (Debugger *debugger, IAnjutaDebuggerGListCallback func, gpointer user_data);
void debugger_set_thread (Debugger *debugger, gint thread);
void debugger_info_thread (Debugger *debugger, gint thread, IAnjutaDebuggerGListCallback func, gpointer user_data);

/* Log */
void debugger_set_log (Debugger *debugger, IAnjutaMessageView *view);

/* Variable object */
void debugger_delete_variable (Debugger *debugger, const gchar *name);
void debugger_evaluate_variable (Debugger *debugger, const gchar *name, IAnjutaDebuggerGCharCallback callback, gpointer user_data);
void debugger_assign_variable (Debugger *debugger, const gchar *name, const gchar *value);
void debugger_list_variable_children (Debugger *debugger, const gchar* name, guint from, IAnjutaDebuggerGListCallback callback, gpointer user_data);
void debugger_create_variable (Debugger *debugger, const gchar *name, IAnjutaDebuggerVariableCallback callback, gpointer user_data);
void debugger_update_variable (Debugger *debugger, IAnjutaDebuggerGListCallback callback, gpointer user_data);

#if 0

/* Evaluations */
void debugger_query_execute (void);
void debugger_query_evaluate_expr_tip (const gchar *expr,
									   DebuggerFunc parser,
									   gpointer data);
void debugger_query_evaluate_expr (const gchar *expr,
								   DebuggerFunc parser,
								   gpointer data);
#endif

G_END_DECLS

#endif
