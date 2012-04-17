#ifndef  SDAICLASSES_H
#define  SDAICLASSES_H
// This file was generated by fedex_plus.  You probably don't want to edit
// it since your modifications will be lost if fedex_plus is used to
// regenerate it.
/* $Id$  */

// Schema:  SdaiHEADER_SECTION_SCHEMA
extern Schema  * s_header_section_schema;

// Types:
typedef SDAI_String   SdaiTime_stamp_text;
extern TypeDescriptor  * header_section_schemat_time_stamp_text;
typedef SDAI_String   SdaiSection_name;
extern TypeDescriptor  * header_section_schemat_section_name;
typedef SDAI_String   SdaiContext_name;
extern TypeDescriptor  * header_section_schemat_context_name;
typedef SDAI_String   SdaiSchema_name;
extern TypeDescriptor  * header_section_schemat_schema_name;
typedef SDAI_String   SdaiLanguage_name;
extern TypeDescriptor  * header_section_schemat_language_name;
typedef SDAI_String   SdaiExchange_structure_identifier;
extern TypeDescriptor  * header_section_schemat_exchange_structure_identifier;

// Entities:
class SdaiSection_language;
typedef SdaiSection_language    *   SdaiSection_languageH;
typedef SdaiSection_language    *   SdaiSection_language_ptr;
typedef SdaiSection_language_ptr    SdaiSection_language_var;
typedef const SdaiSection_language   *  const_SdaiSection_languageH;
typedef const SdaiSection_language   *  const_SdaiSection_language_ptr;
#define SdaiSection_language__set   SDAI_DAObject__set
#define SdaiSection_language__set_var   SDAI_DAObject__set_var
extern EntityDescriptor   *  header_section_schemae_section_language;

class SdaiFile_population;
typedef SdaiFile_population  *  SdaiFile_populationH;
typedef SdaiFile_population  *  SdaiFile_population_ptr;
typedef SdaiFile_population_ptr SdaiFile_population_var;
typedef const SdaiFile_population   *   const_SdaiFile_populationH;
typedef const SdaiFile_population   *   const_SdaiFile_population_ptr;
#define SdaiFile_population__set    SDAI_DAObject__set
#define SdaiFile_population__set_var    SDAI_DAObject__set_var
extern EntityDescriptor   *  header_section_schemae_file_population;

class SdaiFile_name;
typedef SdaiFile_name   *   SdaiFile_nameH;
typedef SdaiFile_name   *   SdaiFile_name_ptr;
typedef SdaiFile_name_ptr   SdaiFile_name_var;
typedef const SdaiFile_name  *  const_SdaiFile_nameH;
typedef const SdaiFile_name  *  const_SdaiFile_name_ptr;
#define SdaiFile_name__set  SDAI_DAObject__set
#define SdaiFile_name__set_var  SDAI_DAObject__set_var
extern EntityDescriptor   *  header_section_schemae_file_name;

class SdaiSection_context;
typedef SdaiSection_context  *  SdaiSection_contextH;
typedef SdaiSection_context  *  SdaiSection_context_ptr;
typedef SdaiSection_context_ptr SdaiSection_context_var;
typedef const SdaiSection_context   *   const_SdaiSection_contextH;
typedef const SdaiSection_context   *   const_SdaiSection_context_ptr;
#define SdaiSection_context__set    SDAI_DAObject__set
#define SdaiSection_context__set_var    SDAI_DAObject__set_var
extern EntityDescriptor   *  header_section_schemae_section_context;

class SdaiFile_description;
typedef SdaiFile_description    *   SdaiFile_descriptionH;
typedef SdaiFile_description    *   SdaiFile_description_ptr;
typedef SdaiFile_description_ptr    SdaiFile_description_var;
typedef const SdaiFile_description   *  const_SdaiFile_descriptionH;
typedef const SdaiFile_description   *  const_SdaiFile_description_ptr;
#define SdaiFile_description__set   SDAI_DAObject__set
#define SdaiFile_description__set_var   SDAI_DAObject__set_var
extern EntityDescriptor   *  header_section_schemae_file_description;

class SdaiFile_schema;
typedef SdaiFile_schema  *  SdaiFile_schemaH;
typedef SdaiFile_schema  *  SdaiFile_schema_ptr;
typedef SdaiFile_schema_ptr SdaiFile_schema_var;
typedef const SdaiFile_schema   *   const_SdaiFile_schemaH;
typedef const SdaiFile_schema   *   const_SdaiFile_schema_ptr;
#define SdaiFile_schema__set    SDAI_DAObject__set
#define SdaiFile_schema__set_var    SDAI_DAObject__set_var
extern EntityDescriptor   *  header_section_schemae_file_schema;

#endif
