prefix ?= /usr
bindir ?= $(prefix)/bin

EXEC = rtc-range rtc rtc-sync

all: $(EXEC)

clean:
	$(RM) $(EXEC)

install:
	install -d $(DESTDIR)$(bindir)
	install $(EXEC) $(DESTDIR)$(bindir)

uninstall:
	$(RM) -r $(addprefix $(DESTDIR)$(bindir)/,$(EXEC))
