# From hankedr@dms.auburn.edu  Sun Jan 28 12:25:43 2001
# Received: from mail.actcom.co.il [192.114.47.13]
# 	by localhost with POP3 (fetchmail-5.5.0)
# 	for arnold@localhost (single-drop); Sun, 28 Jan 2001 12:25:43 +0200 (IST)
# Received: by actcom.co.il (mbox arobbins)
#  (with Cubic Circle's cucipop (v1.31 1998/05/13) Sun Jan 28 12:27:08 2001)
# X-From_: hankedr@dms.auburn.edu Sat Jan 27 15:15:57 2001
# Received: from lmail.actcom.co.il by actcom.co.il  with ESMTP
# 	(8.9.1a/actcom-0.2) id PAA23801 for <arobbins@actcom.co.il>;
# 	Sat, 27 Jan 2001 15:15:55 +0200 (EET)  
# 	(rfc931-sender: lmail.actcom.co.il [192.114.47.13])
# Received: from billohost.com (www.billohost.com [209.196.35.10])
# 	by lmail.actcom.co.il (8.9.3/8.9.1) with ESMTP id PAA15998
# 	for <arobbins@actcom.co.il>; Sat, 27 Jan 2001 15:16:27 +0200
# Received: from yak.dms.auburn.edu (yak.dms.auburn.edu [131.204.53.2])
# 	by billohost.com (8.9.3/8.9.3) with ESMTP id IAA00467
# 	for <arnold@skeeve.com>; Sat, 27 Jan 2001 08:15:52 -0500
# Received: (from hankedr@localhost)
# 	by yak.dms.auburn.edu (8.9.3/8.9.3/Debian/GNU) id HAA24441;
# 	Sat, 27 Jan 2001 07:15:44 -0600
# Date: Sat, 27 Jan 2001 07:15:44 -0600
# Message-Id: <200101271315.HAA24441@yak.dms.auburn.edu>
# From: Darrel Hankerson <hankedr@dms.auburn.edu>
# To: arnold@skeeve.com
# Subject: [stolfi@ic.unicamp.br: Bug in [...]* matching with acute-u]
# Mime-Version: 1.0 (generated by tm-edit 7.106)
# Content-Type: message/rfc822
# Status: R
# 
# From: Jorge Stolfi <stolfi@ic.unicamp.br>
# To: bug-gnu-utils@gnu.org
# Subject: Bug in [...]* matching with acute-u
# MIME-Version: 1.0
# Reply-To: stolfi@ic.unicamp.br
# X-MIME-Autoconverted: from 8bit to quoted-printable by grande.dcc.unicamp.br id GAA10716
# Sender: bug-gnu-utils-admin@gnu.org
# Errors-To: bug-gnu-utils-admin@gnu.org
# X-BeenThere: bug-gnu-utils@gnu.org
# X-Mailman-Version: 2.0
# Precedence: bulk
# List-Help: <mailto:bug-gnu-utils-request@gnu.org?subject=help>
# List-Post: <mailto:bug-gnu-utils@gnu.org>
# List-Subscribe: <http://mail.gnu.org/mailman/listinfo/bug-gnu-utils>,
# 	<mailto:bug-gnu-utils-request@gnu.org?subject=subscribe>
# List-Id: Bug reports for the GNU utilities <bug-gnu-utils.gnu.org>
# List-Unsubscribe: <http://mail.gnu.org/mailman/listinfo/bug-gnu-utils>,
# 	<mailto:bug-gnu-utils-request@gnu.org?subject=unsubscribe>
# List-Archive: <http://mail.gnu.org/pipermail/bug-gnu-utils/>
# Date: Sat, 27 Jan 2001 06:46:11 -0200 (EDT)
# Content-Transfer-Encoding: 8bit
# X-MIME-Autoconverted: from quoted-printable to 8bit by manatee.dms.auburn.edu id CAA14936
# Content-Type: text/plain; charset=iso-8859-1
# 	<mailto:bug-gnu-utils-request@gnu.org?subject=subscribe>
# 	<mailto:bug-gnu-utils-request@gnu.org?subject=uns
# Content-Length: 3137
# 
# 
# 
# Hi,
# 
# I think I have run into a bug in gawk's handling of REs of the
# form [...]* when the bracketed list includes certain 8-bit characters,
# specifically u-acute (octal \372).
# 
# The problem occurs in GNU Awk 3.0.4, both under 
# Linux 2.2.14-5.0 (intel i686) and SunOS 5.5 (Sun sparc).
# 
# Here is a program that illustrates the bug, and its output.
# The first two lines of the output should be equal, shouldn't they?
# 
# ----------------------------------------------------------------------
#! /usr/bin/gawk -f

BEGIN {
  s = "bananas and ananases in canaan";
  t = s; gsub(/[an]*n/, "AN", t);   printf "%-8s  %s\n", "[an]*n", t;
  t = s; gsub(/[an�]*n/, "AN", t);  printf "%-8s  %s\n", "[an�]*n", t;
  print "";
  t = s; gsub(/[a�]*n/, "AN", t);   printf "%-8s  %s\n", "[a�]*n", t;
  print "";
  t = s; gsub(/[an]n/, "AN", t);    printf "%-8s  %s\n", "[an]n", t;
  t = s; gsub(/[a�]n/, "AN", t);    printf "%-8s  %s\n", "[a�]n", t;
  t = s; gsub(/[an�]n/, "AN", t);   printf "%-8s  %s\n", "[an�]n", t;
  print "";
  t = s; gsub(/[an]?n/, "AN", t);   printf "%-8s  %s\n", "[an]?n", t;
  t = s; gsub(/[a�]?n/, "AN", t);   printf "%-8s  %s\n", "[a�]?n", t;
  t = s; gsub(/[an�]?n/, "AN", t);  printf "%-8s  %s\n", "[an�]?n", t;
  print "";
  t = s; gsub(/[an]+n/, "AN", t);   printf "%-8s  %s\n", "[an]+n", t;
  t = s; gsub(/[a�]+n/,  "AN", t);  printf "%-8s  %s\n", "[a�]+n", t;
  t = s; gsub(/[an�]+n/, "AN", t);  printf "%-8s  %s\n", "[an�]+n", t;
}
# ----------------------------------------------------------------------
# [an]*n    bANas ANd ANases iAN cAN
# [an�]*n   bananas and ananases in canaan
# 
# [a�]*n    bANANas ANd ANANases iAN cANAN
# 
# [an]n     bANANas ANd ANANases in cANaAN
# [a�]n     bANANas ANd ANANases in cANaAN
# [an�]n    bANANas ANd ANANases in cANaAN
# 
# [an]?n    bANANas ANd ANANases iAN cANaAN
# [a�]?n    bANANas ANd ANANases iAN cANaAN
# [an�]?n   bANANas ANd ANANases iAN cANaAN
# 
# [an]+n    bANas ANd ANases in cAN
# [a�]+n    bANANas ANd ANANases in cANAN
# [an�]+n   bananas and ananases in canaan
# ----------------------------------------------------------------------
# 
# Apparently the problem is specific to u-acute; I've tried several
# other 8-bit characters and they seem to behave as expected.
# 
# By comparing the second and third output lines, it would seem that the
# problem involves backtracking out of a partial match of [...]* in
# order to match the next sub-expression, when the latter begins with
# one of the given characters.
# 
# 
# All the best,
# 
# --stolfi
# 
# ------------------------------------------------------------------------
# Jorge Stolfi | http://www.dcc.unicamp.br/~stolfi | stolfi@dcc.unicamp.br 
# Institute of Computing (formerly DCC-IMECC)      | Wrk +55 (19)3788-5858
# Universidade Estadual de Campinas (UNICAMP)      |     +55 (19)3788-5840
# Av. Albert Einstein 1251 - Caixa Postal 6176     | Fax +55 (19)3788-5847
# 13083-970 Campinas, SP -- Brazil                 | Hom +55 (19)3287-4069                 
# ------------------------------------------------------------------------
# 
# _______________________________________________
# Bug-gnu-utils mailing list
# Bug-gnu-utils@gnu.org
# http://mail.gnu.org/mailman/listinfo/bug-gnu-utils
# 
# 
