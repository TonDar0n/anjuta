/* Symbol db performance stress test */


#include "../symbol-db-engine.h"
#include <gtk/gtk.h>

static void on_single_file_scan_end (SymbolDBEngine* engine, GPtrArray* files)
{
	static int i = 0;
	g_message ("Finished [%d]: %s", i, (gchar*)g_ptr_array_index (files, i));
	i++;
}
	
static void 
on_scan_end (SymbolDBEngine* engine, gpointer user_data)
{
	g_message ("on_scan_end  ()");
	symbol_db_engine_close_db (engine);
	g_object_unref (engine);
  	exit(0);
}

int main (int argc, char** argv)
{
  	SymbolDBEngine* engine;
  	GPtrArray* files;
	GPtrArray* languages = g_ptr_array_new();
	gchar* root_dir;
	GFile *g_dir;
	GHashTable *mimes;
	int i;
	
  	gtk_init(&argc, &argv);
  	g_thread_init (NULL);
	gda_init ();
	
	if (argc != 2)
	{
		g_message ("Usage: benchmark <source_directory>");
		return 1;
	}

	g_dir = g_file_new_for_path (argv[1]);
	if (g_dir == NULL)
	{
		g_warning ("%s doesn't exist", argv[1]);
		return -1;
	}

	root_dir = g_file_get_path (g_dir);
	
    engine = symbol_db_engine_new_full ("anjuta-tags", "benchmark-db");
  
	if (!symbol_db_engine_open_db (engine, root_dir, root_dir))
	{
		g_message ("Could not open database: %s", root_dir);
		return -1;
	}

	symbol_db_engine_add_new_project (engine, NULL, root_dir);
			
	mimes = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (mimes, "text/x-csrc", "text/x-csrc");
	g_hash_table_insert (mimes, "text/x-chdr", "text/x-chdr");
	g_hash_table_insert (mimes, "text/x-c++src", "text/x-c++src");
	g_hash_table_insert (mimes, "text/x-c+++hdr", "text/x-c++hdr");
	
	files = symbol_db_util_get_source_files_by_mime (root_dir, mimes);
	g_hash_table_destroy (mimes);

	for (i = 0; i < files->len; i++)
		g_ptr_array_add (languages, "C");
	
	g_signal_connect (engine, "scan-end", G_CALLBACK (on_scan_end), NULL);
	g_signal_connect (G_OBJECT (engine), "single-file-scan-end",
		  G_CALLBACK (on_single_file_scan_end), files);
	
	symbol_db_engine_add_new_files_full (engine, root_dir, files, languages, TRUE);	

	g_free (root_dir);
	g_object_unref (g_dir);
	
	gtk_main();
	
	return 0;
}