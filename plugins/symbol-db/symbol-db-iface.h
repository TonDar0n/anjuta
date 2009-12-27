/*
 * anjuta
 * Copyright (C) Massimo Cora' 2007-2009 <maxcvs@email.it>
 * 
 * anjuta is free software.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#ifndef _SYMBOL_DB_IFACE_H_
#define _SYMBOL_DB_IFACE_H_

#include <glib-object.h>
#include <glib.h>

#include <libanjuta/anjuta-async-command.h>
#include <libanjuta/anjuta-async-notify.h>
#include <libanjuta/interfaces/libanjuta-interfaces.h>

void
isymbol_manager_iface_init (IAnjutaSymbolManagerIface *iface);

IAnjutaIterable* 
isymbol_manager_search_file (IAnjutaSymbolManager *sm, IAnjutaSymbolType match_types, 
				gboolean include_types,  IAnjutaSymbolField info_fields, const gchar *pattern, 
			 	GFile *file, gint results_limit, gint results_offset, GError **err);

guint
isymbol_manager_search_file_async (IAnjutaSymbolManager *sm, IAnjutaSymbolType match_types, 
				gboolean include_types,  IAnjutaSymbolField info_fields, const gchar *pattern,     			
			 	GFile *file, gint results_limit, gint results_offset, 
                GCancellable* cancel, AnjutaAsyncNotify *notify, 
                IAnjutaSymbolManagerSearchCallback callback, 
				gpointer callback_user_data, GError **err);

IAnjutaIterable* 
isymbol_manager_search_project (IAnjutaSymbolManager *sm, IAnjutaSymbolType match_types, 
				gboolean include_types,  IAnjutaSymbolField info_fields, const gchar *pattern, 
				IAnjutaSymbolManagerSearchFileScope filescope_search,
    			gint results_limit, gint results_offset, GError **err);

guint
isymbol_manager_search_project_async (IAnjutaSymbolManager *sm, IAnjutaSymbolType match_types, 
				gboolean include_types,  IAnjutaSymbolField info_fields, const gchar *pattern, 
    			IAnjutaSymbolManagerSearchFileScope filescope_search,
				gint results_limit, gint results_offset, 
                GCancellable* cancel, AnjutaAsyncNotify *notify, 
                IAnjutaSymbolManagerSearchCallback callback, 
				gpointer callback_user_data, GError **err);

IAnjutaIterable* 
isymbol_manager_search_system (IAnjutaSymbolManager *sm, IAnjutaSymbolType match_types, 
				gboolean include_types,  IAnjutaSymbolField info_fields, const gchar *pattern, 
    			IAnjutaSymbolManagerSearchFileScope filescope_search,
			    gint results_limit, gint results_offset, GError **err);

guint
isymbol_manager_search_system_async (IAnjutaSymbolManager *sm, IAnjutaSymbolType match_types, 
				gboolean include_types,  IAnjutaSymbolField info_fields, const gchar *pattern, 
    			IAnjutaSymbolManagerSearchFileScope filescope_search,
			    gint results_limit, gint results_offset, 
                GCancellable* cancel, AnjutaAsyncNotify *notify, 
                IAnjutaSymbolManagerSearchCallback callback, 
				gpointer callback_user_data, GError **err);

IAnjutaSymbol*
isymbol_manager_get_symbol_by_id (IAnjutaSymbolManager *sm,
								  gint symbol_id, 
								  IAnjutaSymbolField info_fields,
								  GError **err);
 
IAnjutaIterable*
isymbol_manager_get_symbol_more_info (IAnjutaSymbolManager *sm,
								  IAnjutaSymbol *symbol, 
								  IAnjutaSymbolField info_fields,
								  GError **err);

IAnjutaIterable*
isymbol_manager_get_parent_scope (IAnjutaSymbolManager *sm,
								  IAnjutaSymbol *symbol, 
								  const gchar *filename, 
								  IAnjutaSymbolField info_fields,
								  GError **err);

IAnjutaIterable* 
isymbol_manager_search_symbol_in_scope (IAnjutaSymbolManager *sm,
                                        const gchar *pattern, 
                                        IAnjutaSymbol *container_symbol, 
                                        IAnjutaSymbolType match_types, 
                                        gboolean include_types, 
                                        gint results_limit, 
                                        gint results_offset, 
                                        IAnjutaSymbolField info_fields,
                                        GError **err);

IAnjutaIterable*
isymbol_manager_get_scope_chain (IAnjutaSymbolManager *sm,                                 
                                 const gchar* filename, 
                                 gulong line,
                                 IAnjutaSymbolField info_fields,
                                 GError **err);

IAnjutaIterable*
isymbol_manager_get_scope (IAnjutaSymbolManager *sm,
						   const gchar* filename,  
						   gulong line,  
						   IAnjutaSymbolField info_fields, 
						   GError **err);

IAnjutaIterable*
isymbol_manager_get_class_parents (IAnjutaSymbolManager *sm,
							 IAnjutaSymbol *symbol,
							 IAnjutaSymbolField info_fields,
							 GError **err);

IAnjutaIterable*
isymbol_manager_get_members (IAnjutaSymbolManager *sm,
							 IAnjutaSymbol *symbol, 
							 IAnjutaSymbolField info_fields,
							 GError **err);

IAnjutaIterable*
isymbol_manager_search (IAnjutaSymbolManager *sm,
						IAnjutaSymbolType match_types,
						gboolean include_types,
						IAnjutaSymbolField info_fields,
						const gchar *match_name,
						gboolean partial_name_match,
						IAnjutaSymbolManagerSearchFileScope filescope_search,
						gboolean global_tags_search,
						gint results_limit,
						gint results_offset,
						GError **err);

void
on_isymbol_manager_prj_symbol_inserted (SymbolDBEngine *dbe,
    									gint symbol_id,
    									IAnjutaSymbolManager *sm);

void
on_isymbol_manager_prj_symbol_removed (SymbolDBEngine *dbe,
    									gint symbol_id,
    									IAnjutaSymbolManager *sm);

void
on_isymbol_manager_prj_symbol_updated (SymbolDBEngine *dbe,
    									gint symbol_id,
    									IAnjutaSymbolManager *sm);

void
on_isymbol_manager_prj_scan_end (SymbolDBEngine *dbe,
    								gint process_id,
    								IAnjutaSymbolManager *sm);

void
on_isymbol_manager_sys_symbol_inserted (SymbolDBEngine *dbe,
    									gint symbol_id,
    									IAnjutaSymbolManager *sm);

void
on_isymbol_manager_sys_symbol_removed (SymbolDBEngine *dbe,
    									gint symbol_id,
    									IAnjutaSymbolManager *sm);

void
on_isymbol_manager_sys_symbol_updated (SymbolDBEngine *dbe,
    									gint symbol_id,
    									IAnjutaSymbolManager *sm);

void
on_isymbol_manager_sys_scan_end (SymbolDBEngine *dbe,    								
    								gint process_id,
    								IAnjutaSymbolManager *sm);

#endif /* _SYMBOL_DB_IFACE_H_ */


