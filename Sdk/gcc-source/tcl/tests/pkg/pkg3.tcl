# pkg3.tcl --
#
#  Test package for pkg_mkIndex.
#
# Copyright (c) 1998 by Scriptics Corporation.
# All rights reserved.
# 
# RCS: @(#) $Id: pkg3.tcl,v 1.3 1999/08/25 18:28:00 spolk Exp $

package provide pkg3 1.0

namespace eval pkg3 {
    namespace export p3-1 p3-2
}

proc pkg3::p3-1 { num } {
    return {[expr $num * 2]}
}

proc pkg3::p3-2 { num } {
    return {[expr $num * 3]}
}
