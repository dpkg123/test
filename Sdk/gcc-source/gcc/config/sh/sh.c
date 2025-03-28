/* Output routines for GCC for Hitachi / SuperH SH.
   Copyright (C) 1993, 1994, 1995, 1997, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Contributed by Steve Chamberlain (sac@cygnus.com).
   Improved by Jim Wilson (wilson@cygnus.com). 

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "insn-config.h"
#include "rtl.h"
#include "tree.h"
#include "flags.h"
#include "expr.h"
#include "optabs.h"
#include "function.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "output.h"
#include "insn-attr.h"
#include "toplev.h"
#include "recog.h"
#include "c-pragma.h"
#include "integrate.h"
#include "tm_p.h"
#include "target.h"
#include "target-def.h"
#include "real.h"
#include "langhooks.h"

int code_for_indirect_jump_scratch = CODE_FOR_indirect_jump_scratch;

#define MSW (TARGET_LITTLE_ENDIAN ? 1 : 0)
#define LSW (TARGET_LITTLE_ENDIAN ? 0 : 1)

/* These are some macros to abstract register modes.  */
#define CONST_OK_FOR_ADD(size) \
  (TARGET_SHMEDIA ? CONST_OK_FOR_P (size) : CONST_OK_FOR_I (size))
#define GEN_MOV (*(TARGET_SHMEDIA64 ? gen_movdi : gen_movsi))
#define GEN_ADD3 (*(TARGET_SHMEDIA64 ? gen_adddi3 : gen_addsi3))
#define GEN_SUB3 (*(TARGET_SHMEDIA64 ? gen_subdi3 : gen_subsi3))

/* Set to 1 by expand_prologue() when the function is an interrupt handler.  */
int current_function_interrupt;

/* ??? The pragma interrupt support will not work for SH3.  */
/* This is set by #pragma interrupt and #pragma trapa, and causes gcc to
   output code for the next function appropriate for an interrupt handler.  */
int pragma_interrupt;

/* This is set by the trap_exit attribute for functions.   It specifies
   a trap number to be used in a trapa instruction at function exit
   (instead of an rte instruction).  */
int trap_exit;

/* This is used by the sp_switch attribute for functions.  It specifies
   a variable holding the address of the stack the interrupt function
   should switch to/from at entry/exit.  */
rtx sp_switch;

/* This is set by #pragma trapa, and is similar to the above, except that
   the compiler doesn't emit code to preserve all registers.  */
static int pragma_trapa;

/* This is set by #pragma nosave_low_regs.  This is useful on the SH3,
   which has a separate set of low regs for User and Supervisor modes.
   This should only be used for the lowest level of interrupts.  Higher levels
   of interrupts must save the registers in case they themselves are
   interrupted.  */
int pragma_nosave_low_regs;

/* This is used for communication between SETUP_INCOMING_VARARGS and
   sh_expand_prologue.  */
int current_function_anonymous_args;

/* Global variables for machine-dependent things.  */

/* Which cpu are we scheduling for.  */
enum processor_type sh_cpu;

/* Saved operands from the last compare to use when we generate an scc
   or bcc insn.  */

rtx sh_compare_op0;
rtx sh_compare_op1;

/* Provides the class number of the smallest class containing
   reg number.  */

int regno_reg_class[FIRST_PSEUDO_REGISTER] =
{
  R0_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  GENERAL_REGS, GENERAL_REGS, GENERAL_REGS, GENERAL_REGS,
  FP0_REGS,FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  FP_REGS, FP_REGS, FP_REGS, FP_REGS,
  TARGET_REGS, TARGET_REGS, TARGET_REGS, TARGET_REGS,
  TARGET_REGS, TARGET_REGS, TARGET_REGS, TARGET_REGS,
  DF_REGS, DF_REGS, DF_REGS, DF_REGS,
  DF_REGS, DF_REGS, DF_REGS, DF_REGS,
  NO_REGS, GENERAL_REGS, PR_REGS, T_REGS,
  MAC_REGS, MAC_REGS, FPUL_REGS, FPSCR_REGS,
  GENERAL_REGS,
};

char sh_register_names[FIRST_PSEUDO_REGISTER] \
  [MAX_REGISTER_NAME_LENGTH + 1] = SH_REGISTER_NAMES_INITIALIZER;

char sh_additional_register_names[ADDREGNAMES_SIZE] \
  [MAX_ADDITIONAL_REGISTER_NAME_LENGTH + 1]
  = SH_ADDITIONAL_REGISTER_NAMES_INITIALIZER;

/* Provide reg_class from a letter such as appears in the machine
   description.  *: target independently reserved letter.
   reg_class_from_letter['e'] is set to NO_REGS for TARGET_FMOVD.  */

enum reg_class reg_class_from_letter[] =
{
  /* a */ ALL_REGS,  /* b */ TARGET_REGS, /* c */ FPSCR_REGS, /* d */ DF_REGS,
  /* e */ FP_REGS,   /* f */ FP_REGS,  /* g **/ NO_REGS,     /* h */ NO_REGS,
  /* i **/ NO_REGS,  /* j */ NO_REGS,  /* k */ SIBCALL_REGS, /* l */ PR_REGS,
  /* m **/ NO_REGS,  /* n **/ NO_REGS, /* o **/ NO_REGS,     /* p **/ NO_REGS,
  /* q */ NO_REGS,   /* r **/ NO_REGS, /* s **/ NO_REGS,     /* t */ T_REGS,
  /* u */ NO_REGS,   /* v */ NO_REGS,  /* w */ FP0_REGS,     /* x */ MAC_REGS,
  /* y */ FPUL_REGS, /* z */ R0_REGS
};

int assembler_dialect;

static void split_branches PARAMS ((rtx));
static int branch_dest PARAMS ((rtx));
static void force_into PARAMS ((rtx, rtx));
static void print_slot PARAMS ((rtx));
static rtx add_constant PARAMS ((rtx, enum machine_mode, rtx));
static void dump_table PARAMS ((rtx));
static int hi_const PARAMS ((rtx));
static int broken_move PARAMS ((rtx));
static int mova_p PARAMS ((rtx));
static rtx find_barrier PARAMS ((int, rtx, rtx));
static int noncall_uses_reg PARAMS ((rtx, rtx, rtx *));
static rtx gen_block_redirect PARAMS ((rtx, int, int));
static void output_stack_adjust PARAMS ((int, rtx, int, rtx (*) (rtx)));
static rtx frame_insn PARAMS ((rtx));
static rtx push PARAMS ((int));
static void pop PARAMS ((int));
static void push_regs PARAMS ((HOST_WIDE_INT *));
static void calc_live_regs PARAMS ((int *, HOST_WIDE_INT *));
static void mark_use PARAMS ((rtx, rtx *));
static HOST_WIDE_INT rounded_frame_size PARAMS ((int));
static rtx mark_constant_pool_use PARAMS ((rtx));
const struct attribute_spec sh_attribute_table[];
static tree sh_handle_interrupt_handler_attribute PARAMS ((tree *, tree, tree, int, bool *));
static tree sh_handle_sp_switch_attribute PARAMS ((tree *, tree, tree, int, bool *));
static tree sh_handle_trap_exit_attribute PARAMS ((tree *, tree, tree, int, bool *));
static void sh_output_function_epilogue PARAMS ((FILE *, HOST_WIDE_INT));
static void sh_insert_attributes PARAMS ((tree, tree *));
static int sh_adjust_cost PARAMS ((rtx, rtx, rtx, int));
static int sh_use_dfa_interface PARAMS ((void));
static int sh_issue_rate PARAMS ((void));

static bool sh_cannot_modify_jumps_p PARAMS ((void));
static bool sh_ms_bitfield_layout_p PARAMS ((tree));

static void sh_encode_section_info PARAMS ((tree, int));
static const char *sh_strip_name_encoding PARAMS ((const char *));
static void sh_init_builtins PARAMS ((void));
static void sh_media_init_builtins PARAMS ((void));
static rtx sh_expand_builtin PARAMS ((tree, rtx, rtx, enum machine_mode, int));
static int flow_dependent_p PARAMS ((rtx, rtx));
static void flow_dependent_p_1 PARAMS ((rtx, rtx, void *));


/* Initialize the GCC target structure.  */
#undef TARGET_ATTRIBUTE_TABLE
#define TARGET_ATTRIBUTE_TABLE sh_attribute_table

/* The next two are used for debug info when compiling with -gdwarf.  */
#undef TARGET_ASM_UNALIGNED_HI_OP
#define TARGET_ASM_UNALIGNED_HI_OP "\t.uaword\t"
#undef TARGET_ASM_UNALIGNED_SI_OP
#define TARGET_ASM_UNALIGNED_SI_OP "\t.ualong\t"

/* These are NULLed out on non-SH5 in OVERRIDE_OPTIONS.  */
#undef TARGET_ASM_UNALIGNED_DI_OP
#define TARGET_ASM_UNALIGNED_DI_OP "\t.uaquad\t"
#undef TARGET_ASM_ALIGNED_DI_OP
#define TARGET_ASM_ALIGNED_DI_OP "\t.quad\t"

#undef TARGET_ASM_FUNCTION_EPILOGUE
#define TARGET_ASM_FUNCTION_EPILOGUE sh_output_function_epilogue

#undef TARGET_INSERT_ATTRIBUTES
#define TARGET_INSERT_ATTRIBUTES sh_insert_attributes

#undef TARGET_SCHED_ADJUST_COST
#define TARGET_SCHED_ADJUST_COST sh_adjust_cost

#undef TARGET_SCHED_USE_DFA_PIPELINE_INTERFACE 
#define TARGET_SCHED_USE_DFA_PIPELINE_INTERFACE \
				sh_use_dfa_interface
#undef TARGET_SCHED_ISSUE_RATE
#define TARGET_SCHED_ISSUE_RATE sh_issue_rate

#undef TARGET_CANNOT_MODIFY_JUMPS_P
#define TARGET_CANNOT_MODIFY_JUMPS_P sh_cannot_modify_jumps_p

#undef TARGET_MS_BITFIELD_LAYOUT_P
#define TARGET_MS_BITFIELD_LAYOUT_P sh_ms_bitfield_layout_p

#undef TARGET_ENCODE_SECTION_INFO
#define TARGET_ENCODE_SECTION_INFO sh_encode_section_info
#undef TARGET_STRIP_NAME_ENCODING
#define TARGET_STRIP_NAME_ENCODING sh_strip_name_encoding

#undef TARGET_INIT_BUILTINS
#define TARGET_INIT_BUILTINS sh_init_builtins
#undef TARGET_EXPAND_BUILTIN
#define TARGET_EXPAND_BUILTIN sh_expand_builtin

struct gcc_target targetm = TARGET_INITIALIZER;

/* Print the operand address in x to the stream.  */

void
print_operand_address (stream, x)
     FILE *stream;
     rtx x;
{
  switch (GET_CODE (x))
    {
    case REG:
    case SUBREG:
      fprintf (stream, "@%s", reg_names[true_regnum (x)]);
      break;

    case PLUS:
      {
	rtx base = XEXP (x, 0);
	rtx index = XEXP (x, 1);

	switch (GET_CODE (index))
	  {
	  case CONST_INT:
	    fprintf (stream, "@(%d,%s)", (int) INTVAL (index),
		     reg_names[true_regnum (base)]);
	    break;

	  case REG:
	  case SUBREG:
	    {
	      int base_num = true_regnum (base);
	      int index_num = true_regnum (index);

	      fprintf (stream, "@(r0,%s)",
		       reg_names[MAX (base_num, index_num)]);
	      break;
	    }

	  default:
	    debug_rtx (x);
	    abort ();
	  }
      }
      break;

    case PRE_DEC:
      fprintf (stream, "@-%s", reg_names[true_regnum (XEXP (x, 0))]);
      break;

    case POST_INC:
      fprintf (stream, "@%s+", reg_names[true_regnum (XEXP (x, 0))]);
      break;

    default:
      x = mark_constant_pool_use (x);
      output_addr_const (stream, x);
      break;
    }
}

/* Print operand x (an rtx) in assembler syntax to file stream
   according to modifier code.

   '.'  print a .s if insn needs delay slot
   ','  print LOCAL_LABEL_PREFIX
   '@'  print trap, rte or rts depending upon pragma interruptness
   '#'  output a nop if there is nothing to put in the delay slot
   '''  print likelyhood suffix (/u for unlikely).
   'O'  print a constant without the #
   'R'  print the LSW of a dp value - changes if in little endian
   'S'  print the MSW of a dp value - changes if in little endian
   'T'  print the next word of a dp value - same as 'R' in big endian mode.
   'M'  print an `x' if `m' will print `base,index'.
   'N'  print 'r63' if the operand is (const_int 0).
   'm'  print a pair `base,offset' or `base,index', for LD and ST.
   'u'  prints the lowest 16 bits of CONST_INT, as an unsigned value.
   'o'  output an operator.  */

void
print_operand (stream, x, code)
     FILE *stream;
     rtx x;
     int code;
{
  switch (code)
    {
    case '.':
      if (final_sequence
	  && ! INSN_ANNULLED_BRANCH_P (XVECEXP (final_sequence, 0, 0))
	  && get_attr_length (XVECEXP (final_sequence, 0, 1)))
	fprintf (stream, ASSEMBLER_DIALECT ? "/s" : ".s");
      break;
    case ',':
      fprintf (stream, "%s", LOCAL_LABEL_PREFIX);
      break;
    case '@':
      if (trap_exit)
	fprintf (stream, "trapa #%d", trap_exit);
      else if (sh_cfun_interrupt_handler_p ())
	fprintf (stream, "rte");
      else
	fprintf (stream, "rts");
      break;
    case '#':
      /* Output a nop if there's nothing in the delay slot.  */
      if (dbr_sequence_length () == 0)
	fprintf (stream, "\n\tnop");
      break;
    case '\'':
      {
	rtx note = find_reg_note (current_output_insn, REG_BR_PROB, 0);

	if (note && INTVAL (XEXP (note, 0)) * 2 < REG_BR_PROB_BASE)
	  fputs ("/u", stream);
	break;
      }
    case 'O':
      x = mark_constant_pool_use (x);
      output_addr_const (stream, x);
      break;
    case 'R':
      fputs (reg_names[REGNO (x) + LSW], (stream));
      break;
    case 'S':
      fputs (reg_names[REGNO (x) + MSW], (stream));
      break;
    case 'T':
      /* Next word of a double.  */
      switch (GET_CODE (x))
	{
	case REG:
	  fputs (reg_names[REGNO (x) + 1], (stream));
	  break;
	case MEM:
	  if (GET_CODE (XEXP (x, 0)) != PRE_DEC
	      && GET_CODE (XEXP (x, 0)) != POST_INC)
	    x = adjust_address (x, SImode, 4);
	  print_operand_address (stream, XEXP (x, 0));
	  break;
	default:
	  break;
	}
      break;
    case 'o':
      switch (GET_CODE (x))
	{
	case PLUS:  fputs ("add", stream); break;
	case MINUS: fputs ("sub", stream); break;
	case MULT:  fputs ("mul", stream); break;
	case DIV:   fputs ("div", stream); break;
	case EQ:    fputs ("eq",  stream); break;
	case NE:    fputs ("ne",  stream); break;
	case GT:  case LT:  fputs ("gt",  stream); break;
	case GE:  case LE:  fputs ("ge",  stream); break;
	case GTU: case LTU: fputs ("gtu", stream); break;
	case GEU: case LEU: fputs ("geu", stream); break;
	default:
	  break;
	}
      break;
    case 'M':
      if (GET_CODE (x) == MEM
	  && GET_CODE (XEXP (x, 0)) == PLUS
	  && (GET_CODE (XEXP (XEXP (x, 0), 1)) == REG
	      || GET_CODE (XEXP (XEXP (x, 0), 1)) == SUBREG))
	fputc ('x', stream);
      break;

    case 'm':
      if (GET_CODE (x) != MEM)
	abort ();
      x = XEXP (x, 0);
      switch (GET_CODE (x))
	{
	case REG:
	case SUBREG:
	  print_operand (stream, x, 0);
	  fputs (", 0", stream);
	  break;

	case PLUS:
	  print_operand (stream, XEXP (x, 0), 0);
	  fputs (", ", stream);
	  print_operand (stream, XEXP (x, 1), 0);
	  break;

	default:
	  abort ();
	}
      break;

    case 'N':
      if (x == CONST0_RTX (GET_MODE (x)))
	{
	  fprintf ((stream), "r63");
	  break;
	}
      goto default_output;
    case 'u':
      if (GET_CODE (x) == CONST_INT)
	{
	  fprintf ((stream), "%u", (unsigned) INTVAL (x) & (0x10000 - 1));
	  break;
	}
      /* Fall through.  */

    default_output:
    default:
      switch (GET_CODE (x))
	{
	  /* FIXME: We need this on SHmedia32 because reload generates
	     some sign-extended HI or QI loads into DImode registers
	     but, because Pmode is SImode, the address ends up with a
	     subreg:SI of the DImode register.  Maybe reload should be
	     fixed so as to apply alter_subreg to such loads?  */
	case SUBREG:
	  if (SUBREG_BYTE (x) != 0
	      || GET_CODE (SUBREG_REG (x)) != REG)
	    abort ();

	  x = SUBREG_REG (x);
	  /* Fall through.  */

	case REG:
	  if (FP_REGISTER_P (REGNO (x))
	      && GET_MODE (x) == V16SFmode)
	    fprintf ((stream), "mtrx%s", reg_names[REGNO (x)] + 2);
	  else if (FP_REGISTER_P (REGNO (x))
		   && GET_MODE (x) == V4SFmode)
	    fprintf ((stream), "fv%s", reg_names[REGNO (x)] + 2);
	  else if (GET_CODE (x) == REG
		   && GET_MODE (x) == V2SFmode)
	    fprintf ((stream), "fp%s", reg_names[REGNO (x)] + 2);
	  else if (FP_REGISTER_P (REGNO (x))
		   && GET_MODE_SIZE (GET_MODE (x)) > 4)
	    fprintf ((stream), "d%s", reg_names[REGNO (x)] + 1);
	  else
	    fputs (reg_names[REGNO (x)], (stream));
	  break;

	case MEM:
	  output_address (XEXP (x, 0));
	  break;
	  
	case CONST:
	  if (TARGET_SHMEDIA
	      && GET_CODE (XEXP (x, 0)) == SIGN_EXTEND
	      && GET_MODE (XEXP (x, 0)) == DImode
	      && GET_CODE (XEXP (XEXP (x, 0), 0)) == TRUNCATE
	      && GET_MODE (XEXP (XEXP (x, 0), 0)) == HImode)
	    {
	      rtx val = XEXP (XEXP (XEXP (x, 0), 0), 0);

	      fputc ('(', stream);
	      if (GET_CODE (val) == ASHIFTRT)
		{
		  fputc ('(', stream);
		  if (GET_CODE (XEXP (val, 0)) == CONST)
		    fputc ('(', stream);
		  output_addr_const (stream, XEXP (val, 0));
		  if (GET_CODE (XEXP (val, 0)) == CONST)
		    fputc (')', stream);
		  fputs (" >> ", stream);
		  output_addr_const (stream, XEXP (val, 1));
		  fputc (')', stream);
		}
	      else
		{
		  if (GET_CODE (val) == CONST)
		    fputc ('(', stream);
		  output_addr_const (stream, val);
		  if (GET_CODE (val) == CONST)
		    fputc (')', stream);
		}
	      fputs (" & 65535)", stream);
	      break;
	    }

	  /* Fall through.  */
	default:
	  if (TARGET_SH1)
	    fputc ('#', stream);
	  output_addr_const (stream, x);
	  break;
	}
      break;
    }
}

/* Like force_operand, but guarantees that VALUE ends up in TARGET.  */
static void
force_into (value, target)
     rtx value, target;
{
  value = force_operand (value, target);
  if (! rtx_equal_p (value, target))
    emit_insn (gen_move_insn (target, value));
}

/* Emit code to perform a block move.  Choose the best method.

   OPERANDS[0] is the destination.
   OPERANDS[1] is the source.
   OPERANDS[2] is the size.
   OPERANDS[3] is the alignment safe to use.  */

int
expand_block_move (operands)
     rtx *operands;
{
  int align = INTVAL (operands[3]);
  int constp = (GET_CODE (operands[2]) == CONST_INT);
  int bytes = (constp ? INTVAL (operands[2]) : 0);

  /* If it isn't a constant number of bytes, or if it doesn't have 4 byte
     alignment, or if it isn't a multiple of 4 bytes, then fail.  */
  if (! constp || align < 4 || (bytes % 4 != 0))
    return 0;

  if (TARGET_HARD_SH4)
    {
      if (bytes < 12)
	return 0;
      else if (bytes == 12)
	{
	  tree entry_name;
	  rtx sym;
	  rtx func_addr_rtx;
	  rtx r4 = gen_rtx (REG, SImode, 4);
	  rtx r5 = gen_rtx (REG, SImode, 5);

	  entry_name = get_identifier ("__movstrSI12_i4");

	  sym = gen_rtx_SYMBOL_REF (Pmode, IDENTIFIER_POINTER (entry_name));
	  func_addr_rtx = copy_to_mode_reg (Pmode, sym);
	  force_into (XEXP (operands[0], 0), r4);
	  force_into (XEXP (operands[1], 0), r5);
	  emit_insn (gen_block_move_real_i4 (func_addr_rtx));
	  return 1;
	}
      else if (! TARGET_SMALLCODE)
	{
	  tree entry_name;
	  rtx sym;
	  rtx func_addr_rtx;
	  int dwords;
	  rtx r4 = gen_rtx (REG, SImode, 4);
	  rtx r5 = gen_rtx (REG, SImode, 5);
	  rtx r6 = gen_rtx (REG, SImode, 6);

	  entry_name = get_identifier (bytes & 4
				       ? "__movstr_i4_odd"
				       : "__movstr_i4_even");
	  sym = gen_rtx_SYMBOL_REF (Pmode, IDENTIFIER_POINTER (entry_name));
	  func_addr_rtx = copy_to_mode_reg (Pmode, sym);
	  force_into (XEXP (operands[0], 0), r4);
	  force_into (XEXP (operands[1], 0), r5);

	  dwords = bytes >> 3;
	  emit_insn (gen_move_insn (r6, GEN_INT (dwords - 1)));
	  emit_insn (gen_block_lump_real_i4 (func_addr_rtx));
	  return 1;
	}
      else
	return 0;
    }
  if (bytes < 64)
    {
      char entry[30];
      tree entry_name;
      rtx sym;
      rtx func_addr_rtx;
      rtx r4 = gen_rtx_REG (SImode, 4);
      rtx r5 = gen_rtx_REG (SImode, 5);

      sprintf (entry, "__movstrSI%d", bytes);
      entry_name = get_identifier (entry);
      sym = gen_rtx_SYMBOL_REF (Pmode, IDENTIFIER_POINTER (entry_name));
      func_addr_rtx = copy_to_mode_reg (Pmode, sym);
      force_into (XEXP (operands[0], 0), r4);
      force_into (XEXP (operands[1], 0), r5);
      emit_insn (gen_block_move_real (func_addr_rtx));
      return 1;
    }

  /* This is the same number of bytes as a memcpy call, but to a different
     less common function name, so this will occasionally use more space.  */
  if (! TARGET_SMALLCODE)
    {
      tree entry_name;
      rtx sym;
      rtx func_addr_rtx;
      int final_switch, while_loop;
      rtx r4 = gen_rtx_REG (SImode, 4);
      rtx r5 = gen_rtx_REG (SImode, 5);
      rtx r6 = gen_rtx_REG (SImode, 6);

      entry_name = get_identifier ("__movstr");
      sym = gen_rtx_SYMBOL_REF (Pmode, IDENTIFIER_POINTER (entry_name));
      func_addr_rtx = copy_to_mode_reg (Pmode, sym);
      force_into (XEXP (operands[0], 0), r4);
      force_into (XEXP (operands[1], 0), r5);

      /* r6 controls the size of the move.  16 is decremented from it
	 for each 64 bytes moved.  Then the negative bit left over is used
	 as an index into a list of move instructions.  e.g., a 72 byte move
	 would be set up with size(r6) = 14, for one iteration through the
	 big while loop, and a switch of -2 for the last part.  */

      final_switch = 16 - ((bytes / 4) % 16);
      while_loop = ((bytes / 4) / 16 - 1) * 16;
      emit_insn (gen_move_insn (r6, GEN_INT (while_loop + final_switch)));
      emit_insn (gen_block_lump_real (func_addr_rtx));
      return 1;
    }

  return 0;
}

/* Prepare operands for a move define_expand; specifically, one of the
   operands must be in a register.  */

int
prepare_move_operands (operands, mode)
     rtx operands[];
     enum machine_mode mode;
{
  if ((mode == SImode || mode == DImode) && flag_pic)
    {
      rtx temp;
      if (SYMBOLIC_CONST_P (operands[1]))
	{
	  if (GET_CODE (operands[0]) == MEM)
	    operands[1] = force_reg (Pmode, operands[1]);
	  else if (TARGET_SHMEDIA
		   && GET_CODE (operands[1]) == LABEL_REF
		   && target_reg_operand (operands[0], mode))
	    /* It's ok.  */;
	  else
	    {
	      temp = no_new_pseudos ? operands[0] : gen_reg_rtx (Pmode);
	      operands[1] = legitimize_pic_address (operands[1], mode, temp);
	    }
	}
      else if (GET_CODE (operands[1]) == CONST
	       && GET_CODE (XEXP (operands[1], 0)) == PLUS
	       && SYMBOLIC_CONST_P (XEXP (XEXP (operands[1], 0), 0)))
	{
	  temp = no_new_pseudos ? operands[0] : gen_reg_rtx (Pmode);
	  temp = legitimize_pic_address (XEXP (XEXP (operands[1], 0), 0),
					 mode, temp);
	  operands[1] = expand_binop (mode, add_optab, temp,
				      XEXP (XEXP (operands[1], 0), 1),
				      no_new_pseudos ? temp
				      : gen_reg_rtx (Pmode),
				      0, OPTAB_LIB_WIDEN);
	}
    }

  if (! reload_in_progress && ! reload_completed)
    {
      /* Copy the source to a register if both operands aren't registers.  */
      if (! register_operand (operands[0], mode)
	  && ! sh_register_operand (operands[1], mode))
	operands[1] = copy_to_mode_reg (mode, operands[1]);

      /* This case can happen while generating code to move the result
	 of a library call to the target.  Reject `st r0,@(rX,rY)' because
	 reload will fail to find a spill register for rX, since r0 is already
	 being used for the source.  */
      else if (GET_CODE (operands[1]) == REG && REGNO (operands[1]) == 0
	       && GET_CODE (operands[0]) == MEM
	       && GET_CODE (XEXP (operands[0], 0)) == PLUS
	       && GET_CODE (XEXP (XEXP (operands[0], 0), 1)) == REG)
	operands[1] = copy_to_mode_reg (mode, operands[1]);
    }

  return 0;
}

/* Prepare the operands for an scc instruction; make sure that the
   compare has been done.  */
rtx
prepare_scc_operands (code)
     enum rtx_code code;
{
  rtx t_reg = gen_rtx_REG (SImode, T_REG);
  enum rtx_code oldcode = code;
  enum machine_mode mode;

  /* First need a compare insn.  */
  switch (code)
    {
    case NE:
      /* It isn't possible to handle this case.  */
      abort ();
    case LT:
      code = GT;
      break;
    case LE:
      code = GE;
      break;
    case LTU:
      code = GTU;
      break;
    case LEU:
      code = GEU;
      break;
    default:
      break;
    }
  if (code != oldcode)
    {
      rtx tmp = sh_compare_op0;
      sh_compare_op0 = sh_compare_op1;
      sh_compare_op1 = tmp;
    }

  mode = GET_MODE (sh_compare_op0);
  if (mode == VOIDmode)
    mode = GET_MODE (sh_compare_op1);

  sh_compare_op0 = force_reg (mode, sh_compare_op0);
  if ((code != EQ && code != NE
       && (sh_compare_op1 != const0_rtx
	   || code == GTU  || code == GEU || code == LTU || code == LEU))
      || (mode == DImode && sh_compare_op1 != const0_rtx)
      || (TARGET_SH3E && GET_MODE_CLASS (mode) == MODE_FLOAT))
    sh_compare_op1 = force_reg (mode, sh_compare_op1);

  if (TARGET_SH4 && GET_MODE_CLASS (mode) == MODE_FLOAT)
    (mode == SFmode ? emit_sf_insn : emit_df_insn)
     (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, t_reg,
			 gen_rtx (code, SImode,
				  sh_compare_op0, sh_compare_op1)),
		gen_rtx (USE, VOIDmode, get_fpscr_rtx ()))));
  else
    emit_insn (gen_rtx (SET, VOIDmode, t_reg,
			gen_rtx (code, SImode, sh_compare_op0,
				 sh_compare_op1)));

  return t_reg;
}

/* Called from the md file, set up the operands of a compare instruction.  */

void
from_compare (operands, code)
     rtx *operands;
     int code;
{
  enum machine_mode mode = GET_MODE (sh_compare_op0);
  rtx insn;
  if (mode == VOIDmode)
    mode = GET_MODE (sh_compare_op1);
  if (code != EQ
      || mode == DImode
      || (TARGET_SH3E && GET_MODE_CLASS (mode) == MODE_FLOAT))
    {
      /* Force args into regs, since we can't use constants here.  */
      sh_compare_op0 = force_reg (mode, sh_compare_op0);
      if (sh_compare_op1 != const0_rtx
	  || code == GTU  || code == GEU
	  || (TARGET_SH3E && GET_MODE_CLASS (mode) == MODE_FLOAT))
	sh_compare_op1 = force_reg (mode, sh_compare_op1);
    }
  if (TARGET_SH3E && GET_MODE_CLASS (mode) == MODE_FLOAT && code == GE)
    {
      from_compare (operands, GT);
      insn = gen_ieee_ccmpeqsf_t (sh_compare_op0, sh_compare_op1);
    }
  else
    insn = gen_rtx_SET (VOIDmode,
			gen_rtx_REG (SImode, T_REG),
			gen_rtx (code, SImode, sh_compare_op0,
				 sh_compare_op1));
  if (TARGET_SH4 && GET_MODE_CLASS (mode) == MODE_FLOAT)
    {
      insn = gen_rtx (PARALLEL, VOIDmode,
		      gen_rtvec (2, insn,
				 gen_rtx (USE, VOIDmode, get_fpscr_rtx ())));
      (mode == SFmode ? emit_sf_insn : emit_df_insn) (insn);
    }
  else
    emit_insn (insn);
}

/* Functions to output assembly code.  */

/* Return a sequence of instructions to perform DI or DF move.

   Since the SH cannot move a DI or DF in one instruction, we have
   to take care when we see overlapping source and dest registers.  */

const char *
output_movedouble (insn, operands, mode)
     rtx insn ATTRIBUTE_UNUSED;
     rtx operands[];
     enum machine_mode mode;
{
  rtx dst = operands[0];
  rtx src = operands[1];

  if (GET_CODE (dst) == MEM
      && GET_CODE (XEXP (dst, 0)) == PRE_DEC)
    return "mov.l	%T1,%0\n\tmov.l	%1,%0";

  if (register_operand (dst, mode)
      && register_operand (src, mode))
    {
      if (REGNO (src) == MACH_REG)
	return "sts	mach,%S0\n\tsts	macl,%R0";

      /* When mov.d r1,r2 do r2->r3 then r1->r2;
         when mov.d r1,r0 do r1->r0 then r2->r1.  */

      if (REGNO (src) + 1 == REGNO (dst))
	return "mov	%T1,%T0\n\tmov	%1,%0";
      else
	return "mov	%1,%0\n\tmov	%T1,%T0";
    }
  else if (GET_CODE (src) == CONST_INT)
    {
      if (INTVAL (src) < 0)
	output_asm_insn ("mov	#-1,%S0", operands);
      else
	output_asm_insn ("mov	#0,%S0", operands);

      return "mov	%1,%R0";
    }
  else if (GET_CODE (src) == MEM)
    {
      int ptrreg = -1;
      int dreg = REGNO (dst);
      rtx inside = XEXP (src, 0);

      if (GET_CODE (inside) == REG)
	ptrreg = REGNO (inside);
      else if (GET_CODE (inside) == SUBREG)
	ptrreg = subreg_regno (inside);
      else if (GET_CODE (inside) == PLUS)
	{
	  ptrreg = REGNO (XEXP (inside, 0));
	  /* ??? A r0+REG address shouldn't be possible here, because it isn't
	     an offsettable address.  Unfortunately, offsettable addresses use
	     QImode to check the offset, and a QImode offsettable address
	     requires r0 for the other operand, which is not currently
	     supported, so we can't use the 'o' constraint.
	     Thus we must check for and handle r0+REG addresses here.
	     We punt for now, since this is likely very rare.  */
	  if (GET_CODE (XEXP (inside, 1)) == REG)
	    abort ();
	}
      else if (GET_CODE (inside) == LABEL_REF)
	return "mov.l	%1,%0\n\tmov.l	%1+4,%T0";
      else if (GET_CODE (inside) == POST_INC)
	return "mov.l	%1,%0\n\tmov.l	%1,%T0";
      else
	abort ();

      /* Work out the safe way to copy.  Copy into the second half first.  */
      if (dreg == ptrreg)
	return "mov.l	%T1,%T0\n\tmov.l	%1,%0";
    }

  return "mov.l	%1,%0\n\tmov.l	%T1,%T0";
}

/* Print an instruction which would have gone into a delay slot after
   another instruction, but couldn't because the other instruction expanded
   into a sequence where putting the slot insn at the end wouldn't work.  */

static void
print_slot (insn)
     rtx insn;
{
  final_scan_insn (XVECEXP (insn, 0, 1), asm_out_file, optimize, 0, 1);

  INSN_DELETED_P (XVECEXP (insn, 0, 1)) = 1;
}

const char *
output_far_jump (insn, op)
     rtx insn;
     rtx op;
{
  struct { rtx lab, reg, op; } this;
  rtx braf_base_lab = NULL_RTX;
  const char *jump;
  int far;
  int offset = branch_dest (insn) - INSN_ADDRESSES (INSN_UID (insn));
  rtx prev;

  this.lab = gen_label_rtx ();

  if (TARGET_SH2
      && offset >= -32764
      && offset - get_attr_length (insn) <= 32766)
    {
      far = 0;
      jump = "mov.w	%O0,%1; braf	%1";
    }
  else
    {
      far = 1;
      if (flag_pic)
	{
	  if (TARGET_SH2)
	    jump = "mov.l	%O0,%1; braf	%1";
	  else
	    jump = "mov.l	r0,@-r15; mova	%O0,r0; mov.l	@r0,%1; add	r0,%1; mov.l	@r15+,r0; jmp	@%1";
	}
      else
	jump = "mov.l	%O0,%1; jmp	@%1";
    }
  /* If we have a scratch register available, use it.  */
  if (GET_CODE ((prev = prev_nonnote_insn (insn))) == INSN
      && INSN_CODE (prev) == CODE_FOR_indirect_jump_scratch)
    {
      this.reg = SET_DEST (XVECEXP (PATTERN (prev), 0, 0));
      if (REGNO (this.reg) == R0_REG && flag_pic && ! TARGET_SH2)
	jump = "mov.l	r1,@-r15; mova	%O0,r0; mov.l	@r0,r1; add	r1,r0; mov.l	@r15+,r1; jmp	@%1";
      output_asm_insn (jump, &this.lab);
      if (dbr_sequence_length ())
	print_slot (final_sequence);
      else
	output_asm_insn ("nop", 0);
    }
  else
    {
      /* Output the delay slot insn first if any.  */
      if (dbr_sequence_length ())
	print_slot (final_sequence);

      this.reg = gen_rtx_REG (SImode, 13);
      /* We must keep the stack aligned to 8-byte boundaries on SH5.
	 Fortunately, MACL is fixed and call-clobbered, and we never
	 need its value across jumps, so save r13 in it instead of in
	 the stack.  */
      if (TARGET_SH5)
	output_asm_insn ("lds	r13, macl", 0);
      else
	output_asm_insn ("mov.l	r13,@-r15", 0);
      output_asm_insn (jump, &this.lab);
      if (TARGET_SH5)
	output_asm_insn ("sts	macl, r13", 0);
      else
	output_asm_insn ("mov.l	@r15+,r13", 0);
    }
  if (far && flag_pic && TARGET_SH2)
    {
      braf_base_lab = gen_label_rtx ();
      ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "L",
				 CODE_LABEL_NUMBER (braf_base_lab));
    }
  if (far)
    output_asm_insn (".align	2", 0);
  ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "L", CODE_LABEL_NUMBER (this.lab));
  this.op = op;
  if (far && flag_pic)
    {
      if (TARGET_SH2)
	this.lab = braf_base_lab;
      output_asm_insn (".long	%O2-%O0", &this.lab);
    }
  else
    output_asm_insn (far ? ".long	%O2" : ".word %O2-%O0", &this.lab);
  return "";
}

/* Local label counter, used for constants in the pool and inside
   pattern branches.  */

static int lf = 100;

/* Output code for ordinary branches.  */

const char *
output_branch (logic, insn, operands)
     int logic;
     rtx insn;
     rtx *operands;
{
  switch (get_attr_length (insn))
    {
    case 6:
      /* This can happen if filling the delay slot has caused a forward
	 branch to exceed its range (we could reverse it, but only
	 when we know we won't overextend other branches; this should
	 best be handled by relaxation).
	 It can also happen when other condbranches hoist delay slot insn
	 from their destination, thus leading to code size increase.
	 But the branch will still be in the range -4092..+4098 bytes.  */

      if (! TARGET_RELAX)
	{
	  int label = lf++;
	  /* The call to print_slot will clobber the operands.  */
	  rtx op0 = operands[0];
    
	  /* If the instruction in the delay slot is annulled (true), then
	     there is no delay slot where we can put it now.  The only safe
	     place for it is after the label.  final will do that by default.  */
    
	  if (final_sequence
	      && ! INSN_ANNULLED_BRANCH_P (XVECEXP (final_sequence, 0, 0)))
	    {
	      asm_fprintf (asm_out_file, "\tb%s%ss\t%LLF%d\n", logic ? "f" : "t",
	                   ASSEMBLER_DIALECT ? "/" : ".", label);
	      print_slot (final_sequence);
	    }
	  else
	    asm_fprintf (asm_out_file, "\tb%s\t%LLF%d\n", logic ? "f" : "t", label);
    
	  output_asm_insn ("bra\t%l0", &op0);
	  fprintf (asm_out_file, "\tnop\n");
	  ASM_OUTPUT_INTERNAL_LABEL(asm_out_file, "LF", label);
    
	  return "";
	}
      /* When relaxing, handle this like a short branch.  The linker
	 will fix it up if it still doesn't fit after relaxation.  */
    case 2:
      return logic ? "bt%.\t%l0" : "bf%.\t%l0";
    default:
      /* There should be no longer branches now - that would
	 indicate that something has destroyed the branches set
	 up in machine_dependent_reorg.  */
      abort ();
    }
}

const char *
output_branchy_insn (code, template, insn, operands)
     enum rtx_code code;
     const char *template;
     rtx insn;
     rtx *operands;
{
  rtx next_insn = NEXT_INSN (insn);

  if (next_insn && GET_CODE (next_insn) == JUMP_INSN && condjump_p (next_insn))
    {
      rtx src = SET_SRC (PATTERN (next_insn));
      if (GET_CODE (src) == IF_THEN_ELSE && GET_CODE (XEXP (src, 0)) != code)
	{
	  /* Following branch not taken */
	  operands[9] = gen_label_rtx ();
	  emit_label_after (operands[9], next_insn);
	  INSN_ADDRESSES_NEW (operands[9],
			      INSN_ADDRESSES (INSN_UID (next_insn))
			      + get_attr_length (next_insn));
	  return template;
	}
      else
	{
	  int offset = (branch_dest (next_insn)
			- INSN_ADDRESSES (INSN_UID (next_insn)) + 4);
	  if (offset >= -252 && offset <= 258)
	    {
	      if (GET_CODE (src) == IF_THEN_ELSE)
		/* branch_true */
		src = XEXP (src, 1);
	      operands[9] = src;
	      return template;
	    }
	}
    }
  operands[9] = gen_label_rtx ();
  emit_label_after (operands[9], insn);
  INSN_ADDRESSES_NEW (operands[9],
		      INSN_ADDRESSES (INSN_UID (insn))
		      + get_attr_length (insn));
  return template;
}

const char *
output_ieee_ccmpeq (insn, operands)
     rtx insn, *operands;
{
  return output_branchy_insn (NE, "bt\t%l9\\;fcmp/eq\t%1,%0", insn, operands);
}

/* Output to FILE the start of the assembler file.  */

void
output_file_start (file)
     FILE *file;
{
  output_file_directive (file, main_input_filename);

  /* Switch to the data section so that the coffsem symbol
     isn't in the text section.  */
  data_section ();

  if (TARGET_LITTLE_ENDIAN)
    fprintf (file, "\t.little\n");

  if (TARGET_SHCOMPACT)
    fprintf (file, "\t.mode\tSHcompact\n");
  else if (TARGET_SHMEDIA)
    fprintf (file, "\t.mode\tSHmedia\n\t.abi\t%i\n",
	     TARGET_SHMEDIA64 ? 64 : 32);
}

/* Actual number of instructions used to make a shift by N.  */
static const char ashiftrt_insns[] =
  { 0,1,2,3,4,5,8,8,8,8,8,8,8,8,8,8,2,3,4,5,8,8,8,8,8,8,8,8,8,8,8,2};

/* Left shift and logical right shift are the same.  */
static const char shift_insns[]    =
  { 0,1,1,2,2,3,3,4,1,2,2,3,3,4,3,3,1,2,2,3,3,4,3,3,2,3,3,4,4,4,3,3};

/* Individual shift amounts needed to get the above length sequences.
   One bit right shifts clobber the T bit, so when possible, put one bit
   shifts in the middle of the sequence, so the ends are eligible for
   branch delay slots.  */
static const short shift_amounts[32][5] = {
  {0}, {1}, {2}, {2, 1},
  {2, 2}, {2, 1, 2}, {2, 2, 2}, {2, 2, 1, 2},
  {8}, {8, 1}, {8, 2}, {8, 1, 2},
  {8, 2, 2}, {8, 2, 1, 2}, {8, -2, 8}, {8, -1, 8},
  {16}, {16, 1}, {16, 2}, {16, 1, 2},
  {16, 2, 2}, {16, 2, 1, 2}, {16, -2, 8}, {16, -1, 8},
  {16, 8}, {16, 1, 8}, {16, 8, 2}, {16, 8, 1, 2},
  {16, 8, 2, 2}, {16, -1, -2, 16}, {16, -2, 16}, {16, -1, 16}};

/* Likewise, but for shift amounts < 16, up to three highmost bits
   might be clobbered.  This is typically used when combined with some
   kind of sign or zero extension.  */
   
static const char ext_shift_insns[]    =
  { 0,1,1,2,2,3,2,2,1,2,2,3,3,3,2,2,1,2,2,3,3,4,3,3,2,3,3,4,4,4,3,3};

static const short ext_shift_amounts[32][4] = {
  {0}, {1}, {2}, {2, 1},
  {2, 2}, {2, 1, 2}, {8, -2}, {8, -1},
  {8}, {8, 1}, {8, 2}, {8, 1, 2},
  {8, 2, 2}, {16, -2, -1}, {16, -2}, {16, -1},
  {16}, {16, 1}, {16, 2}, {16, 1, 2},
  {16, 2, 2}, {16, 2, 1, 2}, {16, -2, 8}, {16, -1, 8},
  {16, 8}, {16, 1, 8}, {16, 8, 2}, {16, 8, 1, 2},
  {16, 8, 2, 2}, {16, -1, -2, 16}, {16, -2, 16}, {16, -1, 16}};

/* Assuming we have a value that has been sign-extended by at least one bit,
   can we use the ext_shift_amounts with the last shift turned to an arithmetic shift
   to shift it by N without data loss, and quicker than by other means?  */
#define EXT_SHIFT_SIGNED(n) (((n) | 8) == 15)

/* This is used in length attributes in sh.md to help compute the length
   of arbitrary constant shift instructions.  */

int
shift_insns_rtx (insn)
     rtx insn;
{
  rtx set_src = SET_SRC (XVECEXP (PATTERN (insn), 0, 0));
  int shift_count = INTVAL (XEXP (set_src, 1));
  enum rtx_code shift_code = GET_CODE (set_src);

  switch (shift_code)
    {
    case ASHIFTRT:
      return ashiftrt_insns[shift_count];
    case LSHIFTRT:
    case ASHIFT:
      return shift_insns[shift_count];
    default:
      abort();
    }
}

/* Return the cost of a shift.  */

int
shiftcosts (x)
     rtx x;
{
  int value;

  if (TARGET_SHMEDIA)
    return 1;

  if (GET_MODE_SIZE (GET_MODE (x)) > UNITS_PER_WORD)
    {
      if (GET_MODE (x) == DImode
	  && GET_CODE (XEXP (x, 1)) == CONST_INT
	  && INTVAL (XEXP (x, 1)) == 1)
	return 2;

      /* Everything else is invalid, because there is no pattern for it.  */
      return 10000;
    }
  /* If shift by a non constant, then this will be expensive.  */
  if (GET_CODE (XEXP (x, 1)) != CONST_INT)
    return SH_DYNAMIC_SHIFT_COST;

  value = INTVAL (XEXP (x, 1));

  /* Otherwise, return the true cost in instructions.  */
  if (GET_CODE (x) == ASHIFTRT)
    {
      int cost = ashiftrt_insns[value];
      /* If SH3, then we put the constant in a reg and use shad.  */
      if (cost > 1 + SH_DYNAMIC_SHIFT_COST)
	cost = 1 + SH_DYNAMIC_SHIFT_COST;
      return cost;
    }
  else
    return shift_insns[value];
}

/* Return the cost of an AND operation.  */

int
andcosts (x)
     rtx x;
{
  int i;

  /* Anding with a register is a single cycle and instruction.  */
  if (GET_CODE (XEXP (x, 1)) != CONST_INT)
    return 1;

  i = INTVAL (XEXP (x, 1));

  if (TARGET_SHMEDIA)
    {
      if ((GET_CODE (XEXP (x, 1)) == CONST_INT
	   && CONST_OK_FOR_J (INTVAL (XEXP (x, 1))))
	  || EXTRA_CONSTRAINT_S (XEXP (x, 1)))
	return 1;
      else
	return 2;
    }

  /* These constants are single cycle extu.[bw] instructions.  */
  if (i == 0xff || i == 0xffff)
    return 1;
  /* Constants that can be used in an and immediate instruction is a single
     cycle, but this requires r0, so make it a little more expensive.  */
  if (CONST_OK_FOR_L (i))
    return 2;
  /* Constants that can be loaded with a mov immediate and an and.
     This case is probably unnecessary.  */
  if (CONST_OK_FOR_I (i))
    return 2;
  /* Any other constants requires a 2 cycle pc-relative load plus an and.
     This case is probably unnecessary.  */
  return 3;
}

/* Return the cost of an addition or a subtraction.  */

int
addsubcosts (x)
     rtx x;
{
  /* Adding a register is a single cycle insn.  */
  if (GET_CODE (XEXP (x, 1)) == REG
      || GET_CODE (XEXP (x, 1)) == SUBREG)
    return 1;

  /* Likewise for small constants.  */
  if (GET_CODE (XEXP (x, 1)) == CONST_INT
      && CONST_OK_FOR_ADD (INTVAL (XEXP (x, 1))))
    return 1;

  if (TARGET_SHMEDIA)
    switch (GET_CODE (XEXP (x, 1)))
      {
      case CONST:
      case LABEL_REF:
      case SYMBOL_REF:
	return TARGET_SHMEDIA64 ? 5 : 3;

      case CONST_INT:
	if (CONST_OK_FOR_J (INTVAL (XEXP (x, 1))))
          return 2;
	else if (CONST_OK_FOR_J (INTVAL (XEXP (x, 1)) >> 16))
	  return 3;
	else if (CONST_OK_FOR_J ((INTVAL (XEXP (x, 1)) >> 16) >> 16))
	  return 4;

	/* Fall through.  */
      default:
	  return 5;
      }

  /* Any other constant requires a 2 cycle pc-relative load plus an
     addition.  */
  return 3;
}

/* Return the cost of a multiply.  */
int
multcosts (x)
     rtx x ATTRIBUTE_UNUSED;
{
  if (TARGET_SHMEDIA)
    return 3;

  if (TARGET_SH2)
    {
      /* We have a mul insn, so we can never take more than the mul and the
	 read of the mac reg, but count more because of the latency and extra
	 reg usage.  */
      if (TARGET_SMALLCODE)
	return 2;
      return 3;
    }

  /* If we're aiming at small code, then just count the number of
     insns in a multiply call sequence.  */
  if (TARGET_SMALLCODE)
    return 5;

  /* Otherwise count all the insns in the routine we'd be calling too.  */
  return 20;
}

/* Code to expand a shift.  */

void
gen_ashift (type, n, reg)
     int type;
     int n;
     rtx reg;
{
  /* Negative values here come from the shift_amounts array.  */
  if (n < 0)
    {
      if (type == ASHIFT)
	type = LSHIFTRT;
      else
	type = ASHIFT;
      n = -n;
    }

  switch (type)
    {
    case ASHIFTRT:
      emit_insn (gen_ashrsi3_k (reg, reg, GEN_INT (n)));
      break;
    case LSHIFTRT:
      if (n == 1)
	emit_insn (gen_lshrsi3_m (reg, reg, GEN_INT (n)));
      else
	emit_insn (gen_lshrsi3_k (reg, reg, GEN_INT (n)));
      break;
    case ASHIFT:
      emit_insn (gen_ashlsi3_std (reg, reg, GEN_INT (n)));
      break;
    }
}

/* Same for HImode */

void
gen_ashift_hi (type, n, reg)
     int type;
     int n;
     rtx reg;
{
  /* Negative values here come from the shift_amounts array.  */
  if (n < 0)
    {
      if (type == ASHIFT)
	type = LSHIFTRT;
      else
	type = ASHIFT;
      n = -n;
    }

  switch (type)
    {
    case ASHIFTRT:
    case LSHIFTRT:
      /* We don't have HImode right shift operations because using the
	 ordinary 32 bit shift instructions for that doesn't generate proper
	 zero/sign extension.
	 gen_ashift_hi is only called in contexts where we know that the
	 sign extension works out correctly.  */
      {
	int offset = 0;
	if (GET_CODE (reg) == SUBREG)
	  {
	    offset = SUBREG_BYTE (reg);
	    reg = SUBREG_REG (reg);
	  }
	gen_ashift (type, n, gen_rtx_SUBREG (SImode, reg, offset));
	break;
      }
    case ASHIFT:
      emit_insn (gen_ashlhi3_k (reg, reg, GEN_INT (n)));
      break;
    }
}

/* Output RTL to split a constant shift into its component SH constant
   shift instructions.  */
   
void
gen_shifty_op (code, operands)
     int code;
     rtx *operands;
{
  int value = INTVAL (operands[2]);
  int max, i;

  /* Truncate the shift count in case it is out of bounds.  */
  value = value & 0x1f;
 
  if (value == 31)
    {
      if (code == LSHIFTRT)
	{
	  emit_insn (gen_rotlsi3_1 (operands[0], operands[0]));
	  emit_insn (gen_movt (operands[0]));
	  return;
	}
      else if (code == ASHIFT)
	{
	  /* There is a two instruction sequence for 31 bit left shifts,
	     but it requires r0.  */
	  if (GET_CODE (operands[0]) == REG && REGNO (operands[0]) == 0)
	    {
	      emit_insn (gen_andsi3 (operands[0], operands[0], const1_rtx));
	      emit_insn (gen_rotlsi3_31 (operands[0], operands[0]));
	      return;
	    }
	}
    }
  else if (value == 0)
    {
      /* This can happen when not optimizing.  We must output something here
	 to prevent the compiler from aborting in final.c after the try_split
	 call.  */
      emit_insn (gen_nop ());
      return;
    }

  max = shift_insns[value];
  for (i = 0; i < max; i++)
    gen_ashift (code, shift_amounts[value][i], operands[0]);
}
   
/* Same as above, but optimized for values where the topmost bits don't
   matter.  */

void
gen_shifty_hi_op (code, operands)
     int code;
     rtx *operands;
{
  int value = INTVAL (operands[2]);
  int max, i;
  void (*gen_fun) PARAMS ((int, int, rtx));

  /* This operation is used by and_shl for SImode values with a few
     high bits known to be cleared.  */
  value &= 31;
  if (value == 0)
    {
      emit_insn (gen_nop ());
      return;
    }

  gen_fun = GET_MODE (operands[0]) == HImode ? gen_ashift_hi : gen_ashift;
  if (code == ASHIFT)
    {
      max = ext_shift_insns[value];
      for (i = 0; i < max; i++)
	gen_fun (code, ext_shift_amounts[value][i], operands[0]);
    }
  else
    /* When shifting right, emit the shifts in reverse order, so that
       solitary negative values come first.  */
    for (i = ext_shift_insns[value] - 1; i >= 0; i--)
      gen_fun (code, ext_shift_amounts[value][i], operands[0]);
}

/* Output RTL for an arithmetic right shift.  */

/* ??? Rewrite to use super-optimizer sequences.  */

int
expand_ashiftrt (operands)
     rtx *operands;
{
  rtx sym;
  rtx wrk;
  char func[18];
  tree func_name;
  int value;

  if (TARGET_SH3)
    {
      if (GET_CODE (operands[2]) != CONST_INT)
	{
	  rtx count = copy_to_mode_reg (SImode, operands[2]);
	  emit_insn (gen_negsi2 (count, count));
	  emit_insn (gen_ashrsi3_d (operands[0], operands[1], count));
	  return 1;
	}
      else if (ashiftrt_insns[INTVAL (operands[2]) & 31]
	       > 1 + SH_DYNAMIC_SHIFT_COST)
	{
	  rtx count
	    = force_reg (SImode, GEN_INT (- (INTVAL (operands[2]) & 31)));
	  emit_insn (gen_ashrsi3_d (operands[0], operands[1], count));
	  return 1;
	}
    }
  if (GET_CODE (operands[2]) != CONST_INT)
    return 0;

  value = INTVAL (operands[2]) & 31;

  if (value == 31)
    {
      emit_insn (gen_ashrsi2_31 (operands[0], operands[1]));
      return 1;
    }
  else if (value >= 16 && value <= 19)
    {
      wrk = gen_reg_rtx (SImode);
      emit_insn (gen_ashrsi2_16 (wrk, operands[1]));
      value -= 16;
      while (value--)
	gen_ashift (ASHIFTRT, 1, wrk);
      emit_move_insn (operands[0], wrk);
      return 1;
    }
  /* Expand a short sequence inline, longer call a magic routine.  */
  else if (value <= 5)
    {
      wrk = gen_reg_rtx (SImode);
      emit_move_insn (wrk, operands[1]);
      while (value--)
	gen_ashift (ASHIFTRT, 1, wrk);
      emit_move_insn (operands[0], wrk);
      return 1;
    }

  wrk = gen_reg_rtx (Pmode);

  /* Load the value into an arg reg and call a helper.  */
  emit_move_insn (gen_rtx_REG (SImode, 4), operands[1]);
  sprintf (func, "__ashiftrt_r4_%d", value);
  func_name = get_identifier (func);
  sym = gen_rtx_SYMBOL_REF (Pmode, IDENTIFIER_POINTER (func_name));
  emit_move_insn (wrk, sym);
  emit_insn (gen_ashrsi3_n (GEN_INT (value), wrk));
  emit_move_insn (operands[0], gen_rtx_REG (SImode, 4));
  return 1;
}

int
sh_dynamicalize_shift_p (count)
     rtx count;
{
  return shift_insns[INTVAL (count)] > 1 + SH_DYNAMIC_SHIFT_COST;
}

/* Try to find a good way to implement the combiner pattern
  [(set (match_operand:SI 0 "register_operand" "r")
        (and:SI (ashift:SI (match_operand:SI 1 "register_operand" "r")
                           (match_operand:SI 2 "const_int_operand" "n"))
                (match_operand:SI 3 "const_int_operand" "n"))) .
  LEFT_RTX is operand 2 in the above pattern, and MASK_RTX is operand 3.
  return 0 for simple right / left or left/right shift combination.
  return 1 for a combination of shifts with zero_extend.
  return 2 for a combination of shifts with an AND that needs r0.
  return 3 for a combination of shifts with an AND that needs an extra
    scratch register, when the three highmost bits of the AND mask are clear.
  return 4 for a combination of shifts with an AND that needs an extra
    scratch register, when any of the three highmost bits of the AND mask
    is set.
  If ATTRP is set, store an initial right shift width in ATTRP[0],
  and the instruction length in ATTRP[1] .  These values are not valid
  when returning 0.
  When ATTRP is set and returning 1, ATTRP[2] gets set to the index into
  shift_amounts for the last shift value that is to be used before the
  sign extend.  */
int
shl_and_kind (left_rtx, mask_rtx, attrp)
     rtx left_rtx, mask_rtx;
     int *attrp;
{
  unsigned HOST_WIDE_INT mask, lsb, mask2, lsb2;
  int left = INTVAL (left_rtx), right;
  int best = 0;
  int cost, best_cost = 10000;
  int best_right = 0, best_len = 0;
  int i;
  int can_ext;

  if (left < 0 || left > 31)
    return 0;
  if (GET_CODE (mask_rtx) == CONST_INT)
    mask = (unsigned HOST_WIDE_INT) INTVAL (mask_rtx) >> left;
  else
    mask = (unsigned HOST_WIDE_INT) GET_MODE_MASK (SImode) >> left;
  /* Can this be expressed as a right shift / left shift pair ? */
  lsb = ((mask ^ (mask - 1)) >> 1) + 1;
  right = exact_log2 (lsb);
  mask2 = ~(mask + lsb - 1);
  lsb2 = ((mask2 ^ (mask2 - 1)) >> 1) + 1;
  /* mask has no zeroes but trailing zeroes <==> ! mask2 */
  if (! mask2)
    best_cost = shift_insns[right] + shift_insns[right + left];
  /* mask has no trailing zeroes <==> ! right */
  else if (! right && mask2 == ~(lsb2 - 1))
    {
      int late_right = exact_log2 (lsb2);
      best_cost = shift_insns[left + late_right] + shift_insns[late_right];
    }
  /* Try to use zero extend */
  if (mask2 == ~(lsb2 - 1))
    {
      int width, first;

      for (width = 8; width <= 16; width += 8)
	{
	  /* Can we zero-extend right away? */
	  if (lsb2 == (unsigned HOST_WIDE_INT)1 << width)
	    {
	      cost
		= 1 + ext_shift_insns[right] + ext_shift_insns[left + right];
	      if (cost < best_cost)
		{
		  best = 1;
		  best_cost = cost;
		  best_right = right;
		  best_len = cost;
		  if (attrp)
		    attrp[2] = -1;
		}
	      continue;
	    }
	  /* ??? Could try to put zero extend into initial right shift,
	     or even shift a bit left before the right shift.  */
	  /* Determine value of first part of left shift, to get to the
	     zero extend cut-off point.  */
	  first = width - exact_log2 (lsb2) + right;
	  if (first >= 0 && right + left - first >= 0)
	    {
	      cost = ext_shift_insns[right] + ext_shift_insns[first] + 1
		+ ext_shift_insns[right + left - first];
	      if (cost < best_cost)
		{
		  best = 1;
		  best_cost = cost;
		  best_right = right;
		  best_len = cost;
		  if (attrp)
		    attrp[2] = first;
		  }
	    }
	}
    }
  /* Try to use r0 AND pattern */
  for (i = 0; i <= 2; i++)
    {
      if (i > right)
	break;
      if (! CONST_OK_FOR_L (mask >> i))
	continue;
      cost = (i != 0) + 2 + ext_shift_insns[left + i];
      if (cost < best_cost)
	{
	  best = 2;
	  best_cost = cost;
	  best_right = i;
	  best_len = cost - 1;
	}
    }
  /* Try to use a scratch register to hold the AND operand.  */
  can_ext = ((mask << left) & ((unsigned HOST_WIDE_INT)3 << 30)) == 0;
  for (i = 0; i <= 2; i++)
    {
      if (i > right)
	break;
      cost = (i != 0) + (CONST_OK_FOR_I (mask >> i) ? 2 : 3)
	+ (can_ext ? ext_shift_insns : shift_insns)[left + i];
      if (cost < best_cost)
	{
	  best = 4 - can_ext;
	  best_cost = cost;
	  best_right = i;
	  best_len = cost - 1 - ! CONST_OK_FOR_I (mask >> i);
	}
    }

  if (attrp)
    {
      attrp[0] = best_right;
      attrp[1] = best_len;
    }
  return best;
}

/* This is used in length attributes of the unnamed instructions
   corresponding to shl_and_kind return values of 1 and 2.  */
int
shl_and_length (insn)
     rtx insn;
{
  rtx set_src, left_rtx, mask_rtx;
  int attributes[3];

  set_src = SET_SRC (XVECEXP (PATTERN (insn), 0, 0));
  left_rtx = XEXP (XEXP (set_src, 0), 1);
  mask_rtx = XEXP (set_src, 1);
  shl_and_kind (left_rtx, mask_rtx, attributes);
  return attributes[1];
}

/* This is used in length attribute of the and_shl_scratch instruction.  */

int
shl_and_scr_length (insn)
     rtx insn;
{
  rtx set_src = SET_SRC (XVECEXP (PATTERN (insn), 0, 0));
  int len = shift_insns[INTVAL (XEXP (set_src, 1))];
  rtx op = XEXP (set_src, 0);
  len += shift_insns[INTVAL (XEXP (op, 1))] + 1;
  op = XEXP (XEXP (op, 0), 0);
  return len + shift_insns[INTVAL (XEXP (op, 1))];
}

/* Generating rtl? */
extern int rtx_equal_function_value_matters;

/* Generate rtl for instructions for which shl_and_kind advised a particular
   method of generating them, i.e. returned zero.  */

int
gen_shl_and (dest, left_rtx, mask_rtx, source)
     rtx dest, left_rtx, mask_rtx, source;
{
  int attributes[3];
  unsigned HOST_WIDE_INT mask;
  int kind = shl_and_kind (left_rtx, mask_rtx, attributes);
  int right, total_shift;
  void (*shift_gen_fun) PARAMS ((int, rtx*)) = gen_shifty_hi_op;

  right = attributes[0];
  total_shift = INTVAL (left_rtx) + right;
  mask = (unsigned HOST_WIDE_INT) INTVAL (mask_rtx) >> total_shift;
  switch (kind)
    {
    default:
      return -1;
    case 1:
      {
	int first = attributes[2];
	rtx operands[3];

	if (first < 0)
	  {
	    emit_insn ((mask << right) <= 0xff
		       ? gen_zero_extendqisi2(dest,
					      gen_lowpart (QImode, source))
		       : gen_zero_extendhisi2(dest,
					      gen_lowpart (HImode, source)));
	    source = dest;
	  }
	if (source != dest)
	  emit_insn (gen_movsi (dest, source));
	operands[0] = dest;
	if (right)
	  {
	    operands[2] = GEN_INT (right);
	    gen_shifty_hi_op (LSHIFTRT, operands);
	  }
	if (first > 0)
	  {
	    operands[2] = GEN_INT (first);
	    gen_shifty_hi_op (ASHIFT, operands);
	    total_shift -= first;
	    mask <<= first;
	  }
	if (first >= 0)
	  emit_insn (mask <= 0xff
		     ? gen_zero_extendqisi2(dest, gen_lowpart (QImode, dest))
		     : gen_zero_extendhisi2(dest, gen_lowpart (HImode, dest)));
	if (total_shift > 0)
	  {
	    operands[2] = GEN_INT (total_shift);
	    gen_shifty_hi_op (ASHIFT, operands);
	  }
	break;
      }
    case 4:
      shift_gen_fun = gen_shifty_op;
    case 3:
      /* If the topmost bit that matters is set, set the topmost bits
	 that don't matter.  This way, we might be able to get a shorter
	 signed constant.  */
      if (mask & ((HOST_WIDE_INT)1 << (31 - total_shift)))
	mask |= (HOST_WIDE_INT)~0 << (31 - total_shift);
    case 2:
      /* Don't expand fine-grained when combining, because that will
         make the pattern fail.  */
      if (rtx_equal_function_value_matters
	  || reload_in_progress || reload_completed)
	{
	  rtx operands[3];
  
	  /* Cases 3 and 4 should be handled by this split
	     only while combining  */
	  if (kind > 2)
	    abort ();
	  if (right)
	    {
	      emit_insn (gen_lshrsi3 (dest, source, GEN_INT (right)));
	      source = dest;
	    }
	  emit_insn (gen_andsi3 (dest, source, GEN_INT (mask)));
	  if (total_shift)
	    {
	      operands[0] = dest;
	      operands[1] = dest;
	      operands[2] = GEN_INT (total_shift);
	      shift_gen_fun (ASHIFT, operands);
	    }
	  break;
	}
      else
	{
	  int neg = 0;
	  if (kind != 4 && total_shift < 16)
	    {
	      neg = -ext_shift_amounts[total_shift][1];
	      if (neg > 0)
		neg -= ext_shift_amounts[total_shift][2];
	      else
		neg = 0;
	    }
	  emit_insn (gen_and_shl_scratch (dest, source,
					  GEN_INT (right),
					  GEN_INT (mask),
					  GEN_INT (total_shift + neg),
					  GEN_INT (neg)));
	  emit_insn (gen_movsi (dest, dest));
	  break;
	}
    }
  return 0;
}

/* Try to find a good way to implement the combiner pattern
  [(set (match_operand:SI 0 "register_operand" "=r")
        (sign_extract:SI (ashift:SI (match_operand:SI 1 "register_operand" "r")
                                    (match_operand:SI 2 "const_int_operand" "n")
                         (match_operand:SI 3 "const_int_operand" "n")
                         (const_int 0)))
   (clobber (reg:SI T_REG))]
  LEFT_RTX is operand 2 in the above pattern, and SIZE_RTX is operand 3.
  return 0 for simple left / right shift combination.
  return 1 for left shift / 8 bit sign extend / left shift.
  return 2 for left shift / 16 bit sign extend / left shift.
  return 3 for left shift / 8 bit sign extend / shift / sign extend.
  return 4 for left shift / 16 bit sign extend / shift / sign extend.
  return 5 for left shift / 16 bit sign extend / right shift
  return 6 for < 8 bit sign extend / left shift.
  return 7 for < 8 bit sign extend / left shift / single right shift.
  If COSTP is nonzero, assign the calculated cost to *COSTP.  */

int
shl_sext_kind (left_rtx, size_rtx, costp)
     rtx left_rtx, size_rtx;
     int *costp;
{
  int left, size, insize, ext;
  int cost, best_cost;
  int kind;

  left = INTVAL (left_rtx);
  size = INTVAL (size_rtx);
  insize = size - left;
  if (insize <= 0)
    abort ();
  /* Default to left / right shift.  */
  kind = 0;
  best_cost = shift_insns[32 - insize] + ashiftrt_insns[32 - size];
  if (size <= 16)
    {
      /* 16 bit shift / sign extend / 16 bit shift */
      cost = shift_insns[16 - insize] + 1 + ashiftrt_insns[16 - size];
      /* If ashiftrt_insns[16 - size] is 8, this choice will be overridden
	 below, by alternative 3 or something even better.  */
      if (cost < best_cost)
	{
	  kind = 5;
	  best_cost = cost;
	}
    }
  /* Try a plain sign extend between two shifts.  */
  for (ext = 16; ext >= insize; ext -= 8)
    {
      if (ext <= size)
	{
	  cost = ext_shift_insns[ext - insize] + 1 + shift_insns[size - ext];
	  if (cost < best_cost)
	    {
	      kind = ext / (unsigned) 8;
	      best_cost = cost;
	    }
	}
      /* Check if we can do a sloppy shift with a final signed shift
	 restoring the sign.  */
      if (EXT_SHIFT_SIGNED (size - ext))
	cost = ext_shift_insns[ext - insize] + ext_shift_insns[size - ext] + 1;
      /* If not, maybe it's still cheaper to do the second shift sloppy,
	 and do a final sign extend?  */
      else if (size <= 16)
	cost = ext_shift_insns[ext - insize] + 1
	  + ext_shift_insns[size > ext ? size - ext : ext - size] + 1;
      else
	continue;
      if (cost < best_cost)
	{
	  kind = ext / (unsigned) 8 + 2;
	  best_cost = cost;
	}
    }
  /* Check if we can sign extend in r0 */
  if (insize < 8)
    {
      cost = 3 + shift_insns[left];
      if (cost < best_cost)
	{
	  kind = 6;
	  best_cost = cost;
	}
      /* Try the same with a final signed shift.  */
      if (left < 31)
	{
	  cost = 3 + ext_shift_insns[left + 1] + 1;
	  if (cost < best_cost)
	    {
	      kind = 7;
	      best_cost = cost;
	    }
	}
    }
  if (TARGET_SH3)
    {
      /* Try to use a dynamic shift.  */
      cost = shift_insns[32 - insize] + 1 + SH_DYNAMIC_SHIFT_COST;
      if (cost < best_cost)
	{
	  kind = 0;
	  best_cost = cost;
	}
    }
  if (costp)
    *costp = cost;
  return kind;
}

/* Function to be used in the length attribute of the instructions
   implementing this pattern.  */

int
shl_sext_length (insn)
     rtx insn;
{
  rtx set_src, left_rtx, size_rtx;
  int cost;

  set_src = SET_SRC (XVECEXP (PATTERN (insn), 0, 0));
  left_rtx = XEXP (XEXP (set_src, 0), 1);
  size_rtx = XEXP (set_src, 1);
  shl_sext_kind (left_rtx, size_rtx, &cost);
  return cost;
}

/* Generate rtl for this pattern */

int
gen_shl_sext (dest, left_rtx, size_rtx, source)
     rtx dest, left_rtx, size_rtx, source;
{
  int kind;
  int left, size, insize, cost;
  rtx operands[3];

  kind = shl_sext_kind (left_rtx, size_rtx, &cost);
  left = INTVAL (left_rtx);
  size = INTVAL (size_rtx);
  insize = size - left;
  switch (kind)
    {
    case 1:
    case 2:
    case 3:
    case 4:
      {
	int ext = kind & 1 ? 8 : 16;
	int shift2 = size - ext;

	/* Don't expand fine-grained when combining, because that will
	   make the pattern fail.  */
	if (! rtx_equal_function_value_matters
	    && ! reload_in_progress && ! reload_completed)
	  {
	    emit_insn (gen_shl_sext_ext (dest, source, left_rtx, size_rtx));
	    emit_insn (gen_movsi (dest, source));
	    break;
	  }
	if (dest != source)
	  emit_insn (gen_movsi (dest, source));
	operands[0] = dest;
	if (ext - insize)
	  {
	    operands[2] = GEN_INT (ext - insize);
	    gen_shifty_hi_op (ASHIFT, operands);
	  }
	emit_insn (kind & 1
		   ? gen_extendqisi2(dest, gen_lowpart (QImode, dest))
		   : gen_extendhisi2(dest, gen_lowpart (HImode, dest)));
	if (kind <= 2)
	  {
	    if (shift2)
	      {
		operands[2] = GEN_INT (shift2);
		gen_shifty_op (ASHIFT, operands);
	      }
	  }
	else
	  {
	    if (shift2 > 0)
	      {
		if (EXT_SHIFT_SIGNED (shift2))
		  {
		    operands[2] = GEN_INT (shift2 + 1);
		    gen_shifty_op (ASHIFT, operands);
		    operands[2] = GEN_INT (1);
		    gen_shifty_op (ASHIFTRT, operands);
		    break;
		  }
		operands[2] = GEN_INT (shift2);
		gen_shifty_hi_op (ASHIFT, operands);
	      }
	    else if (shift2)
	      {
		operands[2] = GEN_INT (-shift2);
		gen_shifty_hi_op (LSHIFTRT, operands);
	      }
	    emit_insn (size <= 8
		       ? gen_extendqisi2 (dest, gen_lowpart (QImode, dest))
		       : gen_extendhisi2 (dest, gen_lowpart (HImode, dest)));
	  }
	break;
      }
    case 5:
      {
	int i = 16 - size;
	if (! rtx_equal_function_value_matters
	    && ! reload_in_progress && ! reload_completed)
	  emit_insn (gen_shl_sext_ext (dest, source, left_rtx, size_rtx));
	else
	  {
	    operands[0] = dest;
	    operands[2] = GEN_INT (16 - insize);
	    gen_shifty_hi_op (ASHIFT, operands);
	    emit_insn (gen_extendhisi2 (dest, gen_lowpart (HImode, dest)));
	  }
	/* Don't use gen_ashrsi3 because it generates new pseudos.  */
	while (--i >= 0)
	  gen_ashift (ASHIFTRT, 1, dest);
	break;
      }
    case 6:
    case 7:
      /* Don't expand fine-grained when combining, because that will
	 make the pattern fail.  */
      if (! rtx_equal_function_value_matters
	  && ! reload_in_progress && ! reload_completed)
	{
	  emit_insn (gen_shl_sext_ext (dest, source, left_rtx, size_rtx));
	  emit_insn (gen_movsi (dest, source));
	  break;
	}
      emit_insn (gen_andsi3 (dest, source, GEN_INT ((1 << insize) - 1)));
      emit_insn (gen_xorsi3 (dest, dest, GEN_INT (1 << (insize - 1))));
      emit_insn (gen_addsi3 (dest, dest, GEN_INT (-1 << (insize - 1))));
      operands[0] = dest;
      operands[2] = kind == 7 ? GEN_INT (left + 1) : left_rtx;
      gen_shifty_op (ASHIFT, operands);
      if (kind == 7)
	emit_insn (gen_ashrsi3_k (dest, dest, GEN_INT (1)));
      break;
    default:
      return -1;
    }
  return 0;
}

/* Prefix a symbol_ref name with "datalabel".  */

rtx
gen_datalabel_ref (sym)
     rtx sym;
{
  if (GET_CODE (sym) == LABEL_REF)
    return gen_rtx_CONST (GET_MODE (sym),
			  gen_rtx_UNSPEC (GET_MODE (sym),
					  gen_rtvec (1, sym),
					  UNSPEC_DATALABEL));
    
  if (GET_CODE (sym) != SYMBOL_REF)
    abort ();

  XSTR (sym, 0) = concat (SH_DATALABEL_ENCODING, XSTR (sym, 0), NULL);

  return sym;
}


/* The SH cannot load a large constant into a register, constants have to
   come from a pc relative load.  The reference of a pc relative load
   instruction must be less than 1k infront of the instruction.  This
   means that we often have to dump a constant inside a function, and
   generate code to branch around it.

   It is important to minimize this, since the branches will slow things
   down and make things bigger.

   Worst case code looks like:

   mov.l L1,rn
   bra   L2
   nop
   align
   L1:   .long value
   L2:
   ..

   mov.l L3,rn
   bra   L4
   nop
   align
   L3:   .long value
   L4:
   ..

   We fix this by performing a scan before scheduling, which notices which
   instructions need to have their operands fetched from the constant table
   and builds the table.

   The algorithm is:

   scan, find an instruction which needs a pcrel move.  Look forward, find the
   last barrier which is within MAX_COUNT bytes of the requirement.
   If there isn't one, make one.  Process all the instructions between
   the find and the barrier.

   In the above example, we can tell that L3 is within 1k of L1, so
   the first move can be shrunk from the 3 insn+constant sequence into
   just 1 insn, and the constant moved to L3 to make:

   mov.l        L1,rn
   ..
   mov.l        L3,rn
   bra          L4
   nop
   align
   L3:.long value
   L4:.long value

   Then the second move becomes the target for the shortening process.  */

typedef struct
{
  rtx value;			/* Value in table.  */
  rtx label;			/* Label of value.  */
  rtx wend;			/* End of window.  */
  enum machine_mode mode;	/* Mode of value.  */

  /* True if this constant is accessed as part of a post-increment
     sequence.  Note that HImode constants are never accessed in this way.  */
  bool part_of_sequence_p;
} pool_node;

/* The maximum number of constants that can fit into one pool, since
   the pc relative range is 0...1020 bytes and constants are at least 4
   bytes long.  */

#define MAX_POOL_SIZE (1020/4)
static pool_node pool_vector[MAX_POOL_SIZE];
static int pool_size;
static rtx pool_window_label;
static int pool_window_last;

/* ??? If we need a constant in HImode which is the truncated value of a
   constant we need in SImode, we could combine the two entries thus saving
   two bytes.  Is this common enough to be worth the effort of implementing
   it?  */

/* ??? This stuff should be done at the same time that we shorten branches.
   As it is now, we must assume that all branches are the maximum size, and
   this causes us to almost always output constant pools sooner than
   necessary.  */

/* Add a constant to the pool and return its label.  */

static rtx
add_constant (x, mode, last_value)
     rtx x;
     enum machine_mode mode;
     rtx last_value;
{
  int i;
  rtx lab, new, ref, newref;

  /* First see if we've already got it.  */
  for (i = 0; i < pool_size; i++)
    {
      if (x->code == pool_vector[i].value->code
	  && mode == pool_vector[i].mode)
	{
	  if (x->code == CODE_LABEL)
	    {
	      if (XINT (x, 3) != XINT (pool_vector[i].value, 3))
		continue;
	    }
	  if (rtx_equal_p (x, pool_vector[i].value))
	    {
	      lab = new = 0;
	      if (! last_value
		  || ! i
		  || ! rtx_equal_p (last_value, pool_vector[i-1].value))
		{
		  new = gen_label_rtx ();
		  LABEL_REFS (new) = pool_vector[i].label;
		  pool_vector[i].label = lab = new;
		}
	      if (lab && pool_window_label)
		{
		  newref = gen_rtx_LABEL_REF (VOIDmode, pool_window_label);
		  ref = pool_vector[pool_window_last].wend;
		  LABEL_NEXTREF (newref) = ref;
		  pool_vector[pool_window_last].wend = newref;
		}
	      if (new)
		pool_window_label = new;
	      pool_window_last = i;
	      return lab;
	    }
	}
    }

  /* Need a new one.  */
  pool_vector[pool_size].value = x;
  if (last_value && rtx_equal_p (last_value, pool_vector[pool_size - 1].value))
    {
      lab = 0;
      pool_vector[pool_size - 1].part_of_sequence_p = true;
    }
  else
    lab = gen_label_rtx ();
  pool_vector[pool_size].mode = mode;
  pool_vector[pool_size].label = lab;
  pool_vector[pool_size].wend = NULL_RTX;
  pool_vector[pool_size].part_of_sequence_p = (lab == 0);
  if (lab && pool_window_label)
    {
      newref = gen_rtx_LABEL_REF (VOIDmode, pool_window_label);
      ref = pool_vector[pool_window_last].wend;
      LABEL_NEXTREF (newref) = ref;
      pool_vector[pool_window_last].wend = newref;
    }
  if (lab)
    pool_window_label = lab;
  pool_window_last = pool_size;
  pool_size++;
  return lab;
}

/* Output the literal table.  */

static void
dump_table (scan)
     rtx scan;
{
  int i;
  int need_align = 1;
  rtx lab, ref;
  int have_df = 0;

  /* Do two passes, first time dump out the HI sized constants.  */

  for (i = 0; i < pool_size; i++)
    {
      pool_node *p = &pool_vector[i];

      if (p->mode == HImode)
	{
	  if (need_align)
	    {
	      scan = emit_insn_after (gen_align_2 (), scan);
	      need_align = 0;
	    }
	  for (lab = p->label; lab; lab = LABEL_REFS (lab))
	    scan = emit_label_after (lab, scan);
	  scan = emit_insn_after (gen_consttable_2 (p->value, const0_rtx),
				  scan);
	  for (ref = p->wend; ref; ref = LABEL_NEXTREF (ref))
	    {
	      lab = XEXP (ref, 0);
	      scan = emit_insn_after (gen_consttable_window_end (lab), scan);
	    }
	}
      else if (p->mode == DFmode)
	have_df = 1;
    }

  need_align = 1;

  if (TARGET_FMOVD && TARGET_ALIGN_DOUBLE && have_df)
    {
      rtx align_insn = NULL_RTX;

      scan = emit_label_after (gen_label_rtx (), scan);
      scan = emit_insn_after (gen_align_log (GEN_INT (3)), scan);
      need_align = 0;

      for (i = 0; i < pool_size; i++)
	{
	  pool_node *p = &pool_vector[i];

	  switch (p->mode)
	    {
	    case HImode:
	      break;
	    case SImode:
	    case SFmode:
	      if (align_insn && !p->part_of_sequence_p)
		{
		  for (lab = p->label; lab; lab = LABEL_REFS (lab))
		    emit_label_before (lab, align_insn);
		  emit_insn_before (gen_consttable_4 (p->value, const0_rtx),
				    align_insn);
		  for (ref = p->wend; ref; ref = LABEL_NEXTREF (ref))
		    {
		      lab = XEXP (ref, 0);
		      emit_insn_before (gen_consttable_window_end (lab),
				       align_insn);
		    }
		  delete_insn (align_insn);
		  align_insn = NULL_RTX;
		  continue;
		}
	      else
		{
		  for (lab = p->label; lab; lab = LABEL_REFS (lab))
		    scan = emit_label_after (lab, scan);
		  scan = emit_insn_after (gen_consttable_4 (p->value,
							    const0_rtx), scan);
		  need_align = ! need_align;
		}
	      break;
	    case DFmode:
	      if (need_align)
		{
		  scan = emit_insn_after (gen_align_log (GEN_INT (3)), scan);
		  align_insn = scan;
		  need_align = 0;
		}
	    case DImode:
	      for (lab = p->label; lab; lab = LABEL_REFS (lab))
		scan = emit_label_after (lab, scan);
	      scan = emit_insn_after (gen_consttable_8 (p->value, const0_rtx),
				      scan);
	      break;
	    default:
	      abort ();
	      break;
	    }

	  if (p->mode != HImode)
	    {
	      for (ref = p->wend; ref; ref = LABEL_NEXTREF (ref))
		{
		  lab = XEXP (ref, 0);
		  scan = emit_insn_after (gen_consttable_window_end (lab),
					  scan);
		}
	    }
	}

      pool_size = 0;
    }
  
  for (i = 0; i < pool_size; i++)
    {
      pool_node *p = &pool_vector[i];

      switch (p->mode)
	{
	case HImode:
	  break;
	case SImode:
	case SFmode:
	  if (need_align)
	    {
	      need_align = 0;
	      scan = emit_label_after (gen_label_rtx (), scan);
	      scan = emit_insn_after (gen_align_4 (), scan);
	    }
	  for (lab = p->label; lab; lab = LABEL_REFS (lab))
	    scan = emit_label_after (lab, scan);
	  scan = emit_insn_after (gen_consttable_4 (p->value, const0_rtx),
				  scan);
	  break;
	case DFmode:
	case DImode:
	  if (need_align)
	    {
	      need_align = 0;
	      scan = emit_label_after (gen_label_rtx (), scan);
	      scan = emit_insn_after (gen_align_4 (), scan);
	    }
	  for (lab = p->label; lab; lab = LABEL_REFS (lab))
	    scan = emit_label_after (lab, scan);
	  scan = emit_insn_after (gen_consttable_8 (p->value, const0_rtx),
				  scan);
	  break;
	default:
	  abort ();
	  break;
	}

      if (p->mode != HImode)
	{
	  for (ref = p->wend; ref; ref = LABEL_NEXTREF (ref))
	    {
	      lab = XEXP (ref, 0);
	      scan = emit_insn_after (gen_consttable_window_end (lab), scan);
	    }
	}
    }

  scan = emit_insn_after (gen_consttable_end (), scan);
  scan = emit_barrier_after (scan);
  pool_size = 0;
  pool_window_label = NULL_RTX;
  pool_window_last = 0;
}

/* Return nonzero if constant would be an ok source for a
   mov.w instead of a mov.l.  */

static int
hi_const (src)
     rtx src;
{
  return (GET_CODE (src) == CONST_INT
	  && INTVAL (src) >= -32768
	  && INTVAL (src) <= 32767);
}

/* Nonzero if the insn is a move instruction which needs to be fixed.  */

/* ??? For a DImode/DFmode moves, we don't need to fix it if each half of the
   CONST_DOUBLE input value is CONST_OK_FOR_I.  For a SFmode move, we don't
   need to fix it if the input value is CONST_OK_FOR_I.  */

static int
broken_move (insn)
     rtx insn;
{
  if (GET_CODE (insn) == INSN)
    {
      rtx pat = PATTERN (insn);
      if (GET_CODE (pat) == PARALLEL)
	pat = XVECEXP (pat, 0, 0);
      if (GET_CODE (pat) == SET
	  /* We can load any 8 bit value if we don't care what the high
	     order bits end up as.  */
	  && GET_MODE (SET_DEST (pat)) != QImode
	  && (CONSTANT_P (SET_SRC (pat))
	      /* Match mova_const.  */
	      || (GET_CODE (SET_SRC (pat)) == UNSPEC
		  && XINT (SET_SRC (pat), 1) == UNSPEC_MOVA
		  && GET_CODE (XVECEXP (SET_SRC (pat), 0, 0)) == CONST))
	  && ! (TARGET_SH3E
		&& GET_CODE (SET_SRC (pat)) == CONST_DOUBLE
		&& (fp_zero_operand (SET_SRC (pat))
		    || fp_one_operand (SET_SRC (pat)))
		/* ??? If this is a -m4 or -m4-single compilation, in general
		   we don't know the current setting of fpscr, so disable fldi.
		   There is an exception if this was a register-register move
		   before reload - and hence it was ascertained that we have
		   single precision setting - and in a post-reload optimization
		   we changed this to do a constant load.  In that case
		   we don't have an r0 clobber, hence we must use fldi.  */
		&& (! TARGET_SH4 || TARGET_FMOVD
		    || (GET_CODE (XEXP (XVECEXP (PATTERN (insn), 0, 2), 0))
			== SCRATCH))
		&& GET_CODE (SET_DEST (pat)) == REG
		&& FP_REGISTER_P (REGNO (SET_DEST (pat))))
	  && (GET_CODE (SET_SRC (pat)) != CONST_INT
	      || ! CONST_OK_FOR_I (INTVAL (SET_SRC (pat)))))
	return 1;
    }

  return 0;
}

static int
mova_p (insn)
     rtx insn;
{
  return (GET_CODE (insn) == INSN
	  && GET_CODE (PATTERN (insn)) == SET
	  && GET_CODE (SET_SRC (PATTERN (insn))) == UNSPEC
	  && XINT (SET_SRC (PATTERN (insn)), 1) == UNSPEC_MOVA
	  /* Don't match mova_const.  */
	  && GET_CODE (XVECEXP (SET_SRC (PATTERN (insn)), 0, 0)) == LABEL_REF);
}

/* Find the last barrier from insn FROM which is close enough to hold the
   constant pool.  If we can't find one, then create one near the end of
   the range.  */

static rtx
find_barrier (num_mova, mova, from)
     int num_mova;
     rtx mova, from;
{
  int count_si = 0;
  int count_hi = 0;
  int found_hi = 0;
  int found_si = 0;
  int found_di = 0;
  int hi_align = 2;
  int si_align = 2;
  int leading_mova = num_mova;
  rtx barrier_before_mova, found_barrier = 0, good_barrier = 0;
  int si_limit;
  int hi_limit;

  /* For HImode: range is 510, add 4 because pc counts from address of
     second instruction after this one, subtract 2 for the jump instruction
     that we may need to emit before the table, subtract 2 for the instruction
     that fills the jump delay slot (in very rare cases, reorg will take an
     instruction from after the constant pool or will leave the delay slot
     empty).  This gives 510.
     For SImode: range is 1020, add 4 because pc counts from address of
     second instruction after this one, subtract 2 in case pc is 2 byte
     aligned, subtract 2 for the jump instruction that we may need to emit
     before the table, subtract 2 for the instruction that fills the jump
     delay slot.  This gives 1018.  */

  /* The branch will always be shortened now that the reference address for
     forward branches is the successor address, thus we need no longer make
     adjustments to the [sh]i_limit for -O0.  */

  si_limit = 1018;
  hi_limit = 510;

  while (from && count_si < si_limit && count_hi < hi_limit)
    {
      int inc = get_attr_length (from);
      int new_align = 1;

      if (GET_CODE (from) == CODE_LABEL)
	{
	  if (optimize)
	    new_align = 1 << label_to_alignment (from);
	  else if (GET_CODE (prev_nonnote_insn (from)) == BARRIER)
	    new_align = 1 << barrier_align (from);
	  else
	    new_align = 1;
	  inc = 0;
	}

      if (GET_CODE (from) == BARRIER)
	{

	  found_barrier = from;

	  /* If we are at the end of the function, or in front of an alignment
	     instruction, we need not insert an extra alignment.  We prefer
	     this kind of barrier.  */
	  if (barrier_align (from) > 2)
	    good_barrier = from;
	}

      if (broken_move (from))
	{
	  rtx pat, src, dst;
	  enum machine_mode mode;

	  pat = PATTERN (from);
	  if (GET_CODE (pat) == PARALLEL)
	    pat = XVECEXP (pat, 0, 0);
	  src = SET_SRC (pat);
	  dst = SET_DEST (pat);
	  mode = GET_MODE (dst);

	  /* We must explicitly check the mode, because sometimes the
	     front end will generate code to load unsigned constants into
	     HImode targets without properly sign extending them.  */
	  if (mode == HImode
	      || (mode == SImode && hi_const (src) && REGNO (dst) != FPUL_REG))
	    {
	      found_hi += 2;
	      /* We put the short constants before the long constants, so
		 we must count the length of short constants in the range
		 for the long constants.  */
	      /* ??? This isn't optimal, but is easy to do.  */
	      si_limit -= 2;
	    }
	  else
	    {
	      /* We dump DF/DI constants before SF/SI ones, because
		 the limit is the same, but the alignment requirements
		 are higher.  We may waste up to 4 additional bytes
		 for alignment, and the DF/DI constant may have
		 another SF/SI constant placed before it. */
	      if (TARGET_SHCOMPACT
		  && ! found_di
		  && (mode == DFmode || mode == DImode))
		{
		  found_di = 1;
		  si_limit -= 8;
		}
	      while (si_align > 2 && found_si + si_align - 2 > count_si)
		si_align >>= 1;
	      if (found_si > count_si)
		count_si = found_si;
	      found_si += GET_MODE_SIZE (mode);
	      if (num_mova)
		si_limit -= GET_MODE_SIZE (mode);
	    }

	  /* See the code in machine_dependent_reorg, which has a similar if
	     statement that generates a new mova insn in many cases.  */
	  if (GET_CODE (dst) == REG && FP_ANY_REGISTER_P (REGNO (dst)))
	    inc += 2;
	}

      if (mova_p (from))
	{
	  if (! num_mova++)
	    {
	      leading_mova = 0;
	      mova = from;
	      barrier_before_mova = good_barrier ? good_barrier : found_barrier;
	    }
	  if (found_si > count_si)
	    count_si = found_si;
	}
      else if (GET_CODE (from) == JUMP_INSN
	       && (GET_CODE (PATTERN (from)) == ADDR_VEC
		   || GET_CODE (PATTERN (from)) == ADDR_DIFF_VEC))
	{
	  if (num_mova)
	    num_mova--;
	  if (barrier_align (next_real_insn (from)) == align_jumps_log)
	    {
	      /* We have just passed the barrier in front of the
		 ADDR_DIFF_VEC, which is stored in found_barrier.  Since
		 the ADDR_DIFF_VEC is accessed as data, just like our pool
		 constants, this is a good opportunity to accommodate what
		 we have gathered so far.
		 If we waited any longer, we could end up at a barrier in
		 front of code, which gives worse cache usage for separated
		 instruction / data caches.  */
	      good_barrier = found_barrier;
	      break;
	    }
	  else
	    {
	      rtx body = PATTERN (from);
	      inc = XVECLEN (body, 1) * GET_MODE_SIZE (GET_MODE (body));
	    }
	}
      /* For the SH1, we generate alignments even after jumps-around-jumps.  */
      else if (GET_CODE (from) == JUMP_INSN
	       && ! TARGET_SH2
	       && ! TARGET_SMALLCODE)
	new_align = 4;

      if (found_si)
	{
	  count_si += inc;
	  if (new_align > si_align)
	    {
	      si_limit -= (count_si - 1) & (new_align - si_align);
	      si_align = new_align;
	    }
	  count_si = (count_si + new_align - 1) & -new_align;
	}
      if (found_hi)
	{
	  count_hi += inc;
	  if (new_align > hi_align)
	    {
	      hi_limit -= (count_hi - 1) & (new_align - hi_align);
	      hi_align = new_align;
	    }
	  count_hi = (count_hi + new_align - 1) & -new_align;
	}
      from = NEXT_INSN (from);
    }

  if (num_mova)
    {
      if (leading_mova)
	{
	  /* Try as we might, the leading mova is out of range.  Change
	     it into a load (which will become a pcload) and retry.  */
	  SET_SRC (PATTERN (mova)) = XVECEXP (SET_SRC (PATTERN (mova)), 0, 0);
	  INSN_CODE (mova) = -1;
	  return find_barrier (0, 0, mova);
	}
      else
	{
	  /* Insert the constant pool table before the mova instruction,
	     to prevent the mova label reference from going out of range.  */
	  from = mova;
	  good_barrier = found_barrier = barrier_before_mova;
	}
    }

  if (found_barrier)
    {
      if (good_barrier && next_real_insn (found_barrier))
	found_barrier = good_barrier;
    }
  else
    {
      /* We didn't find a barrier in time to dump our stuff,
	 so we'll make one.  */
      rtx label = gen_label_rtx ();

      /* If we exceeded the range, then we must back up over the last
	 instruction we looked at.  Otherwise, we just need to undo the
	 NEXT_INSN at the end of the loop.  */
      if (count_hi > hi_limit || count_si > si_limit)
	from = PREV_INSN (PREV_INSN (from));
      else
	from = PREV_INSN (from);

      /* Walk back to be just before any jump or label.
	 Putting it before a label reduces the number of times the branch
	 around the constant pool table will be hit.  Putting it before
	 a jump makes it more likely that the bra delay slot will be
	 filled.  */
      while (GET_CODE (from) == JUMP_INSN || GET_CODE (from) == NOTE
	     || GET_CODE (from) == CODE_LABEL)
	from = PREV_INSN (from);

      from = emit_jump_insn_after (gen_jump (label), from);
      JUMP_LABEL (from) = label;
      LABEL_NUSES (label) = 1;
      found_barrier = emit_barrier_after (from);
      emit_label_after (label, found_barrier);
    }

  return found_barrier;
}

/* If the instruction INSN is implemented by a special function, and we can
   positively find the register that is used to call the sfunc, and this
   register is not used anywhere else in this instruction - except as the
   destination of a set, return this register; else, return 0.  */
rtx
sfunc_uses_reg (insn)
     rtx insn;
{
  int i;
  rtx pattern, part, reg_part, reg;

  if (GET_CODE (insn) != INSN)
    return 0;
  pattern = PATTERN (insn);
  if (GET_CODE (pattern) != PARALLEL || get_attr_type (insn) != TYPE_SFUNC)
    return 0;

  for (reg_part = 0, i = XVECLEN (pattern, 0) - 1; i >= 1; i--)
    {
      part = XVECEXP (pattern, 0, i);
      if (GET_CODE (part) == USE && GET_MODE (XEXP (part, 0)) == SImode)
	reg_part = part;
    }
  if (! reg_part)
    return 0;
  reg = XEXP (reg_part, 0);
  for (i = XVECLEN (pattern, 0) - 1; i >= 0; i--)
    {
      part = XVECEXP (pattern, 0, i);
      if (part == reg_part || GET_CODE (part) == CLOBBER)
	continue;
      if (reg_mentioned_p (reg, ((GET_CODE (part) == SET
				  && GET_CODE (SET_DEST (part)) == REG)
				 ? SET_SRC (part) : part)))
	return 0;
    }
  return reg;
}

/* See if the only way in which INSN uses REG is by calling it, or by
   setting it while calling it.  Set *SET to a SET rtx if the register
   is set by INSN.  */

static int
noncall_uses_reg (reg, insn, set)
     rtx reg;
     rtx insn;
     rtx *set;
{
  rtx pattern, reg2;

  *set = NULL_RTX;

  reg2 = sfunc_uses_reg (insn);
  if (reg2 && REGNO (reg2) == REGNO (reg))
    {
      pattern = single_set (insn);
      if (pattern
	  && GET_CODE (SET_DEST (pattern)) == REG
	  && REGNO (reg) == REGNO (SET_DEST (pattern)))
	*set = pattern;
      return 0;
    }
  if (GET_CODE (insn) != CALL_INSN)
    {
      /* We don't use rtx_equal_p because we don't care if the mode is
	 different.  */
      pattern = single_set (insn);
      if (pattern
	  && GET_CODE (SET_DEST (pattern)) == REG
	  && REGNO (reg) == REGNO (SET_DEST (pattern)))
	{
	  rtx par, part;
	  int i;

	  *set = pattern;
	  par = PATTERN (insn);
	  if (GET_CODE (par) == PARALLEL)
	    for (i = XVECLEN (par, 0) - 1; i >= 0; i--)
	      {
		part = XVECEXP (par, 0, i);
		if (GET_CODE (part) != SET && reg_mentioned_p (reg, part))
		  return 1;
	      }
	  return reg_mentioned_p (reg, SET_SRC (pattern));
	}

      return 1;
    }

  pattern = PATTERN (insn);

  if (GET_CODE (pattern) == PARALLEL)
    {
      int i;

      for (i = XVECLEN (pattern, 0) - 1; i >= 1; i--)
	if (reg_mentioned_p (reg, XVECEXP (pattern, 0, i)))
	  return 1;
      pattern = XVECEXP (pattern, 0, 0);
    }

  if (GET_CODE (pattern) == SET)
    {
      if (reg_mentioned_p (reg, SET_DEST (pattern)))
	{
	  /* We don't use rtx_equal_p, because we don't care if the
             mode is different.  */
	  if (GET_CODE (SET_DEST (pattern)) != REG
	      || REGNO (reg) != REGNO (SET_DEST (pattern)))
	    return 1;

	  *set = pattern;
	}

      pattern = SET_SRC (pattern);
    }

  if (GET_CODE (pattern) != CALL
      || GET_CODE (XEXP (pattern, 0)) != MEM
      || ! rtx_equal_p (reg, XEXP (XEXP (pattern, 0), 0)))
    return 1;

  return 0;
}

/* Given a X, a pattern of an insn or a part of it, return a mask of used
   general registers.  Bits 0..15 mean that the respective registers
   are used as inputs in the instruction.  Bits 16..31 mean that the
   registers 0..15, respectively, are used as outputs, or are clobbered.
   IS_DEST should be set to 16 if X is the destination of a SET, else to 0.  */
int
regs_used (x, is_dest)
     rtx x; int is_dest;
{
  enum rtx_code code;
  const char *fmt;
  int i, used = 0;

  if (! x)
    return used;
  code = GET_CODE (x);
  switch (code)
    {
    case REG:
      if (REGNO (x) < 16)
	return (((1 << HARD_REGNO_NREGS (0, GET_MODE (x))) - 1)
		<< (REGNO (x) + is_dest));
      return 0;
    case SUBREG:
      {
	rtx y = SUBREG_REG (x);
     
	if (GET_CODE (y) != REG)
	  break;
	if (REGNO (y) < 16)
	  return (((1 << HARD_REGNO_NREGS (0, GET_MODE (x))) - 1)
		  << (REGNO (y) +
		      subreg_regno_offset (REGNO (y),
					   GET_MODE (y),
					   SUBREG_BYTE (x),
					   GET_MODE (x)) + is_dest));
	return 0;
      }
    case SET:
      return regs_used (SET_SRC (x), 0) | regs_used (SET_DEST (x), 16);
    case RETURN:
      /* If there was a return value, it must have been indicated with USE.  */
      return 0x00ffff00;
    case CLOBBER:
      is_dest = 1;
      break;
    case MEM:
      is_dest = 0;
      break;
    case CALL:
      used |= 0x00ff00f0;
      break;
    default:
      break;
    }

  fmt = GET_RTX_FORMAT (code);

  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'E')
	{
	  register int j;
	  for (j = XVECLEN (x, i) - 1; j >= 0; j--)
	    used |= regs_used (XVECEXP (x, i, j), is_dest);
	}
      else if (fmt[i] == 'e')
	used |= regs_used (XEXP (x, i), is_dest);
    }
  return used;
}

/* Create an instruction that prevents redirection of a conditional branch
   to the destination of the JUMP with address ADDR.
   If the branch needs to be implemented as an indirect jump, try to find
   a scratch register for it.
   If NEED_BLOCK is 0, don't do anything unless we need a scratch register.
   If any preceding insn that doesn't fit into a delay slot is good enough,
   pass 1.  Pass 2 if a definite blocking insn is needed.
   -1 is used internally to avoid deep recursion.
   If a blocking instruction is made or recognized, return it.  */
   
static rtx
gen_block_redirect (jump, addr, need_block)
     rtx jump;
     int addr, need_block;
{
  int dead = 0;
  rtx prev = prev_nonnote_insn (jump);
  rtx dest;

  /* First, check if we already have an instruction that satisfies our need.  */
  if (prev && GET_CODE (prev) == INSN && ! INSN_DELETED_P (prev))
    {
      if (INSN_CODE (prev) == CODE_FOR_indirect_jump_scratch)
	return prev;
      if (GET_CODE (PATTERN (prev)) == USE
	  || GET_CODE (PATTERN (prev)) == CLOBBER
	  || get_attr_in_delay_slot (prev) == IN_DELAY_SLOT_YES)
	prev = jump;
      else if ((need_block &= ~1) < 0)
	return prev;
      else if (recog_memoized (prev) == CODE_FOR_block_branch_redirect)
	need_block = 0;
    }
  /* We can't use JUMP_LABEL here because it might be undefined
     when not optimizing.  */
  dest = XEXP (SET_SRC (PATTERN (jump)), 0);
  /* If the branch is out of range, try to find a scratch register for it.  */
  if (optimize
      && (INSN_ADDRESSES (INSN_UID (dest)) - addr + (unsigned) 4092
	  > 4092 + 4098))
    {
      rtx scan;
      /* Don't look for the stack pointer as a scratch register,
	 it would cause trouble if an interrupt occurred.  */
      unsigned try = 0x7fff, used;
      int jump_left = flag_expensive_optimizations + 1;
    
      /* It is likely that the most recent eligible instruction is wanted for
	 the delay slot.  Therefore, find out which registers it uses, and
	 try to avoid using them.  */
	 
      for (scan = jump; (scan = PREV_INSN (scan)); )
	{
	  enum rtx_code code;

	  if (INSN_DELETED_P (scan))
	    continue;
	  code = GET_CODE (scan);
	  if (code == CODE_LABEL || code == JUMP_INSN)
	    break;
	  if (code == INSN
	      && GET_CODE (PATTERN (scan)) != USE
	      && GET_CODE (PATTERN (scan)) != CLOBBER
	      && get_attr_in_delay_slot (scan) == IN_DELAY_SLOT_YES)
	    {
	      try &= ~regs_used (PATTERN (scan), 0);
	      break;
	    }
	}
      for (used = dead = 0, scan = JUMP_LABEL (jump);
	   (scan = NEXT_INSN (scan)); )
	{
	  enum rtx_code code;

	  if (INSN_DELETED_P (scan))
	    continue;
	  code = GET_CODE (scan);
	  if (GET_RTX_CLASS (code) == 'i')
	    {
	      used |= regs_used (PATTERN (scan), 0);
	      if (code == CALL_INSN)
		used |= regs_used (CALL_INSN_FUNCTION_USAGE (scan), 0);
	      dead |= (used >> 16) & ~used;
	      if (dead & try)
		{
		  dead &= try;
		  break;
		}
	      if (code == JUMP_INSN)
		{
		  if (jump_left-- && simplejump_p (scan))
		    scan = JUMP_LABEL (scan);
		  else
		    break;
		}
	    }
	}
      /* Mask out the stack pointer again, in case it was
	 the only 'free' register we have found.  */
      dead &= 0x7fff;
    }
  /* If the immediate destination is still in range, check for possible
     threading with a jump beyond the delay slot insn.
     Don't check if we are called recursively; the jump has been or will be
     checked in a different invocation then.  */
	
  else if (optimize && need_block >= 0)
    {
      rtx next = next_active_insn (next_active_insn (dest));
      if (next && GET_CODE (next) == JUMP_INSN
	  && GET_CODE (PATTERN (next)) == SET
	  && recog_memoized (next) == CODE_FOR_jump_compact)
	{
	  dest = JUMP_LABEL (next);
	  if (dest
	      && (INSN_ADDRESSES (INSN_UID (dest)) - addr + (unsigned) 4092
		  > 4092 + 4098))
	    gen_block_redirect (next, INSN_ADDRESSES (INSN_UID (next)), -1);
	}
    }

  if (dead)
    {
      rtx reg = gen_rtx_REG (SImode, exact_log2 (dead & -dead));

      /* It would be nice if we could convert the jump into an indirect
	 jump / far branch right now, and thus exposing all constituent
	 instructions to further optimization.  However, reorg uses
	 simplejump_p to determine if there is an unconditional jump where
	 it should try to schedule instructions from the target of the
	 branch; simplejump_p fails for indirect jumps even if they have
	 a JUMP_LABEL.  */
      rtx insn = emit_insn_before (gen_indirect_jump_scratch
				   (reg, GEN_INT (INSN_UID (JUMP_LABEL (jump))))
				   , jump);
      /* ??? We would like this to have the scope of the jump, but that
	 scope will change when a delay slot insn of an inner scope is added.
	 Hence, after delay slot scheduling, we'll have to expect
	 NOTE_INSN_BLOCK_END notes between the indirect_jump_scratch and
	 the jump.  */
	 
      INSN_SCOPE (insn) = INSN_SCOPE (jump);
      INSN_CODE (insn) = CODE_FOR_indirect_jump_scratch;
      return insn;
    }
  else if (need_block)
    /* We can't use JUMP_LABEL here because it might be undefined
       when not optimizing.  */
    return emit_insn_before (gen_block_branch_redirect
		      (GEN_INT (INSN_UID (XEXP (SET_SRC (PATTERN (jump)), 0))))
		      , jump);
  return prev;
}

#define CONDJUMP_MIN -252
#define CONDJUMP_MAX 262
struct far_branch
{
  /* A label (to be placed) in front of the jump
     that jumps to our ultimate destination.  */
  rtx near_label;
  /* Where we are going to insert it if we cannot move the jump any farther,
     or the jump itself if we have picked up an existing jump.  */
  rtx insert_place;
  /* The ultimate destination.  */
  rtx far_label;
  struct far_branch *prev;
  /* If the branch has already been created, its address;
     else the address of its first prospective user.  */
  int address;
};

static void gen_far_branch PARAMS ((struct far_branch *));
enum mdep_reorg_phase_e mdep_reorg_phase;
static void
gen_far_branch (bp)
     struct far_branch *bp;
{
  rtx insn = bp->insert_place;
  rtx jump;
  rtx label = gen_label_rtx ();

  emit_label_after (label, insn);
  if (bp->far_label)
    {
      jump = emit_jump_insn_after (gen_jump (bp->far_label), insn);
      LABEL_NUSES (bp->far_label)++;
    }
  else
    jump = emit_jump_insn_after (gen_return (), insn);
  /* Emit a barrier so that reorg knows that any following instructions
     are not reachable via a fall-through path.
     But don't do this when not optimizing, since we wouldn't supress the
     alignment for the barrier then, and could end up with out-of-range
     pc-relative loads.  */
  if (optimize)
    emit_barrier_after (jump);
  emit_label_after (bp->near_label, insn);
  JUMP_LABEL (jump) = bp->far_label;
  if (! invert_jump (insn, label, 1))
    abort ();
  (emit_insn_after
   (gen_stuff_delay_slot
    (GEN_INT (INSN_UID (XEXP (SET_SRC (PATTERN (jump)), 0))),
     GEN_INT (recog_memoized (insn) == CODE_FOR_branch_false)),
    insn));
  /* Prevent reorg from undoing our splits.  */
  gen_block_redirect (jump, bp->address += 2, 2);
}

/* Fix up ADDR_DIFF_VECs.  */
void
fixup_addr_diff_vecs (first)
     rtx first;
{
  rtx insn;

  for (insn = first; insn; insn = NEXT_INSN (insn))
    {
      rtx vec_lab, pat, prev, prevpat, x, braf_label;

      if (GET_CODE (insn) != JUMP_INSN
	  || GET_CODE (PATTERN (insn)) != ADDR_DIFF_VEC)
	continue;
      pat = PATTERN (insn);
      vec_lab = XEXP (XEXP (pat, 0), 0);

      /* Search the matching casesi_jump_2.  */
      for (prev = vec_lab; ; prev = PREV_INSN (prev))
	{
	  if (GET_CODE (prev) != JUMP_INSN)
	    continue;
	  prevpat = PATTERN (prev);
	  if (GET_CODE (prevpat) != PARALLEL || XVECLEN (prevpat, 0) != 2)
	    continue;
	  x = XVECEXP (prevpat, 0, 1);
	  if (GET_CODE (x) != USE)
	    continue;
	  x = XEXP (x, 0);
	  if (GET_CODE (x) == LABEL_REF && XEXP (x, 0) == vec_lab)
	    break;
	}

      /* Emit the reference label of the braf where it belongs, right after
	 the casesi_jump_2 (i.e. braf).  */
      braf_label = XEXP (XEXP (SET_SRC (XVECEXP (prevpat, 0, 0)), 1), 0);
      emit_label_after (braf_label, prev);

      /* Fix up the ADDR_DIF_VEC to be relative
	 to the reference address of the braf.  */
      XEXP (XEXP (pat, 0), 0) = braf_label;
    }
}

/* BARRIER_OR_LABEL is either a BARRIER or a CODE_LABEL immediately following
   a barrier.  Return the base 2 logarithm of the desired alignment.  */
int
barrier_align (barrier_or_label)
     rtx barrier_or_label;
{
  rtx next = next_real_insn (barrier_or_label), pat, prev;
  int slot, credit, jump_to_next;
 
  if (! next)
    return 0;

  pat = PATTERN (next);

  if (GET_CODE (pat) == ADDR_DIFF_VEC)
    return 2;

  if (GET_CODE (pat) == UNSPEC_VOLATILE && XINT (pat, 1) == UNSPECV_ALIGN)
    /* This is a barrier in front of a constant table.  */
    return 0;

  prev = prev_real_insn (barrier_or_label);
  if (GET_CODE (PATTERN (prev)) == ADDR_DIFF_VEC)
    {
      pat = PATTERN (prev);
      /* If this is a very small table, we want to keep the alignment after
	 the table to the minimum for proper code alignment.  */
      return ((TARGET_SMALLCODE
	       || ((unsigned) XVECLEN (pat, 1) * GET_MODE_SIZE (GET_MODE (pat))
		   <= (unsigned)1 << (CACHE_LOG - 2)))
	      ? 1 << TARGET_SHMEDIA : align_jumps_log);
    }

  if (TARGET_SMALLCODE)
    return 0;

  if (! TARGET_SH2 || ! optimize)
    return align_jumps_log;

  /* When fixing up pcloads, a constant table might be inserted just before
     the basic block that ends with the barrier.  Thus, we can't trust the
     instruction lengths before that.  */
  if (mdep_reorg_phase > SH_FIXUP_PCLOAD)
    {
      /* Check if there is an immediately preceding branch to the insn beyond
	 the barrier.  We must weight the cost of discarding useful information
	 from the current cache line when executing this branch and there is
	 an alignment, against that of fetching unneeded insn in front of the
	 branch target when there is no alignment.  */

      /* There are two delay_slot cases to consider.  One is the simple case 
	 where the preceding branch is to the insn beyond the barrier (simple 
	 delay slot filling), and the other is where the preceding branch has 
	 a delay slot that is a duplicate of the insn after the barrier 
	 (fill_eager_delay_slots) and the branch is to the insn after the insn 
	 after the barrier.  */

      /* PREV is presumed to be the JUMP_INSN for the barrier under
	 investigation.  Skip to the insn before it.  */
      prev = prev_real_insn (prev);

      for (slot = 2, credit = (1 << (CACHE_LOG - 2)) + 2;
	   credit >= 0 && prev && GET_CODE (prev) == INSN;
	   prev = prev_real_insn (prev))
	{
	  jump_to_next = 0;
	  if (GET_CODE (PATTERN (prev)) == USE
	      || GET_CODE (PATTERN (prev)) == CLOBBER)
	    continue;
	  if (GET_CODE (PATTERN (prev)) == SEQUENCE)
	    {
	      prev = XVECEXP (PATTERN (prev), 0, 1);
	      if (INSN_UID (prev) == INSN_UID (next)) 
		{
	  	  /* Delay slot was filled with insn at jump target.  */
		  jump_to_next = 1;
		  continue;
  		}
	    }

	  if (slot &&
	      get_attr_in_delay_slot (prev) == IN_DELAY_SLOT_YES)
	    slot = 0;
	  credit -= get_attr_length (prev);
	}
      if (prev
	  && GET_CODE (prev) == JUMP_INSN
	  && JUMP_LABEL (prev))
	{
	  rtx x;
	  if (jump_to_next
	      || next_real_insn (JUMP_LABEL (prev)) == next
	      /* If relax_delay_slots() decides NEXT was redundant
		 with some previous instruction, it will have
		 redirected PREV's jump to the following insn.  */
	      || JUMP_LABEL (prev) == next_nonnote_insn (next)
	      /* There is no upper bound on redundant instructions
		 that might have been skipped, but we must not put an
		 alignment where none had been before.  */
	      || (x = (NEXT_INSN (NEXT_INSN (PREV_INSN (prev)))),	    
		  (INSN_P (x) 
		   && (INSN_CODE (x) == CODE_FOR_block_branch_redirect
		       || INSN_CODE (x) == CODE_FOR_indirect_jump_scratch
		       || INSN_CODE (x) == CODE_FOR_stuff_delay_slot))))
	    {
	      rtx pat = PATTERN (prev);
	      if (GET_CODE (pat) == PARALLEL)
		pat = XVECEXP (pat, 0, 0);
	      if (credit - slot >= (GET_CODE (SET_SRC (pat)) == PC ? 2 : 0))
		return 0;
	    }
	}
    }
  
  return align_jumps_log;
}

/* If we are inside a phony loop, almost any kind of label can turn up as the
   first one in the loop.  Aligning a braf label causes incorrect switch
   destination addresses; we can detect braf labels because they are
   followed by a BARRIER.
   Applying loop alignment to small constant or switch tables is a waste
   of space, so we suppress this too.  */
int
sh_loop_align (label)
     rtx label;
{
  rtx next = label;

  do
    next = next_nonnote_insn (next);
  while (next && GET_CODE (next) == CODE_LABEL);

  if (! next
      || ! INSN_P (next)
      || GET_CODE (PATTERN (next)) == ADDR_DIFF_VEC
      || recog_memoized (next) == CODE_FOR_consttable_2)
    return 0;

  return align_loops_log;
}

/* Exported to toplev.c.

   Do a final pass over the function, just before delayed branch
   scheduling.  */

void
machine_dependent_reorg (first)
     rtx first;
{
  rtx insn, mova;
  int num_mova;
  rtx r0_rtx = gen_rtx_REG (Pmode, 0);
  rtx r0_inc_rtx = gen_rtx_POST_INC (Pmode, r0_rtx);

  /* We must split call insns before introducing `mova's.  If we're
     optimizing, they'll have already been split.  Otherwise, make
     sure we don't split them too late.  */
  if (! optimize)
    split_all_insns_noflow ();

  if (TARGET_SHMEDIA)
    return;

  /* If relaxing, generate pseudo-ops to associate function calls with
     the symbols they call.  It does no harm to not generate these
     pseudo-ops.  However, when we can generate them, it enables to
     linker to potentially relax the jsr to a bsr, and eliminate the
     register load and, possibly, the constant pool entry.  */

  mdep_reorg_phase = SH_INSERT_USES_LABELS;
  if (TARGET_RELAX)
    {
      /* Remove all REG_LABEL notes.  We want to use them for our own
	 purposes.  This works because none of the remaining passes
	 need to look at them.

	 ??? But it may break in the future.  We should use a machine
	 dependent REG_NOTE, or some other approach entirely.  */
      for (insn = first; insn; insn = NEXT_INSN (insn))
	{
	  if (INSN_P (insn))
	    {
	      rtx note;

	      while ((note = find_reg_note (insn, REG_LABEL, NULL_RTX)) != 0)
		remove_note (insn, note);
	    }
	}

      for (insn = first; insn; insn = NEXT_INSN (insn))
	{
	  rtx pattern, reg, link, set, scan, dies, label;
	  int rescan = 0, foundinsn = 0;

	  if (GET_CODE (insn) == CALL_INSN)
	    {
	      pattern = PATTERN (insn);

	      if (GET_CODE (pattern) == PARALLEL)
		pattern = XVECEXP (pattern, 0, 0);
	      if (GET_CODE (pattern) == SET)
		pattern = SET_SRC (pattern);

	      if (GET_CODE (pattern) != CALL
		  || GET_CODE (XEXP (pattern, 0)) != MEM)
		continue;

	      reg = XEXP (XEXP (pattern, 0), 0);
	    }
	  else
	    {
	      reg = sfunc_uses_reg (insn);
	      if (! reg)
		continue;
	    }

	  if (GET_CODE (reg) != REG)
	    continue;

	  /* This is a function call via REG.  If the only uses of REG
	     between the time that it is set and the time that it dies
	     are in function calls, then we can associate all the
	     function calls with the setting of REG.  */

	  for (link = LOG_LINKS (insn); link; link = XEXP (link, 1))
	    {
	      if (REG_NOTE_KIND (link) != 0)
		continue;
	      set = single_set (XEXP (link, 0));
	      if (set && rtx_equal_p (reg, SET_DEST (set)))
		{
		  link = XEXP (link, 0);
		  break;
		}
	    }

	  if (! link)
	    {
	      /* ??? Sometimes global register allocation will have
                 deleted the insn pointed to by LOG_LINKS.  Try
                 scanning backward to find where the register is set.  */
	      for (scan = PREV_INSN (insn);
		   scan && GET_CODE (scan) != CODE_LABEL;
		   scan = PREV_INSN (scan))
		{
		  if (! INSN_P (scan))
		    continue;

		  if (! reg_mentioned_p (reg, scan))
		    continue;

		  if (noncall_uses_reg (reg, scan, &set))
		    break;

		  if (set)
		    {
		      link = scan;
		      break;
		    }
		}
	    }

	  if (! link)
	    continue;

	  /* The register is set at LINK.  */

	  /* We can only optimize the function call if the register is
             being set to a symbol.  In theory, we could sometimes
             optimize calls to a constant location, but the assembler
             and linker do not support that at present.  */
	  if (GET_CODE (SET_SRC (set)) != SYMBOL_REF
	      && GET_CODE (SET_SRC (set)) != LABEL_REF)
	    continue;

	  /* Scan forward from LINK to the place where REG dies, and
             make sure that the only insns which use REG are
             themselves function calls.  */

	  /* ??? This doesn't work for call targets that were allocated
	     by reload, since there may not be a REG_DEAD note for the
	     register.  */

	  dies = NULL_RTX;
	  for (scan = NEXT_INSN (link); scan; scan = NEXT_INSN (scan))
	    {
	      rtx scanset;

	      /* Don't try to trace forward past a CODE_LABEL if we haven't
		 seen INSN yet.  Ordinarily, we will only find the setting insn
		 in LOG_LINKS if it is in the same basic block.  However,
		 cross-jumping can insert code labels in between the load and
		 the call, and can result in situations where a single call
		 insn may have two targets depending on where we came from.  */

	      if (GET_CODE (scan) == CODE_LABEL && ! foundinsn)
		break;

	      if (! INSN_P (scan))
		continue;

	      /* Don't try to trace forward past a JUMP.  To optimize
                 safely, we would have to check that all the
                 instructions at the jump destination did not use REG.  */

	      if (GET_CODE (scan) == JUMP_INSN)
		break;

	      if (! reg_mentioned_p (reg, scan))
		continue;

	      if (noncall_uses_reg (reg, scan, &scanset))
		break;

	      if (scan == insn)
		foundinsn = 1;

	      if (scan != insn
		  && (GET_CODE (scan) == CALL_INSN || sfunc_uses_reg (scan)))
		{
		  /* There is a function call to this register other
                     than the one we are checking.  If we optimize
                     this call, we need to rescan again below.  */
		  rescan = 1;
		}

	      /* ??? We shouldn't have to worry about SCANSET here.
		 We should just be able to check for a REG_DEAD note
		 on a function call.  However, the REG_DEAD notes are
		 apparently not dependable around libcalls; c-torture
		 execute/920501-2 is a test case.  If SCANSET is set,
		 then this insn sets the register, so it must have
		 died earlier.  Unfortunately, this will only handle
		 the cases in which the register is, in fact, set in a
		 later insn.  */

	      /* ??? We shouldn't have to use FOUNDINSN here.
		 However, the LOG_LINKS fields are apparently not
		 entirely reliable around libcalls;
		 newlib/libm/math/e_pow.c is a test case.  Sometimes
		 an insn will appear in LOG_LINKS even though it is
		 not the most recent insn which sets the register.  */

	      if (foundinsn
		  && (scanset
		      || find_reg_note (scan, REG_DEAD, reg)))
		{
		  dies = scan;
		  break;
		}
	    }

	  if (! dies)
	    {
	      /* Either there was a branch, or some insn used REG
                 other than as a function call address.  */
	      continue;
	    }

	  /* Create a code label, and put it in a REG_LABEL note on
             the insn which sets the register, and on each call insn
             which uses the register.  In final_prescan_insn we look
             for the REG_LABEL notes, and output the appropriate label
             or pseudo-op.  */

	  label = gen_label_rtx ();
	  REG_NOTES (link) = gen_rtx_INSN_LIST (REG_LABEL, label,
						REG_NOTES (link));
	  REG_NOTES (insn) = gen_rtx_INSN_LIST (REG_LABEL, label,
						REG_NOTES (insn));
	  if (rescan)
	    {
	      scan = link;
	      do
		{
		  rtx reg2;

		  scan = NEXT_INSN (scan);
		  if (scan != insn
		      && ((GET_CODE (scan) == CALL_INSN
			   && reg_mentioned_p (reg, scan))
			  || ((reg2 = sfunc_uses_reg (scan))
			      && REGNO (reg2) == REGNO (reg))))
		    REG_NOTES (scan)
		      = gen_rtx_INSN_LIST (REG_LABEL, label, REG_NOTES (scan));
		}
	      while (scan != dies);
	    }
	}
    }

  if (TARGET_SH2)
    fixup_addr_diff_vecs (first);

  if (optimize)
    {
      mdep_reorg_phase = SH_SHORTEN_BRANCHES0;
      shorten_branches (first);
    }
  /* Scan the function looking for move instructions which have to be
     changed to pc-relative loads and insert the literal tables.  */

  mdep_reorg_phase = SH_FIXUP_PCLOAD;
  for (insn = first, num_mova = 0; insn; insn = NEXT_INSN (insn))
    {
      if (mova_p (insn))
	{
	  if (! num_mova++)
	    mova = insn;
	}
      else if (GET_CODE (insn) == JUMP_INSN
	       && GET_CODE (PATTERN (insn)) == ADDR_DIFF_VEC
	       && num_mova)
	{
	  rtx scan;
	  int total;

	  num_mova--;

	  /* Some code might have been inserted between the mova and
	     its ADDR_DIFF_VEC.  Check if the mova is still in range.  */
	  for (scan = mova, total = 0; scan != insn; scan = NEXT_INSN (scan))
	    total += get_attr_length (scan);

	  /* range of mova is 1020, add 4 because pc counts from address of
	     second instruction after this one, subtract 2 in case pc is 2
	     byte aligned.  Possible alignment needed for the ADDR_DIFF_VEC
	     cancels out with alignment effects of the mova itself.  */
	  if (total > 1022)
	    {
	      /* Change the mova into a load, and restart scanning
		 there.  broken_move will then return true for mova.  */
	      SET_SRC (PATTERN (mova))
		= XVECEXP (SET_SRC (PATTERN (mova)), 0, 0);
	      INSN_CODE (mova) = -1;
	      insn = mova;
	    }
	}
      if (broken_move (insn))
	{
	  rtx scan;
	  /* Scan ahead looking for a barrier to stick the constant table
	     behind.  */
	  rtx barrier = find_barrier (num_mova, mova, insn);
	  rtx last_float_move, last_float = 0, *last_float_addr;

	  if (num_mova && ! mova_p (mova))
	    {
	      /* find_barrier had to change the first mova into a
		 pcload; thus, we have to start with this new pcload.  */
	      insn = mova;
	      num_mova = 0;
	    }
	  /* Now find all the moves between the points and modify them.  */
	  for (scan = insn; scan != barrier; scan = NEXT_INSN (scan))
	    {
	      if (GET_CODE (scan) == CODE_LABEL)
		last_float = 0;
	      if (broken_move (scan))
		{
		  rtx *patp = &PATTERN (scan), pat = *patp;
		  rtx src, dst;
		  rtx lab;
		  rtx newsrc;
		  enum machine_mode mode;

		  if (GET_CODE (pat) == PARALLEL)
		    patp = &XVECEXP (pat, 0, 0), pat = *patp;
		  src = SET_SRC (pat);
		  dst = SET_DEST (pat);
		  mode = GET_MODE (dst);

		  if (mode == SImode && hi_const (src)
		      && REGNO (dst) != FPUL_REG)
		    {
		      int offset = 0;

		      mode = HImode;
		      while (GET_CODE (dst) == SUBREG)
			{
			  offset += subreg_regno_offset (REGNO (SUBREG_REG (dst)),
							 GET_MODE (SUBREG_REG (dst)),
							 SUBREG_BYTE (dst),
							 GET_MODE (dst));
			  dst = SUBREG_REG (dst);
			}
		      dst = gen_rtx_REG (HImode, REGNO (dst) + offset);
		    }

		  if (GET_CODE (dst) == REG && FP_ANY_REGISTER_P (REGNO (dst)))
		    {
		      /* This must be an insn that clobbers r0.  */
		      rtx clobber = XVECEXP (PATTERN (scan), 0,
					     XVECLEN (PATTERN (scan), 0) - 1);

		      if (GET_CODE (clobber) != CLOBBER
			  || ! rtx_equal_p (XEXP (clobber, 0), r0_rtx))
			abort ();

		      if (last_float
			  && reg_set_between_p (r0_rtx, last_float_move, scan))
			last_float = 0;
		      if (last_float
			  && TARGET_SHCOMPACT
			  && GET_MODE_SIZE (mode) != 4
			  && GET_MODE_SIZE (GET_MODE (last_float)) == 4)
			last_float = 0;
		      lab = add_constant (src, mode, last_float);
		      if (lab)
			emit_insn_before (gen_mova (lab), scan);
		      else
			{
			  /* There will be a REG_UNUSED note for r0 on
			     LAST_FLOAT_MOVE; we have to change it to REG_INC,
			     lest reorg:mark_target_live_regs will not
			     consider r0 to be used, and we end up with delay
			     slot insn in front of SCAN that clobbers r0.  */
			  rtx note
			    = find_regno_note (last_float_move, REG_UNUSED, 0);

			  /* If we are not optimizing, then there may not be
			     a note.  */
			  if (note)
			    PUT_MODE (note, REG_INC);

			  *last_float_addr = r0_inc_rtx;
			}
		      last_float_move = scan;
		      last_float = src;
		      newsrc = gen_rtx (MEM, mode,
					(((TARGET_SH4 && ! TARGET_FMOVD)
					  || REGNO (dst) == FPUL_REG)
					 ? r0_inc_rtx
					 : r0_rtx));
		      last_float_addr = &XEXP (newsrc, 0);

		      /* Remove the clobber of r0.  */
		      XEXP (clobber, 0) = gen_rtx_SCRATCH (Pmode);
		      RTX_UNCHANGING_P (newsrc) = 1;
		    }
		  /* This is a mova needing a label.  Create it.  */
		  else if (GET_CODE (src) == UNSPEC
			   && XINT (src, 1) == UNSPEC_MOVA
			   && GET_CODE (XVECEXP (src, 0, 0)) == CONST)
		    {
		      lab = add_constant (XVECEXP (src, 0, 0), mode, 0);
		      newsrc = gen_rtx_LABEL_REF (VOIDmode, lab);
		      newsrc = gen_rtx_UNSPEC (SImode,
					       gen_rtvec (1, newsrc),
					       UNSPEC_MOVA);
		    }
		  else
		    {
		      lab = add_constant (src, mode, 0);
		      newsrc = gen_rtx_MEM (mode,
					    gen_rtx_LABEL_REF (VOIDmode, lab));
		      RTX_UNCHANGING_P (newsrc) = 1;
		    }
		  *patp = gen_rtx_SET (VOIDmode, dst, newsrc);
		  INSN_CODE (scan) = -1;
		}
	    }
	  dump_table (barrier);
	  insn = barrier;
	}
    }

  mdep_reorg_phase = SH_SHORTEN_BRANCHES1;
  INSN_ADDRESSES_FREE ();
  split_branches (first);

  /* The INSN_REFERENCES_ARE_DELAYED in sh.h is problematic because it
     also has an effect on the register that holds the address of the sfunc.
     Insert an extra dummy insn in front of each sfunc that pretends to
     use this register.  */
  if (flag_delayed_branch)
    {
      for (insn = first; insn; insn = NEXT_INSN (insn))
	{
	  rtx reg = sfunc_uses_reg (insn);

	  if (! reg)
	    continue;
	  emit_insn_before (gen_use_sfunc_addr (reg), insn);
	}
    }
#if 0
  /* fpscr is not actually a user variable, but we pretend it is for the
     sake of the previous optimization passes, since we want it handled like
     one.  However, we don't have any debugging information for it, so turn
     it into a non-user variable now.  */
  if (TARGET_SH4)
    REG_USERVAR_P (get_fpscr_rtx ()) = 0;
#endif
  mdep_reorg_phase = SH_AFTER_MDEP_REORG;
}

int
get_dest_uid (label, max_uid)
     rtx label;
     int max_uid;
{
  rtx dest = next_real_insn (label);
  int dest_uid;
  if (! dest)
    /* This can happen for an undefined label.  */
    return 0;
  dest_uid = INSN_UID (dest);
  /* If this is a newly created branch redirection blocking instruction,
     we cannot index the branch_uid or insn_addresses arrays with its
     uid.  But then, we won't need to, because the actual destination is
     the following branch.  */
  while (dest_uid >= max_uid)
    {
      dest = NEXT_INSN (dest);
      dest_uid = INSN_UID (dest);
    }
  if (GET_CODE (dest) == JUMP_INSN && GET_CODE (PATTERN (dest)) == RETURN)
    return 0;
  return dest_uid;
}

/* Split condbranches that are out of range.  Also add clobbers for
   scratch registers that are needed in far jumps.
   We do this before delay slot scheduling, so that it can take our
   newly created instructions into account.  It also allows us to
   find branches with common targets more easily.  */

static void
split_branches (first)
     rtx first;
{
  rtx insn;
  struct far_branch **uid_branch, *far_branch_list = 0;
  int max_uid = get_max_uid ();

  /* Find out which branches are out of range.  */
  shorten_branches (first);

  uid_branch = (struct far_branch **) alloca (max_uid * sizeof *uid_branch);
  memset ((char *) uid_branch, 0, max_uid * sizeof *uid_branch);

  for (insn = first; insn; insn = NEXT_INSN (insn))
    if (! INSN_P (insn))
      continue;
    else if (INSN_DELETED_P (insn))
      {
	/* Shorten_branches would split this instruction again,
	   so transform it into a note.  */
	PUT_CODE (insn, NOTE);
	NOTE_LINE_NUMBER (insn) = NOTE_INSN_DELETED;
	NOTE_SOURCE_FILE (insn) = 0;
      }
    else if (GET_CODE (insn) == JUMP_INSN
	     /* Don't mess with ADDR_DIFF_VEC */
	     && (GET_CODE (PATTERN (insn)) == SET
		 || GET_CODE (PATTERN (insn)) == RETURN))
      {
	enum attr_type type = get_attr_type (insn);
	if (type == TYPE_CBRANCH)
	  {
	    rtx next, beyond;
    
	    if (get_attr_length (insn) > 4)
	      {
		rtx src = SET_SRC (PATTERN (insn));
		rtx olabel = XEXP (XEXP (src, 1), 0);
		int addr = INSN_ADDRESSES (INSN_UID (insn));
		rtx label = 0;
		int dest_uid = get_dest_uid (olabel, max_uid);
		struct far_branch *bp = uid_branch[dest_uid];
    
		/* redirect_jump needs a valid JUMP_LABEL, and it might delete
		   the label if the LABEL_NUSES count drops to zero.  There is
		   always a jump_optimize pass that sets these values, but it
		   proceeds to delete unreferenced code, and then if not
		   optimizing, to un-delete the deleted instructions, thus
		   leaving labels with too low uses counts.  */
		if (! optimize)
		  {
		    JUMP_LABEL (insn) = olabel;
		    LABEL_NUSES (olabel)++;
		  }
		if (! bp)
		  {
		    bp = (struct far_branch *) alloca (sizeof *bp);
		    uid_branch[dest_uid] = bp;
		    bp->prev = far_branch_list;
		    far_branch_list = bp;
		    bp->far_label
		      = XEXP (XEXP (SET_SRC (PATTERN (insn)), 1), 0);
		    LABEL_NUSES (bp->far_label)++;
		  }
		else
		  {
		    label = bp->near_label;
		    if (! label && bp->address - addr >= CONDJUMP_MIN)
		      {
			rtx block = bp->insert_place;

			if (GET_CODE (PATTERN (block)) == RETURN)
			  block = PREV_INSN (block);
			else
			  block = gen_block_redirect (block,
						      bp->address, 2);
			label = emit_label_after (gen_label_rtx (),
						  PREV_INSN (block));
			bp->near_label = label;
		      }
		    else if (label && ! NEXT_INSN (label))
		      {
			if (addr + 2 - bp->address <= CONDJUMP_MAX)
			  bp->insert_place = insn;
			else
			  gen_far_branch (bp);
		      }
		  }
		if (! label
		    || (NEXT_INSN (label) && bp->address - addr < CONDJUMP_MIN))
		  {
		    bp->near_label = label = gen_label_rtx ();
		    bp->insert_place = insn;
		    bp->address = addr;
		  }
		if (! redirect_jump (insn, label, 1))
		  abort ();
	      }
	    else
	      {
		/* get_attr_length (insn) == 2 */
		/* Check if we have a pattern where reorg wants to redirect
		   the branch to a label from an unconditional branch that
		   is too far away.  */
		/* We can't use JUMP_LABEL here because it might be undefined
		   when not optimizing.  */
		/* A syntax error might cause beyond to be NULL_RTX.  */
		beyond
		  = next_active_insn (XEXP (XEXP (SET_SRC (PATTERN (insn)), 1),
					    0));
	
		if (beyond
		    && (GET_CODE (beyond) == JUMP_INSN
			|| ((beyond = next_active_insn (beyond))
			    && GET_CODE (beyond) == JUMP_INSN))
		    && GET_CODE (PATTERN (beyond)) == SET
		    && recog_memoized (beyond) == CODE_FOR_jump_compact
		    && ((INSN_ADDRESSES
			 (INSN_UID (XEXP (SET_SRC (PATTERN (beyond)), 0)))
			 - INSN_ADDRESSES (INSN_UID (insn)) + (unsigned) 252)
			> 252 + 258 + 2))
		  gen_block_redirect (beyond,
				      INSN_ADDRESSES (INSN_UID (beyond)), 1);
	      }
    
	    next = next_active_insn (insn);

	    if ((GET_CODE (next) == JUMP_INSN
		 || GET_CODE (next = next_active_insn (next)) == JUMP_INSN)
		&& GET_CODE (PATTERN (next)) == SET
		&& recog_memoized (next) == CODE_FOR_jump_compact
		&& ((INSN_ADDRESSES
		     (INSN_UID (XEXP (SET_SRC (PATTERN (next)), 0)))
		     - INSN_ADDRESSES (INSN_UID (insn)) + (unsigned) 252)
		    > 252 + 258 + 2))
	      gen_block_redirect (next, INSN_ADDRESSES (INSN_UID (next)), 1);
	  }
	else if (type == TYPE_JUMP || type == TYPE_RETURN)
	  {
	    int addr = INSN_ADDRESSES (INSN_UID (insn));
	    rtx far_label = 0;
	    int dest_uid = 0;
	    struct far_branch *bp;

	    if (type == TYPE_JUMP)
	      {
		far_label = XEXP (SET_SRC (PATTERN (insn)), 0);
		dest_uid = get_dest_uid (far_label, max_uid);
		if (! dest_uid)
		  {
		    /* Parse errors can lead to labels outside
		      the insn stream.  */
		    if (! NEXT_INSN (far_label))
		      continue;

		    if (! optimize)
		      {
			JUMP_LABEL (insn) = far_label;
			LABEL_NUSES (far_label)++;
		      }
		    redirect_jump (insn, NULL_RTX, 1);
		    far_label = 0;
		  }
	      }
	    bp = uid_branch[dest_uid];
	    if (! bp)
	      {
		bp = (struct far_branch *) alloca (sizeof *bp);
		uid_branch[dest_uid] = bp;
		bp->prev = far_branch_list;
		far_branch_list = bp;
		bp->near_label = 0;
		bp->far_label = far_label;
		if (far_label)
		  LABEL_NUSES (far_label)++;
	      }
	    else if (bp->near_label && ! NEXT_INSN (bp->near_label))
	      if (addr - bp->address <= CONDJUMP_MAX)
		emit_label_after (bp->near_label, PREV_INSN (insn));
	      else
		{
		  gen_far_branch (bp);
		  bp->near_label = 0;
		}
	    else
	      bp->near_label = 0;
	    bp->address = addr;
	    bp->insert_place = insn;
	    if (! far_label)
	      emit_insn_before (gen_block_branch_redirect (const0_rtx), insn);
	    else
	      gen_block_redirect (insn, addr, bp->near_label ? 2 : 0);
	  }
      }
  /* Generate all pending far branches,
     and free our references to the far labels.  */
  while (far_branch_list)
    {
      if (far_branch_list->near_label
	  && ! NEXT_INSN (far_branch_list->near_label))
	gen_far_branch (far_branch_list);
      if (optimize
	  && far_branch_list->far_label
	  && ! --LABEL_NUSES (far_branch_list->far_label))
	delete_insn (far_branch_list->far_label);
      far_branch_list = far_branch_list->prev;
    }

  /* Instruction length information is no longer valid due to the new
     instructions that have been generated.  */
  init_insn_lengths ();
}

/* Dump out instruction addresses, which is useful for debugging the
   constant pool table stuff.

   If relaxing, output the label and pseudo-ops used to link together
   calls and the instruction which set the registers.  */

/* ??? The addresses printed by this routine for insns are nonsense for
   insns which are inside of a sequence where none of the inner insns have
   variable length.  This is because the second pass of shorten_branches
   does not bother to update them.  */

void
final_prescan_insn (insn, opvec, noperands)
     rtx insn;
     rtx *opvec ATTRIBUTE_UNUSED;
     int noperands ATTRIBUTE_UNUSED;
{
  if (TARGET_DUMPISIZE)
    fprintf (asm_out_file, "\n! at %04x\n", INSN_ADDRESSES (INSN_UID (insn)));

  if (TARGET_RELAX)
    {
      rtx note;

      note = find_reg_note (insn, REG_LABEL, NULL_RTX);
      if (note)
	{
	  rtx pattern;

	  pattern = PATTERN (insn);
	  if (GET_CODE (pattern) == PARALLEL)
	    pattern = XVECEXP (pattern, 0, 0);
	  if (GET_CODE (pattern) == CALL
	      || (GET_CODE (pattern) == SET
		  && (GET_CODE (SET_SRC (pattern)) == CALL
		      || get_attr_type (insn) == TYPE_SFUNC)))
	    asm_fprintf (asm_out_file, "\t.uses %LL%d\n",
			 CODE_LABEL_NUMBER (XEXP (note, 0)));
	  else if (GET_CODE (pattern) == SET)
	    ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "L",
				       CODE_LABEL_NUMBER (XEXP (note, 0)));
	  else
	    abort ();
	}
    }
}

/* Dump out any constants accumulated in the final pass.  These will
   only be labels.  */

const char *
output_jump_label_table ()
{
  int i;

  if (pool_size)
    {
      fprintf (asm_out_file, "\t.align 2\n");
      for (i = 0; i < pool_size; i++)
	{
	  pool_node *p = &pool_vector[i];

	  ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "L",
				     CODE_LABEL_NUMBER (p->label));
	  output_asm_insn (".long	%O0", &p->value);
	}
      pool_size = 0;
    }

  return "";
}

/* A full frame looks like:

   arg-5
   arg-4
   [ if current_function_anonymous_args
   arg-3
   arg-2
   arg-1
   arg-0 ]
   saved-fp
   saved-r10
   saved-r11
   saved-r12
   saved-pr
   local-n
   ..
   local-1
   local-0        <- fp points here.  */

/* Number of bytes pushed for anonymous args, used to pass information
   between expand_prologue and expand_epilogue.  */

static int extra_push;

/* Adjust the stack by SIZE bytes.  REG holds the rtl of the register
  to be adjusted, and TEMP, if nonnegative, holds the register number
  of a general register that we may clobber.  */

static void
output_stack_adjust (size, reg, temp, emit_fn)
     int size;
     rtx reg;
     int temp;
     rtx (*emit_fn) PARAMS ((rtx));
{
  if (size)
    {
      HOST_WIDE_INT align = STACK_BOUNDARY / BITS_PER_UNIT;

      if (size % align)
	abort ();

      if (CONST_OK_FOR_ADD (size))
	emit_fn (GEN_ADD3 (reg, reg, GEN_INT (size)));
      /* Try to do it with two partial adjustments; however, we must make
	 sure that the stack is properly aligned at all times, in case
	 an interrupt occurs between the two partial adjustments.  */
      else if (CONST_OK_FOR_ADD (size / 2 & -align)
	       && CONST_OK_FOR_ADD (size - (size / 2 & -align)))
	{
	  emit_fn (GEN_ADD3 (reg, reg, GEN_INT (size / 2 & -align)));
	  emit_fn (GEN_ADD3 (reg, reg, GEN_INT (size - (size / 2 & -align))));
	}
      else
	{
	  rtx const_reg;
	  rtx insn;

	  /* If TEMP is invalid, we could temporarily save a general
	     register to MACL.  However, there is currently no need
	     to handle this case, so just abort when we see it.  */
	  if (temp < 0)
	    abort ();
	  const_reg = gen_rtx_REG (GET_MODE (reg), temp);

	  /* If SIZE is negative, subtract the positive value.
	     This sometimes allows a constant pool entry to be shared
	     between prologue and epilogue code.  */
	  if (size < 0)
	    {
	      emit_insn (GEN_MOV (const_reg, GEN_INT (-size)));
	      insn = emit_fn (GEN_SUB3 (reg, reg, const_reg));
	    }
	  else
	    {
	      emit_insn (GEN_MOV (const_reg, GEN_INT (size)));
	      insn = emit_fn (GEN_ADD3 (reg, reg, const_reg));
	    }
	  if (emit_fn == frame_insn)
	    REG_NOTES (insn)
	      = (gen_rtx_EXPR_LIST
		 (REG_FRAME_RELATED_EXPR,
		  gen_rtx_SET (VOIDmode, reg,
			       gen_rtx_PLUS (SImode, reg, GEN_INT (size))),
		  REG_NOTES (insn)));
	}
    }
}

static rtx
frame_insn (x)
     rtx x;
{
  x = emit_insn (x);
  RTX_FRAME_RELATED_P (x) = 1;
  return x;
}

/* Output RTL to push register RN onto the stack.  */

static rtx
push (rn)
     int rn;
{
  rtx x;
  if (rn == FPUL_REG)
    x = gen_push_fpul ();
  else if (rn == FPSCR_REG)
    x = gen_push_fpscr ();
  else if (TARGET_SH4 && TARGET_FMOVD && ! TARGET_FPU_SINGLE
	   && FP_OR_XD_REGISTER_P (rn))
    {
      if (FP_REGISTER_P (rn) && (rn - FIRST_FP_REG) & 1)
	return NULL_RTX;
      x = gen_push_4 (gen_rtx_REG (DFmode, rn));
    }
  else if (TARGET_SH3E && FP_REGISTER_P (rn))
    x = gen_push_e (gen_rtx_REG (SFmode, rn));
  else
    x = gen_push (gen_rtx_REG (SImode, rn));

  x = frame_insn (x);
  REG_NOTES (x)
    = gen_rtx_EXPR_LIST (REG_INC,
			 gen_rtx_REG (SImode, STACK_POINTER_REGNUM), 0);
  return x;
}

/* Output RTL to pop register RN from the stack.  */

static void
pop (rn)
     int rn;
{
  rtx x;
  if (rn == FPUL_REG)
    x = gen_pop_fpul ();
  else if (rn == FPSCR_REG)
    x = gen_pop_fpscr ();
  else if (TARGET_SH4 && TARGET_FMOVD && ! TARGET_FPU_SINGLE
	   && FP_OR_XD_REGISTER_P (rn))
    {
      if (FP_REGISTER_P (rn) && (rn - FIRST_FP_REG) & 1)
	return;
      x = gen_pop_4 (gen_rtx_REG (DFmode, rn));
    }
  else if (TARGET_SH3E && FP_REGISTER_P (rn))
    x = gen_pop_e (gen_rtx_REG (SFmode, rn));
  else
    x = gen_pop (gen_rtx_REG (SImode, rn));
    
  x = emit_insn (x);
  REG_NOTES (x)
    = gen_rtx_EXPR_LIST (REG_INC,
			 gen_rtx_REG (SImode, STACK_POINTER_REGNUM), 0);
}

/* Generate code to push the regs specified in the mask.  */

static void
push_regs (mask)
     HOST_WIDE_INT *mask;
{
  int i;

  /* Push PR last; this gives better latencies after the prologue, and
     candidates for the return delay slot when there are no general
     registers pushed.  */
  for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
    if (i != PR_REG && mask[i / 32] & (1 << (i % 32)))
      push (i);
  if (mask[PR_REG / 32] & (1 << (PR_REG % 32)))
    push (PR_REG);
}

/* Work out the registers which need to be saved, both as a mask and a
   count of saved words.

   If doing a pragma interrupt function, then push all regs used by the
   function, and if we call another function (we can tell by looking at PR),
   make sure that all the regs it clobbers are safe too.  */

static void
calc_live_regs (count_ptr, live_regs_mask)
     int *count_ptr;
     HOST_WIDE_INT *live_regs_mask;
{
  int reg;
  int count;
  int interrupt_handler;
  int pr_live;

  interrupt_handler = sh_cfun_interrupt_handler_p ();

  for (count = 0; 32 * count < FIRST_PSEUDO_REGISTER; count++)
    live_regs_mask[count] = 0;
  /* If we can save a lot of saves by switching to double mode, do that.  */
  if (TARGET_SH4 && TARGET_FMOVD && TARGET_FPU_SINGLE)
    for (count = 0, reg = FIRST_FP_REG; reg <= LAST_FP_REG; reg += 2)
      if (regs_ever_live[reg] && regs_ever_live[reg+1]
	  && (! call_used_regs[reg] || (interrupt_handler && ! pragma_trapa))
	  && ++count > 2)
	{
	  target_flags &= ~FPU_SINGLE_BIT;
	  break;
	}
  /* PR_MEDIA_REG is a general purpose register, thus global_alloc already
     knows how to use it.  That means the pseudo originally allocated for
     the initial value can become the PR_MEDIA_REG hard register, as seen for
     execute/20010122-1.c:test9.  */
  if (TARGET_SHMEDIA)
    pr_live = regs_ever_live[PR_MEDIA_REG];
  else
    {
      rtx pr_initial = has_hard_reg_initial_val (Pmode, PR_REG);
      pr_live = (pr_initial
		 ? REGNO (pr_initial) != (PR_REG) : regs_ever_live[PR_REG]);
    }
  /* Force PR to be live if the prologue has to call the SHmedia
     argument decoder or register saver.  */
  if (TARGET_SHCOMPACT
      && ((current_function_args_info.call_cookie
	   & ~ CALL_COOKIE_RET_TRAMP (1))
	  || current_function_has_nonlocal_label))
    pr_live = 1;
  for (count = 0, reg = FIRST_PSEUDO_REGISTER - 1; reg >= 0; reg--)
    {
      if (reg == (TARGET_SHMEDIA ? PR_MEDIA_REG : PR_REG)
	  ? pr_live
	  : (interrupt_handler && ! pragma_trapa)
	  ? (/* Need to save all the regs ever live.  */
	     (regs_ever_live[reg]
	      || (call_used_regs[reg]
		  && (! fixed_regs[reg] || reg == MACH_REG || reg == MACL_REG)
		  && pr_live))
	     && reg != STACK_POINTER_REGNUM && reg != ARG_POINTER_REGNUM
	     && reg != RETURN_ADDRESS_POINTER_REGNUM
	     && reg != T_REG && reg != GBR_REG
	     /* Push fpscr only on targets which have FPU */
	     && (reg != FPSCR_REG || TARGET_FPU_ANY))
	  : (/* Only push those regs which are used and need to be saved.  */
	     (TARGET_SHCOMPACT
	      && flag_pic
	      && current_function_args_info.call_cookie
	      && reg == PIC_OFFSET_TABLE_REGNUM)
	     || (regs_ever_live[reg] && ! call_used_regs[reg])))
	{
	  live_regs_mask[reg / 32] |= 1 << (reg % 32);
	  count += GET_MODE_SIZE (REGISTER_NATURAL_MODE (reg));

	  if ((TARGET_SH4 || TARGET_SH5) && TARGET_FMOVD
	      && GET_MODE_CLASS (REGISTER_NATURAL_MODE (reg)) == MODE_FLOAT)
	    {
	      if (FP_REGISTER_P (reg))
		{
		  if (! TARGET_FPU_SINGLE && ! regs_ever_live[reg ^ 1])
		    {
		      live_regs_mask[(reg ^ 1) / 32] |= 1 << ((reg ^ 1) % 32);
		      count += GET_MODE_SIZE (REGISTER_NATURAL_MODE (reg ^ 1));
		    }
		}
	      else if (XD_REGISTER_P (reg))
		{
		  /* Must switch to double mode to access these registers.  */
		  target_flags &= ~FPU_SINGLE_BIT;
		}
	    }
	}
    }

  *count_ptr = count;
}

/* Code to generate prologue and epilogue sequences */

/* PUSHED is the number of bytes that are bing pushed on the
   stack for register saves.  Return the frame size, padded
   appropriately so that the stack stays properly aligned.  */
static HOST_WIDE_INT
rounded_frame_size (pushed)
     int pushed;
{
  HOST_WIDE_INT size = get_frame_size ();
  HOST_WIDE_INT align = STACK_BOUNDARY / BITS_PER_UNIT;

  return ((size + pushed + align - 1) & -align) - pushed;
}

/* Choose a call-clobbered target-branch register that remains
   unchanged along the whole function.  We set it up as the return
   value in the prologue.  */
int
sh_media_register_for_return ()
{
  int regno;
  int tr0_used;

  if (! current_function_is_leaf)
    return -1;

  tr0_used = flag_pic && regs_ever_live[PIC_OFFSET_TABLE_REGNUM];

  for (regno = FIRST_TARGET_REG + tr0_used; regno <= LAST_TARGET_REG; regno++)
    if (call_used_regs[regno] && ! regs_ever_live[regno])
      return regno;

  return -1;
}

void
sh_expand_prologue ()
{
  HOST_WIDE_INT live_regs_mask[(FIRST_PSEUDO_REGISTER + 31) / 32];
  int d, i;
  int d_rounding = 0;
  int save_flags = target_flags;

  current_function_interrupt = sh_cfun_interrupt_handler_p ();

  /* We have pretend args if we had an object sent partially in registers
     and partially on the stack, e.g. a large structure.  */
  output_stack_adjust (-current_function_pretend_args_size
		       - current_function_args_info.stack_regs * 8,
		       stack_pointer_rtx, TARGET_SH5 ? 0 : 1, frame_insn);

  extra_push = 0;

  if (TARGET_SHCOMPACT && flag_pic && current_function_args_info.call_cookie)
    /* We're going to use the PIC register to load the address of the
       incoming-argument decoder and/or of the return trampoline from
       the GOT, so make sure the PIC register is preserved and
       initialized.  */
    regs_ever_live[PIC_OFFSET_TABLE_REGNUM] = 1;

  if (TARGET_SHCOMPACT
      && (current_function_args_info.call_cookie & ~ CALL_COOKIE_RET_TRAMP(1)))
    {
      int reg;

      /* First, make all registers with incoming arguments that will
	 be pushed onto the stack live, so that register renaming
	 doesn't overwrite them.  */
      for (reg = 0; reg < NPARM_REGS (SImode); reg++)
	if (CALL_COOKIE_STACKSEQ_GET (current_function_args_info.call_cookie)
	    >= NPARM_REGS (SImode) - reg)
	  for (; reg < NPARM_REGS (SImode); reg++)
	    emit_insn (gen_shcompact_preserve_incoming_args
		       (gen_rtx_REG (SImode, FIRST_PARM_REG + reg)));
	else if (CALL_COOKIE_INT_REG_GET
		 (current_function_args_info.call_cookie, reg) == 1)
	  emit_insn (gen_shcompact_preserve_incoming_args
		     (gen_rtx_REG (SImode, FIRST_PARM_REG + reg)));

      emit_move_insn (gen_rtx_REG (Pmode, MACL_REG),
		      stack_pointer_rtx);
      emit_move_insn (gen_rtx_REG (SImode, R0_REG),
		      GEN_INT (current_function_args_info.call_cookie));
      emit_move_insn (gen_rtx_REG (SImode, MACH_REG),
		      gen_rtx_REG (SImode, R0_REG));
    }
  else if (TARGET_SHMEDIA)
    {
      int tr = sh_media_register_for_return ();

      if (tr >= 0)
	{
	  rtx insn = emit_move_insn (gen_rtx_REG (DImode, tr),
				     gen_rtx_REG (DImode, PR_MEDIA_REG));

	  /* If this function only exits with sibcalls, this copy
	     will be flagged as dead.  */
	  REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD,
						const0_rtx,
						REG_NOTES (insn));
	}
    }

  /* Emit the code for SETUP_VARARGS.  */
  if (current_function_stdarg)
    {
      /* This is not used by the SH3E calling convention  */
      if (TARGET_SH1 && ! TARGET_SH3E && ! TARGET_SH5 && ! TARGET_HITACHI)
	{
	  /* Push arg regs as if they'd been provided by caller in stack.  */
	  for (i = 0; i < NPARM_REGS(SImode); i++)
	    {
	      int rn = NPARM_REGS(SImode) + FIRST_PARM_REG - i - 1;
	      rtx insn;

	      if (i >= (NPARM_REGS(SImode) 
			- current_function_args_info.arg_count[(int) SH_ARG_INT]
			))
		break;
	      insn = push (rn);
	      RTX_FRAME_RELATED_P (insn) = 0;
	      extra_push += 4;
	    }
	}
    }

  /* If we're supposed to switch stacks at function entry, do so now.  */
  if (sp_switch)
    emit_insn (gen_sp_switch_1 ());

  calc_live_regs (&d, live_regs_mask);
  /* ??? Maybe we could save some switching if we can move a mode switch
     that already happens to be at the function start into the prologue.  */
  if (target_flags != save_flags)
    emit_insn (gen_toggle_sz ());
    
  if (TARGET_SH5)
    {
      int i;
      int offset;
      int align;
      rtx r0 = gen_rtx_REG (Pmode, R0_REG);
      int offset_in_r0 = -1;
      int sp_in_r0 = 0;

      if (d % (STACK_BOUNDARY / BITS_PER_UNIT))
	d_rounding = ((STACK_BOUNDARY / BITS_PER_UNIT)
		      - d % (STACK_BOUNDARY / BITS_PER_UNIT));

      offset = d + d_rounding;
      output_stack_adjust (-offset, stack_pointer_rtx, 1, frame_insn);

      /* We loop twice: first, we save 8-byte aligned registers in the
	 higher addresses, that are known to be aligned.  Then, we
	 proceed to saving 32-bit registers that don't need 8-byte
	 alignment.  */
      for (align = 1; align >= 0; align--)
	for (i = FIRST_PSEUDO_REGISTER - 1; i >= 0; i--)
	  if (live_regs_mask[i/32] & (1 << (i % 32)))
	    {
	      enum machine_mode mode = REGISTER_NATURAL_MODE (i);
	      int reg = i;
	      rtx reg_rtx, mem_rtx, pre_dec = NULL_RTX;

	      if (mode == SFmode && (i % 2) == 1
		  && ! TARGET_FPU_SINGLE && FP_REGISTER_P (i)
		  && (live_regs_mask[(i ^ 1) / 32] & (1 << ((i ^ 1) % 32))))
		{
		  mode = DFmode;
		  i--;
		  reg--;
		}
		
	      /* If we're doing the aligned pass and this is not aligned,
		 or we're doing the unaligned pass and this is aligned,
		 skip it.  */
	      if ((GET_MODE_SIZE (mode) % (STACK_BOUNDARY / BITS_PER_UNIT)
		   == 0) != align)
		continue;

	      offset -= GET_MODE_SIZE (mode);

	      reg_rtx = gen_rtx_REG (mode, reg);

	      mem_rtx = gen_rtx_MEM (mode,
				     gen_rtx_PLUS (Pmode,
						   stack_pointer_rtx,
						   GEN_INT (offset)));

	      GO_IF_LEGITIMATE_ADDRESS (mode, XEXP (mem_rtx, 0), try_pre_dec);

	      mem_rtx = NULL_RTX;

	    try_pre_dec:
	      do
		if (HAVE_PRE_DECREMENT
		    && (offset_in_r0 - offset == GET_MODE_SIZE (mode)
			|| mem_rtx == NULL_RTX
			|| i == PR_REG || SPECIAL_REGISTER_P (i)))
		  {
		    pre_dec = gen_rtx_MEM (mode,
					   gen_rtx_PRE_DEC (Pmode, r0));

		    GO_IF_LEGITIMATE_ADDRESS (mode, XEXP (pre_dec, 0),
					      pre_dec_ok);

		    pre_dec = NULL_RTX;

		    break;

		  pre_dec_ok:
		    mem_rtx = NULL_RTX;
		    offset += GET_MODE_SIZE (mode);
		  }
	      while (0);

	      if (mem_rtx != NULL_RTX)
		goto addr_ok;

	      if (offset_in_r0 == -1)
		{
		  emit_move_insn (r0, GEN_INT (offset));
		  offset_in_r0 = offset;
		}
	      else if (offset != offset_in_r0)
		{
		  emit_move_insn (r0,
				  gen_rtx_PLUS
				  (Pmode, r0,
				   GEN_INT (offset - offset_in_r0)));
		  offset_in_r0 += offset - offset_in_r0;
		}
						  
	      if (pre_dec != NULL_RTX)
		{
		  if (! sp_in_r0)
		    {
		      emit_move_insn (r0,
				      gen_rtx_PLUS
				      (Pmode, r0, stack_pointer_rtx));
		      sp_in_r0 = 1;
		    }

		  offset -= GET_MODE_SIZE (mode);
		  offset_in_r0 -= GET_MODE_SIZE (mode);

		  mem_rtx = pre_dec;
		}
	      else if (sp_in_r0)
		mem_rtx = gen_rtx_MEM (mode, r0);
	      else
		mem_rtx = gen_rtx_MEM (mode,
				       gen_rtx_PLUS (Pmode,
						     stack_pointer_rtx,
						     r0));

	      /* We must not use an r0-based address for target-branch
		 registers or for special registers without pre-dec
		 memory addresses, since we store their values in r0
		 first.  */
	      if (TARGET_REGISTER_P (i)
		  || ((i == PR_REG || SPECIAL_REGISTER_P (i))
		      && mem_rtx != pre_dec))
		abort ();

	    addr_ok:
	      if (TARGET_REGISTER_P (i)
		  || ((i == PR_REG || SPECIAL_REGISTER_P (i))
		      && mem_rtx != pre_dec))
		{
		  rtx r0mode = gen_rtx_REG (GET_MODE (reg_rtx), R0_REG);

		  emit_move_insn (r0mode, reg_rtx);

		  offset_in_r0 = -1;
		  sp_in_r0 = 0;

		  reg_rtx = r0mode;
		}

	      emit_move_insn (mem_rtx, reg_rtx);
	    }

      if (offset != d_rounding)
	abort ();
    }
  else
    push_regs (live_regs_mask);

  if (flag_pic && regs_ever_live[PIC_OFFSET_TABLE_REGNUM])
    {
      rtx insn = get_last_insn ();
      rtx last = emit_insn (gen_GOTaddr2picreg ());

      /* Mark these insns as possibly dead.  Sometimes, flow2 may
	 delete all uses of the PIC register.  In this case, let it
	 delete the initialization too.  */
      do
	{
	  insn = NEXT_INSN (insn);

	  REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD,
						const0_rtx,
						REG_NOTES (insn));
	}
      while (insn != last);
    }

  if (SHMEDIA_REGS_STACK_ADJUST ())
    {
      emit_move_insn (gen_rtx_REG (Pmode, R0_REG),
		      gen_rtx_SYMBOL_REF (Pmode,
					  TARGET_FPU_ANY
					  ? "__GCC_push_shmedia_regs"
					  : "__GCC_push_shmedia_regs_nofpu"));
      /* This must NOT go through the PLT, otherwise mach and macl
	 may be clobbered.  */
      emit_insn (gen_shmedia_save_restore_regs_compact
		 (GEN_INT (-SHMEDIA_REGS_STACK_ADJUST ())));
    }

  if (target_flags != save_flags)
    {
      rtx insn = emit_insn (gen_toggle_sz ());

      /* If we're lucky, a mode switch in the function body will
	 overwrite fpscr, turning this insn dead.  Tell flow this
	 insn is ok to delete.  */
      REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD,
					    const0_rtx,
					    REG_NOTES (insn));
    }

  target_flags = save_flags;

  output_stack_adjust (-rounded_frame_size (d) + d_rounding,
		       stack_pointer_rtx, TARGET_SH5 ? 0 : 1, frame_insn);

  if (frame_pointer_needed)
    frame_insn (GEN_MOV (frame_pointer_rtx, stack_pointer_rtx));

  if (TARGET_SHCOMPACT
      && (current_function_args_info.call_cookie & ~ CALL_COOKIE_RET_TRAMP(1)))
    {
      /* This must NOT go through the PLT, otherwise mach and macl
	 may be clobbered.  */
      emit_move_insn (gen_rtx_REG (Pmode, R0_REG),
		      gen_rtx_SYMBOL_REF (Pmode,
					  "__GCC_shcompact_incoming_args"));
      emit_insn (gen_shcompact_incoming_args ());
    }
}

void
sh_expand_epilogue ()
{
  HOST_WIDE_INT live_regs_mask[(FIRST_PSEUDO_REGISTER + 31) / 32];
  int d, i;
  int d_rounding = 0;

  int save_flags = target_flags;
  int frame_size;

  calc_live_regs (&d, live_regs_mask);

  if (TARGET_SH5 && d % (STACK_BOUNDARY / BITS_PER_UNIT))
    d_rounding = ((STACK_BOUNDARY / BITS_PER_UNIT)
		  - d % (STACK_BOUNDARY / BITS_PER_UNIT));

  frame_size = rounded_frame_size (d) - d_rounding;

  if (frame_pointer_needed)
    {
      output_stack_adjust (frame_size, frame_pointer_rtx, 7, emit_insn);

      /* We must avoid moving the stack pointer adjustment past code
	 which reads from the local frame, else an interrupt could
	 occur after the SP adjustment and clobber data in the local
	 frame.  */
      emit_insn (gen_blockage ());
      emit_insn (GEN_MOV (stack_pointer_rtx, frame_pointer_rtx));
    }
  else if (frame_size)
    {
      /* We must avoid moving the stack pointer adjustment past code
	 which reads from the local frame, else an interrupt could
	 occur after the SP adjustment and clobber data in the local
	 frame.  */
      emit_insn (gen_blockage ());
      output_stack_adjust (frame_size, stack_pointer_rtx, 7, emit_insn);
    }

  if (SHMEDIA_REGS_STACK_ADJUST ())
    {
      emit_move_insn (gen_rtx_REG (Pmode, R0_REG),
		      gen_rtx_SYMBOL_REF (Pmode,
					  TARGET_FPU_ANY
					  ? "__GCC_pop_shmedia_regs"
					  : "__GCC_pop_shmedia_regs_nofpu"));
      /* This must NOT go through the PLT, otherwise mach and macl
	 may be clobbered.  */
      emit_insn (gen_shmedia_save_restore_regs_compact
		 (GEN_INT (SHMEDIA_REGS_STACK_ADJUST ())));
    }

  /* Pop all the registers.  */

  if (target_flags != save_flags)
    emit_insn (gen_toggle_sz ());
  if (TARGET_SH5)
    {
      int offset = d_rounding;
      int offset_in_r0 = -1;
      int sp_in_r0 = 0;
      int align;
      rtx r0 = gen_rtx_REG (Pmode, R0_REG);
      
      /* We loop twice: first, we save 8-byte aligned registers in the
	 higher addresses, that are known to be aligned.  Then, we
	 proceed to saving 32-bit registers that don't need 8-byte
	 alignment.  */
      for (align = 0; align <= 1; align++)
	for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
	  if (live_regs_mask[i/32] & (1 << (i % 32)))
	    {
	      enum machine_mode mode = REGISTER_NATURAL_MODE (i);
	      int reg = i;
	      rtx reg_rtx, mem_rtx, post_inc = NULL_RTX, insn;

	      if (mode == SFmode && (i % 2) == 0
		  && ! TARGET_FPU_SINGLE && FP_REGISTER_P (i)
		  && (live_regs_mask[(i ^ 1) / 32] & (1 << ((i ^ 1) % 32))))
		{
		  mode = DFmode;
		  i++;
		}

	      /* If we're doing the aligned pass and this is not aligned,
		 or we're doing the unaligned pass and this is aligned,
		 skip it.  */
	      if ((GET_MODE_SIZE (mode) % (STACK_BOUNDARY / BITS_PER_UNIT)
		   == 0) != align)
		continue;

	      reg_rtx = gen_rtx_REG (mode, reg);

	      mem_rtx = gen_rtx_MEM (mode,
				     gen_rtx_PLUS (Pmode,
						   stack_pointer_rtx,
						   GEN_INT (offset)));

	      GO_IF_LEGITIMATE_ADDRESS (mode, XEXP (mem_rtx, 0), try_post_inc);

	      mem_rtx = NULL_RTX;

	    try_post_inc:
	      do
		if (HAVE_POST_INCREMENT
		    && (offset == offset_in_r0
			|| (offset + GET_MODE_SIZE (mode) != d + d_rounding
			    && mem_rtx == NULL_RTX)
			|| i == PR_REG || SPECIAL_REGISTER_P (i)))
		  {
		    post_inc = gen_rtx_MEM (mode,
					    gen_rtx_POST_INC (Pmode, r0));

		    GO_IF_LEGITIMATE_ADDRESS (mode, XEXP (post_inc, 0),
					      post_inc_ok);

		    post_inc = NULL_RTX;

		    break;
		    
		  post_inc_ok:
		    mem_rtx = NULL_RTX;
		  }
	      while (0);
	      
	      if (mem_rtx != NULL_RTX)
		goto addr_ok;

	      if (offset_in_r0 == -1)
		{
		  emit_move_insn (r0, GEN_INT (offset));
		  offset_in_r0 = offset;
		}
	      else if (offset != offset_in_r0)
		{
		  emit_move_insn (r0,
				  gen_rtx_PLUS
				  (Pmode, r0,
				   GEN_INT (offset - offset_in_r0)));
		  offset_in_r0 += offset - offset_in_r0;
		}
		  
	      if (post_inc != NULL_RTX)
		{
		  if (! sp_in_r0)
		    {
		      emit_move_insn (r0,
				      gen_rtx_PLUS
				      (Pmode, r0, stack_pointer_rtx));
		      sp_in_r0 = 1;
		    }
		  
		  mem_rtx = post_inc;

		  offset_in_r0 += GET_MODE_SIZE (mode);
		}
	      else if (sp_in_r0)
		mem_rtx = gen_rtx_MEM (mode, r0);
	      else
		mem_rtx = gen_rtx_MEM (mode,
				       gen_rtx_PLUS (Pmode,
						     stack_pointer_rtx,
						     r0));

	      if ((i == PR_REG || SPECIAL_REGISTER_P (i))
		  && mem_rtx != post_inc)
		abort ();

	    addr_ok:
	      if ((i == PR_REG || SPECIAL_REGISTER_P (i))
		  && mem_rtx != post_inc)
		{
		  insn = emit_move_insn (r0, mem_rtx);
		  mem_rtx = r0;
		}
	      else if (TARGET_REGISTER_P (i))
		{
		  rtx r1 = gen_rtx_REG (mode, R1_REG);

		  insn = emit_move_insn (r1, mem_rtx);
		  mem_rtx = r1;
		}

	      insn = emit_move_insn (reg_rtx, mem_rtx);

	      offset += GET_MODE_SIZE (mode);
	    }

      if (offset != d + d_rounding)
	abort ();

      goto finish;
    }
  else
    d = 0;
  if (live_regs_mask[PR_REG / 32] & (1 << (PR_REG % 32)))
    pop (PR_REG);
  for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
    {
      int j = (FIRST_PSEUDO_REGISTER - 1) - i;

      if (j != PR_REG && live_regs_mask[j / 32] & (1 << (j % 32)))
	pop (j);
    }
 finish:
  if (target_flags != save_flags)
    emit_insn (gen_toggle_sz ());
  target_flags = save_flags;

  output_stack_adjust (extra_push + current_function_pretend_args_size
		       + d + d_rounding
		       + current_function_args_info.stack_regs * 8,
		       stack_pointer_rtx, 7, emit_insn);

  /* Switch back to the normal stack if necessary.  */
  if (sp_switch)
    emit_insn (gen_sp_switch_2 ());

  /* Tell flow the insn that pops PR isn't dead.  */
  /* PR_REG will never be live in SHmedia mode, and we don't need to
     USE PR_MEDIA_REG, since it will be explicitly copied to TR0_REG
     by the return pattern.  */
  if (live_regs_mask[PR_REG / 32] & (1 << (PR_REG % 32)))
    emit_insn (gen_rtx_USE (VOIDmode, gen_rtx_REG (SImode, PR_REG)));
}

static int sh_need_epilogue_known = 0;

int
sh_need_epilogue ()
{
  if (! sh_need_epilogue_known)
    {
      rtx epilogue;

      start_sequence ();
      sh_expand_epilogue ();
      epilogue = get_insns ();
      end_sequence ();
      sh_need_epilogue_known = (epilogue == NULL ? -1 : 1);
    }
  return sh_need_epilogue_known > 0;
}

/* Clear variables at function end.  */

static void
sh_output_function_epilogue (file, size)
     FILE *file ATTRIBUTE_UNUSED;
     HOST_WIDE_INT size ATTRIBUTE_UNUSED;
{
  trap_exit = pragma_interrupt = pragma_trapa = pragma_nosave_low_regs = 0;
  sh_need_epilogue_known = 0;
  sp_switch = NULL_RTX;
}

rtx
sh_builtin_saveregs ()
{
  /* First unnamed integer register.  */
  int first_intreg = current_function_args_info.arg_count[(int) SH_ARG_INT];
  /* Number of integer registers we need to save.  */
  int n_intregs = MAX (0, NPARM_REGS (SImode) - first_intreg);
  /* First unnamed SFmode float reg */
  int first_floatreg = current_function_args_info.arg_count[(int) SH_ARG_FLOAT];
  /* Number of SFmode float regs to save.  */
  int n_floatregs = MAX (0, NPARM_REGS (SFmode) - first_floatreg);
  rtx regbuf, fpregs;
  int bufsize, regno;
  HOST_WIDE_INT alias_set;

  if (TARGET_SH5)
    {
      if (n_intregs)
	{
	  int pushregs = n_intregs;

	  while (pushregs < NPARM_REGS (SImode) - 1
		 && (CALL_COOKIE_INT_REG_GET
			(current_function_args_info.call_cookie,
			 NPARM_REGS (SImode) - pushregs)
		     == 1))
	    {
	      current_function_args_info.call_cookie
		&= ~ CALL_COOKIE_INT_REG (NPARM_REGS (SImode)
					  - pushregs, 1);
	      pushregs++;
	    }

	  if (pushregs == NPARM_REGS (SImode))
	    current_function_args_info.call_cookie
	      |= (CALL_COOKIE_INT_REG (0, 1)
		  | CALL_COOKIE_STACKSEQ (pushregs - 1));
	  else
	    current_function_args_info.call_cookie
	      |= CALL_COOKIE_STACKSEQ (pushregs);

	  current_function_pretend_args_size += 8 * n_intregs;
	}
      if (TARGET_SHCOMPACT)
	return const0_rtx;
    }
  
  if (! TARGET_SH3E && ! TARGET_SH4 && ! TARGET_SH5)
    {
      error ("__builtin_saveregs not supported by this subtarget");
      return const0_rtx;
    }

  if (TARGET_SHMEDIA)
    n_floatregs = 0;

  /* Allocate block of memory for the regs.  */
  /* ??? If n_intregs + n_floatregs == 0, should we allocate at least 1 byte?
     Or can assign_stack_local accept a 0 SIZE argument?  */
  bufsize = (n_intregs * UNITS_PER_WORD) + (n_floatregs * UNITS_PER_WORD);

  if (TARGET_SHMEDIA)
    regbuf = gen_rtx_MEM (BLKmode,
			  gen_rtx_REG (Pmode, ARG_POINTER_REGNUM));
  else if (n_floatregs & 1)
    {
      rtx addr;

      regbuf = assign_stack_local (BLKmode, bufsize + UNITS_PER_WORD, 0);
      addr = copy_to_mode_reg (Pmode, XEXP (regbuf, 0));
      emit_insn (gen_iorsi3 (addr, addr, GEN_INT (UNITS_PER_WORD)));
      regbuf = change_address (regbuf, BLKmode, addr);
    }
  else
    regbuf = assign_stack_local (BLKmode, bufsize, 0);
  alias_set = get_varargs_alias_set ();
  set_mem_alias_set (regbuf, alias_set);

  /* Save int args.
     This is optimized to only save the regs that are necessary.  Explicitly
     named args need not be saved.  */
  if (n_intregs > 0)
    move_block_from_reg (BASE_ARG_REG (SImode) + first_intreg,
			 adjust_address (regbuf, BLKmode,
					 n_floatregs * UNITS_PER_WORD),
			 n_intregs, n_intregs * UNITS_PER_WORD);

  if (TARGET_SHMEDIA)
    /* Return the address of the regbuf.  */
    return XEXP (regbuf, 0);

  /* Save float args.
     This is optimized to only save the regs that are necessary.  Explicitly
     named args need not be saved.
     We explicitly build a pointer to the buffer because it halves the insn
     count when not optimizing (otherwise the pointer is built for each reg
     saved).
     We emit the moves in reverse order so that we can use predecrement.  */

  fpregs = gen_reg_rtx (Pmode);
  emit_move_insn (fpregs, XEXP (regbuf, 0));
  emit_insn (gen_addsi3 (fpregs, fpregs,
			 GEN_INT (n_floatregs * UNITS_PER_WORD)));
  if (TARGET_SH4)
    {
      rtx mem;
      for (regno = NPARM_REGS (DFmode) - 2; regno >= first_floatreg; regno -= 2)
	{
	  emit_insn (gen_addsi3 (fpregs, fpregs,
				 GEN_INT (-2 * UNITS_PER_WORD)));
	  mem = gen_rtx_MEM (DFmode, fpregs);
	  set_mem_alias_set (mem, alias_set);
	  emit_move_insn (mem, 
			  gen_rtx (REG, DFmode, BASE_ARG_REG (DFmode) + regno));
	}
      regno = first_floatreg;
      if (regno & 1)
	{
	  emit_insn (gen_addsi3 (fpregs, fpregs, GEN_INT (- UNITS_PER_WORD)));
	  mem = gen_rtx_MEM (SFmode, fpregs);
	  set_mem_alias_set (mem, alias_set);
	  emit_move_insn (mem,
			  gen_rtx (REG, SFmode, BASE_ARG_REG (SFmode) + regno
						- (TARGET_LITTLE_ENDIAN != 0)));
	}
    }
  else
    for (regno = NPARM_REGS (SFmode) - 1; regno >= first_floatreg; regno--)
      {
        rtx mem;

	emit_insn (gen_addsi3 (fpregs, fpregs, GEN_INT (- UNITS_PER_WORD)));
	mem = gen_rtx_MEM (SFmode, fpregs);
	set_mem_alias_set (mem, alias_set);
	emit_move_insn (mem,
			gen_rtx_REG (SFmode, BASE_ARG_REG (SFmode) + regno));
      }

  /* Return the address of the regbuf.  */
  return XEXP (regbuf, 0);
}

/* Define the `__builtin_va_list' type for the ABI.  */

tree
sh_build_va_list ()
{
  tree f_next_o, f_next_o_limit, f_next_fp, f_next_fp_limit, f_next_stack;
  tree record;

  if (TARGET_SH5 || (! TARGET_SH3E && ! TARGET_SH4) || TARGET_HITACHI)
    return ptr_type_node;

  record = make_node (RECORD_TYPE);

  f_next_o = build_decl (FIELD_DECL, get_identifier ("__va_next_o"),
			 ptr_type_node);
  f_next_o_limit = build_decl (FIELD_DECL,
			       get_identifier ("__va_next_o_limit"),
			       ptr_type_node);
  f_next_fp = build_decl (FIELD_DECL, get_identifier ("__va_next_fp"),
			  ptr_type_node);
  f_next_fp_limit = build_decl (FIELD_DECL,
				get_identifier ("__va_next_fp_limit"),
				ptr_type_node);
  f_next_stack = build_decl (FIELD_DECL, get_identifier ("__va_next_stack"),
			     ptr_type_node);

  DECL_FIELD_CONTEXT (f_next_o) = record;
  DECL_FIELD_CONTEXT (f_next_o_limit) = record;
  DECL_FIELD_CONTEXT (f_next_fp) = record;
  DECL_FIELD_CONTEXT (f_next_fp_limit) = record;
  DECL_FIELD_CONTEXT (f_next_stack) = record;

  TYPE_FIELDS (record) = f_next_o;
  TREE_CHAIN (f_next_o) = f_next_o_limit;
  TREE_CHAIN (f_next_o_limit) = f_next_fp;
  TREE_CHAIN (f_next_fp) = f_next_fp_limit;
  TREE_CHAIN (f_next_fp_limit) = f_next_stack;

  layout_type (record);

  return record;
}

/* Implement `va_start' for varargs and stdarg.  */

void
sh_va_start (valist, nextarg)
     tree valist;
     rtx nextarg;
{
  tree f_next_o, f_next_o_limit, f_next_fp, f_next_fp_limit, f_next_stack;
  tree next_o, next_o_limit, next_fp, next_fp_limit, next_stack;
  tree t, u;
  int nfp, nint;

  if (TARGET_SH5)
    {
      expand_builtin_saveregs ();
      std_expand_builtin_va_start (valist, nextarg);
      return;
    }

  if ((! TARGET_SH3E && ! TARGET_SH4) || TARGET_HITACHI)
    {
      std_expand_builtin_va_start (valist, nextarg);
      return;
    }

  f_next_o = TYPE_FIELDS (va_list_type_node);
  f_next_o_limit = TREE_CHAIN (f_next_o);
  f_next_fp = TREE_CHAIN (f_next_o_limit);
  f_next_fp_limit = TREE_CHAIN (f_next_fp);
  f_next_stack = TREE_CHAIN (f_next_fp_limit);

  next_o = build (COMPONENT_REF, TREE_TYPE (f_next_o), valist, f_next_o);
  next_o_limit = build (COMPONENT_REF, TREE_TYPE (f_next_o_limit),
			valist, f_next_o_limit);
  next_fp = build (COMPONENT_REF, TREE_TYPE (f_next_fp), valist, f_next_fp);
  next_fp_limit = build (COMPONENT_REF, TREE_TYPE (f_next_fp_limit),
			 valist, f_next_fp_limit);
  next_stack = build (COMPONENT_REF, TREE_TYPE (f_next_stack),
		      valist, f_next_stack);

  /* Call __builtin_saveregs.  */
  u = make_tree (ptr_type_node, expand_builtin_saveregs ());
  t = build (MODIFY_EXPR, ptr_type_node, next_fp, u);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  nfp = current_function_args_info.arg_count[SH_ARG_FLOAT];
  if (nfp < 8)
    nfp = 8 - nfp;
  else
    nfp = 0;
  u = fold (build (PLUS_EXPR, ptr_type_node, u,
		   build_int_2 (UNITS_PER_WORD * nfp, 0)));
  t = build (MODIFY_EXPR, ptr_type_node, next_fp_limit, u);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  t = build (MODIFY_EXPR, ptr_type_node, next_o, u);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  nint = current_function_args_info.arg_count[SH_ARG_INT];
  if (nint < 4)
    nint = 4 - nint;
  else
    nint = 0;
  u = fold (build (PLUS_EXPR, ptr_type_node, u,
		   build_int_2 (UNITS_PER_WORD * nint, 0)));
  t = build (MODIFY_EXPR, ptr_type_node, next_o_limit, u);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  u = make_tree (ptr_type_node, nextarg);
  t = build (MODIFY_EXPR, ptr_type_node, next_stack, u);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);
}

/* Implement `va_arg'.  */

rtx
sh_va_arg (valist, type)
     tree valist, type;
{
  HOST_WIDE_INT size, rsize;
  tree tmp, pptr_type_node;
  rtx addr_rtx, r;
  rtx result;
  int pass_by_ref = MUST_PASS_IN_STACK (TYPE_MODE (type), type);

  size = int_size_in_bytes (type);
  rsize = (size + UNITS_PER_WORD - 1) & -UNITS_PER_WORD;
  pptr_type_node = build_pointer_type (ptr_type_node);

  if (pass_by_ref)
    type = build_pointer_type (type);

  if (! TARGET_SH5 && (TARGET_SH3E || TARGET_SH4) && ! TARGET_HITACHI)
    {
      tree f_next_o, f_next_o_limit, f_next_fp, f_next_fp_limit, f_next_stack;
      tree next_o, next_o_limit, next_fp, next_fp_limit, next_stack;
      int pass_as_float;
      rtx lab_false, lab_over;

      f_next_o = TYPE_FIELDS (va_list_type_node);
      f_next_o_limit = TREE_CHAIN (f_next_o);
      f_next_fp = TREE_CHAIN (f_next_o_limit);
      f_next_fp_limit = TREE_CHAIN (f_next_fp);
      f_next_stack = TREE_CHAIN (f_next_fp_limit);

      next_o = build (COMPONENT_REF, TREE_TYPE (f_next_o), valist, f_next_o);
      next_o_limit = build (COMPONENT_REF, TREE_TYPE (f_next_o_limit),
			    valist, f_next_o_limit);
      next_fp = build (COMPONENT_REF, TREE_TYPE (f_next_fp),
		       valist, f_next_fp);
      next_fp_limit = build (COMPONENT_REF, TREE_TYPE (f_next_fp_limit),
			     valist, f_next_fp_limit);
      next_stack = build (COMPONENT_REF, TREE_TYPE (f_next_stack),
			  valist, f_next_stack);

      if (TARGET_SH4)
	{
	  pass_as_float = ((TREE_CODE (type) == REAL_TYPE && size <= 8)
			   || (TREE_CODE (type) == COMPLEX_TYPE
			       && TREE_CODE (TREE_TYPE (type)) == REAL_TYPE
			       && size <= 16));
	}
      else
	{
	  pass_as_float = (TREE_CODE (type) == REAL_TYPE && size == 4);
	}

      addr_rtx = gen_reg_rtx (Pmode);
      lab_false = gen_label_rtx ();
      lab_over = gen_label_rtx ();

      if (pass_as_float)
	{
	  int first_floatreg
	    = current_function_args_info.arg_count[(int) SH_ARG_FLOAT];
	  int n_floatregs = MAX (0, NPARM_REGS (SFmode) - first_floatreg);

	  emit_cmp_and_jump_insns (expand_expr (next_fp, NULL_RTX, Pmode,
						EXPAND_NORMAL),
				   expand_expr (next_fp_limit, NULL_RTX,
						Pmode, EXPAND_NORMAL),
				   GE, const1_rtx, Pmode, 1, lab_false);

	  if (TYPE_ALIGN (type) > BITS_PER_WORD
	      || (((TREE_CODE (type) == REAL_TYPE && size == 8) || size == 16)
		  && (n_floatregs & 1)))
	    {
	      tmp = build (BIT_AND_EXPR, ptr_type_node, next_fp,
			   build_int_2 (UNITS_PER_WORD, 0));
	      tmp = build (PLUS_EXPR, ptr_type_node, next_fp, tmp);
	      tmp = build (MODIFY_EXPR, ptr_type_node, next_fp, tmp);
	      TREE_SIDE_EFFECTS (tmp) = 1;
	      expand_expr (tmp, const0_rtx, VOIDmode, EXPAND_NORMAL);
	    }

	  tmp = build1 (ADDR_EXPR, pptr_type_node, next_fp);
	  r = expand_expr (tmp, addr_rtx, Pmode, EXPAND_NORMAL);
	  if (r != addr_rtx)
	    emit_move_insn (addr_rtx, r);

	  emit_jump_insn (gen_jump (lab_over));
	  emit_barrier ();
	  emit_label (lab_false);

	  tmp = build1 (ADDR_EXPR, pptr_type_node, next_stack);
	  r = expand_expr (tmp, addr_rtx, Pmode, EXPAND_NORMAL);
	  if (r != addr_rtx)
	    emit_move_insn (addr_rtx, r);
	}
      else
	{
	  tmp = build (PLUS_EXPR, ptr_type_node, next_o,
		       build_int_2 (rsize, 0));
	  
	  emit_cmp_and_jump_insns (expand_expr (tmp, NULL_RTX, Pmode,
						EXPAND_NORMAL),
				   expand_expr (next_o_limit, NULL_RTX,
						Pmode, EXPAND_NORMAL),
				   GT, const1_rtx, Pmode, 1, lab_false);

	  tmp = build1 (ADDR_EXPR, pptr_type_node, next_o);
	  r = expand_expr (tmp, addr_rtx, Pmode, EXPAND_NORMAL);
	  if (r != addr_rtx)
	    emit_move_insn (addr_rtx, r);

	  emit_jump_insn (gen_jump (lab_over));
	  emit_barrier ();
	  emit_label (lab_false);

	  if (size > 4 && ! TARGET_SH4)
	    {
	      tmp = build (MODIFY_EXPR, ptr_type_node, next_o, next_o_limit);
	      TREE_SIDE_EFFECTS (tmp) = 1;
	      expand_expr (tmp, const0_rtx, VOIDmode, EXPAND_NORMAL);
	    }

	  tmp = build1 (ADDR_EXPR, pptr_type_node, next_stack);
	  r = expand_expr (tmp, addr_rtx, Pmode, EXPAND_NORMAL);
	  if (r != addr_rtx)
	    emit_move_insn (addr_rtx, r);
	}

      emit_label (lab_over);

      tmp = make_tree (pptr_type_node, addr_rtx);
      valist = build1 (INDIRECT_REF, ptr_type_node, tmp);
    }

  /* ??? In va-sh.h, there had been code to make values larger than
     size 8 indirect.  This does not match the FUNCTION_ARG macros.  */

  result = std_expand_builtin_va_arg (valist, type);
  if (pass_by_ref)
    {
#ifdef POINTERS_EXTEND_UNSIGNED
      if (GET_MODE (addr) != Pmode)
	addr = convert_memory_address (Pmode, result);
#endif
      result = gen_rtx_MEM (ptr_mode, force_reg (Pmode, result));
      set_mem_alias_set (result, get_varargs_alias_set ());
    }
  /* ??? expand_builtin_va_arg will also set the alias set of the dereferenced
     argument to the varargs alias set.  */
  return result;
}

/* Define the offset between two registers, one to be eliminated, and
   the other its replacement, at the start of a routine.  */

int
initial_elimination_offset (from, to)
     int from;
     int to;
{
  int regs_saved;
  int regs_saved_rounding = 0;
  int total_saved_regs_space;
  int total_auto_space;
  int save_flags = target_flags;
  int copy_flags;

  HOST_WIDE_INT live_regs_mask[(FIRST_PSEUDO_REGISTER + 31) / 32];
  calc_live_regs (&regs_saved, live_regs_mask);
  regs_saved += SHMEDIA_REGS_STACK_ADJUST ();
  if (TARGET_SH5 && regs_saved % (STACK_BOUNDARY / BITS_PER_UNIT))
    regs_saved_rounding = ((STACK_BOUNDARY / BITS_PER_UNIT)
			   - regs_saved % (STACK_BOUNDARY / BITS_PER_UNIT));

  total_auto_space = rounded_frame_size (regs_saved) - regs_saved_rounding;
  copy_flags = target_flags;
  target_flags = save_flags;

  total_saved_regs_space = regs_saved + regs_saved_rounding;

  if (from == ARG_POINTER_REGNUM && to == FRAME_POINTER_REGNUM)
    return total_saved_regs_space + total_auto_space
      + current_function_args_info.byref_regs * 8;

  if (from == ARG_POINTER_REGNUM && to == STACK_POINTER_REGNUM)
    return total_saved_regs_space + total_auto_space
      + current_function_args_info.byref_regs * 8;

  /* Initial gap between fp and sp is 0.  */
  if (from == FRAME_POINTER_REGNUM && to == STACK_POINTER_REGNUM)
    return 0;

  if (from == RETURN_ADDRESS_POINTER_REGNUM
      && (to == FRAME_POINTER_REGNUM || to == STACK_POINTER_REGNUM))
    {
      if (TARGET_SH5)
	{
	  int i, n = total_saved_regs_space;
	  int align;
	  int pr_reg = TARGET_SHMEDIA ? PR_MEDIA_REG : PR_REG;
	  
	  n += total_auto_space;

	  /* If it wasn't saved, there's not much we can do.  */
	  if ((live_regs_mask[pr_reg / 32] & (1 << (pr_reg % 32))) == 0)
	    return n;

	  target_flags = copy_flags;

	  /* We loop twice: first, check 8-byte aligned registers,
	     that are stored in the higher addresses, that are known
	     to be aligned.  Then, check 32-bit registers that don't
	     need 8-byte alignment.  */
	  for (align = 1; align >= 0; align--)
	    for (i = FIRST_PSEUDO_REGISTER - 1; i >= 0; i--)
	      if (live_regs_mask[i/32] & (1 << (i % 32)))
		{
		  enum machine_mode mode = REGISTER_NATURAL_MODE (i);

		  if (mode == SFmode && (i % 2) == 1
		      && ! TARGET_FPU_SINGLE && FP_REGISTER_P (i)
		      && (live_regs_mask[(i ^ 1) / 32]
			  & (1 << ((i ^ 1) % 32))))
		    {
		      mode = DFmode;
		      i--;
		    }
		
		  /* If we're doing the aligned pass and this is not aligned,
		     or we're doing the unaligned pass and this is aligned,
		     skip it.  */
		  if ((GET_MODE_SIZE (mode) % (STACK_BOUNDARY / BITS_PER_UNIT)
		       == 0) != align)
		    continue;

		  n -= GET_MODE_SIZE (mode);

		  if (i == pr_reg)
		    {
		      target_flags = save_flags;
		      return n;
		    }
		}

	  abort ();
	}
      else
	return total_auto_space;
    }

  abort ();
}

/* Handle machine specific pragmas to be semi-compatible with Hitachi
   compiler.  */

void
sh_pr_interrupt (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  pragma_interrupt = 1;
}

void
sh_pr_trapa (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  pragma_interrupt = pragma_trapa = 1;
}

void
sh_pr_nosave_low_regs (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  pragma_nosave_low_regs = 1;
}

/* Generate 'handle_interrupt' attribute for decls */

static void
sh_insert_attributes (node, attributes)
     tree node;
     tree * attributes;
{
  if (! pragma_interrupt
      || TREE_CODE (node) != FUNCTION_DECL)
    return;

  /* We are only interested in fields.  */
  if (TREE_CODE_CLASS (TREE_CODE (node)) != 'd')
    return;

  /* Add a 'handle_interrupt' attribute.  */
  * attributes = tree_cons (get_identifier ("interrupt_handler"), NULL, * attributes);

  return;
}

/* Supported attributes:

   interrupt_handler -- specifies this function is an interrupt handler.

   sp_switch -- specifies an alternate stack for an interrupt handler
   to run on.

   trap_exit -- use a trapa to exit an interrupt function instead of
   an rte instruction.  */

const struct attribute_spec sh_attribute_table[] =
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req, handler } */
  { "interrupt_handler", 0, 0, true,  false, false, sh_handle_interrupt_handler_attribute },
  { "sp_switch",         1, 1, true,  false, false, sh_handle_sp_switch_attribute },
  { "trap_exit",         1, 1, true,  false, false, sh_handle_trap_exit_attribute },
  { NULL,                0, 0, false, false, false, NULL }
};

/* Handle an "interrupt_handler" attribute; arguments as in
   struct attribute_spec.handler.  */
static tree
sh_handle_interrupt_handler_attribute (node, name, args, flags, no_add_attrs)
     tree *node;
     tree name;
     tree args ATTRIBUTE_UNUSED;
     int flags ATTRIBUTE_UNUSED;
     bool *no_add_attrs;
{
  if (TREE_CODE (*node) != FUNCTION_DECL)
    {
      warning ("`%s' attribute only applies to functions",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }
  else if (TARGET_SHCOMPACT)
    {
      error ("attribute interrupt_handler is not compatible with -m5-compact");
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle an "sp_switch" attribute; arguments as in
   struct attribute_spec.handler.  */
static tree
sh_handle_sp_switch_attribute (node, name, args, flags, no_add_attrs)
     tree *node;
     tree name;
     tree args;
     int flags ATTRIBUTE_UNUSED;
     bool *no_add_attrs;
{
  if (TREE_CODE (*node) != FUNCTION_DECL)
    {
      warning ("`%s' attribute only applies to functions",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }
  else if (!pragma_interrupt)
    {
      /* The sp_switch attribute only has meaning for interrupt functions.  */
      warning ("`%s' attribute only applies to interrupt functions",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }
  else if (TREE_CODE (TREE_VALUE (args)) != STRING_CST)
    {
      /* The argument must be a constant string.  */
      warning ("`%s' attribute argument not a string constant",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }
  else
    {
      sp_switch = gen_rtx_SYMBOL_REF (VOIDmode,
				      TREE_STRING_POINTER (TREE_VALUE (args)));
    }

  return NULL_TREE;
}

/* Handle an "trap_exit" attribute; arguments as in
   struct attribute_spec.handler.  */
static tree
sh_handle_trap_exit_attribute (node, name, args, flags, no_add_attrs)
     tree *node;
     tree name;
     tree args;
     int flags ATTRIBUTE_UNUSED;
     bool *no_add_attrs;
{
  if (TREE_CODE (*node) != FUNCTION_DECL)
    {
      warning ("`%s' attribute only applies to functions",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }
  else if (!pragma_interrupt)
    {
      /* The trap_exit attribute only has meaning for interrupt functions.  */
      warning ("`%s' attribute only applies to interrupt functions",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }
  else if (TREE_CODE (TREE_VALUE (args)) != INTEGER_CST)
    {
      /* The argument must be a constant integer.  */
      warning ("`%s' attribute argument not an integer constant",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }
  else
    {
      trap_exit = TREE_INT_CST_LOW (TREE_VALUE (args));
    }

  return NULL_TREE;
}

int
sh_cfun_interrupt_handler_p ()
{
  return (lookup_attribute ("interrupt_handler",
			    DECL_ATTRIBUTES (current_function_decl))
	  != NULL_TREE);
}

/* Predicates used by the templates.  */

/* Returns 1 if OP is MACL, MACH or PR.  The input must be a REG rtx.
   Used only in general_movsrc_operand.  */

int
system_reg_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  switch (REGNO (op))
    {
    case PR_REG:
    case MACL_REG:
    case MACH_REG:
      return 1;
    }
  return 0;
}

/* Returns 1 if OP can be source of a simple move operation.
   Same as general_operand, but a LABEL_REF is valid, PRE_DEC is
   invalid as are subregs of system registers.  */

int
general_movsrc_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) == MEM)
    {
      rtx inside = XEXP (op, 0);
      if (GET_CODE (inside) == CONST)
	inside = XEXP (inside, 0);

      if (GET_CODE (inside) == LABEL_REF)
	return 1;

      if (GET_CODE (inside) == PLUS
	  && GET_CODE (XEXP (inside, 0)) == LABEL_REF
	  && GET_CODE (XEXP (inside, 1)) == CONST_INT)
	return 1;

      /* Only post inc allowed.  */
      if (GET_CODE (inside) == PRE_DEC)
	return 0;
    }

  if ((mode == QImode || mode == HImode)
      && (GET_CODE (op) == SUBREG
	  && GET_CODE (XEXP (op, 0)) == REG
	  && system_reg_operand (XEXP (op, 0), mode)))
    return 0;

  return general_operand (op, mode);
}

/* Returns 1 if OP can be a destination of a move.
   Same as general_operand, but no preinc allowed.  */

int
general_movdst_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  /* Only pre dec allowed.  */
  if (GET_CODE (op) == MEM && GET_CODE (XEXP (op, 0)) == POST_INC)
    return 0;

  return general_operand (op, mode);
}

/* Returns 1 if OP is a normal arithmetic register.  */

int
arith_reg_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (register_operand (op, mode))
    {
      int regno;

      if (GET_CODE (op) == REG)
	regno = REGNO (op);
      else if (GET_CODE (op) == SUBREG && GET_CODE (SUBREG_REG (op)) == REG)
	regno = REGNO (SUBREG_REG (op));
      else
	return 1;

      return (regno != T_REG && regno != PR_REG
	      && ! TARGET_REGISTER_P (regno)
	      && (regno != FPUL_REG || TARGET_SH4)
	      && regno != MACH_REG && regno != MACL_REG);
    }
  return 0;
}

/* Like above, but for DImode destinations: forbid paradoxical DImode subregs,
   because this would lead to missing sign extensions when truncating from
   DImode to SImode.  */
int
arith_reg_dest (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (mode == DImode && GET_CODE (op) == SUBREG
      && GET_MODE_SIZE (GET_MODE (SUBREG_REG (op))) < 8)
    return 0;
  return arith_reg_operand (op, mode);
}

int
int_gpr_dest (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  enum machine_mode op_mode = GET_MODE (op);

  if (GET_MODE_CLASS (op_mode) != MODE_INT
      || GET_MODE_SIZE (op_mode) >= UNITS_PER_WORD)
    return 0;
  if (! reload_completed)
    return 0;
  return true_regnum (op) <= LAST_GENERAL_REG;
}

int
fp_arith_reg_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (register_operand (op, mode))
    {
      int regno;

      if (GET_CODE (op) == REG)
	regno = REGNO (op);
      else if (GET_CODE (op) == SUBREG && GET_CODE (SUBREG_REG (op)) == REG)
	regno = REGNO (SUBREG_REG (op));
      else
	return 1;

      return (regno >= FIRST_PSEUDO_REGISTER
	      || FP_REGISTER_P (regno));
    }
  return 0;
}

/* Returns 1 if OP is a valid source operand for an arithmetic insn.  */

int
arith_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (arith_reg_operand (op, mode))
    return 1;

  if (TARGET_SHMEDIA)
    {
      /* FIXME: We should be checking whether the CONST_INT fits in a
	 CONST_OK_FOR_J here, but this causes reload_cse to crash when
	 attempting to transform a sequence of two 64-bit sets of the
	 same register from literal constants into a set and an add,
	 when the difference is too wide for an add.  */
      if (GET_CODE (op) == CONST_INT
	  || EXTRA_CONSTRAINT_S (op))
	return 1;
      else
	return 0;
    }
  else if (GET_CODE (op) == CONST_INT && CONST_OK_FOR_I (INTVAL (op)))
    return 1;

  return 0;
}

/* Returns 1 if OP is a valid source operand for a compare insn.  */

int
arith_reg_or_0_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (arith_reg_operand (op, mode))
    return 1;

  if (EXTRA_CONSTRAINT_U (op))
    return 1;

  return 0;
}

/* Return 1 if OP is a valid source operand for an SHmedia operation
   that takes either a register or a 6-bit immediate.  */

int
shmedia_6bit_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (arith_reg_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT && CONST_OK_FOR_O (INTVAL (op))));
}

/* Returns 1 if OP is a valid source operand for a logical operation.  */

int
logical_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (arith_reg_operand (op, mode))
    return 1;

  if (TARGET_SHMEDIA)
    {
      if (GET_CODE (op) == CONST_INT && CONST_OK_FOR_P (INTVAL (op)))
	return 1;
      else
	return 0;
    }
  else if (GET_CODE (op) == CONST_INT && CONST_OK_FOR_L (INTVAL (op)))
    return 1;

  return 0;
}

int
and_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (logical_operand (op, mode))
    return 1;

  /* Check mshflo.l / mshflhi.l opportunities.  */
  if (TARGET_SHMEDIA
      && mode == DImode
      && GET_CODE (op) == CONST_INT
      && (INTVAL (op) == (unsigned) 0xffffffff
	  || INTVAL (op) == (HOST_WIDE_INT) -1 << 32))
	return 1;

  return 0;
}

/* Nonzero if OP is a floating point value with value 0.0.  */

int
fp_zero_operand (op)
     rtx op;
{
  REAL_VALUE_TYPE r;

  if (GET_MODE (op) != SFmode)
    return 0;

  REAL_VALUE_FROM_CONST_DOUBLE (r, op);
  return REAL_VALUES_EQUAL (r, dconst0) && ! REAL_VALUE_MINUS_ZERO (r);
}

/* Nonzero if OP is a floating point value with value 1.0.  */

int
fp_one_operand (op)
     rtx op;
{
  REAL_VALUE_TYPE r;

  if (GET_MODE (op) != SFmode)
    return 0;

  REAL_VALUE_FROM_CONST_DOUBLE (r, op);
  return REAL_VALUES_EQUAL (r, dconst1);
}

/* For -m4 and -m4-single-only, mode switching is used.  If we are
   compiling without -mfmovd, movsf_ie isn't taken into account for
   mode switching.  We could check in machine_dependent_reorg for
   cases where we know we are in single precision mode, but there is
   interface to find that out during reload, so we must avoid
   choosing an fldi alternative during reload and thus failing to
   allocate a scratch register for the constant loading.  */
int
fldi_ok ()
{
  return ! TARGET_SH4 || TARGET_FMOVD || reload_completed;
}

int
tertiary_reload_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  enum rtx_code code = GET_CODE (op);
  return code == MEM || (TARGET_SH4 && code == CONST_DOUBLE);
}

int
fpscr_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == REG && REGNO (op) == FPSCR_REG
	  && GET_MODE (op) == PSImode);
}

int
fpul_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (TARGET_SHMEDIA)
    return fp_arith_reg_operand (op, mode);

  return (GET_CODE (op) == REG
	  && (REGNO (op) == FPUL_REG || REGNO (op) >= FIRST_PSEUDO_REGISTER)
	  && GET_MODE (op) == mode);
}

int
symbol_ref_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == SYMBOL_REF);
}

int
commutative_float_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_MODE (op) != mode)
    return 0;
  switch (GET_CODE (op))
    {
    case PLUS:
    case MULT:
      return 1;
    default:
      break;
    }
  return 0;
}

int
noncommutative_float_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_MODE (op) != mode)
    return 0;
  switch (GET_CODE (op))
    {
    case MINUS:
    case DIV:
      return 1;
    default:
      break;
    }
  return 0;
}

int
unary_float_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_MODE (op) != mode)
    return 0;
  switch (GET_CODE (op))
    {
    case ABS:
    case NEG:
    case SQRT:
      return 1;
    default:
      break;
    }
  return 0;
}

int
binary_float_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_MODE (op) != mode)
    return 0;
  switch (GET_CODE (op))
    {
    case PLUS:
    case MINUS:
    case MULT:
    case DIV:
      return 1;
    default:
      break;
    }
  return 0;
}

int
binary_logical_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_MODE (op) != mode)
    return 0;
  switch (GET_CODE (op))
    {
    case IOR:
    case AND:
    case XOR:
      return 1;
    default:
      break;
    }
  return 0;
}

int
equality_comparison_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return ((mode == VOIDmode || GET_MODE (op) == mode)
	  && (GET_CODE (op) == EQ || GET_CODE (op) == NE));
}

int greater_comparison_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (mode != VOIDmode && GET_MODE (op) == mode)
    return 0;
  switch (GET_CODE (op))
    {
    case GT:
    case GE:
    case GTU:
    case GEU:
      return 1;
    default:
      return 0;
    }
}

int less_comparison_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (mode != VOIDmode && GET_MODE (op) == mode)
    return 0;
  switch (GET_CODE (op))
    {
    case LT:
    case LE:
    case LTU:
    case LEU:
      return 1;
    default:
      return 0;
    }
}

/* Accept pseudos and branch target registers.  */
int
target_reg_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (mode != DImode
      || GET_MODE (op) != DImode)
    return 0;

  if (GET_CODE (op) == SUBREG)
    op = XEXP (op, 0);

  if (GET_CODE (op) != REG)
    return 0;

  /* We must protect ourselves from matching pseudos that are virtual
     register, because they will eventually be replaced with hardware
     registers that aren't branch-target registers.  */
  if (REGNO (op) > LAST_VIRTUAL_REGISTER
      || TARGET_REGISTER_P (REGNO (op)))
    return 1;

  return 0;
}

/* Same as target_reg_operand, except that label_refs and symbol_refs
   are accepted before reload.  */
int
target_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (mode != DImode)
    return 0;

  if ((GET_MODE (op) == DImode || GET_MODE (op) == VOIDmode)
      && EXTRA_CONSTRAINT_T (op))
    return ! reload_completed;

  return target_reg_operand (op, mode);
}

int
mextr_bit_offset (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  HOST_WIDE_INT i;

  if (GET_CODE (op) != CONST_INT)
    return 0;
  i = INTVAL (op);
  return i >= 1*8 && i <= 7*8 && (i & 7) == 0;
}

int
extend_reg_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == TRUNCATE
	  ? arith_operand
	  : arith_reg_operand) (op, mode);
}

int
trunc_hi_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  enum machine_mode op_mode = GET_MODE (op);

  if (op_mode != SImode && op_mode != DImode
      && op_mode != V4HImode && op_mode != V2SImode)
    return 0;
  return extend_reg_operand (op, mode);
}

int
extend_reg_or_0_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == TRUNCATE
	  ? arith_operand
	  : arith_reg_or_0_operand) (op, mode);
}

int
general_extend_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (GET_CODE (op) == TRUNCATE
	  ? arith_operand
	  : nonimmediate_operand) (op, mode);
}

int
inqhi_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) != TRUNCATE || mode != GET_MODE (op))
    return 0;
  op = XEXP (op, 0);
  /* Can't use true_regnum here because copy_cost wants to know about
     SECONDARY_INPUT_RELOAD_CLASS.  */
  return GET_CODE (op) == REG && FP_REGISTER_P (REGNO (op));
}

int
sh_rep_vec (v, mode)
     rtx v;
     enum machine_mode mode;
{
  int i;
  rtx x, y;

  if ((GET_CODE (v) != CONST_VECTOR && GET_CODE (v) != PARALLEL)
      || (GET_MODE (v) != mode && mode != VOIDmode))
    return 0;
  i = XVECLEN (v, 0) - 2;
  x = XVECEXP (v, 0, i + 1);
  if (GET_MODE_UNIT_SIZE (mode) == 1)
    {
      y = XVECEXP (v, 0, i);
      for (i -= 2 ; i >= 0; i -= 2)
	if (! rtx_equal_p (XVECEXP (v, 0, i + 1), x)
	    || ! rtx_equal_p (XVECEXP (v, 0, i), y))
	  return 0;
    }
  else
    for (; i >= 0; i--)
      if (XVECEXP (v, 0, i) != x)
	return 0;
  return 1;
}

/* Determine if V is a constant vector matching MODE with only one element
   that is not a sign extension.  Two byte-sized elements count as one.  */
int
sh_1el_vec (v, mode)
     rtx v;
     enum machine_mode mode;
{
  int unit_size;
  int i, last, least, sign_ix;
  rtx sign;

  if (GET_CODE (v) != CONST_VECTOR
      || (GET_MODE (v) != mode && mode != VOIDmode))
    return 0;
  /* Determine numbers of last and of least significat elements.  */
  last = XVECLEN (v, 0) - 1;
  least = TARGET_LITTLE_ENDIAN ? 0 : last;
  if (GET_CODE (XVECEXP (v, 0, least)) != CONST_INT)
    return 0;
  sign_ix = least;
  if (GET_MODE_UNIT_SIZE (mode) == 1)
    sign_ix = TARGET_LITTLE_ENDIAN ? 1 : last - 1;
  if (GET_CODE (XVECEXP (v, 0, sign_ix)) != CONST_INT)
    return 0;
  unit_size = GET_MODE_UNIT_SIZE (GET_MODE (v));
  sign = (INTVAL (XVECEXP (v, 0, sign_ix)) >> (unit_size * BITS_PER_UNIT - 1)
	  ? constm1_rtx : const0_rtx);
  i = XVECLEN (v, 0) - 1;
  do
    if (i != least && i != sign_ix && XVECEXP (v, 0, i) != sign)
      return 0;
  while (--i);
  return 1;
}

int
sh_const_vec (v, mode)
     rtx v;
     enum machine_mode mode;
{
  int i;

  if (GET_CODE (v) != CONST_VECTOR
      || (GET_MODE (v) != mode && mode != VOIDmode))
    return 0;
  i = XVECLEN (v, 0) - 1;
  for (; i >= 0; i--)
    if (GET_CODE (XVECEXP (v, 0, i)) != CONST_INT)
      return 0;
  return 1;
}

/* Return the destination address of a branch.  */
   
static int
branch_dest (branch)
     rtx branch;
{
  rtx dest = SET_SRC (PATTERN (branch));
  int dest_uid;

  if (GET_CODE (dest) == IF_THEN_ELSE)
    dest = XEXP (dest, 1);
  dest = XEXP (dest, 0);
  dest_uid = INSN_UID (dest);
  return INSN_ADDRESSES (dest_uid);
}

/* Return nonzero if REG is not used after INSN.
   We assume REG is a reload reg, and therefore does
   not live past labels.  It may live past calls or jumps though.  */
int
reg_unused_after (reg, insn)
     rtx reg;
     rtx insn;
{
  enum rtx_code code;
  rtx set;

  /* If the reg is set by this instruction, then it is safe for our
     case.  Disregard the case where this is a store to memory, since
     we are checking a register used in the store address.  */
  set = single_set (insn);
  if (set && GET_CODE (SET_DEST (set)) != MEM
      && reg_overlap_mentioned_p (reg, SET_DEST (set)))
    return 1;

  while ((insn = NEXT_INSN (insn)))
    {
      code = GET_CODE (insn);

#if 0
      /* If this is a label that existed before reload, then the register
	 if dead here.  However, if this is a label added by reorg, then
	 the register may still be live here.  We can't tell the difference,
	 so we just ignore labels completely.  */
      if (code == CODE_LABEL)
	return 1;
      /* else */
#endif

      if (code == JUMP_INSN)
	return 0;

      /* If this is a sequence, we must handle them all at once.
	 We could have for instance a call that sets the target register,
	 and an insn in a delay slot that uses the register.  In this case,
	 we must return 0.  */
      else if (code == INSN && GET_CODE (PATTERN (insn)) == SEQUENCE)
	{
	  int i;
	  int retval = 0;

	  for (i = 0; i < XVECLEN (PATTERN (insn), 0); i++)
	    {
	      rtx this_insn = XVECEXP (PATTERN (insn), 0, i);
	      rtx set = single_set (this_insn);

	      if (GET_CODE (this_insn) == CALL_INSN)
		code = CALL_INSN;
	      else if (GET_CODE (this_insn) == JUMP_INSN)
		{
		  if (INSN_ANNULLED_BRANCH_P (this_insn))
		    return 0;
		  code = JUMP_INSN;
		}

	      if (set && reg_overlap_mentioned_p (reg, SET_SRC (set)))
		return 0;
	      if (set && reg_overlap_mentioned_p (reg, SET_DEST (set)))
		{
		  if (GET_CODE (SET_DEST (set)) != MEM)
		    retval = 1;
		  else
		    return 0;
		}
	      if (set == 0
		  && reg_overlap_mentioned_p (reg, PATTERN (this_insn)))
		return 0;
	    }
	  if (retval == 1)
	    return 1;
	  else if (code == JUMP_INSN)
	    return 0;
	}
      else if (GET_RTX_CLASS (code) == 'i')
	{
	  rtx set = single_set (insn);

	  if (set && reg_overlap_mentioned_p (reg, SET_SRC (set)))
	    return 0;
	  if (set && reg_overlap_mentioned_p (reg, SET_DEST (set)))
	    return GET_CODE (SET_DEST (set)) != MEM;
	  if (set == 0 && reg_overlap_mentioned_p (reg, PATTERN (insn)))
	    return 0;
	}

      if (code == CALL_INSN && call_used_regs[REGNO (reg)])
	return 1;
    }
  return 1;
}

#include "ggc.h"

static GTY(()) rtx fpscr_rtx;
rtx
get_fpscr_rtx ()
{
  if (! fpscr_rtx)
    {
      fpscr_rtx = gen_rtx (REG, PSImode, FPSCR_REG);
      REG_USERVAR_P (fpscr_rtx) = 1;
      mark_user_reg (fpscr_rtx);
    }
  if (! reload_completed || mdep_reorg_phase != SH_AFTER_MDEP_REORG)
    mark_user_reg (fpscr_rtx);
  return fpscr_rtx;
}

void
emit_sf_insn (pat)
     rtx pat;
{
  emit_insn (pat);
}

void
emit_df_insn (pat)
     rtx pat;
{
  emit_insn (pat);
}

void
expand_sf_unop (fun, operands)
     rtx (*fun) PARAMS ((rtx, rtx, rtx));
     rtx *operands;
{
  emit_sf_insn ((*fun) (operands[0], operands[1], get_fpscr_rtx ()));
}

void
expand_sf_binop (fun, operands)
     rtx (*fun) PARAMS ((rtx, rtx, rtx, rtx));
     rtx *operands;
{
  emit_sf_insn ((*fun) (operands[0], operands[1], operands[2],
			 get_fpscr_rtx ()));
}

void
expand_df_unop (fun, operands)
     rtx (*fun) PARAMS ((rtx, rtx, rtx));
     rtx *operands;
{
  emit_df_insn ((*fun) (operands[0], operands[1], get_fpscr_rtx ()));
}

void
expand_df_binop (fun, operands)
     rtx (*fun) PARAMS ((rtx, rtx, rtx, rtx));
     rtx *operands;
{
  emit_df_insn ((*fun) (operands[0], operands[1], operands[2],
			 get_fpscr_rtx ()));
}

/* ??? gcc does flow analysis strictly after common subexpression
   elimination.  As a result, common subespression elimination fails
   when there are some intervening statements setting the same register.
   If we did nothing about this, this would hurt the precision switching
   for SH4 badly.  There is some cse after reload, but it is unable to
   undo the extra register pressure from the unused instructions, and
   it cannot remove auto-increment loads.

   A C code example that shows this flow/cse weakness for (at least) SH
   and sparc (as of gcc ss-970706) is this:

double
f(double a)
{
  double d;
  d = 0.1;
  a += d;
  d = 1.1;
  d = 0.1;
  a *= d;
  return a;
}

   So we add another pass before common subexpression elimination, to
   remove assignments that are dead due to a following assignment in the
   same basic block.  */

static void 
mark_use (x, reg_set_block)
     rtx x, *reg_set_block;
{
  enum rtx_code code;

  if (! x)
    return;
  code = GET_CODE (x);
  switch (code)
    {
    case REG:
      {
	int regno = REGNO (x);
	int nregs = (regno < FIRST_PSEUDO_REGISTER
		     ? HARD_REGNO_NREGS (regno, GET_MODE (x))
		     : 1);
	do
	  {
	    reg_set_block[regno + nregs - 1] = 0;
	  }
	while (--nregs);
	break;
      }
    case SET:
      {
	rtx dest = SET_DEST (x);

	if (GET_CODE (dest) == SUBREG)
	  dest = SUBREG_REG (dest);
	if (GET_CODE (dest) != REG)
	  mark_use (dest, reg_set_block);
	mark_use (SET_SRC (x), reg_set_block);
	break;
      }
    case CLOBBER:
      break;
    default:
      {
	const char *fmt = GET_RTX_FORMAT (code);
	int i, j;
	for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
	  {
	    if (fmt[i] == 'e')
	      mark_use (XEXP (x, i), reg_set_block);
	    else if (fmt[i] == 'E')
	      for (j = XVECLEN (x, i) - 1; j >= 0; j--)
		mark_use (XVECEXP (x, i, j), reg_set_block);
	  }
	break;
      }
    }
}

static rtx get_free_reg PARAMS ((HARD_REG_SET));

/* This function returns a register to use to load the address to load
   the fpscr from.  Currently it always returns r1 or r7, but when we are
   able to use pseudo registers after combine, or have a better mechanism
   for choosing a register, it should be done here.  */
/* REGS_LIVE is the liveness information for the point for which we
   need this allocation.  In some bare-bones exit blocks, r1 is live at the
   start.  We can even have all of r0..r3 being live:
__complex__ long long f (double d) { if (d == 0) return 2; else return 3; }
   INSN before which new insns are placed with will clobber the register
   we return.  If a basic block consists only of setting the return value
   register to a pseudo and using that register, the return value is not
   live before or after this block, yet we we'll insert our insns right in
   the middle.  */

static rtx
get_free_reg (regs_live)
     HARD_REG_SET regs_live;
{
  if (! TEST_HARD_REG_BIT (regs_live, 1))
    return gen_rtx_REG (Pmode, 1);

  /* Hard reg 1 is live; since this is a SMALL_REGISTER_CLASSES target,
     there shouldn't be anything but a jump before the function end.  */
  if (! TEST_HARD_REG_BIT (regs_live, 7))
    return gen_rtx_REG (Pmode, 7);

  abort ();
}

/* This function will set the fpscr from memory. 
   MODE is the mode we are setting it to.  */
void
fpscr_set_from_mem (mode, regs_live)
     int mode;
     HARD_REG_SET regs_live;
{
  enum attr_fp_mode fp_mode = mode;
  rtx addr_reg = get_free_reg (regs_live);

  if (fp_mode == (enum attr_fp_mode) NORMAL_MODE (FP_MODE))
    emit_insn (gen_fpu_switch1 (addr_reg));
  else
    emit_insn (gen_fpu_switch0 (addr_reg));
}

/* Is the given character a logical line separator for the assembler?  */
#ifndef IS_ASM_LOGICAL_LINE_SEPARATOR
#define IS_ASM_LOGICAL_LINE_SEPARATOR(C) ((C) == ';')
#endif

int
sh_insn_length_adjustment (insn)
     rtx insn;
{
  /* Instructions with unfilled delay slots take up an extra two bytes for
     the nop in the delay slot.  */
  if (((GET_CODE (insn) == INSN
	&& GET_CODE (PATTERN (insn)) != USE
	&& GET_CODE (PATTERN (insn)) != CLOBBER)
       || GET_CODE (insn) == CALL_INSN
       || (GET_CODE (insn) == JUMP_INSN
	   && GET_CODE (PATTERN (insn)) != ADDR_DIFF_VEC
	   && GET_CODE (PATTERN (insn)) != ADDR_VEC))
      && GET_CODE (PATTERN (NEXT_INSN (PREV_INSN (insn)))) != SEQUENCE
      && get_attr_needs_delay_slot (insn) == NEEDS_DELAY_SLOT_YES)
    return 2;

  /* sh-dsp parallel processing insn take four bytes instead of two.  */
     
  if (GET_CODE (insn) == INSN)
    {
      int sum = 0;
      rtx body = PATTERN (insn);
      const char *template;
      char c;
      int maybe_label = 1;

      if (GET_CODE (body) == ASM_INPUT)
	template = XSTR (body, 0);
      else if (asm_noperands (body) >= 0)
	template
	  = decode_asm_operands (body, NULL, NULL, NULL, NULL);
      else
	return 0;
      do
	{
	  int ppi_adjust = 0;

	  do
	    c = *template++;
	  while (c == ' ' || c == '\t');
	  /* all sh-dsp parallel-processing insns start with p.
	     The only non-ppi sh insn starting with p is pref.
	     The only ppi starting with pr is prnd.  */
	  if ((c == 'p' || c == 'P') && strncasecmp ("re", template, 2))
	    ppi_adjust = 2;
	  /* The repeat pseudo-insn expands two three insns, a total of
	     six bytes in size.  */
	  else if ((c == 'r' || c == 'R')
		   && ! strncasecmp ("epeat", template, 5))
	    ppi_adjust = 4;
	  while (c && c != '\n' && ! IS_ASM_LOGICAL_LINE_SEPARATOR (c))
	    {
	      /* If this is a label, it is obviously not a ppi insn.  */
	      if (c == ':' && maybe_label)
		{
		  ppi_adjust = 0;
		  break;
		}
	      else if (c == '\'' || c == '"')
		maybe_label = 0;
	      c = *template++;
	    }
	  sum += ppi_adjust;
	  maybe_label = c != ':';
	}
      while (c);
      return sum;
    }
  return 0;
}

/* Return TRUE if X references a SYMBOL_REF or LABEL_REF whose symbol
   isn't protected by a PIC unspec.  */
int
nonpic_symbol_mentioned_p (x)
     rtx x;
{
  register const char *fmt;
  register int i;

  if (GET_CODE (x) == SYMBOL_REF || GET_CODE (x) == LABEL_REF
      || GET_CODE (x) == PC)
    return 1;

  /* We don't want to look into the possible MEM location of a
     CONST_DOUBLE, since we're not going to use it, in general.  */
  if (GET_CODE (x) == CONST_DOUBLE)
    return 0;

  if (GET_CODE (x) == UNSPEC
      && (XINT (x, 1) == UNSPEC_PIC
	  || XINT (x, 1) == UNSPEC_GOT
	  || XINT (x, 1) == UNSPEC_GOTOFF
	  || XINT (x, 1) == UNSPEC_GOTPLT
	  || XINT (x, 1) == UNSPEC_PLT))
      return 0;

  fmt = GET_RTX_FORMAT (GET_CODE (x));
  for (i = GET_RTX_LENGTH (GET_CODE (x)) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'E')
	{
	  register int j;

	  for (j = XVECLEN (x, i) - 1; j >= 0; j--)
	    if (nonpic_symbol_mentioned_p (XVECEXP (x, i, j)))
	      return 1;
	}
      else if (fmt[i] == 'e' && nonpic_symbol_mentioned_p (XEXP (x, i)))
	return 1;
    }

  return 0;
}

/* Convert a non-PIC address in `orig' to a PIC address using @GOT or
   @GOTOFF in `reg'.  */
rtx
legitimize_pic_address (orig, mode, reg)
     rtx orig;
     enum machine_mode mode ATTRIBUTE_UNUSED;
     rtx reg;
{
  if (GET_CODE (orig) == LABEL_REF
      || (GET_CODE (orig) == SYMBOL_REF
	  && (CONSTANT_POOL_ADDRESS_P (orig)
	      /* SYMBOL_REF_FLAG is set on static symbols.  */
	      || SYMBOL_REF_FLAG (orig))))
    {
      if (reg == 0)
	reg = gen_reg_rtx (Pmode);

      emit_insn (gen_symGOTOFF2reg (reg, orig));
      return reg;
    }
  else if (GET_CODE (orig) == SYMBOL_REF)
    {
      if (reg == 0)
	reg = gen_reg_rtx (Pmode);

      emit_insn (gen_symGOT2reg (reg, orig));
      return reg;
    }
  return orig;
}

/* Mark the use of a constant in the literal table. If the constant
   has multiple labels, make it unique.  */
static rtx
mark_constant_pool_use (x)
     rtx x;
{
  rtx insn, lab, pattern;

  if (x == NULL)
    return x;

  switch (GET_CODE (x))
    {
    case LABEL_REF:
      x = XEXP (x, 0);
    case CODE_LABEL:
      break;
    default:
      return x;
    }

  /* Get the first label in the list of labels for the same constant
     and delete another labels in the list.  */
  lab = x;
  for (insn = PREV_INSN (x); insn; insn = PREV_INSN (insn))
    {
      if (GET_CODE (insn) != CODE_LABEL
	  || LABEL_REFS (insn) != NEXT_INSN (insn))
	break;
      lab = insn;
    }

  for (insn = LABEL_REFS (lab); insn; insn = LABEL_REFS (insn))
    INSN_DELETED_P (insn) = 1;

  /* Mark constants in a window.  */
  for (insn = NEXT_INSN (x); insn; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) != INSN)
	continue;

      pattern = PATTERN (insn);
      if (GET_CODE (pattern) != UNSPEC_VOLATILE)
	continue;

      switch (XINT (pattern, 1))
	{
	case UNSPECV_CONST2:
	case UNSPECV_CONST4:
	case UNSPECV_CONST8:
	  XVECEXP (pattern, 0, 1) = const1_rtx;
	  break;
	case UNSPECV_WINDOW_END:
	  if (XVECEXP (pattern, 0, 0) == x)
	    return lab;
	  break;
	case UNSPECV_CONST_END:
	  return lab;
	default:
	  break;
	}
    }

  return lab;
}

/* Return true if it's possible to redirect BRANCH1 to the destination
   of an unconditional jump BRANCH2.  We only want to do this if the
   resulting branch will have a short displacement.  */
int 
sh_can_redirect_branch (branch1, branch2)
     rtx branch1;
     rtx branch2;
{
  if (flag_expensive_optimizations && simplejump_p (branch2))
    {
      rtx dest = XEXP (SET_SRC (single_set (branch2)), 0);
      rtx insn;
      int distance;
      
      for (distance = 0, insn = NEXT_INSN (branch1); 
	   insn && distance < 256; 
	   insn = PREV_INSN (insn))
	{
	  if (insn == dest)    
	    return 1;
	  else
	    distance += get_attr_length (insn);
	}
      for (distance = 0, insn = NEXT_INSN (branch1); 
	   insn && distance < 256; 
	   insn = NEXT_INSN (insn))
	{
	  if (insn == dest)    
	    return 1;
	  else
	    distance += get_attr_length (insn);
	}
    }
  return 0;
}

/* Return nonzero if register old_reg can be renamed to register new_reg.  */
int
sh_hard_regno_rename_ok (old_reg, new_reg)
     unsigned int old_reg ATTRIBUTE_UNUSED;
     unsigned int new_reg;
{

/* Interrupt functions can only use registers that have already been
   saved by the prologue, even if they would normally be
   call-clobbered.  */

  if (sh_cfun_interrupt_handler_p () && !regs_ever_live[new_reg])
     return 0;

   return 1;
}

/* Function to update the integer COST
   based on the relationship between INSN that is dependent on
   DEP_INSN through the dependence LINK.  The default is to make no
   adjustment to COST.  This can be used for example to specify to
   the scheduler that an output- or anti-dependence does not incur
   the same cost as a data-dependence.  The return value should be
   the new value for COST.  */
static int
sh_adjust_cost (insn, link, dep_insn, cost)
     rtx insn;
     rtx link ATTRIBUTE_UNUSED;
     rtx dep_insn;
     int cost;
{
  rtx reg, use_pat;

  if (TARGET_SHMEDIA)
    {
      /* On SHmedia, if the dependence is an anti-dependence or
         output-dependence, there is no cost. */              
      if (REG_NOTE_KIND (link) != 0)
        cost = 0;

      if (get_attr_is_mac_media (insn)
          && get_attr_is_mac_media (dep_insn))
        cost = 1;
    }
  else if (REG_NOTE_KIND (link) == 0)
    {
      enum attr_type dep_type, type;

      if (recog_memoized (insn) < 0
	  || recog_memoized (dep_insn) < 0)
	return cost;

      dep_type = get_attr_type (dep_insn);
      if (dep_type == TYPE_FLOAD || dep_type == TYPE_PCFLOAD)
	cost--;
      if ((dep_type == TYPE_LOAD_SI || dep_type == TYPE_PCLOAD_SI)
	  && (type = get_attr_type (insn)) != TYPE_CALL
	  && type != TYPE_SFUNC)
	cost--;

      /* The only input for a call that is timing-critical is the
	 function's address.  */
      if (GET_CODE(insn) == CALL_INSN)
	{
	  rtx call = PATTERN (insn);

	  if (GET_CODE (call) == PARALLEL)
	    call = XVECEXP (call, 0 ,0);
	  if (GET_CODE (call) == SET)
	    call = SET_SRC (call);
	  if (GET_CODE (call) == CALL && GET_CODE (XEXP (call, 0)) == MEM
	      && ! reg_set_p (XEXP (XEXP (call, 0), 0), dep_insn))
	    cost = 0;
	}
      /* Likewise, the most timing critical input for an sfuncs call
	 is the function address.  However, sfuncs typically start
	 using their arguments pretty quickly.
	 Assume a four cycle delay before they are needed.  */
      /* All sfunc calls are parallels with at least four components.
	 Exploit this to avoid unnecessary calls to sfunc_uses_reg.  */
      else if (GET_CODE (PATTERN (insn)) == PARALLEL
	       && XVECLEN (PATTERN (insn), 0) >= 4
	       && (reg = sfunc_uses_reg (insn)))
	{
	  if (! reg_set_p (reg, dep_insn))
	    cost -= 4;
	}
      /* When the preceding instruction loads the shift amount of
	 the following SHAD/SHLD, the latency of the load is increased
	 by 1 cycle.  */
      else if (TARGET_SH4
	       && get_attr_type (insn) == TYPE_DYN_SHIFT
	       && get_attr_any_int_load (dep_insn) == ANY_INT_LOAD_YES
	       && reg_overlap_mentioned_p (SET_DEST (PATTERN (dep_insn)),
					   XEXP (SET_SRC (single_set(insn)),
						 1)))
	cost++;
      /* When an LS group instruction with a latency of less than
	 3 cycles is followed by a double-precision floating-point
	 instruction, FIPR, or FTRV, the latency of the first
	 instruction is increased to 3 cycles.  */
      else if (cost < 3
	       && get_attr_insn_class (dep_insn) == INSN_CLASS_LS_GROUP
	       && get_attr_dfp_comp (insn) == DFP_COMP_YES)
	cost = 3;
      /* The lsw register of a double-precision computation is ready one
	 cycle earlier.  */
      else if (reload_completed
	       && get_attr_dfp_comp (dep_insn) == DFP_COMP_YES
	       && (use_pat = single_set (insn))
	       && ! regno_use_in (REGNO (SET_DEST (single_set (dep_insn))),
				  SET_SRC (use_pat)))
	cost -= 1;

      if (get_attr_any_fp_comp (dep_insn) == ANY_FP_COMP_YES
	  && get_attr_late_fp_use (insn) == LATE_FP_USE_YES)
	cost -= 1;
    }
  /* An anti-dependence penalty of two applies if the first insn is a double
     precision fadd / fsub / fmul.  */
  else if (REG_NOTE_KIND (link) == REG_DEP_ANTI
	   && recog_memoized (dep_insn) >= 0
	   && get_attr_type (dep_insn) == TYPE_DFP_ARITH
	   /* A lot of alleged anti-flow dependences are fake,
	      so check this one is real.  */
	   && flow_dependent_p (dep_insn, insn))
    cost = 2;


  return cost;
}

/* Check if INSN is flow-dependent on DEP_INSN.  Can also be used to check
   if DEP_INSN is anti-flow dependent on INSN.  */
static int
flow_dependent_p (insn, dep_insn)
     rtx insn, dep_insn;
{
  rtx tmp = PATTERN (insn);

  note_stores (PATTERN (dep_insn), flow_dependent_p_1, &tmp);
  return tmp == NULL_RTX;
}

/* A helper function for flow_dependent_p called through note_stores.  */
static void
flow_dependent_p_1 (x, pat, data)
     rtx x;
     rtx pat ATTRIBUTE_UNUSED;
     void *data;
{
  rtx * pinsn = (rtx *) data;

  if (*pinsn && reg_referenced_p (x, *pinsn))
    *pinsn = NULL_RTX;
}

/* For use by ALLOCATE_INITIAL_VALUE.  Note that sh.md contains some
   'special function' patterns (type sfunc) that clobber pr, but that
   do not look like function calls to leaf_function_p.  Hence we must
   do this extra check.  */
int
sh_pr_n_sets ()
{
  return REG_N_SETS (TARGET_SHMEDIA ? PR_MEDIA_REG : PR_REG);
}

/* This Function returns nonzero if the DFA based scheduler interface
   is to be used.  At present this is supported for the SH4 only.  */
static int
sh_use_dfa_interface()
{
  if (TARGET_HARD_SH4)
    return 1;
  else
    return 0;
}

/* This function returns "2" to indicate dual issue for the SH4
   processor.  To be used by the DFA pipeline description.  */
static int
sh_issue_rate()
{
  if (TARGET_SUPERSCALAR)
    return 2;
  else
    return 1;
}

/* SHmedia requires registers for branches, so we can't generate new
   branches past reload.  */
static bool
sh_cannot_modify_jumps_p ()
{
  return (TARGET_SHMEDIA && (reload_in_progress || reload_completed));
}

static bool
sh_ms_bitfield_layout_p (record_type)
     tree record_type ATTRIBUTE_UNUSED;
{
  return TARGET_SH5;
}

/* If using PIC, mark a SYMBOL_REF for a non-global symbol so that we
   may access it using GOTOFF instead of GOT.  */

static void
sh_encode_section_info (decl, first)
     tree decl;
     int first;
{
  rtx rtl, symbol;

  if (DECL_P (decl))
    rtl = DECL_RTL (decl);
  else
    rtl = TREE_CST_RTL (decl);
  if (GET_CODE (rtl) != MEM)
    return;
  symbol = XEXP (rtl, 0);
  if (GET_CODE (symbol) != SYMBOL_REF)
    return;

  if (flag_pic)
    SYMBOL_REF_FLAG (symbol) = (*targetm.binds_local_p) (decl);

  if (TARGET_SH5 && first && TREE_CODE (decl) != FUNCTION_DECL)
    XEXP (rtl, 0) = gen_datalabel_ref (symbol);
}

/* Undo the effects of the above.  */

static const char *
sh_strip_name_encoding (str)
     const char *str;
{
  STRIP_DATALABEL_ENCODING (str, str);
  str += *str == '*';
  return str;
}


/* 
   On the SH1..SH4, the trampoline looks like
   2 0002 D202     	   	mov.l	l2,r2
   1 0000 D301     		mov.l	l1,r3
   3 0004 422B     		jmp	@r2
   4 0006 0009     		nop
   5 0008 00000000 	l1:  	.long   area
   6 000c 00000000 	l2:	.long   function

   SH5 (compact) uses r1 instead of r3 for the static chain.  */


/* Emit RTL insns to initialize the variable parts of a trampoline.
   FNADDR is an RTX for the address of the function's pure code.
   CXT is an RTX for the static chain value for the function.  */

void
sh_initialize_trampoline (tramp, fnaddr, cxt)
     rtx tramp, fnaddr, cxt;
{
  if (TARGET_SHMEDIA64)
    {
      rtx tramp_templ;
      int fixed_len;

      rtx movi1 = GEN_INT (0xcc000010);
      rtx shori1 = GEN_INT (0xc8000010);
      rtx src, dst;

      /* The following trampoline works within a +- 128 KB range for cxt:
	 ptb/u cxt,tr1; movi fnaddr >> 48,r0; shori fnaddr >> 32,r0;
         shori fnaddr >> 16,r0; shori fnaddr,r0; ptabs/l r0,tr0
         gettr tr1,r1; blink tr0,r63  */
      /* Address rounding makes it hard to compute the exact bounds of the
	 offset for this trampoline, but we have a rather generous offset
	 range, so frame_offset should do fine as an upper bound.  */
      if (cxt == virtual_stack_vars_rtx && frame_offset < 0x20000)
	{
	  /* ??? could optimize this trampoline initialization
	     by writing DImode words with two insns each.  */
	  rtx mask = force_reg (DImode, GEN_INT (0x3fffc00));
	  rtx insn = gen_rtx_MINUS (DImode, cxt, tramp);
	  insn = gen_rtx_ASHIFT (DImode, insn, GEN_INT (10-2));
	  insn = gen_rtx_AND (DImode, insn, mask);
	  /* Or in ptb/u .,tr1 pattern */
	  insn = gen_rtx_IOR (DImode, insn, gen_int_mode (0xec000010, SImode));
	  insn = force_operand (insn, NULL_RTX);
	  insn = gen_lowpart (SImode, insn);
	  emit_move_insn (gen_rtx_MEM (SImode, tramp), insn);
	  insn = gen_rtx_LSHIFTRT (DImode, fnaddr, GEN_INT (38));
	  insn = gen_rtx_AND (DImode, insn, mask);
	  insn = force_operand (gen_rtx_IOR (DImode, movi1, insn), NULL_RTX);
	  insn = gen_lowpart (SImode, insn);
	  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 4)), insn);
	  insn = gen_rtx_LSHIFTRT (DImode, fnaddr, GEN_INT (22));
	  insn = gen_rtx_AND (DImode, insn, mask);
	  insn = force_operand (gen_rtx_IOR (DImode, shori1, insn), NULL_RTX);
	  insn = gen_lowpart (SImode, insn);
	  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 8)), insn);
	  insn = gen_rtx_LSHIFTRT (DImode, fnaddr, GEN_INT (6));
	  insn = gen_rtx_AND (DImode, insn, mask);
	  insn = force_operand (gen_rtx_IOR (DImode, shori1, insn), NULL_RTX);
	  insn = gen_lowpart (SImode, insn);
	  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 12)),
			  insn);
	  insn = gen_rtx_ASHIFT (DImode, fnaddr, GEN_INT (10));
	  insn = gen_rtx_AND (DImode, insn, mask);
	  insn = force_operand (gen_rtx_IOR (DImode, shori1, insn), NULL_RTX);
	  insn = gen_lowpart (SImode, insn);
	  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 16)),
			  insn);
	  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 20)),
			  GEN_INT (0x6bf10600));
	  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 24)),
			  GEN_INT (0x4415fc10));
	  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 28)),
			  GEN_INT (0x4401fff0));
	  emit_insn (gen_ic_invalidate_line (tramp));
	  return;
	}
      tramp_templ = gen_rtx_SYMBOL_REF (Pmode,"__GCC_nested_trampoline");
      fixed_len = TRAMPOLINE_SIZE - 2 * GET_MODE_SIZE (Pmode);

      tramp_templ = gen_datalabel_ref (tramp_templ);
      dst = gen_rtx_MEM (BLKmode, tramp);
      src = gen_rtx_MEM (BLKmode, tramp_templ);
      set_mem_align (dst, 256);
      set_mem_align (src, 64);
      emit_block_move (dst, src, GEN_INT (fixed_len), BLOCK_OP_NORMAL);

      emit_move_insn (gen_rtx_MEM (Pmode, plus_constant (tramp,	fixed_len)),
		      fnaddr);
      emit_move_insn (gen_rtx_MEM (Pmode,
				   plus_constant (tramp,
						  fixed_len
						  + GET_MODE_SIZE (Pmode))), 
		      cxt);
      emit_insn (gen_ic_invalidate_line (tramp));
      return;
    }
  else if (TARGET_SHMEDIA)
    {
      /* movi fnaddr >> 16,r1; shori fnaddr,r1; ptabs/l r1,tr0
         movi cxt >> 16,r1; shori cxt,r1; blink tr0,r63  */
      rtx quad0 = gen_reg_rtx (DImode), cxtload = gen_reg_rtx (DImode);
      rtx quad1 = gen_reg_rtx (DImode), quad2 = gen_reg_rtx (DImode);
      /* movi 0,r1: 0xcc000010 shori 0,r1: c8000010  concatenated,
	 rotated 10 right, and higer 16 bit of every 32 selected.  */
      rtx movishori
	= force_reg (V2HImode, (simplify_gen_subreg
				(V2HImode, GEN_INT (0x4330432), SImode, 0)));
      rtx ptabs = force_reg (DImode, GEN_INT (0x6bf10600));
      rtx blink = force_reg (DImode, GEN_INT (0x4401fff0));

      tramp = force_reg (Pmode, tramp);
      fnaddr = force_reg (SImode, fnaddr);
      cxt = force_reg (SImode, cxt);
      emit_insn (gen_mshflo_w_x (gen_rtx_SUBREG (V4HImode, quad0, 0),
				 gen_rtx_SUBREG (V2HImode, fnaddr, 0),
				 movishori));
      emit_insn (gen_rotrdi3_mextr (quad0, quad0,
				    GEN_INT (TARGET_LITTLE_ENDIAN ? 24 : 56)));
      emit_insn (gen_ashldi3_media (quad0, quad0, GEN_INT (2)));
      emit_move_insn (gen_rtx_MEM (DImode, tramp), quad0);
      emit_insn (gen_mshflo_w_x (gen_rtx_SUBREG (V4HImode, cxtload, 0),
				 gen_rtx_SUBREG (V2HImode, cxt, 0),
				 movishori));
      emit_insn (gen_rotrdi3_mextr (cxtload, cxtload,
				    GEN_INT (TARGET_LITTLE_ENDIAN ? 24 : 56)));
      emit_insn (gen_ashldi3_media (cxtload, cxtload, GEN_INT (2)));
      if (TARGET_LITTLE_ENDIAN)
	{
	  emit_insn (gen_mshflo_l_di (quad1, ptabs, cxtload));
	  emit_insn (gen_mextr4 (quad2, cxtload, blink));
	}
      else
	{
	  emit_insn (gen_mextr4 (quad1, cxtload, ptabs));
	  emit_insn (gen_mshflo_l_di (quad2, blink, cxtload));
	}
      emit_move_insn (gen_rtx_MEM (DImode, plus_constant (tramp, 8)), quad1);
      emit_move_insn (gen_rtx_MEM (DImode, plus_constant (tramp, 16)), quad2);
      emit_insn (gen_ic_invalidate_line (tramp));
      return;
    }
  else if (TARGET_SHCOMPACT)
    {
      emit_insn (gen_initialize_trampoline (tramp, cxt, fnaddr));
      return;
    }
  emit_move_insn (gen_rtx_MEM (SImode, tramp),
		  gen_int_mode (TARGET_LITTLE_ENDIAN ? 0xd301d202 : 0xd202d301,
				SImode));
  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 4)),
		  gen_int_mode (TARGET_LITTLE_ENDIAN ? 0x0009422b : 0x422b0009,
				SImode));
  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 8)),
		  cxt);
  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (tramp, 12)),
		  fnaddr);
  if (TARGET_HARVARD)
    {
      if (TARGET_USERMODE)
	emit_library_call (gen_rtx_SYMBOL_REF (Pmode, "__ic_invalidate"),
			   0, VOIDmode, 1, tramp, SImode);
      else
	emit_insn (gen_ic_invalidate_line (tramp));
    }
}


/* Machine specific built-in functions.  */

struct builtin_description
{
  const enum insn_code icode;
  const char *const name;
  int signature;
};

/* describe number and signedness of arguments; arg[0] == result
   (1: unsigned, 2: signed, 4: don't care, 8: pointer 0: no argument */
static const char signature_args[][4] =
{
#define SH_BLTIN_V2SI2 0
  { 4, 4 },
#define SH_BLTIN_V4HI2 1
  { 4, 4 },
#define SH_BLTIN_V2SI3 2
  { 4, 4, 4 },
#define SH_BLTIN_V4HI3 3
  { 4, 4, 4 },
#define SH_BLTIN_V8QI3 4
  { 4, 4, 4 },
#define SH_BLTIN_MAC_HISI 5
  { 1, 4, 4, 1 },
#define SH_BLTIN_SH_HI 6
  { 4, 4, 1 },
#define SH_BLTIN_SH_SI 7
  { 4, 4, 1 },
#define SH_BLTIN_V4HI2V2SI 8
  { 4, 4, 4 },
#define SH_BLTIN_V4HI2V8QI 9
  { 4, 4, 4 },
#define SH_BLTIN_SISF 10
  { 4, 2 },
#define SH_BLTIN_LDUA_L 11
  { 2, 8 },
#define SH_BLTIN_LDUA_Q 12
  { 1, 8 },
#define SH_BLTIN_STUA_L 13
  { 0, 8, 2 },
#define SH_BLTIN_STUA_Q 14
  { 0, 8, 1 },
#define SH_BLTIN_UDI 15
  { 0, 8, 1 },
#define SH_BLTIN_NUM_SHARED_SIGNATURES 16
#define SH_BLTIN_2 16
#define SH_BLTIN_SU 16
  { 1, 2 },
#define SH_BLTIN_3 17
#define SH_BLTIN_SUS 17
  { 2, 2, 1 },
#define SH_BLTIN_PSSV 18
  { 0, 8, 2, 2 },
#define SH_BLTIN_XXUU 19
#define SH_BLTIN_UUUU 19
  { 1, 1, 1, 1 },
#define SH_BLTIN_PV 20
  { 0, 8 },
};
/* mcmv: operands considered unsigned. */
/* mmulsum_wq, msad_ubq: result considered unsigned long long.  */
/* mperm: control value considered unsigned int. */
/* mshalds, mshard, mshards, mshlld, mshlrd: shift count is unsigned int. */
/* mshards_q: returns signed short.  */
/* nsb: takes long long arg, returns unsigned char.  */
static const struct builtin_description bdesc[] =
{
  { CODE_FOR_absv2si2,	"__builtin_absv2si2", SH_BLTIN_V2SI2 },
  { CODE_FOR_absv4hi2,	"__builtin_absv4hi2", SH_BLTIN_V4HI2 },
  { CODE_FOR_addv2si3,	"__builtin_addv2si3", SH_BLTIN_V2SI3 },
  { CODE_FOR_addv4hi3,	"__builtin_addv4hi3", SH_BLTIN_V4HI3 },
  { CODE_FOR_ssaddv2si3,"__builtin_ssaddv2si3", SH_BLTIN_V2SI3 },
  { CODE_FOR_usaddv8qi3,"__builtin_usaddv8qi3", SH_BLTIN_V8QI3 },
  { CODE_FOR_ssaddv4hi3,"__builtin_ssaddv4hi3", SH_BLTIN_V4HI3 },
#if 0
  { CODE_FOR_alloco32,	"__builtin_sh_media_ALLOCO", SH_BLTIN_PV },
  { CODE_FOR_alloco64,	"__builtin_sh_media_ALLOCO", SH_BLTIN_PV },
#endif
  { CODE_FOR_negcmpeqv8qi,"__builtin_sh_media_MCMPEQ_B", SH_BLTIN_V8QI3 },
  { CODE_FOR_negcmpeqv2si,"__builtin_sh_media_MCMPEQ_L", SH_BLTIN_V2SI3 },
  { CODE_FOR_negcmpeqv4hi,"__builtin_sh_media_MCMPEQ_W", SH_BLTIN_V4HI3 },
  { CODE_FOR_negcmpgtuv8qi,"__builtin_sh_media_MCMPGT_UB", SH_BLTIN_V8QI3 },
  { CODE_FOR_negcmpgtv2si,"__builtin_sh_media_MCMPGT_L", SH_BLTIN_V2SI3 },
  { CODE_FOR_negcmpgtv4hi,"__builtin_sh_media_MCMPGT_W", SH_BLTIN_V4HI3 },
  { CODE_FOR_mcmv,	"__builtin_sh_media_MCMV", SH_BLTIN_UUUU },
  { CODE_FOR_mcnvs_lw,	"__builtin_sh_media_MCNVS_LW", SH_BLTIN_3 },
  { CODE_FOR_mcnvs_wb,	"__builtin_sh_media_MCNVS_WB", SH_BLTIN_V4HI2V8QI },
  { CODE_FOR_mcnvs_wub,	"__builtin_sh_media_MCNVS_WUB", SH_BLTIN_V4HI2V8QI },
  { CODE_FOR_mextr1,	"__builtin_sh_media_MEXTR1", SH_BLTIN_UDI },
  { CODE_FOR_mextr2,	"__builtin_sh_media_MEXTR2", SH_BLTIN_UDI },
  { CODE_FOR_mextr3,	"__builtin_sh_media_MEXTR3", SH_BLTIN_UDI },
  { CODE_FOR_mextr4,	"__builtin_sh_media_MEXTR4", SH_BLTIN_UDI },
  { CODE_FOR_mextr5,	"__builtin_sh_media_MEXTR5", SH_BLTIN_UDI },
  { CODE_FOR_mextr6,	"__builtin_sh_media_MEXTR6", SH_BLTIN_UDI },
  { CODE_FOR_mextr7,	"__builtin_sh_media_MEXTR7", SH_BLTIN_UDI },
  { CODE_FOR_mmacfx_wl,	"__builtin_sh_media_MMACFX_WL", SH_BLTIN_MAC_HISI },
  { CODE_FOR_mmacnfx_wl,"__builtin_sh_media_MMACNFX_WL", SH_BLTIN_MAC_HISI },
  { CODE_FOR_mulv2si3,	"__builtin_mulv2si3", SH_BLTIN_V2SI3, },
  { CODE_FOR_mulv4hi3,	"__builtin_mulv4hi3", SH_BLTIN_V4HI3 },
  { CODE_FOR_mmulfx_l,	"__builtin_sh_media_MMULFX_L", SH_BLTIN_V2SI3 },
  { CODE_FOR_mmulfx_w,	"__builtin_sh_media_MMULFX_W", SH_BLTIN_V4HI3 },
  { CODE_FOR_mmulfxrp_w,"__builtin_sh_media_MMULFXRP_W", SH_BLTIN_V4HI3 },
  { CODE_FOR_mmulhi_wl,	"__builtin_sh_media_MMULHI_WL", SH_BLTIN_V4HI2V2SI },
  { CODE_FOR_mmullo_wl,	"__builtin_sh_media_MMULLO_WL", SH_BLTIN_V4HI2V2SI },
  { CODE_FOR_mmulsum_wq,"__builtin_sh_media_MMULSUM_WQ", SH_BLTIN_XXUU },
  { CODE_FOR_mperm_w,	"__builtin_sh_media_MPERM_W", SH_BLTIN_SH_HI },
  { CODE_FOR_msad_ubq,	"__builtin_sh_media_MSAD_UBQ", SH_BLTIN_XXUU },
  { CODE_FOR_mshalds_l,	"__builtin_sh_media_MSHALDS_L", SH_BLTIN_SH_SI },
  { CODE_FOR_mshalds_w,	"__builtin_sh_media_MSHALDS_W", SH_BLTIN_SH_HI },
  { CODE_FOR_ashrv2si3,	"__builtin_ashrv2si3", SH_BLTIN_SH_SI },
  { CODE_FOR_ashrv4hi3,	"__builtin_ashrv4hi3", SH_BLTIN_SH_HI },
  { CODE_FOR_mshards_q,	"__builtin_sh_media_MSHARDS_Q", SH_BLTIN_SUS },
  { CODE_FOR_mshfhi_b,	"__builtin_sh_media_MSHFHI_B", SH_BLTIN_V8QI3 },
  { CODE_FOR_mshfhi_l,	"__builtin_sh_media_MSHFHI_L", SH_BLTIN_V2SI3 },
  { CODE_FOR_mshfhi_w,	"__builtin_sh_media_MSHFHI_W", SH_BLTIN_V4HI3 },
  { CODE_FOR_mshflo_b,	"__builtin_sh_media_MSHFLO_B", SH_BLTIN_V8QI3 },
  { CODE_FOR_mshflo_l,	"__builtin_sh_media_MSHFLO_L", SH_BLTIN_V2SI3 },
  { CODE_FOR_mshflo_w,	"__builtin_sh_media_MSHFLO_W", SH_BLTIN_V4HI3 },
  { CODE_FOR_ashlv2si3,	"__builtin_ashlv2si3", SH_BLTIN_SH_SI },
  { CODE_FOR_ashlv4hi3,	"__builtin_ashlv4hi3", SH_BLTIN_SH_HI },
  { CODE_FOR_lshrv2si3,	"__builtin_lshrv2si3", SH_BLTIN_SH_SI },
  { CODE_FOR_lshrv4hi3,	"__builtin_lshrv4hi3", SH_BLTIN_SH_HI },
  { CODE_FOR_subv2si3,	"__builtin_subv2si3", SH_BLTIN_V2SI3 },
  { CODE_FOR_subv4hi3,	"__builtin_subv4hi3", SH_BLTIN_V4HI3 },
  { CODE_FOR_sssubv2si3,"__builtin_sssubv2si3", SH_BLTIN_V2SI3 },
  { CODE_FOR_ussubv8qi3,"__builtin_ussubv8qi3", SH_BLTIN_V8QI3 },
  { CODE_FOR_sssubv4hi3,"__builtin_sssubv4hi3", SH_BLTIN_V4HI3 },
  { CODE_FOR_fcosa_s,	"__builtin_sh_media_FCOSA_S", SH_BLTIN_SISF },
  { CODE_FOR_fsina_s,	"__builtin_sh_media_FSINA_S", SH_BLTIN_SISF },
  { CODE_FOR_fipr,	"__builtin_sh_media_FIPR_S", SH_BLTIN_3 },
  { CODE_FOR_ftrv,	"__builtin_sh_media_FTRV_S", SH_BLTIN_3 },
  { CODE_FOR_fsrra_s,	"__builtin_sh_media_FSRRA_S", SH_BLTIN_2 },
#if 0
  { CODE_FOR_ldhi_l,	"__builtin_sh_media_LDHI_L", SH_BLTIN_LDUA_L },
  { CODE_FOR_ldhi_q,	"__builtin_sh_media_LDHI_Q", SH_BLTIN_LDUA_Q },
  { CODE_FOR_ldlo_l,	"__builtin_sh_media_LDLO_L", SH_BLTIN_LDUA_L },
  { CODE_FOR_ldlo_q,	"__builtin_sh_media_LDLO_Q", SH_BLTIN_LDUA_Q },
  { CODE_FOR_sthi_l,	"__builtin_sh_media_STHI_L", SH_BLTIN_STUA_L },
  { CODE_FOR_sthi_q,	"__builtin_sh_media_STHI_Q", SH_BLTIN_STUA_Q },
  { CODE_FOR_stlo_l,	"__builtin_sh_media_STLO_L", SH_BLTIN_STUA_L },
  { CODE_FOR_stlo_q,	"__builtin_sh_media_STLO_Q", SH_BLTIN_STUA_Q },
  { CODE_FOR_ldhi_l64,	"__builtin_sh_media_LDHI_L", SH_BLTIN_LDUA_L },
  { CODE_FOR_ldhi_q64,	"__builtin_sh_media_LDHI_Q", SH_BLTIN_LDUA_Q },
  { CODE_FOR_ldlo_l64,	"__builtin_sh_media_LDLO_L", SH_BLTIN_LDUA_L },
  { CODE_FOR_ldlo_q64,	"__builtin_sh_media_LDLO_Q", SH_BLTIN_LDUA_Q },
  { CODE_FOR_sthi_l64,	"__builtin_sh_media_STHI_L", SH_BLTIN_STUA_L },
  { CODE_FOR_sthi_q64,	"__builtin_sh_media_STHI_Q", SH_BLTIN_STUA_Q },
  { CODE_FOR_stlo_l64,	"__builtin_sh_media_STLO_L", SH_BLTIN_STUA_L },
  { CODE_FOR_stlo_q64,	"__builtin_sh_media_STLO_Q", SH_BLTIN_STUA_Q },
#endif
  { CODE_FOR_nsb,	"__builtin_sh_media_NSB", SH_BLTIN_SU },
  { CODE_FOR_byterev,	"__builtin_sh_media_BYTEREV", SH_BLTIN_2 },
#if 0
  { CODE_FOR_prefetch32,"__builtin_sh_media_PREFO", SH_BLTIN_PSSV },
  { CODE_FOR_prefetch64,"__builtin_sh_media_PREFO", SH_BLTIN_PSSV }
#endif
};

static void
sh_media_init_builtins ()
{
  tree shared[SH_BLTIN_NUM_SHARED_SIGNATURES];
  const struct builtin_description *d;

  memset (shared, 0, sizeof shared);
  for (d = bdesc; d - bdesc < (int) (sizeof bdesc / sizeof bdesc[0]); d++)
    {
      tree type, arg_type;
      int signature = d->signature;
      int i;

      if (signature < SH_BLTIN_NUM_SHARED_SIGNATURES && shared[signature])
	type = shared[signature];
      else
	{
	  int has_result = signature_args[signature][0] != 0;

	  if (signature_args[signature][1] == 8
	      && (insn_data[d->icode].operand[has_result].mode != Pmode))
	    continue;
	  if (! TARGET_FPU_ANY
	      && FLOAT_MODE_P (insn_data[d->icode].operand[0].mode))
	    continue;
	  type = void_list_node;
	  for (i = 3; ; i--)
	    {
	      int arg = signature_args[signature][i];
	      int opno = i - 1 + has_result;

	      if (arg == 8)
		arg_type = ptr_type_node;
	      else if (arg)
		arg_type = ((*lang_hooks.types.type_for_mode)
			    (insn_data[d->icode].operand[opno].mode,
			     (arg & 1)));
	      else if (i)
		continue;
	      else
		arg_type = void_type_node;
	      if (i == 0)
		break;
	      type = tree_cons (NULL_TREE, arg_type, type);
	    }
	  type = build_function_type (arg_type, type);
	  if (signature < SH_BLTIN_NUM_SHARED_SIGNATURES)
	    shared[signature] = type;
	}
      builtin_function (d->name, type, d - bdesc, BUILT_IN_MD,
			NULL, NULL_TREE);
    }
}

static void
sh_init_builtins ()
{
  if (TARGET_SHMEDIA)
    sh_media_init_builtins ();
}

/* Expand an expression EXP that calls a built-in function,
   with result going to TARGET if that's convenient
   (and in mode MODE if that's convenient).
   SUBTARGET may be used as the target for computing one of EXP's operands.
   IGNORE is nonzero if the value is to be ignored.  */

static rtx
sh_expand_builtin (exp, target, subtarget, mode, ignore)
     tree exp;
     rtx target;
     rtx subtarget ATTRIBUTE_UNUSED;
     enum machine_mode mode ATTRIBUTE_UNUSED;
     int ignore;
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  const struct builtin_description *d = &bdesc[fcode];
  enum insn_code icode = d->icode;
  int signature = d->signature;
  enum machine_mode tmode = VOIDmode;
  int nop = 0, i;
  rtx op[4];
  rtx pat;

  if (signature_args[signature][0])
    {
      if (ignore)
	return 0;

      tmode = insn_data[icode].operand[0].mode;
      if (! target
	  || GET_MODE (target) != tmode
	  || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
	target = gen_reg_rtx (tmode);
      op[nop++] = target;
    }
  else
    target = 0;

  for (i = 1; i <= 3; i++, nop++)
    {
      tree arg;
      enum machine_mode opmode, argmode;

      if (! signature_args[signature][i])
	break;
      arg = TREE_VALUE (arglist);
      if (arg == error_mark_node)
	return const0_rtx;
      arglist = TREE_CHAIN (arglist);
      opmode = insn_data[icode].operand[nop].mode;
      argmode = TYPE_MODE (TREE_TYPE (arg));
      if (argmode != opmode)
	arg = build1 (NOP_EXPR,
		      (*lang_hooks.types.type_for_mode) (opmode, 0), arg);
      op[nop] = expand_expr (arg, NULL_RTX, opmode, 0);
      if (! (*insn_data[icode].operand[nop].predicate) (op[nop], opmode))
	op[nop] = copy_to_mode_reg (opmode, op[nop]);
    }

  switch (nop)
    {
    case 1:
      pat = (*insn_data[d->icode].genfun) (op[0]);
      break;
    case 2:
      pat = (*insn_data[d->icode].genfun) (op[0], op[1]);
      break;
    case 3:
      pat = (*insn_data[d->icode].genfun) (op[0], op[1], op[2]);
      break;
    case 4:
      pat = (*insn_data[d->icode].genfun) (op[0], op[1], op[2], op[3]);
      break;
    default:
      abort ();
    }
  if (! pat)
    return 0;
  emit_insn (pat);
  return target;
}

void
sh_expand_unop_v2sf (code, op0, op1)
     enum rtx_code code;
     rtx op0, op1;
{
  rtx sel0 = const0_rtx;
  rtx sel1 = const1_rtx;
  rtx (*fn) PARAMS ((rtx, rtx, rtx, rtx, rtx)) = gen_unary_sf_op;
  rtx op = gen_rtx_fmt_e (code, SFmode, op1);

  emit_insn ((*fn) (op0, op1, op, sel0, sel0));
  emit_insn ((*fn) (op0, op1, op, sel1, sel1));
}

void
sh_expand_binop_v2sf (code, op0, op1, op2)
     enum rtx_code code;
     rtx op0, op1, op2;
{
  rtx sel0 = const0_rtx;
  rtx sel1 = const1_rtx;
  rtx (*fn) PARAMS ((rtx, rtx, rtx, rtx, rtx, rtx, rtx)) = gen_binary_sf_op;
  rtx op = gen_rtx_fmt_ee (code, SFmode, op1, op2);

  emit_insn ((*fn) (op0, op1, op2, op, sel0, sel0, sel0));
  emit_insn ((*fn) (op0, op1, op2, op, sel1, sel1, sel1));
}

/* Return the class of registers for which a mode change from FROM to TO
   is invalid.  */
bool
sh_cannot_change_mode_class (from, to, class)
     enum machine_mode from, to;
     enum reg_class class;
{
  if (GET_MODE_SIZE (from) != GET_MODE_SIZE (to))
    {
       if (TARGET_LITTLE_ENDIAN)
         {
	   if (GET_MODE_SIZE (to) < 8 || GET_MODE_SIZE (from) < 8)
	     return reg_classes_intersect_p (DF_REGS, class);
	 }
       else
	 {
	   if (GET_MODE_SIZE (from) < 8)
	     return reg_classes_intersect_p (DF_HI_REGS, class);
	 }
    }
  return 0;
}


/* If ADDRESS refers to a CODE_LABEL, add NUSES to the number of times
   that label is used.  */

void
sh_mark_label (address, nuses)
     rtx address;
     int nuses;
{
  if (GOTOFF_P (address))
    {
      /* Extract the label or symbol.  */
      address = XEXP (address, 0);
      if (GET_CODE (address) == PLUS)
	address = XEXP (address, 0);
      address = XVECEXP (address, 0, 0);
    }
  if (GET_CODE (address) == LABEL_REF
      && GET_CODE (XEXP (address, 0)) == CODE_LABEL)
    LABEL_NUSES (XEXP (address, 0)) += nuses;
}

/* Compute extra cost of moving data between one register class
   and another.  */

/* If SECONDARY*_RELOAD_CLASS says something about the src/dst pair, regclass
   uses this information.  Hence, the general register <-> floating point
   register information here is not used for SFmode.  */

int
sh_register_move_cost (mode, srcclass, dstclass)
     enum machine_mode mode;
     enum reg_class srcclass, dstclass;
{
  if (dstclass == T_REGS || dstclass == PR_REGS)
    return 10;

  if (mode == SImode && ! TARGET_SHMEDIA && TARGET_FMOVD
      && REGCLASS_HAS_FP_REG (srcclass)
      && REGCLASS_HAS_FP_REG (dstclass))
    return 4;

  if ((REGCLASS_HAS_FP_REG (dstclass)
       && REGCLASS_HAS_GENERAL_REG (srcclass))
      || (REGCLASS_HAS_GENERAL_REG (dstclass)
	  && REGCLASS_HAS_FP_REG (srcclass)))
   return ((TARGET_SHMEDIA ? 4 : TARGET_FMOVD ? 8 : 12)
	   * ((GET_MODE_SIZE (mode) + 7) / 8U));

  if ((dstclass == FPUL_REGS
       && REGCLASS_HAS_GENERAL_REG (srcclass))
      || (srcclass == FPUL_REGS
	  && REGCLASS_HAS_GENERAL_REG (dstclass)))
    return 5;

  if ((dstclass == FPUL_REGS
       && (srcclass == PR_REGS || srcclass == MAC_REGS || srcclass == T_REGS))
      || (srcclass == FPUL_REGS		
	  && (dstclass == PR_REGS || dstclass == MAC_REGS)))
    return 7;

  if ((srcclass == TARGET_REGS && ! REGCLASS_HAS_GENERAL_REG (dstclass))
      || ((dstclass) == TARGET_REGS && ! REGCLASS_HAS_GENERAL_REG (srcclass)))
    return 20;

  if ((srcclass == FPSCR_REGS && ! REGCLASS_HAS_GENERAL_REG (dstclass))
      || (dstclass == FPSCR_REGS && ! REGCLASS_HAS_GENERAL_REG (srcclass)))
  return 4;

  if (TARGET_SHMEDIA
      || (TARGET_FMOVD
	  && ! REGCLASS_HAS_GENERAL_REG (srcclass)
	  && ! REGCLASS_HAS_GENERAL_REG (dstclass)))
    return 2 * ((GET_MODE_SIZE (mode) + 7) / 8U);

  return 2 * ((GET_MODE_SIZE (mode) + 3) / 4U);
}

/* Like register_operand, but take into account that SHMEDIA can use
   the constant zero like a general register.  */
int
sh_register_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (op == CONST0_RTX (mode) && TARGET_SHMEDIA)
    return 1;
  return register_operand (op, mode);
}

#include "gt-sh.h"
