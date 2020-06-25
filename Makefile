# Makefile for PROOT.

ifneq ($(words $(subst :, ,$(CURDIR))), 1)
  $(error main directory cannot contain spaces nor colons)
endif

# Always point to the root of the build tree (needs GNU make).
BUILD_DIR=$(CURDIR)

# Before including a proper config-host.mk, assume we are in the source tree
SRC_PATH=.

UNCHECKED_GOALS := %clean TAGS ctags dist \
    html info pdf txt \
    help check-help print-% \
    docker docker-% vm-help vm-test vm-build-%

# All following code might depend on configuration variables
ifneq ($(wildcard config-host.mk),)
# Put the all: rule here so that config-host.mk can contain dependencies.
all:
include config-host.mk

# Check that we're not trying to do an out-of-tree build from
# a tree that's been used for an in-tree build.
ifneq ($(realpath $(SRC_PATH)),$(realpath .))
ifneq ($(wildcard $(SRC_PATH)/config-host.mk),)
$(error This is an out of tree build but your source tree ($(SRC_PATH)) \
seems to have been used for an in-tree build. You can fix this by running \
"$(MAKE) distclean " in your source tree)
endif
endif

config-host.mk: $(SRC_PATH)/configure $(SRC_PATH)/VERSION
	@echo $@ is out-of-date, running configure
	@./config.status

else
config-host.mk:
ifneq ($(filter-out $(UNCHECKED_GOALS),$(MAKECMDGOALS)),$(if $(MAKECMDGOALS),,fail))
	@echo "Please call configure before running make!"
	@exit 1
endif
endif

include $(SRC_PATH)/rules.mk

# Create PROOT_PKGVERSION and FULL_VERSION strings
# If PKGVERSION is set, use that; otherwise get version and -dirty status from git
PROOT_PKGVERSION := $(if $(PKGVERSION),$(PKGVERSION),$(shell \
  cd $(SRC_PATH); \
  if test -e .git; then \
    git describe --match 'v*' 2>/dev/null | tr -d '\n'; \
    if ! git diff-index --quiet HEAD &>/dev/null; then \
      echo "-dirty"; \
    fi; \
  fi))

# Either "version (pkgversion)", or just "version" if pkgversion not set
FULL_VERSION := $(if $(PROOT_PKGVERSION),$(VERSION) ($(PROOT_PKGVERSION)),$(VERSION))

# Don't try to regenerate Makefile or configure
# We don't generate any of them
Makefile: ;
configure: ;

.PHONY: all clean distclean html info install install-doc \
	recurse-all dist FORCE

$(call set-vpath, $(SRC_PATH))

# Sphinx does not allow building manuals into the same directory as
# the source files, so if we're doing an in-tree PROOT build we must
# build the manuals into a subdirectory (and then install them from
# there for 'make install'). For an out-of-tree build we can just
# use the docs/ subdirectory in the build tree as normal.
ifeq ($(realpath $(SRC_PATH)),$(realpath .))
MANUAL_BUILDDIR := docs/built
else
MANUAL_BUILDDIR := docs
endif

ifdef BUILD_DOCS
DOCS+=
else
DOCS=
endif

SUBDIR_MAKEFLAGS=$(if $(V),,--no-print-directory --quiet) BUILD_DIR=$(BUILD_DIR)

ifneq ($(wildcard config-host.mk),)
include $(SRC_PATH)/src/Makefile.objs
endif

dummy := $(call unnest-vars,, \
                stub-obj-y \
                util-obj-y \
                common-obj-y \
                common-obj-m)

include $(SRC_PATH)/test/Makefile.include

all: $(DOCS) recurse-all

proot-version.h: FORCE
	$(call quiet-command, \
                (printf '#define PROOT_PKGVERSION "$(PROOT_PKGVERSION)"\n'; \
		printf '#define PROOT_FULL_VERSION "$(FULL_VERSION)"\n'; \
		) > $@.tmp)
	$(call quiet-command, if ! cmp -s $@ $@.tmp; then \
	  mv $@.tmp $@; \
	 else \
	  rm $@.tmp; \
	 fi)

config-host.h: config-host.h-timestamp
config-host.h-timestamp: config-host.mk

ifdef CONFIG_GCOV
.PHONY: clean-coverage
clean-coverage:
	$(call quiet-command, \
		find . \( -name '*.gcda' -o -name '*.gcov' \) -type f -exec rm {} +, \
		"CLEAN", "coverage files")
endif

clean:
	rm -f config.mk
	find . \( -name '*.so' -o -name '*.[oa]' \) -type f \
		-exec rm {} +

GIT_VERSION := $(shell git describe --tags `git rev-list --tags --max-count=1`)

GIT_COMMIT := $(shell git rev-list --all --max-count=1 | cut -c 1-8)

VERSION = $(GIT_VERSION)-$(GIT_COMMIT)

dist: proot-$(VERSION).tar.bz2

proot-%.tar.bz2:
	$(SRC_PATH)/scripts/make-release "$(SRC_PATH)" "$(patsubst proot-%.tar.bz2,%,$@)"

distclean: clean
	rm -f config-host.mk config-host.h* $(DOCS)
	rm -f config.log

install-doc: $(DOCS)
	$(INSTALL_DIR) "$(DESTDIR)$(proot_docdir)"
	$(INSTALL_DATA) $(MANUAL_BUILDDIR)/index.html "$(DESTDIR)$(proot_docdir)"
	$(INSTALL_DATA) docs/interop/proot-qmp-ref.html "$(DESTDIR)$(proot_docdir)"
	$(INSTALL_DATA) docs/interop/proot-qmp-ref.txt "$(DESTDIR)$(proot_docdir)"

install-datadir:
	$(INSTALL_DIR) "$(DESTDIR)$(proot_datadir)"

install-includedir:
	$(INSTALL_DIR) "$(DESTDIR)$(includedir)"

install: all $(if $(BUILD_DOCS),install-doc) \
	install-datadir install-localstatedir install-includedir \
	$(if $(INSTALL_BLOBS),$(edk2-decompressed)) \
	recurse-install

.PHONY: ctags
ctags:
	rm -f tags
	find "$(SRC_PATH)" -name '*.[hc]' -exec ctags --append {} +

.PHONY: TAGS
TAGS:
	rm -f TAGS
	find "$(SRC_PATH)" -name '*.[hc]' -exec etags --append {} +

# We assume all RST files in the manual's directory are used in it
manual-deps = $(wildcard $(SRC_PATH)/docs/$1/*.rst $(SRC_PATH)/docs/$1/*/*.rst)
# Macro to write out the rule and dependencies for building manpages
# Usage: $(call define-manpage-rule,manualname,manpage1 manpage2...[,extradeps])
# 'extradeps' is optional, and specifies extra files (eg .hx files) that
# the manual page depends on.
define define-manpage-rule
$(call atomic,$(foreach manpage,$2,$(MANUAL_BUILDDIR)/$1/$(manpage)),$(call manual-deps,$1) $3)
	$(call build-manual,$1,man)
endef

$(MANUAL_BUILDDIR)/devel/index.html: $(call manual-deps,devel)
	$(call build-manual,devel,html)

$(MANUAL_BUILDDIR)/index.html: $(SRC_PATH)/docs/index.html.in proot-version.h
	@mkdir -p "$(MANUAL_BUILDDIR)"
	$(call quiet-command, sed "s|@@VERSION@@|${VERSION}|g" $< >$@, \
             "GEN","$@")

html: doc

# Reports/Analysis

%/coverage-report.html:
	@mkdir -p $*
	$(call quiet-command,\
		gcovr -r $(SRC_PATH) \
		$(foreach t, $(TARGET_DIRS), --object-directory $(BUILD_DIR)/$(t)) \
		 --object-directory $(BUILD_DIR) \
		-p --html --html-details -o $@, \
		"GEN", "coverage-report.html")

.PHONY: coverage-report
coverage-report: $(CURDIR)/reports/coverage/coverage-report.html

# Add a dependency on the generated files, so that they are always
# rebuilt before other object files
ifneq ($(wildcard config-host.mk),)
ifneq ($(filter-out $(UNCHECKED_GOALS),$(MAKECMDGOALS)),$(if $(MAKECMDGOALS),,fail))
Makefile: $(generated-files-y)
endif
endif

# Include automatically generated dependency files
# Dependencies in Makefile.objs files come from our recursive subdir rules
-include $(wildcard *.d tests/*.d)

include $(SRC_PATH)/test/docker/Makefile.include
include $(SRC_PATH)/test/vagrant/Makefile.include

print-help-run = printf "  %-30s - %s\\n" "$1" "$2"
print-help = $(quiet-@)$(call print-help-run,$1,$2)

.PHONY: help
help:
	@echo  'Generic targets:'
	$(call print-help,all,Build all)
ifdef CONFIG_MODULES
	$(call print-help,extensions,Build all extensions)
endif
	$(call print-help,dir/file.o,Build specified target only)
	$(call print-help,install,Install PROOT, documentation and tools)
	$(call print-help,ctags/TAGS,Generate tags file for editors)
	$(call print-help,cscope,Generate cscope index)
	@echo  ''
	@$(if $(TARGET_DIRS), \
		echo 'Architecture specific targets:'; \
		$(foreach t, $(TARGET_DIRS), \
		$(call print-help-run,$(t)/all,Build for $(t)); \
		$(if $(CONFIG_FUZZ), \
			$(if $(findstring softmmu,$(t)), \
				$(call print-help-run,$(t)/fuzz,Build fuzzer for $(t)); \
		))) \
		echo '')
	@$(if $(TOOLS), \
		echo 'Tools targets:'; \
		$(foreach t, $(TOOLS), \
		$(call print-help-run,$(t),Build $(shell basename $(t)) tool);) \
		echo '')
	@echo  'Cleaning targets:'
	$(call print-help,clean,Remove most generated files but keep the config)
ifdef CONFIG_GCOV
	$(call print-help,clean-coverage,Remove coverage files)
endif
	$(call print-help,distclean,Remove all generated files)
	$(call print-help,dist,Build a distributable tarball)
	@echo  ''
	@echo  'Test targets:'
	$(call print-help,check,Run all tests (check-help for details))
	$(call print-help,docker,Help about targets running tests inside containers)
	$(call print-help,vm-help,Help about targets running tests inside VM)
	@echo  ''
	@echo  'Documentation targets:'
	$(call print-help,html info pdf txt,Build documentation in specified format)
ifdef CONFIG_GCOV
	$(call print-help,coverage-report,Create code coverage report)
endif
	$(call print-help,$(MAKE) [targets],(quiet build, default))
	$(call print-help,$(MAKE) V=1 [targets],(verbose build))
