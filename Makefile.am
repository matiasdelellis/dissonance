SUBDIRS =								\
	po								\
	src

AUTOMAKE_OPTIONS =							\
	1.8								\
	dist-bzip2

man_MANS = data/pragha.1

doc_DATA = README ChangeLog NEWS FAQ

imagedir = $(datadir)/pixmaps/pragha
image_DATA = 		\
	data/album.png	\
	data/artist.png	\
	data/track.png  \
	data/cover.png  \
	data/genre.png	\
	data/pragha.png

iconsdir = $(datadir)/icons/hicolor/128x128/apps
icons_DATA = data/pragha.png

desktopdir = $(datadir)/applications
desktop_DATA = data/pragha.desktop

EXTRA_DIST = 		\
	$(text_DATA)	\
	$(doc_DATA)	\
	$(image_DATA)	\
	$(desktop_DATA)	\
	$(icons_DATA)	\
	$(man_MANS)

distclean-local:
	rm -rf *.cache *~

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then					\
		echo "Updating Gtk icon cache.";			\
		$(gtk_update_icon_cache);				\
	else								\
		echo "*** Icon cache not updated. Remember to run:";	\
		echo "***";						\
		echo "***   $(gtk_update_icon_cache)";			\
		echo "***";						\
	fi
