# SOURCES =
#include src/include.am

SUBDIRS = src src/include src/test pkgconfig
ACLOCAL_AMFLAGS = -I m4

@CODE_COVERAGE_RULES@

@VALGRIND_CHECK_RULES@

ci: test mem-check

test: check
	$(top_builddir)/src/test/test

code-check:
	splint +trytorecover src/*.h src/**.c `pkg-config --cflags glib-2.0` -preproc

VALGRIND_SUPPRESSIONS = \
	valgrind.supp

VALGRIND_ARGS = --track-origins=yes --quiet --error-exitcode=2 --leak-check=full \
	--suppressions=$(VALGRIND_SUPPRESSIONS)

mem-check: check
	valgrind $(VALGRIND_ARGS) $(top_builddir)/src/test/test

cache-check: check
	valgrind --tool=cachegrind --branch-sim=yes --cachegrind-out-file=cachegrindout.out $(top_builddir)/src/test/test
	cg_annotate cachegrindout.out

coverage-check: test
	@make check-code-coverage

code-style-doctor: code-style
	git diff --exit-code .

code-style:
	clang-format -style=file -i config.h src/*.h src/*.c src/**/*.h src/**/*.c

LOOPS = 10
test-loop:
	for ((i=1; i <= ${LOOPS}; ++i)) do make test && echo $$i || break; done

# For the make check-valgrind
check-valgrind-memcheck: check
	valgrind $(VALGRIND_ARGS) $(top_builddir)/src/test/test

check-valgrind-helgrind: check
	valgrind --tool=helgrind $(top_builddir)/src/test/test

check-valgrind-drd: check
	valgrind --tool=drd $(top_builddir)/src/test/test

# Finds todo items
TODO_TYPES ?= HACK TODO @todo FIXME BUG XXX PERF FUTURE REMOVE MAGIC UNIFY
TODO_LOCATIONS ?= src/*.c src/test/*.c Makefile.am
todo:
	@(find $(TODO_LOCATIONS) -name '*.h' -or -name '*.c') | xargs egrep --color=auto -w \
		`echo $(TODO_TYPES) | tr ' ' '|'`
	@echo '============================='
	@(for i in $(TODO_TYPES); do \
	  (find $(TODO_LOCATIONS) -name '*.h' -or -name '*.c') | xargs egrep -w $$i > /dev/null || continue; \
	  /bin/echo -n $$i'       ' | head -c 10; \
	  (find $(TODO_LOCATIONS) -name '*.h' -or -name '*.c') | xargs egrep -w $$i| wc -l; \
	done)
	@echo '============================='
	@echo -n 'Total     '
	@(find $(TODO_LOCATIONS) -name '*.h' -or -name '*.c') | xargs egrep -w \
		`echo $(TODO_TYPES) | tr ' ' '|'` | wc -l
