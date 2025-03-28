/* Target dependent code for the NS32000, for GDB.
   Copyright 1986, 1988, 1991, 1992, 1994, 1995, 1998, 1999, 2000, 2001,
   2002 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "frame.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "inferior.h"
#include "regcache.h"
#include "target.h"

#include "arch-utils.h"

#include "ns32k-tdep.h"
#include "gdb_string.h"

static int sign_extend (int value, int bits);
static CORE_ADDR ns32k_get_enter_addr (CORE_ADDR);
static int ns32k_localcount (CORE_ADDR enter_pc);
static void flip_bytes (void *, int);

static const char *
ns32k_register_name_32082 (int regno)
{
  static char *register_names[] =
  {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
    "sp", "fp", "pc", "ps",
    "l0", "l1", "l2", "l3", "xx",
  };

  if (regno < 0)
    return NULL;
  if (regno >= sizeof (register_names) / sizeof (*register_names))
    return NULL;

  return (register_names[regno]);
}

static const char *
ns32k_register_name_32382 (int regno)
{
  static char *register_names[] =
  {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
    "sp", "fp", "pc", "ps",
    "fsr",
    "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7", "xx",
  };

  if (regno < 0)
    return NULL;
  if (regno >= sizeof (register_names) / sizeof (*register_names))
    return NULL;

  return (register_names[regno]);
}

static int
ns32k_register_byte_32082 (int regno)
{
  if (regno >= NS32K_LP0_REGNUM)
    return (NS32K_LP0_REGNUM * 4) + ((regno - NS32K_LP0_REGNUM) * 8);

  return (regno * 4);
}

static int
ns32k_register_byte_32382 (int regno)
{
  /* This is a bit yuk.  The even numbered double precision floating
     point long registers occupy the same space as the even:odd numbered
     single precision floating point registers, but the extra 32381 FPU
     registers are at the end.  Doing it this way is compatible for both
     32081 and 32381 equipped machines.  */

  return ((regno < NS32K_LP0_REGNUM ? regno
           : (regno - NS32K_LP0_REGNUM) & 1 ? regno - 1
           : (regno - NS32K_LP0_REGNUM + FP0_REGNUM)) * 4);
}

static int
ns32k_register_raw_size (int regno)
{
  /* All registers are 4 bytes, except for the doubled floating
     registers.  */

  return ((regno >= NS32K_LP0_REGNUM) ? 8 : 4);
}

static int
ns32k_register_virtual_size (int regno)
{
  return ((regno >= NS32K_LP0_REGNUM) ? 8 : 4);
}

static struct type *
ns32k_register_virtual_type (int regno)
{
  if (regno < FP0_REGNUM)
    return (builtin_type_int);

  if (regno < FP0_REGNUM + 8)
    return (builtin_type_float);

  if (regno < NS32K_LP0_REGNUM)
    return (builtin_type_int); 

  return (builtin_type_double);
}

/* Immediately after a function call, return the saved PC.  Can't
   always go through the frames for this because on some systems,
   the new frame is not set up until the new function executes some
   instructions.  */

static CORE_ADDR
ns32k_saved_pc_after_call (struct frame_info *frame)
{
  return (read_memory_integer (read_register (SP_REGNUM), 4));
}

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

static CORE_ADDR
umax_skip_prologue (CORE_ADDR pc)
{
  register unsigned char op = read_memory_integer (pc, 1);
  if (op == 0x82)
    {
      op = read_memory_integer (pc + 2, 1);
      if ((op & 0x80) == 0)
	pc += 3;
      else if ((op & 0xc0) == 0x80)
	pc += 4;
      else
	pc += 6;
    }
  return pc;
}

static const unsigned char *
ns32k_breakpoint_from_pc (CORE_ADDR *pcp, int *lenp)
{
  static const unsigned char breakpoint_insn[] = { 0xf2 };

  *lenp = sizeof (breakpoint_insn);
  return breakpoint_insn;
}

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.
   Encore's C compiler often reuses same area on stack for args,
   so this will often not work properly.  If the arg names
   are known, it's likely most of them will be printed. */

static int
umax_frame_num_args (struct frame_info *fi)
{
  int numargs;
  CORE_ADDR pc;
  CORE_ADDR enter_addr;
  unsigned int insn;
  unsigned int addr_mode;
  int width;

  numargs = -1;
  enter_addr = ns32k_get_enter_addr ((fi)->pc);
  if (enter_addr > 0)
    {
      pc = ((enter_addr == 1)
	    ? SAVED_PC_AFTER_CALL (fi)
	    : FRAME_SAVED_PC (fi));
      insn = read_memory_integer (pc, 2);
      addr_mode = (insn >> 11) & 0x1f;
      insn = insn & 0x7ff;
      if ((insn & 0x7fc) == 0x57c
	  && addr_mode == 0x14)	/* immediate */
	{
	  if (insn == 0x57c)	/* adjspb */
	    width = 1;
	  else if (insn == 0x57d)	/* adjspw */
	    width = 2;
	  else if (insn == 0x57f)	/* adjspd */
	    width = 4;
	  else
	    internal_error (__FILE__, __LINE__, "bad else");
	  numargs = read_memory_integer (pc + 2, width);
	  if (width > 1)
	    flip_bytes (&numargs, width);
	  numargs = -sign_extend (numargs, width * 8) / 4;
	}
    }
  return numargs;
}

static int
sign_extend (int value, int bits)
{
  value = value & ((1 << bits) - 1);
  return (value & (1 << (bits - 1))
	  ? value | (~((1 << bits) - 1))
	  : value);
}

static void
flip_bytes (void *p, int count)
{
  char tmp;
  char *ptr = 0;

  while (count > 0)
    {
      tmp = *ptr;
      ptr[0] = ptr[count - 1];
      ptr[count - 1] = tmp;
      ptr++;
      count -= 2;
    }
}

/* Return the number of locals in the current frame given a
   pc pointing to the enter instruction.  This is used by
   ns32k_frame_init_saved_regs.  */

static int
ns32k_localcount (CORE_ADDR enter_pc)
{
  unsigned char localtype;
  int localcount;

  localtype = read_memory_integer (enter_pc + 2, 1);
  if ((localtype & 0x80) == 0)
    localcount = localtype;
  else if ((localtype & 0xc0) == 0x80)
    localcount = (((localtype & 0x3f) << 8)
		  | (read_memory_integer (enter_pc + 3, 1) & 0xff));
  else
    localcount = (((localtype & 0x3f) << 24)
		  | ((read_memory_integer (enter_pc + 3, 1) & 0xff) << 16)
		  | ((read_memory_integer (enter_pc + 4, 1) & 0xff) << 8)
		  | (read_memory_integer (enter_pc + 5, 1) & 0xff));
  return localcount;
}


/* Nonzero if instruction at PC is a return instruction.  */

static int
ns32k_about_to_return (CORE_ADDR pc)
{
  return (read_memory_integer (pc, 1) == 0x12);
}

/* Get the address of the enter opcode for this function, if it is active.
   Returns positive address > 1 if pc is between enter/exit, 
   1 if pc before enter or after exit, 0 otherwise. */
static CORE_ADDR
ns32k_get_enter_addr (CORE_ADDR pc)
{
  CORE_ADDR enter_addr;
  unsigned char op;

  if (pc == 0)
    return 0;

  if (ns32k_about_to_return (pc))
    return 1;			/* after exit */

  enter_addr = get_pc_function_start (pc);

  if (pc == enter_addr)
    return 1;			/* before enter */

  op = read_memory_integer (enter_addr, 1);

  if (op != 0x82)
    return 0;			/* function has no enter/exit */

  return enter_addr;		/* pc is between enter and exit */
}

static CORE_ADDR
ns32k_frame_chain (struct frame_info *frame)
{
  /* In the case of the NS32000 series, the frame's nominal address is the
     FP value, and that address is saved at the previous FP value as a
     4-byte word.  */

  if (inside_entry_file (frame->pc))
    return 0;

  return (read_memory_integer (frame->frame, 4));
}

static CORE_ADDR
ns32k_frame_saved_pc (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return (sigtramp_saved_pc (frame)); /* XXXJRT */

  return (read_memory_integer (frame->frame + 4, 4));
}

static CORE_ADDR
ns32k_frame_args_address (struct frame_info *frame)
{
  if (ns32k_get_enter_addr (frame->pc) > 1)
    return (frame->frame);

  return (read_register (SP_REGNUM) - 4);
}

static CORE_ADDR
ns32k_frame_locals_address (struct frame_info *frame)
{
  return (frame->frame);
}

/* Code to initialize the addresses of the saved registers of frame described
   by FRAME_INFO.  This includes special registers such as pc and fp saved in
   special ways in the stack frame.  sp is even more special: the address we
   return for it IS the sp for the next frame.  */

static void
ns32k_frame_init_saved_regs (struct frame_info *frame)
{
  int regmask, regnum;
  int localcount;
  CORE_ADDR enter_addr, next_addr;

  if (frame->saved_regs)
    return;

  frame_saved_regs_zalloc (frame);

  enter_addr = ns32k_get_enter_addr (frame->pc);
  if (enter_addr > 1)
    {
      regmask = read_memory_integer (enter_addr + 1, 1) & 0xff;
      localcount = ns32k_localcount (enter_addr);
      next_addr = frame->frame + localcount;

      for (regnum = 0; regnum < 8; regnum++)
	{
          if (regmask & (1 << regnum))
	    frame->saved_regs[regnum] = next_addr -= 4;
	}

      frame->saved_regs[SP_REGNUM] = frame->frame + 4;
      frame->saved_regs[PC_REGNUM] = frame->frame + 4;
      frame->saved_regs[FP_REGNUM] = read_memory_integer (frame->frame, 4);
    }
  else if (enter_addr == 1)
    {
      CORE_ADDR sp = read_register (SP_REGNUM);
      frame->saved_regs[PC_REGNUM] = sp;
      frame->saved_regs[SP_REGNUM] = sp + 4;
    }
}

static void
ns32k_push_dummy_frame (void)
{
  CORE_ADDR sp = read_register (SP_REGNUM);
  int regnum;

  sp = push_word (sp, read_register (PC_REGNUM));
  sp = push_word (sp, read_register (FP_REGNUM));
  write_register (FP_REGNUM, sp);

  for (regnum = 0; regnum < 8; regnum++)
    sp = push_word (sp, read_register (regnum));

  write_register (SP_REGNUM, sp);
}

static void
ns32k_pop_frame (void)
{
  struct frame_info *frame = get_current_frame ();
  CORE_ADDR fp;
  int regnum;

  fp = frame->frame;
  FRAME_INIT_SAVED_REGS (frame);

  for (regnum = 0; regnum < 8; regnum++)
    if (frame->saved_regs[regnum])
      write_register (regnum,
		      read_memory_integer (frame->saved_regs[regnum], 4));

  write_register (FP_REGNUM, read_memory_integer (fp, 4));
  write_register (PC_REGNUM, read_memory_integer (fp + 4, 4));
  write_register (SP_REGNUM, fp + 8);
  flush_cached_frames ();
}

/* The NS32000 call dummy sequence:

	enter	0xff,0			82 ff 00
	jsr	@0x00010203		7f ae c0 01 02 03
	adjspd	0x69696969		7f a5 01 02 03 04
	bpt				f2

   It is 16 bytes long.  */

static LONGEST ns32k_call_dummy_words[] =
{
  0x7f00ff82,
  0x0201c0ae,
  0x01a57f03,
  0xf2040302
};
static int sizeof_ns32k_call_dummy_words = sizeof (ns32k_call_dummy_words);

#define NS32K_CALL_DUMMY_ADDR         5
#define NS32K_CALL_DUMMY_NARGS        11

static void
ns32k_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun, int nargs,
                      struct value **args, struct type *type, int gcc_p)
{
  int flipped;

  flipped = fun | 0xc0000000;
  flip_bytes (&flipped, 4);
  store_unsigned_integer (dummy + NS32K_CALL_DUMMY_ADDR, 4, flipped);

  flipped = - nargs * 4;
  flip_bytes (&flipped, 4);
  store_unsigned_integer (dummy + NS32K_CALL_DUMMY_NARGS, 4, flipped);
}

static void
ns32k_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  /* On this machine, this is a no-op (Encore Umax didn't use GCC).  */
}

static void
ns32k_extract_return_value (struct type *valtype, char *regbuf, char *valbuf)
{
  memcpy (valbuf,
          regbuf + REGISTER_BYTE (TYPE_CODE (valtype) == TYPE_CODE_FLT ?
				  FP0_REGNUM : 0), TYPE_LENGTH (valtype));
}

static void
ns32k_store_return_value (struct type *valtype, char *valbuf)
{
  write_register_bytes (TYPE_CODE (valtype) == TYPE_CODE_FLT ?
			FP0_REGNUM : 0, valbuf, TYPE_LENGTH (valtype));
}

static CORE_ADDR
ns32k_extract_struct_value_address (char *regbuf)
{
  return (extract_address (regbuf + REGISTER_BYTE (0), REGISTER_RAW_SIZE (0)));
}

void
ns32k_gdbarch_init_32082 (struct gdbarch *gdbarch)
{
  set_gdbarch_num_regs (gdbarch, NS32K_NUM_REGS_32082);

  set_gdbarch_register_name (gdbarch, ns32k_register_name_32082);
  set_gdbarch_register_bytes (gdbarch, NS32K_REGISTER_BYTES_32082);
  set_gdbarch_register_byte (gdbarch, ns32k_register_byte_32082);
}

void
ns32k_gdbarch_init_32382 (struct gdbarch *gdbarch)
{
  set_gdbarch_num_regs (gdbarch, NS32K_NUM_REGS_32382);

  set_gdbarch_register_name (gdbarch, ns32k_register_name_32382);
  set_gdbarch_register_bytes (gdbarch, NS32K_REGISTER_BYTES_32382);
  set_gdbarch_register_byte (gdbarch, ns32k_register_byte_32382);
}

/* Initialize the current architecture based on INFO.  If possible, re-use an
   architecture from ARCHES, which is a list of architectures already created
   during this debugging session.

   Called e.g. at program startup, when reading a core file, and when reading
   a binary file.  */

static struct gdbarch *
ns32k_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;
  enum gdb_osabi osabi = GDB_OSABI_UNKNOWN;

  /* Try to determine the OS ABI of the object we are loading.  */
  if (info.abfd != NULL)
    {
      osabi = gdbarch_lookup_osabi (info.abfd);
    }

  /* Find a candidate among extant architectures.  */
  for (arches = gdbarch_list_lookup_by_info (arches, &info);
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    {
      /* Make sure the OS ABI selection matches.  */
      tdep = gdbarch_tdep (arches->gdbarch);
      if (tdep && tdep->osabi == osabi)
	return arches->gdbarch;
    }

  tdep = xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  tdep->osabi = osabi;

  /* Register info */
  ns32k_gdbarch_init_32082 (gdbarch);
  set_gdbarch_num_regs (gdbarch, NS32K_SP_REGNUM);
  set_gdbarch_num_regs (gdbarch, NS32K_FP_REGNUM);
  set_gdbarch_num_regs (gdbarch, NS32K_PC_REGNUM);
  set_gdbarch_num_regs (gdbarch, NS32K_PS_REGNUM);

  set_gdbarch_register_size (gdbarch, NS32K_REGISTER_SIZE);
  set_gdbarch_register_raw_size (gdbarch, ns32k_register_raw_size);
  set_gdbarch_max_register_raw_size (gdbarch, NS32K_MAX_REGISTER_RAW_SIZE);
  set_gdbarch_register_virtual_size (gdbarch, ns32k_register_virtual_size);
  set_gdbarch_max_register_virtual_size (gdbarch,
                                         NS32K_MAX_REGISTER_VIRTUAL_SIZE);
  set_gdbarch_register_virtual_type (gdbarch, ns32k_register_virtual_type);

  /* Frame and stack info */
  set_gdbarch_skip_prologue (gdbarch, umax_skip_prologue);
  set_gdbarch_saved_pc_after_call (gdbarch, ns32k_saved_pc_after_call);

  set_gdbarch_frame_num_args (gdbarch, umax_frame_num_args);
  set_gdbarch_frameless_function_invocation (gdbarch,
                                   generic_frameless_function_invocation_not);
  
  set_gdbarch_frame_chain (gdbarch, ns32k_frame_chain);
  set_gdbarch_frame_chain_valid (gdbarch, func_frame_chain_valid);
  set_gdbarch_frame_saved_pc (gdbarch, ns32k_frame_saved_pc);

  set_gdbarch_frame_args_address (gdbarch, ns32k_frame_args_address);
  set_gdbarch_frame_locals_address (gdbarch, ns32k_frame_locals_address);

  set_gdbarch_frame_init_saved_regs (gdbarch, ns32k_frame_init_saved_regs);

  set_gdbarch_frame_args_skip (gdbarch, 8);

  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  /* Return value info */
  set_gdbarch_store_struct_return (gdbarch, ns32k_store_struct_return);
  set_gdbarch_deprecated_extract_return_value (gdbarch, ns32k_extract_return_value);
  set_gdbarch_deprecated_store_return_value (gdbarch, ns32k_store_return_value);
  set_gdbarch_deprecated_extract_struct_value_address (gdbarch,
                                            ns32k_extract_struct_value_address);

  /* Call dummy info */
  set_gdbarch_push_dummy_frame (gdbarch, ns32k_push_dummy_frame);
  set_gdbarch_pop_frame (gdbarch, ns32k_pop_frame);
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
  set_gdbarch_call_dummy_p (gdbarch, 1);
  set_gdbarch_call_dummy_words (gdbarch, ns32k_call_dummy_words);
  set_gdbarch_sizeof_call_dummy_words (gdbarch, sizeof_ns32k_call_dummy_words);
  set_gdbarch_fix_call_dummy (gdbarch, ns32k_fix_call_dummy);
  set_gdbarch_call_dummy_start_offset (gdbarch, 3);
  set_gdbarch_call_dummy_breakpoint_offset_p (gdbarch, 0);
  set_gdbarch_use_generic_dummy_frames (gdbarch, 0);
  set_gdbarch_pc_in_call_dummy (gdbarch, pc_in_call_dummy_on_stack);
  set_gdbarch_call_dummy_stack_adjust_p (gdbarch, 0);

  /* Breakpoint info */
  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_breakpoint_from_pc (gdbarch, ns32k_breakpoint_from_pc);

  /* Misc info */
  set_gdbarch_function_start_offset (gdbarch, 0);

  /* Hook in OS ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch, osabi);

  return (gdbarch);
}

static void
ns32k_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep == NULL)
    return;

  fprintf_unfiltered (file, "ns32k_dump_tdep: OS ABI = %s\n",
		      gdbarch_osabi_name (tdep->osabi));
}

void
_initialize_ns32k_tdep (void)
{
  gdbarch_register (bfd_arch_ns32k, ns32k_gdbarch_init, ns32k_dump_tdep);

  tm_print_insn = print_insn_ns32k;
}
