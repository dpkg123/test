#C+
# Copyright 1995 by Softway Systems, Inc.  All rights reserved.
# 
# This Software Source is the unpublished, valuable, confidential property
# of Softway Systems, Inc.  Use is authorised only under the terms and
# conditions set out in the source code licence agreement protecting this
# Software Source.  Any unauthorised use or disclosure of this Software 
# Source is prohibited.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' BY SOFTWAY SYSTEMS, INC. AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL SOFTWAY SYSTEMS, INC. BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#C-
#
# makefile 
#
# Created by NAME, DATE
#

# DEBUG= -g -DNDEBUG
# DEBUG= 
# DEBUG_C89= -X


# Things you might add to DEFS (configure usually figures out what to do):
# -DGNU_STANDARD        Behave as gzip even if invoked as gunzip (GNU standard)
# -DDIRENT              Use <dirent.h>  for recursion (-r)
# -DSYSDIR              Use <sys/dir.h> for recursion (-r)
# -DSYSNDIR             Use <sys/ndir.h> for recursion (-r)
# -DNDIR                Use <ndir.h> for recursion (-r)
# -DSTDC_HEADERS        Use <stdlib.h>
# -DHAVE_UNISTD_H       Use <unistd.h>
# -DNO_FCNTL_H          Don't use <fcntl.h>
# -DNO_UTIME_H          Don't use <utime.h>
# -DHAVE_SYSUTIME_H     Use <sys/utime.h>
# -DNO_MEMORY_H         Don't use <memory.h>. Not needed if STDC_HEADERS.
# -DNO_STRING_H         Use strings.h, not string.h. Not needed if STDC_HEADERS
# -DRETSIGTYPE=int      Define this if signal handlers must return an int.
# -DNO_SYMLINK          OS defines S_IFLNK but does not support symbolic links
# -DNO_MULTIPLE_DOTS    System does not allow file names with multiple dots
# -DNO_UTIME            System does not support setting file modification time
# -DNO_CHOWN            System does not support setting file owner
# -DNO_DIR              System does not support readdir()
# -DPROTO               Force function prototypes even if __STDC__ not defined
# -DASMV                Use asm version match.S
# -DMSDOS               MSDOS specific
# -DOS2                 OS/2 specific
# -DVAXC                Vax/VMS with Vax C compiler
# -DVMS                 Vax/VMS with gcc
# -DDEBUG               Debug code
# -DDYN_ALLOC           Use dynamic allocation of large data structures
# -DMAXSEG_64K          Maximum array size is 64K (for 16 bit system)
# -DRECORD_IO           read() and write() are rounded to record sizes.
# -DNO_STDIN_FSTAT      fstat() is not available on stdin
# -DNO_FSTAT            fstat() is not available
# -DNO_SIZE_CHECK       stat() does not give a reliable file size

DEFS=-DDIRENT -DSTDC_HEADERS -DHAVE_UNISTD_C

PRE_INCLDIRS=-I.
CFLAGS+= $(DEFS)

PROG=gunzip
ALIASES=gzip pack unpack pcat

OBJS = gzip.o zip.o deflate.o trees.o bits.o unzip.o inflate.o util.o \
       crypt.o lzw.o unlzw.o unpack.o unlzh.o getopt.o getopt1.o rpmatch.o \
	yesno.o

targets:
	@echo " "
	@echo "Targets available:"
	@echo " "
	@echo "	gunzip"
	@echo " "
	@echo "	clean   - remove all intermediate targets."
	@echo "	clobber - remove all targets."
	@echo "	install - Install product in $DESTDIR ($DESTDIR)"
	@echo " "

$(PROG): $(OBJS)
	$(BUILD)

clean:
	rm -rf *.o

clobber: clean
	rm -rf *.exe

install:
	$(INSTALL) $(PROG) $(DESTDIR_CONTRIB_BIN) $(ALIASES)
	$(INSTALL) "$(PROG).pdb" $(DESTDIR_PDB)

fullbuild: clobber $(PROG) install

gzip.o:      gzip.c gzip.h tailor.h
zip.o:       zip.c gzip.h tailor.h crypt.h
deflate.o:   deflate.c gzip.h tailor.h
trees.o:     trees.c gzip.h tailor.h
bits.o:      bits.c gzip.h tailor.h crypt.h
unzip.o:     unzip.c gzip.h tailor.h crypt.h
inflate.o:   inflate.c gzip.h tailor.h crypt.h
util.o:      util.c gzip.h tailor.h
lzw.o:       lzw.c gzip.h tailor.h
unlzw.o:     unlzw.c gzip.h tailor.h revision.h lzw.h
unpack.o:    unpack.c gzip.h tailor.h
crypt.o:     crypt.c gzip.h tailor.h
unlzh.o:     unlzh.c gzip.h tailor.h lzw.h

