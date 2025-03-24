/* Native-dependent code for Interix running on i386's, for GDB.
   Copyright 2002 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"

#include <sys/procfs.h>
#include <inferior.h>
#include <fcntl.h>

#include <i386-tdep.h>
#include "gdb_string.h"
#include "gdbcore.h"
#include "gregset.h"
#include "regcache.h"

typedef unsigned long greg_t;

/* This is a duplicate of the table in i386-linux-nat.c.  */

static int regmap[] = {
  EAX, ECX, EDX, EBX,
  UESP, EBP, ESI, EDI,
  EIP, EFL, CS, SS,
  DS, ES, FS, GS,
};

/* Forward declarations.  */
extern void _initialize_core_interix (void);
extern initialize_file_ftype _initialize_core_interix;

/*  Given a pointer to a general register set in /proc format (gregset_t *),
    unpack the register contents and supply them as gdb's idea of the current
    register values.  */

void
supply_gregset (gregset_t *gregsetp)
{
  int regi;
  greg_t *regp = (greg_t *) & gregsetp->gregs;

  for (regi = 0; regi < I386_NUM_GREGS; regi++)
    {
      supply_register (regi, (char *) (regp + regmap[regi]));
    }
}

/* Store GDB's value for REGNO in *GREGSETP.  If REGNO is -1, do all
   of them.  */

void
fill_gregset (gregset_t *gregsetp, int regno)
{
  int regi;
  greg_t *regp = (greg_t *) gregsetp->gregs;

  for (regi = 0; regi < I386_NUM_GREGS; regi++)
    if (regno == -1 || regi == regno)
      regcache_collect (regi, (void *) (regp + regmap[regi]));
}

/*  Given a pointer to a floating point register set in Interix /proc format
    (fpregset_t *), unpack the register contents and supply them as gdb's
    idea of the current floating point register values. */

/* What is the address of st(N) within the fpregset_t structure F?  */
#define FP_REG_SIZE 10
#define FPREGSET_T_FPREG_ADDR(f, n) \
        ((char *) &(f)->fpstate.fpregs + (n) * FP_REG_SIZE)

/* Fill GDB's register file with the floating-point register values in
   *FPREGSETP.  */

void 
supply_fpregset (fpregset_t *fpregsetp)
{
  int i;
  long l;

  /* Supply the floating-point registers.  */
  for (i = 0; i < 8; i++)
    supply_register (FP0_REGNUM + i, FPREGSET_T_FPREG_ADDR (fpregsetp, i));

  l = fpregsetp->fpstate.control & 0xffff;
  supply_register (FCTRL_REGNUM, (char *) &l);
  l = fpregsetp->fpstate.status & 0xffff;
  supply_register (FSTAT_REGNUM, (char *) &l);
  l = fpregsetp->fpstate.tag & 0xffff;
  supply_register (FTAG_REGNUM,  (char *) &l);
  supply_register (FCOFF_REGNUM, (char *) &fpregsetp->fpstate.erroroffset);
  l = fpregsetp->fpstate.dataselector & 0xffff;
  supply_register (FDS_REGNUM,   (char *) &l);
  supply_register (FDOFF_REGNUM, (char *) &fpregsetp->fpstate.dataoffset);
  
  /* Extract the code segment and opcode from the "fcs" member.  */

  l = fpregsetp->fpstate.errorselector & 0xffff;
  supply_register (FCS_REGNUM, (char *) &l);

  l = (fpregsetp->fpstate.errorselector >> 16) & ((1 << 11) - 1);
  supply_register (FOP_REGNUM, (char *) &l);
}


extern char * register_buffer (struct regcache *regcache, int regnum);

/* Given a pointer to a floating point register set in (fpregset_t *)
   format, update all of the registers from gdb's idea of the current
   floating point register set. 
   - FPREGSETP points to the structure to be filled. 
     NOTE: this is a structure, and altho we use memcpy to move the
     data around, it should be treated as a structure as much as possible. */

void
fill_fpregset (fpregset_t *fpregsetp,
	       int regno)
{
  int i;

#define fill(MEMBER, REGNO) memcpy (&fpregsetp->fpstate.MEMBER, 	      \
	    register_buffer (current_regcache, REGNO),			      \
	    sizeof (fpregsetp->fpstate.MEMBER))

  for (i = FP0_REGNUM; i < XMM0_REGNUM; i++)
    {
      /* Only do the registers we were asked to. */
      if (regno != -1 && regno != i)
	{
	  continue;
	}

      if (i == FCTRL_REGNUM)
	{
	  fill (control, FCTRL_REGNUM); 
	  continue;
	}
      if (i == FSTAT_REGNUM)
	{
	  fill (status, FSTAT_REGNUM); 
	  continue;
	}
      if (i == FTAG_REGNUM)
	{
	  fill (tag, FTAG_REGNUM); 
	  continue;
	}
      if (i == FCOFF_REGNUM)
	{
	  fill (erroroffset, FCOFF_REGNUM); 
	  continue;
	}
      if (i == FDOFF_REGNUM)
	{
	  fill (dataoffset, FDOFF_REGNUM); 
	  continue;
	}
      if (i == FDS_REGNUM)
	{
	  fill (dataselector, FDS_REGNUM); 
	  continue;
	}

      if (i >= FP0_REGNUM && i <= FP0_REGNUM+7)
	{

	  memcpy (FPREGSET_T_FPREG_ADDR (fpregsetp, i-FP0_REGNUM),
		  register_buffer (current_regcache, i),
		  FP_REG_SIZE);
	  continue;
	}
      if (i == FCS_REGNUM)
	{
	  fpregsetp->fpstate.errorselector
	    = (fpregsetp->fpstate.errorselector & ~0xffff)
	       | ((* (int *) register_buffer (current_regcache, FCS_REGNUM)) 
		 & 0xffff);
	  continue;
	}

      if (i == FOP_REGNUM)
	{
	  fpregsetp->fpstate.errorselector
	    = (fpregsetp->fpstate.errorselector & 0xffff)
	       | (((*(int *) register_buffer (current_regcache, FOP_REGNUM)) 
		 & ((1 << 11) - 1)) << 16);
	  continue;
	}
    }

#undef fill
}

/* Read the values of either the general register set (WHICH equals 0)
   or the floating point register set (WHICH equals 2) from the core
   file data (pointed to by CORE_REG_SECT), and update gdb's idea of
   their current values.  The CORE_REG_SIZE parameter is compared to
   the size of the gregset or fpgregset structures (as appropriate) to
   validate the size of the structure from the core file.  The
   REG_ADDR parameter is ignored.  */

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size, int which,
                      CORE_ADDR reg_addr)
{
  gdb_gregset_t gregset;
  gdb_fpregset_t fpregset;

  if (which == 0)
    {
      if (core_reg_size != sizeof (gregset))
        {
          warning ("wrong size gregset struct in core file");
        }
      else
        {
          memcpy ((char *) &gregset, core_reg_sect, sizeof (gregset));
          supply_gregset (&gregset);
        }
    }
  else if (which == 2)
    {
      if (core_reg_size != sizeof (fpregset))
        {
          warning ("wrong size fpregset struct in core file");
        }
      else
        {
          memcpy ((char *) &fpregset, core_reg_sect, sizeof (fpregset));
          supply_fpregset (&fpregset);
        }
    }
}

#include <setjmp.h>

static struct core_fns interix_core_fns =
{
  bfd_target_coff_flavour,      /* core_flavour (more or less) */
  default_check_format,         /* check_format */
  default_core_sniffer,         /* core_sniffer */
  fetch_core_registers,         /* core_read_registers */
  NULL                          /* next */
};

void
_initialize_core_interix (void)
{
  add_core_fns (&interix_core_fns);
}

/* We don't have a /proc/pid/file or /proc/pid/exe to read a link from,
   so read it from the same place ps gets the name.  */

char *
child_pid_to_exec_file (int pid)
{
  char *path;
  char *buf;
  int fd, c;
  char *p;

  xasprintf (&path, "/proc/%d/stat", pid);
  buf = xcalloc (MAXPATHLEN + 1, sizeof (char));
  make_cleanup (xfree, path);
  make_cleanup (xfree, buf);

  fd = open (path, O_RDONLY);

  if (fd < 0)
    return NULL;

  /* Skip over "Argv0\t".  */
  lseek (fd, 6, SEEK_SET);

  c = read (fd, buf, MAXPATHLEN);
  close (fd);

  if (c < 0)
    return NULL;

  buf[c] = '\0';                /* Ensure null termination.  */
  p = strchr (buf, '\n');
  if (p != NULL)
    *p = '\0';

  return buf;
}
