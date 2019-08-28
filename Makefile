.POSIX:
.SUFFIXES:
OUTDIR=.build
include $(OUTDIR)/config.mk

INCLUDE=-Iinclude

public_includes=\
		include/mrsh/arithm.h \
		include/mrsh/array.h \
		include/mrsh/ast.h \
		include/mrsh/buffer.h \
		include/mrsh/builtin.h \
		include/mrsh/entry.h \
		include/mrsh/getopt.h \
		include/mrsh/hashtable.h \
		include/mrsh/parser.h \
		include/mrsh/shell.h

tests=\
		test/conformance/if.sh \
		test/case.sh \
		test/command.sh \
		test/for.sh \
		test/function.sh \
		test/loop.sh \
		test/pipeline.sh \
		test/subshell.sh \
		test/syntax.sh \
		test/ulimit.sh \
		test/word.sh

include $(OUTDIR)/cppcache

.SUFFIXES: .c .o

.c.o:
	@mkdir -p $$(dirname "$@")
	@printf 'CC\t$@\n'
	@touch $(OUTDIR)/cppcache
	@grep $< $(OUTDIR)/cppcache >/dev/null || \
		$(CPP) $(INCLUDE) -MM -MT $@ $< >> $(OUTDIR)/cppcache
	@$(CC) -c $(CFLAGS) $(INCLUDE) -o $@ $<

$(OUTDIR)/libmrsh.a: $(libmrsh_objects)
	@printf 'AR\t$@\n'
	@$(AR) -csr $@ $(libmrsh_objects)

libmrsh.so.$(SOVERSION): $(OUTDIR)/libmrsh.a
	@printf 'LD\t$@\n'
	@$(CC) -shared $(LDFLAGS) -o $@ $(OUTDIR)/libmrsh.a

$(OUTDIR)/mrsh.pc:
	@printf 'MKPC\t$@\n'
	@PREFIX=$(PREFIX) ./mkpc $@

mrsh: $(OUTDIR)/libmrsh.a $(mrsh_objects)
	@printf 'CCLD\t$@\n'
	@$(CC) -o $@ $(LIBS) $(mrsh_objects) -L$(OUTDIR) -lmrsh

highlight: $(OUTDIR)/libmrsh.a $(highlight_objects)
	@printf 'CCLD\t$@\n'
	@$(CC) -o $@ $(LIBS) $(highlight_objects) -L$(OUTDIR) -lmrsh

check: mrsh $(tests)
	@for t in $(tests); do \
		printf '%-30s... ' "$$t" && \
		MRSH=./mrsh REF_SH=$${REF_SH:-sh} ./test/harness.sh $$t >/dev/null && \
		echo OK || echo FAIL; \
	done

install: mrsh libmrsh.so.$(SOVERSION) $(OUTDIR)/mrsh.pc
	mkdir -p $(BINDIR) $(LIBDIR) $(INCDIR)/mrsh $(PCDIR)
	install -m755 mrsh $(BINDIR)/mrsh
	install -m755 libmrsh.so.$(SOVERSION) $(LIBDIR)/libmrsh.so.$(SOVERSION)
	for inc in $(public_includes); do \
		install -m644 $$inc $(INCDIR)/mrsh/$$(basename $$inc); \
	done
	install -m644 $(OUTDIR)/mrsh.pc $(PCDIR)/mrsh.pc

clean:
	rm -rf \
		$(libmrsh_objects) \
		$(mrsh_objects) \
		$(highlight_objects) \
		mrsh highlight libmrsh.so.$(SOVERSION) $(OUTDIR)/mrsh.pc

mrproper: clean
	rm -rf $(OUTDIR)

.PHONY: all install clean check
