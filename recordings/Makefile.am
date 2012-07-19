recordingsdir = $(datadir)/recordings
# FIXME
# That find comment below is broken, it won't print any *.desc file
# Should use instead: $(shell find $(top_srcdir)/recordings \( -name \*.desc -o -name \*.events \) -print "'%P'\n" )
# But then it finds all the file but fails because they contain spaces in their names.
dist_recordings_DATA  = $(shell find $(top_srcdir)/recordings -name "*.desc" -or -name "*.events" -printf "%P\n")

all-local:
	@find  $(top_srcdir)/recordings/*/ -type f -and -not -name "*.desc" -and -not -name "*.events" -printf "\nWARNING: file '%p' does not end in .desc or .events\n\n"
