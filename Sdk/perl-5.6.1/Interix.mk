#
# Makefile to build PERL during Interix build process
#
# This top section contains the rules/targets
# to build from Perl sources
#


#
GDBMFLAGS = --prefix=/usr/local --host=intel-pclocal-interix
GDBM = gdbm-1.8.0/.libs/libgdbm.a

all: begin $(GDBM) perl package tgz test 

build: begin $(GDBM) perl package tgz # omit test
	
$(GDBM): begin
	cd gdbm-1.8.0; ./configure $(GDBMFLAGS); make

checkfs:
	# Perl can only be built if run on a local filesystem
	# which is case sensitive.
	#
	@print "this is a test" > .testfile1
	@print "this is a 2nd test" > .Testfile1
	@if  cmp -s .testfile1 .Testfile1 ; then \
	     print "This is not a case sensitive filesystem. Exiting"; \
	     exit 1; \
	fi; \
	


begin: checkfs
	# SD initializes files with funny perms. 
	# We need to make sure they are alright for the build
	#
	chmod 555 ext/*/typemap
	chmod 555 lib/ExtUtils/typemap
	#
	# change file/directory ownership and perms in the test directory
	# dirs - read/execute
	# files - read
	# files with .t suffix - read/execute
	#
	find t -type d -o -type f  | xargs chown $$(id -un)
	find t -type d | xargs chmod 755
	find t -type f | xargs chmod 444
	find t -type f -a -name \*.t | xargs chmod 544
	#
	# and certain files are presumed to be executable.
	chmod a+x  */configure   */config.sub   Configure  nm.Interix

perl: $(GDBM) 
	sh ./buildperl
	$(MAKE)

package:
	# "Install" perl into a local usr directory.
	rm -rf usr
	./perl ./installperl -p $$(pwd)
	# Because Windows has problems with Pod vs. pod, combine them.
	mv usr/local/lib/perl5/5.6.1/pod/* usr/local/lib/perl5/5.6.1/Pod
	rmdir usr/local/lib/perl5/5.6.1/pod
	# The install process will put in a symlink.
	# We don't run installman because Configure figured out we
	# didn't have nroff, and it shouldn't do anything.
	# If we ever do get nroff, then we need to run installman with
	# the --man1dir and --man3dir options to put things in the right
	# place.  (Can't do that now because the test for no-nroff is
	# overridden if --man1dir or --man3dir are used (overloaded test)).

test: 
	$(MAKE) test

clobber: #clean
	cd gdbm-1.8.0; rm -f config.status config.cache
	rm -f $(GDBM)    # gdbm-1.8.0/.libs/libgdbm.a

clean:
	cd gdbm-1.8.0; make clean
	pwd
	make clean  #in Makefile, not here

distclean: clobber
	#
	# in case there's a Makefile left around from
	# a previous build, use it
	#
	-cd gdbm-1.8.0; make distclean
	-make distclean  #in Makefile, not here
	#
	# now make sure we're clean here
	#
	rm -f config.sh config.h config.out Policy.sh

#
#
# ***** special targets/rules for Interix build process.
#
# This assumes there exists a .tgz file 
# with all the perl files in it - already built.
# So all the build process has to do is unpack the tar file.
#

clobber: FRC

index:
	gunzip -c ${.CURDIR}/$(MACHINE).tgz | tar tvf -

unpack:
	gunzip -c ${.CURDIR}/$(MACHINE).tgz | tar xf -

install: FRC
	# install the snapshot tarball into the DESTDIR_ROOT
	# The tarball contains the sub-directory names that
	# are appropriate (like "usr/local/bin")
	#
	cd $(DESTDIR_ROOT); gunzip -c ${.CURDIR}/$(MACHINE).tgz | tar xvf -


#
# fullbuild: target used in Interix nightly build process.
#    Since perl is pre-compiled and in a tarball for distribution
#   (ie the build of perl is not part of the nightly build process yet)
#   we just want fullbuild to install the tarball
#
#   Use 'build' target to actually re-build perl
#
fullbuild: clobber install

tgz:
	rm -f i386.tgz
	find usr -type f -o -type l | tar cf - | gzip > i386.tgz

FRC:	# special target to force the recipe to run
