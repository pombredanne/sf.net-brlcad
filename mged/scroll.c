/*
 *			S C R O L L . C
 *
 * Functions -
 *	scroll_display		Add a list of items to the display list
 *	scroll_select		Called by usepen() for pointing
 *
 * Authors -
 *	Bill Mermagen Jr.
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <stdlib.h>

#include "tcl.h"

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./scroll.h"
#include "./dm.h"

#include "./mgedtcl.h"

/************************************************************************
 *									*
 *	First part:  scroll bar definitions				*
 *									*
 ************************************************************************/

static void sl_tol();
static void	sl_itol();
static double	sld_xadc, sld_yadc, sld_1adc, sld_2adc, sld_distadc;

struct scroll_item scr_menu[] = {
	{ "xslew",	sl_tol,		0,	"knob X" },
	{ "yslew",	sl_tol,		1,	"knob Y" },
	{ "zslew",	sl_tol,		2,	"knob Z" },
	{ "zoom",	sl_tol,		3,	"knob S" },
	{ "xrot",	sl_tol,		4,   "knob x" },
	{ "yrot",	sl_tol,		5,   "knob y" },
	{ "zrot",	sl_tol,		6,   "knob z" },
	{ "",		(void (*)())NULL, 0, "" }
      };

struct scroll_item sl_abs_menu[] = {
	{ "Xslew",	sl_tol,		0,	"knob aX" },
	{ "Yslew",	sl_tol,		1,	"knob aY" },
	{ "Zslew",	sl_tol,		2,	"knob aZ" },
	{ "Zoom",	sl_tol,		3,	"knob aS" },
	{ "Xrot",	sl_tol,		4,   "knob ax" },
	{ "Yrot",	sl_tol,		5,   "knob ay" },
	{ "Zrot",	sl_tol,		6,   "knob az" },
	{ "",		(void (*)())NULL, 0, "" }
};

struct scroll_item sl_adc_menu[] = {
	{ "xadc",	sl_itol,	0, "knob xadc" },
	{ "yadc",	sl_itol,	1, "knob yadc" },
	{ "ang 1",	sl_itol,	2, "knob ang1" },
	{ "ang 2",	sl_itol,	3, "knob ang2" },
	{ "tick",	sl_itol,	4, "knob distadc" },
	{ "",		(void (*)())NULL, 0, "" }
};

/************************************************************************
 *									*
 *	Second part: Event Handlers called from menu items by buttons.c *
 *									*
 ************************************************************************/


/*
 *			S L _ H A L T _ S C R O L L
 *
 *  Reset all scroll bars to the zero position.
 */
void sl_halt_scroll()
{
	/* The 'knob' command will zero the rate_slew[] array, etc. */
	bu_vls_printf(&dm_values.dv_string, "knob zero\n");
}

/*
 *			S L _ T O G G L E _ S C R O L L
 */
void sl_toggle_scroll()
{
  struct bu_vls cmd;

  bu_vls_init(&cmd);

  if( mged_variables.scroll_enabled == 0 )
    bu_vls_strcpy( &cmd, "sliders on\n" );
  else
    bu_vls_strcpy( &cmd, "sliders off\n" );

  (void)cmdline(&cmd, False);

  bu_vls_free(&cmd);
}

/*
 *                      C M D _ S L I D E R S
 *
 * If no arguments are given, returns 1 if the sliders are displayed, and 0
 * if not.  If one argument is given, parses it as a boolean value and
 * turns on the sliders if it is 1, and turns them off it is 0.
 * Otherwise, returns an error;
 */

int
cmd_sliders(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
      return TCL_ERROR;

    if (argc == 1) {
	Tcl_AppendResult(interp, mged_variables.scroll_enabled ? "1" : "0", (char *)NULL);
	return TCL_OK;
    }

    if (Tcl_GetBoolean(interp, argv[1], &mged_variables.scroll_enabled) == TCL_ERROR)
	return TCL_ERROR;

    if (mged_variables.scroll_enabled) {
      if(mged_variables.rateknobs)
	scroll_array[0] = scr_menu;
      else
	scroll_array[0] = sl_abs_menu;
    } else {
	scroll_array[0] = SCROLL_NULL;	
	scroll_array[1] = SCROLL_NULL;	
    }

    if(mged_variables.show_menu)
      dmaflag++;

    return TCL_OK;
}

/************************************************************************
 *									*
 *	Third part:  event handlers called from tables, above		*
 *									*
 *  Where the floating point value pointed to by scroll_val		*
 *  in the range -1.0 to +1.0 is the only desired result,		*
 *  everything can bel handled by sl_tol().				*
 *									*
 ************************************************************************/

static void sl_tol( mptr, val )
register struct scroll_item     *mptr;
double				val;
{
	struct bu_vls cmd;

	if( val < -SL_TOL )   {
		val += SL_TOL;
	} else if( val > SL_TOL )   {
		val -= SL_TOL;
	} else {
		val = 0.0;
	}

	bu_vls_init( &cmd );
	bu_vls_printf( &cmd, "%s %f\n", mptr->scroll_cmd, val );
	(void)cmdline( &cmd, FALSE );
	bu_vls_free(&cmd);
}

static void sl_itol( mptr, val )
register struct scroll_item     *mptr;
double				val;
{
	struct bu_vls cmd;

	if( val < -SL_TOL )   {
		val += SL_TOL;
	} else if( val > SL_TOL )   {
		val -= SL_TOL;
	} else {
		val = 0.0;
	}

	bu_vls_init( &cmd );
	bu_vls_printf( &cmd, "%s %d\n", mptr->scroll_cmd, (int)(val * 2047.0) );
	(void)cmdline( &cmd, FALSE );
	bu_vls_free(&cmd);
}


/************************************************************************
 *									*
 *	Fourth part:  general-purpose interface mechanism		*
 *									*
 ************************************************************************/

/*
 *			S C R O L L _ D I S P L A Y
 *
 *  The parameter is the Y pixel address of the starting
 *  screen Y to be used, and the return value is the last screen Y
 *  position used.
 */
int
scroll_display( y_top )
int y_top;
{ 
	register int		y;
	struct scroll_item	*mptr;
	struct scroll_item	**m;
	int		xpos;
	int second_menu = -1;
	fastf_t f;

	/* XXX this should be driven by the button event */
	if( mged_variables.adcflag && mged_variables.scroll_enabled )
		scroll_array[1] = sl_adc_menu;
	else
		scroll_array[1] = SCROLL_NULL;

	scroll_top = y_top;
	y = y_top;

	for( m = &scroll_array[0]; *m != SCROLL_NULL; m++ )  {
	  ++second_menu;
	  for( mptr = *m; mptr->scroll_string[0] != '\0'; mptr++ )  {
	    y += SCROLL_DY;		/* y is now bottom line pos */

	    switch(mptr->scroll_val){
	    case 0:
	      if(second_menu)
                f = (double)dv_xadc / 2047.0;
	      else {
		if(mged_variables.rateknobs)
		  f = rate_slew[X];
		else
		  f = absolute_slew[X];
	      }
	      break;
	    case 1:
	      if(second_menu)
		f = (double)dv_yadc / 2047.0;
	      else {
		if(mged_variables.rateknobs)
		  f = rate_slew[Y];
		else
		  f = absolute_slew[Y];
	      }
	      break;
	    case 2:
	      if(second_menu)
		f = (double)dv_1adc / 2047.0;
	      else {
		if(mged_variables.rateknobs)
		  f = rate_slew[Z];
		else
		  f = absolute_slew[Z];
	      }
	      break;
	    case 3:
	      if(second_menu)
		f = (double)dv_2adc / 2047.0;
	      else {
		if(mged_variables.rateknobs)
		  f = rate_zoom;
		else
		  f = absolute_zoom;
	      }
	      break;
	    case 4:
	      if(second_menu)
		f = (double)dv_distadc / 2047.0;
	      else {
		if(mged_variables.rateknobs)
		  f = rate_rotate[X];
		else
		  f = absolute_rotate[X];
	      }
	      break;
	    case 5:
	      if(second_menu)
		Tcl_AppendResult(interp, "scroll_display: 2nd scroll menu is hosed\n",
				 (char *)NULL);
	      else {
		if(mged_variables.rateknobs)
		  f = rate_rotate[Y];
		else
		  f = absolute_rotate[Y];
	      }
	      break;
	    case 6:
	      if(second_menu)
		Tcl_AppendResult(interp, "scroll_display: 2nd scroll menu is hosed\n",
				 (char *)NULL);
	      else {
		if(mged_variables.rateknobs)
		  f = rate_rotate[Z];
		else
		  f = absolute_rotate[Z];
	      }
	      break;
	    default:
	      if(second_menu)
		Tcl_AppendResult(interp, "scroll_display: 2nd scroll menu is hosed\n",
				 (char *)NULL);
	      else
		Tcl_AppendResult(interp, "scroll_display: first scroll menu is hosed\n",
				 (char *)NULL);
	    }

	    if(f > 0)
	      xpos = (f + SL_TOL) * 2047.0;
	    else if(f < 0)
	      xpos = (f - SL_TOL) * -MENUXLIM;
	    else
	      xpos = 0;

	    dmp->dmr_puts( mptr->scroll_string,
			   xpos, y-SCROLL_DY/2, 0, DM_RED );
	    dmp->dmr_2d_line(XMAX, y, MENUXLIM, y, 0);
	  }
	}


	if( y != y_top )  {
		/* Sliders were drawn, so make left vert edge */
		dmp->dmr_2d_line( MENUXLIM, scroll_top-1,
			MENUXLIM, y, 0 );
	}
	return( y );
}

/*
 *			S C R O L L _ S E L E C T
 *
 *  Called with Y coordinate of pen in menu area.
 *
 * Returns:	1 if menu claims these pen co-ordinates,
 *		0 if pen is BELOW scroll
 *		-1 if pen is ABOVE scroll	(error)
 */
int
scroll_select( pen_x, pen_y )
int		pen_x;
register int	pen_y;
{ 
	register int		yy;
	struct scroll_item	**m;
	register struct scroll_item     *mptr;

	if( !mged_variables.scroll_enabled )  return(0);	/* not enabled */

	if( pen_y > scroll_top )
		return(-1);	/* pen above menu area */

	/*
	 * Start at the top of the list and see if the pen is
	 * above here.
	 */
	yy = scroll_top;
	for( m = &scroll_array[0]; *m != SCROLL_NULL; m++ )  {
		for( mptr = *m; mptr->scroll_string[0] != '\0'; mptr++ )  {
			fastf_t	val;
			yy += SCROLL_DY;	/* bottom line pos */
			if( pen_y < yy )
				continue;	/* pen below this item */

			/*
			 *  Record the location of scroll marker.
			 *  Note that the left side has less width than
			 *  the right side, due to the presence of the
			 *  menu text area on the left.
			 */
			if( pen_x >= 0 )  {
				val = pen_x/2047.0;
			} else {
				val = pen_x/(double)(-MENUXLIM);
			}

			/* See if hooked function has been specified */
			if( mptr->scroll_func == ((void (*)())0) )  continue;

			(*(mptr->scroll_func))(mptr, val);
#if 0
			dmaflag = 1;
#endif
			return( 1 );		/* scroll claims pen value */
		}
	}
	return( 0 );		/* pen below scroll area */
}
