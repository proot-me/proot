<?xml version="1.0" encoding="UTF-8"?>

<!--
<?xml version="1.0" encoding="utf-8" ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
-->


<xsl:transform version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:template match="/">
    <html xmlns="http://www.w3.org/1999/xhtml" itemscope="" itemtype="http://schema.org/Product" xml:lang="en" lang="en">

      <head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
	<title>
	  <xsl:value-of select="document/title" /> — <xsl:value-of select="document/subtitle" />
	</title>
	<link rel="stylesheet" href="website.css" type="text/css" />
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
	  <div class="g-plusone" data-size="small" data-annotation="none">
	    <xsl:comment>By default XSLTproc converts tags with no
	    content to self-closing tags</xsl:comment>
	  </div>
	</div>

	  <div id="contents">
	    <ul>
	      <li><a href="#description">Description</a></li>
	      <li><a href="#examples">Examples</a></li>
	      <li><a href="#download">Download</a></li>
	      <li><a href="#help">Help</a></li>
	    </ul>
	  </div>

	  <xsl:apply-templates select="//section[@names='description']" />
	  <xsl:apply-templates select="//section[@names='examples']" />

	  <div class="section" id="download">
	    <h2>Download</h2>
	    <p>
	      PRoot sources are hosted on <a
	      href="https://github.com/cedric-vincent/PRoot">GitHub</a>
	      and the latest release is <a
	      href="http://software.opensuse.org/download.html?project=home:cedric-vincent&#38;package=proot">packaged
	      for most PC Linux distributions</a>, choose wisely ;)
	    </p>
	  </div>

	  <div class="section" id="help">
	    <h2>Help</h2>
	    <p>Feel free to send your questions, bug reports,
	    suggestions, and patchs to <a
	    href="mailto:proot_me@googlegroups.com">the
	    mailing-list</a> or to <a
	    href="https://groups.google.com/forum/?fromgroups#!forum/proot_me">the
	    forum</a>, but please be sure that your answer isn't in
	    the <a
	    href="https://github.com/cedric-vincent/PRoot/blob/master/doc/manual.txt">user
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
  
  <xsl:template match="section">
    <div class="section">
      <xsl:attribute name="id"><xsl:value-of select="@names" />
      </xsl:attribute>
      <xsl:apply-templates/>
    </div>
  </xsl:template>

  <xsl:template match="section/title">
    <h2>
      <xsl:apply-templates/>
    </h2>
  </xsl:template>

  <xsl:template match="section/section/title">
    <h3>
      <xsl:apply-templates/>
    </h3>
  </xsl:template>

  <xsl:template match="paragraph">
    <p>
      <xsl:apply-templates/>
    </p>
  </xsl:template>

  <xsl:template match="literal">
    <tt>
      <xsl:apply-templates/>
    </tt>
  </xsl:template>

  <xsl:template match="emphasis">
    <em>
      <xsl:apply-templates/>
    </em>
  </xsl:template>

  <xsl:template match="literal_block">
    <pre>
      <xsl:apply-templates/>
    </pre>
  </xsl:template>

  <xsl:template match="strong">
    <strong>
      <xsl:apply-templates/>
    </strong>
  </xsl:template>

  <xsl:template match="comment">
  </xsl:template>

  <xsl:template match="reference">
    <a>
      <xsl:attribute name="href"><xsl:value-of select="@refuri" />
      </xsl:attribute>
      <xsl:apply-templates/>
    </a>
  </xsl:template>

</xsl:transform>
