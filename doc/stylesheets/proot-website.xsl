<?xml version="1.0" encoding="UTF-8"?>

<xsl:transform version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:import href = "website.xsl" />

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
	<script type="text/javascript">
	  var _gaq = _gaq || [];
	  _gaq.push(['_setAccount', 'UA-20176046-1']);
	  _gaq.push(['_trackPageview']);
	  (function() {
	  var ga = document.createElement('script'); ga.type = 'text/javascript'; ga.async = true;
	  ga.src = ('https:' == document.location.protocol ? 'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';
	  var s = document.getElementsByTagName('script')[0]; s.parentNode.insertBefore(ga, s);
	  })();
	</script>
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
	      <li><a href="#support">Support</a></li>
	      <li><a href="https://plus.google.com/107605112469213359575/posts">News+</a></li>
	    </ul>
	  </div>

	  <xsl:apply-templates select="//section[@names='description']" />
	  <xsl:apply-templates select="//section[@names='examples']" />
	  <xsl:apply-templates select="//section[@names='downloads']" />

	  <div class="section" id="support">
	    <h2>Support</h2>
	    <p>Feel free to send your questions, bug reports,
	    suggestions, and patchs to <a
	    href="mailto:proot_me@googlegroups.com">the
	    mailing-list</a> or to <a
	    href="https://groups.google.com/forum/?fromgroups#!forum/proot_me">the
	    forum</a>, but please be sure that your answer isn't in
	    the <a
	    href="https://github.com/cedric-vincent/PRoot#proot">user
	    manual</a> first.
	    </p>
	    <p>Also, Rémi Duraffort has written interesting articles
	    on <a
	    href="http://ivoire.dinauz.org/blog/post/2012/04/16/PRoot-sorcery">how
	    to use a foreign Debian rootfs with PRoot</a> in order to
	    <a
	    href="http://ivoire.dinauz.org/blog/post/2012/05/04/Making-VLC-at-home">build
	    and test VLC on this guest Linux distribution</a>.
	    </p>
	  </div>

	  <script type="text/javascript">
	    (function() {
	    var po = document.createElement('script'); po.type = 'text/javascript'; po.async = true;
	    po.src = 'https://apis.google.com/js/plusone.js';
	    var s = document.getElementsByTagName('script')[0]; s.parentNode.insertBefore(po, s);
	    })();
	  </script>

      </body>
    </html>

  </xsl:template>
</xsl:transform>
