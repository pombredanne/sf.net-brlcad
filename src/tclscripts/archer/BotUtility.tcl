#                B O T U T I L I T Y . T C L
# BRL-CAD
#
# Copyright (c) 2002-2010 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Description:
#	This is an Archer class for editing a BoT primitive.
#

::itcl::class BotUtility {
    inherit Utility

    constructor {_archer _handleToplevel args} {}
    destructor {}

    # public widget variable for holding selected bot
    itk_option define -selectedbot selectedBot Selected "" {}

    public {
	# Override's for the Utility class
	common utilityMajorType $Archer::pluginMajorTypeUtility
	common utilityMinorType $Archer::pluginMinorTypeMged
	common utilityName "Bot Utility"
	common utilityVersion "1.0"
	common utilityClass BotUtility

	method editSelected {}
	method selectBot {bots}
	proc editBot {bot}
    }

    protected {
	variable mHandleToplevel 0
	variable mParentBg ""
	variable mToplevel ""

	method updateBgColor {_bg}
    }

    private {
	common instances 0
    }
}

## - constructor
::itcl::body BotUtility::constructor {_archer _handleToplevel args} {
    global env

    set mHandleToplevel $_handleToplevel
    set parent [winfo parent $itk_interior]
    set mParentBg [$parent cget -background]

    # process widget options
    eval itk_initialize $args

    # load BotEditor class for first instance
    if {$instances == 0} {
	if {[catch {
	    set script [file join [bu_brlcad_data "tclscripts"] boteditor botEditor.tcl]
	    source $script
	} errMsg] > 0} {
	    puts "Couldn't load \"botEditor.tcl\"\n$errMsg"
	}
    }

    # search for bots
    set bots {}
    catch {set bots [$_archer search -type bot]}
    set numBots [llength $bots]

    if {$numBots == 0} {

	# no bots - show error message
	itk_component add btError {

	    label $itk_interior.errLbl \
		-text {There are no bots to edit.}
	}
	
	grid $itk_component(btError)

    } elseif {$numBots == 1} {	

	# edit only bot
	editBot [lindex $bots 0]

    } else {

	# select from available bots
	selectBot $bots
    }

    # manage plugin window
    set mToplevel [winfo toplevel $itk_interior]

    if {$mHandleToplevel} {
	wm geometry $mToplevel "400x200"
    }
}

::itcl::body BotUtility::destructor {} {
}

::itcl::body BotUtility::updateBgColor {_bg} {
    # TODO: update background color of all widgets
}

# selectBot bots
#     Lets user pick what bot they want to edit by displaying a combobox
#     populated with the elements of the bots list argument.
#
::itcl::body BotUtility::selectBot {bots} {    

    # create combobox of available bots
    set combo [ttk::combobox $itk_interior.selectCombo -values $bots]

    # auto-select first bot
    # - initialize selectedbot variable to first bot
    # - make combobox show firstbot
    namespace eval :: "$this configure -selectedbot [lindex $bots 0]"    
    $combo current 0

    # will update selectedbot variable whenever user changes combobox selection
    # 0 return value prevents user from editing the combobox entry
    $combo configure \
        -validate all \
	-validatecommand "$this configure -selectedbot %s; return 0"

    # create button that starts editing for the selected bot
    set button [ttk::button $itk_interior.selectButton \
        -text {Edit Selected} \
	-command "$this editSelected"]

    # display select widgets
    grid $combo -row 0 -column 0 -sticky new -pady 5
    grid $button -row 0 -column 1 -padx 5
    grid rowconfigure $itk_interior 0 -pad 5
    grid columnconfigure $itk_interior 0 -weight 1 -pad 5
}

# editBot bot
#     Edit the bot named by the bot argument.
#
::itcl::body BotUtility::editBot {bot} {

    # create new editor instance
    set editor [BotEditor .editor$instances]
    incr instances

    $editor load $bot
}

# editSelected
#     Calls editBot with current selection.
#
#     Used as the selectBot button callback, since scoping problems make it
#     hard to query the current selection in a more direct fashion.
#
::itcl::body BotUtility::editSelected {} {
    BotUtility::editBot $itk_option(-selectedbot)

    # close the selection window
    set mToplevel [winfo toplevel $itk_interior]
    namespace eval :: "$mToplevel deactivate; ::itcl::delete object $mToplevel"
}
	
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
