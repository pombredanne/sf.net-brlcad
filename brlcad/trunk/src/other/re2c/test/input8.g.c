/* Generated by re2c */
#line 1 "input8.g.re"

#line 5 "<stdout>"
{
	YYCTYPE yych;
	unsigned int yyaccept = 0;

	if ((YYLIMIT - YYCURSOR) < 4) YYFILL(4);
	yych = *YYCURSOR;
	if (yych == '\n') goto yy4;
	yyaccept = 0;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= 0x00) goto yy6;
	if (yych != '\n') goto yy8;
yy3:
#line 6 "input8.g.re"
	{ return 1; }
#line 20 "<stdout>"
yy4:
	++YYCURSOR;
#line 7 "input8.g.re"
	{ return 2; }
#line 25 "<stdout>"
yy6:
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych <= 0x00) goto yy10;
	if (yych != '\n') goto yy11;
yy7:
#line 5 "input8.g.re"
	{ return 0; }
#line 34 "<stdout>"
yy8:
	yych = *++YYCURSOR;
	if (yych <= 0x00) goto yy10;
	if (yych != '\n') goto yy11;
yy9:
	YYCURSOR = YYMARKER;
	if (yyaccept <= 0) {
		goto yy3;
	} else {
		goto yy7;
	}
yy10:
	yych = *++YYCURSOR;
	if (yych <= 0x00) goto yy12;
	goto yy7;
yy11:
	yych = *++YYCURSOR;
	if (yych >= 0x01) goto yy9;
yy12:
	++YYCURSOR;
	yych = *YYCURSOR;
	goto yy7;
}
#line 9 "input8.g.re"

