<?xml version="1.0" encoding="UTF-8"?>

<!--
<?xml version="1.0" encoding="utf-8" ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
-->

<xsl:transform version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

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

  <xsl:template match="bullet_list">
    <ul>
      <xsl:apply-templates/>
    </ul>
  </xsl:template>

  <xsl:template match="list_item">
    <li>
      <xsl:apply-templates/>
    </li>
  </xsl:template>

  <xsl:template match="reference[@refuri]">
    <a>
      <xsl:attribute name="href"><xsl:value-of select="@refuri" />
      </xsl:attribute>
      <xsl:apply-templates/>
    </a>
  </xsl:template>

  <xsl:template match="reference[@refid]">
    <a>
      <xsl:attribute name="href"><xsl:text>#</xsl:text><xsl:value-of select="@refid" />
      </xsl:attribute>
      <xsl:apply-templates/>
    </a>
  </xsl:template>

  <xsl:template match="footnote_reference">
    [<xsl:apply-templates/>]
  </xsl:template>

  <xsl:template match="footnote">
      <p>
	<xsl:apply-templates/>
      </p>
  </xsl:template>

  <xsl:template match="footnote/label">
    [<xsl:apply-templates/>]
  </xsl:template>

  <xsl:template match="footnote/paragraph">
    <xsl:apply-templates/>
  </xsl:template>

</xsl:transform>
