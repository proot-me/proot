<?xml version="1.0" encoding="UTF-8"?>

<xsl:transform version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:import href = "../../stylesheets/website.xsl" />

  <xsl:template match="/">
    <html xmlns="http://www.w3.org/1999/xhtml" itemscope="" itemtype="http://schema.org/Product" xml:lang="en" lang="en">

      <head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
	<title>
	  <xsl:value-of select="document/title" /> — <xsl:value-of select="document/subtitle" />
	</title>
	<link rel="stylesheet" href="care.css" type="text/css" />
	<meta itemprop="name" content="CARE" />
	<meta itemprop="description">
	  <xsl:attribute name="content">
	    <xsl:value-of select="//section[@names='description']/paragraph[1]" />
	  </xsl:attribute>
	</meta>
      </head>

      <body>

	<div id="title">
	  <h1>CARE</h1>
	  <xsl:text> </xsl:text>
	</div>

	  <div id="contents">
	    <ul>
	      <li><a href="#description">Description</a></li>
	      <li><a href="#example">Example</a></li>
	      <li><a href="https://github.com/proot-me/proot">Source</a></li>
	      <li><a href="#downloads">Downloads</a></li>
	      <li><a href="#support">Support</a></li>
	    </ul>
	  </div>

	  <xsl:apply-templates select="//section[@names='description']" />
	  <xsl:apply-templates select="//section[@names='example']" />
	  <xsl:apply-templates select="//section[@names='downloads']" />

	  <div class="section" id="support">
	    <h2>Support</h2>
	    <p>Feel free to send your questions, bug reports,
	    suggestions, and patches to <a
	    href="mailto:proot_me@googlegroups.com">the
	    mailing-list</a> or to <a
	    href="https://groups.google.com/forum/?fromgroups#!forum/proot_me">the
	    forum</a>, or chat with us on <a href="https://gitter.im/proot-me/devs">Gitter</a>;
            but please be sure that your answer isn't in the
            <a href="https://raw.githubusercontent.com/proot-me/proot/master/doc/care/manual.rst">user
	    manual</a> first.
	    </p>

	  </div>
	  <a href="#" style="float: right; position: sticky;bottom: 10px;">top</a>
      </body>
    </html>

  </xsl:template>
</xsl:transform>
