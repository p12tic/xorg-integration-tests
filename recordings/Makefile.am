recordingsdir = $(datadir)/recordings
# FIXME: avoid using spaces in file names or the following will fail
dist_recordings_DATA  = $(shell find $(top_srcdir)/recordings \( -name \*.desc -o -name \*.events \) -printf "%P\n")

all-local:
	@find  $(top_srcdir)/recordings/*/ -type f -and -not -name "*.desc" -and -not -name "*.events" -printf "\nWARNING: file '%p' does not end in .desc or .events\n\n"
