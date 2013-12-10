NON_PERSISTANT = proot-cli.h proot.spec proot-index.html care-cli.h care-index.html
all: proot.1 care.1 $(NON_PERSISTANT)

%.1: %-manual.txt
	rst2man $< $@

%.xml: %.txt
	rst2xml --no-doctype $< $@

%.html: %.txt
	rst2html $< $@

# Workaround to avoid unescaped C character.
%-manual-quoted.txt: %-manual.txt
	sed 's/"/\\\\"/g' $^ > $@

%-cli.h: stylesheets/%-cli.xsl %-manual-quoted.xml
	xsltproc --output $@ $^

%.spec:  stylesheets/rpm-spec.xsl %-manual.xml
	xsltproc --output $@ $^
	echo "* $(shell date +'%a %b %d %Y') Cédric VINCENT <cedric.vincent@st.com>" >> $@
	cat changelog.txt >> $@

%-index.html: stylesheets/website.xsl stylesheets/%-website.xsl %-manual.xml
	xsltproc --output $@ stylesheets/$*-website.xsl $*-manual.xml

clean:
	rm -f *.xml $(NON_PERSISTANT) *-quoted.*
