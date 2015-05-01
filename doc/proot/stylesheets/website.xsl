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
	<link rel="stylesheet" href="proot-website.css" type="text/css" />
	<meta itemprop="name" content="PRoot" />
	<meta itemprop="description">
	  <xsl:attribute name="content">
	    <xsl:value-of select="//section[@names='description']/paragraph[1]" />
	  </xsl:attribute>
	</meta>
      </head>

      <body>

	<div id="title">
	  <h1>PRoot</h1>
	  <xsl:text> </xsl:text>
	  <div class="g-plusone" data-size="small">
	    <xsl:comment>By default XSLTproc converts tags with no
	    content to self-closing tags</xsl:comment>
	  </div>
	</div>

	  <div id="contents">
	    <ul>
	      <li><a href="#description">Description</a></li>
	      <li><a href="#examples">Examples</a></li>
	      <li><a href="#downloads">Downloads</a></li>
	      <li><a href="#ecosystem">Ecosystem</a></li>
	      <li><a href="#support">Support</a></li>
	      <li><a href="https://plus.google.com/107605112469213359575/posts">News+</a></li>
	    </ul>
	  </div>

	  <xsl:apply-templates select="//section[@names='description']" />
	  <xsl:apply-templates select="//section[@names='examples']" />
	  <xsl:apply-templates select="//section[@names='downloads']" />
	  <xsl:apply-templates select="//section[@names='ecosystem']" />

	  <div class="section" id="support">
	    <h2>Support</h2>
	    <p>Feel free to send your questions, bug reports,
	    suggestions, and patchs to <a
	    href="mailto:proot_me@googlegroups.com">the
	    mailing-list</a> or to <a
	    href="https://groups.google.com/forum/?fromgroups#!forum/proot_me">the
	    forum</a>, but please be sure that your answer isn't in
	    the <a
	    href="https://github.com/proot-me/PRoot/blob/master/doc/proot/manual.txt">user
	    manual</a> first.
	    </p>
	  </div>

      </body>
    </html>

  </xsl:template>
</xsl:transform>
