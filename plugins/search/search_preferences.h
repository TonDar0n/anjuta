#ifndef _SEARCH_PREFERENCES_H
#define _SEARCH_PREFERENCES_H

#ifdef __cplusplus
extern "C"
{
#endif

// FIXME:
typedef int PropsID;	
	
gboolean search_preferences_save_yourself (FILE *stream);
gboolean search_preferences_load_yourself (PropsID props);

void on_setting_pref_add_clicked(GtkButton *button, gpointer user_data);
void on_setting_pref_remove_clicked(GtkButton *button, gpointer user_data);
void on_setting_pref_modify_clicked(GtkButton *button, gpointer user_data);

void search_preferences_initialize_setting_treeview(GtkWidget *dialog);

void search_preferences_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _SEARCH_PREFERENCES_H */
