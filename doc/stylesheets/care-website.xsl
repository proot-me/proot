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
	<link rel="stylesheet" href="care-website.css" type="text/css" />
	<meta itemprop="name" content="CARE" />
	<meta itemprop="description">
	  <xsl:attribute name="content">
	    <xsl:value-of select="//section[@names='description']/paragraph[1]" />
	  </xsl:attribute>
	</meta>
	<script type="text/javascript">
	  (function(i,s,o,g,r,a,m){i['GoogleAnalyticsObject']=r;i[r]=i[r]||function(){
	  (i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();a=s.createElement(o),
	  m=s.getElementsByTagName(o)[0];a.async=1;a.src=g;m.parentNode.insertBefore(a,m)
	  })(window,document,'script','//www.google-analytics.com/analytics.js','ga');

	  ga('create', 'UA-46144158-1', 'reproducible.io');
	  ga('send', 'pageview');
	</script>
      </head>

      <body>

	<div id="title">
	  <h1>CARE</h1>
	  <xsl:text> </xsl:text>
	  <div class="g-plusone" data-size="small">
	    <xsl:comment>By default XSLTproc converts tags with no
	    content to self-closing tags</xsl:comment>
	  </div>
	</div>

	  <div id="contents">
	    <ul>
	      <li><a href="#description">Description</a></li>
	      <li><a href="#example">Example</a></li>
	      <li><a href="#downloads">Downloads</a></li>
	      <li><a href="#support">Support</a></li>
	      <li><a href="https://plus.google.com/107771643881342199474/posts">News+</a></li>
	    </ul>
	  </div>

	  <xsl:apply-templates select="//section[@names='description']" />
	  <xsl:apply-templates select="//section[@names='example']" />
	  <xsl:apply-templates select="//section[@names='downloads']" />

	  <div class="section" id="support">
	    <h2>Support</h2>
	    <p>Feel free to send your questions, bug reports,
	    suggestions, and patchs to <a
	    href="mailto:reproducible@googlegroups.com">the
	    mailing-list</a> or to <a
	    href="https://groups.google.com/forum/?fromgroups#!forum/reproducible">the
	    forum</a>, but please be sure that your answer isn't in
	    the <a
	    href="https://raw.github.com/cedric-vincent/PRoot/next/doc/care-manual.txt">user
	    manual</a> first.
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
