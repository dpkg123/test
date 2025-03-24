#C+
# Copyright 1997 by Softway Systems, Inc.  All rights reserved.
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
# Created by Bruce Payette, Aug. 22/1997
#
# $Header: /E/interix/src/cpio/Makefile,v 1.4 1997/10/09 15:30:47 SSI_DEV+bruce Exp $

# DEBUG= -g -DNDEBUG
# DEBUG= 
# DEBUG_C89= -X

#       @(#)Makefile	8.1 (Berkeley) 5/31/93


PROG=cpio

CFLAGS += -I. -DRETSIGTYPE=void -DHAVE_NETDB_H=1 -DSTDC_HEADERS=1 \
	-DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_FCNTL_H=1 \
	-DHAVE_UTIME_H=1 -DHAVE_STRERROR=1 -DHAVE_VPRINTF=1 -DC_ALLOCA=1 \
	-DSTACK_DIRECTION=-1 -DHAVE_DIRENT_H=1 -D_POSIX_SOURCE=1 -DNO_REMOTE

SRCS = copyin.c copyout.c copypass.c defer.c dstring.c global.c \
	main.c tar.c util.c error.c getopt.c getopt1.c filemode.c version.c \
	idcache.c makepath.c xmalloc.c stripslash.c \
	userspec.c xstrdup.c alloca.c dirname.c

OBJS=$(SRCS:.c=.o)

build: $(PROG)

targets:
	@( echo " ";\
	echo "Targets available:";\
	echo " ";\
	echo "	cpio";\
	echo " ";\
	echo "	clean   - remove all intermediate targets.";\
	echo "	clobber - remove all targets.";\
	echo "	install - Install product in $DESTDIR ($DESTDIR)";\
	echo " ";\
	)

$(PROG): $(OBJS)
	$(BUILD)

clean:
	rm -rf *.o

clobber: clean
	rm -rf *.exe

install:
	$(INSTALL) $(PROG) $(DESTDIR)
	$(INSTALL) "$(PROG).pdb" $(DESTDIR_PDB)

fullbuild: clobber $(PROG) install
