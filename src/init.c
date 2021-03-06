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

static gchar *audio_backend = NULL;
static gchar *audio_device = NULL;
static gchar *audio_mixer = NULL;

GOptionEntry cmd_entries[] = {
	{"version", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_version, "Version", NULL},
	{"debug", 'e', 0, G_OPTION_ARG_INT,
	 &debug_level, "Enable Debug ( Levels: 1,2,3,4 )", NULL},
	{"play", 'p', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_play, "Play", NULL},
	{"stop", 's', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_stop, "Stop", NULL},
	{"pause", 't', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_pause, "Play/Pause/Resume", NULL},
	{"prev", 'r', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_prev, "Prev", NULL},
	{"next", 'n', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_next, "Next", NULL},
	{"shuffle", 'f', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_shuffle, "Shuffle", NULL},
	{"repeat", 'u', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_repeat, "Repeat", NULL},
	{"inc_vol", 'i', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_inc_volume, "Increase volume by 1", NULL},
	{"dec_vol", 'd', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_dec_volume, "Decrease volume by 1", NULL},
	{"show_osd", 'o', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_show_osd, "Show OSD notification", NULL},
	{"toggle_view", 'x', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_toggle_view, "Toggle player visibility", NULL},
	{"current_state", 'c', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	 cmd_current_state, "Get current player state", NULL},
	{"audio_backend", 'a', 0, G_OPTION_ARG_STRING,
	 &audio_backend, "Audio backend (valid options: alsa/oss)", NULL},
	{"audio_device", 'g', 0, G_OPTION_ARG_STRING,
	 &audio_device, "Audio Device (For ALSA: hw:0,0 etc.., For OSS: /dev/dsp etc..)", NULL},
	{"audio_mixer", 'm', 0, G_OPTION_ARG_STRING,
	 &audio_mixer, "Mixer Element (For ALSA: Master, PCM, etc.., For OSS: /dev/mixer, etc...)", NULL},
	{G_OPTION_REMAINING, 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK,
	 cmd_add_file, "", "[URI1 [URI2...]]"},
	{NULL}
};

static gboolean _init_gui_state(gpointer data)
{
	struct con_win *cwin = data;

	if (gtk_main_iteration_do(FALSE))
		return TRUE;
	init_playlist_view(cwin);

	if (gtk_main_iteration_do(FALSE))
		return TRUE;
	init_tag_completion(cwin);

	if (gtk_main_iteration_do(FALSE))
		return TRUE;
	init_library_view(cwin);

	if (gtk_main_iteration_do(FALSE))
		return TRUE;
	if (cwin->cpref->save_playlist)
		init_current_playlist_view(cwin);

	if (gtk_main_iteration_do(FALSE))
		return TRUE;
	if (cwin->cpref->lw.lastfm_support)
		lastfm_init_thread(cwin);

	return TRUE;
}

static gint init_audio_mixer(struct con_win *cwin)
{
	if (!g_ascii_strcasecmp(ao_driver_info(cwin->clibao->ao_driver_id)->short_name,
				ALSA_SINK))
		set_alsa_mixer(cwin, audio_mixer);
	else if (!g_ascii_strcasecmp(ao_driver_info(cwin->clibao->ao_driver_id)->short_name,
				     OSS_SINK))
		set_oss_mixer(cwin, audio_mixer);
	else
		return -1;

	return 0;
}

gint init_dbus(struct con_win *cwin)
{
	DBusConnection *conn = NULL;
	DBusError error;
	gint ret = 0;

	CDEBUG(DBG_INFO, "Initializing DBUS");

	dbus_error_init(&error);
	conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (!conn) {
		g_critical("Unable to get a DBUS connection");
		dbus_error_free(&error);
		return -1;
	}

	ret = dbus_bus_request_name(conn, DBUS_NAME, 0, &error);
	if (ret == -1) {
		g_critical("Unable to request for DBUS service name");
		dbus_error_free(&error);
		return -1;
	}

	if (ret & DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		cwin->cstate->unique_instance = TRUE;
	else if (ret & DBUS_REQUEST_NAME_REPLY_EXISTS)
		cwin->cstate->unique_instance = FALSE;

	dbus_connection_setup_with_g_main(conn, NULL);
	cwin->con_dbus = conn;

	return 0;
}

gint init_dbus_handlers(struct con_win *cwin)
{
	DBusError error;

	dbus_error_init(&error);
	if (!cwin->con_dbus) {
		g_critical("No DBUS connection");
		return -1;
	}

	dbus_bus_add_match(cwin->con_dbus,
			   "type='signal',path='/org/pragha/DBus'",
			   &error);
	if (dbus_error_is_set(&error)) {
		g_critical("Unable to register match rule for DBUS");
		dbus_error_free(&error);
		return -1;
	}

	if (!dbus_connection_add_filter(cwin->con_dbus, dbus_filter_handler, cwin, NULL)) {
		g_critical("Unable to allocate memory for DBUS filter");
		return -1;
	}

	return 0;
}

gint init_options(struct con_win *cwin, int argc, char **argv)
{
	GError *error = NULL;
	GOptionContext *context;
	GOptionGroup *group;

	CDEBUG(DBG_INFO, "Initializing Command line options");

	context = g_option_context_new("- A lightweight music player");
	group = g_option_group_new("General", "General", "General Options", cwin, NULL);
	g_option_group_add_entries(group, cmd_entries);
	g_option_context_set_main_group(context, group);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		gchar *txt;

		g_message("Unable to parse options. Some options need another instance of pragha running.");
		txt = g_option_context_get_help(context, TRUE, NULL);
		g_print("%s", txt);
		g_free(txt);
		g_option_context_free(context);
		g_error_free(error);
		return -1;
	}
	
	/* Store reference */

	cwin->cmd_context = context;
	return 0;
}

gint init_config(struct con_win *cwin)
{
	GError *error = NULL;
	gint *col_widths, *win_size, *win_position;
	gchar *conrc, *condir, **libs, **columns, **nodes, *last_rescan_time;
	gchar *u_file;
	const gchar *config_dir;
	gboolean err = FALSE;
	gsize cnt = 0, i;

	gboolean last_folder_f, recursively_f, album_art_pattern_f, timer_remaining_mode_f, close_to_tray_f, lastfm_f;
	gboolean save_playlist_f, shuffle_f,repeat_f, columns_f, col_widths_f;
	gboolean libs_f, lib_add_f, lib_delete_f, nodes_f, cur_lib_view_f, fuse_folders_f, sort_by_year_f;
	gboolean audio_sink_f, audio_alsa_device_f, audio_oss_device_f, software_mixer_f, use_cddb_f;
	gboolean remember_window_state_f, start_mode_f, window_size_f, window_position_f, sidebar_size_f, sidebar_pane_f, album_f, album_art_size_f, controls_below_f, status_bar_f;
	gboolean show_osd_f, osd_in_systray_f, albumart_in_osd_f, actions_in_osd_f;
	gboolean instant_filter_f, use_hint_f;
	gboolean all_f;
#if HAVE_GLIB_2_26
	gboolean use_mpris2_f;
#endif

	CDEBUG(DBG_INFO, "Initializing configuration");

	last_folder_f = recursively_f = album_art_pattern_f = timer_remaining_mode_f = close_to_tray_f = lastfm_f = FALSE;
	save_playlist_f = shuffle_f = repeat_f = columns_f = col_widths_f = FALSE;
	libs_f = lib_add_f = lib_delete_f = nodes_f = cur_lib_view_f = fuse_folders_f = sort_by_year_f = FALSE;
	audio_sink_f = audio_alsa_device_f = audio_oss_device_f = software_mixer_f = use_cddb_f = FALSE;
	remember_window_state_f = start_mode_f = window_size_f = window_position_f = sidebar_size_f = sidebar_pane_f = album_f = album_art_size_f = controls_below_f = status_bar_f = FALSE;
	instant_filter_f = use_hint_f = FALSE;
	#ifdef HAVE_LIBGLYR
	gboolean get_album_art_f = FALSE;
	gchar *cache_folder = NULL;
	#endif
	#if HAVE_GLIB_2_26
	use_mpris2_f = FALSE;
	#endif
	all_f = FALSE;

	config_dir = g_get_user_config_dir();
	condir = g_strdup_printf("%s%s", config_dir, "/pragha");
	conrc = g_strdup_printf("%s%s", config_dir, "/pragha/config");

	/* Does .config/pragha exist ? */

	if (g_file_test(condir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) == FALSE) {
		if (g_mkdir(condir, S_IRWXU) == -1) {
			g_critical("Unable to create preferences directory, err: %s",
				   strerror(errno));
			err = TRUE;
		}
		CDEBUG(DBG_INFO, "Created .config/pragha");
	}

	cwin->cpref->configrc_file = g_strdup(conrc);
	cwin->cpref->configrc_keyfile = g_key_file_new();

	/* Does conrc exist ? */

	if (g_file_test(conrc, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR) == FALSE) {
		if (g_creat(conrc, S_IRWXU) == -1) {
			g_critical("Unable to create config file, err: %s",
				   strerror(errno));
			err = TRUE;
		}
		CDEBUG(DBG_INFO, "Created config file");
	}

	/* Get cache of downloaded albums arts */
	#ifdef HAVE_LIBGLYR
	cache_folder = g_strdup_printf("%s/pragha",
					g_get_user_cache_dir());

	if (g_file_test(cache_folder, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) == FALSE)
		g_mkdir(cache_folder, S_IRWXU);
	cwin->cpref->cache_folder = g_strdup(cache_folder);
	#endif


	/* Load the settings file */

	if (!g_key_file_load_from_file(cwin->cpref->configrc_keyfile,
				       conrc,
				       G_KEY_FILE_NONE,
				       &error)) {
		g_critical("Unable to load config file (Possible first start), err: %s", error->message);
		g_error_free(error);
		all_f = TRUE;
	}
	else {
		/* Retrieve version */

		cwin->cpref->installed_version =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_GENERAL,
					      KEY_INSTALLED_VERSION,
					      &error);
		if (!cwin->cpref->installed_version) {
			g_error_free(error);
			error = NULL;
		}

		/* Retrieve Window preferences */

		win_size = g_key_file_get_integer_list(cwin->cpref->configrc_keyfile,
						       GROUP_WINDOW,
						       KEY_WINDOW_SIZE,
						       &cnt,
						       &error);
		if (win_size) {
			cwin->cpref->window_width = MAX(win_size[0], MIN_WINDOW_WIDTH);
			cwin->cpref->window_height = MAX(win_size[1], MIN_WINDOW_HEIGHT);
			g_free(win_size);
		}
		else {
			g_error_free(error);
			error = NULL;
			window_size_f = TRUE;
		}

		win_position = g_key_file_get_integer_list(cwin->cpref->configrc_keyfile,
							   GROUP_WINDOW,
							   KEY_WINDOW_POSITION,
							   &cnt,
							   &error);
		if (win_position) {
			cwin->cpref->window_x = win_position[0];
			cwin->cpref->window_y = win_position[1];
			g_free(win_position);
		}
		else {
			g_error_free(error);
			error = NULL;
			window_position_f = TRUE;
		}

		cwin->cpref->status_bar =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_WINDOW,
					       KEY_STATUS_BAR,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			status_bar_f = TRUE;
		}

		cwin->cpref->controls_below =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_WINDOW,
					       KEY_CONTROLS_BELOW,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			controls_below_f = TRUE;
		}

		cwin->cpref->sidebar_size = g_key_file_get_integer(cwin->cpref->configrc_keyfile,
								   GROUP_WINDOW,
								   KEY_SIDEBAR_SIZE,
								   &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			sidebar_size_f = TRUE;
		}

		cwin->cpref->sidebar_pane =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_WINDOW,
					      KEY_SIDEBAR,
					      &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			sidebar_pane_f = TRUE;
		}

		/* Retrieve Audio preferences */

		cwin->cpref->audio_sink =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_AUDIO,
					      KEY_AUDIO_SINK,
					      &error);
		if (!cwin->cpref->audio_sink) {
			g_error_free(error);
			error = NULL;
			audio_sink_f = TRUE;
		}

		cwin->cpref->audio_alsa_device =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_AUDIO,
					      KEY_AUDIO_ALSA_DEVICE,
					      &error);
		if (!cwin->cpref->audio_alsa_device) {
			g_error_free(error);
			error = NULL;
			audio_alsa_device_f = TRUE;
		}

		cwin->cpref->audio_oss_device =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_AUDIO,
					      KEY_AUDIO_OSS_DEVICE,
					      &error);
		if (!cwin->cpref->audio_oss_device) {
			g_error_free(error);
			error = NULL;
			audio_oss_device_f = TRUE;
		}

		cwin->cpref->audio_cd_device =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_AUDIO,
					      KEY_AUDIO_CD_DEVICE,
					      &error);
		if (!cwin->cpref->audio_cd_device) {
			g_error_free(error);
			error = NULL;
		}

		cwin->cpref->software_mixer =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_AUDIO,
					       KEY_SOFTWARE_MIXER,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			software_mixer_f = TRUE;
		}

		/* Retrieve Collection preferences */

		libs = g_key_file_get_string_list(cwin->cpref->configrc_keyfile,
						  GROUP_LIBRARY,
						  KEY_LIBRARY_DIR,
						  &cnt,
						  &error);
		if (libs) {
			for (i=0; i<cnt; i++) {
				gchar *file = g_filename_from_utf8(libs[i],
						   -1, NULL, NULL, &error);
				if (!file) {
					g_warning("Unable to get filename "
						  "from UTF-8 string: %s",
						  libs[i]);
					g_error_free(error);
					error = NULL;
					continue;
				}
				cwin->cpref->library_dir =
					g_slist_append(cwin->cpref->library_dir,
						       g_strdup(file));
				g_free(file);
			}
			g_strfreev(libs);
		}
		else {
			g_error_free(error);
			error = NULL;
			libs_f = TRUE;
		}

		columns = g_key_file_get_string_list(cwin->cpref->configrc_keyfile,
						     GROUP_PLAYLIST,
						     KEY_PLAYLIST_COLUMNS,
						     &cnt,
						     &error);
		if (columns) {
			for (i=0; i<cnt; i++) {
				cwin->cpref->playlist_columns =
					g_slist_append(cwin->cpref->playlist_columns,
						       g_strdup(columns[i]));
			}
			g_strfreev(columns);
		}
		else {
			g_error_free(error);
			error = NULL;
			columns_f = TRUE;
		}

		col_widths = g_key_file_get_integer_list(cwin->cpref->configrc_keyfile,
							 GROUP_PLAYLIST,
							 KEY_PLAYLIST_COLUMN_WIDTHS,
							 &cnt,
							 &error);
		if (col_widths) {
			for (i = 0; i < cnt; i++) {
				cwin->cpref->playlist_column_widths =
					g_slist_append(cwin->cpref->playlist_column_widths,
						       GINT_TO_POINTER(col_widths[i]));
			}
			g_free(col_widths);
		}
		else {
			g_error_free(error);
			error = NULL;
			col_widths_f = TRUE;
		}

		nodes = g_key_file_get_string_list(cwin->cpref->configrc_keyfile,
						   GROUP_LIBRARY,
						   KEY_LIBRARY_TREE_NODES,
						   &cnt,
						   &error);
		if (nodes) {
			for (i=0; i<cnt; i++) {
				cwin->cpref->library_tree_nodes =
					g_slist_append(cwin->cpref->library_tree_nodes,
						       g_strdup(nodes[i]));
			}
			g_strfreev(nodes);
		}
		else {
			g_error_free(error);
			error = NULL;
			nodes_f = TRUE;
		}

		cwin->cpref->cur_library_view =
			g_key_file_get_integer(cwin->cpref->configrc_keyfile,
					       GROUP_LIBRARY,
					       KEY_LIBRARY_VIEW_ORDER,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			cur_lib_view_f = TRUE;
		}

		last_rescan_time =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_LIBRARY,
					      KEY_LIBRARY_LAST_SCANNED,
					      &error);
		if (!last_rescan_time) {
			g_error_free(error);
			error = NULL;
		} else {
			if (!g_time_val_from_iso8601(last_rescan_time,
						     &cwin->cpref->last_rescan_time))
				g_warning("Unable to convert last rescan time");
			g_free(last_rescan_time);
		}

		libs = g_key_file_get_string_list(cwin->cpref->configrc_keyfile,
						  GROUP_LIBRARY,
						  KEY_LIBRARY_ADD,
						  &cnt,
						  &error);
		if (libs) {
			for (i = 0; i < cnt; i++) {
				gchar *file = g_filename_from_utf8(libs[i],
						   -1, NULL, NULL, &error);
				if (!file) {
					g_warning("Unable to get filename "
						  "from UTF-8 string: %s",
						  libs[i]);
					g_error_free(error);
					error = NULL;
					continue;
				}
				cwin->cpref->lib_add =
					g_slist_append(cwin->cpref->lib_add,
						       g_strdup(file));
				g_free(file);
			}
			g_strfreev(libs);
		}
		else {
			g_error_free(error);
			error = NULL;
			lib_add_f = TRUE;
		}

		libs = g_key_file_get_string_list(cwin->cpref->configrc_keyfile,
						  GROUP_LIBRARY,
						  KEY_LIBRARY_DELETE,
						  &cnt,
						  &error);
		if (libs) {
			for (i = 0; i < cnt; i++) {
				gchar *file = g_filename_from_utf8(libs[i],
						   -1, NULL, NULL, &error);
				if (!file) {
					g_warning("Unable to get filename "
						  "from UTF-8 string: %s",
						  libs[i]);
					g_error_free(error);
					error = NULL;
					continue;
				}
				cwin->cpref->lib_delete =
					g_slist_append(cwin->cpref->lib_delete,
						       g_strdup(file));
				g_free(file);
			}
			g_strfreev(libs);
		}
		else {
			g_error_free(error);
			error = NULL;
			lib_delete_f = TRUE;
		}

		cwin->cpref->fuse_folders = 
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_LIBRARY,
					       KEY_FUSE_FOLDERS,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			fuse_folders_f = TRUE;
		}

		cwin->cpref->sort_by_year = 
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_LIBRARY,
					       KEY_SORT_BY_YEAR,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			sort_by_year_f = TRUE;
		}

		/* Retrieve General preferences */

		cwin->cpref->remember_window_state =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_WINDOW,
					       KEY_REMEMBER_STATE,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			remember_window_state_f = TRUE;
		}

		cwin->cpref->start_mode =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_WINDOW,
					      KEY_START_MODE,
					      &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			start_mode_f = TRUE;
		}

		cwin->cpref->save_playlist =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_SAVE_PLAYLIST,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			save_playlist_f = TRUE;
		}

		cwin->cpref->close_to_tray =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_CLOSE_TO_TRAY,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			close_to_tray_f = TRUE;
		}

		cwin->cpref->add_recursively_files =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_ADD_RECURSIVELY_FILES,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			recursively_f = TRUE;
		}

		cwin->cpref->show_album_art =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_WINDOW,
					       KEY_SHOW_ALBUM_ART,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			album_f = TRUE;
		}

		cwin->cpref->album_art_size =
			g_key_file_get_integer(cwin->cpref->configrc_keyfile,
					      GROUP_WINDOW,
					      KEY_ALBUM_ART_SIZE,
					      &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			album_art_size_f = TRUE;
		}

		cwin->cpref->album_art_pattern =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_GENERAL,
					      KEY_ALBUM_ART_PATTERN,
					      &error);
		if (!cwin->cpref->album_art_pattern) {
			g_error_free(error);
			error = NULL;
			album_art_pattern_f = TRUE;
		}

		cwin->cpref->timer_remaining_mode =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_TIMER_REMAINING_MODE,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			timer_remaining_mode_f = TRUE;
		}

		u_file = g_key_file_get_string(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_LAST_FOLDER,
					       &error);
		if (!u_file) {
			g_error_free(error);
			error = NULL;
			last_folder_f = TRUE;
		} else {
			cwin->cstate->last_folder = g_filename_from_utf8(u_file,
							   -1, NULL, NULL, &error);
			if (!cwin->cstate->last_folder) {
				g_warning("Unable to get filename "
					  "from UTF-8 string: %s",
					  u_file);
				g_error_free(error);
				error = NULL;
				last_folder_f = TRUE;
			}
			g_free(u_file);
		}

		cwin->cpref->instant_filter =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_INSTANT_FILTER,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			instant_filter_f= TRUE;
		}
		cwin->cpref->use_hint =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_USE_HINT,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			use_hint_f= TRUE;
		}

		cwin->cpref->shuffle =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_PLAYLIST,
					       KEY_SHUFFLE,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			shuffle_f = TRUE;
		}

		cwin->cpref->repeat =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_PLAYLIST,
					       KEY_REPEAT,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			repeat_f= TRUE;
		}

		/* Retrieve Notification preferences */

		cwin->cpref->show_osd =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_SHOW_OSD,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			show_osd_f = TRUE;
		}

		cwin->cpref->osd_in_systray =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_OSD_IN_TRAY,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			osd_in_systray_f = TRUE;
		}

		cwin->cpref->albumart_in_osd =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_SHOW_ALBUM_ART_OSD,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			albumart_in_osd_f = TRUE;
		}
		cwin->cpref->actions_in_osd =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_GENERAL,
					       KEY_SHOW_ACTIONS_OSD,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			actions_in_osd_f = TRUE;
		}

		/* Retrieve Services Internet preferences */

		cwin->cpref->lw.lastfm_support =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_SERVICES,
					       KEY_LASTFM,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			lastfm_f = TRUE;
		}

		cwin->cpref->lw.lastfm_user =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_SERVICES,
					      KEY_LASTFM_USER,
					      &error);
		if (error) {
			g_error_free(error);
			error = NULL;
		}

		cwin->cpref->lw.lastfm_pass =
			g_key_file_get_string(cwin->cpref->configrc_keyfile,
					      GROUP_SERVICES,
					      KEY_LASTFM_PASS,
					      &error);
		if (error) {
			g_error_free(error);
			error = NULL;
		}
		#ifdef HAVE_LIBGLYR
		cwin->cpref->get_album_art =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_SERVICES,
					       KEY_GET_ALBUM_ART,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			get_album_art_f = TRUE;
		}
		#endif
		cwin->cpref->use_cddb =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_SERVICES,
					       KEY_USE_CDDB,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			use_cddb_f = TRUE;
		}

#if HAVE_GLIB_2_26		
		cwin->cpref->use_mpris2 =
			g_key_file_get_boolean(cwin->cpref->configrc_keyfile,
					       GROUP_SERVICES,
					       KEY_ALLOW_MPRIS2,
					       &error);
		if (error) {
			g_error_free(error);
			error = NULL;
			use_mpris2_f = TRUE;
		}
#endif		
	}

	/* Fill up with failsafe defaults */

	if (all_f || window_size_f) {
		cwin->cpref->window_width = MIN_WINDOW_WIDTH;
		cwin->cpref->window_height = MIN_WINDOW_HEIGHT;
	}
	if (all_f || window_position_f) {
		cwin->cpref->window_x = -1;
		cwin->cpref->window_y = -1;
	}
	if (all_f || sidebar_size_f)
		cwin->cpref->sidebar_size = DEFAULT_SIDEBAR_SIZE;
	if (all_f || sidebar_pane_f)
		cwin->cpref->sidebar_pane = g_strdup(PANE_LIBRARY);
	if (all_f || libs_f)
		cwin->cpref->library_dir = NULL;
	if (all_f || lib_add_f)
		cwin->cpref->lib_add = NULL;
	if (all_f || lib_delete_f)
		cwin->cpref->lib_delete = NULL;
	if (all_f || album_art_pattern_f)
		cwin->cpref->album_art_pattern = NULL;
	if (all_f || columns_f) {
		cwin->cpref->playlist_columns =
			g_slist_append(cwin->cpref->playlist_columns,
				       g_strdup(P_TITLE_STR));
		cwin->cpref->playlist_columns =
			g_slist_append(cwin->cpref->playlist_columns,
				       g_strdup(P_ARTIST_STR));
		cwin->cpref->playlist_columns =
			g_slist_append(cwin->cpref->playlist_columns,
				       g_strdup(P_ALBUM_STR));
		cwin->cpref->playlist_columns =
			g_slist_append(cwin->cpref->playlist_columns,
				       g_strdup(P_LENGTH_STR));
	}
	if (all_f || nodes_f) {
		cwin->cpref->library_tree_nodes =
			g_slist_append(cwin->cpref->library_tree_nodes,
				       g_strdup(P_ARTIST_STR));
		cwin->cpref->library_tree_nodes =
			g_slist_append(cwin->cpref->library_tree_nodes,
				       g_strdup(P_ALBUM_STR));
		cwin->cpref->library_tree_nodes =
			g_slist_append(cwin->cpref->library_tree_nodes,
				       g_strdup(P_TITLE_STR));
	}
	if (all_f || fuse_folders_f)
		cwin->cpref->fuse_folders = FALSE;
	if (all_f || sort_by_year_f)
		cwin->cpref->sort_by_year = FALSE;
	if (all_f || col_widths_f) {
		for (i=0; i<4; i++) {
			cwin->cpref->playlist_column_widths =
				g_slist_append(cwin->cpref->playlist_column_widths,
				       GINT_TO_POINTER(DEFAULT_PLAYLIST_COL_WIDTH));
		}
	}
	if (all_f || cur_lib_view_f)
		cwin->cpref->cur_library_view = FOLDERS;
	if (all_f || recursively_f)
		cwin->cpref->add_recursively_files = FALSE;
	if (all_f || last_folder_f)
		cwin->cstate->last_folder = (gchar*)g_get_home_dir();
	if (all_f || album_f)
		cwin->cpref->show_album_art = TRUE;
	if (all_f || album_art_size_f)
		cwin->cpref->album_art_size = ALBUM_ART_SIZE;
	if (all_f || timer_remaining_mode_f)
		cwin->cpref->timer_remaining_mode = FALSE;
	if (all_f || show_osd_f)
		cwin->cpref->show_osd = TRUE;
	if (all_f || osd_in_systray_f)
		cwin->cpref->osd_in_systray = TRUE;
	if (all_f || albumart_in_osd_f)
		cwin->cpref->albumart_in_osd = TRUE;
	if (all_f || actions_in_osd_f)
		cwin->cpref->actions_in_osd = FALSE;
	if (all_f || remember_window_state_f)
		cwin->cpref->remember_window_state = TRUE;
	if (all_f || start_mode_f)
		cwin->cpref->start_mode = g_strdup(NORMAL_STATE);
	if (all_f || close_to_tray_f)
		cwin->cpref->close_to_tray = TRUE;
	if (all_f || status_bar_f)
		cwin->cpref->status_bar = TRUE;
	if (all_f || controls_below_f)
		cwin->cpref->controls_below = FALSE;
	if (all_f || save_playlist_f)
		cwin->cpref->save_playlist = TRUE;
	if (all_f || software_mixer_f)
		cwin->cpref->software_mixer = TRUE;
	if (all_f || shuffle_f)
		cwin->cpref->shuffle = FALSE;
	if (all_f || repeat_f)
		cwin->cpref->repeat = FALSE;

	if (all_f || audio_sink_f)
		cwin->cpref->audio_sink = g_strdup(DEFAULT_SINK);
	if (all_f || audio_alsa_device_f)
		cwin->cpref->audio_alsa_device = g_strdup(ALSA_DEFAULT_DEVICE);
	if (all_f || audio_oss_device_f)
		cwin->cpref->audio_oss_device = g_strdup(OSS_DEFAULT_DEVICE);
	if (all_f || lastfm_f)
		cwin->cpref->lw.lastfm_support = FALSE;
	#ifdef HAVE_LIBGLYR
	if (all_f || get_album_art_f)
		cwin->cpref->get_album_art = FALSE;
	#endif
	if (all_f || use_cddb_f)
		cwin->cpref->use_cddb = TRUE;
	if (all_f || use_mpris2_f)
		cwin->cpref->use_mpris2 = TRUE;
	if (all_f || instant_filter_f)
		cwin->cpref->instant_filter = TRUE;
	if (all_f || use_hint_f)
		cwin->cpref->use_hint = TRUE;

	/* Cleanup */

	g_free(conrc);
	g_free(condir);
#ifdef HAVE_LIBGLYR
	g_free(cache_folder);
#endif
	if (err)
		return -1;
	else
		return 0;
}

static gboolean rescand_icompatible_db(gpointer data)
{
	struct con_win *cwin = data;

	GtkWidget *dialog;
	gint result;

	dialog = gtk_message_dialog_new(GTK_WINDOW(cwin->mainwindow),
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_YES_NO,
					_("Sorry: The music database is incompatible with previous versions to 0.8.0\n\n"
					"Want to upgrade the collection?."));

	result = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if( result == GTK_RESPONSE_YES)
		rescan_library_handler(cwin);

	return TRUE;
}

gint init_musicdbase(struct con_win *cwin)
{
	gint ret;
	const gchar *home;

	CDEBUG(DBG_INFO, "Initializing music dbase");

	home = g_get_user_config_dir();
	cwin->cdbase->db_file = g_strdup_printf("%s%s", home,
						"/pragha/pragha.db");


	if (g_ascii_strcasecmp(cwin->cpref->installed_version, MIN_DATABASE_VERSION) < 0 ) {
		g_critical("Deleted Music database incompatible with previous to 0.8.0. Please rescan library.");
		ret = g_unlink(cwin->cdbase->db_file);
		if (ret != 0)
			g_warning("%s", strerror(ret));
		gtk_init_add(rescand_icompatible_db, cwin);
	}

	/* Create the database file */

	ret = sqlite3_open(cwin->cdbase->db_file, &cwin->cdbase->db);
	if (ret) {
		g_critical("Unable to open/create DB file : %s",
			   cwin->cdbase->db_file);
		g_free(cwin->cdbase->db_file);
		return -1;
	}

	return init_dbase_schema(cwin);
}

/*
 * EINVAL: We bail out.
 * ENODEV: We continue to launch the GUI.
 */
gint init_audio(struct con_win *cwin)
{
	gint id = 0, ret = 0;
	ao_sample_format format;
	ao_device *ao_dev;
	gchar *audio_sink = NULL, *audio_dev = NULL;

	CDEBUG(DBG_INFO, "Initializing audio");

	ao_initialize();

	if (audio_backend) {
		if (!g_ascii_strcasecmp(audio_backend, ALSA_SINK)) {
			audio_sink = ALSA_SINK;
		} else if (!g_ascii_strcasecmp(audio_backend, OSS_SINK)) {
			audio_sink = OSS_SINK;
		} else {
			g_warning("Invalid cmdline audio device: %s",
				  audio_backend);
			ao_shutdown();
			ret = -EINVAL;
			goto bad1;
		}
	}

	if (!audio_sink)
		audio_sink = cwin->cpref->audio_sink;

	if (!g_ascii_strcasecmp(audio_sink, DEFAULT_SINK))
		id = ao_default_driver_id();
	else
		id = ao_driver_id(audio_sink);

	if (id == -1) {
		g_critical("%s driver not found",
			   cwin->cpref->audio_sink);
		ao_shutdown();
		ret = -ENODEV;
		goto bad1;
	}

	/*
	 * Decide the user's preferred audio device.
	 * Handle both the command line option and the preference
	 * from conrc. Command line option takes precedence over conrc.
	 */

	if (audio_device) {
		audio_dev = audio_device;
	} else {
		if (!g_ascii_strcasecmp(audio_sink, ALSA_SINK)) {
			if (cwin->cpref->audio_alsa_device)
				audio_dev = cwin->cpref->audio_alsa_device;
		} else if (!g_ascii_strcasecmp(audio_sink, OSS_SINK)) {
			if (cwin->cpref->audio_oss_device)
				audio_dev = cwin->cpref->audio_oss_device;
		}
	}

	cwin->clibao->ao_driver_id = id;

	/* Now append the audio device as a libao option */

	if (audio_dev) {
		if (!g_ascii_strcasecmp(audio_sink, ALSA_SINK)) {
			ao_append_option(&cwin->clibao->ao_opts, "dev", audio_dev);
		} else if (!g_ascii_strcasecmp(audio_sink, OSS_SINK)) {
			ao_append_option(&cwin->clibao->ao_opts, "dsp", audio_dev);
		} else if (!g_ascii_strcasecmp(audio_sink, DEFAULT_SINK)) {
			g_critical("Choose a valid audio backend explicitly");
			ret = -EINVAL;
			goto bad1;
		}
	}

	CDEBUG(DBG_INFO, "Using audio driver: %s (%s) with id: %d",
	       audio_sink, ao_driver_info(id)->name, id);

	if (audio_dev ) {
		CDEBUG(DBG_INFO, "Using audio device: %s", audio_dev);
	}

	/* Check if audio device is accessible */

	format.bits = 16;
	format.channels = 2;
	format.rate = 44100;
	format.byte_format = AO_FMT_LITTLE;

	ao_dev = ao_open_live(id, &format, cwin->clibao->ao_opts);

	if (ao_dev == NULL) {
		g_critical("Unable to open audio device");
		ret = -ENODEV;
		goto bad1;
	}

	/* Check if user wants software mixer */

	if (cwin->cpref->software_mixer && (audio_mixer == NULL)) {
		set_soft_mixer(cwin);
		cwin->cmixer->init_mixer(cwin);
		goto audio_init;
	}

	/* If not, try initializing HW mixers */

	if (init_audio_mixer(cwin) == -1) {
		g_critical("Unable to choose a HW audio mixer");
		set_soft_mixer(cwin);
	}

	if (cwin->cmixer->init_mixer(cwin) == -1) {
		ret = -ENODEV;
		goto bad2;
	}

audio_init:
	cwin->cstate->audio_init = true;
bad2:
	ao_close(ao_dev);
bad1:
	return ret;
}

gint init_threads(struct con_win *cwin)
{
	CDEBUG(DBG_INFO, "Initializing threads");

	#if !GLIB_CHECK_VERSION(2,31,0)
	if (!g_thread_supported())
		g_thread_init(NULL);
	#endif
	#if GLIB_CHECK_VERSION(2,24,0)
	g_type_init ();
	#endif
	gdk_threads_init();
	cwin->cstate->c_thread = NULL;
	cwin->cstate->c_mutex = g_mutex_new();
	cwin->cstate->l_mutex = g_mutex_new();
	cwin->cstate->c_cond = g_cond_new();

	return 0;
}

gint init_notify(struct con_win *cwin)
{
	if (cwin->cpref->show_osd) {
		if (!notify_init(PACKAGE_NAME))
			return -1;
	}

	return 0;
}

#ifdef HAVE_LIBKEYBINDER
gint init_keybinder(struct con_win *cwin)
{
	keybinder_init ();

	keybinder_bind("XF86AudioPlay", (KeybinderHandler) keybind_play_handler, cwin);
	keybinder_bind("XF86AudioStop", (KeybinderHandler) keybind_stop_handler, cwin);
	keybinder_bind("XF86AudioPrev", (KeybinderHandler) keybind_prev_handler, cwin);
	keybinder_bind("XF86AudioNext", (KeybinderHandler) keybind_next_handler, cwin);
	keybinder_bind("XF86AudioMedia", (KeybinderHandler) keybind_media_handler, cwin);

	return 0;
}
#endif

gint init_lastfm(struct con_win *cwin)
{
	gint ret = 0;

	CDEBUG(DBG_INFO, "Initializing libcurl");

	ret = curl_global_init(CURL_GLOBAL_ALL);
	if (ret) {
		g_critical("Unable to init curl: %d\n", ret);
		return -1;
	}

	cwin->clastfm->curl_handle = curl_easy_init();
	if (!cwin->clastfm->curl_handle) {
		g_critical("Unable to setup curl handle");
		return -1;
	}

	return 0;
}


gint init_first_state(struct con_win *cwin)
{
	CDEBUG(DBG_INFO, "Initializing state");

	cwin->cstate->state = ST_STOPPED;

	cwin->cstate->arturl = NULL;
	cwin->cstate->filter_entry = NULL;
	cwin->cstate->jump_filter = NULL;

	cwin->cstate->rand = g_rand_new();
	cwin->cstate->rand_track_refs = NULL;
	cwin->cstate->queue_track_refs = NULL;

	cwin->cstate->dragging = FALSE;
	cwin->cstate->curr_mobj_clear = FALSE;

	cwin->cstate->curr_mobj= NULL;

	cwin->osd_notify = NULL;

	return 0;
}

void init_tag_completion(struct con_win *cwin)
{
	GtkListStore *artist_tag_model, *album_tag_model, *genre_tag_model;

	/* Artist */

	artist_tag_model = gtk_list_store_new(1, G_TYPE_STRING);
	cwin->completion[0] = gtk_entry_completion_new();
	gtk_entry_completion_set_model(cwin->completion[0],
				       GTK_TREE_MODEL(artist_tag_model));
	gtk_entry_completion_set_text_column(cwin->completion[0], 0);
	g_object_unref(artist_tag_model);

	/* Album */

	album_tag_model = gtk_list_store_new(1, G_TYPE_STRING);
	cwin->completion[1] = gtk_entry_completion_new();
	gtk_entry_completion_set_model(cwin->completion[1],
				       GTK_TREE_MODEL(album_tag_model));
	gtk_entry_completion_set_text_column(cwin->completion[1], 0);
	g_object_unref(album_tag_model);

	/* Genre */

	genre_tag_model = gtk_list_store_new(1, G_TYPE_STRING);
	cwin->completion[2] = gtk_entry_completion_new();
	gtk_entry_completion_set_model(cwin->completion[2],
				       GTK_TREE_MODEL(genre_tag_model));
	gtk_entry_completion_set_text_column(cwin->completion[2], 0);
	g_object_unref(genre_tag_model);
}

void init_states_pixbuf(struct con_win *cwin)
{
	GtkIconTheme *icon_theme;
	icon_theme = gtk_icon_theme_get_default ();

	cwin->pixbuf->pixbuf_playing = gtk_icon_theme_load_icon (icon_theme, "stock_media-play",16, 0,NULL);
	cwin->pixbuf->pixbuf_paused = gtk_icon_theme_load_icon (icon_theme, "stock_media-pause", 16, 0, NULL);

	g_object_ref(cwin->pixbuf->pixbuf_playing);
	g_object_ref(cwin->pixbuf->pixbuf_paused);
}

void init_toggle_buttons(struct con_win *cwin)
{
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(cwin->shuffle_button), cwin->cpref->shuffle);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(cwin->repeat_button), cwin->cpref->repeat);

	if (!g_ascii_strcasecmp(cwin->cpref->sidebar_pane, PANE_LIBRARY))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cwin->toggle_lib), TRUE);
	else if (!g_ascii_strcasecmp(cwin->cpref->sidebar_pane, PANE_PLAYLISTS))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cwin->toggle_playlists), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cwin->toggle_lib), FALSE);
}

void init_menu_actions(struct con_win *cwin)
{
	GtkAction *action = NULL;

	action = gtk_ui_manager_get_action(cwin->bar_context_menu,"/Menubar/EditMenu/Shuffle");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION(action), cwin->cpref->shuffle);

	action = gtk_ui_manager_get_action(cwin->bar_context_menu,"/Menubar/EditMenu/Repeat");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION(action), cwin->cpref->repeat);

	action = gtk_ui_manager_get_action(cwin->bar_context_menu,"/Menubar/ViewMenu/Fullscreen");
	if(!g_ascii_strcasecmp(cwin->cpref->start_mode, FULLSCREEN_STATE))
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION(action), TRUE);
	else
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION(action), FALSE);

	action = gtk_ui_manager_get_action(cwin->bar_context_menu,"/Menubar/ViewMenu/Status bar");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION(action), cwin->cpref->status_bar);

	action = gtk_ui_manager_get_action(cwin->bar_context_menu,"/Menubar/ViewMenu/Playback controls below");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION(action), cwin->cpref->controls_below);

#ifndef HAVE_LIBGLYR
	action = gtk_ui_manager_get_action(cwin->bar_context_menu,"/Menubar/ToolsMenu/Search lyric");
	gtk_action_set_sensitive(action, FALSE);

	action = gtk_ui_manager_get_action(cwin->bar_context_menu,"/Menubar/ToolsMenu/Search artist info");
	gtk_action_set_sensitive(action, FALSE);

	action = gtk_ui_manager_get_action(cwin->bar_context_menu,"/Menubar/ToolsMenu/Search album art");
	gtk_action_set_sensitive(action, FALSE);
#endif
}

void init_pixbufs(struct con_win *cwin)
{
	GtkIconTheme *icontheme = gtk_icon_theme_get_default();

	cwin->pixbuf->pixbuf_app = gdk_pixbuf_new_from_file(PIXMAPDIR"/pragha.png", NULL);
	if (!cwin->pixbuf->pixbuf_app)
		g_warning("Unable to load pragha png");

	cwin->pixbuf->pixbuf_artist = gdk_pixbuf_new_from_file_at_scale(PIXMAPDIR
									"/artist.png",
									16,
									16,
									TRUE,
									NULL);
	if (!cwin->pixbuf->pixbuf_artist)
		g_warning("Unable to load artist png");

	cwin->pixbuf->pixbuf_album = gtk_icon_theme_load_icon(icontheme,
							      "media-optical",
							      16,
							      0,
							      NULL);
	if (!cwin->pixbuf->pixbuf_album)
		cwin->pixbuf->pixbuf_album = gdk_pixbuf_new_from_file_at_scale(PIXMAPDIR
										"/album.png",
										16,
										16,
										TRUE,
										NULL);
	if (!cwin->pixbuf->pixbuf_album)
		g_warning("Unable to load album png");

	cwin->pixbuf->pixbuf_track = gtk_icon_theme_load_icon(icontheme,
							     "gnome-mime-audio",
							     16,
							     0,
							     NULL);
	if (!cwin->pixbuf->pixbuf_track)
		cwin->pixbuf->pixbuf_track = gdk_pixbuf_new_from_file_at_scale(PIXMAPDIR
										"/track.png",
										16,
										16,
										TRUE,
										NULL);
	if (!cwin->pixbuf->pixbuf_track)
		g_warning("Unable to load track png");

	cwin->pixbuf->pixbuf_genre = gdk_pixbuf_new_from_file_at_scale(PIXMAPDIR
								       "/genre.png",
								       16,
								       16,
								       TRUE,
								       NULL);
	if (!cwin->pixbuf->pixbuf_genre)
		g_warning("Unable to load genre png");

	cwin->pixbuf->pixbuf_dir = gtk_icon_theme_load_icon(icontheme,
							    "folder-music",
							    16,
							    0,
							    NULL);
	if (!cwin->pixbuf->pixbuf_dir)
		cwin->pixbuf->pixbuf_dir = gtk_icon_theme_load_icon(icontheme,
										"gtk-directory",
										16,
										0,
										NULL);
	if (!cwin->pixbuf->pixbuf_dir)
		g_warning("Unable to load folder png");
}

#if HAVE_EXO
static void
pragha_save_yourself (ExoXsessionClient *client, struct con_win *cwin)
{
	if (cwin->cpref->save_playlist)
		save_current_playlist_state(cwin);
	save_preferences(cwin);
}

gint init_session_support(struct con_win *cwin)
{
	ExoXsessionClient *client;
	GdkDisplay        *display;
	GdkWindow         *leader;

	display = gtk_widget_get_display (cwin->mainwindow);
	leader = gdk_display_get_default_group (display);
	client = exo_xsession_client_new_with_group (leader);

	g_signal_connect (G_OBJECT (client), "save-yourself",
			  G_CALLBACK (pragha_save_yourself), cwin);

	return 0;
}
#endif

static gboolean
window_state_event (GtkWidget *widget, GdkEventWindowState *event, struct con_win *cwin)
{
	GtkAction *action_fullscreen;

 	if (event->type == GDK_WINDOW_STATE && (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)) {
		action_fullscreen = gtk_ui_manager_get_action(cwin->bar_context_menu, "/Menubar/ViewMenu/Fullscreen");

		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action_fullscreen), (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0);
	}

	return TRUE;
}

void init_gui(gint argc, gchar **argv, struct con_win *cwin)
{
	GtkUIManager *menu;
	GtkWidget *vbox, *hbox_panel, *hbox_main, *status_bar, *menu_bar;
	gchar *role;

	CDEBUG(DBG_INFO, "Initializing gui");

	gtk_init(&argc, &argv);

	g_set_application_name(_("Pragha Music Player"));
	g_setenv("PULSE_PROP_media.role", "audio", TRUE);

	init_pixbufs(cwin);
	init_states_pixbuf(cwin);

	/* Main window */

	cwin->mainwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	if (cwin->pixbuf->pixbuf_app)
		gtk_window_set_icon(GTK_WINDOW(cwin->mainwindow),
				    cwin->pixbuf->pixbuf_app);

	gtk_window_set_title(GTK_WINDOW(cwin->mainwindow), _("Pragha Music Player"));

	GdkScreen *screen = gtk_widget_get_screen(cwin->mainwindow);
	GdkColormap *colormap = gdk_screen_get_rgba_colormap (screen);
	if (colormap && gdk_screen_is_composited (screen)){
		gtk_widget_set_default_colormap(colormap);
	}

	g_signal_connect(G_OBJECT(cwin->mainwindow),
			 "window-state-event",
			 G_CALLBACK(window_state_event), cwin);
	g_signal_connect(G_OBJECT(cwin->mainwindow),
			 "delete_event",
			 G_CALLBACK(exit_gui), cwin);
	g_signal_connect(G_OBJECT(cwin->mainwindow),
			 "destroy",
			 G_CALLBACK(exit_pragha), cwin);

	/* Set Default Size */

	gtk_window_set_default_size(GTK_WINDOW(cwin->mainwindow),
				    cwin->cpref->window_width,
				    cwin->cpref->window_height);

	/* Set Position */

	if (cwin->cpref->window_x != -1) {
		gtk_window_move(GTK_WINDOW(cwin->mainwindow),
				cwin->cpref->window_x,
				cwin->cpref->window_y);
	}

	/* Systray */

	create_status_icon(cwin);

	/* Main Vbox */

	vbox = gtk_vbox_new(FALSE, 2);

	gtk_container_add(GTK_CONTAINER(cwin->mainwindow), vbox);

	/* Create hboxen */

	menu = create_menu(cwin);
	hbox_main = create_main_region(cwin);
	hbox_panel = create_panel(cwin);
	status_bar = create_status_bar(cwin);
	menu_bar = gtk_ui_manager_get_widget(menu, "/Menubar");

	/* Pack all hboxen into vbox */

	gtk_box_pack_start(GTK_BOX(vbox),
			   GTK_WIDGET(menu_bar),
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			   GTK_WIDGET(hbox_panel),
			   FALSE,FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			   GTK_WIDGET(hbox_main),
			   TRUE,TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
			   GTK_WIDGET(status_bar),
			   FALSE,FALSE, 0);

	/* Init window state */

	if(!g_ascii_strcasecmp(cwin->cpref->start_mode, FULLSCREEN_STATE)) {
		gtk_widget_show_all(cwin->mainwindow);
	}
	else if(!g_ascii_strcasecmp(cwin->cpref->start_mode, ICONIFIED_STATE)) {
		if(gtk_status_icon_is_embedded(GTK_STATUS_ICON(cwin->status_icon))) {
			gtk_widget_hide(GTK_WIDGET(cwin->mainwindow));
		}
		else {
			g_warning("(%s): No embedded status_icon.", __func__);
			gtk_window_iconify (GTK_WINDOW (cwin->mainwindow));
			gtk_widget_show_all(cwin->mainwindow);
			gtk_widget_hide(cwin->unfull_button);
		}
	}
	else {
		gtk_widget_show_all(cwin->mainwindow);
		gtk_widget_hide(cwin->unfull_button);
	}

	init_menu_actions(cwin);
	init_toggle_buttons(cwin);

	if (cwin->album_art_frame)
		resize_album_art_frame(cwin);

	gtk_widget_grab_focus(GTK_WIDGET(cwin->play_button));

	/* set a unique role on each window (for session management) */
	role = g_strdup_printf ("Pragha-%p-%d-%d", cwin->mainwindow, (gint) getpid (), (gint) time (NULL));
	gtk_window_set_role (GTK_WINDOW (cwin->mainwindow), role);
	g_free (role);

	#if HAVE_EXO
	init_session_support(cwin);
	#endif

	gtk_init_add(_init_gui_state, cwin);
}
