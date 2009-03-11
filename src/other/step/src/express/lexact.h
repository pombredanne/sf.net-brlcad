#ifndef LEX_ACTIONS_H
#define LEX_ACTIONS_H

/* $Id: lexact.h,v 1.5 1995/04/05 13:55:40 clark dec96 $ */

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: lexact.h,v $
 * Revision 1.5  1995/04/05 13:55:40  clark
 * CADDETC preval
 *
 * Revision 1.4  1994/11/22  18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.3  1994/05/11  19:51:05  libes
 * numerous fixes
 *
 * Revision 1.2  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.5  1992/08/27  23:41:58  libes
 * modified decl of SCANinitialize
 *
 * Revision 1.4  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 */

#define keep_nul

/*************/
/* constants */
/*************/

/*****************/
/* packages used */
/*****************/

#include <ctype.h>
#include "basic.h"
#include "error.h"

/************/
/* typedefs */
/************/

#define SCAN_BUFFER_SIZE	1024
#define SCAN_NESTING_DEPTH	6
#define SCAN_ESCAPE		'\001'

typedef struct Scan_Buffer {
	char	text[SCAN_BUFFER_SIZE + 1];
#ifdef keep_nul
	int	numRead;
#endif
	char*	savedPos;
	FILE*	file;
	char	*filename;
	Boolean	readEof;
	int	lineno;
	int	bol;
} Scan_Buffer;

/********************/
/* global variables */
/********************/

#ifdef LEX_ACTIONS_C
# define GLOBAL
# define INITIALLY(value) = value
#else
# define GLOBAL extern
# define INITIALLY(value)
#endif /* LEX_ACTIONS_C */

GLOBAL Scan_Buffer	SCAN_buffers[SCAN_NESTING_DEPTH];
GLOBAL int		SCAN_current_buffer	INITIALLY(0);
GLOBAL char*		SCANcurrent;

GLOBAL Error		ERROR_include_file		INITIALLY(ERROR_none);
GLOBAL Error		ERROR_unmatched_close_comment	INITIALLY(ERROR_none);
GLOBAL Error		ERROR_unmatched_open_comment	INITIALLY(ERROR_none);
GLOBAL Error		ERROR_unterminated_string	INITIALLY(ERROR_none);
GLOBAL Error		ERROR_encoded_string_bad_digit	INITIALLY(ERROR_none);
GLOBAL Error		ERROR_encoded_string_bad_count	INITIALLY(ERROR_none);
GLOBAL Error		ERROR_bad_identifier		INITIALLY(ERROR_none);
GLOBAL Error		ERROR_unexpected_character	INITIALLY(ERROR_none);
GLOBAL Error		ERROR_nonascii_char;


#undef GLOBAL
#undef INITIALLY

/******************************/
/* macro function definitions */
/******************************/

#define SCANbuffer	SCAN_buffers[SCAN_current_buffer]
#define SCANbol		SCANbuffer.bol

#ifdef keep_nul
# define SCANtext_ready	(SCANbuffer.numRead != 0)
#else
# define SCANtext_ready	(*SCANcurrent != '\0')
#endif

/***********************/
/* function prototypes */
/***********************/

extern int	yylex PROTO((void));	/* the scanner */

extern void	SCANinitialize PROTO((void));
extern int	SCANprocess_real_literal PROTO((void));
extern int	SCANprocess_integer_literal PROTO((void));
extern int	SCANprocess_binary_literal PROTO((void));
extern int	SCANprocess_logical_literal PROTO((char *));
extern int	SCANprocess_identifier_or_keyword PROTO((void));
extern int	SCANprocess_string PROTO((void));
extern int	SCANprocess_encoded_string PROTO((void));
extern int	SCANprocess_semicolon PROTO((int));
extern void	SCANsave_comment PROTO((void));
extern Boolean	SCANread PROTO((void));
#if macros_bit_the_dust
extern void	SCANdefine_macro PROTO((char *, char *));
#endif
extern void	SCANinclude_file PROTO((char *));
void		SCANlowerize PROTO((char *));
void		SCANupperize PROTO((char *));
extern char *	SCANstrdup PROTO((char *));
extern long	SCANtell PROTO((void));

/*******************************/
/* inline function definitions */
/*******************************/

#if supports_inline_functions || defined(LEX_ACTIONS_C)

static_inline
int
SCANnextchar(char* buffer)
{
    extern Boolean SCANread(void);
#ifdef keep_nul
    static int escaped = 0;
#endif

    if (SCANtext_ready || SCANread()) {
#ifdef keep_nul
	if (!*SCANcurrent) {
	    buffer[0] = SCAN_ESCAPE;
	    *SCANcurrent = '0';
	    return 1;
	} else if ((*SCANcurrent == SCAN_ESCAPE) && !escaped) {
	    escaped = 1;
	    buffer[0] = SCAN_ESCAPE;
	    return 1;
	}
	SCANbuffer.numRead--;
#endif
	buffer[0] = *(SCANcurrent++);
	if (!isascii(buffer[0])) {
	    ERRORreport_with_line(ERROR_nonascii_char,expyylineno,
				  0xff & buffer[0]);
	    buffer[0] = ' ';	/* substitute space */
	}
	return 1;
    } else
	return 0;
}

#endif /* supports_inline_functions || defined(LEX_ACTIONS_C) */

#endif /* LEX_ACTIONS_H */
