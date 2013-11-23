// see db5_attrs.cpp (and raytrace.h)  for registered attribute info

#include "common.h"

#include "raytrace.h"
#include "db5_attrs_private.h"

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <sys/stat.h>

using namespace std;

using namespace db5_attrs_private;

// local funcs
//static void gen_attr_xml_tables(const std::string& fname, const std::string& fname2);
static void gen_attr_html_page(const std::string& fname);
static void open_file_write(ofstream& fs, const string& f);

// local vars
static bool debug(true);

int
main()
{
    load_maps();

    // write the html file
    string ofil("t.html");
    gen_attr_html_page(ofil);

    // write the xml table for file 'attributes.xml'
    //string adir("../../doc/docbook/system/man5/en/");

    // these two files are included in the manually generated
    // 'attributes.xml' file:
    //string stable(adir + "attributes-standard-table.xml");
    //string utable(adir + "attributes-user-table.xml");

    //gen_attr_xml_tables(stable, utable);

    return 0;
}

void
open_file_write(ofstream& fs, const string& f)
{
  fs.open(f.c_str());
  if (fs.bad()) {
    fs.close();
    printf("File '%s' open error.\n", f.c_str());
    exit(1);
  }
} // open_file_write

/*
void
gen_attr_xml_tables(const std::string& fname, const std::string& fname2)
{
    ofstream fo, fo2;
    open_file_write(fo, fname);
    open_file_write(fo2, fname2);

    // the table files will be included in a parent DocBook xml file
    // for man pages and will be child elements of a DB <para>

    // the standard (core) attributes
    fo <<
        "<article xmlns='http://docbook.org/ns/docbook' version='5.0'\n"
        "  xmlns:xi='http://www.w3.org/2001/XInclude'\n"
        ">\n"


        "  <table>\n"
        "    <tr>\n"
        "      <th>Attribute</th>\n"
        "      <th>Binary?</th>\n"
        "      <th>Definition</th>\n"
        "      <th>Example</th>\n"
        "      <th>Aliases</th>\n"
        "    </tr>\n"
        ;

    // track ATTR_REGISTERED type for separate listing
    map<string,db5_attr_t> rattrs;
    for (map<string,db5_attr_t>::iterator i = name2attr.begin(); i != name2attr.end(); ++i) {
        const string& name(i->first);
        db5_attr_t& a(i->second);
        if (a.attr_subtype == ATTR_REGISTERED) {
            rattrs.insert(make_pair(name,a));
            continue;
        }
        fo <<
            "    <tr>\n"
            "      <td>" << name                       << "</td>\n"
            "      <td>" << (a.is_binary ? "yes" : "") << "</td>\n"
            "      <td>" << a.description              << "</td>\n"
            "      <td>" << a.examples                 << "</td>\n"
            "      <td>"
            ;
        if (!a.aliases.empty()) {
            for (set<string>::iterator j = a.aliases.begin();
                 j != a.aliases.end(); ++j) {
                if (j != a.aliases.begin())
                    fo << ", ";
                fo << *j;
            }
        }
        fo <<
            "</td>\n"
            "    </tr>\n"
            ;
    }
    fo << "  </table>\n";

    // now show ATTR_REGISTERED types, if any
    fo << "  <h3>User-Registered Attributes</h3>\n";
    if (rattrs.empty()) {
        fo << "    <p>None at this time.</p>\n";
    } else {
        // need a table here
        fo <<
            "  <table>\n"
            "    <tr>\n"
            "      <th>Attribute</th>\n"
            "      <th>Binary?</th>\n"
            "      <th>Definition</th>\n"
            "      <th>Example</th>\n"
            "      <th>Aliases</th>\n"
            "    </tr>\n"
            ;
        for (map<string,db5_attr_t>::iterator i = rattrs.begin(); i != rattrs.end(); ++i) {
            const string& name(i->first);
            db5_attr_t& a(i->second);
            fo <<
                "    <tr>\n"
                "      <td>" << name                       << "</td>\n"
                "      <td>" << (a.is_binary ? "yes" : "") << "</td>\n"
                "      <td>" << a.description              << "</td>\n"
                "      <td>" << a.examples                 << "</td>\n"
                "      <td>"
                ;
            if (!a.aliases.empty()) {
                for (set<string>::iterator j = a.aliases.begin();
                     j != a.aliases.end(); ++i) {
                    if (j != a.aliases.begin())
                        fo << ", ";
                    fo << *j;
                }
            }
            fo <<
                "</td>\n"
                "    </tr>\n"
                ;
        }
        fo << "  </table>\n";
    }

    fo <<
        "</body>\n"
        "</html>\n"
        ;

    fo.close();

    if (debug)
        printf("DEBUG:  see output file '%s'\n", fname.c_str());

} // gen_attr_xml_table
*/

void
gen_attr_html_page(const std::string& fname)
{
    ofstream fo;
    open_file_write(fo, fname);

    fo <<
        "<!doctype html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <title>brlcad-attributes.html</title>\n"
        "  <meta charset = \"UTF-8\" />\n"
        "  <style type = \"text/css\">\n"
        "  table, td, th {\n"
        "    border: 1px solid black;\n"
        "  }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>BRL-CAD Standard and User-Registered Attributes</h2>\n"
        "  <p>Following are lists of the BRL-CAD standard and user-registered attribute names\n"
        "  along with their value definitions and aliases (if any).  Users should\n"
        "  not assign values to them in other than their defined format.\n"
        "  (Note that attribute names are not case-sensitive although their canonical form is\n"
        "  lower-case.)</p>\n"

        "  <p>Any code setting or reading the value of one of these attributes\n"
        "  must handle all aliases to ensure all functions asking for\n"
        "  the value in question get a consistent answer.</p>\n"

        "  <p>Some attributes have ASCII names but binary values (e.g., 'mtime').  Their values cannot\n"
        "  be modified by a user with the 'attr' command.  In some cases, but not all, their\n"
        "  values may be shown in a human readable form with the 'attr' command.)</p>\n"

        "  <p>If a user wishes to register an attribute to protect its use for models\n"
        "  transferred to other BRL-CAD users, submit the attribute, along with a description\n"
        "  of its intended use, to the\n"
        "  <a href=\"mailto:brlcad-devel@lists.sourceforge.net\">BRL-CAD developers</a>.\n"
        "  Its approval will be formal when it appears in the separate, registered-attribute\n"
        "  table following the standard attribute table.</p>\n"
        ;

    // need a table here (5 columns at the moment)
    fo <<
        "  <h3>BRL-CAD Standard (Core) Attributes</h3>\n"
        "  <table>\n"
        "    <tr>\n"
        "      <th>Property</th>\n"
        "      <th>Attribute</th>\n"
        "      <th>Binary?</th>\n"
        "      <th>Definition</th>\n"
        "      <th>Example</th>\n"
        "      <th>Aliases</th>\n"
        "    </tr>\n"
        ;

    // track ATTR_REGISTERED type for separate listing
    map<string,db5_attr_t> rattrs;
    for (map<string,db5_attr_t>::iterator i = name2attr.begin(); i != name2attr.end(); ++i) {
        const string& name(i->first);
        db5_attr_t& a(i->second);
        if (a.attr_subtype == ATTR_REGISTERED) {
            rattrs.insert(make_pair(name,a));
            continue;
        }
        fo <<
            "    <tr>\n"
            "      <td>" << a.property                 << "</td>\n"
            "      <td>" << name                       << "</td>\n"
            "      <td>" << (a.is_binary ? "yes" : "") << "</td>\n"
            "      <td>" << a.description              << "</td>\n"
            "      <td>" << a.examples                 << "</td>\n"
            "      <td>"
            ;
        if (!a.aliases.empty()) {
            for (set<string>::iterator j = a.aliases.begin();
                 j != a.aliases.end(); ++j) {
                if (j != a.aliases.begin())
                    fo << ", ";
                fo << *j;
            }
        }
        fo <<
            "</td>\n"
            "    </tr>\n"
            ;
    }
    fo << "  </table>\n";

    // now show ATTR_REGISTERED types, if any
    fo << "  <h3>User-Registered Attributes</h3>\n";
    if (rattrs.empty()) {
        fo << "    <p>None at this time.</p>\n";
    } else {
        // need a table here
        fo <<
            "  <table>\n"
            "    <tr>\n"
            "      <th>Property</th>\n"
            "      <th>Attribute</th>\n"
            "      <th>Binary?</th>\n"
            "      <th>Definition</th>\n"
            "      <th>Example</th>\n"
            "      <th>Aliases</th>\n"
            "    </tr>\n"
            ;
        for (map<string,db5_attr_t>::iterator i = rattrs.begin(); i != rattrs.end(); ++i) {
            const string& name(i->first);
            db5_attr_t& a(i->second);
            fo <<
                "    <tr>\n"
                "      <td>" << a.property                 << "</td>\n"
                "      <td>" << name                       << "</td>\n"
                "      <td>" << (a.is_binary ? "yes" : "") << "</td>\n"
                "      <td>" << a.description              << "</td>\n"
                "      <td>" << a.examples                 << "</td>\n"
                "      <td>"
                ;
            if (!a.aliases.empty()) {
                for (set<string>::iterator j = a.aliases.begin();
                     j != a.aliases.end(); ++i) {
                    if (j != a.aliases.begin())
                        fo << ", ";
                    fo << *j;
                }
            }
            fo <<
                "</td>\n"
                "    </tr>\n"
                ;
        }
        fo << "  </table>\n";
    }

    fo <<
        "</body>\n"
        "</html>\n"
        ;

    fo.close();

    if (debug)
        printf("DEBUG:  see output file '%s'\n", fname.c_str());

} // gen_attr_html_page
