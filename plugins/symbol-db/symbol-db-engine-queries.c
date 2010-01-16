/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * anjuta
 * Copyright (C) Massimo Cora' 2007-2008 <maxcvs@email.it>
 * 
 * anjuta is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * anjuta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with anjuta.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <libanjuta/anjuta-debug.h>

#include "symbol-db-engine-queries.h"
#include "symbol-db-engine-priv.h"
#include "libgda-extra/gda-data-model-concat.h"

/*
 * extern declarations 
 */

extern GNUC_INLINE const GdaStatement *
sdb_engine_get_statement_by_query_id (SymbolDBEngine * dbe, static_query_type query_id);

extern GNUC_INLINE const GdaSet *
sdb_engine_get_query_parameters_list (SymbolDBEngine *dbe, static_query_type query_id);

extern GNUC_INLINE const DynChildQueryNode *
sdb_engine_get_dyn_query_node_by_id (SymbolDBEngine *dbe, dyn_query_type query_id,
									 SymExtraInfo sym_info, gsize other_parameters);

extern void sdb_engine_dyn_child_query_node_destroy (gpointer data);

extern GNUC_INLINE const DynChildQueryNode *
sdb_engine_insert_dyn_query_node_by_id (SymbolDBEngine *dbe, dyn_query_type query_id,
									 	SymExtraInfo sym_info, gsize other_parameters,
										const gchar *sql);
extern GNUC_INLINE gint
sdb_engine_get_tuple_id_by_unique_name (SymbolDBEngine * dbe, static_query_type qtype,
										gchar * param_key,
										GValue * param_value);


/*
 * implementation starts here 
 */

static GNUC_INLINE gint
sdb_engine_walk_down_scope_path (SymbolDBEngine *dbe, const GPtrArray* scope_path) 
{
	SymbolDBEnginePriv *priv;
	gint final_definition_id;
	gint scope_path_len;
	gint i;
	GdaDataModel *data;
	const GdaSet *plist;
	const GdaStatement *stmt;
	GdaHolder *param;
	GValue *ret_value;
	gboolean ret_bool;	
	
		
	g_return_val_if_fail (dbe != NULL, FALSE);
	priv = dbe->priv;
	
	scope_path_len = scope_path->len;
	
	/* we'll return if the length is even or minor than 3 */
	if (scope_path_len < 3 || scope_path_len % 2 == 0)
	{
		g_warning ("bad scope_path.");
		return -1;
	}
	
	if ((stmt =
		 sdb_engine_get_statement_by_query_id (dbe, 
			PREP_QUERY_GET_SCOPE_DEFINITION_ID_BY_WALK_DOWN_SCOPE_PATH)) == NULL)
	{
		g_warning ("query is null"); 
		return -1;
	}

	plist = sdb_engine_get_query_parameters_list (dbe, 
			PREP_QUERY_GET_SCOPE_DEFINITION_ID_BY_WALK_DOWN_SCOPE_PATH);	
	final_definition_id = 0;
	for (i=0; i < scope_path_len -1; i = i + 2)
	{
		const GValue *value;

		if ((param = gda_set_get_holder ((GdaSet*)plist, "symtype")) == NULL)
		{
			return -1;
		}
		
		MP_SET_HOLDER_BATCH_STR(priv, param, (gchar*)g_ptr_array_index (scope_path, i), 
								ret_bool, ret_value);
		
		if ((param = gda_set_get_holder ((GdaSet*)plist, "scopename")) == NULL)
		{
			return -1;
		}

		MP_SET_HOLDER_BATCH_STR(priv, param, (gchar*)g_ptr_array_index (scope_path, i + 1), 
								ret_bool, ret_value);		
		
		if ((param = gda_set_get_holder ((GdaSet*)plist, "scopeid")) == NULL)
		{
			return -1;
		}
		
		MP_SET_HOLDER_BATCH_INT(priv, param, final_definition_id, ret_bool, ret_value);

		data = gda_connection_statement_execute_select (priv->db_connection, 
														  (GdaStatement*)stmt, 
														  (GdaSet*)plist, NULL);
	
		if (!GDA_IS_DATA_MODEL (data) ||
			gda_data_model_get_n_rows (GDA_DATA_MODEL (data)) <= 0)
		{
			if (data != NULL)
				g_object_unref (data);
			return -1;
		}
		
		value = gda_data_model_get_value_at (data, 0, 0, NULL);
		if (G_VALUE_HOLDS (value, G_TYPE_INT))
		{
			final_definition_id = g_value_get_int (value);
			g_object_unref (data);
		}
		else
		{
			/* something went wrong. Our symbol cannot be retrieved coz of a
			 * bad scope path.
			 */
			final_definition_id = -1;
			break;
		}
	}	
	
	return final_definition_id;
}

static GNUC_INLINE void
sdb_engine_prepare_file_info_sql (SymbolDBEngine *dbe, GString *info_data,
									GString *join_data, SymExtraInfo sym_info) 
{
	if (sym_info & SYMINFO_FILE_PATH 	|| 
		sym_info & SYMINFO_LANGUAGE  	||
		sym_info & SYMINFO_PROJECT_NAME ||
		sym_info & SYMINFO_FILE_IGNORE  ||
		sym_info & SYMINFO_FILE_INCLUDE) 
	{
		info_data = g_string_append (info_data, ",file.file_path AS db_file_path ");
		join_data = g_string_append (join_data, "LEFT JOIN file ON "
				"symbol.file_defined_id = file.file_id ");
	}

	if (sym_info & SYMINFO_LANGUAGE)
	{
		info_data = g_string_append (info_data, ",language.language_name "
									 "AS language_name ");
		join_data = g_string_append (join_data, "LEFT JOIN language ON "
				"file.lang_id = language.language_id ");
	}
	
	if (sym_info & SYMINFO_PROJECT_NAME ||
		sym_info & SYMINFO_FILE_IGNORE  ||
		sym_info & SYMINFO_FILE_INCLUDE)
	{
		info_data = g_string_append (info_data, ",project.project_name AS project_name ");
		join_data = g_string_append (join_data, "LEFT JOIN project ON "
				"file.prj_id = project.project_id ");
	}	

	if (sym_info & SYMINFO_FILE_IGNORE)
	{
		info_data = g_string_append (info_data, ",file_ignore.file_ignore_type "
									 "AS ignore_type ");
		join_data = g_string_append (join_data, "LEFT JOIN ext_ignore ON "
				"ext_ignore.prj_id = project.project_id "
				"LEFT JOIN file_ignore ON "
				"ext_ignore.file_ign_id = file_ignore.file_ignore_id ");
	}

	if (sym_info & SYMINFO_FILE_INCLUDE)
	{
		info_data = g_string_append (info_data, ",file_include.file_include_type "
									 "AS file_include_type ");
		join_data = g_string_append (join_data, "LEFT JOIN ext_include ON "
				"ext_include.prj_id = project.project_id "
				"LEFT JOIN file_include ON "
				"ext_include.file_incl_id = file_include.file_include_id ");
	}	
}

static GNUC_INLINE void
sdb_engine_prepare_symbol_info_sql (SymbolDBEngine *dbe, GString *info_data,
									GString *join_data, SymExtraInfo sym_info) 
{
	if (sym_info & SYMINFO_FILE_PATH 	|| 
		sym_info & SYMINFO_LANGUAGE  	||
		sym_info & SYMINFO_PROJECT_NAME ||
		sym_info & SYMINFO_FILE_IGNORE  ||
		sym_info & SYMINFO_FILE_INCLUDE) 
	{
		info_data = g_string_append (info_data, ",file.file_path AS db_file_path ");
		join_data = g_string_append (join_data, "LEFT JOIN file ON "
				"symbol.file_defined_id = file.file_id ");
	}

	if (sym_info & SYMINFO_LANGUAGE)
	{
		info_data = g_string_append (info_data, ",language.language_name "
									 "AS language_name ");
		join_data = g_string_append (join_data, "LEFT JOIN language ON "
				"file.lang_id = language.language_id ");
	}
	
	if (sym_info & SYMINFO_IMPLEMENTATION)
	{
		info_data = g_string_append (info_data, ",sym_implementation.implementation_name "
									 "AS implementation_name " );
		join_data = g_string_append (join_data, "LEFT JOIN sym_implementation ON "
				"symbol.implementation_kind_id = sym_implementation.sym_impl_id ");
	}
	
	if (sym_info & SYMINFO_ACCESS)
	{
		info_data = g_string_append (info_data, ",sym_access.access_name AS access_name ");
		join_data = g_string_append (join_data, "LEFT JOIN sym_access ON "
				"symbol.access_kind_id = sym_access.access_kind_id ");
	}
	
	if (sym_info & SYMINFO_KIND)
	{
		info_data = g_string_append (info_data, ",sym_kind.kind_name AS kind_name");
		join_data = g_string_append (join_data, "LEFT JOIN sym_kind ON "
				"symbol.kind_id = sym_kind.sym_kind_id ");
	}
	
	if (sym_info & SYMINFO_TYPE || sym_info & SYMINFO_TYPE_NAME)
	{
		info_data = g_string_append (info_data, ",sym_type.type_type AS type_type, "
									 "sym_type.type_name AS type_name");
		join_data = g_string_append (join_data, "LEFT JOIN sym_type ON "
				"symbol.type_id = sym_type.type_id ");
	}

	if (sym_info & SYMINFO_PROJECT_NAME ||
		sym_info & SYMINFO_FILE_IGNORE  ||
		sym_info & SYMINFO_FILE_INCLUDE)
	{
		info_data = g_string_append (info_data, ",project.project_name AS project_name ");
		join_data = g_string_append (join_data, "LEFT JOIN project ON "
				"file.prj_id = project.project_id ");
	}	

	if (sym_info & SYMINFO_FILE_IGNORE)
	{
		info_data = g_string_append (info_data, ",file_ignore.file_ignore_type "
									 "AS ignore_type ");
		join_data = g_string_append (join_data, "LEFT JOIN ext_ignore ON "
				"ext_ignore.prj_id = project.project_id "
				"LEFT JOIN file_ignore ON "
				"ext_ignore.file_ign_id = file_ignore.file_ignore_id ");
	}

	if (sym_info & SYMINFO_FILE_INCLUDE)
	{
		info_data = g_string_append (info_data, ",file_include.file_include_type "
									 "AS file_include_type ");
		join_data = g_string_append (join_data, "LEFT JOIN ext_include ON "
				"ext_include.prj_id = project.project_id "
				"LEFT JOIN file_include ON "
				"ext_include.file_incl_id = file_include.file_include_id ");
	}
}


/**
 * Same behaviour as symbol_db_engine_get_class_parents () but this is quicker because
 * of the child_klass_symbol_id, aka the derived class symbol_id.
 * Return an iterator (eventually) containing the base classes or NULL if error occurred.
 */
SymbolDBEngineIterator *
symbol_db_engine_get_class_parents_by_symbol_id (SymbolDBEngine *dbe, 
												 gint child_klass_symbol_id,
												 SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	gchar *query_str;
	GdaDataModel *data;
	GdaHolder *param;
	GString *info_data;
	GString *join_data;
	GValue *ret_value;
	gboolean ret_bool;
	const DynChildQueryNode *dyn_node;
	
	g_return_val_if_fail (dbe != NULL, FALSE);
	priv = dbe->priv;

	SDB_LOCK(priv);

	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
					DYN_PREP_QUERY_GET_CLASS_PARENTS_BY_SYMBOL_ID, sym_info, 0)) == NULL)
	{
	
		/* info_data contains the stuff after SELECT and befor FROM */
		info_data = g_string_new ("");
	
		/* join_data contains the optionals joins to do to retrieve new data on other
	 	 * tables.
	 	 */
		join_data = g_string_new ("");

		/* fill info_data and join data with optional sql */
		sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
	
		query_str = g_strdup_printf("SELECT symbol.symbol_id AS symbol_id, "
				"symbol.name AS name, symbol.file_position AS file_position, "
				"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
		    	"symbol.returntype AS returntype "
				"%s FROM heritage "
				"JOIN symbol ON heritage.symbol_id_base = symbol.symbol_id %s "
				"WHERE heritage.symbol_id_derived = ## /* name:'childklassid' type:gint */", 
						info_data->str, join_data->str);
	
		dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_GET_CLASS_PARENTS_BY_SYMBOL_ID,
						sym_info, 0,
						query_str);
		
		g_free (query_str);
		g_string_free (info_data, TRUE);
		g_string_free (join_data, TRUE);
	}

	if (dyn_node == NULL) 
	{		
		SDB_UNLOCK(priv);
		return NULL;
	}

	/*DEBUG_PRINT ("symbol_db_engine_get_class_parents_by_symbol_id () query_str is: %s",
					dyn_node->query_str);*/
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "childklassid")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	MP_SET_HOLDER_BATCH_INT(priv, param, child_klass_symbol_id, ret_bool, ret_value);

	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);
		
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (data) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	SDB_UNLOCK(priv);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

/** 
 * Returns an iterator to the data retrieved from database. 
 * The iterator, if not null, will contain a list of parent classes for the given 
 * symbol name.
 * scope_path can be NULL.
 */
#define DYN_GET_CLASS_PARENTS_EXTRA_PAR_FINAL_DEF_ZERO		1
#define DYN_GET_CLASS_PARENTS_EXTRA_PAR_FINAL_DEF_POSITIVE	2

SymbolDBEngineIterator *
symbol_db_engine_get_class_parents (SymbolDBEngine *dbe, const gchar *klass_name, 
									 const GPtrArray *scope_path, SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	gchar *query_str;
	GdaDataModel *data;
	GdaHolder *param;
	GString *info_data;
	GString *join_data;
	gint final_definition_id;
	const DynChildQueryNode *dyn_node;
	GValue *ret_value;
	gboolean ret_bool;
	
	g_return_val_if_fail (dbe != NULL, FALSE);
	priv = dbe->priv;
	
	SDB_LOCK(priv);
	
	final_definition_id = -1;
	if (scope_path != NULL)	
		final_definition_id = sdb_engine_walk_down_scope_path (dbe, scope_path);

	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
					DYN_PREP_QUERY_GET_CLASS_PARENTS, sym_info, 
					final_definition_id > 0 ? 
					DYN_GET_CLASS_PARENTS_EXTRA_PAR_FINAL_DEF_POSITIVE :
					DYN_GET_CLASS_PARENTS_EXTRA_PAR_FINAL_DEF_ZERO)) == NULL)
	{
		
		/* info_data contains the stuff after SELECT and before FROM */
		info_data = g_string_new ("");
	
		/* join_data contains the optionals joins to do to retrieve new data on other
	 	 * tables.
	 	 */
		join_data = g_string_new ("");

		/* fill info_data and join data with optional sql */
		sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
	
		if (final_definition_id > 0)
		{		
			query_str = g_strdup_printf("SELECT symbol.symbol_id AS symbol_id, "
				"symbol.name AS name, symbol.file_position AS file_position, "
				"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
			    "symbol.returntype AS returntype "
				"%s FROM heritage "
				"JOIN symbol ON heritage.symbol_id_base = symbol.symbol_id %s "
				"WHERE symbol_id_derived = ("
					"SELECT symbol_id FROM symbol "
						"JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "
						"WHERE symbol.name = ## /* name:'klassname' type:gchararray */ "
							"AND sym_kind.kind_name = 'class' "
							"AND symbol.scope_id = ## /* name:'defid' type:gint */"
					")", info_data->str, join_data->str);
			
			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
							DYN_PREP_QUERY_GET_CLASS_PARENTS,
							sym_info, DYN_GET_CLASS_PARENTS_EXTRA_PAR_FINAL_DEF_POSITIVE,
							query_str);
		}
		else 
		{
			query_str = g_strdup_printf("SELECT symbol.symbol_id AS symbol_id, "
				"symbol.name AS name, symbol.file_position AS file_position, "
				"symbol.is_file_scope AS is_file_scope, "
				"symbol.signature AS signature, symbol.returntype AS returntype "
			    "%s FROM heritage "
				"JOIN symbol ON heritage.symbol_id_base = symbol.symbol_id %s "
				"WHERE symbol_id_derived = ("
					"SELECT symbol_id FROM symbol "
						"JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "
						"WHERE symbol.name = ## /* name:'klassname' type:gchararray */ "
							"AND sym_kind.kind_name = 'class' "
					")", info_data->str, join_data->str);
			
			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
							DYN_PREP_QUERY_GET_CLASS_PARENTS,
							sym_info, DYN_GET_CLASS_PARENTS_EXTRA_PAR_FINAL_DEF_ZERO,
							query_str);
		}	
		
		g_free (query_str);
		g_string_free (info_data, TRUE);
		g_string_free (join_data, TRUE);
	}

	
	if (dyn_node == NULL) 
	{		
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "klassname")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	MP_SET_HOLDER_BATCH_STR(priv, param, klass_name, ret_bool, ret_value);	
	
	if (final_definition_id > 0)
	{
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "defid")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}
		
		MP_SET_HOLDER_BATCH_INT(priv, param, final_definition_id, ret_bool, ret_value);
	}	
			
	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);
		
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (GDA_DATA_MODEL (data)) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}

	SDB_UNLOCK(priv);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);
}

/**
 * Personalized GTree mapping:
 * Considering that a gint on a x86 is 4 bytes: we'll reserve:
 * 3 bytes to map the main parameters.
 * 1 byte is for filter_kinds number, so you'll be able to filter up to 255 parameters.
 * |--------------------------------|-------------|
 *        main parameters [3 bytes]  extra [1 byte]
 */
#define DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_LIMIT					0x0100
#define DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_OFFSET				0x0200
#define DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_GROUP_YES				0x0400
#define DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_GROUP_NO				0x0800
#define DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_INCLUDE_KINDS_YES 	0x1000
#define DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_INCLUDE_KINDS_NO	 	0x2000

SymbolDBEngineIterator *
symbol_db_engine_get_global_members_filtered (SymbolDBEngine *dbe, 
									SymType filter_kinds,
									gboolean include_kinds, 
									gboolean group_them,
									gint results_limit, 
									gint results_offset,
								 	SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	GString *filter_str;
	gchar *query_str;
	const gchar *group_by_option;
	gchar *limit = "";
	gboolean limit_free = FALSE;
	gchar *offset = "";
	gboolean offset_free = FALSE;
	const DynChildQueryNode *dyn_node = NULL;
	GdaHolder *param;
	GValue *ret_value;
	gboolean ret_bool;
	GPtrArray *filter_kinds_array;

	/* use to merge multiple extra_parameters flags */
	gint other_parameters = 0;	
	
	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;
	
	SDB_LOCK(priv);
	
	/* check for an already flagged sym_info with KIND. SYMINFO_KIND on sym_info
	 * is already contained into the default query infos.
	 */
	sym_info = sym_info & ~SYMINFO_KIND;

	/* determine the filter_kinds thing */
	if (filter_kinds == SYMTYPE_UNDEF)
		filter_kinds_array = NULL;
	else
		filter_kinds_array = symbol_db_util_fill_type_array (filter_kinds);
	
	if (group_them == TRUE)
	{
		other_parameters |= DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_GROUP_YES;
		group_by_option = "GROUP BY symbol.name";
	}
	else 
	{
		other_parameters |= DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_GROUP_NO;
		group_by_option = "";
	}

	if (results_limit > 0)
	{
		other_parameters |= DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_LIMIT;
		limit_free = TRUE;
		limit = g_strdup_printf ("LIMIT ## /* name:'limit' type:gint */");
	}
	
	if (results_offset > 0)
	{
		other_parameters |= DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_OFFSET;
		offset = g_strdup_printf ("OFFSET ## /* name:'offset' type:gint */");
		offset_free = TRUE;
	}
	
	/* test if user gave an array with more than 255 filter_kinds. In that case
	 * we'll not be able to save/handle it, so consider it as a NULL array 
	 */
	if (filter_kinds_array == NULL || filter_kinds_array->len > 255 || filter_kinds_array->len <= 0) 
	{
		if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
					DYN_PREP_QUERY_GET_CLASS_PARENTS, sym_info, 
					other_parameters)) == NULL)
		{
			/* info_data contains the stuff after SELECT and befor FROM */
			info_data = g_string_new ("");
	
			/* join_data contains the optionals joins to do to retrieve new data on other
	 	 	 * tables.
	 	 	 */
			join_data = g_string_new ("");

			/* fill info_data and join data with optional sql */
			sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
			
			query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
				"symbol.name AS name, symbol.file_position AS file_position, "
				"symbol.is_file_scope AS is_file_scope, "
				"symbol.signature AS signature, symbol.returntype AS returntype, "
			    "sym_kind.kind_name AS kind_name %s FROM symbol "
					"JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id %s "
					"WHERE symbol.scope_id <= 0 AND symbol.is_file_scope = 0 "
							"%s %s %s", info_data->str, join_data->str,
						 	group_by_option, limit, offset);
			
			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
							DYN_PREP_QUERY_GET_GLOBAL_MEMBERS_FILTERED,
							sym_info, other_parameters,
							query_str);			
			
			g_free (query_str);
			g_string_free (join_data, TRUE);
			g_string_free (info_data, TRUE);
		}
	}
	else
	{
		if (include_kinds == TRUE)
		{
			other_parameters |= 
				DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_INCLUDE_KINDS_YES;
		}
		else
		{
			other_parameters |= 
				DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_INCLUDE_KINDS_NO;
		}
		
		/* set the number of parameters in the less important byte */
		other_parameters |= filter_kinds_array->len;
		
		if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
				DYN_PREP_QUERY_GET_CLASS_PARENTS, sym_info, 
				other_parameters)) == NULL)
		{		
			gint i;
			/* info_data contains the stuff after SELECT and befor FROM */
			info_data = g_string_new ("");
	
			/* join_data contains the optionals joins to do to retrieve new data on other
 	 	 	 * tables.
 	 	 	 */
			join_data = g_string_new ("");

			/* fill info_data and join data with optional sql */
			sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);				

			/* prepare the dynamic filter string before the final query */
			filter_str = g_string_new ("");
			
			if (include_kinds == TRUE)
			{				
				filter_str = g_string_append (filter_str , 
					"AND sym_kind.kind_name IN (## /* name:'filter0' type:gchararray */");
			}
			else
			{
				filter_str = g_string_append (filter_str , 
					"AND sym_kind.kind_name NOT IN (## /* name:'filter0' type:gchararray */");
			}
			
			for (i = 1; i < filter_kinds_array->len; i++)
			{				
				g_string_append_printf (filter_str , 
						",## /* name:'filter%d' type:gchararray */", i);
			}
			filter_str = g_string_append (filter_str , ")");
			
			query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
				"symbol.name AS name, symbol.file_position AS file_position, "
				"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
			    	"symbol.returntype AS returntype, "
					"sym_kind.kind_name AS kind_name %s FROM symbol "
					"%s JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "
					"WHERE symbol.scope_id <= 0 AND symbol.is_file_scope = 0 "
					"%s %s %s %s", info_data->str, join_data->str, 
							 filter_str->str, group_by_option, limit, offset);
		
			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_GET_GLOBAL_MEMBERS_FILTERED,
						sym_info, other_parameters,
						query_str);
				
			g_free (query_str);
			g_string_free (join_data, TRUE);
			g_string_free (info_data, TRUE);
			g_string_free (filter_str, TRUE);
		}
	}	
	
	if (limit_free)
		g_free (limit);
	
	if (offset_free)
		g_free (offset);
	
	if (dyn_node == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	if (other_parameters & DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_LIMIT)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "limit")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_limit, ret_bool, ret_value);
	}

	if (other_parameters & DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_OFFSET)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "offset")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_offset, ret_bool, ret_value);		
	}
	
	
	if (other_parameters & DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_INCLUDE_KINDS_YES ||
		other_parameters & DYN_GET_GLOBAL_MEMBERS_FILTERED_EXTRA_PAR_INCLUDE_KINDS_NO)
	{	
		gint i;
		for (i = 0; i < filter_kinds_array->len; i++)
		{
			gchar *curr_str = g_strdup_printf ("filter%d", i);
			param = gda_set_get_holder ((GdaSet*)dyn_node->plist, curr_str);

			MP_SET_HOLDER_BATCH_STR(priv, param, g_ptr_array_index (filter_kinds_array, i), 
									ret_bool, ret_value);
			g_free (curr_str);
		}
	}	

	/*DEBUG_PRINT ("symbol_db_engine_get_global_members_filtered  () query_str is %s",
				 dyn_node->query_str);*/


	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);

	/* free the filter kinds, if it's not null */
	if (filter_kinds_array)
	{
		g_ptr_array_foreach (filter_kinds_array, (GFunc)g_free, NULL);
		g_ptr_array_free (filter_kinds_array, TRUE);
	}
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (GDA_DATA_MODEL (data)) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}

	SDB_UNLOCK(priv);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

/**
 * Personalized GTree mapping:
 * Considering that a gint on a x86 is 4 bytes: we'll reserve:
 * 3 bytes to map the main parameters.
 * 1 byte is for filter_kinds number, so you'll be able to filter up to 255 parameters.
 * |--------------------------------|-------------|
 *        main parameters [3 bytes]  extra [1 byte] 
 */
#define DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_LIMIT					0x0100
#define DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_OFFSET				0x0200
#define DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_INCLUDE_KINDS_YES		0x0400
#define DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_INCLUDE_KINDS_NO		0x0800

SymbolDBEngineIterator *
symbol_db_engine_get_scope_members_by_symbol_id_filtered (SymbolDBEngine *dbe, 
									gint scope_parent_symbol_id, 
									SymType filter_kinds,
									gboolean include_kinds,
									gint results_limit,
									gint results_offset,
									SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	gchar *query_str;	
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	GString *filter_str;
	gchar *limit = "";
	gboolean limit_free = FALSE;
	gchar *offset = "";
	gboolean offset_free = FALSE;
	gint other_parameters;
	const DynChildQueryNode *dyn_node = NULL;
	GdaHolder *param;
	GValue *ret_value;
	gboolean ret_bool;
	GPtrArray *filter_kinds_array;
	
	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;

	SDB_LOCK(priv);
	
	if (scope_parent_symbol_id <= 0)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	/* syminfo kind is already included in results */
	sym_info = sym_info & ~SYMINFO_KIND;

	/* determine the filter_kinds thing */
	if (filter_kinds == SYMTYPE_UNDEF)
		filter_kinds_array = NULL;
	else
		filter_kinds_array = symbol_db_util_fill_type_array (filter_kinds);
	
	/* init parameters */
	other_parameters = 0;	

	if (results_limit > 0)
	{
		limit = g_strdup_printf ("LIMIT ## /* name:'limit' type:gint */");
		limit_free = TRUE;
		other_parameters |= 
			DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_LIMIT;
	}
	
	if (results_offset > 0)
	{
		offset = g_strdup_printf ("OFFSET ## /* name:'offset' type:gint */");
		offset_free = TRUE;
		other_parameters |=
			DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_OFFSET;
	}
		
	/* build filter string */
	if (include_kinds == TRUE)
	{
		other_parameters |= 
			DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_INCLUDE_KINDS_YES;
	}
	else
	{
		other_parameters |= 
			DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_INCLUDE_KINDS_NO;
	}
	
	/* we'll take into consideration the number of filter_kinds only it the number
	 * is fillable in a byte.
	 */
	if (filter_kinds_array != NULL && filter_kinds_array->len < 255 
		&& filter_kinds_array->len > 0)
	{		
		/* set the number of parameters in the less important byte */
		other_parameters |= filter_kinds_array->len;	
	}
	
	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
				DYN_PREP_QUERY_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED, sym_info, 
				other_parameters)) == NULL)
	{	
		gint i;		
		
		/* info_data contains the stuff after SELECT and befor FROM */
		info_data = g_string_new ("");
	
		/* join_data contains the optionals joins to do to retrieve new data on other
	 	 * tables.
	 	 */
		join_data = g_string_new ("");
			
		/* fill info_data and join data with optional sql */
		sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);

		filter_str = g_string_new ("");
		if (include_kinds == TRUE)
		{			
			filter_str = g_string_append (filter_str , 
				"AND sym_kind.kind_name IN (## /* name:'filter0' type:gchararray */");
		}
		else
		{
			filter_str = g_string_append (filter_str , 
				"AND sym_kind.kind_name NOT IN (## /* name:'filter0' type:gchararray */");
		}
		
		for (i = 1; i < filter_kinds_array->len; i++)
		{				
			g_string_append_printf (filter_str , 
					",## /* name:'filter%d' type:gchararray */", i);
		}
		filter_str = g_string_append (filter_str , ")");
		
		/* ok, beware that we use an 'alias hack' to accomplish compatibility with 
	 	 * sdb_engine_prepare_symbol_info_sql () function. In particular we called
	 	 * the first joining table 'a', the second one 'symbol', where there is the info we
	 	 * want 
	 	 */		
		query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
			"symbol.name AS name, "
			"symbol.file_position AS file_position, "
			"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
		    "symbol.returntype AS returntype, "
			"sym_kind.kind_name AS kind_name %s "
			"FROM symbol a, symbol symbol "
			"%s JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "
			"WHERE a.symbol_id = ## /* name:'scopeparentsymid' type:gint */ "
			"AND symbol.scope_id = a.scope_definition_id "
			"AND symbol.scope_id > 0 %s %s %s", info_data->str, join_data->str,
									 filter_str->str, limit, offset);		
									 
		dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
							DYN_PREP_QUERY_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED,
							sym_info, other_parameters,
							query_str);
			
		g_free (query_str);
		g_string_free (join_data, TRUE);
		g_string_free (info_data, TRUE);
		g_string_free (filter_str, TRUE);
	}	
	
	if (limit_free)
		g_free (limit);
	
	if (offset_free)
		g_free (offset);

	if (dyn_node == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}	
	
	if (other_parameters & DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_LIMIT)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "limit")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_limit, ret_bool, ret_value);
	}

	if (other_parameters & DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_OFFSET)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "offset")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_offset, ret_bool, ret_value);
	}	
	
	if (other_parameters & 
			DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_INCLUDE_KINDS_YES ||
		other_parameters & 
			DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_FILTERED_EXTRA_PAR_INCLUDE_KINDS_NO)
	{	
		gint i;
		for (i = 0; i < filter_kinds_array->len; i++)
		{
			gchar *curr_str = g_strdup_printf ("filter%d", i);
			param = gda_set_get_holder ((GdaSet*)dyn_node->plist, curr_str);
			
			MP_SET_HOLDER_BATCH_STR(priv, param, g_ptr_array_index (filter_kinds_array, i), 
									ret_bool, ret_value);
			g_free (curr_str);
		}
	}	

	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "scopeparentsymid")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}

	MP_SET_HOLDER_BATCH_INT(priv, param, scope_parent_symbol_id, ret_bool, ret_value);	


	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);

	/* free the filter kinds, if it's not null */
	if (filter_kinds_array)
	{
		g_ptr_array_foreach (filter_kinds_array, (GFunc)g_free, NULL);
		g_ptr_array_free (filter_kinds_array, TRUE);
	}
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (GDA_DATA_MODEL (data)) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}

	SDB_UNLOCK(priv);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

#define DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_EXTRA_PAR_LIMIT		1
#define DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_EXTRA_PAR_OFFSET		2

SymbolDBEngineIterator *
symbol_db_engine_get_scope_members_by_symbol_id (SymbolDBEngine *dbe, 
									gint scope_parent_symbol_id, 
									gint results_limit,
									gint results_offset,
									SymExtraInfo sym_info)
{
/*
select b.* from symbol a, symbol b where a.symbol_id = 348 and 
			b.scope_id = a.scope_definition_id;
*/
	SymbolDBEnginePriv *priv;
	gchar *query_str;	
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	gchar *limit = "";
	gboolean limit_free = FALSE;
	gchar *offset = "";
	gboolean offset_free = FALSE;
	gint other_parameters;
	const DynChildQueryNode *dyn_node = NULL;
	GdaHolder *param;
	GValue *ret_value;
	gboolean ret_bool;
	
	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;

	SDB_LOCK(priv);
	
	if (scope_parent_symbol_id <= 0)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}

	other_parameters = 0;
	
	if (results_limit > 0)
	{
		limit = g_strdup_printf ("LIMIT ## /* name:'limit' type:gint */");
		limit_free = TRUE;
		other_parameters |= DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_EXTRA_PAR_LIMIT;
	}
	
	if (results_offset > 0)
	{
		offset = g_strdup_printf ("OFFSET ## /* name:'offset' type:gint */");
		offset_free = TRUE;
		other_parameters |= DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_EXTRA_PAR_OFFSET;
	}
	
	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
				DYN_PREP_QUERY_GET_SCOPE_MEMBERS_BY_SYMBOL_ID, sym_info, 
				other_parameters)) == NULL)
	{
		/* info_data contains the stuff after SELECT and befor FROM */
		info_data = g_string_new ("");
		
		/* join_data contains the optionals joins to do to retrieve new data on other
	 	 * tables.
	 	 */
		join_data = g_string_new ("");

		/* fill info_data and join data with optional sql */
		sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);		
	
	
		/* ok, beware that we use an 'alias hack' to accomplish compatibility with 
	 	 * sdb_engine_prepare_symbol_info_sql () function. In particular we called
	 	 * the first joining table 'a', the second one 'symbol', where is the info we
	 	 * want 
	 	 */
		query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
			"symbol.name AS name, symbol.file_position AS file_position, "
			"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
		    "symbol.returntype AS returntype "
			"%s FROM symbol a, symbol symbol "
			"%s WHERE a.symbol_id = ## /* name:'scopeparentsymid' type:gint */ "
			"AND symbol.scope_id = a.scope_definition_id "
			"AND symbol.scope_id > 0 %s %s", info_data->str, join_data->str,
								 limit, offset);	
		
		dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
							DYN_PREP_QUERY_GET_SCOPE_MEMBERS_BY_SYMBOL_ID,
							sym_info, other_parameters,
							query_str);
		
		g_free (query_str);
		g_string_free (info_data, TRUE);
		g_string_free (join_data, TRUE);
	}
	
	if (limit_free)
		g_free (limit);
	
	if (offset_free)
		g_free (offset);

	if (dyn_node == NULL) 
	{		
		SDB_UNLOCK(priv);
		return NULL;
	}

	
	if (other_parameters & DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_EXTRA_PAR_LIMIT)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "limit")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_limit, ret_bool, ret_value);	
	}

	if (other_parameters & DYN_GET_SCOPE_MEMBERS_BY_SYMBOL_ID_EXTRA_PAR_OFFSET)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "offset")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_offset, ret_bool, ret_value);	
	}
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "scopeparentsymid")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}

	MP_SET_HOLDER_BATCH_INT(priv, param, scope_parent_symbol_id, ret_bool, ret_value);	
	
	/*DEBUG_PRINT ("symbol_db_engine_get_scope_members_by_symbol_id (): %s", 
				 dyn_node->query_str);*/
	
	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);
	
	/*gda_data_model_dump (data, stdout);*/
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (GDA_DATA_MODEL (data)) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}

	SDB_UNLOCK(priv);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

SymbolDBEngineIterator *
symbol_db_engine_get_scope_members_by_path (SymbolDBEngine *dbe, 
									const GPtrArray* scope_path, 
									SymExtraInfo sym_info)
{
/*
simple scope 
	
select * from symbol where scope_id = (
	select scope.scope_id from scope
		inner join sym_type on scope.type_id = sym_type.type_id 
		where sym_type.type = 'class' 
			and scope.scope_name = 'MyClass'
	);
	
select * from symbol where scope_id = (
	select scope.scope_id from scope 
		inner join sym_type on scope.type_id = sym_type.type_id 
		where sym_type.type = 'struct' 
			and scope.scope_name = '_faa_1');
	
	
es. scope_path = First, namespace, Second, namespace, NULL, 
	symbol_name = Second_1_class	
*/
	SymbolDBEnginePriv *priv;
	gchar *query_str;	
	GdaDataModel *data;
	gint final_definition_id;
	GString *info_data;
	GString *join_data;
	GdaHolder *param;
	GValue *ret_value;
	gboolean ret_bool;	
	const DynChildQueryNode *dyn_node;
	
	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;

	SDB_LOCK(priv);
	
	final_definition_id = sdb_engine_walk_down_scope_path (dbe, scope_path);
	
	if (final_definition_id <= 0) 
	{
		SDB_UNLOCK(priv);
		return NULL;
	}

	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
		DYN_PREP_QUERY_GET_SCOPE_MEMBERS, sym_info, 0)) == NULL)
	{
		/* info_data contains the stuff after SELECT and befor FROM */
		info_data = g_string_new ("");
	
		/* join_data contains the optionals joins to do to retrieve new data on other
	 	 * tables.
	 	 */
		join_data = g_string_new ("");

		/* fill info_data and join data with optional sql */
		sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
	
		query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
			"symbol.name AS name, symbol.file_position AS file_position, "
			"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
		    "symbol.returntype AS returntype "
			"%s FROM symbol "
			"%s WHERE scope_id = ## /* name:'defid' type:gint */", 
									 info_data->str, join_data->str);
		
		dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_GET_SCOPE_MEMBERS,
						sym_info, 0,
						query_str);
		
		g_free (query_str);
		g_string_free (info_data, TRUE);
		g_string_free (join_data, TRUE);		
	}

	if (dyn_node == NULL) 
	{		
		SDB_UNLOCK(priv);
		return NULL;
	}	
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "defid")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}

	MP_SET_HOLDER_BATCH_INT(priv, param, final_definition_id, ret_bool, ret_value);

	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (data) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}

	
	SDB_UNLOCK(priv);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

SymbolDBEngineIterator *
symbol_db_engine_get_current_scope (SymbolDBEngine *dbe, const gchar* full_local_file_path,
									gulong line, SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	gchar *query_str;
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	GdaHolder *param;
	const DynChildQueryNode *dyn_node;
	gchar *db_relative_file;
	GValue *ret_value;
	gboolean ret_bool;
	
	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;
	
	db_relative_file = symbol_db_util_get_file_db_path (dbe, full_local_file_path);
	if (db_relative_file == NULL)
		return NULL;
	
	/*DEBUG_PRINT ("db_relative_file  %s", db_relative_file);*/
	/*DEBUG_PRINT ("full_local_file_path %s", full_local_file_path);*/
	
	SDB_LOCK(priv);

	sym_info = sym_info & ~SYMINFO_FILE_PATH;
	
	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
		DYN_PREP_QUERY_GET_CURRENT_SCOPE, sym_info, 0)) == NULL)
	{
		/* info_data contains the stuff after SELECT and befor FROM */
		info_data = g_string_new ("");
	
		/* join_data contains the optionals joins to do to retrieve new data on other
	 	 * tables.
	 	 */
		join_data = g_string_new ("");

		/* fill info_data and join data with optional sql */
		sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
	
		query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
			"symbol.name AS name, symbol.file_position AS file_position, "
			"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
		    "symbol.returntype AS returntype "
			"%s FROM symbol "
				"JOIN file ON file_defined_id = file_id "
				"%s WHERE file.file_path = ## /* name:'filepath' type:gchararray */ "
					"AND symbol.file_position <= ## /* name:'linenum' type:gint */  "
					"ORDER BY symbol.file_position DESC LIMIT 1", 
									 info_data->str, join_data->str);
/*		DEBUG_PRINT ("symbol_db_engine_get_current_scope () %s", query_str);*/
		
		dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_GET_CURRENT_SCOPE,
						sym_info, 0,
						query_str);
		
		g_string_free (info_data, TRUE);
		g_string_free (join_data, TRUE);		
		g_free (query_str);
	}
	
	if (dyn_node == NULL) 
	{		
		SDB_UNLOCK(priv);
		g_free (db_relative_file);
		return NULL;
	}
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "linenum")) == NULL)
	{
		SDB_UNLOCK(priv);
		g_free (db_relative_file);
		return NULL;
	}
		
	MP_SET_HOLDER_BATCH_INT(priv, param, line, ret_bool, ret_value);	
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "filepath")) == NULL)
	{
		SDB_UNLOCK(priv);
		g_free (db_relative_file);
		return NULL;
	}

	MP_SET_HOLDER_BATCH_STR(priv, param, db_relative_file, ret_bool, ret_value);
	
	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (data) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		g_free (db_relative_file);
		return NULL;
	}
	
	SDB_UNLOCK(priv);
	
	g_free (db_relative_file);
	
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

SymbolDBEngineIterator *
symbol_db_engine_get_file_symbols (SymbolDBEngine *dbe, 
								   const gchar *file_path, 
								   SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	gchar *query_str;	
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	GdaHolder *param;
	const DynChildQueryNode *dyn_node;
	GValue *ret_value;
	gboolean ret_bool;
	
	g_return_val_if_fail (dbe != NULL, NULL);
	g_return_val_if_fail (file_path != NULL, NULL);
	priv = dbe->priv;	
	g_return_val_if_fail (priv->db_directory != NULL, NULL);

	SDB_LOCK(priv);
	
	/* check for an already flagged sym_info with FILE_PATH. SYMINFO_FILE_PATH on 
	 * sym_info is already contained into the default query infos.
	 */
	sym_info = sym_info & ~SYMINFO_FILE_PATH;
	

	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
		DYN_PREP_QUERY_GET_FILE_SYMBOLS, sym_info, 0)) == NULL)
	{
		/* info_data contains the stuff after SELECT and befor FROM */
		info_data = g_string_new ("");

		/* join_data contains the optionals joins to do to retrieve new data on other
	 	 * tables.
	 	 */
		join_data = g_string_new ("");

		/* fill info_data and join data with optional sql */
		sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);

		/* rember to do a file_path + strlen(priv->db_directory): a project relative 
	 	 * file path 
	 	 */
		query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
			"symbol.name AS name, symbol.file_position AS file_position, "
			"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
		    "symbol.returntype AS returntype "
			"%s FROM symbol "
				"JOIN file ON symbol.file_defined_id = file.file_id "
			"%s WHERE file.file_path = ## /* name:'filepath' type:gchararray */", 
						info_data->str, join_data->str);
	
		dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_GET_FILE_SYMBOLS,
						sym_info, 0,
						query_str);
		
		g_free (query_str);
		g_string_free (info_data, TRUE);
		g_string_free (join_data, TRUE);
	}

	if (dyn_node == NULL) 
	{		
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	/*DEBUG_PRINT ("query for symbol_db_engine_get_file_symbols is %s [filepath: %s]",
				 dyn_node->query_str, file_path);*/
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "filepath")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}
		
	gchar *relative_path = symbol_db_util_get_file_db_path (dbe, file_path);
	if (relative_path == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	MP_SET_HOLDER_BATCH_STR(priv, param, relative_path, ret_bool, ret_value);		
	
	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (data) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	g_free (relative_path);
	
	SDB_UNLOCK(priv);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

#define DYN_FIND_SYMBOL_NAME_BY_PATTERN_EXTRA_PAR_EXACT_MATCH_YES			0x010000
#define DYN_FIND_SYMBOL_NAME_BY_PATTERN_EXTRA_PAR_EXACT_MATCH_NO			0x020000

SymbolDBEngineIterator *
symbol_db_engine_find_symbol_by_name_pattern (SymbolDBEngine *dbe, 
									const gchar *pattern, gboolean case_sensitive,
									SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	gchar *query_str;	
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	GdaHolder *param;
	const DynChildQueryNode *dyn_node;
	GValue *ret_value;
	gboolean ret_bool;	
	const gchar *match_str;
	gint other_parameters;
	
	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;

	/* the function uses a mutex lock, so call it before locking this one */
	if (case_sensitive == FALSE)
		symbol_db_engine_set_db_case_sensitive (dbe, FALSE);
	
	SDB_LOCK(priv);

	other_parameters = 0;
	
	/* check for match */
	if (g_strrstr (pattern, "%") == NULL)
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_NAME_BY_PATTERN_EXTRA_PAR_EXACT_MATCH_YES;
		match_str = " = ## /* name:'pattern' type:gchararray */";
	}
	else
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_NAME_BY_PATTERN_EXTRA_PAR_EXACT_MATCH_NO;
		match_str = " LIKE ## /* name:'pattern' type:gchararray */";
	}
	
	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
		DYN_PREP_QUERY_FIND_SYMBOL_NAME_BY_PATTERN, sym_info, other_parameters)) == NULL)
	{
		/* info_data contains the stuff after SELECT and befor FROM */
		info_data = g_string_new ("");
	
		/* join_data contains the optionals joins to do to retrieve new data on other
		 * tables.
		 */
		join_data = g_string_new ("");

		/* fill info_data and join data with optional sql */
		sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
	
		query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
			"symbol.name AS name, symbol.file_position AS file_position, "
			"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
		    "symbol.returntype AS returntype "
			"%s FROM symbol %s "
			"WHERE symbol.name %s", info_data->str, join_data->str, match_str);
		
		dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_FIND_SYMBOL_NAME_BY_PATTERN,
						sym_info, other_parameters,
						query_str);		
		/* DEBUG_PRINT ("query %s", query_str);*/
		g_free (query_str);
		g_string_free (info_data, TRUE);
		g_string_free (join_data, TRUE);
	}

	if (dyn_node == NULL) 
	{		
		SDB_UNLOCK(priv);
		return NULL;
	}

	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "pattern")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}

	MP_SET_HOLDER_BATCH_STR(priv, param, pattern, ret_bool, ret_value);		
	
	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (data) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	SDB_UNLOCK(priv);
	
	/* re enable the sensitiveness on the db */
	if (case_sensitive == FALSE)
		symbol_db_engine_set_db_case_sensitive (dbe, TRUE);
	
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

gint
symbol_db_engine_get_parent_scope_id_by_symbol_id (SymbolDBEngine *dbe,
									gint scoped_symbol_id,
									const gchar * db_file)
{
/*
select * from symbol where scope_definition_id = (
	select scope_id from symbol where symbol_id = 26
	);
*/
	SymbolDBEnginePriv *priv;
	GdaDataModel *data;
	const GValue* value;
	GdaHolder *param;
	gint res;
	gint num_rows;
	const GdaSet *plist;
	const GdaStatement *stmt, *stmt2;	
	GValue *ret_value;
	gboolean ret_bool;
	
	
	g_return_val_if_fail (dbe != NULL, -1);
	priv = dbe->priv;

	SDB_LOCK(priv);

	if (db_file == NULL)
	{
		if ((stmt = sdb_engine_get_statement_by_query_id (dbe, 
						PREP_QUERY_GET_PARENT_SCOPE_ID_BY_SYMBOL_ID_NO_FILE))
			== NULL)
		{
			SDB_UNLOCK(priv);
			return -1;
		}

		plist = sdb_engine_get_query_parameters_list (dbe, 
						PREP_QUERY_GET_PARENT_SCOPE_ID_BY_SYMBOL_ID_NO_FILE);		
	}
	else 
	{
		if ((stmt = sdb_engine_get_statement_by_query_id (dbe, 
						PREP_QUERY_GET_PARENT_SCOPE_ID_BY_SYMBOL_ID))
			== NULL)
		{
			SDB_UNLOCK(priv);
			return -1;
		}

		plist = sdb_engine_get_query_parameters_list (dbe, 
						PREP_QUERY_GET_PARENT_SCOPE_ID_BY_SYMBOL_ID);
		
		/* dbfile parameter */
		if ((param = gda_set_get_holder ((GdaSet*)plist, "dbfile")) == NULL)
		{			
			g_warning ("param dbfile is NULL from pquery!");
			SDB_UNLOCK(priv);
			return -1;
		}
		
		MP_SET_HOLDER_BATCH_STR(priv, param, db_file, ret_bool, ret_value);
	}

	/* scoped symbol id */
	if ((param = gda_set_get_holder ((GdaSet*)plist, "symid")) == NULL)
	{			
		g_warning ("param symid is NULL from pquery!");
		SDB_UNLOCK(priv);
		return -1;
	}	
	
	MP_SET_HOLDER_BATCH_INT(priv, param, scoped_symbol_id, ret_bool, ret_value);		
	
	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
													  (GdaStatement*)stmt, 
													  (GdaSet*)plist, NULL);
		
	if (!GDA_IS_DATA_MODEL (data) ||
		(num_rows = gda_data_model_get_n_rows (GDA_DATA_MODEL (data))) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		SDB_UNLOCK(priv);
		return -1;
	}

	/* Hey, we can go in trouble here. Imagine this situation:
	 * We have two namespaces, 'Editor' and 'Entwickler'. Inside
	 * them there are two classes with the same name, 'Preferences', hence 
	 * the same scope. We still aren't able to retrieve the real parent_symbol_id
	 * of a method into these classes.
	 *
	 * This is how the above query will result:
	 * 
	 * 324|31|Preferences|28|0||192|92|276|3|-1|-1|0
	 * 365|36|Preferences|33|0||192|2|276|3|-1|-1|0
	 *
	 * Say that 92 is 'Editor' scope, and '2' is 'Entwickler' one. We're looking 
	 * for the latter, and so we know that the right answer is symbol_id 365.
	 * 1. We'll store the symbold_id(s) and the scope_id(s) of both 324 and 365.
	 * 2. We'll query the db to retrieve all the symbols written before our 
	 *    scoped_symbol_id in the file where scoped_symbol_id is.
	 * 3. Compare every symbol's scope_definition_id retrieved on step 2 against
	 *    all the ones saved at point 1. The first match is our parent_scope symbol_id.
	 */
	if (num_rows > 1) 
	{
		gint i;
		
		GList *candidate_parents = NULL;
		typedef struct _candidate_node {
			gint symbol_id;
			gint scope_id;
		} candidate_node;
			
		GdaDataModel *detailed_data;
		
		/* step 1 */
		for (i=0; i < num_rows; i++)
		{
			candidate_node *node;
			const GValue *tmp_value, *tmp_value1;
			
			node = g_new0 (candidate_node, 1);
			
			/* get the symbol_id from the former 'data' result set */
			tmp_value = gda_data_model_get_value_at (data, 0, i, NULL);
			node->symbol_id = tmp_value != NULL && G_VALUE_HOLDS_INT (tmp_value)
				? g_value_get_int (tmp_value) : -1;
			
			/* get scope_id [or the definition] */
			/* handles this case:
			 * 
			 * namespace_foo [scope_id 0 or > 0, scope_def X]
			 * |
			 * +--- class_foo 
			 *      |
			 *      +---- scoped_symbol_id
  		     *
			 */
			tmp_value1 = gda_data_model_get_value_at (data, 4, i, NULL);
			node->scope_id = tmp_value1 != NULL && G_VALUE_HOLDS_INT (tmp_value1)
				? g_value_get_int (tmp_value1) : -1;

			/* handles this case:
			 * 
			 * namespace_foo [scope_id 0, scope_def X]
			 * |
			 * +--- scoped_symbol_id
			 *
			 */
			if (node->scope_id <= 0)
			{
				tmp_value1 = gda_data_model_get_value_at (data, 3, i, NULL);
				node->scope_id = tmp_value1 != NULL && G_VALUE_HOLDS_INT (tmp_value1)
					? g_value_get_int (tmp_value1) : -1;
			}
						
			candidate_parents = g_list_prepend (candidate_parents, node);
		}

		
		/* step 2 */
		if ((stmt2 = sdb_engine_get_statement_by_query_id (dbe, 
					PREP_QUERY_GET_PARENT_SCOPE_ID_BY_SYMBOL_ID_BY_SYMBOL_ID)) == NULL)
		{
			SDB_UNLOCK(priv);
			return -1;
		}

		plist = sdb_engine_get_query_parameters_list (dbe, 
						PREP_QUERY_GET_PARENT_SCOPE_ID_BY_SYMBOL_ID_BY_SYMBOL_ID);		

		/* scoped symbol id */
		if ((param = gda_set_get_holder ((GdaSet*)plist, "scopedsymid")) == NULL)
		{			
			g_warning ("param scopedsymid is NULL from pquery!");
			SDB_UNLOCK(priv);
			return -1;
		}	
		
		MP_SET_HOLDER_BATCH_INT(priv, param, scoped_symbol_id, ret_bool, ret_value);		
	
		/* we should receive just ONE row. If it's > 1 than we have surely an
		 * error on code.
		 */
		detailed_data = gda_connection_statement_execute_select (priv->db_connection, 
													  (GdaStatement*)stmt2, 
													  (GdaSet*)plist, NULL);
				
		if (!GDA_IS_DATA_MODEL (detailed_data) ||
			(num_rows = gda_data_model_get_n_rows (GDA_DATA_MODEL (detailed_data))) <= 0)
		{
			if (detailed_data != NULL)
				g_object_unref (detailed_data);
			SDB_UNLOCK(priv);
			res = -1;
		}	
		else		/* ok we have a good result here */
		{
			gint j;
			gboolean found = FALSE;
			gint parent_scope_definition_id;
			gint found_symbol_id = -1;
			candidate_node *node = NULL;
			
			for (j=0; j < gda_data_model_get_n_rows (detailed_data); j++)
			{
				const GValue *tmp_value;				
				gint k;

				tmp_value = gda_data_model_get_value_at (detailed_data, 0, j, NULL);
				parent_scope_definition_id = tmp_value != NULL && G_VALUE_HOLDS_INT (tmp_value)
					? g_value_get_int (tmp_value) : -1;
				
				for (k = 0; k < g_list_length (candidate_parents); k++) 
				{
					node = g_list_nth_data (candidate_parents, k); 
					/*DEBUG_PRINT ("node->symbol_id %d node->scope_id %d", 
								 node->symbol_id, node->scope_id);*/
					if (node->scope_id == parent_scope_definition_id) 
					{
						found = TRUE;
						found_symbol_id = node->symbol_id;
						break;
					}						
				}
				
				if (found)
					break;
			}
			res = found_symbol_id;
		}
		
		if (detailed_data)
			g_object_unref (detailed_data);

		for (i=0; i < g_list_length (candidate_parents); i++) 
		{
			g_free (g_list_nth_data (candidate_parents, i));
		}
		g_list_free (candidate_parents);
	}
	else
	{
		value = gda_data_model_get_value_at (data, 0, 0, NULL);
		res = value != NULL && G_VALUE_HOLDS_INT (value)
			? g_value_get_int (value) : -1;		
	}
	g_object_unref (data);
	
	SDB_UNLOCK(priv);
	return res;
}

SymbolDBEngineIterator *
symbol_db_engine_get_parent_scope_by_symbol_id (SymbolDBEngine *dbe, 
									gint scoped_symbol_id,
									const gchar* db_file,
    								SymExtraInfo sym_info)
{
	/* no need to lock */
	gint parent_sym_id = symbol_db_engine_get_parent_scope_id_by_symbol_id (dbe,
	    								scoped_symbol_id, db_file);

	if (parent_sym_id < 0)
		return NULL;
	
	/* get the info now */
	return symbol_db_engine_get_symbol_info_by_id (dbe, parent_sym_id, sym_info);
}

static GdaDataModel *
sdb_engine_get_symbol_info_by_id_1 (SymbolDBEngine *dbe, 
									gint sym_id, SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	gchar *query_str;	
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	GdaHolder *param;
	const DynChildQueryNode *dyn_node;
	GValue *ret_value;
	gboolean ret_bool;
	
		
	priv = dbe->priv;

	SDB_LOCK(priv);

	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
		DYN_PREP_QUERY_GET_SYMBOL_INFO_BY_ID, sym_info, 0)) == NULL)
	{
		/* info_data contains the stuff after SELECT and befor FROM */
		info_data = g_string_new ("");
	
		/* join_data contains the optionals joins to do to retrieve new data on other
	 	 * tables.
	 	 */
		join_data = g_string_new ("");

		/* fill info_data and join data with optional sql */
		sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
	
		query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
			"symbol.name AS name, symbol.file_position AS file_position, "
			"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
		    "symbol.returntype AS returntype "
			"%s FROM symbol "
			"%s WHERE symbol.symbol_id = ## /* name:'symid' type:gint */", 
									info_data->str, join_data->str);

		dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_GET_SYMBOL_INFO_BY_ID,
						sym_info, 0,
						query_str);
		
		g_free (query_str);
		g_string_free (info_data, TRUE);
		g_string_free (join_data, TRUE);	
	}
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "symid")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	MP_SET_HOLDER_BATCH_INT(priv, param, sym_id, ret_bool, ret_value);		
	
	/* execute the query with parameters just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (data) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	SDB_UNLOCK(priv);
	return data;
}

SymbolDBEngineIterator *
symbol_db_engine_get_symbol_info_by_id (SymbolDBEngine *dbe, 
									gint sym_id, SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	GdaDataModel *data;

	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;
	
	data = sdb_engine_get_symbol_info_by_id_1 (dbe, sym_id, sym_info);
	return data == NULL ? NULL : 
		(SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

SymbolDBEngineIterator *
symbol_db_engine_get_scope_chain (SymbolDBEngine *dbe,
    								gint scoped_symbol_id,
    								const gchar* db_file,
    								SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	GdaDataModel *tmp_res_data = NULL;	
	GdaDataModel *final_data;
	gint parent_sym_id;
	gint i;
	
	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;

	/* use an hacked GdaDataModel that can receive multiple datamodels in sequence. 
	 * This is necessary because unfortunately there's no way to retrieve the 
	 * scopes in one go: the returned query would always order them by id and there's no way
	 * to keep the results unsorted.
	 */	 
	final_data = gda_data_model_concat_new ();
	
	tmp_res_data = sdb_engine_get_symbol_info_by_id_1 (dbe, scoped_symbol_id, sym_info);
	gda_data_model_concat_append_model (GDA_DATA_MODEL_CONCAT (final_data), 
	    tmp_res_data);	
	g_object_unref (tmp_res_data);
	
	/* no need to get a lock */
	/* first item is considered separately */
	parent_sym_id = symbol_db_engine_get_parent_scope_id_by_symbol_id (dbe, 
	    					scoped_symbol_id, db_file);
	
	if (parent_sym_id <= 0)
	{
		return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (final_data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);
	}

	tmp_res_data = sdb_engine_get_symbol_info_by_id_1 (dbe, parent_sym_id, sym_info);

	/* did something go wrong? */
	if (!GDA_IS_DATA_MODEL (tmp_res_data) ||
		gda_data_model_get_n_rows (GDA_DATA_MODEL (tmp_res_data)) <= 0	)
	{
		return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (final_data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);
	}

	gda_data_model_concat_append_model (GDA_DATA_MODEL_CONCAT (final_data), 
	    tmp_res_data);

	/* a ref will be taken into the GdaDataModelConcat object so it's safe to 
	 * unref here 
	 */
	g_object_unref (tmp_res_data);

	/* we're ready for the big loop. The 100 is an infinite-loop protection. */
	for (i = 0; i < 100; i++) 
	{
		/* get the parent scope */
		parent_sym_id = symbol_db_engine_get_parent_scope_id_by_symbol_id (dbe, 
	    					parent_sym_id, db_file);
		/* DEBUG_PRINT ("got parent symbol_id  %d", parent_sym_id); */
		if (parent_sym_id <= 0)
		{
			break;
		}
		else 
		{
			tmp_res_data = sdb_engine_get_symbol_info_by_id_1 (dbe, parent_sym_id, 
			    sym_info);

			/* merge into the result data */
			gda_data_model_concat_append_model (GDA_DATA_MODEL_CONCAT (final_data), 
			    tmp_res_data);
			g_object_unref (tmp_res_data);
		}
	}
	
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (final_data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);
}

SymbolDBEngineIterator *
symbol_db_engine_get_scope_chain_by_file_line (SymbolDBEngine *dbe,
									const gchar* full_local_file_path, 
    								gulong line,
    								SymExtraInfo sym_info)
{
	SymbolDBEngineIterator *iter, *res_iter;	
	SymbolDBEngineIteratorNode *node;	
	gint symbol_id;
	
	g_return_val_if_fail (dbe != NULL, NULL);
	g_return_val_if_fail (full_local_file_path != NULL, NULL);

	/* passing SYMINFO_SIMPLE is ok because we don't need extra info at this step */
	iter = symbol_db_engine_get_current_scope (dbe,	
	    full_local_file_path, line, SYMINFO_SIMPLE);

	if (iter == NULL)
	{
		return NULL;
	}

	node = SYMBOL_DB_ENGINE_ITERATOR_NODE (iter);
	symbol_id = symbol_db_engine_iterator_node_get_symbol_id (node);

	/* note the NULL: we don't want to limit the search to that db file.
	 * It must be project-wide or some scopes may be left out.
	 */
	res_iter = symbol_db_engine_get_scope_chain (dbe, symbol_id, NULL, sym_info);	   
	
	return res_iter;
}


#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_INCLUDE_KINDS_YES		0x010000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_INCLUDE_KINDS_NO			0x020000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_LIMIT					0x040000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_OFFSET					0x080000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_EXACT_MATCH_YES			0x100000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_EXACT_MATCH_NO			0x200000

SymbolDBEngineIterator *
symbol_db_engine_find_symbol_by_name_pattern_on_file (SymbolDBEngine *dbe,
									const gchar *pattern,
									const gchar *full_local_file_path,
									SymType filter_kinds,
									gboolean include_kinds,
									gint results_limit,
									gint results_offset,
									SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	GString *filter_str;
	gchar *query_str;
	const gchar *match_str;
	GdaHolder *param;
	const DynChildQueryNode *dyn_node;
	gint other_parameters;
	gchar *limit = "";
	gboolean limit_free = FALSE;
	gchar *offset = "";
	gboolean offset_free = FALSE;
	GValue *ret_value;
	gboolean ret_bool;	
	gchar *db_rel_path;
	GPtrArray *filter_kinds_array;

	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;

	SDB_LOCK(priv);
	
	/* remove kind and path. They're already provided */
	sym_info = sym_info & ~SYMINFO_KIND;
	sym_info = sym_info & ~SYMINFO_FILE_PATH;
	
	/* initialize dynamic stuff */
	other_parameters = 0;
	dyn_node = NULL;

	/* determine the filter_kinds thing */
	if (filter_kinds == SYMTYPE_UNDEF)
		filter_kinds_array = NULL;
	else
		filter_kinds_array = symbol_db_util_fill_type_array (filter_kinds);
	
	/* check for match */
	if (g_strrstr (pattern, "%") == NULL)
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_EXACT_MATCH_YES;
		match_str = " = ## /* name:'pattern' type:gchararray */";
	}
	else
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_EXACT_MATCH_NO;
		match_str = " LIKE ## /* name:'pattern' type:gchararray */";
	}	
	
	if (results_limit > 0)
	{
		other_parameters |= DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_LIMIT;
		limit_free = TRUE;
		limit = g_strdup_printf ("LIMIT ## /* name:'limit' type:gint */");
	}
	
	if (results_offset > 0)
	{
		other_parameters |= DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_OFFSET;
		offset_free = TRUE;
		offset = g_strdup_printf ("OFFSET ## /* name:'offset' type:gint */");		
	}
	
	if (filter_kinds_array == NULL || filter_kinds_array->len > 255 || 
	    filter_kinds_array->len <= 0) 
	{
		if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
			DYN_PREP_QUERY_FIND_SYMBOL_BY_NAME_PATTERN_FILE, sym_info, 
														 other_parameters)) == NULL)
		{
			/* info_data contains the stuff after SELECT and befor FROM */
			info_data = g_string_new ("");
	
			/* join_data contains the optionals joins to do to retrieve new data on other
			 * tables.
		 	 */
			join_data = g_string_new ("");
		
			/* fill info_data and join data with optional sql */
			sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
			
			query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
					"symbol.name AS name, symbol.file_position AS file_position, "
					"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
			    	"symbol.returntype AS returntype, "
					"sym_kind.kind_name AS kind_name "
					"%s FROM symbol %s JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "
					"WHERE symbol.name %s AND symbol.file_defined_id IN "
					"(SELECT file_id FROM file WHERE file_path = ## /* name:'fpath' type:gchararray */) %s %s", 
				info_data->str, join_data->str, match_str, limit, offset);			

			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_FIND_SYMBOL_BY_NAME_PATTERN_FILE,
						sym_info, other_parameters,
						query_str);

			g_free (query_str);
			g_string_free (info_data, TRUE);
			g_string_free (join_data, TRUE);
		}
	}
	else
	{		
		if (include_kinds == TRUE)
		{
			other_parameters |= 
				DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_INCLUDE_KINDS_YES;
		}
		else
		{
			other_parameters |= 
				DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_INCLUDE_KINDS_NO;
		}

		/* set the number of parameters in the less important byte */
		other_parameters |= filter_kinds_array->len;
		
		if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
				DYN_PREP_QUERY_FIND_SYMBOL_BY_NAME_PATTERN_FILE, sym_info, 
				other_parameters)) == NULL)
		{					
			gint i;
			
			/* info_data contains the stuff after SELECT and before FROM */
			info_data = g_string_new ("");

			/* join_data contains the optionals joins to do to retrieve new 
			 * data on other tables.
		 	 */
			join_data = g_string_new ("");
	
			/* fill info_data and join data with optional sql */
			sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
			/* prepare the dynamic filter string before the final query */
			filter_str = g_string_new ("");
		
			if (include_kinds == TRUE)
			{				
				filter_str = g_string_append (filter_str , 
					"AND sym_kind.kind_name IN (## /* name:'filter0' type:gchararray */");		
				for (i = 1; i < filter_kinds_array->len; i++)
				{				
					g_string_append_printf (filter_str , 
							",## /* name:'filter%d' type:gchararray */", i);
				}
				filter_str = g_string_append (filter_str , ")");				
			}
			else
			{
				filter_str = g_string_append (filter_str , 
					"AND sym_kind.kind_name NOT IN (## /* name:'filter0' type:gchararray */");
				for (i = 1; i < filter_kinds_array->len; i++)
				{				
					g_string_append_printf (filter_str , 
							",## /* name:'filter%d' type:gchararray */", i);
				}
				filter_str = g_string_append (filter_str , ")");				
			}			
			
			query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, symbol.name AS name, "
				"symbol.file_position AS file_position, "
				"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
			    "symbol.returntype AS returntype, "
				"sym_kind.kind_name AS kind_name "
					"%s FROM symbol %s JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "					
					"WHERE symbol.name %s AND symbol.file_defined_id IN "
					"(SELECT file_id FROM file WHERE file_path = ## /* name:'fpath' type:gchararray */) "
					"%s GROUP BY symbol.name %s %s", 
			 		info_data->str, join_data->str, match_str, 
			 		filter_str->str, limit, offset);
			
			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_FIND_SYMBOL_BY_NAME_PATTERN_FILE,
						sym_info, other_parameters,
						query_str);
			g_free (query_str);
			g_string_free (info_data, TRUE);
			g_string_free (join_data, TRUE);
			g_string_free (filter_str, TRUE);
		}
	}	
	
	if (limit_free)
		g_free (limit);
	
	if (offset_free)
		g_free (offset);

	if (dyn_node == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	if (other_parameters & DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_LIMIT)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "limit")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_limit, ret_bool, ret_value);		
	}

	if (other_parameters & DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_OFFSET)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "offset")) == NULL)
		{
			SDB_UNLOCK(priv);			
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_offset, ret_bool, ret_value);		
	}
	
	/* fill parameters for filter kinds */
	if (other_parameters & DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_INCLUDE_KINDS_YES ||
		other_parameters & DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILE_EXTRA_PAR_INCLUDE_KINDS_NO)
	{	
		gint i;
		for (i = 0; i < filter_kinds_array->len; i++)
		{
			gchar *curr_str = g_strdup_printf ("filter%d", i);
			param = gda_set_get_holder ((GdaSet*)dyn_node->plist, curr_str);
			MP_SET_HOLDER_BATCH_STR(priv, param, g_ptr_array_index (filter_kinds_array, i), 
			    ret_bool, ret_value);		
			g_free (curr_str);
		}
	}
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "pattern")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}

	MP_SET_HOLDER_BATCH_STR(priv, param, pattern, ret_bool, ret_value);

	/* get the db-relative file path */
	db_rel_path = symbol_db_util_get_file_db_path (dbe, full_local_file_path);
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "fpath")) == NULL)
	{
		SDB_UNLOCK(priv);
		g_free (db_rel_path);
		return NULL;
	}

	MP_SET_HOLDER_BATCH_STR(priv, param, db_rel_path, ret_bool, ret_value);
	
	/*DEBUG_PRINT ("query: %s", dyn_node->query_str);*/

	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);

	/* free the filter kinds, if it's not null */
	if (filter_kinds_array)
	{
		g_ptr_array_foreach (filter_kinds_array, (GFunc)g_free, NULL);
		g_ptr_array_free (filter_kinds_array, TRUE);
	}
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (GDA_DATA_MODEL (data)) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		g_free (db_rel_path);
		return NULL;
	}

	SDB_UNLOCK(priv);
	
	g_free (db_rel_path);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);
}

#define DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_INCLUDE_KINDS_YES		0x010000
#define DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_INCLUDE_KINDS_NO			0x020000
#define DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_LIMIT					0x040000
#define DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_OFFSET					0x080000
#define DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_EXACT_MATCH_YES			0x100000
#define DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_EXACT_MATCH_NO			0x200000

SymbolDBEngineIterator *
symbol_db_engine_find_symbol_in_scope (SymbolDBEngine *dbe,
    								const gchar *pattern,
    								gint container_symbol_id,
									SymType filter_kinds,
									gboolean include_kinds,
									gint results_limit,
									gint results_offset,    
    								SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	GString *filter_str;
	gchar *query_str;
	const gchar *match_str;
	GdaHolder *param;
	const DynChildQueryNode *dyn_node;
	gint other_parameters;
	gchar *limit = "";
	gboolean limit_free = FALSE;
	gchar *offset = "";
	gboolean offset_free = FALSE;
	GValue *ret_value;
	gboolean ret_bool;	
	GPtrArray *filter_kinds_array;

	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;

	SDB_LOCK(priv);
	
	/* remove kind and path. They're already provided */
	sym_info = sym_info & ~SYMINFO_KIND;	
	
	/* initialize dynamic stuff */
	other_parameters = 0;
	dyn_node = NULL;

	/* determine the filter_kinds thing */
	if (filter_kinds == SYMTYPE_UNDEF)
		filter_kinds_array = NULL;
	else
		filter_kinds_array = symbol_db_util_fill_type_array (filter_kinds);
	
	/* check for match */
	if (g_strrstr (pattern, "%") == NULL)
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_EXACT_MATCH_YES;
		match_str = " = ## /* name:'pattern' type:gchararray */";
	}
	else
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_EXACT_MATCH_NO;
		match_str = " LIKE ## /* name:'pattern' type:gchararray */";
	}	
	
	if (results_limit > 0)
	{
		other_parameters |= DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_LIMIT;
		limit_free = TRUE;
		limit = g_strdup_printf ("LIMIT ## /* name:'limit' type:gint */");
	}
	
	if (results_offset > 0)
	{
		other_parameters |= DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_OFFSET;
		offset_free = TRUE;
		offset = g_strdup_printf ("OFFSET ## /* name:'offset' type:gint */");		
	}
	
	if (filter_kinds_array == NULL || filter_kinds_array->len > 255 || 
	    filter_kinds_array->len <= 0) 
	{
		if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
			DYN_PREP_QUERY_FIND_SYMBOL_IN_SCOPE, sym_info, 
			other_parameters)) == NULL)
		{
			/* info_data contains the stuff after SELECT and befor FROM */
			info_data = g_string_new ("");
	
			/* join_data contains the optionals joins to do to retrieve new data on other
			 * tables.
		 	 */
			join_data = g_string_new ("");
		
			/* fill info_data and join data with optional sql */
			sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
			
			query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
					"symbol.name AS name, symbol.file_position AS file_position, "
					"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
			    	"symbol.returntype AS returntype, "
					"sym_kind.kind_name AS kind_name "
					"%s FROM symbol %s JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "
					"WHERE symbol.name %s AND symbol.scope_id = (SELECT scope_definition_id "
			    	"FROM symbol WHERE symbol_id = ## /* name:'givensymid' type:gint */) "
			    	"%s %s", 
				info_data->str, join_data->str, match_str, limit, offset);			

			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_FIND_SYMBOL_IN_SCOPE,
						sym_info, other_parameters,
						query_str);

			g_free (query_str);
			g_string_free (info_data, TRUE);
			g_string_free (join_data, TRUE);
		}
	}
	else
	{		
		if (include_kinds == TRUE)
		{
			other_parameters |= 
				DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_INCLUDE_KINDS_YES;
		}
		else
		{
			other_parameters |= 
				DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_INCLUDE_KINDS_NO;
		}

		/* set the number of parameters in the less important byte */
		other_parameters |= filter_kinds_array->len;
		
		if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
				DYN_PREP_QUERY_FIND_SYMBOL_IN_SCOPE, sym_info, 
				other_parameters)) == NULL)
		{					
			gint i;
			
			/* info_data contains the stuff after SELECT and before FROM */
			info_data = g_string_new ("");

			/* join_data contains the optionals joins to do to retrieve new 
			 * data on other tables.
		 	 */
			join_data = g_string_new ("");
	
			/* fill info_data and join data with optional sql */
			sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
			/* prepare the dynamic filter string before the final query */
			filter_str = g_string_new ("");
		
			if (include_kinds == TRUE)
			{				
				filter_str = g_string_append (filter_str , 
					"AND sym_kind.kind_name IN (## /* name:'filter0' type:gchararray */");		
				for (i = 1; i < filter_kinds_array->len; i++)
				{				
					g_string_append_printf (filter_str , 
							",## /* name:'filter%d' type:gchararray */", i);
				}
				filter_str = g_string_append (filter_str , ")");				
			}
			else
			{
				filter_str = g_string_append (filter_str , 
					"AND sym_kind.kind_name NOT IN (## /* name:'filter0' type:gchararray */");
				for (i = 1; i < filter_kinds_array->len; i++)
				{				
					g_string_append_printf (filter_str , 
							",## /* name:'filter%d' type:gchararray */", i);
				}
				filter_str = g_string_append (filter_str , ")");				
			}			
			
			query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, symbol.name AS name, "
				"symbol.file_position AS file_position, "
				"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
			    "symbol.returntype AS returntype, "
				"sym_kind.kind_name AS kind_name "
					"%s FROM symbol %s JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "					
					"WHERE symbol.name %s AND symbol.scope_id = (SELECT scope_definition_id "
			    	"FROM symbol WHERE symbol_id = ## /* name:'givensymid' type:gint */) "
					"%s GROUP BY symbol.name %s %s", 
			 		info_data->str, join_data->str, match_str, 
			 		filter_str->str, limit, offset);
			
			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_FIND_SYMBOL_IN_SCOPE,
						sym_info, other_parameters,
						query_str);
			g_free (query_str);
			g_string_free (info_data, TRUE);
			g_string_free (join_data, TRUE);
			g_string_free (filter_str, TRUE);
		}
	}	
	
	if (limit_free)
		g_free (limit);
	
	if (offset_free)
		g_free (offset);

	if (dyn_node == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}
	
	if (other_parameters & DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_LIMIT)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "limit")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_limit, ret_bool, ret_value);		
	}

	if (other_parameters & DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_OFFSET)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "offset")) == NULL)
		{
			SDB_UNLOCK(priv);			
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_offset, ret_bool, ret_value);		
	}
	
	/* fill parameters for filter kinds */
	if (other_parameters & DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_INCLUDE_KINDS_YES ||
		other_parameters & DYN_FIND_SYMBOL_IN_SCOPE_EXTRA_PAR_INCLUDE_KINDS_NO)
	{	
		gint i;
		for (i = 0; i < filter_kinds_array->len; i++)
		{
			gchar *curr_str = g_strdup_printf ("filter%d", i);
			param = gda_set_get_holder ((GdaSet*)dyn_node->plist, curr_str);
			MP_SET_HOLDER_BATCH_STR(priv, param, g_ptr_array_index (filter_kinds_array, i), 
			    ret_bool, ret_value);		
			g_free (curr_str);
		}
	}
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "pattern")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}

	MP_SET_HOLDER_BATCH_STR(priv, param, pattern, ret_bool, ret_value);

	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "givensymid")) == NULL)
	{
		SDB_UNLOCK(priv);
		return NULL;
	}

	MP_SET_HOLDER_BATCH_INT(priv, param, container_symbol_id, ret_bool, ret_value);
	
	
	/* DEBUG_PRINT ("query: %s", dyn_node->query_str); */

	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);

	/* free the filter kinds, if it's not null */
	if (filter_kinds_array)
	{
		g_ptr_array_foreach (filter_kinds_array, (GFunc)g_free, NULL);
		g_ptr_array_free (filter_kinds_array, TRUE);
	}
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (GDA_DATA_MODEL (data)) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}

	SDB_UNLOCK(priv);
	
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}


/**
 * Personalized GTree mapping:
 * Considering that a gint on a x86 is 4 bytes: we'll reserve:
 * 2 bytes to map the main parameters.
 * 1 byte is for session number.
 * 1 byte is for filter_kinds number, so you'll be able to filter up to 255 parameters.
 * |--------------------------|-------------|-------------|
 *   main parameters [2 bytes]  sessions [1]   filter [1]
 */
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_EXACT_MATCH_YES			0x00010000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_EXACT_MATCH_NO			0x00020000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_INCLUDE_KINDS_YES		0x00040000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_INCLUDE_KINDS_NO			0x00080000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_GLOBAL_SEARCH_YES		0x00100000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_GLOBAL_SEARCH_NO			0x00200000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_GLOBAL_SEARCH_IGNORE		0x00400000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_LIMIT					0x00800000
#define DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_OFFSET					0x01000000

SymbolDBEngineIterator *
symbol_db_engine_find_symbol_by_name_pattern_filtered (SymbolDBEngine *dbe, 
									const gchar *pattern, 
									SymType filter_kinds,
									gboolean include_kinds,
									SymSearchFileScope filescope_search,
									const GList *session_projects,
									gint results_limit, 
									gint results_offset,
									SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	GString *filter_str;
	gchar *query_str;
	const gchar *match_str;
	const gchar *file_scope_str;
	GdaHolder *param;
	const DynChildQueryNode *dyn_node;
	gint other_parameters;
	gchar *limit = "";
	gboolean limit_free = FALSE;
	gchar *offset = "";
	gboolean offset_free = FALSE;
	GValue *ret_value;
	gboolean ret_bool;
	GPtrArray *filter_kinds_array;
	GError* error = NULL;
	GList *session_prjs;
	
	g_return_val_if_fail (dbe != NULL, NULL);
	g_return_val_if_fail (pattern != NULL, NULL);
	priv = dbe->priv;

	/* create a backup internal copy of the struct.
	 * There shouldn't be a big performance loss 
	 */
	session_prjs = NULL;
	GList *node;
	for (node = (GList*)session_projects; node; node = node->next)
	{
		session_prjs = g_list_prepend (session_prjs, g_strdup (node->data));
	}

	SDB_LOCK(priv);
	
	sym_info = sym_info & ~SYMINFO_KIND;
	
	/* initialize dynamic stuff */
	other_parameters = 0;
	dyn_node = NULL;

	/* determine the filter_kinds thing */
	if (filter_kinds == SYMTYPE_UNDEF)
		filter_kinds_array = NULL;
	else
		filter_kinds_array = symbol_db_util_fill_type_array (filter_kinds);
	
	/* check for match */
	if (g_strrstr (pattern, "%") == NULL)
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_EXACT_MATCH_YES;
		match_str = " = ## /* name:'pattern' type:gchararray */";
	}
	else
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_EXACT_MATCH_NO;
		match_str = " LIKE ## /* name:'pattern' type:gchararray */";
	}	
	
	if (filescope_search == SYMSEARCH_FILESCOPE_PUBLIC)
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_GLOBAL_SEARCH_YES;
		file_scope_str = " AND symbol.is_file_scope = 0 ";
	}
	else if (filescope_search == SYMSEARCH_FILESCOPE_PRIVATE)
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_GLOBAL_SEARCH_NO;
		file_scope_str = " AND symbol.is_file_scope = 1 ";
	}
	else 	/* SYMSEARCH_FILESCOPE_IGNORE */
	{
		other_parameters |= 
			DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_GLOBAL_SEARCH_IGNORE;
		file_scope_str = " ";
	}
	
	if (results_limit > 0)
	{
		other_parameters |= DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_LIMIT;
		limit_free = TRUE;
		limit = g_strdup_printf ("LIMIT ## /* name:'limit' type:gint */");
	}
	
	if (results_offset > 0)
	{
		other_parameters |= DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_OFFSET;
		offset = g_strdup_printf ("OFFSET ## /* name:'offset' type:gint */");
		offset_free = TRUE;
	}

	if (session_prjs != NULL)
	{
		gint list_length = g_list_length (session_prjs);
		if (list_length < 255 && list_length > 0) 
		{		
			/* shift the bits. We want to put the result on the third byte */
			list_length <<= 8;
			other_parameters |= list_length;
		}
		else 
		{
			g_warning ("symbol_db_engine_find_symbol_by_name_pattern_filtered (): "
				"session_prjs has 0 length.");						   
		}
	}
	
	if (filter_kinds_array == NULL || filter_kinds_array->len > 255 || 
	    filter_kinds_array->len <= 0) 
	{
		if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
			DYN_PREP_QUERY_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED, sym_info, 
														 other_parameters)) == NULL)
		{
			gint i;
			GString *filter_projects;
			/* info_data contains the stuff after SELECT and befor FROM */
			info_data = g_string_new ("");
	
			/* join_data contains the optionals joins to do to retrieve new data on other
			 * tables.
		 	 */
			join_data = g_string_new ("");
		
			/* fill info_data and join data with optional sql */
			sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
			
			/* build session projects filter string */
			filter_projects = g_string_new ("");
			if (session_prjs != NULL)
			{
				filter_projects = g_string_append (filter_projects,
				"AND symbol.file_defined_id IN "
				"(SELECT file.file_id FROM file JOIN project ON file.prj_id = "
					"project.project_id WHERE project.project_name IN ( "
					"## /* name:'prj_filter0' type:gchararray */");
				
				for (i = 1; i < g_list_length (session_prjs); i++)
				{				
					g_string_append_printf (filter_projects, 
						",## /* name:'prj_filter%d' type:gchararray */", i);
				}				
				filter_projects = g_string_append (filter_projects, "))");
			}

			query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, "
					"symbol.name AS name, symbol.file_position AS file_position, "
					"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
			    	"symbol.returntype AS returntype, "
					"sym_kind.kind_name AS kind_name "
					"%s FROM symbol %s JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "
					"WHERE symbol.name %s %s %s %s %s", 
				info_data->str, join_data->str, match_str, file_scope_str, 
										 filter_projects->str, limit, offset);			

			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED,
						sym_info, other_parameters,
						query_str);

			g_free (query_str);
			g_string_free (info_data, TRUE);
			g_string_free (join_data, TRUE);
			g_string_free (filter_projects, TRUE);
		}
	}
	else
	{		
		if (include_kinds == TRUE)
		{
			other_parameters |= 
				DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_INCLUDE_KINDS_YES;
		}
		else
		{
			other_parameters |= 
				DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_INCLUDE_KINDS_NO;
		}

		/* set the number of parameters in the less important byte */
		other_parameters |= filter_kinds_array->len;
		
		if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
				DYN_PREP_QUERY_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED, sym_info, 
				other_parameters)) == NULL)
		{					
			gint i;
			GString *filter_projects;
			
			/* info_data contains the stuff after SELECT and before FROM */
			info_data = g_string_new ("");

			/* join_data contains the optionals joins to do to retrieve new 
			 * data on other tables.
		 	 */
			join_data = g_string_new ("");
	
			/* fill info_data and join data with optional sql */
			sdb_engine_prepare_symbol_info_sql (dbe, info_data, join_data, sym_info);
			/* prepare the dynamic filter string before the final query */
			filter_str = g_string_new ("");
		
			if (include_kinds == TRUE)
			{				
				filter_str = g_string_append (filter_str , 
					"AND sym_kind.kind_name IN (## /* name:'filter0' type:gchararray */");		
				for (i = 1; i < filter_kinds_array->len; i++)
				{				
					g_string_append_printf (filter_str , 
							",## /* name:'filter%d' type:gchararray */", i);
				}
				filter_str = g_string_append (filter_str , ")");				
			}
			else
			{
				filter_str = g_string_append (filter_str , 
					"AND sym_kind.kind_name NOT IN (## /* name:'filter0' type:gchararray */");
				for (i = 1; i < filter_kinds_array->len; i++)
				{				
					g_string_append_printf (filter_str , 
							",## /* name:'filter%d' type:gchararray */", i);
				}
				filter_str = g_string_append (filter_str , ")");				
			}
			
			/* build session projects filter string */
			filter_projects = g_string_new ("");
			if (session_prjs != NULL)
			{
				filter_projects = g_string_append (filter_projects,
				"AND symbol.file_defined_id IN "
				"(SELECT file.file_id FROM file JOIN project ON file.prj_id = "
					"project.project_id WHERE project.project_name IN ( "
					"## /* name:'prj_filter0' type:gchararray */");
				
				for (i = 1; i < g_list_length (session_prjs); i++)
				{				
					g_string_append_printf (filter_projects, 
						",## /* name:'prj_filter%d' type:gchararray */", i);
				}				
				filter_projects = g_string_append (filter_projects, "))");
			}			
			
			query_str = g_strdup_printf ("SELECT symbol.symbol_id AS symbol_id, symbol.name AS name, "
				"symbol.file_position AS file_position, "
				"symbol.is_file_scope AS is_file_scope, symbol.signature AS signature, "
			    "symbol.returntype AS returntype, "
				"sym_kind.kind_name AS kind_name "
					"%s FROM symbol %s JOIN sym_kind ON symbol.kind_id = sym_kind.sym_kind_id "
					"WHERE symbol.name %s %s %s %s GROUP BY symbol.name %s %s", 
			 		info_data->str, join_data->str, match_str, file_scope_str,
			 		filter_str->str, filter_projects->str, limit, offset);
			
			dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
						DYN_PREP_QUERY_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED,
						sym_info, other_parameters,
						query_str);
			g_free (query_str);
			g_string_free (info_data, TRUE);
			g_string_free (join_data, TRUE);				
			g_string_free (filter_str, TRUE);
			g_string_free (filter_projects, TRUE);
		}
	}
	
	
	if (limit_free)
		g_free (limit);
	
	if (offset_free)
		g_free (offset);

	if (dyn_node == NULL)
	{
		SDB_UNLOCK(priv);
		if (session_prjs)
		{
			g_list_foreach (session_prjs, (GFunc)g_free, NULL);
			g_list_free (session_prjs);
		}
		
		return NULL;
	}
	
	if (other_parameters & DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_LIMIT)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "limit")) == NULL)
		{
			SDB_UNLOCK(priv);
			if (session_prjs)
			{
				g_list_foreach (session_prjs, (GFunc)g_free, NULL);
				g_list_free (session_prjs);
			}
			
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_limit, ret_bool, ret_value);		
	}

	if (other_parameters & DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_OFFSET)
	{	
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "offset")) == NULL)
		{
			SDB_UNLOCK(priv);			
			if (session_prjs)
			{
				g_list_foreach (session_prjs, (GFunc)g_free, NULL);
				g_list_free (session_prjs);
			}
			
			return NULL;
		}

		MP_SET_HOLDER_BATCH_INT(priv, param, results_offset, ret_bool, ret_value);		
	}
	
	/* fill parameters for filter kinds */
	if (other_parameters & DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_INCLUDE_KINDS_YES ||
		other_parameters & DYN_FIND_SYMBOL_BY_NAME_PATTERN_FILTERED_EXTRA_PAR_INCLUDE_KINDS_NO)
	{	
		gint i;
		for (i = 0; i < filter_kinds_array->len; i++)
		{
			gchar *curr_str = g_strdup_printf ("filter%d", i);
			param = gda_set_get_holder ((GdaSet*)dyn_node->plist, curr_str);
			/*DEBUG_PRINT ("adding type %s", g_ptr_array_index (filter_kinds_array, i));*/
			MP_SET_HOLDER_BATCH_STR(priv, param, g_ptr_array_index (filter_kinds_array, i), 
			    ret_bool, ret_value);		
			g_free (curr_str);
		}
	}

	/* fill parameters for filter projects (packages) */
	if (session_prjs != NULL)
	{
		gint i = 0;
		GList *item;
		item = session_prjs;
		while (item != NULL)
		{
			gchar *curr_str = g_strdup_printf ("prj_filter%d", i);
			param = gda_set_get_holder ((GdaSet*)dyn_node->plist, curr_str);
			MP_SET_HOLDER_BATCH_STR(priv, param, item->data, ret_bool, ret_value);
			g_free (curr_str);
			
			item = item->next;
			i++;
		}
	}
	
	if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "pattern")) == NULL)
	{
		SDB_UNLOCK(priv);
		if (session_prjs)
		{
			g_list_foreach (session_prjs, (GFunc)g_free, NULL);
			g_list_free (session_prjs);
		}
		
		return NULL;
	}

	MP_SET_HOLDER_BATCH_STR(priv, param, pattern, ret_bool, ret_value);
	
	/*DEBUG_PRINT ("query: %s",
				 dyn_node->query_str);*/

	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, &error);

	if (error)
	{
		DEBUG_PRINT ("Error while executing statement %s", error->message);
		g_error_free(error);
	}

	/* free the filter kinds, if it's not null */
	if (filter_kinds_array)
	{
		g_ptr_array_foreach (filter_kinds_array, (GFunc)g_free, NULL);
		g_ptr_array_free (filter_kinds_array, TRUE);
	}
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (GDA_DATA_MODEL (data)) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);

		if (session_prjs)
		{
			g_list_foreach (session_prjs, (GFunc)g_free, NULL);
			g_list_free (session_prjs);
		}
		
		SDB_UNLOCK(priv);
		return NULL;
	}

	if (session_prjs)
	{
		g_list_foreach (session_prjs, (GFunc)g_free, NULL);
		g_list_free (session_prjs);
	}
	
	SDB_UNLOCK(priv);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

/**
 * Gets the files of a project.
 * @param project_name name of project you want to know the files of.
 *        It can be NULL. In that case all the files will be returned.
 */
#define DYN_GET_FILES_FOR_PROJECT_EXTRA_PAR_ALL					1
#define DYN_GET_FILES_FOR_PROJECT_EXTRA_PAR_PROJECT				2

SymbolDBEngineIterator *
symbol_db_engine_get_files_for_project (SymbolDBEngine *dbe, 
									const gchar *project_name,
								 	SymExtraInfo sym_info)
{
	SymbolDBEnginePriv *priv;
	GdaDataModel *data;
	GString *info_data;
	GString *join_data;
	gchar *query_str;
	GdaHolder *param;
	const DynChildQueryNode *dyn_node;
	gint other_parameters;
	GValue *ret_value;
	gboolean ret_bool;

	g_return_val_if_fail (dbe != NULL, NULL);
	priv = dbe->priv;

	SDB_LOCK(priv);
	
	sym_info = sym_info & ~SYMINFO_FILE_PATH;
	sym_info = sym_info & ~SYMINFO_PROJECT_NAME;
	
	/* initialize dynamic stuff */
	other_parameters = 0;
	dyn_node = NULL;

	if (project_name == NULL)
	{
		other_parameters |= DYN_GET_FILES_FOR_PROJECT_EXTRA_PAR_ALL;
	}
	else
	{
		other_parameters |= DYN_GET_FILES_FOR_PROJECT_EXTRA_PAR_PROJECT;
	}
	
	/* info_data contains the stuff after SELECT and befor FROM */
	info_data = g_string_new ("");
	
	/* join_data contains the optionals joins to do to retrieve new data on other
	 * tables.
	 */
	join_data = g_string_new ("");
		
	/* fill info_data and join data with optional sql */
	sdb_engine_prepare_file_info_sql (dbe, info_data, join_data, sym_info);

	if ((dyn_node = sdb_engine_get_dyn_query_node_by_id (dbe, 
			DYN_PREP_QUERY_GET_FILES_FOR_PROJECT, sym_info, 
			other_parameters)) == NULL)
	{					
		if (project_name == NULL)
		{
			query_str = g_strdup_printf ("SELECT file.file_path AS db_file_path "
				"%s FROM file %s ",
		 		info_data->str, join_data->str);
		}
		else 
		{
			query_str = g_strdup_printf ("SELECT file.file_path AS db_file_path, "
				"project.project_name AS project_name "
				"%s FROM file JOIN project ON file.prj_id = project.project_id %s "
				"WHERE project.project_name = ## /* name:'prj_name' type:gchararray */",
		 		info_data->str, join_data->str);
			
		}
			
		dyn_node = sdb_engine_insert_dyn_query_node_by_id (dbe, 
					DYN_PREP_QUERY_GET_FILES_FOR_PROJECT,
					sym_info, other_parameters,
					query_str);
		g_free (query_str);
	}

	g_string_free (info_data, TRUE);
	g_string_free (join_data, TRUE);				
	
	/* last check */
	if (other_parameters & DYN_GET_FILES_FOR_PROJECT_EXTRA_PAR_PROJECT)
	{
		if ((param = gda_set_get_holder ((GdaSet*)dyn_node->plist, "prj_name")) == NULL)
		{
			SDB_UNLOCK(priv);
			return NULL;
		}

		MP_SET_HOLDER_BATCH_STR(priv, param, project_name, ret_bool, ret_value);
	}

	/* execute the query with parametes just set */
	data = gda_connection_statement_execute_select (priv->db_connection, 
												  (GdaStatement*)dyn_node->stmt, 
												  (GdaSet*)dyn_node->plist, NULL);
		
/*	DEBUG_PRINT ("symbol_db_engine_get_files_for_project (): query_str is %s",
				 dyn_node->query_str);
	DEBUG_PRINT ("%s", "symbol_db_engine_get_files_for_project (): data dump \n");
	gda_data_model_dump (data, stdout);*/
	
	if (!GDA_IS_DATA_MODEL (data) ||
		gda_data_model_get_n_rows (GDA_DATA_MODEL (data)) <= 0)
	{
		if (data != NULL)
			g_object_unref (data);
		
		SDB_UNLOCK(priv);
		return NULL;
	}

	SDB_UNLOCK(priv);
	return (SymbolDBEngineIterator *)symbol_db_engine_iterator_new (data, 
												priv->sym_type_conversion_hash,
												priv->project_directory);	
}

gint
symbol_db_engine_get_languages_count (SymbolDBEngine *dbe)
{
	SymbolDBEnginePriv *priv;
	GdaDataModel *data_model;	
	const GdaStatement *stmt;
	const GValue *value;
	gint num_rows = 0;
	gint ret = -1;

	g_return_val_if_fail (dbe != NULL, -1);
	priv = dbe->priv;
	
	SDB_LOCK(priv);
	
	if ((stmt = sdb_engine_get_statement_by_query_id (dbe,
								 PREP_QUERY_GET_LANGUAGE_COUNT))
		== NULL)
	{
		SDB_UNLOCK(priv);
		return -1;
	}

	data_model = gda_connection_statement_execute_select (priv->db_connection, 
														  (GdaStatement*)stmt, 
														  NULL, NULL);
	
	if (!GDA_IS_DATA_MODEL (data_model) ||
		(num_rows = gda_data_model_get_n_rows (GDA_DATA_MODEL (data_model))) <= 0)
	{
		if (data_model != NULL)
			g_object_unref (data_model);
		SDB_UNLOCK(priv);
		return -1;
	}	
	
	if ((value = gda_data_model_get_value_at (data_model, 0, 0, NULL)) != NULL)
	{
		ret = g_value_get_int (value);
	}

	if (data_model)
		g_object_unref (data_model);
	
	SDB_UNLOCK(priv);
	
	return ret;	
}

gboolean
symbol_db_engine_is_language_used (SymbolDBEngine *dbe,
								   const gchar *language)
{
	gint table_id;
	GValue *value;
	SymbolDBEnginePriv *priv;		
	
	g_return_val_if_fail (language != NULL, FALSE);
	
	priv = dbe->priv;
	SDB_LOCK(priv);

	MP_LEND_OBJ_STR(priv, value);
	g_value_set_static_string (value, language);

	/* check for an already existing table with language "name". */
	if ((table_id = sdb_engine_get_tuple_id_by_unique_name (dbe,
						PREP_QUERY_GET_LANGUAGE_ID_BY_UNIQUE_NAME,
						"langname",
						value)) < 0)
	{
		SDB_UNLOCK(priv);
	
		return FALSE;
	}

	SDB_UNLOCK(priv);

	return TRUE;
}

