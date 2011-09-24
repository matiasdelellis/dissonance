/*************************************************************************/
/* Copyright (C) 2007-2009 sujith <m.sujith@gmail.com>			 */
/* Copyright (C) 2009-2010 matias <mati86dl@gmail.com>			 */
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

#include <string.h>
#include <fcntl.h>
#include "pragha.h"

const gchar *mime_mpeg[] = {"audio/mpeg", NULL};
const gchar *mime_wav[] = {"audio/x-wav", NULL};
const gchar *mime_flac[] = {"audio/x-flac", NULL};
const gchar *mime_ogg[] = {"audio/x-vorbis+ogg", "audio/ogg",
			   "application/ogg", NULL};
const gchar *mime_modplug[] = {"audio/x-mod", "audio/x-xm", NULL};
const gchar *mime_image[] = {"image/jpeg", "image/png", NULL};

/* Set a message on status bar, and restore it at 5 seconds */

gboolean restore_status_bar(gpointer data)
{
	struct con_win *cwin = data;

	update_status_bar(cwin);

	return FALSE;
}

void set_status_message (gchar *message, struct con_win *cwin)
{
	g_timeout_add_seconds(5, restore_status_bar, cwin);

	gtk_label_set_text(GTK_LABEL(cwin->status_bar), message);
}

/* Obtain Pixbuf of lastfm. Based on Amatory code. */

GdkPixbuf *vgdk_pixbuf_new_from_memory(char *data, size_t size)
{
	GInputStream *buffer_stream=NULL;
	GdkPixbuf *buffer_pix=NULL;
	GError *err = NULL;

	buffer_stream = g_memory_input_stream_new_from_data (data, size, NULL);
	
	buffer_pix = gdk_pixbuf_new_from_stream(buffer_stream, NULL, &err);
	g_input_stream_close(buffer_stream, NULL, NULL);
	g_object_unref(buffer_stream);

	if(buffer_pix == NULL){
		g_warning("vgdk_pixbuf_new_from_memory: %s\n",err->message);
		g_error_free (err);	
	}
	return buffer_pix;
}

/* Based in Midori Web Browser. Copyright (C) 2007 Christian Dywan */
gpointer sokoke_xfce_header_new(const gchar* header, const gchar *icon, struct con_win *cwin)
{
	GtkWidget* entry;
	GtkWidget* xfce_heading;
	GtkWidget* hbox;
	GtkWidget* vbox;
	GtkWidget* image;
	GtkWidget* label;
	GtkWidget* separator;
	gchar* markup;

	entry = gtk_entry_new();
	xfce_heading = gtk_event_box_new();

	gtk_widget_modify_bg(xfce_heading,
				GTK_STATE_NORMAL,
				&entry->style->base[GTK_STATE_NORMAL]);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);

        if (icon)
            image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_DIALOG);
        else
            image = gtk_image_new_from_stock (GTK_STOCK_INFO, GTK_ICON_SIZE_DIALOG);

	label = gtk_label_new(NULL);
	gtk_widget_modify_fg(label,
				GTK_STATE_NORMAL,
				&entry->style->text[GTK_STATE_NORMAL]);
        markup = g_strdup_printf("<span size='large' weight='bold'>%s</span>", header);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	gtk_widget_destroy (entry);

	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(xfce_heading), hbox);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), xfce_heading, FALSE, FALSE, 0);

	separator = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);

	return vbox;
}

/* Accepts only absolute filename */

gboolean is_playable_file(const gchar *file)
{
	if (!file)
		return FALSE;

	if (g_file_test(file, G_FILE_TEST_IS_REGULAR) &&
	    (get_file_type((gchar*)file) != -1))
		return TRUE;
	else
		return FALSE;
}

/* Accepts only absolute path */

gboolean is_dir_and_accessible(gchar *dir, struct con_win *cwin)
{
	gint ret;

	if (!dir)
		return FALSE;

	if (g_file_test(dir, G_FILE_TEST_IS_DIR) && !g_access(dir, R_OK | X_OK))
		ret = TRUE;
	else
		ret = FALSE;

	return ret;
}

gint dir_file_count(gchar *dir_name, gint call_recur)
{
	static gint file_count = 0;
	GDir *dir;
	const gchar *next_file = NULL;
	gchar *ab_file;
	GError *error = NULL;

	/* Reinitialize static variable if called from rescan_library_action */

	if (call_recur)
		file_count = 0;

	dir = g_dir_open(dir_name, 0, &error);
	if (!dir) {
		g_warning("Unable to open library : %s", dir_name);
		return file_count;
	}

	next_file = g_dir_read_name(dir);
	while (next_file) {
		ab_file = g_strconcat(dir_name, "/", next_file, NULL);
		if (g_file_test(ab_file, G_FILE_TEST_IS_DIR))
			dir_file_count(ab_file, 0);
		else {
			file_count++;
		}
		g_free(ab_file);
		next_file = g_dir_read_name(dir);
	}

	g_dir_close(dir);
	return file_count;
}

static gint no_single_quote(gchar *str)
{
	gchar *tmp = str;
	gint i = 0;

	if (!str)
		return 0;

	while (*tmp) {
		if (*tmp == '\'') {
			i++;
		}
		tmp++;
	}
	return i;
}

/* Replace ' by '' */

gchar* sanitize_string_sqlite3(gchar *str)
{
	gint cn, i=0;
	gchar *ch;
	gchar *tmp;

	if (!str)
		return NULL;

	cn = no_single_quote(str);
	ch = g_malloc0(strlen(str) + cn + 1);
	tmp = str;

	while (*tmp) {
		if (*tmp == '\'') {
			ch[i++] = '\'';
			ch[i++] = '\'';
			tmp++;
			continue;
		}
		ch[i++] = *tmp++;
	}
	return ch;
}

static gboolean is_valid_mime(gchar *mime, const gchar **mlist)
{
	gint i=0;

	while (mlist[i]) {
		if (g_content_type_equals(mime, mlist[i]))
			return TRUE;
		i++;
	}

	return FALSE;
}

/* Accepts only absolute filename */
/* NB: Disregarding 'uncertain' flag for now. */

enum file_type
get_file_type(gchar *file)
{
	gint ret = -1;
	gchar *result = NULL;

	if (!file)
		return -1;

	result = get_mime_type(file);

	if (result) {
		if (is_valid_mime(result, mime_wav))
			ret = FILE_WAV;
		else if(is_valid_mime(result, mime_mpeg))
			ret = FILE_MP3;
		else if(is_valid_mime(result, mime_flac))
			ret = FILE_FLAC;
		else if(is_valid_mime(result, mime_ogg))
			ret = FILE_OGGVORBIS;
		else if(is_valid_mime(result, mime_modplug))
			ret = FILE_MODPLUG;
		else ret = -1;
	}

	g_free(result);
	return ret;
}

gchar* get_mime_type(gchar *file)
{
	gboolean uncertain;
	gchar *result = NULL;

	result = g_content_type_guess((const gchar *)file, NULL, 0, &uncertain);

	return result;
}

enum playlist_type
pragha_pl_parser_guess_format_from_extension (const gchar *filename)
{
	if ( g_str_has_suffix (filename, ".m3u") || g_str_has_suffix (filename, ".M3U") )
		return PL_FORMAT_M3U;

	if ( g_str_has_suffix (filename, ".pls") || g_str_has_suffix (filename, ".PLS") )
		return PL_FORMAT_PLS;

	if ( g_str_has_suffix (filename, ".xspf") || g_str_has_suffix (filename, ".XSPF") )
		return PL_FORMAT_XSPF;

	if ( g_str_has_suffix (filename, ".asx") || g_str_has_suffix (filename, ".ASX") )
		return PL_FORMAT_ASX;

	if ( g_str_has_suffix (filename, ".wax") || g_str_has_suffix (filename, ".WAX") )
		return PL_FORMAT_XSPF;

	return PL_FORMAT_UNKNOWN;
}

/* Return true if given file is an image */

gboolean is_image_file(gchar *file)
{
	gboolean uncertain = FALSE, ret = FALSE;
	gchar *result = NULL;

	if (!file)
		return FALSE;

	/* Type: JPG, PNG */

	result = g_content_type_guess((const gchar*)file, NULL, 0, &uncertain);

	if (!result)
		return FALSE;
	else {
		ret = is_valid_mime(result, mime_image);
		g_free(result);
		return ret;
	}
}

/* NB: Have to take care of longer lengths .. */

gchar* convert_length_str(gint length)
{
	static gchar *str, tmp[24];
	gint days = 0, hours = 0, minutes = 0, seconds = 0;

	str = g_new0(char, 128);
	memset(tmp, '\0', 24);

	if (length > 86400) {
		days = length/86400;
		length = length%86400;
		g_sprintf(tmp, "%d %s, ", days, (days>1)?_("days"):_("day"));
		g_strlcat(str, tmp, 24);
	}

	if (length > 3600) {
		hours = length/3600;
		length = length%3600;
		memset(tmp, '\0', 24);
		g_sprintf(tmp, "%d:", hours);
		g_strlcat(str, tmp, 24);
	}

	if (length > 60) {
		minutes = length/60;
		length = length%60;
		memset(tmp, '\0', 24);
		g_sprintf(tmp, "%02d:", minutes);
		g_strlcat(str, tmp, 24);
	}
	else
		g_strlcat(str, "00:", 4);

	seconds = length;
	memset(tmp, '\0', 24);
	g_sprintf(tmp, "%02d", seconds);
	g_strlcat(str, tmp, 24);

	return str;
}

/* Check if str is present in list ( containing gchar* elements in 'data' ) */

gboolean is_present_str_list(const gchar *str, GSList *list)
{
	GSList *i;
	gchar *lstr;
	gboolean ret = FALSE;

	if (list) {
		for (i=list; i != NULL; i = i->next) {
			lstr = i->data;
			if (!g_ascii_strcasecmp(str, lstr)) {
				ret = TRUE;
				break;
			}
		}
	}
	else {
		ret = FALSE;
	}

	return ret;
}

/* Delete str from list */

GSList* delete_from_str_list(const gchar *str, GSList *list)
{
	GSList *i = NULL;
	gchar *lstr;

	if (!list)
		return NULL;

	for (i = list; i != NULL; i = i->next) {
		lstr = i->data;
		if (!g_ascii_strcasecmp(str, lstr)) {
			g_free(i->data);
			return g_slist_delete_link(list, i);
		}
	}

	return list;
}

/* Returns either the basename of the given filename, or (if the parameter 
 * get_folder is set) the basename of the container folder of filename. In both
 * cases the returned string is encoded in utf-8 format. If GLib can not make
 * sense of the encoding of filename, as a last resort it replaces unknown
 * characters with U+FFFD, the Unicode replacement character */

gchar* get_display_filename(const gchar *filename, gboolean get_folder)
{
	gchar *utf8_filename = NULL;
	gchar *dir = NULL;

	/* Get the containing folder of the file or the file itself ? */
	if (get_folder) {
		dir = g_path_get_dirname(filename);
		utf8_filename = g_filename_display_name(dir);
		g_free(dir);
	}
	else {
		utf8_filename = g_filename_display_basename(filename);
	}
	return utf8_filename;
}

/* Free a list of strings */

void free_str_list(GSList *list)
{
	gint cnt = 0, i;
	GSList *l = list;

	cnt = g_slist_length(list);

	for (i=0; i<cnt; i++) {
		g_free(l->data);
		l = l->next;
	}

	g_slist_free(list);
}

/* Compare two UTF-8 strings */

gint compare_utf8_str(gchar *str1, gchar *str2)
{
	gchar *key1, *key2;
	gint ret = 0;

	if (!str1)
		return 1;

	if (!str2)
		return -1;

	key1 = g_utf8_collate_key(str1, -1);
	key2 = g_utf8_collate_key(str2, -1);

	ret = strcmp(key1, key2);

	g_free(key1);
	g_free(key2);

	return ret;
}

gboolean validate_album_art_pattern(const gchar *pattern)
{
	gchar **tokens;
	gint i = 0;
	gboolean ret = FALSE;

	if (!pattern || (pattern && !strlen(pattern)))
		return TRUE;

	if (g_strrstr(pattern, "*")) {
		g_warning("Contains wildcards");
		return FALSE;
	}

	tokens = g_strsplit(pattern, ";", 0);
	while (tokens[i]) i++;

	/* Check if more than six patterns are given */

	if (i > ALBUM_ART_NO_PATTERNS) {
		g_warning("More than six patterns");
		goto exit;
	}

	ret = TRUE;
exit:
	if (tokens)
		g_strfreev(tokens);

	return ret;
}

/* callback used to open default browser when URLs got clicked */
void open_url(struct con_win *cwin, const gchar *url)
{
	gboolean success = TRUE;
	const gchar *argv[3];
	gchar *methods[] = {"xdg-open","firefox","mozilla","opera","konqueror",NULL};
	int i = 0;
			
	/* First try gtk_show_uri() (will fail if gvfs is not installed) */
	if (!gtk_show_uri (NULL, url,  gtk_get_current_event_time (), NULL)) {
		success = FALSE;
		argv[1] = url;
		argv[2] = NULL;
		/* Next try all available methods for opening the URL */
		while (methods[i] != NULL) {
			argv[0] = methods[i++];
			if (g_spawn_async(NULL, (gchar**)argv, NULL, G_SPAWN_SEARCH_PATH,
				NULL, NULL, NULL, NULL)) {
				success = TRUE;
				break;
			}
		}
	}
	/* No method was found to open the URL */
	if (!success) {
		GtkWidget *d;
		d = gtk_message_dialog_new (GTK_WINDOW (cwin->mainwindow), 
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
					"%s", _("Unable to open the browser"));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (d),
							 "%s", "No methods supported");
		g_signal_connect (d, "response", G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_window_present (GTK_WINDOW (d));
	}
}

/* It gives the position of the menu on the 
   basis of the position of combo_order */

void
menu_position(GtkMenu *menu,
		gint *x, gint *y,
		gboolean *push_in,
		gpointer user_data)
{
        GtkWidget *widget;
        GtkRequisition requisition;
        gint menu_xpos;
        gint menu_ypos;

        widget = GTK_WIDGET (user_data);

        gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

        gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);

        menu_xpos += widget->allocation.x;
        menu_ypos += widget->allocation.y;

	if (menu_ypos > gdk_screen_get_height (gtk_widget_get_screen (widget)) / 2)
		menu_ypos -= requisition.height + widget->style->ythickness;
	else
		menu_ypos += widget->allocation.height + widget->style->ythickness;

        *x = menu_xpos;
        *y = menu_ypos - 5;

        *push_in = TRUE;
}

/* Return TRUE if the previous installed version is
   incompatible with the current one */

gboolean is_incompatible_upgrade(struct con_win *cwin)
{
	/* Lesser than 0.2, version string is non-existent */

	if (!cwin->cpref->installed_version)
		return TRUE;

	if (atof(cwin->cpref->installed_version) < atof(PACKAGE_VERSION))
		return TRUE;

	return FALSE;
}
