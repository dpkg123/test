# menubutton.tcl --
#
# This demonstration script creates a window with a bunch of menus
# and cascaded menus using menubuttons.
#
# # RCS: @(#) $Id: menubu.tcl,v 1.7.6.1 2000/05/04 21:26:25 spolk Exp $

if {![info exists widgetDemo]} {
    error "This script should be run from the \"widget\" demo."
}

set w .menubutton
catch {destroy $w}
toplevel $w
wm title $w "Menu Button Demonstration"
wm iconname $w "menubutton"
positionWindow $w


frame $w.body
pack $w.body -expand 1 -fill both

menubutton $w.body.below -text "Below" -underline 0 -direction below -menu $w.body.below.m -relief raised
menu $w.body.below.m -tearoff 0 
$w.body.below.m add command -label "Below menu: first item" -command "puts \"You have selected the first item from the Below menu.\""
$w.body.below.m add command -label "Below menu: second item" -command "puts \"You have selected the second item from the Below menu.\""
grid $w.body.below -row 0 -column 1 -sticky n
menubutton $w.body.right -text "Right" -underline 0 -direction right -menu $w.body.right.m -relief raised
menu $w.body.right.m -tearoff 0
$w.body.right.m add command -label "Right menu: first item" -command "puts \"You have selected the first item from the Right menu.\""
$w.body.right.m add command -label "Right menu: second item" -command "puts \"You have selected the second item from the Right menu.\""
frame $w.body.center
menubutton $w.body.left -text "Left" -underline 0 -direction left -menu $w.body.left.m -relief raised
menu $w.body.left.m -tearoff 0
$w.body.left.m add command -label "Left menu: first item" -command "puts \"You have selected the first item from the Left menu.\""
$w.body.left.m add command -label "Left menu: second item" -command "puts \"You have selected the second item from the Left menu.\""
grid $w.body.right -row 1 -column 0 -sticky w
grid $w.body.center -row 1 -column 1 -sticky news
grid $w.body.left -row 1 -column 2 -sticky e
menubutton $w.body.above -text "Above" -underline 0 -direction above -menu $w.body.above.m -relief raised
menu $w.body.above.m -tearoff 0
$w.body.above.m add command -label "Above menu: first item" -command "puts \"You have selected the first item from the Above menu.\""
$w.body.above.m add command -label "Above menu: second item" -command "puts \"You have selected the second item from the Above menu.\""
grid $w.body.above -row 2 -column 1 -sticky s

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode .menubu"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

set body $w.body.center
label $body.label -wraplength 300 -font "Helvetica 14" -justify left -text "This is a demonstration of menubuttons. The \"Below\" menubutton pops its menu below the button; the \"Right\" button pops to the right, etc. There are two option menus directly below this text; one is just a standard menu and the other is a 16-color palette."
pack $body.label -side top -padx 25 -pady 25
frame $body.buttons
pack $body.buttons -padx 25 -pady 25
tk_optionMenu $body.buttons.options menubuttonoptions one two three
pack $body.buttons.options -side left -padx 25 -pady 25
set m [tk_optionMenu $body.buttons.colors paletteColor Black red4 DarkGreen NavyBlue gray75 Red Green Blue gray50 Yellow Cyan Magenta White Brown DarkSeaGreen DarkViolet]
if {$tcl_platform(platform) == "macintosh"} {
    set topBorderColor Black
    set bottomBorderColor Black
} else {
    set topBorderColor gray50
    set bottomBorderColor gray75
}
for {set i 0} {$i <= [$m index last]} {incr i} {
    set name [$m entrycget $i -label]
    image create photo image_$name -height 16 -width 16
    image_$name put $topBorderColor -to 0 0 16 1
    image_$name put $topBorderColor -to 0 1 1 16
    image_$name put $bottomBorderColor -to 0 15 16 16
    image_$name put $bottomBorderColor -to 15 1 16 16
    image_$name put $name -to 1 1 15 15

    image create photo image_${name}_s -height 16 -width 16
    image_${name}_s put Black -to 0 0 16 2
    image_${name}_s put Black -to 0 2 2 16
    image_${name}_s put Black -to 2 14 16 16
    image_${name}_s put Black -to 14 2 16 14
    image_${name}_s put $name -to 2 2 14 14

    $m entryconfigure $i -image image_$name -selectimage image_${name}_s -hidemargin 1
}
$m configure -tearoff 1
foreach i {Black gray75 gray50 White} {
	$m entryconfigure $i -columnbreak 1
}

pack $body.buttons.colors -side left -padx 25 -pady 25




