/*************************************************************************/
/* Copyright (C) 2007-2009 sujith <m.sujith@gmail.com>			 */
/* Copyright (C) 2009-2012 matias <mati86dl@gmail.com>			 */
/* 									 */
/* This program is free software: you can redistribute it and/or modify	 */
/* it under the terms of the GNU General Public License as published by	 */
/* the Free Software Foundation, either version 3 of the License, or	 */
/* (at your option) any later version.					 */
/* 									 */
/* This program is distributed in the hope that it will be useful,	 */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of	 */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	 */
/* GNU General Public License for more details.				 */
/* 									 */
/* You should have received a copy of the GNU General Public License	 */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/*************************************************************************/

#include "pragha.h"

static void add_entry_playlist(gchar *playlist,
			       GtkTreeIter *root,
			       GtkTreeModel *model,
			       struct con_win *cwin)
{
	GtkTreeIter iter;

	gtk_tree_store_append(GTK_TREE_STORE(model),
			      &iter,
			      root);
	gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
			   P_PIXBUF, cwin->pixbuf->pixbuf_track,
			   P_PLAYLIST, playlist,
			   -1);
}

/* Add all the tracks under the given path to the current playlist */
/* NB: Optimization */

static void add_playlist_row_current_playlist(GtkTreePath *path, struct con_win *cwin)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *playlist;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->playlist_tree));
	if (gtk_tree_model_get_iter(model, &iter, path)) {
		gtk_tree_model_get(model, &iter, P_PLAYLIST, &playlist, -1);
		add_playlist_current_playlist(playlist, cwin);
		g_free(playlist);
	}
}

static gboolean overwrite_existing_playlist(const gchar *playlist,
					    struct con_win *cwin)
{
	gboolean choice = FALSE;
	gint ret;
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(cwin->mainwindow),
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_YES_NO,
				_("Do you want to overwrite the playlist: %s ?"),
				playlist);

	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	switch(ret) {
	case GTK_RESPONSE_YES:
		choice = TRUE;
		break;
	case GTK_RESPONSE_NO:
		choice = FALSE;
		break;
	default:
		break;
	}

	gtk_widget_destroy(dialog);
	return choice;
}

static GIOChannel* create_m3u_playlist(gchar *file, struct con_win *cwin)
{
	GIOChannel *chan = NULL;
	GIOStatus status;
	GError *err = NULL;
	gsize bytes = 0;

	chan = g_io_channel_new_file(file, "w+", &err);
	if (!chan) {
		g_critical("Unable to create M3U playlist IO channel: %s", file);
		goto exit_failure;
	}

	status = g_io_channel_write_chars(chan, "#EXTM3U\n", -1, &bytes, &err);
	if (status != G_IO_STATUS_NORMAL) {
		g_critical("Unable to write to M3U playlist: %s", file);
		goto exit_failure;
	}

	CDEBUG(DBG_INFO, "Created M3U playlist file: %s", file);
	return chan;

exit_failure:
	g_error_free(err);
	err = NULL;

	if (chan) {
		g_io_channel_shutdown(chan, FALSE, &err);
		g_io_channel_unref(chan);
	}

	return NULL;
}

static gint save_complete_m3u_playlist(GIOChannel *chan, gchar *filename, struct con_win *cwin)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *str = NULL, *uri = NULL, *base_m3u = NULL, *base = NULL;
	struct musicobject *mobj = NULL;
	GIOStatus status;
	gsize bytes = 0;
	GError *err = NULL;
	gint ret = 0;
	gboolean next;

	base_m3u = get_display_filename(filename, TRUE);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->current_playlist));

	next = gtk_tree_model_get_iter_first(model, &iter);
	while (next) {
		gtk_tree_model_get (model, &iter, P_MOBJ_PTR, &mobj, -1);
		if (G_LIKELY(mobj && mobj->file_type != FILE_CDDA)) {

			base = get_display_filename(mobj->file, TRUE);

			if (g_ascii_strcasecmp(base_m3u, base) == 0)
				uri = get_display_filename(mobj->file, FALSE);
			else
				uri = g_strdup(mobj->file);

			/* Format: "#EXTINF:seconds, title" */
			str = g_strdup_printf("#EXTINF:%d,%s\n%s\n",
					      mobj->tags->length,
					      mobj->tags->title,
					      uri);

			status = g_io_channel_write_chars(chan, str, -1, &bytes, &err);
			if (status != G_IO_STATUS_NORMAL) {
				g_critical("Unable to write to M3U playlist: %s", filename);
				ret = -1;
				goto exit;
			}
			g_free(base);
			g_free(uri);
		}

		/* Have to give control to GTK periodically ... */
		/* If gtk_main_quit has been called, return -
		   since main loop is no more. */
		while(gtk_events_pending()) {
			if (gtk_main_iteration_do(FALSE)) {
				return 0;
			}
		}
	next = gtk_tree_model_iter_next(model, &iter);
	}

exit:
	g_free(base_m3u);
	if (err) {
		g_error_free(err);
		err = NULL;
	}
	return ret;
}

static gint save_selected_to_m3u_playlist(GIOChannel *chan, gchar *filename, struct con_win *cwin)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	GList *list, *i;
	gchar *str = NULL, *uri = NULL, *base_m3u = NULL, *base = NULL;
	struct musicobject *mobj = NULL;
	GIOStatus status;
	gsize bytes = 0;
	GError *err = NULL;
	gint ret = 0;

	base_m3u = get_display_filename(filename, TRUE);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->current_playlist));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->current_playlist));
	list = gtk_tree_selection_get_selected_rows(selection, NULL);

	if (list) {
		/* Export all selected tracks to the given file */
		for (i=list; i != NULL; i = i->next) {
			path = i->data;
			gtk_tree_model_get_iter(model, &iter, path);
			gtk_tree_model_get(model, &iter,
					   P_MOBJ_PTR, &mobj, -1);
			if (G_LIKELY(mobj && mobj->file_type != FILE_CDDA)) {

				base = get_display_filename(mobj->file, TRUE);

				if (g_ascii_strcasecmp(base_m3u, base) == 0)
					uri = get_display_filename(mobj->file, FALSE);
				else
					uri = g_strdup(mobj->file);

				/* Format: "#EXTINF:seconds, title" */
				str = g_strdup_printf("#EXTINF:%d,%s\n%s\n",
						      mobj->tags->length,
						      mobj->tags->title,
						      uri);

				status = g_io_channel_write_chars(chan, str, -1, &bytes, &err);
				if (status != G_IO_STATUS_NORMAL) {
					g_critical("Unable to write to M3U playlist: %s", filename);
					ret = -1;
					goto exit_list;
				}
				g_free(base);
				g_free(uri);
			}
			gtk_tree_path_free(path);

			/* Have to give control to GTK periodically ... */
			/* If gtk_main_quit has been called, return -
			   since main loop is no more. */

			while(gtk_events_pending()) {
				if (gtk_main_iteration_do(FALSE)) {
					g_list_free(list);
					return 0;
				}
			}
		}
	}

exit_list:
	g_free(base_m3u);
	if (list)
		g_list_free(list);
	if (err) {
		g_error_free(err);
		err = NULL;
	}
	return ret;
}

static gint save_m3u_playlist(GIOChannel *chan, gchar *playlist, gchar *filename,
			      struct con_win *cwin)
{
	GError *err = NULL;
	gchar *query, *str, *title, *file = NULL, *s_playlist, *uri = NULL, *base_m3u = NULL, *base = NULL;
	gint playlist_id, location_id, i = 0, ret = 0;
	gsize bytes = 0;
	struct db_result result;
	struct musicobject *mobj = NULL;
	GIOStatus status;

	s_playlist = sanitize_string_sqlite3(playlist);
	playlist_id = find_playlist_db(s_playlist, cwin);
	query = g_strdup_printf("SELECT FILE FROM PLAYLIST_TRACKS WHERE PLAYLIST=%d",
				playlist_id);
	if (!exec_sqlite_query(query, cwin, &result)) {
		g_free(s_playlist);
		return -1;
	}

	base_m3u = get_display_filename(filename, TRUE);

	for_each_result_row(result, i) {
		file = sanitize_string_sqlite3(result.resultp[i]);

		/* Form a musicobject since length and title are needed */

		if ((location_id = find_location_db(file, cwin)))
			mobj = new_musicobject_from_db(location_id, cwin);
		else
			mobj = new_musicobject_from_file(result.resultp[i]);

		if (!mobj) {
			g_warning("Unable to create musicobject for M3U playlist: %s",
				  filename);
			g_free(file);
			continue;
		}

		base = get_display_filename(mobj->file, TRUE);

		if (g_ascii_strcasecmp(base_m3u, base) == 0)
			uri = get_display_filename(mobj->file, FALSE);
		else
			uri = g_strdup(mobj->file);

		/* If title tag is absent, just store the filename */

		if (mobj->tags->title)
			title = mobj->tags->title;
		else
			title = uri;

		/* Format: "#EXTINF:seconds, title" */

		str = g_strdup_printf("#EXTINF:%d,%s\n%s\n",
				      mobj->tags->length, title, uri);

		status = g_io_channel_write_chars(chan, str, -1, &bytes, &err);
		if (status != G_IO_STATUS_NORMAL) {
			g_critical("Unable to write to M3U playlist: %s", filename);
			g_free(uri);
			g_free(file);
			g_free(str);
			g_free(base);
			ret = -1;
			goto exit;
		}

		g_free(str);
		g_free(file);
		g_free(uri);
		g_free(base);

		if (mobj) {
			delete_musicobject(mobj);
			mobj = NULL;
		}
	}

exit:
	g_free(s_playlist);
	g_free(base_m3u);
	if (mobj)
		delete_musicobject(mobj);
	if (err) {
		g_error_free(err);
		err = NULL;
	}
	sqlite3_free_table(result.resultp);

	return ret;
}

/**********************/
/* External functions */
/**********************/

/* Append the given playlist to the current playlist */

void add_playlist_current_playlist(gchar *playlist, struct con_win *cwin)
{
	GtkTreeModel *model;
	gchar *s_playlist, *query, *file;
	gint playlist_id, location_id, i = 0;
	struct db_result result;
	struct musicobject *mobj;
	GdkCursor *cursor;

	s_playlist = sanitize_string_sqlite3(playlist);
	playlist_id = find_playlist_db(s_playlist, cwin);

	if(playlist_id == 0)
		goto bad;

	query = g_strdup_printf("SELECT FILE FROM PLAYLIST_TRACKS WHERE PLAYLIST=%d",
				playlist_id);
	exec_sqlite_query(query, cwin, &result);

	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor (GDK_WINDOW(cwin->mainwindow->window), cursor);
	gdk_cursor_unref(cursor);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->current_playlist));

	g_object_ref(model);
	gtk_widget_set_sensitive(GTK_WIDGET(cwin->current_playlist), FALSE);
	gtk_tree_view_set_model(GTK_TREE_VIEW(cwin->current_playlist), NULL);

	for_each_result_row(result, i) {
		file = sanitize_string_sqlite3(result.resultp[i]);

		if ((location_id = find_location_db(file, cwin)))
			mobj = new_musicobject_from_db(location_id, cwin);
		else
			mobj = new_musicobject_from_file(result.resultp[i]);

		append_current_playlist_on_model(model, mobj, cwin);

		g_free(file);
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(cwin->current_playlist), model);
	gtk_widget_set_sensitive(GTK_WIDGET(cwin->current_playlist), TRUE);
	g_object_unref(model);

	gdk_window_set_cursor(GDK_WINDOW(cwin->mainwindow->window), NULL);

	update_status_bar(cwin);

	sqlite3_free_table(result.resultp);

bad:
	g_free(s_playlist);
}

void playlist_tree_row_activated_cb(GtkTreeView *playlist_tree,
				    GtkTreePath *path,
				    GtkTreeViewColumn *column,
				    struct con_win *cwin)
{
	GtkTreeIter r_iter;
	GtkTreePath *r_path;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->playlist_tree));

	gtk_tree_model_get_iter_first(model, &r_iter);
	r_path = gtk_tree_model_get_path(model, &r_iter);

	if (!gtk_tree_path_compare(path, r_path)) {
		if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(cwin->playlist_tree),
						path))
			gtk_tree_view_expand_row(GTK_TREE_VIEW(cwin->playlist_tree),
						 path, FALSE);
		else
			gtk_tree_view_collapse_row(GTK_TREE_VIEW(cwin->playlist_tree),
						   path);
	}
	else {
		add_playlist_row_current_playlist(path, cwin);
	}

	gtk_tree_path_free(r_path);
}

gboolean playlist_tree_button_press_cb(GtkWidget *widget,
				      GdkEventButton *event,
				      struct con_win *cwin)
{
	GtkWidget *popup_menu;
	GtkTreePath *path;
	GtkTreeSelection *selection;
	gboolean many_selected = FALSE;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->playlist_tree));

	if(gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), (gint) event->x,(gint) event->y, &path, NULL, NULL, NULL)){
		switch(event->button) {
		case 1:
			if (gtk_tree_selection_path_is_selected(selection, path)
			    && !(event->state & GDK_CONTROL_MASK)
			    && !(event->state & GDK_SHIFT_MASK)) {
				gtk_tree_selection_set_select_function(selection, &tree_selection_func_false, cwin, NULL);
			}
			else {
				gtk_tree_selection_set_select_function(selection, &tree_selection_func_true, cwin, NULL);
			}
			break;
		case 2:
			if (!gtk_tree_selection_path_is_selected(selection, path)){
				gtk_tree_selection_unselect_all(selection);
				gtk_tree_selection_select_path(selection, path);
			}
			playlist_tree_add_to_playlist(cwin);
			break;
		case 3:
			if (!(gtk_tree_selection_path_is_selected(selection, path))){
				gtk_tree_selection_unselect_all(selection);
				gtk_tree_selection_select_path(selection, path);
			}

			popup_menu = gtk_ui_manager_get_widget(cwin->playlist_tree_context_menu,
								"/popup");
			gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
					event->button, event->time);

			/* If more than one track is selected, don't propagate event */

			if (gtk_tree_selection_count_selected_rows(selection) > 1)
				many_selected = TRUE;
			else
				many_selected = FALSE;
			break;
		default:
			many_selected = FALSE;
			break;
		}
	gtk_tree_path_free(path);
	}
	else gtk_tree_selection_unselect_all(selection);

	return many_selected;
}

gboolean playlist_tree_button_release_cb(GtkWidget *widget,
				     GdkEventButton *event,
				     struct con_win *cwin)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->playlist_tree));
	
	if((event->state & GDK_CONTROL_MASK) || (event->state & GDK_SHIFT_MASK) || (cwin->cstate->dragging == TRUE) || (event->button!=1)){
		gtk_tree_selection_set_select_function(selection, &tree_selection_func_true, cwin, NULL);
		cwin->cstate->dragging = FALSE;
		return FALSE;
	}

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), (gint) event->x,(gint) event->y, &path, NULL, NULL, NULL);

	if (path){
		gtk_tree_selection_set_select_function(selection, &tree_selection_func_true, cwin, NULL);
		gtk_tree_selection_unselect_all(selection);
		gtk_tree_selection_select_path(selection, path);
		gtk_tree_path_free(path);
	}
	return FALSE;
}

void playlist_tree_replace_playlist(GtkAction *action, struct con_win *cwin)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GList *list, *i;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->playlist_tree));
	list = gtk_tree_selection_get_selected_rows(selection, NULL);

	if (list) {
		clear_current_playlist(NULL, cwin);

		for (i=list; i != NULL; i = i->next) {
			path = i->data;
			if (gtk_tree_path_get_depth(path) > 1)
				add_playlist_row_current_playlist(path, cwin);

			gtk_tree_path_free(path);

			/* Have to give control to GTK periodically ... */
			/* If gtk_main_quit has been called, return -
			   since main loop is no more. */

			while(gtk_events_pending()) {
				if (gtk_main_iteration_do(FALSE)) {
					g_list_free(list);
					return;
				}
			}
		}
		
		g_list_free(list);
	}
}

void playlist_tree_replace_and_play(GtkAction *action, struct con_win *cwin)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GList *list, *i;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->playlist_tree));
	list = gtk_tree_selection_get_selected_rows(selection, NULL);

	if (list) {
		clear_current_playlist(NULL, cwin);

		for (i=list; i != NULL; i = i->next) {
			path = i->data;
			if (gtk_tree_path_get_depth(path) > 1)
				add_playlist_row_current_playlist(path, cwin);
			gtk_tree_path_free(path);

			/* Have to give control to GTK periodically ... */
			/* If gtk_main_quit has been called, return -
			since main loop is no more. */

			while(gtk_events_pending()) {
				if (gtk_main_iteration_do(FALSE)) {
					g_list_free(list);
					return;
				}
			}
		}
		g_list_free(list);
	}

	play_first_current_playlist(cwin);
}

void playlist_tree_add_to_playlist_action(GtkAction *action, struct con_win *cwin)
{
	playlist_tree_add_to_playlist(cwin);
}

void playlist_tree_add_to_playlist(struct con_win *cwin)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GList *list, *i;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->playlist_tree));
	list = gtk_tree_selection_get_selected_rows(selection, NULL);

	if (list) {
		/* Add all the rows to the current playlist */

		for (i=list; i != NULL; i = i->next) {
			path = i->data;
			if (gtk_tree_path_get_depth(path) > 1)
				add_playlist_row_current_playlist(path, cwin);

			gtk_tree_path_free(path);

			/* Have to give control to GTK periodically ... */
			/* If gtk_main_quit has been called, return -
			   since main loop is no more. */

			while(gtk_events_pending()) {
				if (gtk_main_iteration_do(FALSE)) {
					g_list_free(list);
					return;
				}
			}
		}
		g_list_free(list);
	}
}

/* Build a dialog to get a new playlist name */

gchar* rename_playlist_dialog(const gchar * oplaylist, struct con_win *cwin)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label_new, *entry;
	gchar *playlist = NULL;
	gint result;

	/* Create dialog window */

	hbox = gtk_hbox_new(FALSE, 2);

	label_new = gtk_label_new_with_mnemonic(_("Choose a new name"));

	entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), 255);
	gtk_entry_set_activates_default (GTK_ENTRY(entry), TRUE);

	gtk_entry_set_text(GTK_ENTRY(entry), oplaylist);

	dialog = gtk_dialog_new_with_buttons(_("Rename"),
			     GTK_WINDOW(cwin->mainwindow),
			     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			     GTK_STOCK_CANCEL,
			     GTK_RESPONSE_CANCEL,
			     GTK_STOCK_OK,
			     GTK_RESPONSE_ACCEPT,
			     NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	gtk_box_pack_start(GTK_BOX(hbox), label_new, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 2);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	gtk_widget_show_all(dialog);

	result = gtk_dialog_run(GTK_DIALOG(dialog));
	switch(result) {
	case GTK_RESPONSE_ACCEPT:
		/* Get playlist name */
		playlist = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
		break;
	case GTK_RESPONSE_CANCEL:
		break;
	default:
		break;
	}
	gtk_widget_destroy(dialog);

	return playlist;
}

void playlist_tree_rename(GtkAction *action, struct con_win *cwin)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	GList *list;
	gchar *playlist = NULL, *s_playlist = NULL, *n_playlist = NULL;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->playlist_tree));
	list = gtk_tree_selection_get_selected_rows(selection, &model);

	if (list) {
		path = list->data;
		if (gtk_tree_path_get_depth(path) > 1) {
			gtk_tree_model_get_iter(model, &iter, path);
			gtk_tree_model_get(model, &iter, P_PLAYLIST, &playlist, -1);

			s_playlist = sanitize_string_sqlite3(playlist);

			n_playlist = rename_playlist_dialog(s_playlist, cwin);
			if(n_playlist != NULL) {
				update_playlist_name_db(s_playlist, n_playlist, cwin);
				g_free(n_playlist);
				init_playlist_view(cwin);
			}
			g_free(s_playlist);
			g_free(playlist);
		}
		gtk_tree_path_free(path);
	}
	g_list_free(list);
}

static gboolean delete_existing_item_dialog(const gchar *item,
					    struct con_win *cwin)
{
	gboolean choice = FALSE;
	gint ret;
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(cwin->mainwindow),
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_YES_NO,
				_("Do you want to delete the item: %s ?"),
				item);

	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	switch(ret) {
	case GTK_RESPONSE_YES:
		choice = TRUE;
		break;
	case GTK_RESPONSE_NO:
		choice = FALSE;
		break;
	default:
		break;
	}

	gtk_widget_destroy(dialog);

	return choice;
}

void playlist_tree_delete(GtkAction *action, struct con_win *cwin)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	GList *list, *i;
	gchar *playlist, *s_playlist;
	gboolean removed = FALSE;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->playlist_tree));
	list = gtk_tree_selection_get_selected_rows(selection, &model);

	if (list) {
		/* Delete selected playlists */

		for (i=list; i != NULL; i = i->next) {
			path = i->data;
			if (gtk_tree_path_get_depth(path) > 1) {
				gtk_tree_model_get_iter(model, &iter, path);
				gtk_tree_model_get(model, &iter, P_PLAYLIST,
						   &playlist, -1);
				if(delete_existing_item_dialog(playlist, cwin)) {
					s_playlist = sanitize_string_sqlite3(playlist);
					delete_playlist_db(s_playlist, cwin);
					g_free(s_playlist);
					removed = TRUE;
				}
				g_free(playlist);
			}
			gtk_tree_path_free(path);
		}
		g_list_free(list);
	}

	if(removed)
		init_playlist_view(cwin);
}

/* Export selection/current playlist to a M3U file */

void export_playlist (gint choice, struct con_win *cwin)
{
	GtkWidget *dialog;
	gchar *filename = NULL, *playlistm3u = NULL;
	GIOChannel *chan = NULL;
	GError *err = NULL;
	gint resp;

	dialog = gtk_file_chooser_dialog_new(_("Export playlist to file"),
					     GTK_WINDOW(cwin->mainwindow),
					     GTK_FILE_CHOOSER_ACTION_SAVE,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					     GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					     NULL);

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
						       TRUE);

	playlistm3u = g_strdup_printf("%s.m3u", _("Playlists"));
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(dialog), playlistm3u);

	resp = gtk_dialog_run(GTK_DIALOG(dialog));

	switch (resp) {
	case GTK_RESPONSE_ACCEPT:
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		break;
	default:
		goto exit;
	}

	if (!filename)
		goto exit;

	chan = create_m3u_playlist(filename, cwin);
	if (!chan) {
		g_warning("Unable to create M3U playlist file: %s", filename);
		goto exit;
	}

	switch(choice) {
	case SAVE_COMPLETE:
		if (save_complete_m3u_playlist(chan, filename, cwin) < 0) {
			g_warning("Unable to save M3U playlist: %s", filename);
			goto exit;
		}
		break;
	case SAVE_SELECTED:
		if (save_selected_to_m3u_playlist(chan, filename, cwin) < 0) {
			g_warning("Unable to save M3U playlist: %s", filename);
			goto exit;
		}
		break;
	}

	if (chan) {
		if (g_io_channel_shutdown(chan, TRUE, &err) != G_IO_STATUS_NORMAL) {
			g_critical("Unable to save M3U playlist: %s", filename);
			g_error_free(err);
			err = NULL;
		} else {
			CDEBUG(DBG_INFO, "Saved M3U playlist: %s", filename);
		}
		g_io_channel_unref(chan);
	}

exit:
	g_free(playlistm3u);
	g_free(filename);
	gtk_widget_destroy(dialog);
}

/* Export saved playlist to a M3U file */

void playlist_tree_export(GtkAction *action, struct con_win *cwin)
{
	GtkWidget *dialog;
	GIOChannel *chan = NULL;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GtkTreeIter iter;
	GList *list, *i;
	GError *err = NULL;
	gint resp, cnt;
	gchar *filename = NULL, *playlist = NULL, *playlistpath = NULL, *playlistm3u = NULL;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->playlist_tree));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->playlist_tree));
	cnt = (gtk_tree_selection_count_selected_rows(selection));

	list = gtk_tree_selection_get_selected_rows(selection, NULL);
	path = list->data;

	/* If only is 'Playlist' node, just return, else get playlistname. */
	if ((cnt == 1) && (gtk_tree_path_get_depth(path) == 1)) {
		gtk_tree_path_free(path);
		g_list_free(list);
		return;
	}
	else {
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, P_PLAYLIST, &playlistpath, -1);
	}

	dialog = gtk_file_chooser_dialog_new(_("Export playlist to file"),
					     GTK_WINDOW(cwin->mainwindow),
					     GTK_FILE_CHOOSER_ACTION_SAVE,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					     GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					     NULL);

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
						       TRUE);

	playlistm3u = g_strdup_printf("%s.m3u", playlistpath);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(dialog), playlistm3u);

	resp = gtk_dialog_run(GTK_DIALOG(dialog));

	switch (resp) {
	case GTK_RESPONSE_ACCEPT:
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		break;
	default:
		goto exit;
	}

	if (!filename)
		goto exit;

	chan = create_m3u_playlist(filename, cwin);
	if (!chan) {
		g_warning("Unable to create M3U playlist file: %s", filename);
		goto exit;
	}

	list = gtk_tree_selection_get_selected_rows(selection, NULL);

	if (list) {
		/* Export all the playlists to the given file */

		for (i=list; i != NULL; i = i->next) {
			path = i->data;
			if (gtk_tree_path_get_depth(path) > 1) {
				gtk_tree_model_get_iter(model, &iter, path);
				gtk_tree_model_get(model, &iter, P_PLAYLIST,
						   &playlist, -1);
				if (save_m3u_playlist(chan, playlist,
						      filename, cwin) < 0) {
					g_warning("Unable to save M3U playlist: %s",
						  filename);
					g_free(playlist);
					goto exit_list;
				}
				g_free(playlist);
			}
			gtk_tree_path_free(path);

			/* Have to give control to GTK periodically ... */
			/* If gtk_main_quit has been called, return -
			   since main loop is no more. */

			while(gtk_events_pending()) {
				if (gtk_main_iteration_do(FALSE)) {
					g_list_free(list);
					return;
				}
			}
		}
	}

	if (chan) {
		if (g_io_channel_shutdown(chan, TRUE, &err) != G_IO_STATUS_NORMAL) {
			g_critical("Unable to save M3U playlist: %s", filename);
			g_error_free(err);
			err = NULL;
		} else {
			CDEBUG(DBG_INFO, "Saved M3U playlist: %s", filename);
		}
		g_io_channel_unref(chan);
	}

exit_list:
	if (list)
		g_list_free(list);
exit:
	g_free(playlistm3u);
	g_free(playlistpath);
	g_free(filename);
	gtk_widget_destroy(dialog);
}

GSList *
pragha_pl_parser_parse_pls (const gchar *file)
{
	GKeyFile *plskeyfile;
	GError *error = NULL;
	GSList *list = NULL;
	guint i, nentries;
	gchar key[128], *file_entry = NULL, *uri = NULL, *base = NULL;

	base = get_display_filename(file, TRUE);

	plskeyfile = g_key_file_new();

	if (!g_key_file_load_from_file(plskeyfile, file, G_KEY_FILE_NONE, &error)) {
		g_critical("Unable to load pls playlist, err: %s", error->message);
	}
	else {
		nentries = g_key_file_get_integer (plskeyfile, "playlist", "NumberOfEntries", NULL);
		if (nentries > 0) {
			for (i = 1; i <= nentries; i++) {
				g_snprintf (key, 128, "File%d", i);
				file_entry = g_key_file_get_string(plskeyfile, "playlist", key, NULL);
				if (NULL == file_entry)
					continue;

				if (g_path_is_absolute(file_entry))
					uri = g_strdup(file_entry);
				else {
					uri = g_build_filename (base, file_entry, NULL);
				}
				list = g_slist_append (list, uri);
				g_free(file_entry);
			}
		}
	}
	g_key_file_free (plskeyfile);
	g_free(base);

	return list;
}

/* Load a M3U playlist, and add tracks to current playlist */

GSList *
pragha_pl_parser_parse_m3u (const gchar *file)
{
	GError *err = NULL;
	GIOChannel *chan = NULL;
	gint fd;
	gsize len, term;
	gchar *str, *filename, *f_file, *uri, *base;
	GSList *list = NULL;

	fd = g_open(file, O_RDONLY, 0);
	if (fd == -1) {
		g_critical("Unable to open file : %s",
			   file);
		return NULL;
	}

	chan = g_io_channel_unix_new(fd);
	if (!chan) {
		g_critical("Unable to open an IO channel for file: %s", file);
		goto exit_close;
	}

	base = get_display_filename(file, TRUE);

	while (g_io_channel_read_line(chan, &str, &len, &term, &err) ==
	       G_IO_STATUS_NORMAL) {

		if (!str || !len)
			break;

		/* Skip lines containing #EXTM3U or #EXTINF */

		if (g_strrstr(str, "#EXTM3U") || g_strrstr(str, "#EXTINF")) {
			g_free(str);
			continue;
		}

		filename = g_strndup(str, term);

		f_file = g_filename_from_utf8(filename, -1, NULL, NULL, &err);
		if (!f_file) {
			g_warning("Unable to get filename from UTF-8 string: %s",
				  filename);
			g_error_free(err);
			err = NULL;
			goto continue_read;
		}

		if (g_path_is_absolute(f_file))
			uri = g_strdup(f_file);
		else {
			uri = g_build_filename (base, f_file, NULL);
		}
		list = g_slist_append (list, uri);

		while(gtk_events_pending()) {
			if (gtk_main_iteration_do(FALSE)) {
				g_free(filename);
				g_free(f_file);
				g_free(str);
				goto exit_chan;
			}
		}

		g_free(f_file);
	continue_read:
		g_free(filename);
		g_free(str);
	}

	CDEBUG(DBG_INFO, "Loaded M3U playlist: %s", file);

	g_free(base);

exit_chan:
	if (g_io_channel_shutdown(chan, TRUE, &err) != G_IO_STATUS_NORMAL) {
		g_critical("Unable to open M3U playlist: %s", file);
		g_error_free(err);
		err = NULL;
	}
	g_io_channel_unref(chan);

exit_close:
	close(fd);

	return list;
}

GSList *
pragha_pl_parser_parse (enum playlist_type format, const gchar *filename)
{
	GSList *list = NULL;

	switch (format)
	{
	case PL_FORMAT_M3U:
		list = pragha_pl_parser_parse_m3u (filename);
		break;
	case PL_FORMAT_PLS:
		list = pragha_pl_parser_parse_pls (filename);
		break;
	case PL_FORMAT_ASX:
		//list = pragha_pl_parser_parse_asx (filename);
		break;
	case PL_FORMAT_XSPF:
		//list = pragha_pl_parser_parse_xspf (filename);
		break;
	default:
	break;
    }

    return list;
}

GSList *pragha_pl_parser_parse_from_file_by_extension (const gchar *filename)
{
	enum playlist_type format = PL_FORMAT_UNKNOWN;
	GSList *list = NULL;

	if ((format = pragha_pl_parser_guess_format_from_extension (filename)) != PL_FORMAT_UNKNOWN) {
		list = pragha_pl_parser_parse (format, filename);
	}
	else {
		g_debug ("Unable to guess playlist format : %s", filename);
	}

	return list;
}

void pragha_pl_parser_open_from_file_by_extension (gchar *file, struct con_win *cwin)
{
	GSList *list = NULL, *i = NULL;
	gchar *summary;
	gint try = 0, added = 0;
	struct musicobject *mobj;

	list = pragha_pl_parser_parse_from_file_by_extension (file);

	for (i = list; i != NULL; i = i->next) {
		try++;
		mobj = new_musicobject_from_file(i->data);
		if (mobj) {
			added++;
			append_current_playlist(mobj, cwin);
		}
		while(gtk_events_pending()) {
			if (gtk_main_iteration_do(FALSE))
				return;
		}
		g_free(i->data);
	}
	summary = g_strdup_printf(_("Added %d songs from %d of the imported playlist."), added, try);
	set_status_message(summary, cwin);
	g_free(summary);

	g_slist_free(list);
}

/*******/
/* DnD */
/*******/
gboolean dnd_playlist_tree_begin(GtkWidget *widget,
				    GdkDragContext *context,
				    struct con_win *cwin)
{
	cwin->cstate->dragging = TRUE;
	return FALSE;
}

/* Callback for DnD signal 'drag-data-get' */
void dnd_playlist_tree_get(GtkWidget *widget,
			   GdkDragContext *context,
			   GtkSelectionData *data,
			   guint info,
			   guint time,
			   struct con_win *cwin)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, r_iter;
	GtkTreePath *r_path;
	GList *list = NULL, *l;
	GArray *playlist_arr;
	gchar *playlist;

	switch(info) {
	case TARGET_PLAYLIST:
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cwin->playlist_tree));
		list = gtk_tree_selection_get_selected_rows(selection, &model);

		/* No selections */

		if (!list) {
			gtk_selection_data_set(data, data->type, 8, NULL, 0);
			break;
		}

		gtk_tree_model_get_iter_first(model, &r_iter);
		r_path = gtk_tree_model_get_path(model, &r_iter);

		/* Form an array of playlist names */

		playlist_arr = g_array_new(TRUE, TRUE, sizeof(gchar *));
		l = list;

		while(l) {
			if (gtk_tree_path_compare(r_path, l->data)) {
				gtk_tree_model_get_iter(model, &iter, l->data);
				gtk_tree_model_get(model, &iter, P_PLAYLIST, &playlist, -1);
				g_array_append_val(playlist_arr, playlist);
			}
			gtk_tree_path_free(l->data);
			l = l->next;
		}

		gtk_selection_data_set(data,
				       data->type,
				       8,
				       (guchar *)&playlist_arr,
				       sizeof(GArray *));

		CDEBUG(DBG_VERBOSE, "Fill DnD data, "
		       "selection: %p, playlist_arr: %p",
		       data->data, playlist_arr);

		/* Cleanup */

		gtk_tree_path_free(r_path);
		g_list_free(list);

		break;
	default:
		g_warning("Unknown DND type");
	}
}

/* Save tracks to a playlist using the given type */

void save_playlist(gint playlist_id, enum playlist_mgmt type,
		   struct con_win *cwin)
{
	gchar *file;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	struct musicobject *mobj = NULL;
	GList *list, *i;
	gchar *query;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->current_playlist));

	switch(type) {
	case SAVE_COMPLETE:
		query = g_strdup_printf("BEGIN;");
		exec_sqlite_query(query, cwin, NULL);
		gtk_tree_model_get_iter_first(model, &iter);
		do {
			gtk_tree_model_get(model, &iter, P_MOBJ_PTR, &mobj, -1);
			if (G_LIKELY(mobj && mobj->file_type != FILE_CDDA)) {
				file = sanitize_string_sqlite3(mobj->file);
				add_track_playlist_db(file, playlist_id, cwin);
				g_free(file);
			}
		} while(gtk_tree_model_iter_next(model, &iter));
		query = g_strdup_printf("END;");
		exec_sqlite_query(query, cwin, NULL);
		break;
	case SAVE_SELECTED:
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
							cwin->current_playlist));
		list = gtk_tree_selection_get_selected_rows(selection, NULL);

		if (list) {

			/* Add all the rows to the playlist */

			query = g_strdup_printf("BEGIN;");
			exec_sqlite_query(query, cwin, NULL);

			for (i=list; i != NULL; i = i->next) {
				path = i->data;
				gtk_tree_model_get_iter(model, &iter, path);
				gtk_tree_model_get(model, &iter,
						   P_MOBJ_PTR, &mobj, -1);

				if (G_LIKELY(mobj && mobj->file_type != FILE_CDDA)) {
					file = sanitize_string_sqlite3(mobj->file);
					add_track_playlist_db(file, playlist_id,
							      cwin);
					g_free(file);
				}

				gtk_tree_path_free(path);
			}
			g_list_free(list);

			query = g_strdup_printf("END;");
			exec_sqlite_query(query, cwin, NULL);
		}
		break;
	default:
		break;
	}
}

void new_playlist(const gchar *playlist, enum playlist_mgmt type,
		  struct con_win *cwin)
{
	gchar *s_playlist;
	gint playlist_id = 0;

	if (!playlist || !strlen(playlist)) {
		g_warning("Playlist name is NULL");
		return;
	}

	s_playlist = sanitize_string_sqlite3((gchar *)playlist);

	if ((playlist_id = find_playlist_db(s_playlist, cwin))) {
		if (overwrite_existing_playlist(playlist, cwin))
			delete_playlist_db((gchar *)s_playlist, cwin);
		else
			goto exit;
	}

	playlist_id = add_new_playlist_db(s_playlist, cwin);
	save_playlist(playlist_id, type, cwin);

exit:
	g_free(s_playlist);
}

void append_playlist(const gchar *playlist, gint type, struct con_win *cwin)
{
	gchar *s_playlist;
	gint playlist_id;

	if (!playlist || !strlen(playlist)) {
		g_warning("Playlist name is NULL");
		return;
	}

	s_playlist = sanitize_string_sqlite3((gchar *)playlist);
	playlist_id = find_playlist_db(s_playlist, cwin);

	if (!playlist_id) {
		g_warning("Playlist doesn't exist\n");
		goto exit;
	}

	save_playlist(playlist_id, type, cwin);

exit:
	g_free(s_playlist);
}

void append_to_playlist(GtkMenuItem *menuitem, struct con_win *cwin)
{
	const gchar *playlist;

	playlist = gtk_menu_item_get_label (menuitem);

	append_playlist(playlist, SAVE_SELECTED, cwin);
}

void save_to_playlist(GtkMenuItem *menuitem, struct con_win *cwin)
{
	const gchar *playlist;

	playlist = gtk_menu_item_get_label (menuitem);

	new_playlist(playlist, SAVE_COMPLETE, cwin);
}

void complete_add_to_playlist_submenu (struct con_win *cwin)
{
	struct db_result result;
	GtkWidget *submenu, *menuitem;
	gchar *query;
	gint i;
	
	submenu = gtk_menu_new ();

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (gtk_ui_manager_get_widget (cwin->cp_context_menu, "/popup/Add to another playlist")), submenu);

	menuitem = gtk_image_menu_item_new_with_label (_("New playlist"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menuitem), gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU));
	g_signal_connect(menuitem, "activate", G_CALLBACK(save_selected_playlist), cwin);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);

	menuitem = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);

	query = g_strdup_printf ("SELECT NAME FROM PLAYLIST WHERE NAME != \"%s\";", SAVE_PLAYLIST_STATE);

	exec_sqlite_query (query, cwin, &result);

	for_each_result_row (result, i) {
		menuitem = gtk_image_menu_item_new_with_label (result.resultp[i]);
		g_signal_connect (menuitem, "activate", G_CALLBACK(append_to_playlist), cwin);
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
	}

	gtk_widget_show_all (submenu);
	sqlite3_free_table (result.resultp);
}

void complete_save_playlist_submenu (struct con_win *cwin)
{
	struct db_result result;
	GtkWidget *submenu, *menuitem;
	gchar *query;
	gint i;
	
	submenu = gtk_menu_new ();

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (gtk_ui_manager_get_widget (cwin->cp_context_menu, "/popup/Save playlist")), submenu);

	menuitem = gtk_image_menu_item_new_with_label (_("New playlist"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menuitem), gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU));
	g_signal_connect(menuitem, "activate", G_CALLBACK(save_current_playlist), cwin);
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);

	menuitem = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);

	query = g_strdup_printf ("SELECT NAME FROM PLAYLIST WHERE NAME != \"%s\";", SAVE_PLAYLIST_STATE);

	exec_sqlite_query (query, cwin, &result);

	for_each_result_row (result, i) {
		menuitem = gtk_image_menu_item_new_with_label (result.resultp[i]);
		g_signal_connect (menuitem, "activate", G_CALLBACK(save_to_playlist), cwin);
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
	}

	gtk_widget_show_all (submenu);
	sqlite3_free_table (result.resultp);
}

void complete_main_save_playlist_submenu (struct con_win *cwin)
{
	struct db_result result;
	GtkWidget *submenu, *menuitem;
	GtkAccelGroup* accel_group;
	gchar *query;
	gint i;

	submenu = gtk_menu_new ();

	gtk_menu_item_set_submenu (GTK_MENU_ITEM(gtk_ui_manager_get_widget(cwin->bar_context_menu,"/Menubar/EditMenu/Save playlist")), submenu);

	menuitem = gtk_image_menu_item_new_with_label (_("New playlist"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menuitem), gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU));
	g_signal_connect(menuitem, "activate", G_CALLBACK(save_current_playlist), cwin);

	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group(GTK_WINDOW(cwin->mainwindow), accel_group);
	gtk_menu_set_accel_group(GTK_MENU(submenu), accel_group);
	gtk_accel_map_add_entry ("<SubMenu>/New playlist", gdk_keyval_from_name ("s"), GDK_CONTROL_MASK);
	gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), "<SubMenu>/New playlist");

	gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);

	menuitem = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);

	query = g_strdup_printf ("SELECT NAME FROM PLAYLIST WHERE NAME != \"%s\";", SAVE_PLAYLIST_STATE);

	exec_sqlite_query (query, cwin, &result);

	for_each_result_row (result, i) {
		menuitem = gtk_image_menu_item_new_with_label (result.resultp[i]);
		g_signal_connect (menuitem, "activate", G_CALLBACK(save_to_playlist), cwin);
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
	}

	gtk_widget_show_all (submenu);
	sqlite3_free_table (result.resultp);
}

void complete_main_add_to_playlist_submenu (struct con_win *cwin)
{
	struct db_result result;
	GtkWidget *submenu, *menuitem;
	GtkAccelGroup* accel_group;
	gchar *query;
	gint i;
	
	submenu = gtk_menu_new ();

	gtk_menu_item_set_submenu (GTK_MENU_ITEM(gtk_ui_manager_get_widget(cwin->bar_context_menu,"/Menubar/EditMenu/Add to another playlist")), submenu);

	menuitem = gtk_image_menu_item_new_with_label (_("New playlist"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM(menuitem), gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU));
	g_signal_connect(menuitem, "activate", G_CALLBACK(save_selected_playlist), cwin);

	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group(GTK_WINDOW(cwin->mainwindow), accel_group);
	gtk_menu_set_accel_group(GTK_MENU(submenu), accel_group);
	gtk_accel_map_add_entry ("<SubMenu>/Add to another playlist", gdk_keyval_from_name ("s"), GDK_CONTROL_MASK+GDK_SHIFT_MASK);
	gtk_menu_item_set_accel_path (GTK_MENU_ITEM(menuitem), "<SubMenu>/Add to another playlist");

	gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);

	menuitem = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL(submenu), menuitem);

	query = g_strdup_printf ("SELECT NAME FROM PLAYLIST WHERE NAME != \"%s\";", SAVE_PLAYLIST_STATE);

	exec_sqlite_query (query, cwin, &result);

	for_each_result_row (result, i) {
		menuitem = gtk_image_menu_item_new_with_label (result.resultp[i]);
		g_signal_connect (menuitem, "activate", G_CALLBACK(save_to_playlist), cwin);
		gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
	}

	gtk_widget_show_all (submenu);
	sqlite3_free_table (result.resultp);
}

void init_playlist_view(struct con_win *cwin)
{
	gint i = 0;
	gchar *query;
	struct db_result result;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GdkCursor *cursor;

	cwin->cstate->view_change = TRUE;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->playlist_tree));

	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor (GDK_WINDOW(cwin->mainwindow->window), cursor);
	gdk_cursor_unref(cursor);

	gtk_tree_store_clear(GTK_TREE_STORE(model));

	gtk_tree_store_append(GTK_TREE_STORE(model),
			      &iter,
			      NULL);
	gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
			   P_PIXBUF, cwin->pixbuf->pixbuf_dir,
			   P_PLAYLIST, _("Playlists"),
			   -1);

	/* Query and insert entries */

	query = g_strdup_printf("SELECT NAME FROM PLAYLIST WHERE NAME != \"%s\";",
				SAVE_PLAYLIST_STATE);

	exec_sqlite_query(query, cwin, &result);

	for_each_result_row(result, i) {
		add_entry_playlist(result.resultp[i],
				   &iter, model, cwin);

		/* Have to give control to GTK periodically ... */
		/* If gtk_main_quit has been called, return -
		   since main loop is no more. */

		while(gtk_events_pending()) {
			if (gtk_main_iteration_do(FALSE)) {
				sqlite3_free_table(result.resultp);
				return;
			}
		}
	}

	sqlite3_free_table(result.resultp);

	gtk_tree_view_expand_all(GTK_TREE_VIEW(cwin->playlist_tree));

	gdk_window_set_cursor(GDK_WINDOW(cwin->mainwindow->window), NULL);

	complete_add_to_playlist_submenu (cwin);
	complete_save_playlist_submenu (cwin);
	complete_main_save_playlist_submenu(cwin);
	complete_main_add_to_playlist_submenu (cwin);

	cwin->cstate->view_change = FALSE;
}
