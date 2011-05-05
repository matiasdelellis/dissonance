/************************************************************************
 * Copyright (C) 2009-2010 matias <mati86dl@gmail.com>                  *
 * Copyright (C) 2011      hakan  <smultimeter@gmail.com>               *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    * 
 * along with this program.  If not, see <http:www.gnu.org/licenses/>.  * 
 ************************************************************************/
#include "pragha.h"

/* for GDBusConnection nedd Glib 2.26 */
#if HAVE_GLIB_2_26
static const gchar MPRIS_NAME[] = "org.mpris.MediaPlayer2.pragha";
static const gchar MPRIS_PATH[] = "/org/mpris/MediaPlayer2";
static const gchar mpris2xml[] = 
"<node>"
"        <interface name='org.mpris.MediaPlayer2'>"
"                <method name='Raise'/>"
"                <method name='Quit'/>"
"                <property name='CanQuit' type='b' access='read'/>"
"                <property name='CanRaise' type='b' access='read'/>"
"                <property name='HasTrackList' type='b' access='read'/>"
"                <property name='Identity' type='s' access='read'/>"
"                <property name='DesktopEntry' type='s' access='read'/>"
"                <property name='SupportedUriSchemes' type='as' access='read'/>"
"                <property name='SupportedMimeTypes' type='as' access='read'/>"
"        </interface>"
"        <interface name='org.mpris.MediaPlayer2.Player'>"
"                <method name='Next'/>"
"                <method name='Previous'/>"
"                <method name='Pause'/>"
"                <method name='PlayPause'/>"
"                <method name='Stop'/>"
"                <method name='Play'/>"
"                <method name='Seek'>"
"				 		<arg direction='in' name='Offset' type='x'/>"
"				 </method>"
"                <method name='SetPosition'>"
"						<arg direction='in' name='TrackId' type='o'/>"
"						<arg direction='in' name='Position' type='x'/>"
"                </method>"
"                <method name='OpenUri'>"
"				 		<arg direction='in' name='Uri' type='s'/>"
"				 </method>"
"                <signal name='Seeked'><arg name='Position' type='x'/></signal>"
"                <property name='PlaybackStatus' type='s' access='read'/>"
"                <property name='LoopStatus' type='s' access='readwrite'/>"
"                <property name='Rate' type='d' access='readwrite'/>"
"                <property name='Shuffle' type='b' access='readwrite'/>"
"                <property name='Metadata' type='a{sv}' access='read'/>"
"                <property name='Volume' type='d' access='readwrite'/>"
"                <property name='Position' type='x' access='read'/>"
"                <property name='MinimumRate' type='d' access='read'/>"
"                <property name='MaximumRate' type='d' access='read'/>"
"                <property name='CanGoNext' type='b' access='read'/>"
"                <property name='CanGoPrevious' type='b' access='read'/>"
"                <property name='CanPlay' type='b' access='read'/>"
"                <property name='CanPause' type='b' access='read'/>"
"                <property name='CanSeek' type='b' access='read'/>"
"                <property name='CanControl' type='b' access='read'/>"
"        </interface>"
"        <interface name='org.mpris.MediaPlayer2.Playlists'>"
"                <method name='ActivatePlaylist'>"
"						<arg direction='in' name='PlaylistId' type='o'/>"
"				</method>"
"                <method name='GetPlaylists'>"
"						<arg direction='in' name='Index' type='u'/>"
"						<arg direction='in' name='MaxCount' type='u'/>"
"						<arg direction='in' name='Order' type='s'/>"
"						<arg direction='in' name='ReverseOrder' type='b'/>"
"						<arg direction='out' name='Playlists' type='a(oss)'/>"
"                </method>"
"                <property name='PlaylistCount' type='u' access='read'/>"
"                <property name='Orderings' type='as' access='read'/>"
"                <property name='ActivePlaylist' type='(b(oss))' access='read'/>"
"        </interface>"
"        <interface name='org.mpris.MediaPlayer2.TrackList'>"
"                <method name='GetTracksMetadata'>"
"                        <arg direction='in' name='TrackIds' type='ao'/>"
"                        <arg direction='out' name='Metadata' type='aa{sv}'>"
"                        </arg>"
"                </method>"
"                <method name='AddTrack'>"
"                        <arg direction='in' name='Uri' type='s'/>"
"                        <arg direction='in' name='AfterTrack' type='o'/>"
"                        <arg direction='in' name='SetAsCurrent' type='b'/>"
"                </method>"
"                <method name='RemoveTrack'>"
"                        <arg direction='in' name='TrackId' type='o'/>"
"                </method>"
"                <method name='GoTo'>"
"                        <arg direction='in' name='TrackId' type='o'/>"
"                </method>"
"                <signal name='TrackListReplaced'>"
"                        <arg name='Tracks' type='ao'/>"
"                        <arg name='CurrentTrack' type='o'/>"
"                </signal>"
"                <signal name='TrackAdded'>"
"                        <arg name='Metadata' type='a{sv}'>"
"                        </arg>"
"                        <arg name='AfterTrack' type='o'/>"
"                </signal>"
"                <signal name='TrackRemoved'>"
"                        <arg name='TrackId' type='o'/>"
"                </signal>"
"                <signal name='TrackMetadataChanged'>"
"                        <arg name='TrackId' type='o'/>"
"                        <arg name='Metadata' type='a{sv}'>"
"                        </arg>"
"                </signal>"
"                <property name='Tracks' type='ao' access='read'/>"
"                <property name='CanEditTracks' type='b' access='read'/>"
"        </interface>"
"</node>";

/* some MFCisms */
#define BEGIN_INTERFACE(x) \
	if(g_quark_try_string(interface_name)==cwin->cmpris2->interface_quarks[x]) {
#define MAP_METHOD(x,y) \
	if(!g_strcmp0(#y, method_name)) { \
		g_dbus_method_invocation_return_value (invocation, mpris_##x##_##y(cwin, parameters)); return; }
#define PROPGET(x,y) \
	if(!g_strcmp0(#y, property_name)) \
		return mpris_##x##_get_##y(cwin);
#define PROPPUT(x,y) \
	if(g_quark_try_string(property_name)==g_quark_from_static_string(#y)) \
		mpris_##x##_put_##y(cwin, value);
#define END_INTERFACE }	

/* org.mpris.MediaPlayer2 */
static GVariant* mpris_Root_Raise(struct con_win *cwin, GVariant* parameters) { 
	gtk_window_present(GTK_WINDOW(cwin->mainwindow));
	return NULL; 
}
static GVariant* mpris_Root_Quit(struct con_win *cwin, GVariant* parameters) { 
	exit_pragha(NULL, cwin);
	return NULL; 
}
static GVariant* mpris_Root_get_CanQuit(struct con_win *cwin) { 
	return g_variant_new_boolean(TRUE); 
}
static GVariant* mpris_Root_get_CanRaise(struct con_win *cwin) { 
	return g_variant_new_boolean(TRUE); 
}
static GVariant* mpris_Root_get_HasTrackList(struct con_win *cwin) { 
	return g_variant_new_boolean(TRUE);
}
static GVariant* mpris_Root_get_Identity(struct con_win *cwin) { 	

	return g_variant_new_string("Pragha Music Player"); 
}
static GVariant* mpris_Root_get_DesktopEntry(struct con_win *cwin) { 
	GVariant* ret_val = g_variant_new_string(DESKTOPENTRY); 
	return ret_val;
}
static GVariant* mpris_Root_get_SupportedUriSchemes(struct con_win *cwin) { 
	return g_variant_parse(G_VARIANT_TYPE("as"), 
		"['file', 'cdda']", NULL, NULL, NULL);
}
static GVariant* mpris_Root_get_SupportedMimeTypes(struct con_win *cwin) { 
	return g_variant_parse(G_VARIANT_TYPE("as"), 
		"['audio/x-mp3', 'audio/mpeg', 'audio/x-mpeg', 'audio/mpeg3', "
		"'audio/mp3', 'application/ogg', 'application/x-ogg', 'audio/vorbis', "
		"'audio/x-vorbis', 'audio/ogg', 'audio/x-ogg', 'audio/x-flac', "
		"'application/x-flac', 'audio/flac', 'audio/x-wav']", NULL, NULL, NULL);
}

/* org.mpris.MediaPlayer2.Player */
static GVariant* mpris_Player_Play(struct con_win *cwin, GVariant* parameters) {
	play_track(cwin);
	return NULL; 
}
static GVariant* mpris_Player_Next(struct con_win *cwin, GVariant* parameters) {
	play_next_track(cwin); 
	return NULL; 
}
static GVariant* mpris_Player_Previous(struct con_win *cwin, GVariant* parameters) {
	play_prev_track(cwin); 
	return NULL; 
}
static GVariant* mpris_Player_Pause(struct con_win *cwin, GVariant* parameters) {
	pause_playback(cwin); 
	return NULL; 
}
static GVariant* mpris_Player_PlayPause(struct con_win *cwin, GVariant* parameters) {
	play_pause_resume(cwin);
	return NULL; 
}
static GVariant* mpris_Player_Stop(struct con_win *cwin, GVariant* parameters) {
	stop_playback(cwin); 
	return NULL; 
}
static GVariant* mpris_Player_Seek(struct con_win *cwin, GVariant* parameters) {
	gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(cwin->track_progress_bar));
	gint seek = cwin->cstate->curr_mobj->tags->length * fraction;
	gint64 param;
	g_variant_get(parameters, "(x)", &param);
	seek += (param / 1000000);
	fraction += (gdouble)seek / (gdouble)cwin->cstate->curr_mobj->tags->length;
	seek_playback(cwin, seek, fraction);
	return NULL;
}
static GVariant* mpris_Player_SetPosition(struct con_win *cwin, GVariant* parameters) {
	gint64 param;
	gchar *path = NULL;
	g_variant_get(parameters, "(sx)", &path, &param);
	if(!g_strcmp0(cwin->cstate->curr_mobj->file, path)) {
		gdouble fraction = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(cwin->track_progress_bar));
		gint seek = cwin->cstate->curr_mobj->tags->length * fraction;
		seek += (param / 1000000);
		fraction += (gdouble)seek / (gdouble)cwin->cstate->curr_mobj->tags->length;
		seek_playback(cwin, seek, fraction);
	}
	g_free(path);
	return NULL;
}
static GVariant* mpris_Player_OpenUri(struct con_win *cwin, GVariant* parameters) {
	gchar *uri = NULL;
	g_variant_get(parameters, "(s)", &uri);
	gboolean failed = FALSE;
	if(uri) {
		// TODO: Translate "cdda://sr0/Track 01.wav" URIs for new_musicobject_from_cdda()
		//       If there is such a convention on other players
		gchar *path = g_filename_from_uri(uri, NULL, NULL);
		if(path && is_playable_file(path)) {
			struct musicobject *mobj = new_musicobject_from_file(path);
			if(mobj) {
				GtkTreePath *tree_path;
				append_current_playlist_ex(mobj, cwin, &tree_path);
				
				// Dangerous: reusing double-click-handler here.
				current_playlist_row_activated_cb(
					GTK_TREE_VIEW(cwin->current_playlist), tree_path, NULL, cwin);
				
				gtk_tree_path_free(tree_path);
			} else {
				failed = TRUE;
			}
			g_free(uri);
		} else {
			g_dbus_method_invocation_return_dbus_error(cwin->cmpris2->method_invocation,
					DBUS_ERROR_FILE_NOT_FOUND, "This file does not play here.");
		}
		g_free(path);
	} else {
		failed = TRUE;
	}
	if(failed)
		g_dbus_method_invocation_return_dbus_error(cwin->cmpris2->method_invocation,
				DBUS_ERROR_INVALID_FILE_CONTENT, "This file does not play here.");
	return NULL;
}
static GVariant* mpris_Player_get_PlaybackStatus(struct con_win *cwin) { 
	switch (cwin->cstate->state) {
	case ST_PLAYING:	return g_variant_new_string("Playing");
	case ST_PAUSED:		return g_variant_new_string("Paused");
	default:		return g_variant_new_string("Stopped");
	}
}
static GVariant* mpris_Player_get_LoopStatus(struct con_win *cwin) { 
	return g_variant_new_string(cwin->cpref->repeat ? "Playlist" : "None");
}
static void mpris_Player_put_LoopStatus(struct con_win *cwin, GVariant *value) {
	const gchar *new_loop = g_variant_get_string(value, NULL); 
	cwin->cpref->repeat = g_strcmp0("Playlist", new_loop) ? FALSE : TRUE;
}
static GVariant* mpris_Player_get_Rate(struct con_win *cwin) { 
	return g_variant_new_double(1.0);
}
static void mpris_Player_put_Rate(struct con_win *cwin, GVariant *value) { 
	g_dbus_method_invocation_return_dbus_error(cwin->cmpris2->method_invocation, 
		DBUS_ERROR_NOT_SUPPORTED, "This is not alsaplayer.");
}
static GVariant* mpris_Player_get_Shuffle(struct con_win *cwin) {
	return g_variant_new_boolean(cwin->cpref->shuffle);
}
static void mpris_Player_put_Shuffle(struct con_win *cwin, GVariant *value) { 
	cwin->cpref->shuffle = g_variant_get_boolean(value);
}
static GVariant * handle_get_trackid(struct musicobject *mobj) {
	gchar *o = alloca(260);
	if(NULL == mobj)
		return g_variant_new_object_path("/");
	g_snprintf(o, 260, "%s/TrackList/%p", MPRIS_PATH, mobj);
	return g_variant_new_object_path(o);
}
static void handle_get_metadata(struct musicobject *mobj, GVariantBuilder *b) {
	gchar *genres = g_strdup_printf("['%s']", mobj->tags->genre);
	gchar *date = g_strdup_printf("%d", mobj->tags->year);
	gchar *comments = g_strdup_printf("['%s']", mobj->tags->comment);
	gchar *url = g_str_has_prefix(mobj->file, "cdda") ? 
		g_strdup(mobj->file) : 
		g_filename_to_uri(mobj->file, NULL, NULL);
	g_variant_builder_add (b, "{sv}", "mpris:trackid", 
		handle_get_trackid(mobj));
	g_variant_builder_add (b, "{sv}", "xesam:url", 
		g_variant_new_string(url));
	g_variant_builder_add (b, "{sv}", "xesam:title", 
		g_variant_new_string(mobj->tags->title));
	g_variant_builder_add (b, "{sv}", "xesam:artist", 
		g_variant_new_string(mobj->tags->artist));
	g_variant_builder_add (b, "{sv}", "xesam:album", 
		g_variant_new_string(mobj->tags->album));
	g_variant_builder_add (b, "{sv}", "xesam:genre", 
		g_variant_parse(G_VARIANT_TYPE("as"), genres, NULL, NULL, NULL));
	g_variant_builder_add (b, "{sv}", "xesam:contentCreated", 
		g_variant_new_string (date));
	g_variant_builder_add (b, "{sv}", "xesam:trackNumber", 
		g_variant_new_int32(mobj->tags->track_no));
	g_variant_builder_add (b, "{sv}", "xesam:comment", 
		g_variant_new_string(comments));
	g_variant_builder_add (b, "{sv}", "mpris:length", 
		g_variant_new_int64(mobj->tags->length * 1000000l));
	g_variant_builder_add (b, "{sv}", "audio-bitrate", 
		g_variant_new_int32(mobj->tags->bitrate));
	g_variant_builder_add (b, "{sv}", "audio-channels", 
		g_variant_new_int32(mobj->tags->channels));
	g_variant_builder_add (b, "{sv}", "audio-samplerate", 
		g_variant_new_int32(mobj->tags->samplerate));
	g_free(genres);
	g_free(date);
	g_free(comments);
	g_free(url);
}
static GVariant* mpris_Player_get_Metadata(struct con_win *cwin) { 
	GVariantBuilder *b = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	if (cwin->cstate->state != ST_STOPPED) {
		handle_get_metadata(cwin->cstate->curr_mobj, b);
	} else {
		g_variant_builder_add (b, "{sv}", "mpris:trackid", 
			handle_get_trackid(NULL));
	}
	return g_variant_builder_end(b); 
}
static GVariant* mpris_Player_get_Volume(struct con_win *cwin) { 
	return g_variant_new_double(
		((gdouble)cwin->cmixer->curr_vol - (gdouble)cwin->cmixer->min_vol) / (gdouble)cwin->cmixer->max_vol);
}
static void mpris_Player_put_Volume(struct con_win *cwin, GVariant *value) { 
	gdouble volume = g_variant_get_double(value);
	
	if (!cwin->cstate->audio_init)
		return;
	volume *= cwin->cmixer->max_vol;
	volume -= cwin->cmixer->min_vol;
	cwin->cmixer->curr_vol = (glong)volume;
	cwin->cmixer->set_volume(cwin);
	gtk_scale_button_set_value(GTK_SCALE_BUTTON(cwin->vol_button),
				   cwin->cmixer->curr_vol);
}
static GVariant* mpris_Player_get_Position(struct con_win *cwin) { 
	if (cwin->cstate->state == ST_STOPPED)
		return g_variant_new_int64(0);
	else
		return g_variant_new_int64(cwin->cstate->newsec * 1000000);
}
static GVariant* mpris_Player_get_MinimumRate(struct con_win *cwin) { 
	return g_variant_new_double(1.0);
}
static GVariant* mpris_Player_get_MaximumRate(struct con_win *cwin) { 
	return g_variant_new_double(1.0);
}
static GVariant* mpris_Player_get_CanGoNext(struct con_win *cwin) { 
	// do we need to go into such detail?
	return g_variant_new_boolean(TRUE);
}
static GVariant* mpris_Player_get_CanGoPrevious(struct con_win *cwin) { 
	// do we need to go into such detail?
	return g_variant_new_boolean(TRUE);
}
static GVariant* mpris_Player_get_CanPlay(struct con_win *cwin) { 
	return g_variant_new_boolean(NULL != cwin->cstate->curr_mobj);
}
static GVariant* mpris_Player_get_CanPause(struct con_win *cwin) { 
	return g_variant_new_boolean(NULL != cwin->cstate->curr_mobj);
}
static GVariant* mpris_Player_get_CanSeek(struct con_win *cwin) { 
	return g_variant_new_boolean(TRUE);
}
static GVariant* mpris_Player_get_CanControl(struct con_win *cwin) { 
	// always?
	return g_variant_new_boolean(TRUE);
}

/* org.mpris.MediaPlayer2.Playlists */
static GVariant* mpris_Playlists_ActivatePlaylist(struct con_win *cwin, GVariant* parameters) { 
	gchar* playlist = NULL;
	gboolean found = FALSE;
	g_variant_get(parameters, "(o)", &playlist);
	if(playlist && g_str_has_prefix(playlist, MPRIS_PATH)) {
		gint i = 0;
		gchar **playlists = get_playlist_names_db(cwin);
		while(playlists[i]) {
			gchar *list = g_strdup_printf("%s/Playlists/%d", MPRIS_PATH, i);
			if(!g_strcmp0(list, playlist)) {
				stop_playback(cwin);
				clear_current_playlist(NULL, cwin);
				add_playlist_current_playlist(playlists[i], cwin);
				play_track(cwin);
				found = TRUE;
				break;
			}
			g_free(list);
			i++;
		}
		g_free(playlists);
	}
	if(!found)
		g_dbus_method_invocation_return_dbus_error(cwin->cmpris2->method_invocation, 
				DBUS_ERROR_INVALID_ARGS, "Unknown or malformed playlist object path.");
		
	g_free(playlist);
	return NULL; 
}
static GVariant* mpris_Playlists_GetPlaylists(struct con_win *cwin, GVariant* parameters) { 
	GVariantBuilder *builder;
	guint start, max;
	gchar *order;
	gboolean reverse;
	g_variant_get(parameters, "(uusb)", &start, &max, &order, &reverse);
	gchar ** lists = get_playlist_names_db(cwin);
	builder = g_variant_builder_new (G_VARIANT_TYPE ("(a(oss))"));
	g_variant_builder_open(builder, G_VARIANT_TYPE("a(oss)"));
	gint i = 0;
	gint imax = max;
	while(lists[i]) {
		if(i >= start && imax > 0) {
			gchar *listpath = g_strdup_printf("%s/Playlists/%d", MPRIS_PATH, i);
			g_variant_builder_add (builder, "(oss)", listpath, lists[i], "");
			g_free(listpath);
			imax--;
		}
		i++;
	}
	g_free(lists);
	g_variant_builder_close(builder);
	return g_variant_builder_end(builder); 
}
static GVariant* mpris_Playlists_get_ActivePlaylist(struct con_win *cwin) { 
	return g_variant_new("(b(oss))", 
		FALSE, "/", "invalid", "invalid");
}
static GVariant* mpris_Playlists_get_Orderings(struct con_win *cwin) {
	return g_variant_parse(G_VARIANT_TYPE("as"), 
		"['UserDefined']", NULL, NULL, NULL);
}
static GVariant* mpris_Playlists_get_PlaylistCount(struct con_win *cwin) { 
	return g_variant_new_uint32(get_playlist_count_db(cwin)); 
}

gboolean handle_path_request(struct con_win *cwin, const gchar *dbus_path, 
		struct musicobject **mobj, GtkTreePath **tree_path) {
	gchar *base = g_strdup_printf("%s/TrackList/", MPRIS_PATH);
	gboolean found = FALSE;
	*mobj = NULL;
	if(g_str_has_prefix(dbus_path, base)) {
		
		void *request = NULL;
		sscanf(dbus_path + strlen(base), "%p", &request);
		
		if(request) {
			GtkTreePath *path = current_playlist_path_at_mobj(request, cwin);
			if(path) {
				found = TRUE;
				*mobj = request;
				if(tree_path)
					*tree_path = path;
				else
					gtk_tree_path_free(path);
			}
		}
	}
	g_free(base);
	return found;
}

/* org.mpris.MediaPlayer2.TrackList */
static GVariant* mpris_TrackList_GetTracksMetadata(struct con_win *cwin, GVariant* parameters) { 
	/* In: (ao) out: aa{sv} */
	
	GVariant *param1 = g_variant_get_child_value(parameters, 0);
	gsize i, length;
	GVariantBuilder *b = g_variant_builder_new (G_VARIANT_TYPE ("(aa{sv})"));
	const gchar *path;
	
	g_variant_builder_open(b, G_VARIANT_TYPE("aa{sv}"));
	
	length = g_variant_n_children(param1);
	
	for(i = 0; i < length; i++) {
		g_variant_builder_open(b, G_VARIANT_TYPE("a{sv}"));
		struct musicobject *mobj= NULL;
		path = g_variant_get_string(g_variant_get_child_value(param1, i), NULL);
		if (handle_path_request(cwin, path, &mobj, NULL)) {
			
			handle_get_metadata(mobj, b);
			
		} else {
			
			g_variant_builder_add (b, "{sv}", "mpris:trackid", 
			g_variant_new_object_path(path));
		}
		g_variant_builder_close(b);
	}
	
	g_variant_builder_close(b);
	
	return g_variant_builder_end(b); 
}
static GVariant* mpris_TrackList_AddTrack(struct con_win *cwin, GVariant* parameters) {
	g_dbus_method_invocation_return_dbus_error(cwin->cmpris2->method_invocation, 
		DBUS_ERROR_NOT_SUPPORTED, "TrackList is read-only.");
	return NULL; 
}
static GVariant* mpris_TrackList_RemoveTrack(struct con_win *cwin, GVariant* parameters) { 
	g_dbus_method_invocation_return_dbus_error(cwin->cmpris2->method_invocation, 
		DBUS_ERROR_NOT_SUPPORTED, "TrackList is read-only.");
	return NULL; 
}
static GVariant* mpris_TrackList_GoTo(struct con_win *cwin, GVariant* parameters) {
	gchar *path = NULL;
	GtkTreePath *tree_path = NULL;
	g_variant_get(parameters, "(o)", &path);
	struct musicobject *mobj = NULL;
	if(handle_path_request(cwin, path, &mobj, &tree_path)) {
		// Dangerous: reusing double-click handler here.
		current_playlist_row_activated_cb(
			GTK_TREE_VIEW(cwin->current_playlist), tree_path, NULL, cwin);
	} else
		g_dbus_method_invocation_return_dbus_error(cwin->cmpris2->method_invocation, 
				DBUS_ERROR_INVALID_ARGS, "Unknown or malformed playlist object path.");
	return NULL; 
}
static GVariant* mpris_TrackList_get_Tracks(struct con_win *cwin) { 
	GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("ao"));
	GtkTreeModel *model;
	GtkTreeIter iter;
	struct musicobject *mobj = NULL;

	// TODO: remove tree access
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->current_playlist));
	
	if (!gtk_tree_model_get_iter_first(model, &iter))
		goto bad;

	do {
		gtk_tree_model_get(model, &iter, P_MOBJ_PTR, &mobj, -1);
		if (mobj) {
			g_variant_builder_add_value(builder, handle_get_trackid(mobj));
		}
	} while(gtk_tree_model_iter_next(model, &iter));

bad:	
	return g_variant_builder_end(builder); 
}
static GVariant* mpris_TrackList_get_CanEditTracks(struct con_win *cwin) { 
	return g_variant_new_boolean(FALSE); 
}


/* dbus callbacks */
static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data) {
	struct con_win *cwin = user_data;
	cwin->cmpris2->method_invocation = invocation;
	/* org.mpris.MediaPlayer2 */
	BEGIN_INTERFACE(0)
		MAP_METHOD(Root, Raise)
		MAP_METHOD(Root, Quit)
	END_INTERFACE
	/* org.mpris.MediaPlayer2.Player */
	BEGIN_INTERFACE(1)
		MAP_METHOD(Player, Next)
		MAP_METHOD(Player, Previous)
		MAP_METHOD(Player, Pause)
		MAP_METHOD(Player, PlayPause)
		MAP_METHOD(Player, Stop)
		MAP_METHOD(Player, Play)
		MAP_METHOD(Player, Seek)
		MAP_METHOD(Player, SetPosition)
		MAP_METHOD(Player, OpenUri)
	END_INTERFACE
	/* org.mpris.MediaPlayer2.Playlists */
	BEGIN_INTERFACE(2)
		MAP_METHOD(Playlists, ActivatePlaylist)
		MAP_METHOD(Playlists, GetPlaylists)
	END_INTERFACE
	/* org.mpris.MediaPlayer2.TrackList */
	BEGIN_INTERFACE(3)
		MAP_METHOD(TrackList, GetTracksMetadata)
		MAP_METHOD(TrackList, AddTrack)
		MAP_METHOD(TrackList, RemoveTrack)
		MAP_METHOD(TrackList, GoTo)
	END_INTERFACE
	cwin->cmpris2->method_invocation = NULL;
}

static GVariant *
handle_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data) {
	struct con_win *cwin = user_data;
	cwin->cmpris2->property_error = error;
	/* org.mpris.MediaPlayer2 */
	BEGIN_INTERFACE(0)
		PROPGET(Root, CanQuit)
		PROPGET(Root, CanRaise)
		PROPGET(Root, HasTrackList)
		PROPGET(Root, Identity)
		PROPGET(Root, DesktopEntry)
		PROPGET(Root, SupportedUriSchemes)
		PROPGET(Root, SupportedMimeTypes)
	END_INTERFACE
	/* org.mpris.MediaPlayer2.Player */
	BEGIN_INTERFACE(1)
		PROPGET(Player, PlaybackStatus)
		PROPGET(Player, LoopStatus)
		PROPGET(Player, Rate)
		PROPGET(Player, Shuffle)
		PROPGET(Player, Metadata)
		PROPGET(Player, Volume)
		PROPGET(Player, Position)
		PROPGET(Player, MinimumRate)
		PROPGET(Player, MaximumRate)
		PROPGET(Player, CanGoNext)
		PROPGET(Player, CanGoPrevious)
		PROPGET(Player, CanPlay)
		PROPGET(Player, CanPause)
		PROPGET(Player, CanSeek)
		PROPGET(Player, CanControl)
	END_INTERFACE
	/* org.mpris.MediaPlayer2.Playlists */
	BEGIN_INTERFACE(2)
		PROPGET(Playlists, PlaylistCount)
		PROPGET(Playlists, Orderings)
		PROPGET(Playlists, ActivePlaylist)
	END_INTERFACE
	/* org.mpris.MediaPlayer2.TrackList */
	BEGIN_INTERFACE(3)
		PROPGET(TrackList, Tracks)
		PROPGET(TrackList, CanEditTracks)
	END_INTERFACE
	cwin->cmpris2->property_error = NULL;
	return NULL;
}

static gboolean
handle_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
	struct con_win *cwin = user_data;
	cwin->cmpris2->property_error = error;
	/* org.mpris.MediaPlayer2 */
	BEGIN_INTERFACE(0)
		/* all properties readonly */
	END_INTERFACE
	/* org.mpris.MediaPlayer2.Player */
	BEGIN_INTERFACE(1)
		PROPPUT(Player, LoopStatus)
		PROPPUT(Player, Rate)
		PROPPUT(Player, Shuffle)
		PROPPUT(Player, Volume)
	END_INTERFACE
	/* org.mpris.MediaPlayer2.Playlists */
	BEGIN_INTERFACE(2)
		/* all properties readonly */
	END_INTERFACE
	/* org.mpris.MediaPlayer2.TrackList */
	BEGIN_INTERFACE(3)
		/* all properties readonly */
	END_INTERFACE
	cwin->cmpris2->property_error = NULL;
	return (NULL == *error);
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  handle_get_property,
  handle_set_property
};

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
	guint registration_id;
	gint i;
	struct con_win *cwin = user_data;

	for(i = 0; i < 4; i++)
	{
		cwin->cmpris2->interface_quarks[i] = g_quark_from_string(cwin->cmpris2->introspection_data->interfaces[i]->name);
		registration_id = g_dbus_connection_register_object (connection,
									MPRIS_PATH,
									cwin->cmpris2->introspection_data->interfaces[i],
									&interface_vtable,
									cwin,  /* user_data */
									NULL,  /* user_data_free_func */
									NULL); /* GError** */
		g_assert (registration_id > 0);
	}
	
	cwin->cmpris2->dbus_connection = connection;
	g_object_ref(G_OBJECT(cwin->cmpris2->dbus_connection));

}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	CDEBUG(DBG_INFO, "Acquired DBus name %s", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
	struct con_win *cwin = user_data;
	if(NULL != cwin->cmpris2->dbus_connection) {
		g_object_unref(G_OBJECT(cwin->cmpris2->dbus_connection));
		cwin->cmpris2->dbus_connection = NULL;
	}

	CDEBUG(DBG_INFO, "Lost DBus name %s", name);
}

/* pragha callbacks */
void mpris_update_any(struct con_win *cwin) {
	gboolean change_detected = FALSE;
	GVariantBuilder *b;
	gchar *newtitle;

	if(NULL == cwin->cmpris2->dbus_connection)
		return; /* better safe than sorry */
	if(NULL == cwin->cstate->curr_mobj)
		return;
	
	newtitle = cwin->cstate->curr_mobj->file;
	b = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	if(cwin->cmpris2->saved_shuffle != cwin->cpref->shuffle)
	{
		change_detected = TRUE;
		cwin->cmpris2->saved_shuffle = cwin->cpref->shuffle;
		g_variant_builder_add (b, "{sv}", "Shuffle", mpris_Player_get_Shuffle(cwin));
	}
	if(cwin->cmpris2->state != cwin->cstate->state)
	{
		change_detected = TRUE;
		cwin->cmpris2->state = cwin->cstate->state;
		g_variant_builder_add (b, "{sv}", "PlaybackStatus", mpris_Player_get_PlaybackStatus(cwin));
	}
	if(cwin->cmpris2->saved_playbackstatus != cwin->cpref->repeat)
	{
		change_detected = TRUE;
		cwin->cmpris2->saved_playbackstatus = cwin->cpref->repeat;
		g_variant_builder_add (b, "{sv}", "LoopStatus", mpris_Player_get_LoopStatus(cwin));
	}
	if(g_strcmp0(cwin->cmpris2->saved_title, newtitle))
	{
		change_detected = TRUE;
		if(cwin->cmpris2->saved_title)
			g_free(cwin->cmpris2->saved_title);
		if(newtitle)
			cwin->cmpris2->saved_title = g_strdup(newtitle);
		else 
			cwin->cmpris2->saved_title = NULL;
		g_variant_builder_add (b, "{sv}", "Metadata", mpris_Player_get_Metadata(cwin));
	}
	if(change_detected)
	{
		GVariant * tuples[] = {
			g_variant_new_string("org.mpris.MediaPlayer2.Player"),
			g_variant_builder_end(b),
			g_variant_parse(G_VARIANT_TYPE("as"), "[]", NULL, NULL, NULL)
		};
		if(g_variant_is_floating(tuples[0]))
			g_variant_ref_sink(tuples[0]);
		if(g_variant_is_floating(tuples[1]))
			g_variant_ref_sink(tuples[1]);
		if(g_variant_is_floating(tuples[2]))
			g_variant_ref_sink(tuples[2]);
		g_dbus_connection_emit_signal(cwin->cmpris2->dbus_connection, NULL, MPRIS_PATH,
			"org.freedesktop.DBus.Properties", "PropertiesChanged",
			g_variant_new_tuple(tuples, 3) , NULL);
	}
}

void mpris_update_mobj_remove(struct con_win *cwin, struct musicobject *mobj) {

	GVariant * tuples[1];
	if(NULL == cwin->cmpris2->dbus_connection)
		return; /* better safe than sorry */

	tuples[0] = handle_get_trackid(mobj);
	
	g_dbus_connection_emit_signal(cwin->cmpris2->dbus_connection, NULL, MPRIS_PATH, 
		"org.mpris.MediaPlayer2.TrackList", "TrackRemoved", 
		g_variant_new_tuple(tuples, 1), NULL);
}

void mpris_update_mobj_added(struct con_win *cwin, struct musicobject *mobj, GtkTreeIter *iter) {
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	struct musicobject *prev = NULL;
	GVariantBuilder *b;
	
	if(NULL == cwin->cmpris2->dbus_connection)
		return; /* better safe than sorry */

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->current_playlist));
	if(NULL == model)
		return;
	b = g_variant_builder_new (G_VARIANT_TYPE ("(a{sv}o)"));
	path = gtk_tree_model_get_path(model, iter);

	if (gtk_tree_path_prev(path)) {
		prev = current_playlist_mobj_at_path(path, cwin);
	}
	gtk_tree_path_free(path);
	
	g_variant_builder_open(b, G_VARIANT_TYPE("a{sv}"));
	handle_get_metadata(mobj, b);
	g_variant_builder_close(b);

	g_variant_builder_add_value(b, (prev) ?
		handle_get_trackid(prev) : 
		g_variant_new_object_path("/"));
		// or use g_variant_new_string(""); ?
		// "/" is the only legal empty object path, but 
		// the spec wants an empty string. What do the others do?
	
	g_dbus_connection_emit_signal(cwin->cmpris2->dbus_connection, NULL, MPRIS_PATH, 
		"org.mpris.MediaPlayer2.TrackList", "TrackAdded", 
		g_variant_builder_end(b), NULL);
}

void mpris_update_mobj_changed(struct con_win *cwin, struct musicobject *mobj, gint bitmask) {
	GVariantBuilder *b;

	if(NULL == cwin->cmpris2->dbus_connection)
		return; /* better safe than sorry */

	b = g_variant_builder_new (G_VARIANT_TYPE ("(a{sv})"));
	g_variant_builder_open(b, G_VARIANT_TYPE("a{sv}"));
	
	// should we only submit the changed metadata here? The spec is not clear.
	// If yes, use the portions in the bitmask parameter only.
	handle_get_metadata(mobj, b);
	
	g_variant_builder_close(b);
	
	g_dbus_connection_emit_signal(cwin->cmpris2->dbus_connection, NULL, MPRIS_PATH, 
		"org.mpris.MediaPlayer2.TrackList", "TrackChanged", 
		g_variant_builder_end(b), NULL);
}

void mpris_update_tracklist_changed(struct con_win *cwin) {
	GVariantBuilder *b = g_variant_builder_new (G_VARIANT_TYPE ("(aoo)"));
	GtkTreeModel *model;
	GtkTreeIter iter;
	struct musicobject *mobj = NULL;
	
	if(NULL == cwin->cmpris2->dbus_connection)
		return; /* better safe than sorry */

	g_variant_builder_open(b, G_VARIANT_TYPE("ao"));

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(cwin->current_playlist));
	
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	do {
		gtk_tree_model_get(model, &iter, P_MOBJ_PTR, &mobj, -1);
		g_variant_builder_add_value(b, handle_get_trackid(mobj));
	} while(gtk_tree_model_iter_next(model, &iter));
	
	g_variant_builder_close(b);
	g_variant_builder_add_value(b, handle_get_trackid(cwin->cstate->curr_mobj));
	g_dbus_connection_emit_signal(cwin->cmpris2->dbus_connection, NULL, MPRIS_PATH, 
		"org.mpris.MediaPlayer2.TrackList", "TrackListChanged", 
		g_variant_builder_end(b), NULL);
	
}

gint mpris_init(struct con_win *cwin)
{
	if (!cwin->cpref->use_mpris2)
		return 0;

	CDEBUG(DBG_INFO, "Initializing MPRIS");
	g_type_init();

	cwin->cmpris2->saved_shuffle = false;
	cwin->cmpris2->saved_playbackstatus = false;
	cwin->cmpris2->saved_title = NULL;

	cwin->cmpris2->introspection_data = g_dbus_node_info_new_for_xml (mpris2xml, NULL);
	g_assert (cwin->cmpris2->introspection_data != NULL);

	cwin->cmpris2->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
				MPRIS_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				on_bus_acquired,
				on_name_acquired,
				on_name_lost,
				cwin,
				NULL);
	return (cwin->cmpris2->owner_id) ? 0 : -1;
}

void mpris_close(struct con_win *cwin)
{
	if(NULL != cwin->cmpris2->dbus_connection)
		g_bus_unown_name(cwin->cmpris2->owner_id);

	if(NULL != cwin->cmpris2->introspection_data) {
		g_dbus_node_info_unref(cwin->cmpris2->introspection_data);
		cwin->cmpris2->introspection_data = NULL;
	}
	if(NULL != cwin->cmpris2->dbus_connection) {
		g_object_unref(G_OBJECT(cwin->cmpris2->dbus_connection));
		cwin->cmpris2->dbus_connection = NULL;
	}
}

void mpris_cleanup(struct con_win *cwin)
{
	mpris_close(cwin);
	g_slice_free(struct con_mpris2, cwin->cmpris2);
}

// still todo: 
// * emit Player.Seeked signal when user seeks in track or playback starts not from begin
// * emit Playlists.PlaylistChanged signal when playlist rename is implemented
// * provide an Icon for a playlist when e.g. 'smart playlists' are implemented
// * emit couple of TrackList signals when drag'n drop reordering
// * find a better object path than mobj address & remove all gtk tree model access
// * [optional] implement tracklist edit
#endif
