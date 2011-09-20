<xsl:stylesheet
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:d="http://docbook.org/ns/docbook"
  exclude-result-prefixes="d"
  version='1.0'
>

  <!-- the base stylesheet (resolved by this name
       in the xml catalog file) -->
  <xsl:import href="xhtml-1_1/docbook.xsl"/>

  <!-- common param inputs, etc. -->
  <xsl:import href="brlcad-common.xsl"/>

  <!-- these are used in place of stringparam inputs to xsltproc -->
  <xsl:param name="use.id.as.filename">1</xsl:param>
  <xsl:param name="html.stylesheet">70_value.css</xsl:param>
  <xsl:param name="chunker.output.indent">yes</xsl:param>

<!-- ==================================================================== -->

  <!-- other customizations -->
  <!-- line breaks, DB p. 245 -->
  <xsl:template match="processing-instruction('linebreak')">
    <br />
  </xsl:template>

  <xsl:param name="default.image.width">5in</xsl:param>
  <xsl:param name="chunker.output.indent">yes</xsl:param>

  <!-- html header/footer -->
  <xsl:param name="navig.showtitles">1</xsl:param>
  <!-- from Bob Stayton's book: -->
  <xsl:param name="local.l10n.xml" select="document('')" />
  <l:i18n xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0">
    <l:l10n language="en">
      <l:gentext key="nav-home" text="Table of Contents"/>
    </l:l10n>
  </l:i18n>

  <!-- copyright, etc. in headers, Stayton, p. 153 -->
<xsl:template name="user.head.content">
  <htmlcode xmlns="http://www.w3.org/1999/xhtml">

    <h2 align="center" >
      <font color="#0000ff">VALUE</font>
      <img src="images/value-xhairs-only-32x.png" alt="VALUE logo" />
      <font color="#0000ff">User's Manual</font>
    </h2>

    <p align="center"><font color="#0000ff"><xsl:value-of select="$value.version"/></font></p>

    <p align="center">Copyright &#xA9; 2000-2011 ManTech International Corporation</p>
  </htmlcode>
</xsl:template>


<!-- ==================================================================== -->
<!-- this is a modified header nav template from html/chunk-common.xsl version 1.76.1 -->

<!--
<xsl:import href="brlcad-xhtml-header-navigation.xsl"/>
-->

</xsl:stylesheet>
