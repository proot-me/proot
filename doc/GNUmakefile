all: proot.1 proot.h proot.spec index.html

proot.1: proot-manual.txt
	rst2man $< $@

%.xml: %.txt
	rst2xml --no-doctype $< $@

%.html: %.txt
	rst2html $< $@

# Workaround to avoid unescaped C character.
proot-manual-quoted.txt: proot-manual.txt
	sed 's/"/\\\\"/g' $^ > $@

proot.h: stylesheets/proot-cli.xsl proot-manual-quoted.xml
	xsltproc --output $@ $^

proot.spec:  stylesheets/rpm-spec.xsl proot-manual.xml
	xsltproc --output $@ $^
	echo "* $(shell date +'%a %b %d %Y') Cédric VINCENT <cedric.vincent@st.com>" >> $@
	cat changelog.txt >> $@

index.html:  stylesheets/website.xsl proot-manual.xml
	xsltproc --output $@ $^

clean:
	rm -f *.xml proot-manual-quoted.txt proot.h
