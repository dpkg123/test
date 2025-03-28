# vscale.tcl --
#
# This demonstration script shows an example with a vertical scale.
#
# RCS: @(#) $Id: vscale.tcl,v 1.7.6.1 2000/05/04 21:26:26 spolk Exp $

if {![info exists widgetDemo]} {
    error "This script should be run from the \"widget\" demo."
}

set w .vscale
catch {destroy $w}
toplevel $w
wm title $w "Vertical Scale Demonstration"
wm iconname $w "vscale"
positionWindow $w

label $w.msg -font $font -wraplength 3.5i -justify left -text "An arrow and a vertical scale are displayed below.  If you click or drag mouse button 1 in the scale, you can change the size of the arrow."
pack $w.msg -side top -padx .5c

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

frame $w.frame -borderwidth 10
pack $w.frame

scale $w.frame.scale -orient vertical -length 284 -from 0 -to 250 \
	-command "setHeight $w.frame.canvas" -tickinterval 50
canvas $w.frame.canvas -width 50 -height 50 -bd 0 -highlightthickness 0
$w.frame.canvas create polygon 0 0 1 1 2 2 -fill SeaGreen3 -tags poly
$w.frame.canvas create line 0 0 1 1 2 2 0 0 -fill black -tags line
frame $w.frame.right -borderwidth 15
pack $w.frame.scale -side left -anchor ne
pack $w.frame.canvas -side left -anchor nw -fill y
$w.frame.scale set 75

proc setHeight {w height} {
    incr height 21
    set y2 [expr $height - 30]
    if {$y2 < 21} {
	set y2 21
    }
    $w coords poly 15 20 35 20 35 $y2 45 $y2 25 $height 5 $y2 15 $y2 15 20
    $w coords line 15 20 35 20 35 $y2 45 $y2 25 $height 5 $y2 15 $y2 15 20
}

