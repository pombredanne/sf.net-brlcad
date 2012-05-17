
/*
* NIST STEP Core Class Library
* clstepcore/sdaiString.cc
* April 1997
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <sdai.h>
#include <sstream>

SDAI_String & SDAI_String::operator= ( const char * s ) {
    std::string::operator= ( s );
    return *this;
}

void SDAI_String::STEPwrite( ostream & out ) const {
    out << c_str();
}

void SDAI_String::STEPwrite( std::string & s ) const {
    s += c_str();
}

Severity SDAI_String::StrToVal( const char * s ) {
    operator= ( s );
    if( ! strcmp( c_str(),  s ) ) {
        return SEVERITY_NULL ;
    } else {
        return SEVERITY_INPUT_ERROR;
    }
}

/**
 *  STEPread reads a string in exchange file format
 *  starting with a single quote
 */
Severity SDAI_String::STEPread( istream & in, ErrorDescriptor * err ) {
    clear();  // clear the old string
    // remember the current format state to restore the previous settings
    ios_base::fmtflags flags = in.flags();
    in.unsetf( ios::skipws );

    // extract the string from the inputstream
    std::string s = GetLiteralStr( in, err );
    append( s );

    // retrieve current severity
    Severity sev = err -> severity();

    if ( s.empty() ) {
        // no string was read
        in.flags( flags ); // set the format state back to previous settings
        err -> GreaterSeverity( SEVERITY_INCOMPLETE );
        sev = SEVERITY_INCOMPLETE;
    } else if ( sev != SEVERITY_INPUT_ERROR ) {
        // read valid string
        sev = SEVERITY_NULL;
    }
    return sev;
}

Severity SDAI_String::STEPread( const char * s, ErrorDescriptor * err ) {
    istringstream in( ( char * )s );
    return STEPread( in, err );
}
