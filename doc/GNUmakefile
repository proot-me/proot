all: proot.1 cli.h proot.spec index.html

proot.1: manual.txt
	rst2man.py $< $@

%.xml: %.txt
	rst2xml.py --no-doctype $< $@

%.html: %.txt
	rst2html.py $< $@

# Workaround to avoid unescaped C character.
manual-quoted.txt: manual.txt
	sed 's/"/\\\\"/g' $^ > $@

cli.h: stylesheets/cli.xsl manual-quoted.xml
	xsltproc --output $@ $^

proot.spec:  stylesheets/rpm-spec.xsl manual.xml
	xsltproc --output $@ $^

index.html:  stylesheets/website.xsl manual.xml
	xsltproc --output $@ $^

clean:
	rm -f *.xml manual-quoted.txt cli.h
