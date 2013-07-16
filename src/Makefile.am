bin_PROGRAMS = statsd-tg

statsd_tg_SOURCES = statsd-tg.c
statsd_tg_CFLAGS = $(AM_CFLAGS) -pthread
statsd_tg_LDADD = -lrt

EXTRA_DIST = statsd-tg.pod

dist_man_MANS = statsd-tg.1

.pod.1:
	pod2man --release=$(VERSION) --center=$(PACKAGE) $< \
		>.pod2man.tmp.$$$$ 2>/dev/null && mv -f .pod2man.tmp.$$$$ $@ || true
	@if grep '\<POD ERRORS\>' $@ >/dev/null 2>&1; \
	then \
		echo "$@ has some POD errors!"; false; \
	fi
