OUTPUTS = proot/man.1 proot.h proot/rpm-spec proot/index.html care.h care/index.html
all: $(OUTPUTS)

%/man.1: %/manual.txt
	rst2man $< $@

%.xml: %.txt
	rst2xml --no-doctype $< $@

%.html: %.txt
	rst2html $< $@

# Workaround to avoid unescaped C character.
%/manual-quoted.txt: %/manual.txt
	sed 's/"/\\\\"/g' $^ > $@

%.h: %/stylesheets/cli.xsl %/manual-quoted.xml
	xsltproc --output $@ $^

%/rpm-spec:  %/stylesheets/rpm-spec.xsl %/manual.xml # %/changelog.txt
	xsltproc --output $@ $^
	echo "* $(shell date +'%a %b %d %Y') Cédric VINCENT <cedric.vincent@st.com>" >> $@
	cat $*/changelog.txt >> $@

%/index.html: stylesheets/website.xsl %/stylesheets/website.xsl %/manual.xml
	xsltproc --output $@ $*/stylesheets/website.xsl $*/manual.xml

clean:
	rm -f *.xml $(OUTPUTS) *-quoted.*
