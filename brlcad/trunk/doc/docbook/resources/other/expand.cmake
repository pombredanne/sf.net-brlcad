
# Define the commands to expand the third party components we will need to handle
# DocBook processing from their archives into the build directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/other/docbook-schema)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/resources/other/docbook-schema)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/dejavu-lgc)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/dejavu-lgc)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/stix)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/stix)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/other/offo)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/resources/other/offo/source)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/resources/other/offo/binary)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/resources/other/offo/README)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/svg)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/svg)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/xsl)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/xsl)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources/other/docbook-schema/schema.sentinel
	COMMAND ${CMAKE_COMMAND} -E tar xjf	${CMAKE_CURRENT_SOURCE_DIR}/resources/other/docbook-5.0.tar.bz2
	COMMAND ${CMAKE_COMMAND} -E copy	${CMAKE_CURRENT_SOURCE_DIR}/resources/other/docbookxi.nvdl ${CMAKE_CURRENT_BINARY_DIR}/resources/other/docbook-schema/docbookxi.nvdl
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/resources/other/docbook-schema/schema.sentinel
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/docbookxi.nvdl ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/docbook-5.0.tar.bz2
	WORKING_DIRECTORY	${CMAKE_CURRENT_BINARY_DIR}/resources/other
	)
ADD_CUSTOM_TARGET(schema-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/resources/other/docbook-schema/schema.sentinel)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/dejavu-lgc/dejavu-lgc.sentinel
	COMMAND ${CMAKE_COMMAND} -E tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/fonts/dejavu-lgc-fonts.tar.bz2
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/dejavu-lgc/dejavu-lgc.sentinel
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/fonts/dejavu-lgc-fonts.tar.bz2
	WORKING_DIRECTORY	${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts
	)
ADD_CUSTOM_TARGET(fonts-dejavu-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/dejavu-lgc/dejavu-lgc.sentinel)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/stix/stix.sentinel
	COMMAND ${CMAKE_COMMAND} -E tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/fonts/stix-fonts.tar.bz2
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/stix/stix.sentinel
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/fonts/stix-fonts.tar.bz2
	WORKING_DIRECTORY	${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts
	)
ADD_CUSTOM_TARGET(fonts-stix-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/resources/other/fonts/stix/stix.sentinel)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources/other/offo/offo2_0.sentinel
	COMMAND ${CMAKE_COMMAND} -E tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/offo/offo-2.0.tar.bz2
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/resources/other/offo/offo2_0.sentinel
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/offo/offo-2.0.tar.bz2
	WORKING_DIRECTORY	${CMAKE_CURRENT_BINARY_DIR}/resources/other/
	)
ADD_CUSTOM_TARGET(offo-2-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/resources/other/offo/offo2_0.sentinel)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/svg/svg.sentinel
	COMMAND ${CMAKE_COMMAND} -E tar xjf	${CMAKE_CURRENT_SOURCE_DIR}/resources/other/standard/w3_svg_dtd.tar.bz2
	COMMAND ${CMAKE_COMMAND} -E copy	${CMAKE_CURRENT_SOURCE_DIR}/resources/other/standard/met-fonts.xsl	${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/svg/met-fonts.xsl
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/svg/svg.sentinel
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/standard/met-fonts.xsl ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/standard/w3_svg_dtd.tar.bz2
	WORKING_DIRECTORY	${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard
	)
ADD_CUSTOM_TARGET(svg-dtd-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/svg/svg.sentinel)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/xsl/xsl.sentinel
	COMMAND ${CMAKE_COMMAND} -E tar xjf	${CMAKE_CURRENT_SOURCE_DIR}/resources/other/standard/docbook-xsl-ns.tar.bz2
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/xsl/xsl.sentinel
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/standard/docbook-xsl-ns.tar.bz2
	WORKING_DIRECTORY	${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard
	)
ADD_CUSTOM_TARGET(xsl-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/xsl/xsl.sentinel)

SET(DOCBOOK_RESOURCE_FILES schema-expand fonts-dejavu-expand fonts-stix-expand offo-2-expand svg-dtd-expand xsl-expand)
