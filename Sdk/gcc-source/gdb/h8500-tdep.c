/* Target-dependent code for Hitachi H8/500, for GDB.

   Copyright 1993, 1994, 1995, 1998, 2000, 2001, 2002 Free Software
   Foundation, Inc.

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

/*
   Contributed by Steve Chamberlain
   sac@cygnus.com
 */

#include "defs.h"
#include "frame.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "value.h"
#include "dis-asm.h"
#include "gdbcore.h"
#include "regcache.h"

#define UNSIGNED_SHORT(X) ((X) & 0xffff)

static int code_size = 2;

static int data_size = 2;

/* Shape of an H8/500 frame :

   arg-n
   ..
   arg-2
   arg-1
   return address <2 or 4 bytes>
   old fp         <2 bytes>
   auto-n
   ..
   auto-1
   saved registers

 */

/* an easy to debug H8 stack frame looks like:
   0x6df6               push    r6
   0x0d76       mov.w   r7,r6
   0x6dfn          push    reg
   0x7905 nnnn          mov.w  #n,r5    or   0x1b87  subs #2,sp
   0x1957               sub.w  r5,sp

 */

#define IS_PUSH(x) (((x) & 0xff00)==0x6d00)
#define IS_LINK_8(x) ((x) == 0x17)
#define IS_LINK_16(x) ((x) == 0x1f)
#define IS_MOVE_FP(x) ((x) == 0x0d76)
#define IS_MOV_SP_FP(x) ((x) == 0x0d76)
#define IS_SUB2_SP(x) ((x) == 0x1b87)
#define IS_MOVK_R5(x) ((x) == 0x7905)
#define IS_SUB_R5SP(x) ((x) == 0x1957)

#define LINK_8 0x17
#define LINK_16 0x1f

int minimum_mode = 1;

CORE_ADDR
h8500_skip_prologue (CORE_ADDR start_pc)
{
  short int w;

  w = read_memory_integer (start_pc, 1);
  if (w == LINK_8)
    {
      start_pc += 2;
      w = read_memory_integer (start_pc, 1);
    }

  if (w == LINK_16)
    {
      start_pc += 3;
      w = read_memory_integer (start_pc, 2);
    }

  return start_pc;
}

CORE_ADDR
h8500_addr_bits_remove (CORE_ADDR addr)
{
  return ((addr) & 0xffffff);
}

/* Given a GDB frame, determine the address of the calling function's frame.
   This will be used to create a new GDB frame struct, and then
   INIT_EXTRA_FRAME_INFO and INIT_FRAME_PC will be called for the new frame.

   For us, the frame address is its stack pointer value, so we look up
   the function prologue to determine the caller's sp value, and return it.  */

CORE_ADDR
h8500_frame_chain (struct frame_info *thisframe)
{
  if (!inside_entry_file (thisframe->pc))
    return (read_memory_integer (FRAME_FP (thisframe), PTR_SIZE));
  else
    return 0;
}

/* Fetch the instruction at ADDR, returning 0 if ADDR is beyond LIM or
   is not the address of a valid instruction, the address of the next
   instruction beyond ADDR otherwise.  *PWORD1 receives the first word
   of the instruction. */

CORE_ADDR
NEXT_PROLOGUE_INSN (CORE_ADDR addr, CORE_ADDR lim, char *pword1)
{
  if (addr < lim + 8)
    {
      read_memory (addr, pword1, 1);
      read_memory (addr, pword1 + 1, 1);
      return 1;
    }
  return 0;
}

/* Examine the prologue of a function.  `ip' points to the first
   instruction.  `limit' is the limit of the prologue (e.g. the addr
   of the first linenumber, or perhaps the program counter if we're
   stepping through).  `frame_sp' is the stack pointer value in use in
   this frame.  `fsr' is a pointer to a frame_saved_regs structure
   into which we put info about the registers saved by this frame.
   `fi' is a struct frame_info pointer; we fill in various fields in
   it to reflect the offsets of the arg pointer and the locals
   pointer.  */

/* Return the saved PC from this frame. */

CORE_ADDR
frame_saved_pc (struct frame_info *frame)
{
  return read_memory_integer (FRAME_FP (frame) + 2, PTR_SIZE);
}

void
h8500_pop_frame (void)
{
  unsigned regnum;
  struct frame_saved_regs fsr;
  struct frame_info *frame = get_current_frame ();

  get_frame_saved_regs (frame, &fsr);

  for (regnum = 0; regnum < 8; regnum++)
    {
      if (fsr.regs[regnum])
	write_register (regnum, read_memory_short (fsr.regs[regnum]));

      flush_cached_frames ();
    }
}

static void
h8500_print_register_hook (int regno)
{
  if (regno == CCR_REGNUM)
    {
      /* CCR register */

      int C, Z, N, V;
      unsigned char b[2];
      unsigned char l;

      frame_register_read (selected_frame, regno, b);
      l = b[1];
      printf_unfiltered ("\t");
      printf_unfiltered ("I-%d - ", (l & 0x80) != 0);
      N = (l & 0x8) != 0;
      Z = (l & 0x4) != 0;
      V = (l & 0x2) != 0;
      C = (l & 0x1) != 0;
      printf_unfiltered ("N-%d ", N);
      printf_unfiltered ("Z-%d ", Z);
      printf_unfiltered ("V-%d ", V);
      printf_unfiltered ("C-%d ", C);
      if ((C | Z) == 0)
	printf_unfiltered ("u> ");
      if ((C | Z) == 1)
	printf_unfiltered ("u<= ");
      if ((C == 0))
	printf_unfiltered ("u>= ");
      if (C == 1)
	printf_unfiltered ("u< ");
      if (Z == 0)
	printf_unfiltered ("!= ");
      if (Z == 1)
	printf_unfiltered ("== ");
      if ((N ^ V) == 0)
	printf_unfiltered (">= ");
      if ((N ^ V) == 1)
	printf_unfiltered ("< ");
      if ((Z | (N ^ V)) == 0)
	printf_unfiltered ("> ");
      if ((Z | (N ^ V)) == 1)
	printf_unfiltered ("<= ");
    }
}

static void
h8500_print_registers_info (struct gdbarch *gdbarch,
			    struct ui_file *file,
			    struct frame_info *frame,
			    int regnum, int print_all)
{
  int i;
  const int numregs = NUM_REGS + NUM_PSEUDO_REGS;
  char *raw_buffer = alloca (MAX_REGISTER_RAW_SIZE);
  char *virtual_buffer = alloca (MAX_REGISTER_VIRTUAL_SIZE);

  for (i = 0; i < numregs; i++)
    {
      /* Decide between printing all regs, non-float / vector regs, or
         specific reg.  */
      if (regnum == -1)
	{
	  if (!print_all)
	    {
	      if (TYPE_CODE (REGISTER_VIRTUAL_TYPE (i)) == TYPE_CODE_FLT)
		continue;
	      if (TYPE_VECTOR (REGISTER_VIRTUAL_TYPE (i)))
		continue;
	    }
	}
      else
	{
	  if (i != regnum)
	    continue;
	}

      /* If the register name is empty, it is undefined for this
         processor, so don't display anything.  */
      if (REGISTER_NAME (i) == NULL || *(REGISTER_NAME (i)) == '\0')
	continue;

      fputs_filtered (REGISTER_NAME (i), file);
      print_spaces_filtered (15 - strlen (REGISTER_NAME (i)), file);

      /* Get the data in raw format.  */
      if (! frame_register_read (frame, i, raw_buffer))
	{
	  fprintf_filtered (file, "*value not available*\n");
	  continue;
	}

      /* FIXME: cagney/2002-08-03: This code shouldn't be necessary.
         The function frame_register_read() should have returned the
         pre-cooked register so no conversion is necessary.  */
      /* Convert raw data to virtual format if necessary.  */
      if (REGISTER_CONVERTIBLE (i))
	{
	  REGISTER_CONVERT_TO_VIRTUAL (i, REGISTER_VIRTUAL_TYPE (i),
				       raw_buffer, virtual_buffer);
	}
      else
	{
	  memcpy (virtual_buffer, raw_buffer,
		  REGISTER_VIRTUAL_SIZE (i));
	}

      /* If virtual format is floating, print it that way, and in raw
         hex.  */
      if (TYPE_CODE (REGISTER_VIRTUAL_TYPE (i)) == TYPE_CODE_FLT)
	{
	  int j;

	  val_print (REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0, 0,
		     file, 0, 1, 0, Val_pretty_default);

	  fprintf_filtered (file, "\t(raw 0x");
	  for (j = 0; j < REGISTER_RAW_SIZE (i); j++)
	    {
	      int idx;
	      if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
		idx = j;
	      else
		idx = REGISTER_RAW_SIZE (i) - 1 - j;
	      fprintf_filtered (file, "%02x", (unsigned char) raw_buffer[idx]);
	    }
	  fprintf_filtered (file, ")");
	}
      else
	{
	  /* Print the register in hex.  */
	  val_print (REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0, 0,
		     file, 'x', 1, 0, Val_pretty_default);
          /* If not a vector register, print it also according to its
             natural format.  */
	  if (TYPE_VECTOR (REGISTER_VIRTUAL_TYPE (i)) == 0)
	    {
	      fprintf_filtered (file, "\t");
	      val_print (REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0, 0,
			 file, 0, 1, 0, Val_pretty_default);
	    }
	}

      /* Some h8500 specific info.  */
      h8500_print_register_hook (i);

      fprintf_filtered (file, "\n");
    }
}

void
h8500_do_registers_info (int regnum, int all)
{
  h8500_print_registers_info (current_gdbarch, gdb_stdout, selected_frame,
			      regnum, all);
}

int
h8500_register_size (int regno)
{
  switch (regno)
    {
    case SEG_C_REGNUM:
    case SEG_D_REGNUM:
    case SEG_E_REGNUM:
    case SEG_T_REGNUM:
      return 1;
    case R0_REGNUM:
    case R1_REGNUM:
    case R2_REGNUM:
    case R3_REGNUM:
    case R4_REGNUM:
    case R5_REGNUM:
    case R6_REGNUM:
    case R7_REGNUM:
    case CCR_REGNUM:
      return 2;

    case PR0_REGNUM:
    case PR1_REGNUM:
    case PR2_REGNUM:
    case PR3_REGNUM:
    case PR4_REGNUM:
    case PR5_REGNUM:
    case PR6_REGNUM:
    case PR7_REGNUM:
    case PC_REGNUM:
      return 4;
    default:
      internal_error (__FILE__, __LINE__, "failed internal consistency check");
    }
}

struct type *
h8500_register_virtual_type (int regno)
{
  switch (regno)
    {
    case SEG_C_REGNUM:
    case SEG_E_REGNUM:
    case SEG_D_REGNUM:
    case SEG_T_REGNUM:
      return builtin_type_unsigned_char;
    case R0_REGNUM:
    case R1_REGNUM:
    case R2_REGNUM:
    case R3_REGNUM:
    case R4_REGNUM:
    case R5_REGNUM:
    case R6_REGNUM:
    case R7_REGNUM:
    case CCR_REGNUM:
      return builtin_type_unsigned_short;
    case PR0_REGNUM:
    case PR1_REGNUM:
    case PR2_REGNUM:
    case PR3_REGNUM:
    case PR4_REGNUM:
    case PR5_REGNUM:
    case PR6_REGNUM:
    case PR7_REGNUM:
    case PC_REGNUM:
      return builtin_type_unsigned_long;
    default:
      internal_error (__FILE__, __LINE__, "failed internal consistency check");
    }
}

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

void
frame_find_saved_regs (struct frame_info *frame_info,
		       struct frame_saved_regs *frame_saved_regs)
{
  register int regnum;
  register int regmask;
  register CORE_ADDR next_addr;
  register CORE_ADDR pc;
  unsigned char thebyte;

  memset (frame_saved_regs, '\0', sizeof *frame_saved_regs);

  if ((frame_info)->pc >= (frame_info)->frame - CALL_DUMMY_LENGTH - FP_REGNUM * 4 - 4
      && (frame_info)->pc <= (frame_info)->frame)
    {
      next_addr = (frame_info)->frame;
      pc = (frame_info)->frame - CALL_DUMMY_LENGTH - FP_REGNUM * 4 - 4;
    }
  else
    {
      pc = get_pc_function_start ((frame_info)->pc);
      /* Verify we have a link a6 instruction next;
         if not we lose.  If we win, find the address above the saved
         regs using the amount of storage from the link instruction.
       */

      thebyte = read_memory_integer (pc, 1);
      if (0x1f == thebyte)
	next_addr = (frame_info)->frame + read_memory_integer (pc += 1, 2), pc += 2;
      else if (0x17 == thebyte)
	next_addr = (frame_info)->frame + read_memory_integer (pc += 1, 1), pc += 1;
      else
	goto lose;
#if 0
      /* FIXME steve */
      /* If have an add:g.waddal #-n, sp next, adjust next_addr.  */
      if ((0x0c0177777 & read_memory_integer (pc, 2)) == 0157774)
	next_addr += read_memory_integer (pc += 2, 4), pc += 4;
#endif
    }

  thebyte = read_memory_integer (pc, 1);
  if (thebyte == 0x12)
    {
      /* Got stm */
      pc++;
      regmask = read_memory_integer (pc, 1);
      pc++;
      for (regnum = 0; regnum < 8; regnum++, regmask >>= 1)
	{
	  if (regmask & 1)
	    {
	      (frame_saved_regs)->regs[regnum] = (next_addr += 2) - 2;
	    }
	}
      thebyte = read_memory_integer (pc, 1);
    }
  /* Maybe got a load of pushes */
  while (thebyte == 0xbf)
    {
      pc++;
      regnum = read_memory_integer (pc, 1) & 0x7;
      pc++;
      (frame_saved_regs)->regs[regnum] = (next_addr += 2) - 2;
      thebyte = read_memory_integer (pc, 1);
    }

lose:;

  /* Remember the address of the frame pointer */
  (frame_saved_regs)->regs[FP_REGNUM] = (frame_info)->frame;

  /* This is where the old sp is hidden */
  (frame_saved_regs)->regs[SP_REGNUM] = (frame_info)->frame;

  /* And the PC - remember the pushed FP is always two bytes long */
  (frame_saved_regs)->regs[PC_REGNUM] = (frame_info)->frame + 2;
}

CORE_ADDR
saved_pc_after_call (void)
{
  int x;
  int a = read_register (SP_REGNUM);

  x = read_memory_integer (a, code_size);
  if (code_size == 2)
    {
      /* Stick current code segement onto top */
      x &= 0xffff;
      x |= read_register (SEG_C_REGNUM) << 16;
    }
  x &= 0xffffff;
  return x;
}

void
h8500_set_pointer_size (int newsize)
{
  static int oldsize = 0;

  if (oldsize != newsize)
    {
      printf_unfiltered ("pointer size set to %d bits\n", newsize);
      oldsize = newsize;
      if (newsize == 32)
	{
	  minimum_mode = 0;
	}
      else
	{
	  minimum_mode = 1;
	}
      _initialize_gdbtypes ();
    }
}

static void
big_command (char *arg, int from_tty)
{
  h8500_set_pointer_size (32);
  code_size = 4;
  data_size = 4;
}

static void
medium_command (char *arg, int from_tty)
{
  h8500_set_pointer_size (32);
  code_size = 4;
  data_size = 2;
}

static void
compact_command (char *arg, int from_tty)
{
  h8500_set_pointer_size (32);
  code_size = 2;
  data_size = 4;
}

static void
small_command (char *arg, int from_tty)
{
  h8500_set_pointer_size (16);
  code_size = 2;
  data_size = 2;
}

static struct cmd_list_element *setmemorylist;

static void
set_memory (char *args, int from_tty)
{
  printf_unfiltered ("\"set memory\" must be followed by the name of a memory subcommand.\n");
  help_list (setmemorylist, "set memory ", -1, gdb_stdout);
}

/* See if variable name is ppc or pr[0-7] */

int
h8500_is_trapped_internalvar (char *name)
{
  if (name[0] != 'p')
    return 0;

  if (strcmp (name + 1, "pc") == 0)
    return 1;

  if (name[1] == 'r'
      && name[2] >= '0'
      && name[2] <= '7'
      && name[3] == '\000')
    return 1;
  else
    return 0;
}

struct value *
h8500_value_of_trapped_internalvar (struct internalvar *var)
{
  LONGEST regval;
  unsigned char regbuf[4];
  int page_regnum, regnum;

  regnum = var->name[2] == 'c' ? PC_REGNUM : var->name[2] - '0';

  switch (var->name[2])
    {
    case 'c':
      page_regnum = SEG_C_REGNUM;
      break;
    case '0':
    case '1':
    case '2':
    case '3':
      page_regnum = SEG_D_REGNUM;
      break;
    case '4':
    case '5':
      page_regnum = SEG_E_REGNUM;
      break;
    case '6':
    case '7':
      page_regnum = SEG_T_REGNUM;
      break;
    }

  get_saved_register (regbuf, NULL, NULL, selected_frame, page_regnum, NULL);
  regval = regbuf[0] << 16;

  get_saved_register (regbuf, NULL, NULL, selected_frame, regnum, NULL);
  regval |= regbuf[0] << 8 | regbuf[1];		/* XXX host/target byte order */

  xfree (var->value);		/* Free up old value */

  var->value = value_from_longest (builtin_type_unsigned_long, regval);
  release_value (var->value);	/* Unchain new value */

  VALUE_LVAL (var->value) = lval_internalvar;
  VALUE_INTERNALVAR (var->value) = var;
  return var->value;
}

void
h8500_set_trapped_internalvar (struct internalvar *var, struct value *newval,
			       int bitpos, int bitsize, int offset)
{
  char *page_regnum, *regnum;
  char expression[100];
  unsigned new_regval;
  struct type *type;
  enum type_code newval_type_code;

  type = check_typedef (VALUE_TYPE (newval));
  newval_type_code = TYPE_CODE (type);

  if ((newval_type_code != TYPE_CODE_INT
       && newval_type_code != TYPE_CODE_PTR)
      || TYPE_LENGTH (type) != sizeof (new_regval))
    error ("Illegal type (%s) for assignment to $%s\n",
	   TYPE_NAME (VALUE_TYPE (newval)), var->name);

  new_regval = *(long *) VALUE_CONTENTS_RAW (newval);

  regnum = var->name + 1;

  switch (var->name[2])
    {
    case 'c':
      page_regnum = "cp";
      break;
    case '0':
    case '1':
    case '2':
    case '3':
      page_regnum = "dp";
      break;
    case '4':
    case '5':
      page_regnum = "ep";
      break;
    case '6':
    case '7':
      page_regnum = "tp";
      break;
    }

  sprintf (expression, "$%s=%d", page_regnum, new_regval >> 16);
  parse_and_eval (expression);

  sprintf (expression, "$%s=%d", regnum, new_regval & 0xffff);
  parse_and_eval (expression);
}

CORE_ADDR
h8500_read_sp (void)
{
  return read_register (PR7_REGNUM);
}

void
h8500_write_sp (CORE_ADDR v)
{
  write_register (PR7_REGNUM, v);
}

CORE_ADDR
h8500_read_pc (ptid_t ptid)
{
  return read_register (PC_REGNUM);
}

void
h8500_write_pc (CORE_ADDR v, ptid_t ptid)
{
  write_register (PC_REGNUM, v);
}

CORE_ADDR
h8500_read_fp (void)
{
  return read_register (PR6_REGNUM);
}

void
_initialize_h8500_tdep (void)
{
  tm_print_insn = print_insn_h8500;

  add_prefix_cmd ("memory", no_class, set_memory,
		  "set the memory model", &setmemorylist, "set memory ", 0,
		  &setlist);

  add_cmd ("small", class_support, small_command,
      "Set small memory model. (16 bit code, 16 bit data)", &setmemorylist);

  add_cmd ("big", class_support, big_command,
	"Set big memory model. (32 bit code, 32 bit data)", &setmemorylist);

  add_cmd ("medium", class_support, medium_command,
     "Set medium memory model. (32 bit code, 16 bit data)", &setmemorylist);

  add_cmd ("compact", class_support, compact_command,
    "Set compact memory model. (16 bit code, 32 bit data)", &setmemorylist);

}
