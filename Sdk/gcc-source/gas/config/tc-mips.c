/* tc-mips.c -- assemble code for a MIPS chip.
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Contributed by the OSF and Ralph Campbell.
   Written by Keith Knowles and Ralph Campbell, working independently.
   Modified for ECOFF and R4000 support by Ian Lance Taylor of Cygnus
   Support.

   This file is part of GAS.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "as.h"
#include "config.h"
#include "subsegs.h"
#include "safe-ctype.h"

#ifdef USE_STDARG
#include <stdarg.h>
#endif
#ifdef USE_VARARGS
#include <varargs.h>
#endif

#include "opcode/mips.h"
#include "itbl-ops.h"
#include "dwarf2dbg.h"

#ifdef DEBUG
#define DBG(x) printf x
#else
#define DBG(x)
#endif

#ifdef OBJ_MAYBE_ELF
/* Clean up namespace so we can include obj-elf.h too.  */
static int mips_output_flavor PARAMS ((void));
static int mips_output_flavor () { return OUTPUT_FLAVOR; }
#undef OBJ_PROCESS_STAB
#undef OUTPUT_FLAVOR
#undef S_GET_ALIGN
#undef S_GET_SIZE
#undef S_SET_ALIGN
#undef S_SET_SIZE
#undef obj_frob_file
#undef obj_frob_file_after_relocs
#undef obj_frob_symbol
#undef obj_pop_insert
#undef obj_sec_sym_ok_for_reloc
#undef OBJ_COPY_SYMBOL_ATTRIBUTES

#include "obj-elf.h"
/* Fix any of them that we actually care about.  */
#undef OUTPUT_FLAVOR
#define OUTPUT_FLAVOR mips_output_flavor()
#endif

#if defined (OBJ_ELF)
#include "elf/mips.h"
#endif

#ifndef ECOFF_DEBUGGING
#define NO_ECOFF_DEBUGGING
#define ECOFF_DEBUGGING 0
#endif

int mips_flag_mdebug = -1;

#include "ecoff.h"

#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)
static char *mips_regmask_frag;
#endif

#define ZERO 0
#define AT  1
#define TREG 24
#define PIC_CALL_REG 25
#define KT0 26
#define KT1 27
#define GP  28
#define SP  29
#define FP  30
#define RA  31

#define ILLEGAL_REG (32)

/* Allow override of standard little-endian ECOFF format.  */

#ifndef ECOFF_LITTLE_FORMAT
#define ECOFF_LITTLE_FORMAT "ecoff-littlemips"
#endif

extern int target_big_endian;

/* The name of the readonly data section.  */
#define RDATA_SECTION_NAME (OUTPUT_FLAVOR == bfd_target_aout_flavour \
			    ? ".data" \
			    : OUTPUT_FLAVOR == bfd_target_ecoff_flavour \
			    ? ".rdata" \
			    : OUTPUT_FLAVOR == bfd_target_coff_flavour \
			    ? ".rdata" \
			    : OUTPUT_FLAVOR == bfd_target_elf_flavour \
			    ? ".rodata" \
			    : (abort (), ""))

/* The ABI to use.  */
enum mips_abi_level
{
  NO_ABI = 0,
  O32_ABI,
  O64_ABI,
  N32_ABI,
  N64_ABI,
  EABI_ABI
};

/* MIPS ABI we are using for this output file.  */
static enum mips_abi_level mips_abi = NO_ABI;

/* This is the set of options which may be modified by the .set
   pseudo-op.  We use a struct so that .set push and .set pop are more
   reliable.  */

struct mips_set_options
{
  /* MIPS ISA (Instruction Set Architecture) level.  This is set to -1
     if it has not been initialized.  Changed by `.set mipsN', and the
     -mipsN command line option, and the default CPU.  */
  int isa;
  /* Enabled Application Specific Extensions (ASEs).  These are set to -1
     if they have not been initialized.  Changed by `.set <asename>', by
     command line options, and based on the default architecture.  */
  int ase_mips3d;
  int ase_mdmx;
  /* Whether we are assembling for the mips16 processor.  0 if we are
     not, 1 if we are, and -1 if the value has not been initialized.
     Changed by `.set mips16' and `.set nomips16', and the -mips16 and
     -nomips16 command line options, and the default CPU.  */
  int mips16;
  /* Non-zero if we should not reorder instructions.  Changed by `.set
     reorder' and `.set noreorder'.  */
  int noreorder;
  /* Non-zero if we should not permit the $at ($1) register to be used
     in instructions.  Changed by `.set at' and `.set noat'.  */
  int noat;
  /* Non-zero if we should warn when a macro instruction expands into
     more than one machine instruction.  Changed by `.set nomacro' and
     `.set macro'.  */
  int warn_about_macros;
  /* Non-zero if we should not move instructions.  Changed by `.set
     move', `.set volatile', `.set nomove', and `.set novolatile'.  */
  int nomove;
  /* Non-zero if we should not optimize branches by moving the target
     of the branch into the delay slot.  Actually, we don't perform
     this optimization anyhow.  Changed by `.set bopt' and `.set
     nobopt'.  */
  int nobopt;
  /* Non-zero if we should not autoextend mips16 instructions.
     Changed by `.set autoextend' and `.set noautoextend'.  */
  int noautoextend;
  /* Restrict general purpose registers and floating point registers
     to 32 bit.  This is initially determined when -mgp32 or -mfp32
     is passed but can changed if the assembler code uses .set mipsN.  */
  int gp32;
  int fp32;
};

/* True if -mgp32 was passed.  */
static int file_mips_gp32 = -1;

/* True if -mfp32 was passed.  */
static int file_mips_fp32 = -1;

/* This is the struct we use to hold the current set of options.  Note
   that we must set the isa field to ISA_UNKNOWN and the ASE fields to
   -1 to indicate that they have not been initialized.  */

static struct mips_set_options mips_opts =
{
  ISA_UNKNOWN, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0
};

/* These variables are filled in with the masks of registers used.
   The object format code reads them and puts them in the appropriate
   place.  */
unsigned long mips_gprmask;
unsigned long mips_cprmask[4];

/* MIPS ISA we are using for this output file.  */
static int file_mips_isa = ISA_UNKNOWN;

/* True if -mips16 was passed or implied by arguments passed on the
   command line (e.g., by -march).  */
static int file_ase_mips16;

/* True if -mips3d was passed or implied by arguments passed on the
   command line (e.g., by -march).  */
static int file_ase_mips3d;

/* True if -mdmx was passed or implied by arguments passed on the
   command line (e.g., by -march).  */
static int file_ase_mdmx;

/* The argument of the -march= flag.  The architecture we are assembling.  */
static int mips_arch = CPU_UNKNOWN;
static const char *mips_arch_string;
static const struct mips_cpu_info *mips_arch_info;

/* The argument of the -mtune= flag.  The architecture for which we
   are optimizing.  */
static int mips_tune = CPU_UNKNOWN;
static const char *mips_tune_string;
static const struct mips_cpu_info *mips_tune_info;

/* True when generating 32-bit code for a 64-bit processor.  */
static int mips_32bitmode = 0;

/* Some ISA's have delay slots for instructions which read or write
   from a coprocessor (eg. mips1-mips3); some don't (eg mips4).
   Return true if instructions marked INSN_LOAD_COPROC_DELAY,
   INSN_COPROC_MOVE_DELAY, or INSN_WRITE_COND_CODE actually have a
   delay slot in this ISA.  The uses of this macro assume that any
   ISA that has delay slots for one of these, has them for all.  They
   also assume that ISAs which don't have delays for these insns, don't
   have delays for the INSN_LOAD_MEMORY_DELAY instructions either.  */
#define ISA_HAS_COPROC_DELAYS(ISA) (        \
   (ISA) == ISA_MIPS1                       \
   || (ISA) == ISA_MIPS2                    \
   || (ISA) == ISA_MIPS3                    \
   )

/* True if the given ABI requires 32-bit registers.  */
#define ABI_NEEDS_32BIT_REGS(ABI) ((ABI) == O32_ABI)

/* Likewise 64-bit registers.  */
#define ABI_NEEDS_64BIT_REGS(ABI) \
  ((ABI) == N32_ABI 		  \
   || (ABI) == N64_ABI		  \
   || (ABI) == O64_ABI)

/*  Return true if ISA supports 64 bit gp register instructions.  */
#define ISA_HAS_64BIT_REGS(ISA) (    \
   (ISA) == ISA_MIPS3                \
   || (ISA) == ISA_MIPS4             \
   || (ISA) == ISA_MIPS5             \
   || (ISA) == ISA_MIPS64            \
   )

#define HAVE_32BIT_GPRS		                   \
    (mips_opts.gp32 || ! ISA_HAS_64BIT_REGS (mips_opts.isa))

#define HAVE_32BIT_FPRS                            \
    (mips_opts.fp32 || ! ISA_HAS_64BIT_REGS (mips_opts.isa))

#define HAVE_64BIT_GPRS (! HAVE_32BIT_GPRS)
#define HAVE_64BIT_FPRS (! HAVE_32BIT_FPRS)

#define HAVE_NEWABI (mips_abi == N32_ABI || mips_abi == N64_ABI)

#define HAVE_64BIT_OBJECTS (mips_abi == N64_ABI)

/* We can only have 64bit addresses if the object file format
   supports it.  */
#define HAVE_32BIT_ADDRESSES                           \
   (HAVE_32BIT_GPRS                                    \
    || ((bfd_arch_bits_per_address (stdoutput) == 32   \
         || ! HAVE_64BIT_OBJECTS)                      \
        && mips_pic != EMBEDDED_PIC))

#define HAVE_64BIT_ADDRESSES (! HAVE_32BIT_ADDRESSES)

/* Return true if the given CPU supports the MIPS16 ASE.  */
#define CPU_HAS_MIPS16(cpu)						\
   (strncmp (TARGET_CPU, "mips16", sizeof ("mips16") - 1) == 0		\
    || strncmp (TARGET_CANONICAL, "mips-lsi-elf", sizeof ("mips-lsi-elf") - 1) == 0)

/* Return true if the given CPU supports the MIPS3D ASE.  */
#define CPU_HAS_MIPS3D(cpu)	((cpu) == CPU_SB1      \
				 )

/* Return true if the given CPU supports the MDMX ASE.  */
#define CPU_HAS_MDMX(cpu)	(false                 \
				 )

/* True if CPU has a dror instruction.  */
#define CPU_HAS_DROR(CPU)	((CPU) == CPU_VR5400 || (CPU) == CPU_VR5500)

/* True if CPU has a ror instruction.  */
#define CPU_HAS_ROR(CPU)	CPU_HAS_DROR (CPU)

/* Whether the processor uses hardware interlocks to protect
   reads from the HI and LO registers, and thus does not
   require nops to be inserted.  */

#define hilo_interlocks (mips_arch == CPU_R4010                       \
                         || mips_arch == CPU_VR5500                   \
                         || mips_arch == CPU_SB1                      \
                         )

/* Whether the processor uses hardware interlocks to protect reads
   from the GPRs, and thus does not require nops to be inserted.  */
#define gpr_interlocks \
  (mips_opts.isa != ISA_MIPS1  \
   || mips_arch == CPU_VR5400  \
   || mips_arch == CPU_VR5500  \
   || mips_arch == CPU_R3900)

/* As with other "interlocks" this is used by hardware that has FP
   (co-processor) interlocks.  */
/* Itbl support may require additional care here.  */
#define cop_interlocks (mips_arch == CPU_R4300                        \
                        || mips_arch == CPU_VR5400                    \
                        || mips_arch == CPU_VR5500                    \
                        || mips_arch == CPU_SB1                       \
			)

/* Is this a mfhi or mflo instruction?  */
#define MF_HILO_INSN(PINFO) \
          ((PINFO & INSN_READ_HI) || (PINFO & INSN_READ_LO))

/* MIPS PIC level.  */

enum mips_pic_level mips_pic;

/* Warn about all NOPS that the assembler generates.  */
static int warn_nops = 0;

/* 1 if we should generate 32 bit offsets from the $gp register in
   SVR4_PIC mode.  Currently has no meaning in other modes.  */
static int mips_big_got = 0;

/* 1 if trap instructions should used for overflow rather than break
   instructions.  */
static int mips_trap = 0;

/* 1 if double width floating point constants should not be constructed
   by assembling two single width halves into two single width floating
   point registers which just happen to alias the double width destination
   register.  On some architectures this aliasing can be disabled by a bit
   in the status register, and the setting of this bit cannot be determined
   automatically at assemble time.  */
static int mips_disable_float_construction;

/* Non-zero if any .set noreorder directives were used.  */

static int mips_any_noreorder;

/* Non-zero if nops should be inserted when the register referenced in
   an mfhi/mflo instruction is read in the next two instructions.  */
static int mips_7000_hilo_fix;

/* The size of the small data section.  */
static unsigned int g_switch_value = 8;
/* Whether the -G option was used.  */
static int g_switch_seen = 0;

#define N_RMASK 0xc4
#define N_VFP   0xd4

/* If we can determine in advance that GP optimization won't be
   possible, we can skip the relaxation stuff that tries to produce
   GP-relative references.  This makes delay slot optimization work
   better.

   This function can only provide a guess, but it seems to work for
   gcc output.  It needs to guess right for gcc, otherwise gcc
   will put what it thinks is a GP-relative instruction in a branch
   delay slot.

   I don't know if a fix is needed for the SVR4_PIC mode.  I've only
   fixed it for the non-PIC mode.  KR 95/04/07  */
static int nopic_need_relax PARAMS ((symbolS *, int));

/* handle of the OPCODE hash table */
static struct hash_control *op_hash = NULL;

/* The opcode hash table we use for the mips16.  */
static struct hash_control *mips16_op_hash = NULL;

/* This array holds the chars that always start a comment.  If the
    pre-processor is disabled, these aren't very useful */
const char comment_chars[] = "#";

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.  */
/* Also note that C style comments are always supported.  */
const char line_comment_chars[] = "#";

/* This array holds machine specific line separator characters.  */
const char line_separator_chars[] = ";";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c .  Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.
 */

static char *insn_error;

static int auto_align = 1;

/* When outputting SVR4 PIC code, the assembler needs to know the
   offset in the stack frame from which to restore the $gp register.
   This is set by the .cprestore pseudo-op, and saved in this
   variable.  */
static offsetT mips_cprestore_offset = -1;

/* Similiar for NewABI PIC code, where $gp is callee-saved.  NewABI has some
   more optimizations, it can use a register value instead of a memory-saved
   offset and even an other register than $gp as global pointer.  */
static offsetT mips_cpreturn_offset = -1;
static int mips_cpreturn_register = -1;
static int mips_gp_register = GP;
static int mips_gprel_offset = 0;

/* Whether mips_cprestore_offset has been set in the current function
   (or whether it has already been warned about, if not).  */
static int mips_cprestore_valid = 0;

/* This is the register which holds the stack frame, as set by the
   .frame pseudo-op.  This is needed to implement .cprestore.  */
static int mips_frame_reg = SP;

/* Whether mips_frame_reg has been set in the current function
   (or whether it has already been warned about, if not).  */
static int mips_frame_reg_valid = 0;

/* To output NOP instructions correctly, we need to keep information
   about the previous two instructions.  */

/* Whether we are optimizing.  The default value of 2 means to remove
   unneeded NOPs and swap branch instructions when possible.  A value
   of 1 means to not swap branches.  A value of 0 means to always
   insert NOPs.  */
static int mips_optimize = 2;

/* Debugging level.  -g sets this to 2.  -gN sets this to N.  -g0 is
   equivalent to seeing no -g option at all.  */
static int mips_debug = 0;

/* The previous instruction.  */
static struct mips_cl_insn prev_insn;

/* The instruction before prev_insn.  */
static struct mips_cl_insn prev_prev_insn;

/* If we don't want information for prev_insn or prev_prev_insn, we
   point the insn_mo field at this dummy integer.  */
static const struct mips_opcode dummy_opcode = { NULL, NULL, 0, 0, 0, 0 };

/* Non-zero if prev_insn is valid.  */
static int prev_insn_valid;

/* The frag for the previous instruction.  */
static struct frag *prev_insn_frag;

/* The offset into prev_insn_frag for the previous instruction.  */
static long prev_insn_where;

/* The reloc type for the previous instruction, if any.  */
static bfd_reloc_code_real_type prev_insn_reloc_type[3];

/* The reloc for the previous instruction, if any.  */
static fixS *prev_insn_fixp[3];

/* Non-zero if the previous instruction was in a delay slot.  */
static int prev_insn_is_delay_slot;

/* Non-zero if the previous instruction was in a .set noreorder.  */
static int prev_insn_unreordered;

/* Non-zero if the previous instruction uses an extend opcode (if
   mips16).  */
static int prev_insn_extended;

/* Non-zero if the previous previous instruction was in a .set
   noreorder.  */
static int prev_prev_insn_unreordered;

/* If this is set, it points to a frag holding nop instructions which
   were inserted before the start of a noreorder section.  If those
   nops turn out to be unnecessary, the size of the frag can be
   decreased.  */
static fragS *prev_nop_frag;

/* The number of nop instructions we created in prev_nop_frag.  */
static int prev_nop_frag_holds;

/* The number of nop instructions that we know we need in
   prev_nop_frag.  */
static int prev_nop_frag_required;

/* The number of instructions we've seen since prev_nop_frag.  */
static int prev_nop_frag_since;

/* For ECOFF and ELF, relocations against symbols are done in two
   parts, with a HI relocation and a LO relocation.  Each relocation
   has only 16 bits of space to store an addend.  This means that in
   order for the linker to handle carries correctly, it must be able
   to locate both the HI and the LO relocation.  This means that the
   relocations must appear in order in the relocation table.

   In order to implement this, we keep track of each unmatched HI
   relocation.  We then sort them so that they immediately precede the
   corresponding LO relocation.  */

struct mips_hi_fixup
{
  /* Next HI fixup.  */
  struct mips_hi_fixup *next;
  /* This fixup.  */
  fixS *fixp;
  /* The section this fixup is in.  */
  segT seg;
};

/* The list of unmatched HI relocs.  */

static struct mips_hi_fixup *mips_hi_fixup_list;

/* Map normal MIPS register numbers to mips16 register numbers.  */

#define X ILLEGAL_REG
static const int mips32_to_16_reg_map[] =
{
  X, X, 2, 3, 4, 5, 6, 7,
  X, X, X, X, X, X, X, X,
  0, 1, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X
};
#undef X

/* Map mips16 register numbers to normal MIPS register numbers.  */

static const unsigned int mips16_to_32_reg_map[] =
{
  16, 17, 2, 3, 4, 5, 6, 7
};

static int mips_fix_4122_bugs;

/* We don't relax branches by default, since this causes us to expand
   `la .l2 - .l1' if there's a branch between .l1 and .l2, because we
   fail to compute the offset before expanding the macro to the most
   efficient expansion.  */

static int mips_relax_branch;

/* Since the MIPS does not have multiple forms of PC relative
   instructions, we do not have to do relaxing as is done on other
   platforms.  However, we do have to handle GP relative addressing
   correctly, which turns out to be a similar problem.

   Every macro that refers to a symbol can occur in (at least) two
   forms, one with GP relative addressing and one without.  For
   example, loading a global variable into a register generally uses
   a macro instruction like this:
     lw $4,i
   If i can be addressed off the GP register (this is true if it is in
   the .sbss or .sdata section, or if it is known to be smaller than
   the -G argument) this will generate the following instruction:
     lw $4,i($gp)
   This instruction will use a GPREL reloc.  If i can not be addressed
   off the GP register, the following instruction sequence will be used:
     lui $at,i
     lw $4,i($at)
   In this case the first instruction will have a HI16 reloc, and the
   second reloc will have a LO16 reloc.  Both relocs will be against
   the symbol i.

   The issue here is that we may not know whether i is GP addressable
   until after we see the instruction that uses it.  Therefore, we
   want to be able to choose the final instruction sequence only at
   the end of the assembly.  This is similar to the way other
   platforms choose the size of a PC relative instruction only at the
   end of assembly.

   When generating position independent code we do not use GP
   addressing in quite the same way, but the issue still arises as
   external symbols and local symbols must be handled differently.

   We handle these issues by actually generating both possible
   instruction sequences.  The longer one is put in a frag_var with
   type rs_machine_dependent.  We encode what to do with the frag in
   the subtype field.  We encode (1) the number of existing bytes to
   replace, (2) the number of new bytes to use, (3) the offset from
   the start of the existing bytes to the first reloc we must generate
   (that is, the offset is applied from the start of the existing
   bytes after they are replaced by the new bytes, if any), (4) the
   offset from the start of the existing bytes to the second reloc,
   (5) whether a third reloc is needed (the third reloc is always four
   bytes after the second reloc), and (6) whether to warn if this
   variant is used (this is sometimes needed if .set nomacro or .set
   noat is in effect).  All these numbers are reasonably small.

   Generating two instruction sequences must be handled carefully to
   ensure that delay slots are handled correctly.  Fortunately, there
   are a limited number of cases.  When the second instruction
   sequence is generated, append_insn is directed to maintain the
   existing delay slot information, so it continues to apply to any
   code after the second instruction sequence.  This means that the
   second instruction sequence must not impose any requirements not
   required by the first instruction sequence.

   These variant frags are then handled in functions called by the
   machine independent code.  md_estimate_size_before_relax returns
   the final size of the frag.  md_convert_frag sets up the final form
   of the frag.  tc_gen_reloc adjust the first reloc and adds a second
   one if needed.  */
#define RELAX_ENCODE(old, new, reloc1, reloc2, reloc3, warn) \
  ((relax_substateT) \
   (((old) << 23) \
    | ((new) << 16) \
    | (((reloc1) + 64) << 9) \
    | (((reloc2) + 64) << 2) \
    | ((reloc3) ? (1 << 1) : 0) \
    | ((warn) ? 1 : 0)))
#define RELAX_OLD(i) (((i) >> 23) & 0x7f)
#define RELAX_NEW(i) (((i) >> 16) & 0x7f)
#define RELAX_RELOC1(i) ((valueT) (((i) >> 9) & 0x7f) - 64)
#define RELAX_RELOC2(i) ((valueT) (((i) >> 2) & 0x7f) - 64)
#define RELAX_RELOC3(i) (((i) >> 1) & 1)
#define RELAX_WARN(i) ((i) & 1)

/* Branch without likely bit.  If label is out of range, we turn:

 	beq reg1, reg2, label
	delay slot

   into

        bne reg1, reg2, 0f
        nop
        j label
     0: delay slot

   with the following opcode replacements:

	beq <-> bne
	blez <-> bgtz
	bltz <-> bgez
	bc1f <-> bc1t

	bltzal <-> bgezal  (with jal label instead of j label)

   Even though keeping the delay slot instruction in the delay slot of
   the branch would be more efficient, it would be very tricky to do
   correctly, because we'd have to introduce a variable frag *after*
   the delay slot instruction, and expand that instead.  Let's do it
   the easy way for now, even if the branch-not-taken case now costs
   one additional instruction.  Out-of-range branches are not supposed
   to be common, anyway.

   Branch likely.  If label is out of range, we turn:

	beql reg1, reg2, label
	delay slot (annulled if branch not taken)

   into

        beql reg1, reg2, 1f
        nop
        beql $0, $0, 2f
        nop
     1: j[al] label
        delay slot (executed only if branch taken)
     2:

   It would be possible to generate a shorter sequence by losing the
   likely bit, generating something like:
     
	bne reg1, reg2, 0f
	nop
	j[al] label
	delay slot (executed only if branch taken)
     0:

	beql -> bne
	bnel -> beq
	blezl -> bgtz
	bgtzl -> blez
	bltzl -> bgez
	bgezl -> bltz
	bc1fl -> bc1t
	bc1tl -> bc1f

	bltzall -> bgezal  (with jal label instead of j label)
	bgezall -> bltzal  (ditto)


   but it's not clear that it would actually improve performance.  */
#define RELAX_BRANCH_ENCODE(reloc_s2, uncond, likely, link, toofar) \
  ((relax_substateT) \
   (0xc0000000 \
    | ((toofar) ? 1 : 0) \
    | ((link) ? 2 : 0) \
    | ((likely) ? 4 : 0) \
    | ((uncond) ? 8 : 0) \
    | ((reloc_s2) ? 16 : 0)))
#define RELAX_BRANCH_P(i) (((i) & 0xf0000000) == 0xc0000000)
#define RELAX_BRANCH_RELOC_S2(i) (((i) & 16) != 0)
#define RELAX_BRANCH_UNCOND(i) (((i) & 8) != 0)
#define RELAX_BRANCH_LIKELY(i) (((i) & 4) != 0)
#define RELAX_BRANCH_LINK(i) (((i) & 2) != 0)
#define RELAX_BRANCH_TOOFAR(i) (((i) & 1))

/* For mips16 code, we use an entirely different form of relaxation.
   mips16 supports two versions of most instructions which take
   immediate values: a small one which takes some small value, and a
   larger one which takes a 16 bit value.  Since branches also follow
   this pattern, relaxing these values is required.

   We can assemble both mips16 and normal MIPS code in a single
   object.  Therefore, we need to support this type of relaxation at
   the same time that we support the relaxation described above.  We
   use the high bit of the subtype field to distinguish these cases.

   The information we store for this type of relaxation is the
   argument code found in the opcode file for this relocation, whether
   the user explicitly requested a small or extended form, and whether
   the relocation is in a jump or jal delay slot.  That tells us the
   size of the value, and how it should be stored.  We also store
   whether the fragment is considered to be extended or not.  We also
   store whether this is known to be a branch to a different section,
   whether we have tried to relax this frag yet, and whether we have
   ever extended a PC relative fragment because of a shift count.  */
#define RELAX_MIPS16_ENCODE(type, small, ext, dslot, jal_dslot)	\
  (0x80000000							\
   | ((type) & 0xff)						\
   | ((small) ? 0x100 : 0)					\
   | ((ext) ? 0x200 : 0)					\
   | ((dslot) ? 0x400 : 0)					\
   | ((jal_dslot) ? 0x800 : 0))
#define RELAX_MIPS16_P(i) (((i) & 0xc0000000) == 0x80000000)
#define RELAX_MIPS16_TYPE(i) ((i) & 0xff)
#define RELAX_MIPS16_USER_SMALL(i) (((i) & 0x100) != 0)
#define RELAX_MIPS16_USER_EXT(i) (((i) & 0x200) != 0)
#define RELAX_MIPS16_DSLOT(i) (((i) & 0x400) != 0)
#define RELAX_MIPS16_JAL_DSLOT(i) (((i) & 0x800) != 0)
#define RELAX_MIPS16_EXTENDED(i) (((i) & 0x1000) != 0)
#define RELAX_MIPS16_MARK_EXTENDED(i) ((i) | 0x1000)
#define RELAX_MIPS16_CLEAR_EXTENDED(i) ((i) &~ 0x1000)
#define RELAX_MIPS16_LONG_BRANCH(i) (((i) & 0x2000) != 0)
#define RELAX_MIPS16_MARK_LONG_BRANCH(i) ((i) | 0x2000)
#define RELAX_MIPS16_CLEAR_LONG_BRANCH(i) ((i) &~ 0x2000)

/* Is the given value a sign-extended 32-bit value?  */
#define IS_SEXT_32BIT_NUM(x)						\
  (((x) &~ (offsetT) 0x7fffffff) == 0					\
   || (((x) &~ (offsetT) 0x7fffffff) == ~ (offsetT) 0x7fffffff))

/* Is the given value a sign-extended 16-bit value?  */
#define IS_SEXT_16BIT_NUM(x)						\
  (((x) &~ (offsetT) 0x7fff) == 0					\
   || (((x) &~ (offsetT) 0x7fff) == ~ (offsetT) 0x7fff))


/* Prototypes for static functions.  */

#ifdef __STDC__
#define internalError() \
    as_fatal (_("internal Error, line %d, %s"), __LINE__, __FILE__)
#else
#define internalError() as_fatal (_("MIPS internal Error"));
#endif

enum mips_regclass { MIPS_GR_REG, MIPS_FP_REG, MIPS16_REG };

static int insn_uses_reg PARAMS ((struct mips_cl_insn *ip,
				  unsigned int reg, enum mips_regclass class));
static int reg_needs_delay PARAMS ((unsigned int));
static void mips16_mark_labels PARAMS ((void));
static void append_insn PARAMS ((char *place,
				 struct mips_cl_insn * ip,
				 expressionS * p,
				 bfd_reloc_code_real_type *r,
				 boolean));
static void mips_no_prev_insn PARAMS ((int));
static void mips_emit_delays PARAMS ((boolean));
#ifdef USE_STDARG
static void macro_build PARAMS ((char *place, int *counter, expressionS * ep,
				 const char *name, const char *fmt,
				 ...));
#else
static void macro_build ();
#endif
static void mips16_macro_build PARAMS ((char *, int *, expressionS *,
					const char *, const char *,
					va_list));
static void macro_build_jalr PARAMS ((int, expressionS *));
static void macro_build_lui PARAMS ((char *place, int *counter,
				     expressionS * ep, int regnum));
static void macro_build_ldst_constoffset PARAMS ((char *place, int *counter,
						  expressionS * ep, const char *op,
						  int valreg, int breg));
static void set_at PARAMS ((int *counter, int reg, int unsignedp));
static void check_absolute_expr PARAMS ((struct mips_cl_insn * ip,
					 expressionS *));
static void load_register PARAMS ((int *, int, expressionS *, int));
static void load_address PARAMS ((int *, int, expressionS *, int *));
static void move_register PARAMS ((int *, int, int));
static void macro PARAMS ((struct mips_cl_insn * ip));
static void mips16_macro PARAMS ((struct mips_cl_insn * ip));
#ifdef LOSING_COMPILER
static void macro2 PARAMS ((struct mips_cl_insn * ip));
#endif
static void mips_ip PARAMS ((char *str, struct mips_cl_insn * ip));
static void mips16_ip PARAMS ((char *str, struct mips_cl_insn * ip));
static void mips16_immed PARAMS ((char *, unsigned int, int, offsetT, boolean,
				  boolean, boolean, unsigned long *,
				  boolean *, unsigned short *));
static int my_getPercentOp PARAMS ((char **, unsigned int *, int *));
static int my_getSmallParser PARAMS ((char **, unsigned int *, int *));
static int my_getSmallExpression PARAMS ((expressionS *, char *));
static void my_getExpression PARAMS ((expressionS *, char *));
#ifdef OBJ_ELF
static int support_64bit_objects PARAMS((void));
#endif
static void mips_set_option_string PARAMS ((const char **, const char *));
static symbolS *get_symbol PARAMS ((void));
static void mips_align PARAMS ((int to, int fill, symbolS *label));
static void s_align PARAMS ((int));
static void s_change_sec PARAMS ((int));
static void s_change_section PARAMS ((int));
static void s_cons PARAMS ((int));
static void s_float_cons PARAMS ((int));
static void s_mips_globl PARAMS ((int));
static void s_option PARAMS ((int));
static void s_mipsset PARAMS ((int));
static void s_abicalls PARAMS ((int));
static void s_cpload PARAMS ((int));
static void s_cpsetup PARAMS ((int));
static void s_cplocal PARAMS ((int));
static void s_cprestore PARAMS ((int));
static void s_cpreturn PARAMS ((int));
static void s_gpvalue PARAMS ((int));
static void s_gpword PARAMS ((int));
static void s_gpdword PARAMS ((int));
static void s_cpadd PARAMS ((int));
static void s_insn PARAMS ((int));
static void md_obj_begin PARAMS ((void));
static void md_obj_end PARAMS ((void));
static long get_number PARAMS ((void));
static void s_mips_ent PARAMS ((int));
static void s_mips_end PARAMS ((int));
static void s_mips_frame PARAMS ((int));
static void s_mips_mask PARAMS ((int));
static void s_mips_stab PARAMS ((int));
static void s_mips_weakext PARAMS ((int));
static void s_mips_file PARAMS ((int));
static void s_mips_loc PARAMS ((int));
static int mips16_extended_frag PARAMS ((fragS *, asection *, long));
static int relaxed_branch_length (fragS *, asection *, int);
static int validate_mips_insn PARAMS ((const struct mips_opcode *));
static void show PARAMS ((FILE *, const char *, int *, int *));
#ifdef OBJ_ELF
static int mips_need_elf_addend_fixup PARAMS ((fixS *));
#endif

/* Return values of my_getSmallExpression().  */

enum small_ex_type
{
  S_EX_NONE = 0,
  S_EX_REGISTER,

  /* Direct relocation creation by %percent_op().  */
  S_EX_HALF,
  S_EX_HI,
  S_EX_LO,
  S_EX_GP_REL,
  S_EX_GOT,
  S_EX_CALL16,
  S_EX_GOT_DISP,
  S_EX_GOT_PAGE,
  S_EX_GOT_OFST,
  S_EX_GOT_HI,
  S_EX_GOT_LO,
  S_EX_NEG,
  S_EX_HIGHER,
  S_EX_HIGHEST,
  S_EX_CALL_HI,
  S_EX_CALL_LO
};

/* Table and functions used to map between CPU/ISA names, and
   ISA levels, and CPU numbers.  */

struct mips_cpu_info
{
  const char *name;           /* CPU or ISA name.  */
  int is_isa;                 /* Is this an ISA?  (If 0, a CPU.) */
  int isa;                    /* ISA level.  */
  int cpu;                    /* CPU number (default CPU if ISA).  */
};

static void mips_set_architecture PARAMS ((const struct mips_cpu_info *));
static void mips_set_tune PARAMS ((const struct mips_cpu_info *));
static boolean mips_strict_matching_cpu_name_p PARAMS ((const char *,
							const char *));
static boolean mips_matching_cpu_name_p PARAMS ((const char *, const char *));
static const struct mips_cpu_info *mips_parse_cpu PARAMS ((const char *,
							   const char *));
static const struct mips_cpu_info *mips_cpu_info_from_isa PARAMS ((int));

/* Pseudo-op table.

   The following pseudo-ops from the Kane and Heinrich MIPS book
   should be defined here, but are currently unsupported: .alias,
   .galive, .gjaldef, .gjrlive, .livereg, .noalias.

   The following pseudo-ops from the Kane and Heinrich MIPS book are
   specific to the type of debugging information being generated, and
   should be defined by the object format: .aent, .begin, .bend,
   .bgnb, .end, .endb, .ent, .fmask, .frame, .loc, .mask, .verstamp,
   .vreg.

   The following pseudo-ops from the Kane and Heinrich MIPS book are
   not MIPS CPU specific, but are also not specific to the object file
   format.  This file is probably the best place to define them, but
   they are not currently supported: .asm0, .endr, .lab, .repeat,
   .struct.  */

static const pseudo_typeS mips_pseudo_table[] =
{
  /* MIPS specific pseudo-ops.  */
  {"option", s_option, 0},
  {"set", s_mipsset, 0},
  {"rdata", s_change_sec, 'r'},
  {"sdata", s_change_sec, 's'},
  {"livereg", s_ignore, 0},
  {"abicalls", s_abicalls, 0},
  {"cpload", s_cpload, 0},
  {"cpsetup", s_cpsetup, 0},
  {"cplocal", s_cplocal, 0},
  {"cprestore", s_cprestore, 0},
  {"cpreturn", s_cpreturn, 0},
  {"gpvalue", s_gpvalue, 0},
  {"gpword", s_gpword, 0},
  {"gpdword", s_gpdword, 0},
  {"cpadd", s_cpadd, 0},
  {"insn", s_insn, 0},

  /* Relatively generic pseudo-ops that happen to be used on MIPS
     chips.  */
  {"asciiz", stringer, 1},
  {"bss", s_change_sec, 'b'},
  {"err", s_err, 0},
  {"half", s_cons, 1},
  {"dword", s_cons, 3},
  {"weakext", s_mips_weakext, 0},

  /* These pseudo-ops are defined in read.c, but must be overridden
     here for one reason or another.  */
  {"align", s_align, 0},
  {"byte", s_cons, 0},
  {"data", s_change_sec, 'd'},
  {"double", s_float_cons, 'd'},
  {"float", s_float_cons, 'f'},
  {"globl", s_mips_globl, 0},
  {"global", s_mips_globl, 0},
  {"hword", s_cons, 1},
  {"int", s_cons, 2},
  {"long", s_cons, 2},
  {"octa", s_cons, 4},
  {"quad", s_cons, 3},
  {"section", s_change_section, 0},
  {"short", s_cons, 1},
  {"single", s_float_cons, 'f'},
  {"stabn", s_mips_stab, 'n'},
  {"text", s_change_sec, 't'},
  {"word", s_cons, 2},

  { "extern", ecoff_directive_extern, 0},

  { NULL, NULL, 0 },
};

static const pseudo_typeS mips_nonecoff_pseudo_table[] =
{
  /* These pseudo-ops should be defined by the object file format.
     However, a.out doesn't support them, so we have versions here.  */
  {"aent", s_mips_ent, 1},
  {"bgnb", s_ignore, 0},
  {"end", s_mips_end, 0},
  {"endb", s_ignore, 0},
  {"ent", s_mips_ent, 0},
  {"file", s_mips_file, 0},
  {"fmask", s_mips_mask, 'F'},
  {"frame", s_mips_frame, 0},
  {"loc", s_mips_loc, 0},
  {"mask", s_mips_mask, 'R'},
  {"verstamp", s_ignore, 0},
  { NULL, NULL, 0 },
};

extern void pop_insert PARAMS ((const pseudo_typeS *));

void
mips_pop_insert ()
{
  pop_insert (mips_pseudo_table);
  if (! ECOFF_DEBUGGING)
    pop_insert (mips_nonecoff_pseudo_table);
}

/* Symbols labelling the current insn.  */

struct insn_label_list
{
  struct insn_label_list *next;
  symbolS *label;
};

static struct insn_label_list *insn_labels;
static struct insn_label_list *free_insn_labels;

static void mips_clear_insn_labels PARAMS ((void));

static inline void
mips_clear_insn_labels ()
{
  register struct insn_label_list **pl;

  for (pl = &free_insn_labels; *pl != NULL; pl = &(*pl)->next)
    ;
  *pl = insn_labels;
  insn_labels = NULL;
}

static char *expr_end;

/* Expressions which appear in instructions.  These are set by
   mips_ip.  */

static expressionS imm_expr;
static expressionS offset_expr;

/* Relocs associated with imm_expr and offset_expr.  */

static bfd_reloc_code_real_type imm_reloc[3]
  = {BFD_RELOC_UNUSED, BFD_RELOC_UNUSED, BFD_RELOC_UNUSED};
static bfd_reloc_code_real_type offset_reloc[3]
  = {BFD_RELOC_UNUSED, BFD_RELOC_UNUSED, BFD_RELOC_UNUSED};

/* This is set by mips_ip if imm_reloc is an unmatched HI16_S reloc.  */

static boolean imm_unmatched_hi;

/* These are set by mips16_ip if an explicit extension is used.  */

static boolean mips16_small, mips16_ext;

#ifdef OBJ_ELF
/* The pdr segment for per procedure frame/regmask info.  Not used for
   ECOFF debugging.  */

static segT pdr_seg;
#endif

/* The default target format to use.  */

const char *
mips_target_format ()
{
  switch (OUTPUT_FLAVOR)
    {
    case bfd_target_aout_flavour:
      return target_big_endian ? "a.out-mips-big" : "a.out-mips-little";
    case bfd_target_ecoff_flavour:
      return target_big_endian ? "ecoff-bigmips" : ECOFF_LITTLE_FORMAT;
    case bfd_target_coff_flavour:
      return "pe-mips";
    case bfd_target_elf_flavour:
#ifdef TE_TMIPS
      /* This is traditional mips.  */
      return (target_big_endian
	      ? (HAVE_64BIT_OBJECTS
		 ? "elf64-tradbigmips"
		 : (HAVE_NEWABI
		    ? "elf32-ntradbigmips" : "elf32-tradbigmips"))
	      : (HAVE_64BIT_OBJECTS
		 ? "elf64-tradlittlemips"
		 : (HAVE_NEWABI
		    ? "elf32-ntradlittlemips" : "elf32-tradlittlemips")));
#else
      return (target_big_endian
	      ? (HAVE_64BIT_OBJECTS
		 ? "elf64-bigmips"
		 : (HAVE_NEWABI
		    ? "elf32-nbigmips" : "elf32-bigmips"))
	      : (HAVE_64BIT_OBJECTS
		 ? "elf64-littlemips"
		 : (HAVE_NEWABI
		    ? "elf32-nlittlemips" : "elf32-littlemips")));
#endif
    default:
      abort ();
      return NULL;
    }
}

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc. that the MD part of the assembler will need.  */

void
md_begin ()
{
  register const char *retval = NULL;
  int i = 0;
  int broken = 0;

  if (! bfd_set_arch_mach (stdoutput, bfd_arch_mips, mips_arch))
    as_warn (_("Could not set architecture and machine"));

  op_hash = hash_new ();

  for (i = 0; i < NUMOPCODES;)
    {
      const char *name = mips_opcodes[i].name;

      retval = hash_insert (op_hash, name, (PTR) &mips_opcodes[i]);
      if (retval != NULL)
	{
	  fprintf (stderr, _("internal error: can't hash `%s': %s\n"),
		   mips_opcodes[i].name, retval);
	  /* Probably a memory allocation problem?  Give up now.  */
	  as_fatal (_("Broken assembler.  No assembly attempted."));
	}
      do
	{
	  if (mips_opcodes[i].pinfo != INSN_MACRO)
	    {
	      if (!validate_mips_insn (&mips_opcodes[i]))
		broken = 1;
	    }
	  ++i;
	}
      while ((i < NUMOPCODES) && !strcmp (mips_opcodes[i].name, name));
    }

  mips16_op_hash = hash_new ();

  i = 0;
  while (i < bfd_mips16_num_opcodes)
    {
      const char *name = mips16_opcodes[i].name;

      retval = hash_insert (mips16_op_hash, name, (PTR) &mips16_opcodes[i]);
      if (retval != NULL)
	as_fatal (_("internal: can't hash `%s': %s"),
		  mips16_opcodes[i].name, retval);
      do
	{
	  if (mips16_opcodes[i].pinfo != INSN_MACRO
	      && ((mips16_opcodes[i].match & mips16_opcodes[i].mask)
		  != mips16_opcodes[i].match))
	    {
	      fprintf (stderr, _("internal error: bad mips16 opcode: %s %s\n"),
		       mips16_opcodes[i].name, mips16_opcodes[i].args);
	      broken = 1;
	    }
	  ++i;
	}
      while (i < bfd_mips16_num_opcodes
	     && strcmp (mips16_opcodes[i].name, name) == 0);
    }

  if (broken)
    as_fatal (_("Broken assembler.  No assembly attempted."));

  /* We add all the general register names to the symbol table.  This
     helps us detect invalid uses of them.  */
  for (i = 0; i < 32; i++)
    {
      char buf[5];

      sprintf (buf, "$%d", i);
      symbol_table_insert (symbol_new (buf, reg_section, i,
				       &zero_address_frag));
    }
  symbol_table_insert (symbol_new ("$ra", reg_section, RA,
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("$fp", reg_section, FP,
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("$sp", reg_section, SP,
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("$gp", reg_section, GP,
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("$at", reg_section, AT,
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("$kt0", reg_section, KT0,
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("$kt1", reg_section, KT1,
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("$zero", reg_section, ZERO,
				   &zero_address_frag));
  symbol_table_insert (symbol_new ("$pc", reg_section, -1,
				   &zero_address_frag));

  /* If we don't add these register names to the symbol table, they
     may end up being added as regular symbols by operand(), and then
     make it to the object file as undefined in case they're not
     regarded as local symbols.  They're local in o32, since `$' is a
     local symbol prefix, but not in n32 or n64.  */
  for (i = 0; i < 8; i++)
    {
      char buf[6];

      sprintf (buf, "$fcc%i", i);
      symbol_table_insert (symbol_new (buf, reg_section, -1,
				       &zero_address_frag));
    }

  mips_no_prev_insn (false);

  mips_gprmask = 0;
  mips_cprmask[0] = 0;
  mips_cprmask[1] = 0;
  mips_cprmask[2] = 0;
  mips_cprmask[3] = 0;

  /* set the default alignment for the text section (2**2) */
  record_alignment (text_section, 2);

  if (USE_GLOBAL_POINTER_OPT)
    bfd_set_gp_size (stdoutput, g_switch_value);

  if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
    {
      /* On a native system, sections must be aligned to 16 byte
	 boundaries.  When configured for an embedded ELF target, we
	 don't bother.  */
      if (strcmp (TARGET_OS, "elf") != 0)
	{
	  (void) bfd_set_section_alignment (stdoutput, text_section, 4);
	  (void) bfd_set_section_alignment (stdoutput, data_section, 4);
	  (void) bfd_set_section_alignment (stdoutput, bss_section, 4);
	}

      /* Create a .reginfo section for register masks and a .mdebug
	 section for debugging information.  */
      {
	segT seg;
	subsegT subseg;
	flagword flags;
	segT sec;

	seg = now_seg;
	subseg = now_subseg;

	/* The ABI says this section should be loaded so that the
	   running program can access it.  However, we don't load it
	   if we are configured for an embedded target */
	flags = SEC_READONLY | SEC_DATA;
	if (strcmp (TARGET_OS, "elf") != 0)
	  flags |= SEC_ALLOC | SEC_LOAD;

	if (mips_abi != N64_ABI)
	  {
	    sec = subseg_new (".reginfo", (subsegT) 0);

	    bfd_set_section_flags (stdoutput, sec, flags);
	    bfd_set_section_alignment (stdoutput, sec, HAVE_NEWABI ? 3 : 2);

#ifdef OBJ_ELF
	    mips_regmask_frag = frag_more (sizeof (Elf32_External_RegInfo));
#endif
	  }
	else
	  {
	    /* The 64-bit ABI uses a .MIPS.options section rather than
               .reginfo section.  */
	    sec = subseg_new (".MIPS.options", (subsegT) 0);
	    bfd_set_section_flags (stdoutput, sec, flags);
	    bfd_set_section_alignment (stdoutput, sec, 3);

#ifdef OBJ_ELF
	    /* Set up the option header.  */
	    {
	      Elf_Internal_Options opthdr;
	      char *f;

	      opthdr.kind = ODK_REGINFO;
	      opthdr.size = (sizeof (Elf_External_Options)
			     + sizeof (Elf64_External_RegInfo));
	      opthdr.section = 0;
	      opthdr.info = 0;
	      f = frag_more (sizeof (Elf_External_Options));
	      bfd_mips_elf_swap_options_out (stdoutput, &opthdr,
					     (Elf_External_Options *) f);

	      mips_regmask_frag = frag_more (sizeof (Elf64_External_RegInfo));
	    }
#endif
	  }

	if (ECOFF_DEBUGGING)
	  {
	    sec = subseg_new (".mdebug", (subsegT) 0);
	    (void) bfd_set_section_flags (stdoutput, sec,
					  SEC_HAS_CONTENTS | SEC_READONLY);
	    (void) bfd_set_section_alignment (stdoutput, sec, 2);
	  }
#ifdef OBJ_ELF
	else if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
	  {
	    pdr_seg = subseg_new (".pdr", (subsegT) 0);
	    (void) bfd_set_section_flags (stdoutput, pdr_seg,
					  SEC_READONLY | SEC_RELOC
					  | SEC_DEBUGGING);
	    (void) bfd_set_section_alignment (stdoutput, pdr_seg, 2);
	  }
#endif

	subseg_set (seg, subseg);
      }
    }

  if (! ECOFF_DEBUGGING)
    md_obj_begin ();
}

void
md_mips_end ()
{
  if (! ECOFF_DEBUGGING)
    md_obj_end ();
}

void
md_assemble (str)
     char *str;
{
  struct mips_cl_insn insn;
  bfd_reloc_code_real_type unused_reloc[3]
    = {BFD_RELOC_UNUSED, BFD_RELOC_UNUSED, BFD_RELOC_UNUSED};

  imm_expr.X_op = O_absent;
  imm_unmatched_hi = false;
  offset_expr.X_op = O_absent;
  imm_reloc[0] = BFD_RELOC_UNUSED;
  imm_reloc[1] = BFD_RELOC_UNUSED;
  imm_reloc[2] = BFD_RELOC_UNUSED;
  offset_reloc[0] = BFD_RELOC_UNUSED;
  offset_reloc[1] = BFD_RELOC_UNUSED;
  offset_reloc[2] = BFD_RELOC_UNUSED;

  if (mips_opts.mips16)
    mips16_ip (str, &insn);
  else
    {
      mips_ip (str, &insn);
      DBG ((_("returned from mips_ip(%s) insn_opcode = 0x%x\n"),
	    str, insn.insn_opcode));
    }

  if (insn_error)
    {
      as_bad ("%s `%s'", insn_error, str);
      return;
    }

  if (insn.insn_mo->pinfo == INSN_MACRO)
    {
      if (mips_opts.mips16)
	mips16_macro (&insn);
      else
	macro (&insn);
    }
  else
    {
      if (imm_expr.X_op != O_absent)
	append_insn (NULL, &insn, &imm_expr, imm_reloc, imm_unmatched_hi);
      else if (offset_expr.X_op != O_absent)
	append_insn (NULL, &insn, &offset_expr, offset_reloc, false);
      else
	append_insn (NULL, &insn, NULL, unused_reloc, false);
    }
}

/* See whether instruction IP reads register REG.  CLASS is the type
   of register.  */

static int
insn_uses_reg (ip, reg, class)
     struct mips_cl_insn *ip;
     unsigned int reg;
     enum mips_regclass class;
{
  if (class == MIPS16_REG)
    {
      assert (mips_opts.mips16);
      reg = mips16_to_32_reg_map[reg];
      class = MIPS_GR_REG;
    }

  /* Don't report on general register ZERO, since it never changes.  */
  if (class == MIPS_GR_REG && reg == ZERO)
    return 0;

  if (class == MIPS_FP_REG)
    {
      assert (! mips_opts.mips16);
      /* If we are called with either $f0 or $f1, we must check $f0.
	 This is not optimal, because it will introduce an unnecessary
	 NOP between "lwc1 $f0" and "swc1 $f1".  To fix this we would
	 need to distinguish reading both $f0 and $f1 or just one of
	 them.  Note that we don't have to check the other way,
	 because there is no instruction that sets both $f0 and $f1
	 and requires a delay.  */
      if ((ip->insn_mo->pinfo & INSN_READ_FPR_S)
	  && ((((ip->insn_opcode >> OP_SH_FS) & OP_MASK_FS) &~(unsigned)1)
	      == (reg &~ (unsigned) 1)))
	return 1;
      if ((ip->insn_mo->pinfo & INSN_READ_FPR_T)
	  && ((((ip->insn_opcode >> OP_SH_FT) & OP_MASK_FT) &~(unsigned)1)
	      == (reg &~ (unsigned) 1)))
	return 1;
    }
  else if (! mips_opts.mips16)
    {
      if ((ip->insn_mo->pinfo & INSN_READ_GPR_S)
	  && ((ip->insn_opcode >> OP_SH_RS) & OP_MASK_RS) == reg)
	return 1;
      if ((ip->insn_mo->pinfo & INSN_READ_GPR_T)
	  && ((ip->insn_opcode >> OP_SH_RT) & OP_MASK_RT) == reg)
	return 1;
    }
  else
    {
      if ((ip->insn_mo->pinfo & MIPS16_INSN_READ_X)
	  && (mips16_to_32_reg_map[((ip->insn_opcode >> MIPS16OP_SH_RX)
				    & MIPS16OP_MASK_RX)]
	      == reg))
	return 1;
      if ((ip->insn_mo->pinfo & MIPS16_INSN_READ_Y)
	  && (mips16_to_32_reg_map[((ip->insn_opcode >> MIPS16OP_SH_RY)
				    & MIPS16OP_MASK_RY)]
	      == reg))
	return 1;
      if ((ip->insn_mo->pinfo & MIPS16_INSN_READ_Z)
	  && (mips16_to_32_reg_map[((ip->insn_opcode >> MIPS16OP_SH_MOVE32Z)
				    & MIPS16OP_MASK_MOVE32Z)]
	      == reg))
	return 1;
      if ((ip->insn_mo->pinfo & MIPS16_INSN_READ_T) && reg == TREG)
	return 1;
      if ((ip->insn_mo->pinfo & MIPS16_INSN_READ_SP) && reg == SP)
	return 1;
      if ((ip->insn_mo->pinfo & MIPS16_INSN_READ_31) && reg == RA)
	return 1;
      if ((ip->insn_mo->pinfo & MIPS16_INSN_READ_GPR_X)
	  && ((ip->insn_opcode >> MIPS16OP_SH_REGR32)
	      & MIPS16OP_MASK_REGR32) == reg)
	return 1;
    }

  return 0;
}

/* This function returns true if modifying a register requires a
   delay.  */

static int
reg_needs_delay (reg)
     unsigned int reg;
{
  unsigned long prev_pinfo;

  prev_pinfo = prev_insn.insn_mo->pinfo;
  if (! mips_opts.noreorder
      && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
      && ((prev_pinfo & INSN_LOAD_COPROC_DELAY)
	  || (! gpr_interlocks
	      && (prev_pinfo & INSN_LOAD_MEMORY_DELAY))))
    {
      /* A load from a coprocessor or from memory.  All load
	 delays delay the use of general register rt for one
	 instruction on the r3000.  The r6000 and r4000 use
	 interlocks.  */
      /* Itbl support may require additional care here.  */
      know (prev_pinfo & INSN_WRITE_GPR_T);
      if (reg == ((prev_insn.insn_opcode >> OP_SH_RT) & OP_MASK_RT))
	return 1;
    }

  return 0;
}

/* Mark instruction labels in mips16 mode.  This permits the linker to
   handle them specially, such as generating jalx instructions when
   needed.  We also make them odd for the duration of the assembly, in
   order to generate the right sort of code.  We will make them even
   in the adjust_symtab routine, while leaving them marked.  This is
   convenient for the debugger and the disassembler.  The linker knows
   to make them odd again.  */

static void
mips16_mark_labels ()
{
  if (mips_opts.mips16)
    {
      struct insn_label_list *l;
      valueT val;

      for (l = insn_labels; l != NULL; l = l->next)
	{
#ifdef OBJ_ELF
	  if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
	    S_SET_OTHER (l->label, STO_MIPS16);
#endif
	  val = S_GET_VALUE (l->label);
	  if ((val & 1) == 0)
	    S_SET_VALUE (l->label, val + 1);
	}
    }
}

/* Output an instruction.  PLACE is where to put the instruction; if
   it is NULL, this uses frag_more to get room.  IP is the instruction
   information.  ADDRESS_EXPR is an operand of the instruction to be
   used with RELOC_TYPE.  */

static void
append_insn (place, ip, address_expr, reloc_type, unmatched_hi)
     char *place;
     struct mips_cl_insn *ip;
     expressionS *address_expr;
     bfd_reloc_code_real_type *reloc_type;
     boolean unmatched_hi;
{
  register unsigned long prev_pinfo, pinfo;
  char *f;
  fixS *fixp[3];
  int nops = 0;

  /* Mark instruction labels in mips16 mode.  */
  mips16_mark_labels ();

  prev_pinfo = prev_insn.insn_mo->pinfo;
  pinfo = ip->insn_mo->pinfo;

  if (place == NULL && (! mips_opts.noreorder || prev_nop_frag != NULL))
    {
      int prev_prev_nop;

      /* If the previous insn required any delay slots, see if we need
	 to insert a NOP or two.  There are eight kinds of possible
	 hazards, of which an instruction can have at most one type.
	 (1) a load from memory delay
	 (2) a load from a coprocessor delay
	 (3) an unconditional branch delay
	 (4) a conditional branch delay
	 (5) a move to coprocessor register delay
	 (6) a load coprocessor register from memory delay
	 (7) a coprocessor condition code delay
	 (8) a HI/LO special register delay

	 There are a lot of optimizations we could do that we don't.
	 In particular, we do not, in general, reorder instructions.
	 If you use gcc with optimization, it will reorder
	 instructions and generally do much more optimization then we
	 do here; repeating all that work in the assembler would only
	 benefit hand written assembly code, and does not seem worth
	 it.  */

      /* This is how a NOP is emitted.  */
#define emit_nop()					\
  (mips_opts.mips16					\
   ? md_number_to_chars (frag_more (2), 0x6500, 2)	\
   : md_number_to_chars (frag_more (4), 0, 4))

      /* The previous insn might require a delay slot, depending upon
	 the contents of the current insn.  */
      if (! mips_opts.mips16
	  && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
	  && (((prev_pinfo & INSN_LOAD_COPROC_DELAY)
               && ! cop_interlocks)
	      || (! gpr_interlocks
		  && (prev_pinfo & INSN_LOAD_MEMORY_DELAY))))
	{
	  /* A load from a coprocessor or from memory.  All load
	     delays delay the use of general register rt for one
	     instruction on the r3000.  The r6000 and r4000 use
	     interlocks.  */
	  /* Itbl support may require additional care here.  */
	  know (prev_pinfo & INSN_WRITE_GPR_T);
	  if (mips_optimize == 0
	      || insn_uses_reg (ip,
				((prev_insn.insn_opcode >> OP_SH_RT)
				 & OP_MASK_RT),
				MIPS_GR_REG))
	    ++nops;
	}
      else if (! mips_opts.mips16
	       && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
	       && (((prev_pinfo & INSN_COPROC_MOVE_DELAY)
		    && ! cop_interlocks)
		   || (mips_opts.isa == ISA_MIPS1
		       && (prev_pinfo & INSN_COPROC_MEMORY_DELAY))))
	{
	  /* A generic coprocessor delay.  The previous instruction
	     modified a coprocessor general or control register.  If
	     it modified a control register, we need to avoid any
	     coprocessor instruction (this is probably not always
	     required, but it sometimes is).  If it modified a general
	     register, we avoid using that register.

	     On the r6000 and r4000 loading a coprocessor register
	     from memory is interlocked, and does not require a delay.

	     This case is not handled very well.  There is no special
	     knowledge of CP0 handling, and the coprocessors other
	     than the floating point unit are not distinguished at
	     all.  */
          /* Itbl support may require additional care here. FIXME!
             Need to modify this to include knowledge about
             user specified delays!  */
	  if (prev_pinfo & INSN_WRITE_FPR_T)
	    {
	      if (mips_optimize == 0
		  || insn_uses_reg (ip,
				    ((prev_insn.insn_opcode >> OP_SH_FT)
				     & OP_MASK_FT),
				    MIPS_FP_REG))
		++nops;
	    }
	  else if (prev_pinfo & INSN_WRITE_FPR_S)
	    {
	      if (mips_optimize == 0
		  || insn_uses_reg (ip,
				    ((prev_insn.insn_opcode >> OP_SH_FS)
				     & OP_MASK_FS),
				    MIPS_FP_REG))
		++nops;
	    }
	  else
	    {
	      /* We don't know exactly what the previous instruction
		 does.  If the current instruction uses a coprocessor
		 register, we must insert a NOP.  If previous
		 instruction may set the condition codes, and the
		 current instruction uses them, we must insert two
		 NOPS.  */
              /* Itbl support may require additional care here.  */
	      if (mips_optimize == 0
		  || ((prev_pinfo & INSN_WRITE_COND_CODE)
		      && (pinfo & INSN_READ_COND_CODE)))
		nops += 2;
	      else if (pinfo & INSN_COP)
		++nops;
	    }
	}
      else if (! mips_opts.mips16
	       && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
	       && (prev_pinfo & INSN_WRITE_COND_CODE)
               && ! cop_interlocks)
	{
	  /* The previous instruction sets the coprocessor condition
	     codes, but does not require a general coprocessor delay
	     (this means it is a floating point comparison
	     instruction).  If this instruction uses the condition
	     codes, we need to insert a single NOP.  */
	  /* Itbl support may require additional care here.  */
	  if (mips_optimize == 0
	      || (pinfo & INSN_READ_COND_CODE))
	    ++nops;
	}

      /* If we're fixing up mfhi/mflo for the r7000 and the
	 previous insn was an mfhi/mflo and the current insn
	 reads the register that the mfhi/mflo wrote to, then
	 insert two nops.  */

      else if (mips_7000_hilo_fix
	       && MF_HILO_INSN (prev_pinfo)
	       && insn_uses_reg (ip, ((prev_insn.insn_opcode >> OP_SH_RD)
				      & OP_MASK_RD),
				 MIPS_GR_REG))
	{
	  nops += 2;
	}

      /* If we're fixing up mfhi/mflo for the r7000 and the
	 2nd previous insn was an mfhi/mflo and the current insn
	 reads the register that the mfhi/mflo wrote to, then
	 insert one nop.  */

      else if (mips_7000_hilo_fix
	       && MF_HILO_INSN (prev_prev_insn.insn_opcode)
	       && insn_uses_reg (ip, ((prev_prev_insn.insn_opcode >> OP_SH_RD)
                                       & OP_MASK_RD),
                                    MIPS_GR_REG))

	{
	  ++nops;
	}

      else if (prev_pinfo & INSN_READ_LO)
	{
	  /* The previous instruction reads the LO register; if the
	     current instruction writes to the LO register, we must
	     insert two NOPS.  Some newer processors have interlocks.
	     Also the tx39's multiply instructions can be exectuted
             immediatly after a read from HI/LO (without the delay),
             though the tx39's divide insns still do require the
	     delay.  */
	  if (! (hilo_interlocks
		 || (mips_tune == CPU_R3900 && (pinfo & INSN_MULT)))
	      && (mips_optimize == 0
		  || (pinfo & INSN_WRITE_LO)))
	    nops += 2;
	  /* Most mips16 branch insns don't have a delay slot.
	     If a read from LO is immediately followed by a branch
	     to a write to LO we have a read followed by a write
	     less than 2 insns away.  We assume the target of
	     a branch might be a write to LO, and insert a nop
	     between a read and an immediately following branch.  */
	  else if (mips_opts.mips16
		   && (mips_optimize == 0
		       || (pinfo & MIPS16_INSN_BRANCH)))
	    ++nops;
	}
      else if (prev_insn.insn_mo->pinfo & INSN_READ_HI)
	{
	  /* The previous instruction reads the HI register; if the
	     current instruction writes to the HI register, we must
	     insert a NOP.  Some newer processors have interlocks.
	     Also the note tx39's multiply above.  */
	  if (! (hilo_interlocks
		 || (mips_tune == CPU_R3900 && (pinfo & INSN_MULT)))
	      && (mips_optimize == 0
		  || (pinfo & INSN_WRITE_HI)))
	    nops += 2;
	  /* Most mips16 branch insns don't have a delay slot.
	     If a read from HI is immediately followed by a branch
	     to a write to HI we have a read followed by a write
	     less than 2 insns away.  We assume the target of
	     a branch might be a write to HI, and insert a nop
	     between a read and an immediately following branch.  */
	  else if (mips_opts.mips16
		   && (mips_optimize == 0
		       || (pinfo & MIPS16_INSN_BRANCH)))
	    ++nops;
	}

      /* If the previous instruction was in a noreorder section, then
         we don't want to insert the nop after all.  */
      /* Itbl support may require additional care here.  */
      if (prev_insn_unreordered)
	nops = 0;

      /* There are two cases which require two intervening
	 instructions: 1) setting the condition codes using a move to
	 coprocessor instruction which requires a general coprocessor
	 delay and then reading the condition codes 2) reading the HI
	 or LO register and then writing to it (except on processors
	 which have interlocks).  If we are not already emitting a NOP
	 instruction, we must check for these cases compared to the
	 instruction previous to the previous instruction.  */
      if ((! mips_opts.mips16
	   && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
	   && (prev_prev_insn.insn_mo->pinfo & INSN_COPROC_MOVE_DELAY)
	   && (prev_prev_insn.insn_mo->pinfo & INSN_WRITE_COND_CODE)
	   && (pinfo & INSN_READ_COND_CODE)
	   && ! cop_interlocks)
	  || ((prev_prev_insn.insn_mo->pinfo & INSN_READ_LO)
	      && (pinfo & INSN_WRITE_LO)
	      && ! (hilo_interlocks
		    || (mips_tune == CPU_R3900 && (pinfo & INSN_MULT))))
	  || ((prev_prev_insn.insn_mo->pinfo & INSN_READ_HI)
	      && (pinfo & INSN_WRITE_HI)
	      && ! (hilo_interlocks
		    || (mips_tune == CPU_R3900 && (pinfo & INSN_MULT)))))
	prev_prev_nop = 1;
      else
	prev_prev_nop = 0;

      if (prev_prev_insn_unreordered)
	prev_prev_nop = 0;

      if (prev_prev_nop && nops == 0)
	++nops;

      if (mips_fix_4122_bugs && prev_insn.insn_mo->name)
	{
	  /* We're out of bits in pinfo, so we must resort to string
	     ops here.  Shortcuts are selected based on opcodes being
	     limited to the VR4122 instruction set.  */
	  int min_nops = 0;
	  const char *pn = prev_insn.insn_mo->name;
	  const char *tn = ip->insn_mo->name;
	  if (strncmp(pn, "macc", 4) == 0
	      || strncmp(pn, "dmacc", 5) == 0)
	    {
	      /* Errata 21 - [D]DIV[U] after [D]MACC */
	      if (strstr (tn, "div"))
		{
		  min_nops = 1;
		}

	      /* Errata 23 - Continuous DMULT[U]/DMACC instructions */
	      if (pn[0] == 'd' /* dmacc */
		  && (strncmp(tn, "dmult", 5) == 0
		      || strncmp(tn, "dmacc", 5) == 0))
		{
		  min_nops = 1;
		}

	      /* Errata 24 - MT{LO,HI} after [D]MACC */
	      if (strcmp (tn, "mtlo") == 0
		  || strcmp (tn, "mthi") == 0)
		{
		  min_nops = 1;
		}

	    }
	  else if (strncmp(pn, "dmult", 5) == 0
		   && (strncmp(tn, "dmult", 5) == 0
		       || strncmp(tn, "dmacc", 5) == 0))
	    {
	      /* Here is the rest of errata 23.  */
	      min_nops = 1;
	    }
	  if (nops < min_nops)
	    nops = min_nops;
	}

      /* If we are being given a nop instruction, don't bother with
	 one of the nops we would otherwise output.  This will only
	 happen when a nop instruction is used with mips_optimize set
	 to 0.  */
      if (nops > 0
	  && ! mips_opts.noreorder
	  && ip->insn_opcode == (unsigned) (mips_opts.mips16 ? 0x6500 : 0))
	--nops;

      /* Now emit the right number of NOP instructions.  */
      if (nops > 0 && ! mips_opts.noreorder)
	{
	  fragS *old_frag;
	  unsigned long old_frag_offset;
	  int i;
	  struct insn_label_list *l;

	  old_frag = frag_now;
	  old_frag_offset = frag_now_fix ();

	  for (i = 0; i < nops; i++)
	    emit_nop ();

	  if (listing)
	    {
	      listing_prev_line ();
	      /* We may be at the start of a variant frag.  In case we
                 are, make sure there is enough space for the frag
                 after the frags created by listing_prev_line.  The
                 argument to frag_grow here must be at least as large
                 as the argument to all other calls to frag_grow in
                 this file.  We don't have to worry about being in the
                 middle of a variant frag, because the variants insert
                 all needed nop instructions themselves.  */
	      frag_grow (40);
	    }

	  for (l = insn_labels; l != NULL; l = l->next)
	    {
	      valueT val;

	      assert (S_GET_SEGMENT (l->label) == now_seg);
	      symbol_set_frag (l->label, frag_now);
	      val = (valueT) frag_now_fix ();
	      /* mips16 text labels are stored as odd.  */
	      if (mips_opts.mips16)
		++val;
	      S_SET_VALUE (l->label, val);
	    }

#ifndef NO_ECOFF_DEBUGGING
	  if (ECOFF_DEBUGGING)
	    ecoff_fix_loc (old_frag, old_frag_offset);
#endif
	}
      else if (prev_nop_frag != NULL)
	{
	  /* We have a frag holding nops we may be able to remove.  If
             we don't need any nops, we can decrease the size of
             prev_nop_frag by the size of one instruction.  If we do
             need some nops, we count them in prev_nops_required.  */
	  if (prev_nop_frag_since == 0)
	    {
	      if (nops == 0)
		{
		  prev_nop_frag->fr_fix -= mips_opts.mips16 ? 2 : 4;
		  --prev_nop_frag_holds;
		}
	      else
		prev_nop_frag_required += nops;
	    }
	  else
	    {
	      if (prev_prev_nop == 0)
		{
		  prev_nop_frag->fr_fix -= mips_opts.mips16 ? 2 : 4;
		  --prev_nop_frag_holds;
		}
	      else
		++prev_nop_frag_required;
	    }

	  if (prev_nop_frag_holds <= prev_nop_frag_required)
	    prev_nop_frag = NULL;

	  ++prev_nop_frag_since;

	  /* Sanity check: by the time we reach the second instruction
             after prev_nop_frag, we should have used up all the nops
             one way or another.  */
	  assert (prev_nop_frag_since <= 1 || prev_nop_frag == NULL);
	}
    }

  if (place == NULL
      && address_expr
      && ((*reloc_type == BFD_RELOC_16_PCREL
	   && address_expr->X_op != O_constant)
	  || *reloc_type == BFD_RELOC_16_PCREL_S2)
      && (pinfo & INSN_UNCOND_BRANCH_DELAY || pinfo & INSN_COND_BRANCH_DELAY
	  || pinfo & INSN_COND_BRANCH_LIKELY)
      && mips_relax_branch
      /* Don't try branch relaxation within .set nomacro, or within
	 .set noat if we use $at for PIC computations.  If it turns
	 out that the branch was out-of-range, we'll get an error.  */
      && !mips_opts.warn_about_macros
      && !(mips_opts.noat && mips_pic != NO_PIC)
      && !mips_opts.mips16)
    {
      f = frag_var (rs_machine_dependent,
		    relaxed_branch_length
		    (NULL, NULL,
		     (pinfo & INSN_UNCOND_BRANCH_DELAY) ? -1
		     : (pinfo & INSN_COND_BRANCH_LIKELY) ? 1 : 0), 4,
		    RELAX_BRANCH_ENCODE
		    (*reloc_type == BFD_RELOC_16_PCREL_S2,
		     pinfo & INSN_UNCOND_BRANCH_DELAY,
		     pinfo & INSN_COND_BRANCH_LIKELY,
		     pinfo & INSN_WRITE_GPR_31,
		     0),
		    address_expr->X_add_symbol,
		    address_expr->X_add_number,
		    0);
      *reloc_type = BFD_RELOC_UNUSED;
    }
  else if (*reloc_type > BFD_RELOC_UNUSED)
    {
      /* We need to set up a variant frag.  */
      assert (mips_opts.mips16 && address_expr != NULL);
      f = frag_var (rs_machine_dependent, 4, 0,
		    RELAX_MIPS16_ENCODE (*reloc_type - BFD_RELOC_UNUSED,
					 mips16_small, mips16_ext,
					 (prev_pinfo
					  & INSN_UNCOND_BRANCH_DELAY),
					 (*prev_insn_reloc_type
					  == BFD_RELOC_MIPS16_JMP)),
		    make_expr_symbol (address_expr), 0, NULL);
    }
  else if (place != NULL)
    f = place;
  else if (mips_opts.mips16
	   && ! ip->use_extend
	   && *reloc_type != BFD_RELOC_MIPS16_JMP)
    {
      /* Make sure there is enough room to swap this instruction with
         a following jump instruction.  */
      frag_grow (6);
      f = frag_more (2);
    }
  else
    {
      if (mips_opts.mips16
	  && mips_opts.noreorder
	  && (prev_pinfo & INSN_UNCOND_BRANCH_DELAY) != 0)
	as_warn (_("extended instruction in delay slot"));

      f = frag_more (4);
    }

  fixp[0] = fixp[1] = fixp[2] = NULL;
  if (address_expr != NULL && *reloc_type < BFD_RELOC_UNUSED)
    {
      if (address_expr->X_op == O_constant)
	{
	  valueT tmp;

	  switch (*reloc_type)
	    {
	    case BFD_RELOC_32:
	      ip->insn_opcode |= address_expr->X_add_number;
	      break;

	    case BFD_RELOC_MIPS_HIGHEST:
	      tmp = (address_expr->X_add_number + 0x800080008000) >> 16;
	      tmp >>= 16;
	      ip->insn_opcode |= (tmp >> 16) & 0xffff;
	      break;

	    case BFD_RELOC_MIPS_HIGHER:
	      tmp = (address_expr->X_add_number + 0x80008000) >> 16;
	      ip->insn_opcode |= (tmp >> 16) & 0xffff;
	      break;

	    case BFD_RELOC_HI16_S:
	      ip->insn_opcode |= ((address_expr->X_add_number + 0x8000)
				  >> 16) & 0xffff;
	      break;

	    case BFD_RELOC_HI16:
	      ip->insn_opcode |= (address_expr->X_add_number >> 16) & 0xffff;
	      break;

	    case BFD_RELOC_LO16:
	    case BFD_RELOC_MIPS_GOT_DISP:
	      ip->insn_opcode |= address_expr->X_add_number & 0xffff;
	      break;

	    case BFD_RELOC_MIPS_JMP:
	      if ((address_expr->X_add_number & 3) != 0)
		as_bad (_("jump to misaligned address (0x%lx)"),
			(unsigned long) address_expr->X_add_number);
	      if (address_expr->X_add_number & ~0xfffffff)
		as_bad (_("jump address range overflow (0x%lx)"),
			(unsigned long) address_expr->X_add_number);
	      ip->insn_opcode |= (address_expr->X_add_number >> 2) & 0x3ffffff;
	      break;

	    case BFD_RELOC_MIPS16_JMP:
	      if ((address_expr->X_add_number & 3) != 0)
		as_bad (_("jump to misaligned address (0x%lx)"),
			(unsigned long) address_expr->X_add_number);
	      if (address_expr->X_add_number & ~0xfffffff)
		as_bad (_("jump address range overflow (0x%lx)"),
			(unsigned long) address_expr->X_add_number);
	      ip->insn_opcode |=
		(((address_expr->X_add_number & 0x7c0000) << 3)
		 | ((address_expr->X_add_number & 0xf800000) >> 7)
		 | ((address_expr->X_add_number & 0x3fffc) >> 2));
	      break;

	    case BFD_RELOC_16_PCREL:
	      ip->insn_opcode |= address_expr->X_add_number & 0xffff;
	      break;

	    case BFD_RELOC_16_PCREL_S2:
	      goto need_reloc;

	    default:
	      internalError ();
	    }
	}
      else
	{
	need_reloc:
	  /* Don't generate a reloc if we are writing into a variant frag.  */
	  if (place == NULL)
	    {
	      fixp[0] = fix_new_exp (frag_now, f - frag_now->fr_literal, 4,
				     address_expr,
				     (*reloc_type == BFD_RELOC_16_PCREL
				      || *reloc_type == BFD_RELOC_16_PCREL_S2),
				     reloc_type[0]);

	      /* These relocations can have an addend that won't fit in
	         4 octets for 64bit assembly.  */
	      if (HAVE_64BIT_GPRS &&
		  (*reloc_type == BFD_RELOC_16
		   || *reloc_type == BFD_RELOC_32
		   || *reloc_type == BFD_RELOC_MIPS_JMP
		   || *reloc_type == BFD_RELOC_HI16_S
		   || *reloc_type == BFD_RELOC_LO16
		   || *reloc_type == BFD_RELOC_GPREL16
		   || *reloc_type == BFD_RELOC_MIPS_LITERAL
		   || *reloc_type == BFD_RELOC_GPREL32
		   || *reloc_type == BFD_RELOC_64
		   || *reloc_type == BFD_RELOC_CTOR
		   || *reloc_type == BFD_RELOC_MIPS_SUB
		   || *reloc_type == BFD_RELOC_MIPS_HIGHEST
		   || *reloc_type == BFD_RELOC_MIPS_HIGHER
		   || *reloc_type == BFD_RELOC_MIPS_SCN_DISP
		   || *reloc_type == BFD_RELOC_MIPS_REL16
		   || *reloc_type == BFD_RELOC_MIPS_RELGOT))
		fixp[0]->fx_no_overflow = 1;

	      if (unmatched_hi)
		{
		  struct mips_hi_fixup *hi_fixup;

		  assert (*reloc_type == BFD_RELOC_HI16_S);
		  hi_fixup = ((struct mips_hi_fixup *)
			      xmalloc (sizeof (struct mips_hi_fixup)));
		  hi_fixup->fixp = fixp[0];
		  hi_fixup->seg = now_seg;
		  hi_fixup->next = mips_hi_fixup_list;
		  mips_hi_fixup_list = hi_fixup;
		}

	      if (reloc_type[1] != BFD_RELOC_UNUSED)
		{
		  /* FIXME: This symbol can be one of
		     RSS_UNDEF, RSS_GP, RSS_GP0, RSS_LOC.  */
		  address_expr->X_op = O_absent;
		  address_expr->X_add_symbol = 0;
		  address_expr->X_add_number = 0;

		  fixp[1] = fix_new_exp (frag_now, f - frag_now->fr_literal,
					 4, address_expr, false,
					 reloc_type[1]);

		  /* These relocations can have an addend that won't fit in
		     4 octets for 64bit assembly.  */
		  if (HAVE_64BIT_GPRS &&
		      (*reloc_type == BFD_RELOC_16
		       || *reloc_type == BFD_RELOC_32
		       || *reloc_type == BFD_RELOC_MIPS_JMP
		       || *reloc_type == BFD_RELOC_HI16_S
		       || *reloc_type == BFD_RELOC_LO16
		       || *reloc_type == BFD_RELOC_GPREL16
		       || *reloc_type == BFD_RELOC_MIPS_LITERAL
		       || *reloc_type == BFD_RELOC_GPREL32
		       || *reloc_type == BFD_RELOC_64
		       || *reloc_type == BFD_RELOC_CTOR
		       || *reloc_type == BFD_RELOC_MIPS_SUB
		       || *reloc_type == BFD_RELOC_MIPS_HIGHEST
		       || *reloc_type == BFD_RELOC_MIPS_HIGHER
		       || *reloc_type == BFD_RELOC_MIPS_SCN_DISP
		       || *reloc_type == BFD_RELOC_MIPS_REL16
		       || *reloc_type == BFD_RELOC_MIPS_RELGOT))
		    fixp[1]->fx_no_overflow = 1;

		  if (reloc_type[2] != BFD_RELOC_UNUSED)
		    {
		      address_expr->X_op = O_absent;
		      address_expr->X_add_symbol = 0;
		      address_expr->X_add_number = 0;

		      fixp[2] = fix_new_exp (frag_now,
					     f - frag_now->fr_literal, 4,
					     address_expr, false,
					     reloc_type[2]);

		      /* These relocations can have an addend that won't fit in
			 4 octets for 64bit assembly.  */
		      if (HAVE_64BIT_GPRS &&
			  (*reloc_type == BFD_RELOC_16
			   || *reloc_type == BFD_RELOC_32
			   || *reloc_type == BFD_RELOC_MIPS_JMP
			   || *reloc_type == BFD_RELOC_HI16_S
			   || *reloc_type == BFD_RELOC_LO16
			   || *reloc_type == BFD_RELOC_GPREL16
			   || *reloc_type == BFD_RELOC_MIPS_LITERAL
			   || *reloc_type == BFD_RELOC_GPREL32
			   || *reloc_type == BFD_RELOC_64
			   || *reloc_type == BFD_RELOC_CTOR
			   || *reloc_type == BFD_RELOC_MIPS_SUB
			   || *reloc_type == BFD_RELOC_MIPS_HIGHEST
			   || *reloc_type == BFD_RELOC_MIPS_HIGHER
			   || *reloc_type == BFD_RELOC_MIPS_SCN_DISP
			   || *reloc_type == BFD_RELOC_MIPS_REL16
			   || *reloc_type == BFD_RELOC_MIPS_RELGOT))
			fixp[2]->fx_no_overflow = 1;
		    }
		}
	    }
	}
    }

  if (! mips_opts.mips16)
    {
      md_number_to_chars (f, ip->insn_opcode, 4);
#ifdef OBJ_ELF
      dwarf2_emit_insn (4);
#endif
    }
  else if (*reloc_type == BFD_RELOC_MIPS16_JMP)
    {
      md_number_to_chars (f, ip->insn_opcode >> 16, 2);
      md_number_to_chars (f + 2, ip->insn_opcode & 0xffff, 2);
#ifdef OBJ_ELF
      dwarf2_emit_insn (4);
#endif
    }
  else
    {
      if (ip->use_extend)
	{
	  md_number_to_chars (f, 0xf000 | ip->extend, 2);
	  f += 2;
	}
      md_number_to_chars (f, ip->insn_opcode, 2);
#ifdef OBJ_ELF
      dwarf2_emit_insn (ip->use_extend ? 4 : 2);
#endif
    }

  /* Update the register mask information.  */
  if (! mips_opts.mips16)
    {
      if (pinfo & INSN_WRITE_GPR_D)
	mips_gprmask |= 1 << ((ip->insn_opcode >> OP_SH_RD) & OP_MASK_RD);
      if ((pinfo & (INSN_WRITE_GPR_T | INSN_READ_GPR_T)) != 0)
	mips_gprmask |= 1 << ((ip->insn_opcode >> OP_SH_RT) & OP_MASK_RT);
      if (pinfo & INSN_READ_GPR_S)
	mips_gprmask |= 1 << ((ip->insn_opcode >> OP_SH_RS) & OP_MASK_RS);
      if (pinfo & INSN_WRITE_GPR_31)
	mips_gprmask |= 1 << RA;
      if (pinfo & INSN_WRITE_FPR_D)
	mips_cprmask[1] |= 1 << ((ip->insn_opcode >> OP_SH_FD) & OP_MASK_FD);
      if ((pinfo & (INSN_WRITE_FPR_S | INSN_READ_FPR_S)) != 0)
	mips_cprmask[1] |= 1 << ((ip->insn_opcode >> OP_SH_FS) & OP_MASK_FS);
      if ((pinfo & (INSN_WRITE_FPR_T | INSN_READ_FPR_T)) != 0)
	mips_cprmask[1] |= 1 << ((ip->insn_opcode >> OP_SH_FT) & OP_MASK_FT);
      if ((pinfo & INSN_READ_FPR_R) != 0)
	mips_cprmask[1] |= 1 << ((ip->insn_opcode >> OP_SH_FR) & OP_MASK_FR);
      if (pinfo & INSN_COP)
	{
	  /* We don't keep enough information to sort these cases out.
	     The itbl support does keep this information however, although
	     we currently don't support itbl fprmats as part of the cop
	     instruction.  May want to add this support in the future.  */
	}
      /* Never set the bit for $0, which is always zero.  */
      mips_gprmask &= ~1 << 0;
    }
  else
    {
      if (pinfo & (MIPS16_INSN_WRITE_X | MIPS16_INSN_READ_X))
	mips_gprmask |= 1 << ((ip->insn_opcode >> MIPS16OP_SH_RX)
			      & MIPS16OP_MASK_RX);
      if (pinfo & (MIPS16_INSN_WRITE_Y | MIPS16_INSN_READ_Y))
	mips_gprmask |= 1 << ((ip->insn_opcode >> MIPS16OP_SH_RY)
			      & MIPS16OP_MASK_RY);
      if (pinfo & MIPS16_INSN_WRITE_Z)
	mips_gprmask |= 1 << ((ip->insn_opcode >> MIPS16OP_SH_RZ)
			      & MIPS16OP_MASK_RZ);
      if (pinfo & (MIPS16_INSN_WRITE_T | MIPS16_INSN_READ_T))
	mips_gprmask |= 1 << TREG;
      if (pinfo & (MIPS16_INSN_WRITE_SP | MIPS16_INSN_READ_SP))
	mips_gprmask |= 1 << SP;
      if (pinfo & (MIPS16_INSN_WRITE_31 | MIPS16_INSN_READ_31))
	mips_gprmask |= 1 << RA;
      if (pinfo & MIPS16_INSN_WRITE_GPR_Y)
	mips_gprmask |= 1 << MIPS16OP_EXTRACT_REG32R (ip->insn_opcode);
      if (pinfo & MIPS16_INSN_READ_Z)
	mips_gprmask |= 1 << ((ip->insn_opcode >> MIPS16OP_SH_MOVE32Z)
			      & MIPS16OP_MASK_MOVE32Z);
      if (pinfo & MIPS16_INSN_READ_GPR_X)
	mips_gprmask |= 1 << ((ip->insn_opcode >> MIPS16OP_SH_REGR32)
			      & MIPS16OP_MASK_REGR32);
    }

  if (place == NULL && ! mips_opts.noreorder)
    {
      /* Filling the branch delay slot is more complex.  We try to
	 switch the branch with the previous instruction, which we can
	 do if the previous instruction does not set up a condition
	 that the branch tests and if the branch is not itself the
	 target of any branch.  */
      if ((pinfo & INSN_UNCOND_BRANCH_DELAY)
	  || (pinfo & INSN_COND_BRANCH_DELAY))
	{
	  if (mips_optimize < 2
	      /* If we have seen .set volatile or .set nomove, don't
		 optimize.  */
	      || mips_opts.nomove != 0
	      /* If we had to emit any NOP instructions, then we
		 already know we can not swap.  */
	      || nops != 0
	      /* If we don't even know the previous insn, we can not
		 swap.  */
	      || ! prev_insn_valid
	      /* If the previous insn is already in a branch delay
		 slot, then we can not swap.  */
	      || prev_insn_is_delay_slot
	      /* If the previous previous insn was in a .set
		 noreorder, we can't swap.  Actually, the MIPS
		 assembler will swap in this situation.  However, gcc
		 configured -with-gnu-as will generate code like
		   .set noreorder
		   lw	$4,XXX
		   .set	reorder
		   INSN
		   bne	$4,$0,foo
		 in which we can not swap the bne and INSN.  If gcc is
		 not configured -with-gnu-as, it does not output the
		 .set pseudo-ops.  We don't have to check
		 prev_insn_unreordered, because prev_insn_valid will
		 be 0 in that case.  We don't want to use
		 prev_prev_insn_valid, because we do want to be able
		 to swap at the start of a function.  */
	      || prev_prev_insn_unreordered
	      /* If the branch is itself the target of a branch, we
		 can not swap.  We cheat on this; all we check for is
		 whether there is a label on this instruction.  If
		 there are any branches to anything other than a
		 label, users must use .set noreorder.  */
	      || insn_labels != NULL
	      /* If the previous instruction is in a variant frag, we
		 can not do the swap.  This does not apply to the
		 mips16, which uses variant frags for different
		 purposes.  */
	      || (! mips_opts.mips16
		  && prev_insn_frag->fr_type == rs_machine_dependent)
	      /* If the branch reads the condition codes, we don't
		 even try to swap, because in the sequence
		   ctc1 $X,$31
		   INSN
		   INSN
		   bc1t LABEL
		 we can not swap, and I don't feel like handling that
		 case.  */
	      || (! mips_opts.mips16
		  && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
		  && (pinfo & INSN_READ_COND_CODE))
	      /* We can not swap with an instruction that requires a
		 delay slot, becase the target of the branch might
		 interfere with that instruction.  */
	      || (! mips_opts.mips16
		  && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
		  && (prev_pinfo
              /* Itbl support may require additional care here.  */
		      & (INSN_LOAD_COPROC_DELAY
			 | INSN_COPROC_MOVE_DELAY
			 | INSN_WRITE_COND_CODE)))
	      || (! (hilo_interlocks
		     || (mips_tune == CPU_R3900 && (pinfo & INSN_MULT)))
		  && (prev_pinfo
		      & (INSN_READ_LO
			 | INSN_READ_HI)))
	      || (! mips_opts.mips16
		  && ! gpr_interlocks
		  && (prev_pinfo & INSN_LOAD_MEMORY_DELAY))
	      || (! mips_opts.mips16
		  && mips_opts.isa == ISA_MIPS1
                  /* Itbl support may require additional care here.  */
		  && (prev_pinfo & INSN_COPROC_MEMORY_DELAY))
	      /* We can not swap with a branch instruction.  */
	      || (prev_pinfo
		  & (INSN_UNCOND_BRANCH_DELAY
		     | INSN_COND_BRANCH_DELAY
		     | INSN_COND_BRANCH_LIKELY))
	      /* We do not swap with a trap instruction, since it
		 complicates trap handlers to have the trap
		 instruction be in a delay slot.  */
	      || (prev_pinfo & INSN_TRAP)
	      /* If the branch reads a register that the previous
		 instruction sets, we can not swap.  */
	      || (! mips_opts.mips16
		  && (prev_pinfo & INSN_WRITE_GPR_T)
		  && insn_uses_reg (ip,
				    ((prev_insn.insn_opcode >> OP_SH_RT)
				     & OP_MASK_RT),
				    MIPS_GR_REG))
	      || (! mips_opts.mips16
		  && (prev_pinfo & INSN_WRITE_GPR_D)
		  && insn_uses_reg (ip,
				    ((prev_insn.insn_opcode >> OP_SH_RD)
				     & OP_MASK_RD),
				    MIPS_GR_REG))
	      || (mips_opts.mips16
		  && (((prev_pinfo & MIPS16_INSN_WRITE_X)
		       && insn_uses_reg (ip,
					 ((prev_insn.insn_opcode
					   >> MIPS16OP_SH_RX)
					  & MIPS16OP_MASK_RX),
					 MIPS16_REG))
		      || ((prev_pinfo & MIPS16_INSN_WRITE_Y)
			  && insn_uses_reg (ip,
					    ((prev_insn.insn_opcode
					      >> MIPS16OP_SH_RY)
					     & MIPS16OP_MASK_RY),
					    MIPS16_REG))
		      || ((prev_pinfo & MIPS16_INSN_WRITE_Z)
			  && insn_uses_reg (ip,
					    ((prev_insn.insn_opcode
					      >> MIPS16OP_SH_RZ)
					     & MIPS16OP_MASK_RZ),
					    MIPS16_REG))
		      || ((prev_pinfo & MIPS16_INSN_WRITE_T)
			  && insn_uses_reg (ip, TREG, MIPS_GR_REG))
		      || ((prev_pinfo & MIPS16_INSN_WRITE_31)
			  && insn_uses_reg (ip, RA, MIPS_GR_REG))
		      || ((prev_pinfo & MIPS16_INSN_WRITE_GPR_Y)
			  && insn_uses_reg (ip,
					    MIPS16OP_EXTRACT_REG32R (prev_insn.
								     insn_opcode),
					    MIPS_GR_REG))))
	      /* If the branch writes a register that the previous
		 instruction sets, we can not swap (we know that
		 branches write only to RD or to $31).  */
	      || (! mips_opts.mips16
		  && (prev_pinfo & INSN_WRITE_GPR_T)
		  && (((pinfo & INSN_WRITE_GPR_D)
		       && (((prev_insn.insn_opcode >> OP_SH_RT) & OP_MASK_RT)
			   == ((ip->insn_opcode >> OP_SH_RD) & OP_MASK_RD)))
		      || ((pinfo & INSN_WRITE_GPR_31)
			  && (((prev_insn.insn_opcode >> OP_SH_RT)
			       & OP_MASK_RT)
			      == RA))))
	      || (! mips_opts.mips16
		  && (prev_pinfo & INSN_WRITE_GPR_D)
		  && (((pinfo & INSN_WRITE_GPR_D)
		       && (((prev_insn.insn_opcode >> OP_SH_RD) & OP_MASK_RD)
			   == ((ip->insn_opcode >> OP_SH_RD) & OP_MASK_RD)))
		      || ((pinfo & INSN_WRITE_GPR_31)
			  && (((prev_insn.insn_opcode >> OP_SH_RD)
			       & OP_MASK_RD)
			      == RA))))
	      || (mips_opts.mips16
		  && (pinfo & MIPS16_INSN_WRITE_31)
		  && ((prev_pinfo & MIPS16_INSN_WRITE_31)
		      || ((prev_pinfo & MIPS16_INSN_WRITE_GPR_Y)
			  && (MIPS16OP_EXTRACT_REG32R (prev_insn.insn_opcode)
			      == RA))))
	      /* If the branch writes a register that the previous
		 instruction reads, we can not swap (we know that
		 branches only write to RD or to $31).  */
	      || (! mips_opts.mips16
		  && (pinfo & INSN_WRITE_GPR_D)
		  && insn_uses_reg (&prev_insn,
				    ((ip->insn_opcode >> OP_SH_RD)
				     & OP_MASK_RD),
				    MIPS_GR_REG))
	      || (! mips_opts.mips16
		  && (pinfo & INSN_WRITE_GPR_31)
		  && insn_uses_reg (&prev_insn, RA, MIPS_GR_REG))
	      || (mips_opts.mips16
		  && (pinfo & MIPS16_INSN_WRITE_31)
		  && insn_uses_reg (&prev_insn, RA, MIPS_GR_REG))
	      /* If we are generating embedded PIC code, the branch
		 might be expanded into a sequence which uses $at, so
		 we can't swap with an instruction which reads it.  */
	      || (mips_pic == EMBEDDED_PIC
		  && insn_uses_reg (&prev_insn, AT, MIPS_GR_REG))
	      /* If the previous previous instruction has a load
		 delay, and sets a register that the branch reads, we
		 can not swap.  */
	      || (! mips_opts.mips16
		  && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
              /* Itbl support may require additional care here.  */
		  && ((prev_prev_insn.insn_mo->pinfo & INSN_LOAD_COPROC_DELAY)
		      || (! gpr_interlocks
			  && (prev_prev_insn.insn_mo->pinfo
			      & INSN_LOAD_MEMORY_DELAY)))
		  && insn_uses_reg (ip,
				    ((prev_prev_insn.insn_opcode >> OP_SH_RT)
				     & OP_MASK_RT),
				    MIPS_GR_REG))
	      /* If one instruction sets a condition code and the
                 other one uses a condition code, we can not swap.  */
	      || ((pinfo & INSN_READ_COND_CODE)
		  && (prev_pinfo & INSN_WRITE_COND_CODE))
	      || ((pinfo & INSN_WRITE_COND_CODE)
		  && (prev_pinfo & INSN_READ_COND_CODE))
	      /* If the previous instruction uses the PC, we can not
                 swap.  */
	      || (mips_opts.mips16
		  && (prev_pinfo & MIPS16_INSN_READ_PC))
	      /* If the previous instruction was extended, we can not
                 swap.  */
	      || (mips_opts.mips16 && prev_insn_extended)
	      /* If the previous instruction had a fixup in mips16
                 mode, we can not swap.  This normally means that the
                 previous instruction was a 4 byte branch anyhow.  */
	      || (mips_opts.mips16 && prev_insn_fixp[0])
	      /* If the previous instruction is a sync, sync.l, or
		 sync.p, we can not swap.  */
	      || (prev_pinfo & INSN_SYNC))
	    {
	      /* We could do even better for unconditional branches to
		 portions of this object file; we could pick up the
		 instruction at the destination, put it in the delay
		 slot, and bump the destination address.  */
	      emit_nop ();
	      /* Update the previous insn information.  */
	      prev_prev_insn = *ip;
	      prev_insn.insn_mo = &dummy_opcode;
	    }
	  else
	    {
	      /* It looks like we can actually do the swap.  */
	      if (! mips_opts.mips16)
		{
		  char *prev_f;
		  char temp[4];

		  prev_f = prev_insn_frag->fr_literal + prev_insn_where;
		  memcpy (temp, prev_f, 4);
		  memcpy (prev_f, f, 4);
		  memcpy (f, temp, 4);
		  if (prev_insn_fixp[0])
		    {
		      prev_insn_fixp[0]->fx_frag = frag_now;
		      prev_insn_fixp[0]->fx_where = f - frag_now->fr_literal;
		    }
		  if (prev_insn_fixp[1])
		    {
		      prev_insn_fixp[1]->fx_frag = frag_now;
		      prev_insn_fixp[1]->fx_where = f - frag_now->fr_literal;
		    }
		  if (prev_insn_fixp[2])
		    {
		      prev_insn_fixp[2]->fx_frag = frag_now;
		      prev_insn_fixp[2]->fx_where = f - frag_now->fr_literal;
		    }
		  if (fixp[0])
		    {
		      fixp[0]->fx_frag = prev_insn_frag;
		      fixp[0]->fx_where = prev_insn_where;
		    }
		  if (fixp[1])
		    {
		      fixp[1]->fx_frag = prev_insn_frag;
		      fixp[1]->fx_where = prev_insn_where;
		    }
		  if (fixp[2])
		    {
		      fixp[2]->fx_frag = prev_insn_frag;
		      fixp[2]->fx_where = prev_insn_where;
		    }
		}
	      else
		{
		  char *prev_f;
		  char temp[2];

		  assert (prev_insn_fixp[0] == NULL);
		  assert (prev_insn_fixp[1] == NULL);
		  assert (prev_insn_fixp[2] == NULL);
		  prev_f = prev_insn_frag->fr_literal + prev_insn_where;
		  memcpy (temp, prev_f, 2);
		  memcpy (prev_f, f, 2);
		  if (*reloc_type != BFD_RELOC_MIPS16_JMP)
		    {
		      assert (*reloc_type == BFD_RELOC_UNUSED);
		      memcpy (f, temp, 2);
		    }
		  else
		    {
		      memcpy (f, f + 2, 2);
		      memcpy (f + 2, temp, 2);
		    }
		  if (fixp[0])
		    {
		      fixp[0]->fx_frag = prev_insn_frag;
		      fixp[0]->fx_where = prev_insn_where;
		    }
		  if (fixp[1])
		    {
		      fixp[1]->fx_frag = prev_insn_frag;
		      fixp[1]->fx_where = prev_insn_where;
		    }
		  if (fixp[2])
		    {
		      fixp[2]->fx_frag = prev_insn_frag;
		      fixp[2]->fx_where = prev_insn_where;
		    }
		}

	      /* Update the previous insn information; leave prev_insn
		 unchanged.  */
	      prev_prev_insn = *ip;
	    }
	  prev_insn_is_delay_slot = 1;

	  /* If that was an unconditional branch, forget the previous
	     insn information.  */
	  if (pinfo & INSN_UNCOND_BRANCH_DELAY)
	    {
	      prev_prev_insn.insn_mo = &dummy_opcode;
	      prev_insn.insn_mo = &dummy_opcode;
	    }

	  prev_insn_fixp[0] = NULL;
	  prev_insn_fixp[1] = NULL;
	  prev_insn_fixp[2] = NULL;
	  prev_insn_reloc_type[0] = BFD_RELOC_UNUSED;
	  prev_insn_reloc_type[1] = BFD_RELOC_UNUSED;
	  prev_insn_reloc_type[2] = BFD_RELOC_UNUSED;
	  prev_insn_extended = 0;
	}
      else if (pinfo & INSN_COND_BRANCH_LIKELY)
	{
	  /* We don't yet optimize a branch likely.  What we should do
	     is look at the target, copy the instruction found there
	     into the delay slot, and increment the branch to jump to
	     the next instruction.  */
	  emit_nop ();
	  /* Update the previous insn information.  */
	  prev_prev_insn = *ip;
	  prev_insn.insn_mo = &dummy_opcode;
	  prev_insn_fixp[0] = NULL;
	  prev_insn_fixp[1] = NULL;
	  prev_insn_fixp[2] = NULL;
	  prev_insn_reloc_type[0] = BFD_RELOC_UNUSED;
	  prev_insn_reloc_type[1] = BFD_RELOC_UNUSED;
	  prev_insn_reloc_type[2] = BFD_RELOC_UNUSED;
	  prev_insn_extended = 0;
	}
      else
	{
	  /* Update the previous insn information.  */
	  if (nops > 0)
	    prev_prev_insn.insn_mo = &dummy_opcode;
	  else
	    prev_prev_insn = prev_insn;
	  prev_insn = *ip;

	  /* Any time we see a branch, we always fill the delay slot
	     immediately; since this insn is not a branch, we know it
	     is not in a delay slot.  */
	  prev_insn_is_delay_slot = 0;

	  prev_insn_fixp[0] = fixp[0];
	  prev_insn_fixp[1] = fixp[1];
	  prev_insn_fixp[2] = fixp[2];
	  prev_insn_reloc_type[0] = reloc_type[0];
	  prev_insn_reloc_type[1] = reloc_type[1];
	  prev_insn_reloc_type[2] = reloc_type[2];
	  if (mips_opts.mips16)
	    prev_insn_extended = (ip->use_extend
				  || *reloc_type > BFD_RELOC_UNUSED);
	}

      prev_prev_insn_unreordered = prev_insn_unreordered;
      prev_insn_unreordered = 0;
      prev_insn_frag = frag_now;
      prev_insn_where = f - frag_now->fr_literal;
      prev_insn_valid = 1;
    }
  else if (place == NULL)
    {
      /* We need to record a bit of information even when we are not
         reordering, in order to determine the base address for mips16
         PC relative relocs.  */
      prev_prev_insn = prev_insn;
      prev_insn = *ip;
      prev_insn_reloc_type[0] = reloc_type[0];
      prev_insn_reloc_type[1] = reloc_type[1];
      prev_insn_reloc_type[2] = reloc_type[2];
      prev_prev_insn_unreordered = prev_insn_unreordered;
      prev_insn_unreordered = 1;
    }

  /* We just output an insn, so the next one doesn't have a label.  */
  mips_clear_insn_labels ();

  /* We must ensure that a fixup associated with an unmatched %hi
     reloc does not become a variant frag.  Otherwise, the
     rearrangement of %hi relocs in frob_file may confuse
     tc_gen_reloc.  */
  if (unmatched_hi)
    {
      frag_wane (frag_now);
      frag_new (0);
    }
}

/* This function forgets that there was any previous instruction or
   label.  If PRESERVE is non-zero, it remembers enough information to
   know whether nops are needed before a noreorder section.  */

static void
mips_no_prev_insn (preserve)
     int preserve;
{
  if (! preserve)
    {
      prev_insn.insn_mo = &dummy_opcode;
      prev_prev_insn.insn_mo = &dummy_opcode;
      prev_nop_frag = NULL;
      prev_nop_frag_holds = 0;
      prev_nop_frag_required = 0;
      prev_nop_frag_since = 0;
    }
  prev_insn_valid = 0;
  prev_insn_is_delay_slot = 0;
  prev_insn_unreordered = 0;
  prev_insn_extended = 0;
  prev_insn_reloc_type[0] = BFD_RELOC_UNUSED;
  prev_insn_reloc_type[1] = BFD_RELOC_UNUSED;
  prev_insn_reloc_type[2] = BFD_RELOC_UNUSED;
  prev_prev_insn_unreordered = 0;
  mips_clear_insn_labels ();
}

/* This function must be called whenever we turn on noreorder or emit
   something other than instructions.  It inserts any NOPS which might
   be needed by the previous instruction, and clears the information
   kept for the previous instructions.  The INSNS parameter is true if
   instructions are to follow.  */

static void
mips_emit_delays (insns)
     boolean insns;
{
  if (! mips_opts.noreorder)
    {
      int nops;

      nops = 0;
      if ((! mips_opts.mips16
	   && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
	   && (! cop_interlocks
               && (prev_insn.insn_mo->pinfo
                   & (INSN_LOAD_COPROC_DELAY
                      | INSN_COPROC_MOVE_DELAY
                      | INSN_WRITE_COND_CODE))))
	  || (! hilo_interlocks
	      && (prev_insn.insn_mo->pinfo
		  & (INSN_READ_LO
		     | INSN_READ_HI)))
	  || (! mips_opts.mips16
	      && ! gpr_interlocks
	      && (prev_insn.insn_mo->pinfo
                  & INSN_LOAD_MEMORY_DELAY))
	  || (! mips_opts.mips16
	      && mips_opts.isa == ISA_MIPS1
	      && (prev_insn.insn_mo->pinfo
		  & INSN_COPROC_MEMORY_DELAY)))
	{
	  /* Itbl support may require additional care here.  */
	  ++nops;
	  if ((! mips_opts.mips16
	       && ISA_HAS_COPROC_DELAYS (mips_opts.isa)
	       && (! cop_interlocks
                   && prev_insn.insn_mo->pinfo & INSN_WRITE_COND_CODE))
	      || (! hilo_interlocks
		  && ((prev_insn.insn_mo->pinfo & INSN_READ_HI)
		      || (prev_insn.insn_mo->pinfo & INSN_READ_LO))))
	    ++nops;

	  if (prev_insn_unreordered)
	    nops = 0;
	}
      else if ((! mips_opts.mips16
		&& ISA_HAS_COPROC_DELAYS (mips_opts.isa)
		&& (! cop_interlocks
                    && prev_prev_insn.insn_mo->pinfo & INSN_WRITE_COND_CODE))
	       || (! hilo_interlocks
		   && ((prev_prev_insn.insn_mo->pinfo & INSN_READ_HI)
		       || (prev_prev_insn.insn_mo->pinfo & INSN_READ_LO))))
	{
	  /* Itbl support may require additional care here.  */
	  if (! prev_prev_insn_unreordered)
	    ++nops;
	}

      if (mips_fix_4122_bugs && prev_insn.insn_mo->name)
	{
	  int min_nops = 0;
	  const char *pn = prev_insn.insn_mo->name;
	  if (strncmp(pn, "macc", 4) == 0
	      || strncmp(pn, "dmacc", 5) == 0
	      || strncmp(pn, "dmult", 5) == 0)
	    {
	      min_nops = 1;
	    }
	  if (nops < min_nops)
	    nops = min_nops;
	}

      if (nops > 0)
	{
	  struct insn_label_list *l;

	  if (insns)
	    {
	      /* Record the frag which holds the nop instructions, so
                 that we can remove them if we don't need them.  */
	      frag_grow (mips_opts.mips16 ? nops * 2 : nops * 4);
	      prev_nop_frag = frag_now;
	      prev_nop_frag_holds = nops;
	      prev_nop_frag_required = 0;
	      prev_nop_frag_since = 0;
	    }

	  for (; nops > 0; --nops)
	    emit_nop ();

	  if (insns)
	    {
	      /* Move on to a new frag, so that it is safe to simply
                 decrease the size of prev_nop_frag.  */
	      frag_wane (frag_now);
	      frag_new (0);
	    }

	  for (l = insn_labels; l != NULL; l = l->next)
	    {
	      valueT val;

	      assert (S_GET_SEGMENT (l->label) == now_seg);
	      symbol_set_frag (l->label, frag_now);
	      val = (valueT) frag_now_fix ();
	      /* mips16 text labels are stored as odd.  */
	      if (mips_opts.mips16)
		++val;
	      S_SET_VALUE (l->label, val);
	    }
	}
    }

  /* Mark instruction labels in mips16 mode.  */
  if (insns)
    mips16_mark_labels ();

  mips_no_prev_insn (insns);
}

/* Build an instruction created by a macro expansion.  This is passed
   a pointer to the count of instructions created so far, an
   expression, the name of the instruction to build, an operand format
   string, and corresponding arguments.  */

#ifdef USE_STDARG
static void
macro_build (char *place,
	     int *counter,
	     expressionS * ep,
	     const char *name,
	     const char *fmt,
	     ...)
#else
static void
macro_build (place, counter, ep, name, fmt, va_alist)
     char *place;
     int *counter;
     expressionS *ep;
     const char *name;
     const char *fmt;
     va_dcl
#endif
{
  struct mips_cl_insn insn;
  bfd_reloc_code_real_type r[3];
  va_list args;

#ifdef USE_STDARG
  va_start (args, fmt);
#else
  va_start (args);
#endif

  /*
   * If the macro is about to expand into a second instruction,
   * print a warning if needed. We need to pass ip as a parameter
   * to generate a better warning message here...
   */
  if (mips_opts.warn_about_macros && place == NULL && *counter == 1)
    as_warn (_("Macro instruction expanded into multiple instructions"));

  /*
   * If the macro is about to expand into a second instruction,
   * and it is in a delay slot, print a warning.
   */
  if (place == NULL
      && *counter == 1
      && mips_opts.noreorder
      && (prev_prev_insn.insn_mo->pinfo
	  & (INSN_UNCOND_BRANCH_DELAY | INSN_COND_BRANCH_DELAY
	     | INSN_COND_BRANCH_LIKELY)) != 0)
    as_warn (_("Macro instruction expanded into multiple instructions in a branch delay slot"));

  if (place == NULL)
    ++*counter;		/* bump instruction counter */

  if (mips_opts.mips16)
    {
      mips16_macro_build (place, counter, ep, name, fmt, args);
      va_end (args);
      return;
    }

  r[0] = BFD_RELOC_UNUSED;
  r[1] = BFD_RELOC_UNUSED;
  r[2] = BFD_RELOC_UNUSED;
  insn.insn_mo = (struct mips_opcode *) hash_find (op_hash, name);
  assert (insn.insn_mo);
  assert (strcmp (name, insn.insn_mo->name) == 0);

  /* Search until we get a match for NAME.  */
  while (1)
    {
      /* It is assumed here that macros will never generate 
         MDMX or MIPS-3D instructions.  */
      if (strcmp (fmt, insn.insn_mo->args) == 0
	  && insn.insn_mo->pinfo != INSN_MACRO
  	  && OPCODE_IS_MEMBER (insn.insn_mo,
  			       (mips_opts.isa
	      		        | (file_ase_mips16 ? INSN_MIPS16 : 0)),
			       mips_arch)
	  && (mips_arch != CPU_R4650 || (insn.insn_mo->pinfo & FP_D) == 0))
	break;

      ++insn.insn_mo;
      assert (insn.insn_mo->name);
      assert (strcmp (name, insn.insn_mo->name) == 0);
    }

  insn.insn_opcode = insn.insn_mo->match;
  for (;;)
    {
      switch (*fmt++)
	{
	case '\0':
	  break;

	case ',':
	case '(':
	case ')':
	  continue;

	case 't':
	case 'w':
	case 'E':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_RT;
	  continue;

	case 'c':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_CODE;
	  continue;

	case 'T':
	case 'W':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_FT;
	  continue;

	case 'd':
	case 'G':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_RD;
	  continue;

	case 'U':
	  {
	    int tmp = va_arg (args, int);

	    insn.insn_opcode |= tmp << OP_SH_RT;
	    insn.insn_opcode |= tmp << OP_SH_RD;
	    continue;
	  }

	case 'V':
	case 'S':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_FS;
	  continue;

	case 'z':
	  continue;

	case '<':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_SHAMT;
	  continue;

	case 'D':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_FD;
	  continue;

	case 'B':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_CODE20;
	  continue;

	case 'J':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_CODE19;
	  continue;

	case 'q':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_CODE2;
	  continue;

	case 'b':
	case 's':
	case 'r':
	case 'v':
	  insn.insn_opcode |= va_arg (args, int) << OP_SH_RS;
	  continue;

	case 'i':
	case 'j':
	case 'o':
	  *r = (bfd_reloc_code_real_type) va_arg (args, int);
	  assert (*r == BFD_RELOC_GPREL16
		  || *r == BFD_RELOC_MIPS_LITERAL
		  || *r == BFD_RELOC_MIPS_HIGHER
		  || *r == BFD_RELOC_HI16_S
		  || *r == BFD_RELOC_LO16
		  || *r == BFD_RELOC_MIPS_GOT16
		  || *r == BFD_RELOC_MIPS_CALL16
		  || *r == BFD_RELOC_MIPS_GOT_DISP
		  || *r == BFD_RELOC_MIPS_GOT_PAGE
		  || *r == BFD_RELOC_MIPS_GOT_OFST
		  || *r == BFD_RELOC_MIPS_GOT_LO16
		  || *r == BFD_RELOC_MIPS_CALL_LO16
		  || (ep->X_op == O_subtract
		      && *r == BFD_RELOC_PCREL_LO16));
	  continue;

	case 'u':
	  *r = (bfd_reloc_code_real_type) va_arg (args, int);
	  assert (ep != NULL
		  && (ep->X_op == O_constant
		      || (ep->X_op == O_symbol
			  && (*r == BFD_RELOC_MIPS_HIGHEST
			      || *r == BFD_RELOC_HI16_S
			      || *r == BFD_RELOC_HI16
			      || *r == BFD_RELOC_GPREL16
			      || *r == BFD_RELOC_MIPS_GOT_HI16
			      || *r == BFD_RELOC_MIPS_CALL_HI16))
		      || (ep->X_op == O_subtract
			  && *r == BFD_RELOC_PCREL_HI16_S)));
	  continue;

	case 'p':
	  assert (ep != NULL);
	  /*
	   * This allows macro() to pass an immediate expression for
	   * creating short branches without creating a symbol.
	   * Note that the expression still might come from the assembly
	   * input, in which case the value is not checked for range nor
	   * is a relocation entry generated (yuck).
	   */
	  if (ep->X_op == O_constant)
	    {
	      insn.insn_opcode |= (ep->X_add_number >> 2) & 0xffff;
	      ep = NULL;
	    }
	  else
	    if (mips_pic == EMBEDDED_PIC)
	      *r = BFD_RELOC_16_PCREL_S2;
	    else
	      *r = BFD_RELOC_16_PCREL;
	  continue;

	case 'a':
	  assert (ep != NULL);
	  *r = BFD_RELOC_MIPS_JMP;
	  continue;

	case 'C':
	  insn.insn_opcode |= va_arg (args, unsigned long);
	  continue;

	default:
	  internalError ();
	}
      break;
    }
  va_end (args);
  assert (*r == BFD_RELOC_UNUSED ? ep == NULL : ep != NULL);

  append_insn (place, &insn, ep, r, false);
}

static void
mips16_macro_build (place, counter, ep, name, fmt, args)
     char *place;
     int *counter ATTRIBUTE_UNUSED;
     expressionS *ep;
     const char *name;
     const char *fmt;
     va_list args;
{
  struct mips_cl_insn insn;
  bfd_reloc_code_real_type r[3]
    = {BFD_RELOC_UNUSED, BFD_RELOC_UNUSED, BFD_RELOC_UNUSED};

  insn.insn_mo = (struct mips_opcode *) hash_find (mips16_op_hash, name);
  assert (insn.insn_mo);
  assert (strcmp (name, insn.insn_mo->name) == 0);

  while (strcmp (fmt, insn.insn_mo->args) != 0
	 || insn.insn_mo->pinfo == INSN_MACRO)
    {
      ++insn.insn_mo;
      assert (insn.insn_mo->name);
      assert (strcmp (name, insn.insn_mo->name) == 0);
    }

  insn.insn_opcode = insn.insn_mo->match;
  insn.use_extend = false;

  for (;;)
    {
      int c;

      c = *fmt++;
      switch (c)
	{
	case '\0':
	  break;

	case ',':
	case '(':
	case ')':
	  continue;

	case 'y':
	case 'w':
	  insn.insn_opcode |= va_arg (args, int) << MIPS16OP_SH_RY;
	  continue;

	case 'x':
	case 'v':
	  insn.insn_opcode |= va_arg (args, int) << MIPS16OP_SH_RX;
	  continue;

	case 'z':
	  insn.insn_opcode |= va_arg (args, int) << MIPS16OP_SH_RZ;
	  continue;

	case 'Z':
	  insn.insn_opcode |= va_arg (args, int) << MIPS16OP_SH_MOVE32Z;
	  continue;

	case '0':
	case 'S':
	case 'P':
	case 'R':
	  continue;

	case 'X':
	  insn.insn_opcode |= va_arg (args, int) << MIPS16OP_SH_REGR32;
	  continue;

	case 'Y':
	  {
	    int regno;

	    regno = va_arg (args, int);
	    regno = ((regno & 7) << 2) | ((regno & 0x18) >> 3);
	    insn.insn_opcode |= regno << MIPS16OP_SH_REG32R;
	  }
	  continue;

	case '<':
	case '>':
	case '4':
	case '5':
	case 'H':
	case 'W':
	case 'D':
	case 'j':
	case '8':
	case 'V':
	case 'C':
	case 'U':
	case 'k':
	case 'K':
	case 'p':
	case 'q':
	  {
	    assert (ep != NULL);

	    if (ep->X_op != O_constant)
	      *r = (int) BFD_RELOC_UNUSED + c;
	    else
	      {
		mips16_immed (NULL, 0, c, ep->X_add_number, false, false,
			      false, &insn.insn_opcode, &insn.use_extend,
			      &insn.extend);
		ep = NULL;
		*r = BFD_RELOC_UNUSED;
	      }
	  }
	  continue;

	case '6':
	  insn.insn_opcode |= va_arg (args, int) << MIPS16OP_SH_IMM6;
	  continue;
	}

      break;
    }

  assert (*r == BFD_RELOC_UNUSED ? ep == NULL : ep != NULL);

  append_insn (place, &insn, ep, r, false);
}

/*
 * Generate a "jalr" instruction with a relocation hint to the called
 * function.  This occurs in NewABI PIC code.
 */
static void
macro_build_jalr (icnt, ep)
     int icnt;
     expressionS *ep;
{
  char *f;
  
  if (HAVE_NEWABI)
    {
      frag_grow (4);
      f = frag_more (0);
    }
  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "jalr", "d,s",
	       RA, PIC_CALL_REG);
  if (HAVE_NEWABI)
    fix_new_exp (frag_now, f - frag_now->fr_literal,
		 0, ep, false, BFD_RELOC_MIPS_JALR);
}

/*
 * Generate a "lui" instruction.
 */
static void
macro_build_lui (place, counter, ep, regnum)
     char *place;
     int *counter;
     expressionS *ep;
     int regnum;
{
  expressionS high_expr;
  struct mips_cl_insn insn;
  bfd_reloc_code_real_type r[3]
    = {BFD_RELOC_UNUSED, BFD_RELOC_UNUSED, BFD_RELOC_UNUSED};
  const char *name = "lui";
  const char *fmt = "t,u";

  assert (! mips_opts.mips16);

  if (place == NULL)
    high_expr = *ep;
  else
    {
      high_expr.X_op = O_constant;
      high_expr.X_add_number = ep->X_add_number;
    }

  if (high_expr.X_op == O_constant)
    {
      /* we can compute the instruction now without a relocation entry */
      high_expr.X_add_number = ((high_expr.X_add_number + 0x8000)
				>> 16) & 0xffff;
      *r = BFD_RELOC_UNUSED;
    }
  else
    {
      assert (ep->X_op == O_symbol);
      /* _gp_disp is a special case, used from s_cpload.  */
      assert (mips_pic == NO_PIC
	      || (! HAVE_NEWABI
		  && strcmp (S_GET_NAME (ep->X_add_symbol), "_gp_disp") == 0));
      *r = BFD_RELOC_HI16_S;
    }

  /*
   * If the macro is about to expand into a second instruction,
   * print a warning if needed. We need to pass ip as a parameter
   * to generate a better warning message here...
   */
  if (mips_opts.warn_about_macros && place == NULL && *counter == 1)
    as_warn (_("Macro instruction expanded into multiple instructions"));

  if (place == NULL)
    ++*counter;		/* bump instruction counter */

  insn.insn_mo = (struct mips_opcode *) hash_find (op_hash, name);
  assert (insn.insn_mo);
  assert (strcmp (name, insn.insn_mo->name) == 0);
  assert (strcmp (fmt, insn.insn_mo->args) == 0);

  insn.insn_opcode = insn.insn_mo->match | (regnum << OP_SH_RT);
  if (*r == BFD_RELOC_UNUSED)
    {
      insn.insn_opcode |= high_expr.X_add_number;
      append_insn (place, &insn, NULL, r, false);
    }
  else
    append_insn (place, &insn, &high_expr, r, false);
}

/* Generate a sequence of instructions to do a load or store from a constant
   offset off of a base register (breg) into/from a target register (treg),
   using AT if necessary.  */
static void
macro_build_ldst_constoffset (place, counter, ep, op, treg, breg)
     char *place;
     int *counter;
     expressionS *ep;
     const char *op;
     int treg, breg;
{
  assert (ep->X_op == O_constant);

  /* Right now, this routine can only handle signed 32-bit contants.  */
  if (! IS_SEXT_32BIT_NUM(ep->X_add_number))
    as_warn (_("operand overflow"));

  if (IS_SEXT_16BIT_NUM(ep->X_add_number))
    {
      /* Signed 16-bit offset will fit in the op.  Easy!  */
      macro_build (place, counter, ep, op, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, breg);
    }
  else
    {
      /* 32-bit offset, need multiple instructions and AT, like:
	   lui      $tempreg,const_hi       (BFD_RELOC_HI16_S)
	   addu     $tempreg,$tempreg,$breg
           <op>     $treg,const_lo($tempreg)   (BFD_RELOC_LO16)
         to handle the complete offset.  */
      macro_build_lui (place, counter, ep, AT);
      if (place != NULL)
	place += 4;
      macro_build (place, counter, (expressionS *) NULL,
		   HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
		   "d,v,t", AT, AT, breg);
      if (place != NULL)
	place += 4;
      macro_build (place, counter, ep, op, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, AT);

      if (mips_opts.noat)
	as_warn (_("Macro used $at after \".set noat\""));
    }
}

/*			set_at()
 * Generates code to set the $at register to true (one)
 * if reg is less than the immediate expression.
 */
static void
set_at (counter, reg, unsignedp)
     int *counter;
     int reg;
     int unsignedp;
{
  if (imm_expr.X_op == O_constant
      && imm_expr.X_add_number >= -0x8000
      && imm_expr.X_add_number < 0x8000)
    macro_build ((char *) NULL, counter, &imm_expr,
		 unsignedp ? "sltiu" : "slti",
		 "t,r,j", AT, reg, (int) BFD_RELOC_LO16);
  else
    {
      load_register (counter, AT, &imm_expr, HAVE_64BIT_GPRS);
      macro_build ((char *) NULL, counter, (expressionS *) NULL,
		   unsignedp ? "sltu" : "slt",
		   "d,v,t", AT, reg, AT);
    }
}

/* Warn if an expression is not a constant.  */

static void
check_absolute_expr (ip, ex)
     struct mips_cl_insn *ip;
     expressionS *ex;
{
  if (ex->X_op == O_big)
    as_bad (_("unsupported large constant"));
  else if (ex->X_op != O_constant)
    as_bad (_("Instruction %s requires absolute expression"), ip->insn_mo->name);
}

/* Count the leading zeroes by performing a binary chop. This is a
   bulky bit of source, but performance is a LOT better for the
   majority of values than a simple loop to count the bits:
       for (lcnt = 0; (lcnt < 32); lcnt++)
         if ((v) & (1 << (31 - lcnt)))
           break;
  However it is not code size friendly, and the gain will drop a bit
  on certain cached systems.
*/
#define COUNT_TOP_ZEROES(v)             \
  (((v) & ~0xffff) == 0                 \
   ? ((v) & ~0xff) == 0                 \
     ? ((v) & ~0xf) == 0                \
       ? ((v) & ~0x3) == 0              \
         ? ((v) & ~0x1) == 0            \
           ? !(v)                       \
             ? 32                       \
             : 31                       \
           : 30                         \
         : ((v) & ~0x7) == 0            \
           ? 29                         \
           : 28                         \
       : ((v) & ~0x3f) == 0             \
         ? ((v) & ~0x1f) == 0           \
           ? 27                         \
           : 26                         \
         : ((v) & ~0x7f) == 0           \
           ? 25                         \
           : 24                         \
     : ((v) & ~0xfff) == 0              \
       ? ((v) & ~0x3ff) == 0            \
         ? ((v) & ~0x1ff) == 0          \
           ? 23                         \
           : 22                         \
         : ((v) & ~0x7ff) == 0          \
           ? 21                         \
           : 20                         \
       : ((v) & ~0x3fff) == 0           \
         ? ((v) & ~0x1fff) == 0         \
           ? 19                         \
           : 18                         \
         : ((v) & ~0x7fff) == 0         \
           ? 17                         \
           : 16                         \
   : ((v) & ~0xffffff) == 0             \
     ? ((v) & ~0xfffff) == 0            \
       ? ((v) & ~0x3ffff) == 0          \
         ? ((v) & ~0x1ffff) == 0        \
           ? 15                         \
           : 14                         \
         : ((v) & ~0x7ffff) == 0        \
           ? 13                         \
           : 12                         \
       : ((v) & ~0x3fffff) == 0         \
         ? ((v) & ~0x1fffff) == 0       \
           ? 11                         \
           : 10                         \
         : ((v) & ~0x7fffff) == 0       \
           ? 9                          \
           : 8                          \
     : ((v) & ~0xfffffff) == 0          \
       ? ((v) & ~0x3ffffff) == 0        \
         ? ((v) & ~0x1ffffff) == 0      \
           ? 7                          \
           : 6                          \
         : ((v) & ~0x7ffffff) == 0      \
           ? 5                          \
           : 4                          \
       : ((v) & ~0x3fffffff) == 0       \
         ? ((v) & ~0x1fffffff) == 0     \
           ? 3                          \
           : 2                          \
         : ((v) & ~0x7fffffff) == 0     \
           ? 1                          \
           : 0)

/*			load_register()
 *  This routine generates the least number of instructions neccessary to load
 *  an absolute expression value into a register.
 */
static void
load_register (counter, reg, ep, dbl)
     int *counter;
     int reg;
     expressionS *ep;
     int dbl;
{
  int freg;
  expressionS hi32, lo32;

  if (ep->X_op != O_big)
    {
      assert (ep->X_op == O_constant);
      if (ep->X_add_number < 0x8000
	  && (ep->X_add_number >= 0
	      || (ep->X_add_number >= -0x8000
		  && (! dbl
		      || ! ep->X_unsigned
		      || sizeof (ep->X_add_number) > 4))))
	{
	  /* We can handle 16 bit signed values with an addiu to
	     $zero.  No need to ever use daddiu here, since $zero and
	     the result are always correct in 32 bit mode.  */
	  macro_build ((char *) NULL, counter, ep, "addiu", "t,r,j", reg, 0,
		       (int) BFD_RELOC_LO16);
	  return;
	}
      else if (ep->X_add_number >= 0 && ep->X_add_number < 0x10000)
	{
	  /* We can handle 16 bit unsigned values with an ori to
             $zero.  */
	  macro_build ((char *) NULL, counter, ep, "ori", "t,r,i", reg, 0,
		       (int) BFD_RELOC_LO16);
	  return;
	}
      else if ((IS_SEXT_32BIT_NUM (ep->X_add_number)
		&& (! dbl
		    || ! ep->X_unsigned
		    || sizeof (ep->X_add_number) > 4
		    || (ep->X_add_number & 0x80000000) == 0))
	       || ((HAVE_32BIT_GPRS || ! dbl)
		   && (ep->X_add_number &~ (offsetT) 0xffffffff) == 0)
	       || (HAVE_32BIT_GPRS
		   && ! dbl
		   && ((ep->X_add_number &~ (offsetT) 0xffffffff)
		       == ~ (offsetT) 0xffffffff)))
	{
	  /* 32 bit values require an lui.  */
	  macro_build ((char *) NULL, counter, ep, "lui", "t,u", reg,
		       (int) BFD_RELOC_HI16);
	  if ((ep->X_add_number & 0xffff) != 0)
	    macro_build ((char *) NULL, counter, ep, "ori", "t,r,i", reg, reg,
			 (int) BFD_RELOC_LO16);
	  return;
	}
    }

  /* The value is larger than 32 bits.  */

  if (HAVE_32BIT_GPRS)
    {
      as_bad (_("Number (0x%lx) larger than 32 bits"),
	      (unsigned long) ep->X_add_number);
      macro_build ((char *) NULL, counter, ep, "addiu", "t,r,j", reg, 0,
		   (int) BFD_RELOC_LO16);
      return;
    }

  if (ep->X_op != O_big)
    {
      hi32 = *ep;
      hi32.X_add_number = (valueT) hi32.X_add_number >> 16;
      hi32.X_add_number = (valueT) hi32.X_add_number >> 16;
      hi32.X_add_number &= 0xffffffff;
      lo32 = *ep;
      lo32.X_add_number &= 0xffffffff;
    }
  else
    {
      assert (ep->X_add_number > 2);
      if (ep->X_add_number == 3)
	generic_bignum[3] = 0;
      else if (ep->X_add_number > 4)
	as_bad (_("Number larger than 64 bits"));
      lo32.X_op = O_constant;
      lo32.X_add_number = generic_bignum[0] + (generic_bignum[1] << 16);
      hi32.X_op = O_constant;
      hi32.X_add_number = generic_bignum[2] + (generic_bignum[3] << 16);
    }

  if (hi32.X_add_number == 0)
    freg = 0;
  else
    {
      int shift, bit;
      unsigned long hi, lo;

      if (hi32.X_add_number == (offsetT) 0xffffffff)
	{
	  if ((lo32.X_add_number & 0xffff8000) == 0xffff8000)
	    {
	      macro_build ((char *) NULL, counter, &lo32, "addiu", "t,r,j",
			   reg, 0, (int) BFD_RELOC_LO16);
	      return;
	    }
	  if (lo32.X_add_number & 0x80000000)
	    {
	      macro_build ((char *) NULL, counter, &lo32, "lui", "t,u", reg,
			   (int) BFD_RELOC_HI16);
	      if (lo32.X_add_number & 0xffff)
		macro_build ((char *) NULL, counter, &lo32, "ori", "t,r,i",
			     reg, reg, (int) BFD_RELOC_LO16);
	      return;
	    }
	}

      /* Check for 16bit shifted constant.  We know that hi32 is
         non-zero, so start the mask on the first bit of the hi32
         value.  */
      shift = 17;
      do
	{
	  unsigned long himask, lomask;

	  if (shift < 32)
	    {
	      himask = 0xffff >> (32 - shift);
	      lomask = (0xffff << shift) & 0xffffffff;
	    }
	  else
	    {
	      himask = 0xffff << (shift - 32);
	      lomask = 0;
	    }
	  if ((hi32.X_add_number & ~(offsetT) himask) == 0
	      && (lo32.X_add_number & ~(offsetT) lomask) == 0)
	    {
	      expressionS tmp;

	      tmp.X_op = O_constant;
	      if (shift < 32)
		tmp.X_add_number = ((hi32.X_add_number << (32 - shift))
				    | (lo32.X_add_number >> shift));
	      else
		tmp.X_add_number = hi32.X_add_number >> (shift - 32);
	      macro_build ((char *) NULL, counter, &tmp,
			   "ori", "t,r,i", reg, 0,
			   (int) BFD_RELOC_LO16);
	      macro_build ((char *) NULL, counter, (expressionS *) NULL,
			   (shift >= 32) ? "dsll32" : "dsll",
			   "d,w,<", reg, reg,
			   (shift >= 32) ? shift - 32 : shift);
	      return;
	    }
	  ++shift;
	}
      while (shift <= (64 - 16));

      /* Find the bit number of the lowest one bit, and store the
         shifted value in hi/lo.  */
      hi = (unsigned long) (hi32.X_add_number & 0xffffffff);
      lo = (unsigned long) (lo32.X_add_number & 0xffffffff);
      if (lo != 0)
	{
	  bit = 0;
	  while ((lo & 1) == 0)
	    {
	      lo >>= 1;
	      ++bit;
	    }
	  lo |= (hi & (((unsigned long) 1 << bit) - 1)) << (32 - bit);
	  hi >>= bit;
	}
      else
	{
	  bit = 32;
	  while ((hi & 1) == 0)
	    {
	      hi >>= 1;
	      ++bit;
	    }
	  lo = hi;
	  hi = 0;
	}

      /* Optimize if the shifted value is a (power of 2) - 1.  */
      if ((hi == 0 && ((lo + 1) & lo) == 0)
	  || (lo == 0xffffffff && ((hi + 1) & hi) == 0))
	{
	  shift = COUNT_TOP_ZEROES ((unsigned int) hi32.X_add_number);
	  if (shift != 0)
	    {
	      expressionS tmp;

	      /* This instruction will set the register to be all
                 ones.  */
	      tmp.X_op = O_constant;
	      tmp.X_add_number = (offsetT) -1;
	      macro_build ((char *) NULL, counter, &tmp, "addiu", "t,r,j",
			   reg, 0, (int) BFD_RELOC_LO16);
	      if (bit != 0)
		{
		  bit += shift;
		  macro_build ((char *) NULL, counter, (expressionS *) NULL,
			       (bit >= 32) ? "dsll32" : "dsll",
			       "d,w,<", reg, reg,
			       (bit >= 32) ? bit - 32 : bit);
		}
	      macro_build ((char *) NULL, counter, (expressionS *) NULL,
			   (shift >= 32) ? "dsrl32" : "dsrl",
			   "d,w,<", reg, reg,
			   (shift >= 32) ? shift - 32 : shift);
	      return;
	    }
	}

      /* Sign extend hi32 before calling load_register, because we can
         generally get better code when we load a sign extended value.  */
      if ((hi32.X_add_number & 0x80000000) != 0)
	hi32.X_add_number |= ~(offsetT) 0xffffffff;
      load_register (counter, reg, &hi32, 0);
      freg = reg;
    }
  if ((lo32.X_add_number & 0xffff0000) == 0)
    {
      if (freg != 0)
	{
	  macro_build ((char *) NULL, counter, (expressionS *) NULL,
		       "dsll32", "d,w,<", reg, freg, 0);
	  freg = reg;
	}
    }
  else
    {
      expressionS mid16;

      if ((freg == 0) && (lo32.X_add_number == (offsetT) 0xffffffff))
	{
	  macro_build ((char *) NULL, counter, &lo32, "lui", "t,u", reg,
		       (int) BFD_RELOC_HI16);
	  macro_build ((char *) NULL, counter, (expressionS *) NULL,
		       "dsrl32", "d,w,<", reg, reg, 0);
	  return;
	}

      if (freg != 0)
	{
	  macro_build ((char *) NULL, counter, (expressionS *) NULL, "dsll",
		       "d,w,<", reg, freg, 16);
	  freg = reg;
	}
      mid16 = lo32;
      mid16.X_add_number >>= 16;
      macro_build ((char *) NULL, counter, &mid16, "ori", "t,r,i", reg,
		   freg, (int) BFD_RELOC_LO16);
      macro_build ((char *) NULL, counter, (expressionS *) NULL, "dsll",
		   "d,w,<", reg, reg, 16);
      freg = reg;
    }
  if ((lo32.X_add_number & 0xffff) != 0)
    macro_build ((char *) NULL, counter, &lo32, "ori", "t,r,i", reg, freg,
		 (int) BFD_RELOC_LO16);
}

/* Load an address into a register.  */

static void
load_address (counter, reg, ep, used_at)
     int *counter;
     int reg;
     expressionS *ep;
     int *used_at;
{
  char *p = NULL;

  if (ep->X_op != O_constant
      && ep->X_op != O_symbol)
    {
      as_bad (_("expression too complex"));
      ep->X_op = O_constant;
    }

  if (ep->X_op == O_constant)
    {
      load_register (counter, reg, ep, HAVE_64BIT_ADDRESSES);
      return;
    }

  if (mips_pic == NO_PIC)
    {
      /* If this is a reference to a GP relative symbol, we want
	   addiu	$reg,$gp,<sym>		(BFD_RELOC_GPREL16)
	 Otherwise we want
	   lui		$reg,<sym>		(BFD_RELOC_HI16_S)
	   addiu	$reg,$reg,<sym>		(BFD_RELOC_LO16)
	 If we have an addend, we always use the latter form.

	 With 64bit address space and a usable $at we want
	   lui		$reg,<sym>		(BFD_RELOC_MIPS_HIGHEST)
	   lui		$at,<sym>		(BFD_RELOC_HI16_S)
	   daddiu	$reg,<sym>		(BFD_RELOC_MIPS_HIGHER)
	   daddiu	$at,<sym>		(BFD_RELOC_LO16)
	   dsll32	$reg,0
	   daddu	$reg,$reg,$at

	 If $at is already in use, we use an path which is suboptimal
	 on superscalar processors.
	   lui		$reg,<sym>		(BFD_RELOC_MIPS_HIGHEST)
	   daddiu	$reg,<sym>		(BFD_RELOC_MIPS_HIGHER)
	   dsll		$reg,16
	   daddiu	$reg,<sym>		(BFD_RELOC_HI16_S)
	   dsll		$reg,16
	   daddiu	$reg,<sym>		(BFD_RELOC_LO16)
       */
      if (HAVE_64BIT_ADDRESSES)
	{
	  /* We don't do GP optimization for now because RELAX_ENCODE can't
	     hold the data for such large chunks.  */

	  if (*used_at == 0 && ! mips_opts.noat)
	    {
	      macro_build (p, counter, ep, "lui", "t,u",
			   reg, (int) BFD_RELOC_MIPS_HIGHEST);
	      macro_build (p, counter, ep, "lui", "t,u",
			   AT, (int) BFD_RELOC_HI16_S);
	      macro_build (p, counter, ep, "daddiu", "t,r,j",
			   reg, reg, (int) BFD_RELOC_MIPS_HIGHER);
	      macro_build (p, counter, ep, "daddiu", "t,r,j",
			   AT, AT, (int) BFD_RELOC_LO16);
	      macro_build (p, counter, (expressionS *) NULL, "dsll32",
			   "d,w,<", reg, reg, 0);
	      macro_build (p, counter, (expressionS *) NULL, "daddu",
			   "d,v,t", reg, reg, AT);
	      *used_at = 1;
	    }
	  else
	    {
	      macro_build (p, counter, ep, "lui", "t,u",
			   reg, (int) BFD_RELOC_MIPS_HIGHEST);
	      macro_build (p, counter, ep, "daddiu", "t,r,j",
			   reg, reg, (int) BFD_RELOC_MIPS_HIGHER);
	      macro_build (p, counter, (expressionS *) NULL, "dsll",
			   "d,w,<", reg, reg, 16);
	      macro_build (p, counter, ep, "daddiu", "t,r,j",
			   reg, reg, (int) BFD_RELOC_HI16_S);
	      macro_build (p, counter, (expressionS *) NULL, "dsll",
			   "d,w,<", reg, reg, 16);
	      macro_build (p, counter, ep, "daddiu", "t,r,j",
			   reg, reg, (int) BFD_RELOC_LO16);
	    }
	}
      else
	{
	  if ((valueT) ep->X_add_number <= MAX_GPREL_OFFSET
	      && ! nopic_need_relax (ep->X_add_symbol, 1))
	    {
	      frag_grow (20);
	      macro_build ((char *) NULL, counter, ep,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu", "t,r,j",
			   reg, mips_gp_register, (int) BFD_RELOC_GPREL16);
	      p = frag_var (rs_machine_dependent, 8, 0,
			    RELAX_ENCODE (4, 8, 0, 4, 0,
					  mips_opts.warn_about_macros),
			    ep->X_add_symbol, 0, NULL);
	    }
	  macro_build_lui (p, counter, ep, reg);
	  if (p != NULL)
	    p += 4;
	  macro_build (p, counter, ep,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
		       "t,r,j", reg, reg, (int) BFD_RELOC_LO16);
	}
    }
  else if (mips_pic == SVR4_PIC && ! mips_big_got)
    {
      expressionS ex;

      /* If this is a reference to an external symbol, we want
	   lw		$reg,<sym>($gp)		(BFD_RELOC_MIPS_GOT16)
	 Otherwise we want
	   lw		$reg,<sym>($gp)		(BFD_RELOC_MIPS_GOT16)
	   nop
	   addiu	$reg,$reg,<sym>		(BFD_RELOC_LO16)
	 If we have NewABI, we want
	   lw		$reg,<sym>($gp)		(BFD_RELOC_MIPS_GOT_DISP)
	 If there is a constant, it must be added in after.  */
      ex.X_add_number = ep->X_add_number;
      ep->X_add_number = 0;
      frag_grow (20);
      if (HAVE_NEWABI)
	{
	  macro_build ((char *) NULL, counter, ep,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld", "t,o(b)", reg,
		       (int) BFD_RELOC_MIPS_GOT_DISP, mips_gp_register);
	}
      else
	{
	  macro_build ((char *) NULL, counter, ep,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld", "t,o(b)",
		       reg, (int) BFD_RELOC_MIPS_GOT16, mips_gp_register);
	  macro_build ((char *) NULL, counter, (expressionS *) NULL, "nop", "");
	  p = frag_var (rs_machine_dependent, 4, 0,
			RELAX_ENCODE (0, 4, -8, 0, 0, mips_opts.warn_about_macros),
			ep->X_add_symbol, (offsetT) 0, (char *) NULL);
	  macro_build (p, counter, ep,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
		       "t,r,j", reg, reg, (int) BFD_RELOC_LO16);
	}

      if (ex.X_add_number != 0)
	{
	  if (ex.X_add_number < -0x8000 || ex.X_add_number >= 0x8000)
	    as_bad (_("PIC code offset overflow (max 16 signed bits)"));
	  ex.X_op = O_constant;
	  macro_build ((char *) NULL, counter, &ex,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
		       "t,r,j", reg, reg, (int) BFD_RELOC_LO16);
	}
    }
  else if (mips_pic == SVR4_PIC)
    {
      expressionS ex;
      int off;

      /* This is the large GOT case.  If this is a reference to an
	 external symbol, we want
	   lui		$reg,<sym>		(BFD_RELOC_MIPS_GOT_HI16)
	   addu		$reg,$reg,$gp
	   lw		$reg,<sym>($reg)	(BFD_RELOC_MIPS_GOT_LO16)
	 Otherwise, for a reference to a local symbol, we want
	   lw		$reg,<sym>($gp)		(BFD_RELOC_MIPS_GOT16)
	   nop
	   addiu	$reg,$reg,<sym>		(BFD_RELOC_LO16)
	 If we have NewABI, we want
	   lw		$reg,<sym>($gp)		(BFD_RELOC_MIPS_GOT_PAGE)
	   addiu	$reg,$reg,<sym>		(BFD_RELOC_MIPS_GOT_OFST)
	 If there is a constant, it must be added in after.  */
      ex.X_add_number = ep->X_add_number;
      ep->X_add_number = 0;
      if (HAVE_NEWABI)
	{
	  macro_build ((char *) NULL, counter, ep,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld", "t,o(b)", reg,
		       (int) BFD_RELOC_MIPS_GOT_PAGE, mips_gp_register);
	  macro_build (p, counter, ep,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu", "t,r,j",
		       reg, reg, (int) BFD_RELOC_MIPS_GOT_OFST);
	}
      else
	{
	  if (reg_needs_delay (mips_gp_register))
	    off = 4;
	  else
	    off = 0;
	  frag_grow (32);
	  macro_build ((char *) NULL, counter, ep, "lui", "t,u", reg,
		       (int) BFD_RELOC_MIPS_GOT_HI16);
	  macro_build ((char *) NULL, counter, (expressionS *) NULL,
		       HAVE_32BIT_ADDRESSES ? "addu" : "daddu", "d,v,t", reg,
		       reg, mips_gp_register);
	  macro_build ((char *) NULL, counter, ep,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
		       "t,o(b)", reg, (int) BFD_RELOC_MIPS_GOT_LO16, reg);
	  p = frag_var (rs_machine_dependent, 12 + off, 0,
			RELAX_ENCODE (12, 12 + off, off, 8 + off, 0,
				      mips_opts.warn_about_macros),
			ep->X_add_symbol, 0, NULL);
	  if (off > 0)
	    {
	      /* We need a nop before loading from $gp.  This special
		 check is required because the lui which starts the main
		 instruction stream does not refer to $gp, and so will not
		 insert the nop which may be required.  */
	      macro_build (p, counter, (expressionS *) NULL, "nop", "");
		p += 4;
	    }
	  macro_build (p, counter, ep,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld", "t,o(b)", reg,
		       (int) BFD_RELOC_MIPS_GOT16, mips_gp_register);
	  p += 4;
	  macro_build (p, counter, (expressionS *) NULL, "nop", "");
	  p += 4;
	  macro_build (p, counter, ep,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
		       "t,r,j", reg, reg, (int) BFD_RELOC_LO16);
	}

      if (ex.X_add_number != 0)
	{
	  if (ex.X_add_number < -0x8000 || ex.X_add_number >= 0x8000)
	    as_bad (_("PIC code offset overflow (max 16 signed bits)"));
	  ex.X_op = O_constant;
	  macro_build ((char *) NULL, counter, &ex,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
		       "t,r,j", reg, reg, (int) BFD_RELOC_LO16);
	}
    }
  else if (mips_pic == EMBEDDED_PIC)
    {
      /* We always do
	   addiu	$reg,$gp,<sym>		(BFD_RELOC_GPREL16)
       */
      macro_build ((char *) NULL, counter, ep,
		   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
		   "t,r,j", reg, mips_gp_register, (int) BFD_RELOC_GPREL16);
    }
  else
    abort ();
}

/* Move the contents of register SOURCE into register DEST.  */

static void
move_register (counter, dest, source)
     int *counter;
     int dest;
     int source;
{
  macro_build ((char *) NULL, counter, (expressionS *) NULL,
	       HAVE_32BIT_GPRS ? "addu" : "daddu",
	       "d,v,t", dest, source, 0);
}

/*
 *			Build macros
 *   This routine implements the seemingly endless macro or synthesized
 * instructions and addressing modes in the mips assembly language. Many
 * of these macros are simple and are similar to each other. These could
 * probably be handled by some kind of table or grammer aproach instead of
 * this verbose method. Others are not simple macros but are more like
 * optimizing code generation.
 *   One interesting optimization is when several store macros appear
 * consecutivly that would load AT with the upper half of the same address.
 * The ensuing load upper instructions are ommited. This implies some kind
 * of global optimization. We currently only optimize within a single macro.
 *   For many of the load and store macros if the address is specified as a
 * constant expression in the first 64k of memory (ie ld $2,0x4000c) we
 * first load register 'at' with zero and use it as the base register. The
 * mips assembler simply uses register $zero. Just one tiny optimization
 * we're missing.
 */
static void
macro (ip)
     struct mips_cl_insn *ip;
{
  register int treg, sreg, dreg, breg;
  int tempreg;
  int mask;
  int icnt = 0;
  int used_at = 0;
  expressionS expr1;
  const char *s;
  const char *s2;
  const char *fmt;
  int likely = 0;
  int dbl = 0;
  int coproc = 0;
  int lr = 0;
  int imm = 0;
  offsetT maxnum;
  int off;
  bfd_reloc_code_real_type r;
  int hold_mips_optimize;

  assert (! mips_opts.mips16);

  treg = (ip->insn_opcode >> 16) & 0x1f;
  dreg = (ip->insn_opcode >> 11) & 0x1f;
  sreg = breg = (ip->insn_opcode >> 21) & 0x1f;
  mask = ip->insn_mo->mask;

  expr1.X_op = O_constant;
  expr1.X_op_symbol = NULL;
  expr1.X_add_symbol = NULL;
  expr1.X_add_number = 1;

  switch (mask)
    {
    case M_DABS:
      dbl = 1;
    case M_ABS:
      /* bgez $a0,.+12
	 move v0,$a0
	 sub v0,$zero,$a0
	 */

      mips_emit_delays (true);
      ++mips_opts.noreorder;
      mips_any_noreorder = 1;

      expr1.X_add_number = 8;
      macro_build ((char *) NULL, &icnt, &expr1, "bgez", "s,p", sreg);
      if (dreg == sreg)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "",
		     0);
      else
	move_register (&icnt, dreg, sreg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "dsub" : "sub", "d,v,t", dreg, 0, sreg);

      --mips_opts.noreorder;
      return;

    case M_ADD_I:
      s = "addi";
      s2 = "add";
      goto do_addi;
    case M_ADDU_I:
      s = "addiu";
      s2 = "addu";
      goto do_addi;
    case M_DADD_I:
      dbl = 1;
      s = "daddi";
      s2 = "dadd";
      goto do_addi;
    case M_DADDU_I:
      dbl = 1;
      s = "daddiu";
      s2 = "daddu";
    do_addi:
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number >= -0x8000
	  && imm_expr.X_add_number < 0x8000)
	{
	  macro_build ((char *) NULL, &icnt, &imm_expr, s, "t,r,j", treg, sreg,
		       (int) BFD_RELOC_LO16);
	  return;
	}
      load_register (&icnt, AT, &imm_expr, dbl);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s2, "d,v,t",
		   treg, sreg, AT);
      break;

    case M_AND_I:
      s = "andi";
      s2 = "and";
      goto do_bit;
    case M_OR_I:
      s = "ori";
      s2 = "or";
      goto do_bit;
    case M_NOR_I:
      s = "";
      s2 = "nor";
      goto do_bit;
    case M_XOR_I:
      s = "xori";
      s2 = "xor";
    do_bit:
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number >= 0
	  && imm_expr.X_add_number < 0x10000)
	{
	  if (mask != M_NOR_I)
	    macro_build ((char *) NULL, &icnt, &imm_expr, s, "t,r,i", treg,
			 sreg, (int) BFD_RELOC_LO16);
	  else
	    {
	      macro_build ((char *) NULL, &icnt, &imm_expr, "ori", "t,r,i",
			   treg, sreg, (int) BFD_RELOC_LO16);
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nor",
			   "d,v,t", treg, treg, 0);
	    }
	  return;
	}

      load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s2, "d,v,t",
		   treg, sreg, AT);
      break;

    case M_BEQ_I:
      s = "beq";
      goto beq_i;
    case M_BEQL_I:
      s = "beql";
      likely = 1;
      goto beq_i;
    case M_BNE_I:
      s = "bne";
      goto beq_i;
    case M_BNEL_I:
      s = "bnel";
      likely = 1;
    beq_i:
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr, s, "s,t,p", sreg,
		       0);
	  return;
	}
      load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
      macro_build ((char *) NULL, &icnt, &offset_expr, s, "s,t,p", sreg, AT);
      break;

    case M_BGEL:
      likely = 1;
    case M_BGE:
      if (treg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bgezl" : "bgez", "s,p", sreg);
	  return;
	}
      if (sreg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "blezl" : "blez", "s,p", treg);
	  return;
	}
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "slt", "d,v,t",
		   AT, sreg, treg);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "beql" : "beq", "s,t,p", AT, 0);
      break;

    case M_BGTL_I:
      likely = 1;
    case M_BGT_I:
      /* check for > max integer */
      maxnum = 0x7fffffff;
      if (HAVE_64BIT_GPRS && sizeof (maxnum) > 4)
	{
	  maxnum <<= 16;
	  maxnum |= 0xffff;
	  maxnum <<= 16;
	  maxnum |= 0xffff;
	}
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number >= maxnum
	  && (HAVE_32BIT_GPRS || sizeof (maxnum) > 4))
	{
	do_false:
	  /* result is always false */
	  if (! likely)
	    {
	      if (warn_nops)
		as_warn (_("Branch %s is always false (nop)"),
			 ip->insn_mo->name);
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop",
			   "", 0);
	    }
	  else
	    {
	      if (warn_nops)
		as_warn (_("Branch likely %s is always false"),
			 ip->insn_mo->name);
	      macro_build ((char *) NULL, &icnt, &offset_expr, "bnel",
			   "s,t,p", 0, 0);
	    }
	  return;
	}
      if (imm_expr.X_op != O_constant)
	as_bad (_("Unsupported large constant"));
      ++imm_expr.X_add_number;
      /* FALLTHROUGH */
    case M_BGE_I:
    case M_BGEL_I:
      if (mask == M_BGEL_I)
	likely = 1;
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bgezl" : "bgez", "s,p", sreg);
	  return;
	}
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 1)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bgtzl" : "bgtz", "s,p", sreg);
	  return;
	}
      maxnum = 0x7fffffff;
      if (HAVE_64BIT_GPRS && sizeof (maxnum) > 4)
	{
	  maxnum <<= 16;
	  maxnum |= 0xffff;
	  maxnum <<= 16;
	  maxnum |= 0xffff;
	}
      maxnum = - maxnum - 1;
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number <= maxnum
	  && (HAVE_32BIT_GPRS || sizeof (maxnum) > 4))
	{
	do_true:
	  /* result is always true */
	  as_warn (_("Branch %s is always true"), ip->insn_mo->name);
	  macro_build ((char *) NULL, &icnt, &offset_expr, "b", "p");
	  return;
	}
      set_at (&icnt, sreg, 0);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "beql" : "beq", "s,t,p", AT, 0);
      break;

    case M_BGEUL:
      likely = 1;
    case M_BGEU:
      if (treg == 0)
	goto do_true;
      if (sreg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "beql" : "beq", "s,t,p", 0, treg);
	  return;
	}
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		   "d,v,t", AT, sreg, treg);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "beql" : "beq", "s,t,p", AT, 0);
      break;

    case M_BGTUL_I:
      likely = 1;
    case M_BGTU_I:
      if (sreg == 0
	  || (HAVE_32BIT_GPRS
	      && imm_expr.X_op == O_constant
	      && imm_expr.X_add_number == (offsetT) 0xffffffff))
	goto do_false;
      if (imm_expr.X_op != O_constant)
	as_bad (_("Unsupported large constant"));
      ++imm_expr.X_add_number;
      /* FALLTHROUGH */
    case M_BGEU_I:
    case M_BGEUL_I:
      if (mask == M_BGEUL_I)
	likely = 1;
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 0)
	goto do_true;
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 1)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bnel" : "bne", "s,t,p", sreg, 0);
	  return;
	}
      set_at (&icnt, sreg, 1);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "beql" : "beq", "s,t,p", AT, 0);
      break;

    case M_BGTL:
      likely = 1;
    case M_BGT:
      if (treg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bgtzl" : "bgtz", "s,p", sreg);
	  return;
	}
      if (sreg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bltzl" : "bltz", "s,p", treg);
	  return;
	}
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "slt", "d,v,t",
		   AT, treg, sreg);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "bnel" : "bne", "s,t,p", AT, 0);
      break;

    case M_BGTUL:
      likely = 1;
    case M_BGTU:
      if (treg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bnel" : "bne", "s,t,p", sreg, 0);
	  return;
	}
      if (sreg == 0)
	goto do_false;
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		   "d,v,t", AT, treg, sreg);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "bnel" : "bne", "s,t,p", AT, 0);
      break;

    case M_BLEL:
      likely = 1;
    case M_BLE:
      if (treg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "blezl" : "blez", "s,p", sreg);
	  return;
	}
      if (sreg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bgezl" : "bgez", "s,p", treg);
	  return;
	}
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "slt", "d,v,t",
		   AT, treg, sreg);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "beql" : "beq", "s,t,p", AT, 0);
      break;

    case M_BLEL_I:
      likely = 1;
    case M_BLE_I:
      maxnum = 0x7fffffff;
      if (HAVE_64BIT_GPRS && sizeof (maxnum) > 4)
	{
	  maxnum <<= 16;
	  maxnum |= 0xffff;
	  maxnum <<= 16;
	  maxnum |= 0xffff;
	}
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number >= maxnum
	  && (HAVE_32BIT_GPRS || sizeof (maxnum) > 4))
	goto do_true;
      if (imm_expr.X_op != O_constant)
	as_bad (_("Unsupported large constant"));
      ++imm_expr.X_add_number;
      /* FALLTHROUGH */
    case M_BLT_I:
    case M_BLTL_I:
      if (mask == M_BLTL_I)
	likely = 1;
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bltzl" : "bltz", "s,p", sreg);
	  return;
	}
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 1)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "blezl" : "blez", "s,p", sreg);
	  return;
	}
      set_at (&icnt, sreg, 0);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "bnel" : "bne", "s,t,p", AT, 0);
      break;

    case M_BLEUL:
      likely = 1;
    case M_BLEU:
      if (treg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "beql" : "beq", "s,t,p", sreg, 0);
	  return;
	}
      if (sreg == 0)
	goto do_true;
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		   "d,v,t", AT, treg, sreg);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "beql" : "beq", "s,t,p", AT, 0);
      break;

    case M_BLEUL_I:
      likely = 1;
    case M_BLEU_I:
      if (sreg == 0
	  || (HAVE_32BIT_GPRS
	      && imm_expr.X_op == O_constant
	      && imm_expr.X_add_number == (offsetT) 0xffffffff))
	goto do_true;
      if (imm_expr.X_op != O_constant)
	as_bad (_("Unsupported large constant"));
      ++imm_expr.X_add_number;
      /* FALLTHROUGH */
    case M_BLTU_I:
    case M_BLTUL_I:
      if (mask == M_BLTUL_I)
	likely = 1;
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 0)
	goto do_false;
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 1)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "beql" : "beq",
		       "s,t,p", sreg, 0);
	  return;
	}
      set_at (&icnt, sreg, 1);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "bnel" : "bne", "s,t,p", AT, 0);
      break;

    case M_BLTL:
      likely = 1;
    case M_BLT:
      if (treg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bltzl" : "bltz", "s,p", sreg);
	  return;
	}
      if (sreg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bgtzl" : "bgtz", "s,p", treg);
	  return;
	}
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "slt", "d,v,t",
		   AT, sreg, treg);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "bnel" : "bne", "s,t,p", AT, 0);
      break;

    case M_BLTUL:
      likely = 1;
    case M_BLTU:
      if (treg == 0)
	goto do_false;
      if (sreg == 0)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       likely ? "bnel" : "bne", "s,t,p", 0, treg);
	  return;
	}
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		   "d,v,t", AT, sreg,
		   treg);
      macro_build ((char *) NULL, &icnt, &offset_expr,
		   likely ? "bnel" : "bne", "s,t,p", AT, 0);
      break;

    case M_DDIV_3:
      dbl = 1;
    case M_DIV_3:
      s = "mflo";
      goto do_div3;
    case M_DREM_3:
      dbl = 1;
    case M_REM_3:
      s = "mfhi";
    do_div3:
      if (treg == 0)
	{
	  as_warn (_("Divide by zero."));
	  if (mips_trap)
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "teq",
			 "s,t,q", 0, 0, 7);
	  else
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "break",
			 "c", 7);
	  return;
	}

      mips_emit_delays (true);
      ++mips_opts.noreorder;
      mips_any_noreorder = 1;
      if (mips_trap)
	{
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "teq",
		       "s,t,q", treg, 0, 7);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		       dbl ? "ddiv" : "div", "z,s,t", sreg, treg);
	}
      else
	{
	  expr1.X_add_number = 8;
	  macro_build ((char *) NULL, &icnt, &expr1, "bne", "s,t,p", treg, 0);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		       dbl ? "ddiv" : "div", "z,s,t", sreg, treg);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "break",
		       "c", 7);
	}
      expr1.X_add_number = -1;
      macro_build ((char *) NULL, &icnt, &expr1,
		   dbl ? "daddiu" : "addiu",
		   "t,r,j", AT, 0, (int) BFD_RELOC_LO16);
      expr1.X_add_number = mips_trap ? (dbl ? 12 : 8) : (dbl ? 20 : 16);
      macro_build ((char *) NULL, &icnt, &expr1, "bne", "s,t,p", treg, AT);
      if (dbl)
	{
	  expr1.X_add_number = 1;
	  macro_build ((char *) NULL, &icnt, &expr1, "daddiu", "t,r,j", AT, 0,
		       (int) BFD_RELOC_LO16);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "dsll32",
		       "d,w,<", AT, AT, 31);
	}
      else
	{
	  expr1.X_add_number = 0x80000000;
	  macro_build ((char *) NULL, &icnt, &expr1, "lui", "t,u", AT,
		       (int) BFD_RELOC_HI16);
	}
      if (mips_trap)
	{
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "teq",
		       "s,t,q", sreg, AT, 6);
	  /* We want to close the noreorder block as soon as possible, so
	     that later insns are available for delay slot filling.  */
	  --mips_opts.noreorder;
	}
      else
	{
	  expr1.X_add_number = 8;
	  macro_build ((char *) NULL, &icnt, &expr1, "bne", "s,t,p", sreg, AT);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "",
		       0);

	  /* We want to close the noreorder block as soon as possible, so
	     that later insns are available for delay slot filling.  */
	  --mips_opts.noreorder;

	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "break",
		       "c", 6);
	}
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "d", dreg);
      break;

    case M_DIV_3I:
      s = "div";
      s2 = "mflo";
      goto do_divi;
    case M_DIVU_3I:
      s = "divu";
      s2 = "mflo";
      goto do_divi;
    case M_REM_3I:
      s = "div";
      s2 = "mfhi";
      goto do_divi;
    case M_REMU_3I:
      s = "divu";
      s2 = "mfhi";
      goto do_divi;
    case M_DDIV_3I:
      dbl = 1;
      s = "ddiv";
      s2 = "mflo";
      goto do_divi;
    case M_DDIVU_3I:
      dbl = 1;
      s = "ddivu";
      s2 = "mflo";
      goto do_divi;
    case M_DREM_3I:
      dbl = 1;
      s = "ddiv";
      s2 = "mfhi";
      goto do_divi;
    case M_DREMU_3I:
      dbl = 1;
      s = "ddivu";
      s2 = "mfhi";
    do_divi:
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 0)
	{
	  as_warn (_("Divide by zero."));
	  if (mips_trap)
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "teq",
			 "s,t,q", 0, 0, 7);
	  else
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "break",
			 "c", 7);
	  return;
	}
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 1)
	{
	  if (strcmp (s2, "mflo") == 0)
	    move_register (&icnt, dreg, sreg);
	  else
	    move_register (&icnt, dreg, 0);
	  return;
	}
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number == -1
	  && s[strlen (s) - 1] != 'u')
	{
	  if (strcmp (s2, "mflo") == 0)
	    {
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   dbl ? "dneg" : "neg", "d,w", dreg, sreg);
	    }
	  else
	    move_register (&icnt, dreg, 0);
	  return;
	}

      load_register (&icnt, AT, &imm_expr, dbl);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "z,s,t",
		   sreg, AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s2, "d", dreg);
      break;

    case M_DIVU_3:
      s = "divu";
      s2 = "mflo";
      goto do_divu3;
    case M_REMU_3:
      s = "divu";
      s2 = "mfhi";
      goto do_divu3;
    case M_DDIVU_3:
      s = "ddivu";
      s2 = "mflo";
      goto do_divu3;
    case M_DREMU_3:
      s = "ddivu";
      s2 = "mfhi";
    do_divu3:
      mips_emit_delays (true);
      ++mips_opts.noreorder;
      mips_any_noreorder = 1;
      if (mips_trap)
	{
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "teq",
		       "s,t,q", treg, 0, 7);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "z,s,t",
		       sreg, treg);
	  /* We want to close the noreorder block as soon as possible, so
	     that later insns are available for delay slot filling.  */
	  --mips_opts.noreorder;
	}
      else
	{
	  expr1.X_add_number = 8;
	  macro_build ((char *) NULL, &icnt, &expr1, "bne", "s,t,p", treg, 0);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "z,s,t",
		       sreg, treg);

	  /* We want to close the noreorder block as soon as possible, so
	     that later insns are available for delay slot filling.  */
	  --mips_opts.noreorder;
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "break",
		       "c", 7);
	}
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s2, "d", dreg);
      return;

    case M_DLA_AB:
      dbl = 1;
    case M_LA_AB:
      /* Load the address of a symbol into a register.  If breg is not
	 zero, we then add a base register to it.  */

      if (dbl && HAVE_32BIT_GPRS)
	as_warn (_("dla used to load 32-bit register"));

      if (! dbl && HAVE_64BIT_OBJECTS)
	as_warn (_("la used to load 64-bit address"));

      if (offset_expr.X_op == O_constant
	  && offset_expr.X_add_number >= -0x8000
	  && offset_expr.X_add_number < 0x8000)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       (dbl || HAVE_64BIT_ADDRESSES) ? "daddiu" : "addiu",
		       "t,r,j", treg, sreg, (int) BFD_RELOC_LO16);
	  return;
	}

      if (treg == breg)
	{
	  tempreg = AT;
	  used_at = 1;
	}
      else
	{
	  tempreg = treg;
	  used_at = 0;
	}

      /* When generating embedded PIC code, we permit expressions of
	 the form
	   la	$treg,foo-bar
	   la	$treg,foo-bar($breg)
	 where bar is an address in the current section.  These are used
	 when getting the addresses of functions.  We don't permit
	 X_add_number to be non-zero, because if the symbol is
	 external the relaxing code needs to know that any addend is
	 purely the offset to X_op_symbol.  */
      if (mips_pic == EMBEDDED_PIC
	  && offset_expr.X_op == O_subtract
	  && (symbol_constant_p (offset_expr.X_op_symbol)
	      ? S_GET_SEGMENT (offset_expr.X_op_symbol) == now_seg
	      : (symbol_equated_p (offset_expr.X_op_symbol)
		 && (S_GET_SEGMENT
		     (symbol_get_value_expression (offset_expr.X_op_symbol)
		      ->X_add_symbol)
		     == now_seg)))
	  && (offset_expr.X_add_number == 0
	      || OUTPUT_FLAVOR == bfd_target_elf_flavour))
	{
	  if (breg == 0)
	    {
	      tempreg = treg;
	      used_at = 0;
	      macro_build ((char *) NULL, &icnt, &offset_expr, "lui", "t,u",
			   tempreg, (int) BFD_RELOC_PCREL_HI16_S);
	    }
	  else
	    {
	      macro_build ((char *) NULL, &icnt, &offset_expr, "lui", "t,u",
			   tempreg, (int) BFD_RELOC_PCREL_HI16_S);
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   (dbl || HAVE_64BIT_ADDRESSES) ? "daddu" : "addu",
			   "d,v,t", tempreg, tempreg, breg);
	    }
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       (dbl || HAVE_64BIT_ADDRESSES) ? "daddiu" : "addiu",
		       "t,r,j", treg, tempreg, (int) BFD_RELOC_PCREL_LO16);
	  if (! used_at)
	    return;
	  break;
	}

      if (offset_expr.X_op != O_symbol
	  && offset_expr.X_op != O_constant)
	{
	  as_bad (_("expression too complex"));
	  offset_expr.X_op = O_constant;
	}

      if (offset_expr.X_op == O_constant)
	load_register (&icnt, tempreg, &offset_expr,
		       ((mips_pic == EMBEDDED_PIC || mips_pic == NO_PIC)
			? (dbl || HAVE_64BIT_ADDRESSES)
			: HAVE_64BIT_ADDRESSES));
      else if (mips_pic == NO_PIC)
	{
	  /* If this is a reference to a GP relative symbol, we want
	       addiu	$tempreg,$gp,<sym>	(BFD_RELOC_GPREL16)
	     Otherwise we want
	       lui	$tempreg,<sym>		(BFD_RELOC_HI16_S)
	       addiu	$tempreg,$tempreg,<sym>	(BFD_RELOC_LO16)
	     If we have a constant, we need two instructions anyhow,
	     so we may as well always use the latter form.

	    With 64bit address space and a usable $at we want
	      lui	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHEST)
	      lui	$at,<sym>		(BFD_RELOC_HI16_S)
	      daddiu	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHER)
	      daddiu	$at,<sym>		(BFD_RELOC_LO16)
	      dsll32	$tempreg,0
	      daddu	$tempreg,$tempreg,$at

	    If $at is already in use, we use an path which is suboptimal
	    on superscalar processors.
	      lui	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHEST)
	      daddiu	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHER)
	      dsll	$tempreg,16
	      daddiu	$tempreg,<sym>		(BFD_RELOC_HI16_S)
	      dsll	$tempreg,16
	      daddiu	$tempreg,<sym>		(BFD_RELOC_LO16)
	  */
	  char *p = NULL;
	  if (HAVE_64BIT_ADDRESSES)
	    {
	      /* We don't do GP optimization for now because RELAX_ENCODE can't
		 hold the data for such large chunks.  */

	      if (used_at == 0 && ! mips_opts.noat)
		{
		  macro_build (p, &icnt, &offset_expr, "lui", "t,u",
			       tempreg, (int) BFD_RELOC_MIPS_HIGHEST);
		  macro_build (p, &icnt, &offset_expr, "lui", "t,u",
			       AT, (int) BFD_RELOC_HI16_S);
		  macro_build (p, &icnt, &offset_expr, "daddiu", "t,r,j",
			       tempreg, tempreg, (int) BFD_RELOC_MIPS_HIGHER);
		  macro_build (p, &icnt, &offset_expr, "daddiu", "t,r,j",
			       AT, AT, (int) BFD_RELOC_LO16);
		  macro_build (p, &icnt, (expressionS *) NULL, "dsll32",
			       "d,w,<", tempreg, tempreg, 0);
		  macro_build (p, &icnt, (expressionS *) NULL, "daddu",
			       "d,v,t", tempreg, tempreg, AT);
		  used_at = 1;
		}
	      else
		{
		  macro_build (p, &icnt, &offset_expr, "lui", "t,u",
			       tempreg, (int) BFD_RELOC_MIPS_HIGHEST);
		  macro_build (p, &icnt, &offset_expr, "daddiu", "t,r,j",
			       tempreg, tempreg, (int) BFD_RELOC_MIPS_HIGHER);
		  macro_build (p, &icnt, (expressionS *) NULL, "dsll", "d,w,<",
			       tempreg, tempreg, 16);
		  macro_build (p, &icnt, &offset_expr, "daddiu", "t,r,j",
			       tempreg, tempreg, (int) BFD_RELOC_HI16_S);
		  macro_build (p, &icnt, (expressionS *) NULL, "dsll", "d,w,<",
			       tempreg, tempreg, 16);
		  macro_build (p, &icnt, &offset_expr, "daddiu", "t,r,j",
			       tempreg, tempreg, (int) BFD_RELOC_LO16);
		}
	    }
	  else
	    {
	      if ((valueT) offset_expr.X_add_number <= MAX_GPREL_OFFSET
		  && ! nopic_need_relax (offset_expr.X_add_symbol, 1))
		{
		  frag_grow (20);
		  macro_build ((char *) NULL, &icnt, &offset_expr, "addiu",
			       "t,r,j", tempreg, mips_gp_register,
			       (int) BFD_RELOC_GPREL16);
		  p = frag_var (rs_machine_dependent, 8, 0,
				RELAX_ENCODE (4, 8, 0, 4, 0,
					      mips_opts.warn_about_macros),
				offset_expr.X_add_symbol, 0, NULL);
		}
	      macro_build_lui (p, &icnt, &offset_expr, tempreg);
	      if (p != NULL)
		p += 4;
	      macro_build (p, &icnt, &offset_expr, "addiu",
			   "t,r,j", tempreg, tempreg, (int) BFD_RELOC_LO16);
	    }
	}
      else if (mips_pic == SVR4_PIC && ! mips_big_got)
	{
	  int lw_reloc_type = (int) BFD_RELOC_MIPS_GOT16;

	  /* If this is a reference to an external symbol, and there
	     is no constant, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	     or if tempreg is PIC_CALL_REG
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_CALL16)
	     For a local symbol, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	       nop
	       addiu	$tempreg,$tempreg,<sym>	(BFD_RELOC_LO16)

	     If we have a small constant, and this is a reference to
	     an external symbol, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	       nop
	       addiu	$tempreg,$tempreg,<constant>
	     For a local symbol, we want the same instruction
	     sequence, but we output a BFD_RELOC_LO16 reloc on the
	     addiu instruction.

	     If we have a large constant, and this is a reference to
	     an external symbol, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	       lui	$at,<hiconstant>
	       addiu	$at,$at,<loconstant>
	       addu	$tempreg,$tempreg,$at
	     For a local symbol, we want the same instruction
	     sequence, but we output a BFD_RELOC_LO16 reloc on the
	     addiu instruction.

	     For NewABI, we want for local or external data addresses
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT_DISP)
	     For a local function symbol, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT_PAGE)
	       nop
	       addiu	$tempreg,$tempreg,<sym>	(BFD_RELOC_MIPS_GOT_OFST)
	   */

	  expr1.X_add_number = offset_expr.X_add_number;
	  offset_expr.X_add_number = 0;
	  frag_grow (32);
	  if (expr1.X_add_number == 0 && tempreg == PIC_CALL_REG)
	    lw_reloc_type = (int) BFD_RELOC_MIPS_CALL16;
	  else if (HAVE_NEWABI)
	    lw_reloc_type = (int) BFD_RELOC_MIPS_GOT_DISP;
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
		       "t,o(b)", tempreg, lw_reloc_type, mips_gp_register);
	  if (expr1.X_add_number == 0)
	    {
	      int off;
	      char *p;

	      if (breg == 0)
		off = 0;
	      else
		{
		  /* We're going to put in an addu instruction using
		     tempreg, so we may as well insert the nop right
		     now.  */
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       "nop", "");
		  off = 4;
		}
	      p = frag_var (rs_machine_dependent, 8 - off, 0,
			    RELAX_ENCODE (0, 8 - off, -4 - off, 4 - off, 0,
					  (breg == 0
					   ? mips_opts.warn_about_macros
					   : 0)),
			    offset_expr.X_add_symbol, 0, NULL);
	      if (breg == 0)
		{
		  macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
		  p += 4;
		}
	      macro_build (p, &icnt, &expr1,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
			   "t,r,j", tempreg, tempreg, (int) BFD_RELOC_LO16);
	      /* FIXME: If breg == 0, and the next instruction uses
		 $tempreg, then if this variant case is used an extra
		 nop will be generated.  */
	    }
	  else if (expr1.X_add_number >= -0x8000
		   && expr1.X_add_number < 0x8000)
	    {
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   "nop", "");
	      macro_build ((char *) NULL, &icnt, &expr1,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
			   "t,r,j", tempreg, tempreg, (int) BFD_RELOC_LO16);
	      frag_var (rs_machine_dependent, 0, 0,
			RELAX_ENCODE (0, 0, -12, -4, 0, 0),
			offset_expr.X_add_symbol, 0, NULL);
	    }
	  else
	    {
	      int off1;

	      /* If we are going to add in a base register, and the
		 target register and the base register are the same,
		 then we are using AT as a temporary register.  Since
		 we want to load the constant into AT, we add our
		 current AT (from the global offset table) and the
		 register into the register now, and pretend we were
		 not using a base register.  */
	      if (breg != treg)
		off1 = 0;
	      else
		{
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       "nop", "");
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			       "d,v,t", treg, AT, breg);
		  breg = 0;
		  tempreg = treg;
		  off1 = -8;
		}

	      /* Set mips_optimize around the lui instruction to avoid
		 inserting an unnecessary nop after the lw.  */
	      hold_mips_optimize = mips_optimize;
	      mips_optimize = 2;
	      macro_build_lui (NULL, &icnt, &expr1, AT);
	      mips_optimize = hold_mips_optimize;

	      macro_build ((char *) NULL, &icnt, &expr1,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
			   "t,r,j", AT, AT, (int) BFD_RELOC_LO16);
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			   "d,v,t", tempreg, tempreg, AT);
	      frag_var (rs_machine_dependent, 0, 0,
			RELAX_ENCODE (0, 0, -16 + off1, -8, 0, 0),
			offset_expr.X_add_symbol, 0, NULL);
	      used_at = 1;
	    }
	}
      else if (mips_pic == SVR4_PIC)
	{
	  int gpdel;
	  char *p;
	  int lui_reloc_type = (int) BFD_RELOC_MIPS_GOT_HI16;
	  int lw_reloc_type = (int) BFD_RELOC_MIPS_GOT_LO16;
	  int local_reloc_type = (int) BFD_RELOC_MIPS_GOT16;

	  /* This is the large GOT case.  If this is a reference to an
	     external symbol, and there is no constant, we want
	       lui	$tempreg,<sym>		(BFD_RELOC_MIPS_GOT_HI16)
	       addu	$tempreg,$tempreg,$gp
	       lw	$tempreg,<sym>($tempreg) (BFD_RELOC_MIPS_GOT_LO16)
	     or if tempreg is PIC_CALL_REG
	       lui	$tempreg,<sym>		(BFD_RELOC_MIPS_CALL_HI16)
	       addu	$tempreg,$tempreg,$gp
	       lw	$tempreg,<sym>($tempreg) (BFD_RELOC_MIPS_CALL_LO16)
	     For a local symbol, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	       nop
	       addiu	$tempreg,$tempreg,<sym>	(BFD_RELOC_LO16)

	     If we have a small constant, and this is a reference to
	     an external symbol, we want
	       lui	$tempreg,<sym>		(BFD_RELOC_MIPS_GOT_HI16)
	       addu	$tempreg,$tempreg,$gp
	       lw	$tempreg,<sym>($tempreg) (BFD_RELOC_MIPS_GOT_LO16)
	       nop
	       addiu	$tempreg,$tempreg,<constant>
	     For a local symbol, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	       nop
	       addiu	$tempreg,$tempreg,<constant> (BFD_RELOC_LO16)

	     If we have a large constant, and this is a reference to
	     an external symbol, we want
	       lui	$tempreg,<sym>		(BFD_RELOC_MIPS_GOT_HI16)
	       addu	$tempreg,$tempreg,$gp
	       lw	$tempreg,<sym>($tempreg) (BFD_RELOC_MIPS_GOT_LO16)
	       lui	$at,<hiconstant>
	       addiu	$at,$at,<loconstant>
	       addu	$tempreg,$tempreg,$at
	     For a local symbol, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	       lui	$at,<hiconstant>
	       addiu	$at,$at,<loconstant>	(BFD_RELOC_LO16)
	       addu	$tempreg,$tempreg,$at

	     For NewABI, we want for local data addresses
              lw       $tempreg,<sym>($gp)     (BFD_RELOC_MIPS_GOT_DISP)
	   */

	  expr1.X_add_number = offset_expr.X_add_number;
	  offset_expr.X_add_number = 0;
	  frag_grow (52);
	  if (reg_needs_delay (mips_gp_register))
	    gpdel = 4;
	  else
	    gpdel = 0;
	  if (expr1.X_add_number == 0 && tempreg == PIC_CALL_REG)
	    {
	      lui_reloc_type = (int) BFD_RELOC_MIPS_CALL_HI16;
	      lw_reloc_type = (int) BFD_RELOC_MIPS_CALL_LO16;
	    }
	  macro_build ((char *) NULL, &icnt, &offset_expr, "lui", "t,u",
		       tempreg, lui_reloc_type);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
		       "d,v,t", tempreg, tempreg, mips_gp_register);
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
		       "t,o(b)", tempreg, lw_reloc_type, tempreg);
	  if (expr1.X_add_number == 0)
	    {
	      int off;

	      if (breg == 0)
		off = 0;
	      else
		{
		  /* We're going to put in an addu instruction using
		     tempreg, so we may as well insert the nop right
		     now.  */
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       "nop", "");
		  off = 4;
		}

	      p = frag_var (rs_machine_dependent, 12 + gpdel, 0,
			    RELAX_ENCODE (12 + off, 12 + gpdel, gpdel,
					  8 + gpdel, 0,
					  (breg == 0
					   ? mips_opts.warn_about_macros
					   : 0)),
			    offset_expr.X_add_symbol, 0, NULL);
	    }
	  else if (expr1.X_add_number >= -0x8000
		   && expr1.X_add_number < 0x8000)
	    {
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   "nop", "");
	      macro_build ((char *) NULL, &icnt, &expr1,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
			   "t,r,j", tempreg, tempreg, (int) BFD_RELOC_LO16);

	      p = frag_var (rs_machine_dependent, 12 + gpdel, 0,
			    RELAX_ENCODE (20, 12 + gpdel, gpdel, 8 + gpdel, 0,
					  (breg == 0
					   ? mips_opts.warn_about_macros
					   : 0)),
			    offset_expr.X_add_symbol, 0, NULL);
	    }
	  else
	    {
	      int adj, dreg;

	      /* If we are going to add in a base register, and the
		 target register and the base register are the same,
		 then we are using AT as a temporary register.  Since
		 we want to load the constant into AT, we add our
		 current AT (from the global offset table) and the
		 register into the register now, and pretend we were
		 not using a base register.  */
	      if (breg != treg)
		{
		  adj = 0;
		  dreg = tempreg;
		}
	      else
		{
		  assert (tempreg == AT);
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       "nop", "");
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			       "d,v,t", treg, AT, breg);
		  dreg = treg;
		  adj = 8;
		}

	      /* Set mips_optimize around the lui instruction to avoid
		 inserting an unnecessary nop after the lw.  */
	      hold_mips_optimize = mips_optimize;
	      mips_optimize = 2;
	      macro_build_lui (NULL, &icnt, &expr1, AT);
	      mips_optimize = hold_mips_optimize;

	      macro_build ((char *) NULL, &icnt, &expr1,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
			   "t,r,j", AT, AT, (int) BFD_RELOC_LO16);
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			   "d,v,t", dreg, dreg, AT);

	      p = frag_var (rs_machine_dependent, 16 + gpdel + adj, 0,
			    RELAX_ENCODE (24 + adj, 16 + gpdel + adj, gpdel,
					  8 + gpdel, 0,
					  (breg == 0
					   ? mips_opts.warn_about_macros
					   : 0)),
			    offset_expr.X_add_symbol, 0, NULL);

	      used_at = 1;
	    }

	  if (gpdel > 0)
	    {
	      /* This is needed because this instruction uses $gp, but
                 the first instruction on the main stream does not.  */
	      macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
	      p += 4;
	    }

	  if (HAVE_NEWABI)
	    local_reloc_type = (int) BFD_RELOC_MIPS_GOT_DISP;
	  macro_build (p, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
		       "t,o(b)", tempreg,
		       local_reloc_type,
		       mips_gp_register);
	  p += 4;
	  if (expr1.X_add_number == 0 && HAVE_NEWABI)
	    {
	      /* BFD_RELOC_MIPS_GOT_DISP is sufficient for newabi */
	    }
	 else
	   if (expr1.X_add_number >= -0x8000
	      && expr1.X_add_number < 0x8000)
	    {
	      macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
	      p += 4;
	      macro_build (p, &icnt, &expr1,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
			   "t,r,j", tempreg, tempreg, (int) BFD_RELOC_LO16);
	      /* FIXME: If add_number is 0, and there was no base
                 register, the external symbol case ended with a load,
                 so if the symbol turns out to not be external, and
                 the next instruction uses tempreg, an unnecessary nop
                 will be inserted.  */
	    }
	  else
	    {
	      if (breg == treg)
		{
		  /* We must add in the base register now, as in the
                     external symbol case.  */
		  assert (tempreg == AT);
		  macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
		  p += 4;
		  macro_build (p, &icnt, (expressionS *) NULL,
			       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			       "d,v,t", treg, AT, breg);
		  p += 4;
		  tempreg = treg;
		  /* We set breg to 0 because we have arranged to add
                     it in in both cases.  */
		  breg = 0;
		}

	      macro_build_lui (p, &icnt, &expr1, AT);
	      p += 4;
	      macro_build (p, &icnt, &expr1,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
			   "t,r,j", AT, AT, (int) BFD_RELOC_LO16);
	      p += 4;
	      macro_build (p, &icnt, (expressionS *) NULL,
			   HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			   "d,v,t", tempreg, tempreg, AT);
	      p += 4;
	    }
	}
      else if (mips_pic == EMBEDDED_PIC)
	{
	  /* We use
	       addiu	$tempreg,$gp,<sym>	(BFD_RELOC_GPREL16)
	     */
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu", "t,r,j",
		       tempreg, mips_gp_register, (int) BFD_RELOC_GPREL16);
	}
      else
	abort ();

      if (breg != 0)
	{
	  char *s;

	  if (mips_pic == EMBEDDED_PIC || mips_pic == NO_PIC)
	    s = (dbl || HAVE_64BIT_ADDRESSES) ? "daddu" : "addu";
	  else
	    s = HAVE_64BIT_ADDRESSES ? "daddu" : "addu";

	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s,
		       "d,v,t", treg, tempreg, breg);
	}

      if (! used_at)
	return;

      break;

    case M_J_A:
      /* The j instruction may not be used in PIC code, since it
	 requires an absolute address.  We convert it to a b
	 instruction.  */
      if (mips_pic == NO_PIC)
	macro_build ((char *) NULL, &icnt, &offset_expr, "j", "a");
      else
	macro_build ((char *) NULL, &icnt, &offset_expr, "b", "p");
      return;

      /* The jal instructions must be handled as macros because when
	 generating PIC code they expand to multi-instruction
	 sequences.  Normally they are simple instructions.  */
    case M_JAL_1:
      dreg = RA;
      /* Fall through.  */
    case M_JAL_2:
      if (mips_pic == NO_PIC
	  || mips_pic == EMBEDDED_PIC)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "jalr",
		     "d,s", dreg, sreg);
      else if (mips_pic == SVR4_PIC)
	{
	  if (sreg != PIC_CALL_REG)
	    as_warn (_("MIPS PIC call to register other than $25"));

	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "jalr",
		       "d,s", dreg, sreg);
	  if (! HAVE_NEWABI)
	    {
	      if (mips_cprestore_offset < 0)
		as_warn (_("No .cprestore pseudo-op used in PIC code"));
	      else
		{
		  if (! mips_frame_reg_valid)
		    {
		      as_warn (_("No .frame pseudo-op used in PIC code"));
		      /* Quiet this warning.  */
		      mips_frame_reg_valid = 1;
		    }
		  if (! mips_cprestore_valid)
		    {
		      as_warn (_("No .cprestore pseudo-op used in PIC code"));
		      /* Quiet this warning.  */
		      mips_cprestore_valid = 1;
		    }
		  expr1.X_add_number = mips_cprestore_offset;
  		  macro_build_ldst_constoffset ((char *) NULL, &icnt, &expr1,
					        HAVE_32BIT_ADDRESSES ? "lw" : "ld",
					        mips_gp_register, mips_frame_reg);
		}
	    }
	}
      else
	abort ();

      return;

    case M_JAL_A:
      if (mips_pic == NO_PIC)
	macro_build ((char *) NULL, &icnt, &offset_expr, "jal", "a");
      else if (mips_pic == SVR4_PIC)
	{
	  char *p;

	  /* If this is a reference to an external symbol, and we are
	     using a small GOT, we want
	       lw	$25,<sym>($gp)		(BFD_RELOC_MIPS_CALL16)
	       nop
	       jalr	$ra,$25
	       nop
	       lw	$gp,cprestore($sp)
	     The cprestore value is set using the .cprestore
	     pseudo-op.  If we are using a big GOT, we want
	       lui	$25,<sym>		(BFD_RELOC_MIPS_CALL_HI16)
	       addu	$25,$25,$gp
	       lw	$25,<sym>($25)		(BFD_RELOC_MIPS_CALL_LO16)
	       nop
	       jalr	$ra,$25
	       nop
	       lw	$gp,cprestore($sp)
	     If the symbol is not external, we want
	       lw	$25,<sym>($gp)		(BFD_RELOC_MIPS_GOT16)
	       nop
	       addiu	$25,$25,<sym>		(BFD_RELOC_LO16)
	       jalr	$ra,$25
	       nop
	       lw $gp,cprestore($sp)
	     For NewABI, we want
	       lw	$25,<sym>($gp)		(BFD_RELOC_MIPS_GOT_DISP)
	       jalr	$ra,$25			(BFD_RELOC_MIPS_JALR)
	   */
	  if (HAVE_NEWABI)
	    {
	      macro_build ((char *) NULL, &icnt, &offset_expr,
			   HAVE_32BIT_ADDRESSES ? "lw" : "ld",
			   "t,o(b)", PIC_CALL_REG,
			   (int) BFD_RELOC_MIPS_GOT_DISP, mips_gp_register);
	      macro_build_jalr (icnt, &offset_expr);
	    }
	  else
	    {
	      frag_grow (40);
	      if (! mips_big_got)
		{
		  macro_build ((char *) NULL, &icnt, &offset_expr,
			       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
			       "t,o(b)", PIC_CALL_REG,
			       (int) BFD_RELOC_MIPS_CALL16, mips_gp_register);
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       "nop", "");
		  p = frag_var (rs_machine_dependent, 4, 0,
				RELAX_ENCODE (0, 4, -8, 0, 0, 0),
				offset_expr.X_add_symbol, 0, NULL);
		}
	      else
		{
		  int gpdel;

		  if (reg_needs_delay (mips_gp_register))
		    gpdel = 4;
		  else
		    gpdel = 0;
		  macro_build ((char *) NULL, &icnt, &offset_expr, "lui",
			       "t,u", PIC_CALL_REG,
			       (int) BFD_RELOC_MIPS_CALL_HI16);
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			       "d,v,t", PIC_CALL_REG, PIC_CALL_REG,
			       mips_gp_register);
		  macro_build ((char *) NULL, &icnt, &offset_expr,
			       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
			       "t,o(b)", PIC_CALL_REG,
			       (int) BFD_RELOC_MIPS_CALL_LO16, PIC_CALL_REG);
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       "nop", "");
		  p = frag_var (rs_machine_dependent, 12 + gpdel, 0,
				RELAX_ENCODE (16, 12 + gpdel, gpdel,
					      8 + gpdel, 0, 0),
				offset_expr.X_add_symbol, 0, NULL);
		  if (gpdel > 0)
		    {
		      macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
		      p += 4;
		    }
		  macro_build (p, &icnt, &offset_expr,
			       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
			       "t,o(b)", PIC_CALL_REG,
			       (int) BFD_RELOC_MIPS_GOT16, mips_gp_register);
		  p += 4;
		  macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
		  p += 4;
		}
	      macro_build (p, &icnt, &offset_expr,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
			   "t,r,j", PIC_CALL_REG, PIC_CALL_REG,
			   (int) BFD_RELOC_LO16);
	      macro_build_jalr (icnt, &offset_expr);

	      if (mips_cprestore_offset < 0)
		as_warn (_("No .cprestore pseudo-op used in PIC code"));
	      else
		{
		  if (! mips_frame_reg_valid)
		    {
		      as_warn (_("No .frame pseudo-op used in PIC code"));
		      /* Quiet this warning.  */
		      mips_frame_reg_valid = 1;
		    }
		  if (! mips_cprestore_valid)
		    {
		      as_warn (_("No .cprestore pseudo-op used in PIC code"));
		      /* Quiet this warning.  */
		      mips_cprestore_valid = 1;
		    }
		  if (mips_opts.noreorder)
		    macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
				 "nop", "");
		  expr1.X_add_number = mips_cprestore_offset;
  		  macro_build_ldst_constoffset ((char *) NULL, &icnt, &expr1,
					        HAVE_32BIT_ADDRESSES ? "lw" : "ld",
					        mips_gp_register, mips_frame_reg);
		}
	    }
	}
      else if (mips_pic == EMBEDDED_PIC)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr, "bal", "p");
	  /* The linker may expand the call to a longer sequence which
	     uses $at, so we must break rather than return.  */
	  break;
	}
      else
	abort ();

      return;

    case M_LB_AB:
      s = "lb";
      goto ld;
    case M_LBU_AB:
      s = "lbu";
      goto ld;
    case M_LH_AB:
      s = "lh";
      goto ld;
    case M_LHU_AB:
      s = "lhu";
      goto ld;
    case M_LW_AB:
      s = "lw";
      goto ld;
    case M_LWC0_AB:
      s = "lwc0";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto ld;
    case M_LWC1_AB:
      s = "lwc1";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto ld;
    case M_LWC2_AB:
      s = "lwc2";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto ld;
    case M_LWC3_AB:
      s = "lwc3";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto ld;
    case M_LWL_AB:
      s = "lwl";
      lr = 1;
      goto ld;
    case M_LWR_AB:
      s = "lwr";
      lr = 1;
      goto ld;
    case M_LDC1_AB:
      if (mips_arch == CPU_R4650)
	{
	  as_bad (_("opcode not supported on this processor"));
	  return;
	}
      s = "ldc1";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto ld;
    case M_LDC2_AB:
      s = "ldc2";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto ld;
    case M_LDC3_AB:
      s = "ldc3";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto ld;
    case M_LDL_AB:
      s = "ldl";
      lr = 1;
      goto ld;
    case M_LDR_AB:
      s = "ldr";
      lr = 1;
      goto ld;
    case M_LL_AB:
      s = "ll";
      goto ld;
    case M_LLD_AB:
      s = "lld";
      goto ld;
    case M_LWU_AB:
      s = "lwu";
    ld:
      if (breg == treg || coproc || lr)
	{
	  tempreg = AT;
	  used_at = 1;
	}
      else
	{
	  tempreg = treg;
	  used_at = 0;
	}
      goto ld_st;
    case M_SB_AB:
      s = "sb";
      goto st;
    case M_SH_AB:
      s = "sh";
      goto st;
    case M_SW_AB:
      s = "sw";
      goto st;
    case M_SWC0_AB:
      s = "swc0";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto st;
    case M_SWC1_AB:
      s = "swc1";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto st;
    case M_SWC2_AB:
      s = "swc2";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto st;
    case M_SWC3_AB:
      s = "swc3";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto st;
    case M_SWL_AB:
      s = "swl";
      goto st;
    case M_SWR_AB:
      s = "swr";
      goto st;
    case M_SC_AB:
      s = "sc";
      goto st;
    case M_SCD_AB:
      s = "scd";
      goto st;
    case M_SDC1_AB:
      if (mips_arch == CPU_R4650)
	{
	  as_bad (_("opcode not supported on this processor"));
	  return;
	}
      s = "sdc1";
      coproc = 1;
      /* Itbl support may require additional care here.  */
      goto st;
    case M_SDC2_AB:
      s = "sdc2";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto st;
    case M_SDC3_AB:
      s = "sdc3";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto st;
    case M_SDL_AB:
      s = "sdl";
      goto st;
    case M_SDR_AB:
      s = "sdr";
    st:
      tempreg = AT;
      used_at = 1;
    ld_st:
      /* Itbl support may require additional care here.  */
      if (mask == M_LWC1_AB
	  || mask == M_SWC1_AB
	  || mask == M_LDC1_AB
	  || mask == M_SDC1_AB
	  || mask == M_L_DAB
	  || mask == M_S_DAB)
	fmt = "T,o(b)";
      else if (coproc)
	fmt = "E,o(b)";
      else
	fmt = "t,o(b)";

      /* For embedded PIC, we allow loads where the offset is calculated
         by subtracting a symbol in the current segment from an unknown
         symbol, relative to a base register, e.g.:
		<op>	$treg, <sym>-<localsym>($breg)
	 This is used by the compiler for switch statements.  */
      if (mips_pic == EMBEDDED_PIC
          && offset_expr.X_op == O_subtract
          && (symbol_constant_p (offset_expr.X_op_symbol)
              ? S_GET_SEGMENT (offset_expr.X_op_symbol) == now_seg
              : (symbol_equated_p (offset_expr.X_op_symbol)
                 && (S_GET_SEGMENT
                     (symbol_get_value_expression (offset_expr.X_op_symbol)
                      ->X_add_symbol)
                     == now_seg)))
          && breg != 0
          && (offset_expr.X_add_number == 0
              || OUTPUT_FLAVOR == bfd_target_elf_flavour))
        {
          /* For this case, we output the instructions:
                lui     $tempreg,<sym>          (BFD_RELOC_PCREL_HI16_S)
                addiu   $tempreg,$tempreg,$breg
                <op>    $treg,<sym>($tempreg)   (BFD_RELOC_PCREL_LO16)
             If the relocation would fit entirely in 16 bits, it would be
             nice to emit:
                <op>    $treg,<sym>($breg)      (BFD_RELOC_PCREL_LO16)
             instead, but that seems quite difficult.  */
          macro_build ((char *) NULL, &icnt, &offset_expr, "lui", "t,u",
                       tempreg, (int) BFD_RELOC_PCREL_HI16_S);
          macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
                       ((bfd_arch_bits_per_address (stdoutput) == 32
                         || ! ISA_HAS_64BIT_REGS (mips_opts.isa))
                        ? "addu" : "daddu"),
                       "d,v,t", tempreg, tempreg, breg);
          macro_build ((char *) NULL, &icnt, &offset_expr, s, fmt, treg,
                       (int) BFD_RELOC_PCREL_LO16, tempreg);
          if (! used_at)
            return;
          break;
        }

      if (offset_expr.X_op != O_constant
	  && offset_expr.X_op != O_symbol)
	{
	  as_bad (_("expression too complex"));
	  offset_expr.X_op = O_constant;
	}

      /* A constant expression in PIC code can be handled just as it
	 is in non PIC code.  */
      if (mips_pic == NO_PIC
	  || offset_expr.X_op == O_constant)
	{
	  char *p;

	  /* If this is a reference to a GP relative symbol, and there
	     is no base register, we want
	       <op>	$treg,<sym>($gp)	(BFD_RELOC_GPREL16)
	     Otherwise, if there is no base register, we want
	       lui	$tempreg,<sym>		(BFD_RELOC_HI16_S)
	       <op>	$treg,<sym>($tempreg)	(BFD_RELOC_LO16)
	     If we have a constant, we need two instructions anyhow,
	     so we always use the latter form.

	     If we have a base register, and this is a reference to a
	     GP relative symbol, we want
	       addu	$tempreg,$breg,$gp
	       <op>	$treg,<sym>($tempreg)	(BFD_RELOC_GPREL16)
	     Otherwise we want
	       lui	$tempreg,<sym>		(BFD_RELOC_HI16_S)
	       addu	$tempreg,$tempreg,$breg
	       <op>	$treg,<sym>($tempreg)	(BFD_RELOC_LO16)
	     With a constant we always use the latter case.

	     With 64bit address space and no base register and $at usable,
	     we want
	       lui	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHEST)
	       lui	$at,<sym>		(BFD_RELOC_HI16_S)
	       daddiu	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHER)
	       dsll32	$tempreg,0
	       daddu	$tempreg,$at
	       <op>	$treg,<sym>($tempreg)	(BFD_RELOC_LO16)
	     If we have a base register, we want
	       lui	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHEST)
	       lui	$at,<sym>		(BFD_RELOC_HI16_S)
	       daddiu	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHER)
	       daddu	$at,$breg
	       dsll32	$tempreg,0
	       daddu	$tempreg,$at
	       <op>	$treg,<sym>($tempreg)	(BFD_RELOC_LO16)

	     Without $at we can't generate the optimal path for superscalar
	     processors here since this would require two temporary registers.
	       lui	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHEST)
	       daddiu	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHER)
	       dsll	$tempreg,16
	       daddiu	$tempreg,<sym>		(BFD_RELOC_HI16_S)
	       dsll	$tempreg,16
	       <op>	$treg,<sym>($tempreg)	(BFD_RELOC_LO16)
	     If we have a base register, we want
	       lui	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHEST)
	       daddiu	$tempreg,<sym>		(BFD_RELOC_MIPS_HIGHER)
	       dsll	$tempreg,16
	       daddiu	$tempreg,<sym>		(BFD_RELOC_HI16_S)
	       dsll	$tempreg,16
	       daddu	$tempreg,$tempreg,$breg
	       <op>	$treg,<sym>($tempreg)	(BFD_RELOC_LO16)

	     If we have 64-bit addresses, as an optimization, for
	     addresses which are 32-bit constants (e.g. kseg0/kseg1
	     addresses) we fall back to the 32-bit address generation
	     mechanism since it is more efficient.  Note that due to
	     the signed offset used by memory operations, the 32-bit
	     range is shifted down by 32768 here.  This code should
	     probably attempt to generate 64-bit constants more
	     efficiently in general.
	   */
	  if (HAVE_64BIT_ADDRESSES
	      && !(offset_expr.X_op == O_constant
		   && IS_SEXT_32BIT_NUM (offset_expr.X_add_number + 0x8000)))
	    {
	      p = NULL;

	      /* We don't do GP optimization for now because RELAX_ENCODE can't
		 hold the data for such large chunks.  */

	      if (used_at == 0 && ! mips_opts.noat)
		{
		  macro_build (p, &icnt, &offset_expr, "lui", "t,u",
			       tempreg, (int) BFD_RELOC_MIPS_HIGHEST);
		  macro_build (p, &icnt, &offset_expr, "lui", "t,u",
			       AT, (int) BFD_RELOC_HI16_S);
		  macro_build (p, &icnt, &offset_expr, "daddiu", "t,r,j",
			       tempreg, tempreg, (int) BFD_RELOC_MIPS_HIGHER);
		  if (breg != 0)
		    macro_build (p, &icnt, (expressionS *) NULL, "daddu",
				 "d,v,t", AT, AT, breg);
		  macro_build (p, &icnt, (expressionS *) NULL, "dsll32",
			       "d,w,<", tempreg, tempreg, 0);
		  macro_build (p, &icnt, (expressionS *) NULL, "daddu",
			       "d,v,t", tempreg, tempreg, AT);
		  macro_build (p, &icnt, &offset_expr, s,
			       fmt, treg, (int) BFD_RELOC_LO16, tempreg);
		  used_at = 1;
		}
	      else
		{
		  macro_build (p, &icnt, &offset_expr, "lui", "t,u",
			       tempreg, (int) BFD_RELOC_MIPS_HIGHEST);
		  macro_build (p, &icnt, &offset_expr, "daddiu", "t,r,j",
			       tempreg, tempreg, (int) BFD_RELOC_MIPS_HIGHER);
		  macro_build (p, &icnt, (expressionS *) NULL, "dsll",
			       "d,w,<", tempreg, tempreg, 16);
		  macro_build (p, &icnt, &offset_expr, "daddiu", "t,r,j",
			       tempreg, tempreg, (int) BFD_RELOC_HI16_S);
		  macro_build (p, &icnt, (expressionS *) NULL, "dsll",
			       "d,w,<", tempreg, tempreg, 16);
		  if (breg != 0)
		    macro_build (p, &icnt, (expressionS *) NULL, "daddu",
				 "d,v,t", tempreg, tempreg, breg);
		  macro_build (p, &icnt, &offset_expr, s,
			       fmt, treg, (int) BFD_RELOC_LO16, tempreg);
		}

	      return;
	    }

	  if (breg == 0)
	    {
	      if ((valueT) offset_expr.X_add_number > MAX_GPREL_OFFSET
		  || nopic_need_relax (offset_expr.X_add_symbol, 1))
		p = NULL;
	      else
		{
		  frag_grow (20);
		  macro_build ((char *) NULL, &icnt, &offset_expr, s, fmt,
			       treg, (int) BFD_RELOC_GPREL16,
			       mips_gp_register);
		  p = frag_var (rs_machine_dependent, 8, 0,
				RELAX_ENCODE (4, 8, 0, 4, 0,
					      (mips_opts.warn_about_macros
					       || (used_at
						   && mips_opts.noat))),
				offset_expr.X_add_symbol, 0, NULL);
		  used_at = 0;
		}
	      macro_build_lui (p, &icnt, &offset_expr, tempreg);
	      if (p != NULL)
		p += 4;
	      macro_build (p, &icnt, &offset_expr, s, fmt, treg,
			   (int) BFD_RELOC_LO16, tempreg);
	    }
	  else
	    {
	      if ((valueT) offset_expr.X_add_number > MAX_GPREL_OFFSET
		  || nopic_need_relax (offset_expr.X_add_symbol, 1))
		p = NULL;
	      else
		{
		  frag_grow (28);
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			       "d,v,t", tempreg, breg, mips_gp_register);
		  macro_build ((char *) NULL, &icnt, &offset_expr, s, fmt,
			       treg, (int) BFD_RELOC_GPREL16, tempreg);
		  p = frag_var (rs_machine_dependent, 12, 0,
				RELAX_ENCODE (8, 12, 0, 8, 0, 0),
				offset_expr.X_add_symbol, 0, NULL);
		}
	      macro_build_lui (p, &icnt, &offset_expr, tempreg);
	      if (p != NULL)
		p += 4;
	      macro_build (p, &icnt, (expressionS *) NULL,
			   HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			   "d,v,t", tempreg, tempreg, breg);
	      if (p != NULL)
		p += 4;
	      macro_build (p, &icnt, &offset_expr, s, fmt, treg,
			   (int) BFD_RELOC_LO16, tempreg);
	    }
	}
      else if (mips_pic == SVR4_PIC && ! mips_big_got)
	{
	  char *p;
	  int lw_reloc_type = (int) BFD_RELOC_MIPS_GOT16;

	  /* If this is a reference to an external symbol, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	       nop
	       <op>	$treg,0($tempreg)
	     Otherwise we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	       nop
	       addiu	$tempreg,$tempreg,<sym>	(BFD_RELOC_LO16)
	       <op>	$treg,0($tempreg)
	     If we have NewABI, we want
	       lw	$reg,<sym>($gp)		(BFD_RELOC_MIPS_GOT_DISP)
	     If there is a base register, we add it to $tempreg before
	     the <op>.  If there is a constant, we stick it in the
	     <op> instruction.  We don't handle constants larger than
	     16 bits, because we have no way to load the upper 16 bits
	     (actually, we could handle them for the subset of cases
	     in which we are not using $at).  */
	  assert (offset_expr.X_op == O_symbol);
	  expr1.X_add_number = offset_expr.X_add_number;
	  offset_expr.X_add_number = 0;
	  if (HAVE_NEWABI)
	    lw_reloc_type = (int) BFD_RELOC_MIPS_GOT_DISP;
	  if (expr1.X_add_number < -0x8000
	      || expr1.X_add_number >= 0x8000)
	    as_bad (_("PIC code offset overflow (max 16 signed bits)"));
	  frag_grow (20);
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld", "t,o(b)", tempreg,
		       (int) lw_reloc_type, mips_gp_register);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "");
	  p = frag_var (rs_machine_dependent, 4, 0,
			RELAX_ENCODE (0, 4, -8, 0, 0, 0),
			offset_expr.X_add_symbol, 0, NULL);
	  macro_build (p, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
		       "t,r,j", tempreg, tempreg, (int) BFD_RELOC_LO16);
	  if (breg != 0)
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			 HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			 "d,v,t", tempreg, tempreg, breg);
	  macro_build ((char *) NULL, &icnt, &expr1, s, fmt, treg,
		       (int) BFD_RELOC_LO16, tempreg);
	}
      else if (mips_pic == SVR4_PIC)
	{
	  int gpdel;
	  char *p;

	  /* If this is a reference to an external symbol, we want
	       lui	$tempreg,<sym>		(BFD_RELOC_MIPS_GOT_HI16)
	       addu	$tempreg,$tempreg,$gp
	       lw	$tempreg,<sym>($tempreg) (BFD_RELOC_MIPS_GOT_LO16)
	       <op>	$treg,0($tempreg)
	     Otherwise we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT16)
	       nop
	       addiu	$tempreg,$tempreg,<sym>	(BFD_RELOC_LO16)
	       <op>	$treg,0($tempreg)
	     If there is a base register, we add it to $tempreg before
	     the <op>.  If there is a constant, we stick it in the
	     <op> instruction.  We don't handle constants larger than
	     16 bits, because we have no way to load the upper 16 bits
	     (actually, we could handle them for the subset of cases
	     in which we are not using $at).

	     For NewABI, we want
	       lw	$tempreg,<sym>($gp)	(BFD_RELOC_MIPS_GOT_PAGE)
	       addiu	$tempreg,$tempreg,<sym>	(BFD_RELOC_MIPS_GOT_OFST)
	       <op>	$treg,0($tempreg)
	   */
	  assert (offset_expr.X_op == O_symbol);
	  expr1.X_add_number = offset_expr.X_add_number;
	  offset_expr.X_add_number = 0;
	  if (expr1.X_add_number < -0x8000
	      || expr1.X_add_number >= 0x8000)
	    as_bad (_("PIC code offset overflow (max 16 signed bits)"));
	  if (HAVE_NEWABI)
	    {
	      macro_build ((char *) NULL, &icnt, &offset_expr,
			   HAVE_32BIT_ADDRESSES ? "lw" : "ld",
			   "t,o(b)", tempreg, BFD_RELOC_MIPS_GOT_PAGE,
			   mips_gp_register);
	      macro_build ((char *) NULL, &icnt, &offset_expr,
			   HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
			   "t,r,j", tempreg, tempreg,
			   BFD_RELOC_MIPS_GOT_OFST);
	      if (breg != 0)
		macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			     HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			     "d,v,t", tempreg, tempreg, breg);
	      macro_build ((char *) NULL, &icnt, &expr1, s, fmt, treg,
			   (int) BFD_RELOC_LO16, tempreg);

	      if (! used_at)
		return;

	      break;
	    }
	  if (reg_needs_delay (mips_gp_register))
	    gpdel = 4;
	  else
	    gpdel = 0;
	  frag_grow (36);
	  macro_build ((char *) NULL, &icnt, &offset_expr, "lui", "t,u",
		       tempreg, (int) BFD_RELOC_MIPS_GOT_HI16);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
		       "d,v,t", tempreg, tempreg, mips_gp_register);
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
		       "t,o(b)", tempreg, (int) BFD_RELOC_MIPS_GOT_LO16,
		       tempreg);
	  p = frag_var (rs_machine_dependent, 12 + gpdel, 0,
			RELAX_ENCODE (12, 12 + gpdel, gpdel, 8 + gpdel, 0, 0),
			offset_expr.X_add_symbol, 0, NULL);
	  if (gpdel > 0)
	    {
	      macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
	      p += 4;
	    }
	  macro_build (p, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
		       "t,o(b)", tempreg, (int) BFD_RELOC_MIPS_GOT16,
		       mips_gp_register);
	  p += 4;
	  macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
	  p += 4;
	  macro_build (p, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu",
		       "t,r,j", tempreg, tempreg, (int) BFD_RELOC_LO16);
	  if (breg != 0)
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			 HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			 "d,v,t", tempreg, tempreg, breg);
	  macro_build ((char *) NULL, &icnt, &expr1, s, fmt, treg,
		       (int) BFD_RELOC_LO16, tempreg);
	}
      else if (mips_pic == EMBEDDED_PIC)
	{
	  /* If there is no base register, we want
	       <op>	$treg,<sym>($gp)	(BFD_RELOC_GPREL16)
	     If there is a base register, we want
	       addu	$tempreg,$breg,$gp
	       <op>	$treg,<sym>($tempreg)	(BFD_RELOC_GPREL16)
	     */
	  assert (offset_expr.X_op == O_symbol);
	  if (breg == 0)
	    {
	      macro_build ((char *) NULL, &icnt, &offset_expr, s, fmt,
			   treg, (int) BFD_RELOC_GPREL16, mips_gp_register);
	      used_at = 0;
	    }
	  else
	    {
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			   "d,v,t", tempreg, breg, mips_gp_register);
	      macro_build ((char *) NULL, &icnt, &offset_expr, s, fmt,
			   treg, (int) BFD_RELOC_GPREL16, tempreg);
	    }
	}
      else
	abort ();

      if (! used_at)
	return;

      break;

    case M_LI:
    case M_LI_S:
      load_register (&icnt, treg, &imm_expr, 0);
      return;

    case M_DLI:
      load_register (&icnt, treg, &imm_expr, 1);
      return;

    case M_LI_SS:
      if (imm_expr.X_op == O_constant)
	{
	  load_register (&icnt, AT, &imm_expr, 0);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		       "mtc1", "t,G", AT, treg);
	  break;
	}
      else
	{
	  assert (offset_expr.X_op == O_symbol
		  && strcmp (segment_name (S_GET_SEGMENT
					   (offset_expr.X_add_symbol)),
			     ".lit4") == 0
		  && offset_expr.X_add_number == 0);
	  macro_build ((char *) NULL, &icnt, &offset_expr, "lwc1", "T,o(b)",
		       treg, (int) BFD_RELOC_MIPS_LITERAL, mips_gp_register);
	  return;
	}

    case M_LI_D:
      /* Check if we have a constant in IMM_EXPR.  If the GPRs are 64 bits
         wide, IMM_EXPR is the entire value.  Otherwise IMM_EXPR is the high
         order 32 bits of the value and the low order 32 bits are either
         zero or in OFFSET_EXPR.  */
      if (imm_expr.X_op == O_constant || imm_expr.X_op == O_big)
	{
	  if (HAVE_64BIT_GPRS)
	    load_register (&icnt, treg, &imm_expr, 1);
	  else
	    {
	      int hreg, lreg;

	      if (target_big_endian)
		{
		  hreg = treg;
		  lreg = treg + 1;
		}
	      else
		{
		  hreg = treg + 1;
		  lreg = treg;
		}

	      if (hreg <= 31)
		load_register (&icnt, hreg, &imm_expr, 0);
	      if (lreg <= 31)
		{
		  if (offset_expr.X_op == O_absent)
		    move_register (&icnt, lreg, 0);
		  else
		    {
		      assert (offset_expr.X_op == O_constant);
		      load_register (&icnt, lreg, &offset_expr, 0);
		    }
		}
	    }
	  return;
	}

      /* We know that sym is in the .rdata section.  First we get the
	 upper 16 bits of the address.  */
      if (mips_pic == NO_PIC)
	{
	  macro_build_lui (NULL, &icnt, &offset_expr, AT);
	}
      else if (mips_pic == SVR4_PIC)
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
		       "t,o(b)", AT, (int) BFD_RELOC_MIPS_GOT16,
		       mips_gp_register);
	}
      else if (mips_pic == EMBEDDED_PIC)
	{
	  /* For embedded PIC we pick up the entire address off $gp in
	     a single instruction.  */
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "addiu" : "daddiu", "t,r,j", AT,
		       mips_gp_register, (int) BFD_RELOC_GPREL16);
	  offset_expr.X_op = O_constant;
	  offset_expr.X_add_number = 0;
	}
      else
	abort ();

      /* Now we load the register(s).  */
      if (HAVE_64BIT_GPRS)
	macro_build ((char *) NULL, &icnt, &offset_expr, "ld", "t,o(b)",
		     treg, (int) BFD_RELOC_LO16, AT);
      else
	{
	  macro_build ((char *) NULL, &icnt, &offset_expr, "lw", "t,o(b)",
		       treg, (int) BFD_RELOC_LO16, AT);
	  if (treg != RA)
	    {
	      /* FIXME: How in the world do we deal with the possible
		 overflow here?  */
	      offset_expr.X_add_number += 4;
	      macro_build ((char *) NULL, &icnt, &offset_expr, "lw", "t,o(b)",
			   treg + 1, (int) BFD_RELOC_LO16, AT);
	    }
	}

      /* To avoid confusion in tc_gen_reloc, we must ensure that this
	 does not become a variant frag.  */
      frag_wane (frag_now);
      frag_new (0);

      break;

    case M_LI_DD:
      /* Check if we have a constant in IMM_EXPR.  If the FPRs are 64 bits
         wide, IMM_EXPR is the entire value and the GPRs are known to be 64
         bits wide as well.  Otherwise IMM_EXPR is the high order 32 bits of
         the value and the low order 32 bits are either zero or in
         OFFSET_EXPR.  */
      if (imm_expr.X_op == O_constant || imm_expr.X_op == O_big)
	{
	  load_register (&icnt, AT, &imm_expr, HAVE_64BIT_FPRS);
	  if (HAVE_64BIT_FPRS)
	    {
	      assert (HAVE_64BIT_GPRS);
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   "dmtc1", "t,S", AT, treg);
	    }
	  else
	    {
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   "mtc1", "t,G", AT, treg + 1);
	      if (offset_expr.X_op == O_absent)
		macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			     "mtc1", "t,G", 0, treg);
	      else
		{
		  assert (offset_expr.X_op == O_constant);
		  load_register (&icnt, AT, &offset_expr, 0);
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       "mtc1", "t,G", AT, treg);
		}
	    }
	  break;
	}

      assert (offset_expr.X_op == O_symbol
	      && offset_expr.X_add_number == 0);
      s = segment_name (S_GET_SEGMENT (offset_expr.X_add_symbol));
      if (strcmp (s, ".lit8") == 0)
	{
	  if (mips_opts.isa != ISA_MIPS1)
	    {
	      macro_build ((char *) NULL, &icnt, &offset_expr, "ldc1",
			   "T,o(b)", treg, (int) BFD_RELOC_MIPS_LITERAL,
			   mips_gp_register);
	      return;
	    }
	  breg = mips_gp_register;
	  r = BFD_RELOC_MIPS_LITERAL;
	  goto dob;
	}
      else
	{
	  assert (strcmp (s, RDATA_SECTION_NAME) == 0);
	  if (mips_pic == SVR4_PIC)
	    macro_build ((char *) NULL, &icnt, &offset_expr,
			 HAVE_32BIT_ADDRESSES ? "lw" : "ld",
			 "t,o(b)", AT, (int) BFD_RELOC_MIPS_GOT16,
			 mips_gp_register);
	  else
	    {
	      /* FIXME: This won't work for a 64 bit address.  */
	      macro_build_lui (NULL, &icnt, &offset_expr, AT);
	    }

	  if (mips_opts.isa != ISA_MIPS1)
	    {
	      macro_build ((char *) NULL, &icnt, &offset_expr, "ldc1",
			   "T,o(b)", treg, (int) BFD_RELOC_LO16, AT);

	      /* To avoid confusion in tc_gen_reloc, we must ensure
		 that this does not become a variant frag.  */
	      frag_wane (frag_now);
	      frag_new (0);

	      break;
	    }
	  breg = AT;
	  r = BFD_RELOC_LO16;
	  goto dob;
	}

    case M_L_DOB:
      if (mips_arch == CPU_R4650)
	{
	  as_bad (_("opcode not supported on this processor"));
	  return;
	}
      /* Even on a big endian machine $fn comes before $fn+1.  We have
	 to adjust when loading from memory.  */
      r = BFD_RELOC_LO16;
    dob:
      assert (mips_opts.isa == ISA_MIPS1);
      macro_build ((char *) NULL, &icnt, &offset_expr, "lwc1", "T,o(b)",
		   target_big_endian ? treg + 1 : treg,
		   (int) r, breg);
      /* FIXME: A possible overflow which I don't know how to deal
	 with.  */
      offset_expr.X_add_number += 4;
      macro_build ((char *) NULL, &icnt, &offset_expr, "lwc1", "T,o(b)",
		   target_big_endian ? treg : treg + 1,
		   (int) r, breg);

      /* To avoid confusion in tc_gen_reloc, we must ensure that this
	 does not become a variant frag.  */
      frag_wane (frag_now);
      frag_new (0);

      if (breg != AT)
	return;
      break;

    case M_L_DAB:
      /*
       * The MIPS assembler seems to check for X_add_number not
       * being double aligned and generating:
       *	lui	at,%hi(foo+1)
       *	addu	at,at,v1
       *	addiu	at,at,%lo(foo+1)
       *	lwc1	f2,0(at)
       *	lwc1	f3,4(at)
       * But, the resulting address is the same after relocation so why
       * generate the extra instruction?
       */
      if (mips_arch == CPU_R4650)
	{
	  as_bad (_("opcode not supported on this processor"));
	  return;
	}
      /* Itbl support may require additional care here.  */
      coproc = 1;
      if (mips_opts.isa != ISA_MIPS1)
	{
	  s = "ldc1";
	  goto ld;
	}

      s = "lwc1";
      fmt = "T,o(b)";
      goto ldd_std;

    case M_S_DAB:
      if (mips_arch == CPU_R4650)
	{
	  as_bad (_("opcode not supported on this processor"));
	  return;
	}

      if (mips_opts.isa != ISA_MIPS1)
	{
	  s = "sdc1";
	  goto st;
	}

      s = "swc1";
      fmt = "T,o(b)";
      /* Itbl support may require additional care here.  */
      coproc = 1;
      goto ldd_std;

    case M_LD_AB:
      if (HAVE_64BIT_GPRS)
	{
	  s = "ld";
	  goto ld;
	}

      s = "lw";
      fmt = "t,o(b)";
      goto ldd_std;

    case M_SD_AB:
      if (HAVE_64BIT_GPRS)
	{
	  s = "sd";
	  goto st;
	}

      s = "sw";
      fmt = "t,o(b)";

    ldd_std:
      /* We do _not_ bother to allow embedded PIC (symbol-local_symbol)
	 loads for the case of doing a pair of loads to simulate an 'ld'.
	 This is not currently done by the compiler, and assembly coders
	 writing embedded-pic code can cope.  */

      if (offset_expr.X_op != O_symbol
	  && offset_expr.X_op != O_constant)
	{
	  as_bad (_("expression too complex"));
	  offset_expr.X_op = O_constant;
	}

      /* Even on a big endian machine $fn comes before $fn+1.  We have
	 to adjust when loading from memory.  We set coproc if we must
	 load $fn+1 first.  */
      /* Itbl support may require additional care here.  */
      if (! target_big_endian)
	coproc = 0;

      if (mips_pic == NO_PIC
	  || offset_expr.X_op == O_constant)
	{
	  char *p;

	  /* If this is a reference to a GP relative symbol, we want
	       <op>	$treg,<sym>($gp)	(BFD_RELOC_GPREL16)
	       <op>	$treg+1,<sym>+4($gp)	(BFD_RELOC_GPREL16)
	     If we have a base register, we use this
	       addu	$at,$breg,$gp
	       <op>	$treg,<sym>($at)	(BFD_RELOC_GPREL16)
	       <op>	$treg+1,<sym>+4($at)	(BFD_RELOC_GPREL16)
	     If this is not a GP relative symbol, we want
	       lui	$at,<sym>		(BFD_RELOC_HI16_S)
	       <op>	$treg,<sym>($at)	(BFD_RELOC_LO16)
	       <op>	$treg+1,<sym>+4($at)	(BFD_RELOC_LO16)
	     If there is a base register, we add it to $at after the
	     lui instruction.  If there is a constant, we always use
	     the last case.  */
	  if ((valueT) offset_expr.X_add_number > MAX_GPREL_OFFSET
	      || nopic_need_relax (offset_expr.X_add_symbol, 1))
	    {
	      p = NULL;
	      used_at = 1;
	    }
	  else
	    {
	      int off;

	      if (breg == 0)
		{
		  frag_grow (28);
		  tempreg = mips_gp_register;
		  off = 0;
		  used_at = 0;
		}
	      else
		{
		  frag_grow (36);
		  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			       "d,v,t", AT, breg, mips_gp_register);
		  tempreg = AT;
		  off = 4;
		  used_at = 1;
		}

	      /* Itbl support may require additional care here.  */
	      macro_build ((char *) NULL, &icnt, &offset_expr, s, fmt,
			   coproc ? treg + 1 : treg,
			   (int) BFD_RELOC_GPREL16, tempreg);
	      offset_expr.X_add_number += 4;

	      /* Set mips_optimize to 2 to avoid inserting an
                 undesired nop.  */
	      hold_mips_optimize = mips_optimize;
	      mips_optimize = 2;
	      /* Itbl support may require additional care here.  */
	      macro_build ((char *) NULL, &icnt, &offset_expr, s, fmt,
			   coproc ? treg : treg + 1,
			   (int) BFD_RELOC_GPREL16, tempreg);
	      mips_optimize = hold_mips_optimize;

	      p = frag_var (rs_machine_dependent, 12 + off, 0,
			    RELAX_ENCODE (8 + off, 12 + off, 0, 4 + off, 1,
					  used_at && mips_opts.noat),
			    offset_expr.X_add_symbol, 0, NULL);

	      /* We just generated two relocs.  When tc_gen_reloc
		 handles this case, it will skip the first reloc and
		 handle the second.  The second reloc already has an
		 extra addend of 4, which we added above.  We must
		 subtract it out, and then subtract another 4 to make
		 the first reloc come out right.  The second reloc
		 will come out right because we are going to add 4 to
		 offset_expr when we build its instruction below.

		 If we have a symbol, then we don't want to include
		 the offset, because it will wind up being included
		 when we generate the reloc.  */

	      if (offset_expr.X_op == O_constant)
		offset_expr.X_add_number -= 8;
	      else
		{
		  offset_expr.X_add_number = -4;
		  offset_expr.X_op = O_constant;
		}
	    }
	  macro_build_lui (p, &icnt, &offset_expr, AT);
	  if (p != NULL)
	    p += 4;
	  if (breg != 0)
	    {
	      macro_build (p, &icnt, (expressionS *) NULL,
			   HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			   "d,v,t", AT, breg, AT);
	      if (p != NULL)
		p += 4;
	    }
	  /* Itbl support may require additional care here.  */
	  macro_build (p, &icnt, &offset_expr, s, fmt,
		       coproc ? treg + 1 : treg,
		       (int) BFD_RELOC_LO16, AT);
	  if (p != NULL)
	    p += 4;
	  /* FIXME: How do we handle overflow here?  */
	  offset_expr.X_add_number += 4;
	  /* Itbl support may require additional care here.  */
	  macro_build (p, &icnt, &offset_expr, s, fmt,
		       coproc ? treg : treg + 1,
		       (int) BFD_RELOC_LO16, AT);
	}
      else if (mips_pic == SVR4_PIC && ! mips_big_got)
	{
	  int off;

	  /* If this is a reference to an external symbol, we want
	       lw	$at,<sym>($gp)		(BFD_RELOC_MIPS_GOT16)
	       nop
	       <op>	$treg,0($at)
	       <op>	$treg+1,4($at)
	     Otherwise we want
	       lw	$at,<sym>($gp)		(BFD_RELOC_MIPS_GOT16)
	       nop
	       <op>	$treg,<sym>($at)	(BFD_RELOC_LO16)
	       <op>	$treg+1,<sym>+4($at)	(BFD_RELOC_LO16)
	     If there is a base register we add it to $at before the
	     lwc1 instructions.  If there is a constant we include it
	     in the lwc1 instructions.  */
	  used_at = 1;
	  expr1.X_add_number = offset_expr.X_add_number;
	  offset_expr.X_add_number = 0;
	  if (expr1.X_add_number < -0x8000
	      || expr1.X_add_number >= 0x8000 - 4)
	    as_bad (_("PIC code offset overflow (max 16 signed bits)"));
	  if (breg == 0)
	    off = 0;
	  else
	    off = 4;
	  frag_grow (24 + off);
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld", "t,o(b)", AT,
		       (int) BFD_RELOC_MIPS_GOT16, mips_gp_register);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "");
	  if (breg != 0)
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			 HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			 "d,v,t", AT, breg, AT);
	  /* Itbl support may require additional care here.  */
	  macro_build ((char *) NULL, &icnt, &expr1, s, fmt,
		       coproc ? treg + 1 : treg,
		       (int) BFD_RELOC_LO16, AT);
	  expr1.X_add_number += 4;

	  /* Set mips_optimize to 2 to avoid inserting an undesired
             nop.  */
	  hold_mips_optimize = mips_optimize;
	  mips_optimize = 2;
	  /* Itbl support may require additional care here.  */
	  macro_build ((char *) NULL, &icnt, &expr1, s, fmt,
		       coproc ? treg : treg + 1,
		       (int) BFD_RELOC_LO16, AT);
	  mips_optimize = hold_mips_optimize;

	  (void) frag_var (rs_machine_dependent, 0, 0,
			   RELAX_ENCODE (0, 0, -16 - off, -8, 1, 0),
			   offset_expr.X_add_symbol, 0, NULL);
	}
      else if (mips_pic == SVR4_PIC)
	{
	  int gpdel, off;
	  char *p;

	  /* If this is a reference to an external symbol, we want
	       lui	$at,<sym>		(BFD_RELOC_MIPS_GOT_HI16)
	       addu	$at,$at,$gp
	       lw	$at,<sym>($at)		(BFD_RELOC_MIPS_GOT_LO16)
	       nop
	       <op>	$treg,0($at)
	       <op>	$treg+1,4($at)
	     Otherwise we want
	       lw	$at,<sym>($gp)		(BFD_RELOC_MIPS_GOT16)
	       nop
	       <op>	$treg,<sym>($at)	(BFD_RELOC_LO16)
	       <op>	$treg+1,<sym>+4($at)	(BFD_RELOC_LO16)
	     If there is a base register we add it to $at before the
	     lwc1 instructions.  If there is a constant we include it
	     in the lwc1 instructions.  */
	  used_at = 1;
	  expr1.X_add_number = offset_expr.X_add_number;
	  offset_expr.X_add_number = 0;
	  if (expr1.X_add_number < -0x8000
	      || expr1.X_add_number >= 0x8000 - 4)
	    as_bad (_("PIC code offset overflow (max 16 signed bits)"));
	  if (reg_needs_delay (mips_gp_register))
	    gpdel = 4;
	  else
	    gpdel = 0;
	  if (breg == 0)
	    off = 0;
	  else
	    off = 4;
	  frag_grow (56);
	  macro_build ((char *) NULL, &icnt, &offset_expr, "lui", "t,u",
		       AT, (int) BFD_RELOC_MIPS_GOT_HI16);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
		       "d,v,t", AT, AT, mips_gp_register);
	  macro_build ((char *) NULL, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
		       "t,o(b)", AT, (int) BFD_RELOC_MIPS_GOT_LO16, AT);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "");
	  if (breg != 0)
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			 HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			 "d,v,t", AT, breg, AT);
	  /* Itbl support may require additional care here.  */
	  macro_build ((char *) NULL, &icnt, &expr1, s, fmt,
		       coproc ? treg + 1 : treg,
		       (int) BFD_RELOC_LO16, AT);
	  expr1.X_add_number += 4;

	  /* Set mips_optimize to 2 to avoid inserting an undesired
             nop.  */
	  hold_mips_optimize = mips_optimize;
	  mips_optimize = 2;
	  /* Itbl support may require additional care here.  */
	  macro_build ((char *) NULL, &icnt, &expr1, s, fmt,
		       coproc ? treg : treg + 1,
		       (int) BFD_RELOC_LO16, AT);
	  mips_optimize = hold_mips_optimize;
	  expr1.X_add_number -= 4;

	  p = frag_var (rs_machine_dependent, 16 + gpdel + off, 0,
			RELAX_ENCODE (24 + off, 16 + gpdel + off, gpdel,
				      8 + gpdel + off, 1, 0),
			offset_expr.X_add_symbol, 0, NULL);
	  if (gpdel > 0)
	    {
	      macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
	      p += 4;
	    }
	  macro_build (p, &icnt, &offset_expr,
		       HAVE_32BIT_ADDRESSES ? "lw" : "ld",
		       "t,o(b)", AT, (int) BFD_RELOC_MIPS_GOT16,
		       mips_gp_register);
	  p += 4;
	  macro_build (p, &icnt, (expressionS *) NULL, "nop", "");
	  p += 4;
	  if (breg != 0)
	    {
	      macro_build (p, &icnt, (expressionS *) NULL,
			   HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			   "d,v,t", AT, breg, AT);
	      p += 4;
	    }
	  /* Itbl support may require additional care here.  */
	  macro_build (p, &icnt, &expr1, s, fmt,
		       coproc ? treg + 1 : treg,
		       (int) BFD_RELOC_LO16, AT);
	  p += 4;
	  expr1.X_add_number += 4;

	  /* Set mips_optimize to 2 to avoid inserting an undesired
             nop.  */
	  hold_mips_optimize = mips_optimize;
	  mips_optimize = 2;
	  /* Itbl support may require additional care here.  */
	  macro_build (p, &icnt, &expr1, s, fmt,
		       coproc ? treg : treg + 1,
		       (int) BFD_RELOC_LO16, AT);
	  mips_optimize = hold_mips_optimize;
	}
      else if (mips_pic == EMBEDDED_PIC)
	{
	  /* If there is no base register, we use
	       <op>	$treg,<sym>($gp)	(BFD_RELOC_GPREL16)
	       <op>	$treg+1,<sym>+4($gp)	(BFD_RELOC_GPREL16)
	     If we have a base register, we use
	       addu	$at,$breg,$gp
	       <op>	$treg,<sym>($at)	(BFD_RELOC_GPREL16)
	       <op>	$treg+1,<sym>+4($at)	(BFD_RELOC_GPREL16)
	     */
	  if (breg == 0)
	    {
	      tempreg = mips_gp_register;
	      used_at = 0;
	    }
	  else
	    {
	      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
			   HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
			   "d,v,t", AT, breg, mips_gp_register);
	      tempreg = AT;
	      used_at = 1;
	    }

	  /* Itbl support may require additional care here.  */
	  macro_build ((char *) NULL, &icnt, &offset_expr, s, fmt,
		       coproc ? treg + 1 : treg,
		       (int) BFD_RELOC_GPREL16, tempreg);
	  offset_expr.X_add_number += 4;
	  /* Itbl support may require additional care here.  */
	  macro_build ((char *) NULL, &icnt, &offset_expr, s, fmt,
		       coproc ? treg : treg + 1,
		       (int) BFD_RELOC_GPREL16, tempreg);
	}
      else
	abort ();

      if (! used_at)
	return;

      break;

    case M_LD_OB:
      s = "lw";
      goto sd_ob;
    case M_SD_OB:
      s = "sw";
    sd_ob:
      assert (HAVE_32BIT_ADDRESSES);
      macro_build ((char *) NULL, &icnt, &offset_expr, s, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, breg);
      offset_expr.X_add_number += 4;
      macro_build ((char *) NULL, &icnt, &offset_expr, s, "t,o(b)", treg + 1,
		   (int) BFD_RELOC_LO16, breg);
      return;

   /* New code added to support COPZ instructions.
      This code builds table entries out of the macros in mip_opcodes.
      R4000 uses interlocks to handle coproc delays.
      Other chips (like the R3000) require nops to be inserted for delays.

      FIXME: Currently, we require that the user handle delays.
      In order to fill delay slots for non-interlocked chips,
      we must have a way to specify delays based on the coprocessor.
      Eg. 4 cycles if load coproc reg from memory, 1 if in cache, etc.
      What are the side-effects of the cop instruction?
      What cache support might we have and what are its effects?
      Both coprocessor & memory require delays. how long???
      What registers are read/set/modified?

      If an itbl is provided to interpret cop instructions,
      this knowledge can be encoded in the itbl spec.  */

    case M_COP0:
      s = "c0";
      goto copz;
    case M_COP1:
      s = "c1";
      goto copz;
    case M_COP2:
      s = "c2";
      goto copz;
    case M_COP3:
      s = "c3";
    copz:
      /* For now we just do C (same as Cz).  The parameter will be
         stored in insn_opcode by mips_ip.  */
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "C",
		   ip->insn_opcode);
      return;

    case M_MOVE:
      move_register (&icnt, dreg, sreg);
      return;

#ifdef LOSING_COMPILER
    default:
      /* Try and see if this is a new itbl instruction.
         This code builds table entries out of the macros in mip_opcodes.
         FIXME: For now we just assemble the expression and pass it's
         value along as a 32-bit immediate.
         We may want to have the assembler assemble this value,
         so that we gain the assembler's knowledge of delay slots,
         symbols, etc.
         Would it be more efficient to use mask (id) here? */
      if (itbl_have_entries
	  && (immed_expr = itbl_assemble (ip->insn_mo->name, "")))
	{
	  s = ip->insn_mo->name;
	  s2 = "cop3";
	  coproc = ITBL_DECODE_PNUM (immed_expr);;
	  macro_build ((char *) NULL, &icnt, &immed_expr, s, "C");
	  return;
	}
      macro2 (ip);
      return;
    }
  if (mips_opts.noat)
    as_warn (_("Macro used $at after \".set noat\""));
}

static void
macro2 (ip)
     struct mips_cl_insn *ip;
{
  register int treg, sreg, dreg, breg;
  int tempreg;
  int mask;
  int icnt = 0;
  int used_at;
  expressionS expr1;
  const char *s;
  const char *s2;
  const char *fmt;
  int likely = 0;
  int dbl = 0;
  int coproc = 0;
  int lr = 0;
  int imm = 0;
  int off;
  offsetT maxnum;
  bfd_reloc_code_real_type r;
  char *p;

  treg = (ip->insn_opcode >> 16) & 0x1f;
  dreg = (ip->insn_opcode >> 11) & 0x1f;
  sreg = breg = (ip->insn_opcode >> 21) & 0x1f;
  mask = ip->insn_mo->mask;

  expr1.X_op = O_constant;
  expr1.X_op_symbol = NULL;
  expr1.X_add_symbol = NULL;
  expr1.X_add_number = 1;

  switch (mask)
    {
#endif /* LOSING_COMPILER */

    case M_DMUL:
      dbl = 1;
    case M_MUL:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "dmultu" : "multu", "s,t", sreg, treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "mflo", "d",
		   dreg);
      return;

    case M_DMUL_I:
      dbl = 1;
    case M_MUL_I:
      /* The MIPS assembler some times generates shifts and adds.  I'm
	 not trying to be that fancy. GCC should do this for us
	 anyway.  */
      load_register (&icnt, AT, &imm_expr, dbl);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "dmult" : "mult", "s,t", sreg, AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "mflo", "d",
		   dreg);
      break;

    case M_DMULO_I:
      dbl = 1;
    case M_MULO_I:
      imm = 1;
      goto do_mulo;

    case M_DMULO:
      dbl = 1;
    case M_MULO:
    do_mulo:
      mips_emit_delays (true);
      ++mips_opts.noreorder;
      mips_any_noreorder = 1;
      if (imm)
	load_register (&icnt, AT, &imm_expr, dbl);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "dmult" : "mult", "s,t", sreg, imm ? AT : treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "mflo", "d",
		   dreg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "dsra32" : "sra", "d,w,<", dreg, dreg, RA);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "mfhi", "d",
		   AT);
      if (mips_trap)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "tne",
		     "s,t,q", dreg, AT, 6);
      else
	{
	  expr1.X_add_number = 8;
	  macro_build ((char *) NULL, &icnt, &expr1, "beq", "s,t,p", dreg,
		       AT);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "",
		       0);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "break",
		       "c", 6);
	}
      --mips_opts.noreorder;
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "mflo", "d", dreg);
      break;

    case M_DMULOU_I:
      dbl = 1;
    case M_MULOU_I:
      imm = 1;
      goto do_mulou;

    case M_DMULOU:
      dbl = 1;
    case M_MULOU:
    do_mulou:
      mips_emit_delays (true);
      ++mips_opts.noreorder;
      mips_any_noreorder = 1;
      if (imm)
	load_register (&icnt, AT, &imm_expr, dbl);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "dmultu" : "multu",
		   "s,t", sreg, imm ? AT : treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "mfhi", "d",
		   AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "mflo", "d",
		   dreg);
      if (mips_trap)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "tne",
		     "s,t,q", AT, 0, 6);
      else
	{
	  expr1.X_add_number = 8;
	  macro_build ((char *) NULL, &icnt, &expr1, "beq", "s,t,p", AT, 0);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "",
		       0);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "break",
		       "c", 6);
	}
      --mips_opts.noreorder;
      break;

    case M_DROL:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "dsubu",
		   "d,v,t", AT, 0, treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "dsrlv",
		   "d,t,s", AT, sreg, AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "dsllv",
		   "d,t,s", dreg, sreg, treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or",
		   "d,v,t", dreg, dreg, AT);
      break;

    case M_ROL:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "subu",
		   "d,v,t", AT, 0, treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "srlv",
		   "d,t,s", AT, sreg, AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sllv",
		   "d,t,s", dreg, sreg, treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or",
		   "d,v,t", dreg, dreg, AT);
      break;

    case M_DROL_I:
      {
	unsigned int rot;

	if (imm_expr.X_op != O_constant)
	  as_bad (_("rotate count too large"));
	rot = imm_expr.X_add_number & 0x3f;
	if (CPU_HAS_DROR (mips_arch))
	  {
	    rot = (64 - rot) & 0x3f;
	    if (rot >= 32)
	      macro_build ((char *) NULL, &icnt, NULL, "dror32",
			   "d,w,<", dreg, sreg, rot - 32);
	    else
	      macro_build ((char *) NULL, &icnt, NULL, "dror",
			   "d,w,<", dreg, sreg, rot);
	    break;
	  }
	if (rot == 0)
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "dsrl",
		       "d,w,<", dreg, sreg, 0);
	else
	  {
	    char *l, *r;

	    l = (rot < 0x20) ? "dsll" : "dsll32";
	    r = ((0x40 - rot) < 0x20) ? "dsrl" : "dsrl32";
	    rot &= 0x1f;
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, l,
			 "d,w,<", AT, sreg, rot);
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, r,
			 "d,w,<", dreg, sreg, (0x20 - rot) & 0x1f);
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or",
			 "d,v,t", dreg, dreg, AT);
	  }
      }
      break;

    case M_ROL_I:
      {
	unsigned int rot;

	if (imm_expr.X_op != O_constant)
	  as_bad (_("rotate count too large"));
	rot = imm_expr.X_add_number & 0x1f;
	if (CPU_HAS_ROR (mips_arch))
	  {
	    macro_build ((char *) NULL, &icnt, NULL, "ror",
			 "d,w,<", dreg, sreg, (32 - rot) & 0x1f);
	    break;
	  }
	if (rot == 0)
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "srl",
		       "d,w,<", dreg, sreg, 0);
	else
	  {
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sll",
			 "d,w,<", AT, sreg, rot);
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "srl",
			 "d,w,<", dreg, sreg, (0x20 - rot) & 0x1f);
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or",
			 "d,v,t", dreg, dreg, AT);
	  }
      }
      break;

    case M_DROR:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "dsubu",
		   "d,v,t", AT, 0, treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "dsllv",
		   "d,t,s", AT, sreg, AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "dsrlv",
		   "d,t,s", dreg, sreg, treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or",
		   "d,v,t", dreg, dreg, AT);
      break;

    case M_ROR:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "subu",
		   "d,v,t", AT, 0, treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sllv",
		   "d,t,s", AT, sreg, AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "srlv",
		   "d,t,s", dreg, sreg, treg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or",
		   "d,v,t", dreg, dreg, AT);
      break;

    case M_DROR_I:
      {
	unsigned int rot;

	if (imm_expr.X_op != O_constant)
	  as_bad (_("rotate count too large"));
	rot = imm_expr.X_add_number & 0x3f;
	if (rot == 0)
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "dsrl",
		       "d,w,<", dreg, sreg, 0);
	else
	  {
	    char *l, *r;

	    r = (rot < 0x20) ? "dsrl" : "dsrl32";
	    l = ((0x40 - rot) < 0x20) ? "dsll" : "dsll32";
	    rot &= 0x1f;
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, r,
			 "d,w,<", AT, sreg, rot);
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, l,
			 "d,w,<", dreg, sreg, (0x20 - rot) & 0x1f);
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or",
			 "d,v,t", dreg, dreg, AT);
	  }
      }
      break;

    case M_ROR_I:
      {
	unsigned int rot;

	if (imm_expr.X_op != O_constant)
	  as_bad (_("rotate count too large"));
	rot = imm_expr.X_add_number & 0x1f;
	if (rot == 0)
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "srl",
		       "d,w,<", dreg, sreg, 0);
	else
	  {
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "srl",
			 "d,w,<", AT, sreg, rot);
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sll",
			 "d,w,<", dreg, sreg, (0x20 - rot) & 0x1f);
	    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or",
			 "d,v,t", dreg, dreg, AT);
	  }
      }
      break;

    case M_S_DOB:
      if (mips_arch == CPU_R4650)
	{
	  as_bad (_("opcode not supported on this processor"));
	  return;
	}
      assert (mips_opts.isa == ISA_MIPS1);
      /* Even on a big endian machine $fn comes before $fn+1.  We have
	 to adjust when storing to memory.  */
      macro_build ((char *) NULL, &icnt, &offset_expr, "swc1", "T,o(b)",
		   target_big_endian ? treg + 1 : treg,
		   (int) BFD_RELOC_LO16, breg);
      offset_expr.X_add_number += 4;
      macro_build ((char *) NULL, &icnt, &offset_expr, "swc1", "T,o(b)",
		   target_big_endian ? treg : treg + 1,
		   (int) BFD_RELOC_LO16, breg);
      return;

    case M_SEQ:
      if (sreg == 0)
	macro_build ((char *) NULL, &icnt, &expr1, "sltiu", "t,r,j", dreg,
		     treg, (int) BFD_RELOC_LO16);
      else if (treg == 0)
	macro_build ((char *) NULL, &icnt, &expr1, "sltiu", "t,r,j", dreg,
		     sreg, (int) BFD_RELOC_LO16);
      else
	{
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "xor",
		       "d,v,t", dreg, sreg, treg);
	  macro_build ((char *) NULL, &icnt, &expr1, "sltiu", "t,r,j", dreg,
		       dreg, (int) BFD_RELOC_LO16);
	}
      return;

    case M_SEQ_I:
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 0)
	{
	  macro_build ((char *) NULL, &icnt, &expr1, "sltiu", "t,r,j", dreg,
		       sreg, (int) BFD_RELOC_LO16);
	  return;
	}
      if (sreg == 0)
	{
	  as_warn (_("Instruction %s: result is always false"),
		   ip->insn_mo->name);
	  move_register (&icnt, dreg, 0);
	  return;
	}
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number >= 0
	  && imm_expr.X_add_number < 0x10000)
	{
	  macro_build ((char *) NULL, &icnt, &imm_expr, "xori", "t,r,i", dreg,
		       sreg, (int) BFD_RELOC_LO16);
	  used_at = 0;
	}
      else if (imm_expr.X_op == O_constant
	       && imm_expr.X_add_number > -0x8000
	       && imm_expr.X_add_number < 0)
	{
	  imm_expr.X_add_number = -imm_expr.X_add_number;
	  macro_build ((char *) NULL, &icnt, &imm_expr,
		       HAVE_32BIT_GPRS ? "addiu" : "daddiu",
		       "t,r,j", dreg, sreg,
		       (int) BFD_RELOC_LO16);
	  used_at = 0;
	}
      else
	{
	  load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "xor",
		       "d,v,t", dreg, sreg, AT);
	  used_at = 1;
	}
      macro_build ((char *) NULL, &icnt, &expr1, "sltiu", "t,r,j", dreg, dreg,
		   (int) BFD_RELOC_LO16);
      if (used_at)
	break;
      return;

    case M_SGE:		/* sreg >= treg <==> not (sreg < treg) */
      s = "slt";
      goto sge;
    case M_SGEU:
      s = "sltu";
    sge:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "d,v,t",
		   dreg, sreg, treg);
      macro_build ((char *) NULL, &icnt, &expr1, "xori", "t,r,i", dreg, dreg,
		   (int) BFD_RELOC_LO16);
      return;

    case M_SGE_I:		/* sreg >= I <==> not (sreg < I) */
    case M_SGEU_I:
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number >= -0x8000
	  && imm_expr.X_add_number < 0x8000)
	{
	  macro_build ((char *) NULL, &icnt, &imm_expr,
		       mask == M_SGE_I ? "slti" : "sltiu",
		       "t,r,j", dreg, sreg, (int) BFD_RELOC_LO16);
	  used_at = 0;
	}
      else
	{
	  load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		       mask == M_SGE_I ? "slt" : "sltu", "d,v,t", dreg, sreg,
		       AT);
	  used_at = 1;
	}
      macro_build ((char *) NULL, &icnt, &expr1, "xori", "t,r,i", dreg, dreg,
		   (int) BFD_RELOC_LO16);
      if (used_at)
	break;
      return;

    case M_SGT:		/* sreg > treg  <==>  treg < sreg */
      s = "slt";
      goto sgt;
    case M_SGTU:
      s = "sltu";
    sgt:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "d,v,t",
		   dreg, treg, sreg);
      return;

    case M_SGT_I:		/* sreg > I  <==>  I < sreg */
      s = "slt";
      goto sgti;
    case M_SGTU_I:
      s = "sltu";
    sgti:
      load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "d,v,t",
		   dreg, AT, sreg);
      break;

    case M_SLE:	/* sreg <= treg  <==>  treg >= sreg  <==>  not (treg < sreg) */
      s = "slt";
      goto sle;
    case M_SLEU:
      s = "sltu";
    sle:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "d,v,t",
		   dreg, treg, sreg);
      macro_build ((char *) NULL, &icnt, &expr1, "xori", "t,r,i", dreg, dreg,
		   (int) BFD_RELOC_LO16);
      return;

    case M_SLE_I:	/* sreg <= I <==> I >= sreg <==> not (I < sreg) */
      s = "slt";
      goto slei;
    case M_SLEU_I:
      s = "sltu";
    slei:
      load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "d,v,t",
		   dreg, AT, sreg);
      macro_build ((char *) NULL, &icnt, &expr1, "xori", "t,r,i", dreg, dreg,
		   (int) BFD_RELOC_LO16);
      break;

    case M_SLT_I:
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number >= -0x8000
	  && imm_expr.X_add_number < 0x8000)
	{
	  macro_build ((char *) NULL, &icnt, &imm_expr, "slti", "t,r,j",
		       dreg, sreg, (int) BFD_RELOC_LO16);
	  return;
	}
      load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "slt", "d,v,t",
		   dreg, sreg, AT);
      break;

    case M_SLTU_I:
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number >= -0x8000
	  && imm_expr.X_add_number < 0x8000)
	{
	  macro_build ((char *) NULL, &icnt, &imm_expr, "sltiu", "t,r,j",
		       dreg, sreg, (int) BFD_RELOC_LO16);
	  return;
	}
      load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		   "d,v,t", dreg, sreg, AT);
      break;

    case M_SNE:
      if (sreg == 0)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		     "d,v,t", dreg, 0, treg);
      else if (treg == 0)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		     "d,v,t", dreg, 0, sreg);
      else
	{
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "xor",
		       "d,v,t", dreg, sreg, treg);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		       "d,v,t", dreg, 0, dreg);
	}
      return;

    case M_SNE_I:
      if (imm_expr.X_op == O_constant && imm_expr.X_add_number == 0)
	{
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		       "d,v,t", dreg, 0, sreg);
	  return;
	}
      if (sreg == 0)
	{
	  as_warn (_("Instruction %s: result is always true"),
		   ip->insn_mo->name);
	  macro_build ((char *) NULL, &icnt, &expr1,
		       HAVE_32BIT_GPRS ? "addiu" : "daddiu",
		       "t,r,j", dreg, 0, (int) BFD_RELOC_LO16);
	  return;
	}
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number >= 0
	  && imm_expr.X_add_number < 0x10000)
	{
	  macro_build ((char *) NULL, &icnt, &imm_expr, "xori", "t,r,i",
		       dreg, sreg, (int) BFD_RELOC_LO16);
	  used_at = 0;
	}
      else if (imm_expr.X_op == O_constant
	       && imm_expr.X_add_number > -0x8000
	       && imm_expr.X_add_number < 0)
	{
	  imm_expr.X_add_number = -imm_expr.X_add_number;
	  macro_build ((char *) NULL, &icnt, &imm_expr,
		       HAVE_32BIT_GPRS ? "addiu" : "daddiu",
		       "t,r,j", dreg, sreg, (int) BFD_RELOC_LO16);
	  used_at = 0;
	}
      else
	{
	  load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
	  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "xor",
		       "d,v,t", dreg, sreg, AT);
	  used_at = 1;
	}
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sltu",
		   "d,v,t", dreg, 0, dreg);
      if (used_at)
	break;
      return;

    case M_DSUB_I:
      dbl = 1;
    case M_SUB_I:
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number > -0x8000
	  && imm_expr.X_add_number <= 0x8000)
	{
	  imm_expr.X_add_number = -imm_expr.X_add_number;
	  macro_build ((char *) NULL, &icnt, &imm_expr,
		       dbl ? "daddi" : "addi",
		       "t,r,j", dreg, sreg, (int) BFD_RELOC_LO16);
	  return;
	}
      load_register (&icnt, AT, &imm_expr, dbl);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "dsub" : "sub", "d,v,t", dreg, sreg, AT);
      break;

    case M_DSUBU_I:
      dbl = 1;
    case M_SUBU_I:
      if (imm_expr.X_op == O_constant
	  && imm_expr.X_add_number > -0x8000
	  && imm_expr.X_add_number <= 0x8000)
	{
	  imm_expr.X_add_number = -imm_expr.X_add_number;
	  macro_build ((char *) NULL, &icnt, &imm_expr,
		       dbl ? "daddiu" : "addiu",
		       "t,r,j", dreg, sreg, (int) BFD_RELOC_LO16);
	  return;
	}
      load_register (&icnt, AT, &imm_expr, dbl);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "dsubu" : "subu", "d,v,t", dreg, sreg, AT);
      break;

    case M_TEQ_I:
      s = "teq";
      goto trap;
    case M_TGE_I:
      s = "tge";
      goto trap;
    case M_TGEU_I:
      s = "tgeu";
      goto trap;
    case M_TLT_I:
      s = "tlt";
      goto trap;
    case M_TLTU_I:
      s = "tltu";
      goto trap;
    case M_TNE_I:
      s = "tne";
    trap:
      load_register (&icnt, AT, &imm_expr, HAVE_64BIT_GPRS);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "s,t", sreg,
		   AT);
      break;

    case M_TRUNCWS:
    case M_TRUNCWD:
      assert (mips_opts.isa == ISA_MIPS1);
      sreg = (ip->insn_opcode >> 11) & 0x1f;	/* floating reg */
      dreg = (ip->insn_opcode >> 06) & 0x1f;	/* floating reg */

      /*
       * Is the double cfc1 instruction a bug in the mips assembler;
       * or is there a reason for it?
       */
      mips_emit_delays (true);
      ++mips_opts.noreorder;
      mips_any_noreorder = 1;
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "cfc1", "t,G",
		   treg, RA);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "cfc1", "t,G",
		   treg, RA);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "");
      expr1.X_add_number = 3;
      macro_build ((char *) NULL, &icnt, &expr1, "ori", "t,r,i", AT, treg,
		   (int) BFD_RELOC_LO16);
      expr1.X_add_number = 2;
      macro_build ((char *) NULL, &icnt, &expr1, "xori", "t,r,i", AT, AT,
		     (int) BFD_RELOC_LO16);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "ctc1", "t,G",
		   AT, RA);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "");
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
	      mask == M_TRUNCWD ? "cvt.w.d" : "cvt.w.s", "D,S", dreg, sreg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "ctc1", "t,G",
		   treg, RA);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "nop", "");
      --mips_opts.noreorder;
      break;

    case M_ULH:
      s = "lb";
      goto ulh;
    case M_ULHU:
      s = "lbu";
    ulh:
      if (offset_expr.X_add_number >= 0x7fff)
	as_bad (_("operand overflow"));
      /* avoid load delay */
      if (! target_big_endian)
	++offset_expr.X_add_number;
      macro_build ((char *) NULL, &icnt, &offset_expr, s, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, breg);
      if (! target_big_endian)
	--offset_expr.X_add_number;
      else
	++offset_expr.X_add_number;
      macro_build ((char *) NULL, &icnt, &offset_expr, "lbu", "t,o(b)", AT,
		   (int) BFD_RELOC_LO16, breg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sll", "d,w,<",
		   treg, treg, 8);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or", "d,v,t",
		   treg, treg, AT);
      break;

    case M_ULD:
      s = "ldl";
      s2 = "ldr";
      off = 7;
      goto ulw;
    case M_ULW:
      s = "lwl";
      s2 = "lwr";
      off = 3;
    ulw:
      if (offset_expr.X_add_number >= 0x8000 - off)
	as_bad (_("operand overflow"));
      if (! target_big_endian)
	offset_expr.X_add_number += off;
      macro_build ((char *) NULL, &icnt, &offset_expr, s, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, breg);
      if (! target_big_endian)
	offset_expr.X_add_number -= off;
      else
	offset_expr.X_add_number += off;
      macro_build ((char *) NULL, &icnt, &offset_expr, s2, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, breg);
      return;

    case M_ULD_A:
      s = "ldl";
      s2 = "ldr";
      off = 7;
      goto ulwa;
    case M_ULW_A:
      s = "lwl";
      s2 = "lwr";
      off = 3;
    ulwa:
      used_at = 1;
      load_address (&icnt, AT, &offset_expr, &used_at);
      if (breg != 0)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		     HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
		     "d,v,t", AT, AT, breg);
      if (! target_big_endian)
	expr1.X_add_number = off;
      else
	expr1.X_add_number = 0;
      macro_build ((char *) NULL, &icnt, &expr1, s, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, AT);
      if (! target_big_endian)
	expr1.X_add_number = 0;
      else
	expr1.X_add_number = off;
      macro_build ((char *) NULL, &icnt, &expr1, s2, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, AT);
      break;

    case M_ULH_A:
    case M_ULHU_A:
      used_at = 1;
      load_address (&icnt, AT, &offset_expr, &used_at);
      if (breg != 0)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		     HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
		     "d,v,t", AT, AT, breg);
      if (target_big_endian)
	expr1.X_add_number = 0;
      macro_build ((char *) NULL, &icnt, &expr1,
		   mask == M_ULH_A ? "lb" : "lbu", "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, AT);
      if (target_big_endian)
	expr1.X_add_number = 1;
      else
	expr1.X_add_number = 0;
      macro_build ((char *) NULL, &icnt, &expr1, "lbu", "t,o(b)", AT,
		   (int) BFD_RELOC_LO16, AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sll", "d,w,<",
		   treg, treg, 8);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or", "d,v,t",
		   treg, treg, AT);
      break;

    case M_USH:
      if (offset_expr.X_add_number >= 0x7fff)
	as_bad (_("operand overflow"));
      if (target_big_endian)
	++offset_expr.X_add_number;
      macro_build ((char *) NULL, &icnt, &offset_expr, "sb", "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, breg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "srl", "d,w,<",
		   AT, treg, 8);
      if (target_big_endian)
	--offset_expr.X_add_number;
      else
	++offset_expr.X_add_number;
      macro_build ((char *) NULL, &icnt, &offset_expr, "sb", "t,o(b)", AT,
		   (int) BFD_RELOC_LO16, breg);
      break;

    case M_USD:
      s = "sdl";
      s2 = "sdr";
      off = 7;
      goto usw;
    case M_USW:
      s = "swl";
      s2 = "swr";
      off = 3;
    usw:
      if (offset_expr.X_add_number >= 0x8000 - off)
	as_bad (_("operand overflow"));
      if (! target_big_endian)
	offset_expr.X_add_number += off;
      macro_build ((char *) NULL, &icnt, &offset_expr, s, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, breg);
      if (! target_big_endian)
	offset_expr.X_add_number -= off;
      else
	offset_expr.X_add_number += off;
      macro_build ((char *) NULL, &icnt, &offset_expr, s2, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, breg);
      return;

    case M_USD_A:
      s = "sdl";
      s2 = "sdr";
      off = 7;
      goto uswa;
    case M_USW_A:
      s = "swl";
      s2 = "swr";
      off = 3;
    uswa:
      used_at = 1;
      load_address (&icnt, AT, &offset_expr, &used_at);
      if (breg != 0)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		     HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
		     "d,v,t", AT, AT, breg);
      if (! target_big_endian)
	expr1.X_add_number = off;
      else
	expr1.X_add_number = 0;
      macro_build ((char *) NULL, &icnt, &expr1, s, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, AT);
      if (! target_big_endian)
	expr1.X_add_number = 0;
      else
	expr1.X_add_number = off;
      macro_build ((char *) NULL, &icnt, &expr1, s2, "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, AT);
      break;

    case M_USH_A:
      used_at = 1;
      load_address (&icnt, AT, &offset_expr, &used_at);
      if (breg != 0)
	macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		     HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
		     "d,v,t", AT, AT, breg);
      if (! target_big_endian)
	expr1.X_add_number = 0;
      macro_build ((char *) NULL, &icnt, &expr1, "sb", "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "srl", "d,w,<",
		   treg, treg, 8);
      if (! target_big_endian)
	expr1.X_add_number = 1;
      else
	expr1.X_add_number = 0;
      macro_build ((char *) NULL, &icnt, &expr1, "sb", "t,o(b)", treg,
		   (int) BFD_RELOC_LO16, AT);
      if (! target_big_endian)
	expr1.X_add_number = 0;
      else
	expr1.X_add_number = 1;
      macro_build ((char *) NULL, &icnt, &expr1, "lbu", "t,o(b)", AT,
		   (int) BFD_RELOC_LO16, AT);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "sll", "d,w,<",
		   treg, treg, 8);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "or", "d,v,t",
		   treg, treg, AT);
      break;

    default:
      /* FIXME: Check if this is one of the itbl macros, since they
	 are added dynamically.  */
      as_bad (_("Macro %s not implemented yet"), ip->insn_mo->name);
      break;
    }
  if (mips_opts.noat)
    as_warn (_("Macro used $at after \".set noat\""));
}

/* Implement macros in mips16 mode.  */

static void
mips16_macro (ip)
     struct mips_cl_insn *ip;
{
  int mask;
  int xreg, yreg, zreg, tmp;
  int icnt;
  expressionS expr1;
  int dbl;
  const char *s, *s2, *s3;

  mask = ip->insn_mo->mask;

  xreg = (ip->insn_opcode >> MIPS16OP_SH_RX) & MIPS16OP_MASK_RX;
  yreg = (ip->insn_opcode >> MIPS16OP_SH_RY) & MIPS16OP_MASK_RY;
  zreg = (ip->insn_opcode >> MIPS16OP_SH_RZ) & MIPS16OP_MASK_RZ;

  icnt = 0;

  expr1.X_op = O_constant;
  expr1.X_op_symbol = NULL;
  expr1.X_add_symbol = NULL;
  expr1.X_add_number = 1;

  dbl = 0;

  switch (mask)
    {
    default:
      internalError ();

    case M_DDIV_3:
      dbl = 1;
    case M_DIV_3:
      s = "mflo";
      goto do_div3;
    case M_DREM_3:
      dbl = 1;
    case M_REM_3:
      s = "mfhi";
    do_div3:
      mips_emit_delays (true);
      ++mips_opts.noreorder;
      mips_any_noreorder = 1;
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "ddiv" : "div",
		   "0,x,y", xreg, yreg);
      expr1.X_add_number = 2;
      macro_build ((char *) NULL, &icnt, &expr1, "bnez", "x,p", yreg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "break", "6",
		   7);

      /* FIXME: The normal code checks for of -1 / -0x80000000 here,
         since that causes an overflow.  We should do that as well,
         but I don't see how to do the comparisons without a temporary
         register.  */
      --mips_opts.noreorder;
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "x", zreg);
      break;

    case M_DIVU_3:
      s = "divu";
      s2 = "mflo";
      goto do_divu3;
    case M_REMU_3:
      s = "divu";
      s2 = "mfhi";
      goto do_divu3;
    case M_DDIVU_3:
      s = "ddivu";
      s2 = "mflo";
      goto do_divu3;
    case M_DREMU_3:
      s = "ddivu";
      s2 = "mfhi";
    do_divu3:
      mips_emit_delays (true);
      ++mips_opts.noreorder;
      mips_any_noreorder = 1;
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "0,x,y",
		   xreg, yreg);
      expr1.X_add_number = 2;
      macro_build ((char *) NULL, &icnt, &expr1, "bnez", "x,p", yreg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "break",
		   "6", 7);
      --mips_opts.noreorder;
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s2, "x", zreg);
      break;

    case M_DMUL:
      dbl = 1;
    case M_MUL:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   dbl ? "dmultu" : "multu", "x,y", xreg, yreg);
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "mflo", "x",
		   zreg);
      return;

    case M_DSUBU_I:
      dbl = 1;
      goto do_subu;
    case M_SUBU_I:
    do_subu:
      if (imm_expr.X_op != O_constant)
	as_bad (_("Unsupported large constant"));
      imm_expr.X_add_number = -imm_expr.X_add_number;
      macro_build ((char *) NULL, &icnt, &imm_expr,
		   dbl ? "daddiu" : "addiu", "y,x,4", yreg, xreg);
      break;

    case M_SUBU_I_2:
      if (imm_expr.X_op != O_constant)
	as_bad (_("Unsupported large constant"));
      imm_expr.X_add_number = -imm_expr.X_add_number;
      macro_build ((char *) NULL, &icnt, &imm_expr, "addiu",
		   "x,k", xreg);
      break;

    case M_DSUBU_I_2:
      if (imm_expr.X_op != O_constant)
	as_bad (_("Unsupported large constant"));
      imm_expr.X_add_number = -imm_expr.X_add_number;
      macro_build ((char *) NULL, &icnt, &imm_expr, "daddiu",
		   "y,j", yreg);
      break;

    case M_BEQ:
      s = "cmp";
      s2 = "bteqz";
      goto do_branch;
    case M_BNE:
      s = "cmp";
      s2 = "btnez";
      goto do_branch;
    case M_BLT:
      s = "slt";
      s2 = "btnez";
      goto do_branch;
    case M_BLTU:
      s = "sltu";
      s2 = "btnez";
      goto do_branch;
    case M_BLE:
      s = "slt";
      s2 = "bteqz";
      goto do_reverse_branch;
    case M_BLEU:
      s = "sltu";
      s2 = "bteqz";
      goto do_reverse_branch;
    case M_BGE:
      s = "slt";
      s2 = "bteqz";
      goto do_branch;
    case M_BGEU:
      s = "sltu";
      s2 = "bteqz";
      goto do_branch;
    case M_BGT:
      s = "slt";
      s2 = "btnez";
      goto do_reverse_branch;
    case M_BGTU:
      s = "sltu";
      s2 = "btnez";

    do_reverse_branch:
      tmp = xreg;
      xreg = yreg;
      yreg = tmp;

    do_branch:
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL, s, "x,y",
		   xreg, yreg);
      macro_build ((char *) NULL, &icnt, &offset_expr, s2, "p");
      break;

    case M_BEQ_I:
      s = "cmpi";
      s2 = "bteqz";
      s3 = "x,U";
      goto do_branch_i;
    case M_BNE_I:
      s = "cmpi";
      s2 = "btnez";
      s3 = "x,U";
      goto do_branch_i;
    case M_BLT_I:
      s = "slti";
      s2 = "btnez";
      s3 = "x,8";
      goto do_branch_i;
    case M_BLTU_I:
      s = "sltiu";
      s2 = "btnez";
      s3 = "x,8";
      goto do_branch_i;
    case M_BLE_I:
      s = "slti";
      s2 = "btnez";
      s3 = "x,8";
      goto do_addone_branch_i;
    case M_BLEU_I:
      s = "sltiu";
      s2 = "btnez";
      s3 = "x,8";
      goto do_addone_branch_i;
    case M_BGE_I:
      s = "slti";
      s2 = "bteqz";
      s3 = "x,8";
      goto do_branch_i;
    case M_BGEU_I:
      s = "sltiu";
      s2 = "bteqz";
      s3 = "x,8";
      goto do_branch_i;
    case M_BGT_I:
      s = "slti";
      s2 = "bteqz";
      s3 = "x,8";
      goto do_addone_branch_i;
    case M_BGTU_I:
      s = "sltiu";
      s2 = "bteqz";
      s3 = "x,8";

    do_addone_branch_i:
      if (imm_expr.X_op != O_constant)
	as_bad (_("Unsupported large constant"));
      ++imm_expr.X_add_number;

    do_branch_i:
      macro_build ((char *) NULL, &icnt, &imm_expr, s, s3, xreg);
      macro_build ((char *) NULL, &icnt, &offset_expr, s2, "p");
      break;

    case M_ABS:
      expr1.X_add_number = 0;
      macro_build ((char *) NULL, &icnt, &expr1, "slti", "x,8", yreg);
      if (xreg != yreg)
	move_register (&icnt, xreg, yreg);
      expr1.X_add_number = 2;
      macro_build ((char *) NULL, &icnt, &expr1, "bteqz", "p");
      macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
		   "neg", "x,w", xreg, xreg);
    }
}

/* For consistency checking, verify that all bits are specified either
   by the match/mask part of the instruction definition, or by the
   operand list.  */
static int
validate_mips_insn (opc)
     const struct mips_opcode *opc;
{
  const char *p = opc->args;
  char c;
  unsigned long used_bits = opc->mask;

  if ((used_bits & opc->match) != opc->match)
    {
      as_bad (_("internal: bad mips opcode (mask error): %s %s"),
	      opc->name, opc->args);
      return 0;
    }
#define USE_BITS(mask,shift)	(used_bits |= ((mask) << (shift)))
  while (*p)
    switch (c = *p++)
      {
      case ',': break;
      case '(': break;
      case ')': break;
      case '<': USE_BITS (OP_MASK_SHAMT,	OP_SH_SHAMT);	break;
      case '>':	USE_BITS (OP_MASK_SHAMT,	OP_SH_SHAMT);	break;
      case 'A': break;
      case 'B': USE_BITS (OP_MASK_CODE20,       OP_SH_CODE20);  break;
      case 'C':	USE_BITS (OP_MASK_COPZ,		OP_SH_COPZ);	break;
      case 'D':	USE_BITS (OP_MASK_FD,		OP_SH_FD);	break;
      case 'E':	USE_BITS (OP_MASK_RT,		OP_SH_RT);	break;
      case 'F': break;
      case 'G':	USE_BITS (OP_MASK_RD,		OP_SH_RD);	break;
      case 'H': USE_BITS (OP_MASK_SEL,		OP_SH_SEL);	break;
      case 'I': break;
      case 'J': USE_BITS (OP_MASK_CODE19,       OP_SH_CODE19);  break;
      case 'L': break;
      case 'M':	USE_BITS (OP_MASK_CCC,		OP_SH_CCC);	break;
      case 'N':	USE_BITS (OP_MASK_BCC,		OP_SH_BCC);	break;
      case 'O':	USE_BITS (OP_MASK_ALN,		OP_SH_ALN);	break;
      case 'Q':	USE_BITS (OP_MASK_VSEL,		OP_SH_VSEL);
		USE_BITS (OP_MASK_FT,		OP_SH_FT);	break;
      case 'R':	USE_BITS (OP_MASK_FR,		OP_SH_FR);	break;
      case 'S':	USE_BITS (OP_MASK_FS,		OP_SH_FS);	break;
      case 'T':	USE_BITS (OP_MASK_FT,		OP_SH_FT);	break;
      case 'V':	USE_BITS (OP_MASK_FS,		OP_SH_FS);	break;
      case 'W':	USE_BITS (OP_MASK_FT,		OP_SH_FT);	break;
      case 'X':	USE_BITS (OP_MASK_FD,		OP_SH_FD);	break;
      case 'Y':	USE_BITS (OP_MASK_FS,		OP_SH_FS);	break;
      case 'Z':	USE_BITS (OP_MASK_FT,		OP_SH_FT);	break;
      case 'a':	USE_BITS (OP_MASK_TARGET,	OP_SH_TARGET);	break;
      case 'b':	USE_BITS (OP_MASK_RS,		OP_SH_RS);	break;
      case 'c':	USE_BITS (OP_MASK_CODE,		OP_SH_CODE);	break;
      case 'd':	USE_BITS (OP_MASK_RD,		OP_SH_RD);	break;
      case 'f': break;
      case 'h':	USE_BITS (OP_MASK_PREFX,	OP_SH_PREFX);	break;
      case 'i':	USE_BITS (OP_MASK_IMMEDIATE,	OP_SH_IMMEDIATE); break;
      case 'j':	USE_BITS (OP_MASK_DELTA,	OP_SH_DELTA);	break;
      case 'k':	USE_BITS (OP_MASK_CACHE,	OP_SH_CACHE);	break;
      case 'l': break;
      case 'o': USE_BITS (OP_MASK_DELTA,	OP_SH_DELTA);	break;
      case 'p':	USE_BITS (OP_MASK_DELTA,	OP_SH_DELTA);	break;
      case 'q':	USE_BITS (OP_MASK_CODE2,	OP_SH_CODE2);	break;
      case 'r': USE_BITS (OP_MASK_RS,		OP_SH_RS);	break;
      case 's':	USE_BITS (OP_MASK_RS,		OP_SH_RS);	break;
      case 't':	USE_BITS (OP_MASK_RT,		OP_SH_RT);	break;
      case 'u':	USE_BITS (OP_MASK_IMMEDIATE,	OP_SH_IMMEDIATE); break;
      case 'v':	USE_BITS (OP_MASK_RS,		OP_SH_RS);	break;
      case 'w':	USE_BITS (OP_MASK_RT,		OP_SH_RT);	break;
      case 'x': break;
      case 'z': break;
      case 'P': USE_BITS (OP_MASK_PERFREG,	OP_SH_PERFREG);	break;
      case 'U': USE_BITS (OP_MASK_RD,           OP_SH_RD);
	        USE_BITS (OP_MASK_RT,           OP_SH_RT);	break;
      case 'e': USE_BITS (OP_MASK_VECBYTE,	OP_SH_VECBYTE);	break;
      case '%': USE_BITS (OP_MASK_VECALIGN,	OP_SH_VECALIGN); break;
      case '[': break;
      case ']': break;
      default:
	as_bad (_("internal: bad mips opcode (unknown operand type `%c'): %s %s"),
		c, opc->name, opc->args);
	return 0;
      }
#undef USE_BITS
  if (used_bits != 0xffffffff)
    {
      as_bad (_("internal: bad mips opcode (bits 0x%lx undefined): %s %s"),
	      ~used_bits & 0xffffffff, opc->name, opc->args);
      return 0;
    }
  return 1;
}

/* This routine assembles an instruction into its binary format.  As a
   side effect, it sets one of the global variables imm_reloc or
   offset_reloc to the type of relocation to do if one of the operands
   is an address expression.  */

static void
mips_ip (str, ip)
     char *str;
     struct mips_cl_insn *ip;
{
  char *s;
  const char *args;
  char c = 0;
  struct mips_opcode *insn;
  char *argsStart;
  unsigned int regno;
  unsigned int lastregno = 0;
  char *s_reset;
  char save_c = 0;

  insn_error = NULL;

  /* If the instruction contains a '.', we first try to match an instruction
     including the '.'.  Then we try again without the '.'.  */
  insn = NULL;
  for (s = str; *s != '\0' && !ISSPACE (*s); ++s)
    continue;

  /* If we stopped on whitespace, then replace the whitespace with null for
     the call to hash_find.  Save the character we replaced just in case we
     have to re-parse the instruction.  */
  if (ISSPACE (*s))
    {
      save_c = *s;
      *s++ = '\0';
    }

  insn = (struct mips_opcode *) hash_find (op_hash, str);

  /* If we didn't find the instruction in the opcode table, try again, but
     this time with just the instruction up to, but not including the
     first '.'.  */
  if (insn == NULL)
    {
      /* Restore the character we overwrite above (if any).  */
      if (save_c)
	*(--s) = save_c;

      /* Scan up to the first '.' or whitespace.  */
      for (s = str;
	   *s != '\0' && *s != '.' && !ISSPACE (*s);
	   ++s)
	continue;

      /* If we did not find a '.', then we can quit now.  */
      if (*s != '.')
	{
	  insn_error = "unrecognized opcode";
	  return;
	}

      /* Lookup the instruction in the hash table.  */
      *s++ = '\0';
      if ((insn = (struct mips_opcode *) hash_find (op_hash, str)) == NULL)
	{
	  insn_error = "unrecognized opcode";
	  return;
	}
    }

  argsStart = s;
  for (;;)
    {
      boolean ok;

      assert (strcmp (insn->name, str) == 0);

      if (OPCODE_IS_MEMBER (insn,
			    (mips_opts.isa
			     | (file_ase_mips16 ? INSN_MIPS16 : 0)
	      		     | (mips_opts.ase_mdmx ? INSN_MDMX : 0)
			     | (mips_opts.ase_mips3d ? INSN_MIPS3D : 0)),
			    mips_arch))
	ok = true;
      else
	ok = false;

      if (insn->pinfo != INSN_MACRO)
	{
	  if (mips_arch == CPU_R4650 && (insn->pinfo & FP_D) != 0)
	    ok = false;
	}

      if (! ok)
	{
	  if (insn + 1 < &mips_opcodes[NUMOPCODES]
	      && strcmp (insn->name, insn[1].name) == 0)
	    {
	      ++insn;
	      continue;
	    }
	  else
	    {
	      if (!insn_error)
		{
		  static char buf[100];
		  if (mips_arch_info->is_isa)
		    sprintf (buf,
			     _("opcode not supported at this ISA level (%s)"),
			     mips_cpu_info_from_isa (mips_opts.isa)->name);
		  else
		    sprintf (buf,
			     _("opcode not supported on this processor: %s (%s)"),
			     mips_arch_info->name,
			     mips_cpu_info_from_isa (mips_opts.isa)->name);
		  insn_error = buf;
		}
	      if (save_c)
		*(--s) = save_c;
	      return;
	    }
	}

      ip->insn_mo = insn;
      ip->insn_opcode = insn->match;
      insn_error = NULL;
      for (args = insn->args;; ++args)
	{
	  int is_mdmx;

	  s += strspn (s, " \t");
	  is_mdmx = 0;
	  switch (*args)
	    {
	    case '\0':		/* end of args */
	      if (*s == '\0')
		return;
	      break;

	    case ',':
	      if (*s++ == *args)
		continue;
	      s--;
	      switch (*++args)
		{
		case 'r':
		case 'v':
		  ip->insn_opcode |= lastregno << OP_SH_RS;
		  continue;

		case 'w':
		  ip->insn_opcode |= lastregno << OP_SH_RT;
		  continue;

		case 'W':
		  ip->insn_opcode |= lastregno << OP_SH_FT;
		  continue;

		case 'V':
		  ip->insn_opcode |= lastregno << OP_SH_FS;
		  continue;
		}
	      break;

	    case '(':
	      /* Handle optional base register.
		 Either the base register is omitted or
		 we must have a left paren.  */
	      /* This is dependent on the next operand specifier
		 is a base register specification.  */
	      assert (args[1] == 'b' || args[1] == '5'
		      || args[1] == '-' || args[1] == '4');
	      if (*s == '\0')
		return;

	    case ')':		/* these must match exactly */
	    case '[':
	    case ']':
	      if (*s++ == *args)
		continue;
	      break;

	    case '<':		/* must be at least one digit */
	      /*
	       * According to the manual, if the shift amount is greater
	       * than 31 or less than 0, then the shift amount should be
	       * mod 32.  In reality the mips assembler issues an error.
	       * We issue a warning and mask out all but the low 5 bits.
	       */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number > 31)
		{
		  as_warn (_("Improper shift amount (%lu)"),
			   (unsigned long) imm_expr.X_add_number);
		  imm_expr.X_add_number &= OP_MASK_SHAMT;
		}
	      ip->insn_opcode |= imm_expr.X_add_number << OP_SH_SHAMT;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case '>':		/* shift amount minus 32 */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number < 32
		  || (unsigned long) imm_expr.X_add_number > 63)
		break;
	      ip->insn_opcode |= (imm_expr.X_add_number - 32) << OP_SH_SHAMT;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'k':		/* cache code */
	    case 'h':		/* prefx code */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number > 31)
		{
		  as_warn (_("Invalid value for `%s' (%lu)"),
			   ip->insn_mo->name,
			   (unsigned long) imm_expr.X_add_number);
		  imm_expr.X_add_number &= 0x1f;
		}
	      if (*args == 'k')
		ip->insn_opcode |= imm_expr.X_add_number << OP_SH_CACHE;
	      else
		ip->insn_opcode |= imm_expr.X_add_number << OP_SH_PREFX;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'c':		/* break code */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number > 1023)
		{
		  as_warn (_("Illegal break code (%lu)"),
			   (unsigned long) imm_expr.X_add_number);
		  imm_expr.X_add_number &= OP_MASK_CODE;
		}
	      ip->insn_opcode |= imm_expr.X_add_number << OP_SH_CODE;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'q':		/* lower break code */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number > 1023)
		{
		  as_warn (_("Illegal lower break code (%lu)"),
			   (unsigned long) imm_expr.X_add_number);
		  imm_expr.X_add_number &= OP_MASK_CODE2;
		}
	      ip->insn_opcode |= imm_expr.X_add_number << OP_SH_CODE2;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'B':           /* 20-bit syscall/break code.  */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number > OP_MASK_CODE20)
		as_warn (_("Illegal 20-bit code (%lu)"),
			 (unsigned long) imm_expr.X_add_number);
	      ip->insn_opcode |= imm_expr.X_add_number << OP_SH_CODE20;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'C':           /* Coprocessor code */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number >= (1 << 25))
		{
		  as_warn (_("Coproccesor code > 25 bits (%lu)"),
			   (unsigned long) imm_expr.X_add_number);
		  imm_expr.X_add_number &= ((1 << 25) - 1);
		}
	      ip->insn_opcode |= imm_expr.X_add_number;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'J':           /* 19-bit wait code.  */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number > OP_MASK_CODE19)
		as_warn (_("Illegal 19-bit code (%lu)"),
			 (unsigned long) imm_expr.X_add_number);
	      ip->insn_opcode |= imm_expr.X_add_number << OP_SH_CODE19;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'P':		/* Performance register */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if (imm_expr.X_add_number != 0 && imm_expr.X_add_number != 1)
		{
		  as_warn (_("Invalid performance register (%lu)"),
			   (unsigned long) imm_expr.X_add_number);
		  imm_expr.X_add_number &= OP_MASK_PERFREG;
		}
	      ip->insn_opcode |= (imm_expr.X_add_number << OP_SH_PERFREG);
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'b':		/* base register */
	    case 'd':		/* destination register */
	    case 's':		/* source register */
	    case 't':		/* target register */
	    case 'r':		/* both target and source */
	    case 'v':		/* both dest and source */
	    case 'w':		/* both dest and target */
	    case 'E':		/* coprocessor target register */
	    case 'G':		/* coprocessor destination register */
	    case 'x':		/* ignore register name */
	    case 'z':		/* must be zero register */
	    case 'U':           /* destination register (clo/clz).  */
	      s_reset = s;
	      if (s[0] == '$')
		{

		  if (ISDIGIT (s[1]))
		    {
		      ++s;
		      regno = 0;
		      do
			{
			  regno *= 10;
			  regno += *s - '0';
			  ++s;
			}
		      while (ISDIGIT (*s));
		      if (regno > 31)
			as_bad (_("Invalid register number (%d)"), regno);
		    }
		  else if (*args == 'E' || *args == 'G')
		    goto notreg;
		  else
		    {
		      if (s[1] == 'r' && s[2] == 'a')
			{
			  s += 3;
			  regno = RA;
			}
		      else if (s[1] == 'f' && s[2] == 'p')
			{
			  s += 3;
			  regno = FP;
			}
		      else if (s[1] == 's' && s[2] == 'p')
			{
			  s += 3;
			  regno = SP;
			}
		      else if (s[1] == 'g' && s[2] == 'p')
			{
			  s += 3;
			  regno = GP;
			}
		      else if (s[1] == 'a' && s[2] == 't')
			{
			  s += 3;
			  regno = AT;
			}
		      else if (s[1] == 'k' && s[2] == 't' && s[3] == '0')
			{
			  s += 4;
			  regno = KT0;
			}
		      else if (s[1] == 'k' && s[2] == 't' && s[3] == '1')
			{
			  s += 4;
			  regno = KT1;
			}
		      else if (s[1] == 'z' && s[2] == 'e' && s[3] == 'r' && s[4] == 'o')
			{
			  s += 5;
			  regno = ZERO;
			}
		      else if (itbl_have_entries)
			{
			  char *p, *n;
			  unsigned long r;

			  p = s + 1; 	/* advance past '$' */
			  n = itbl_get_field (&p);  /* n is name */

			  /* See if this is a register defined in an
			     itbl entry.  */
			  if (itbl_get_reg_val (n, &r))
			    {
			      /* Get_field advances to the start of
				 the next field, so we need to back
				 rack to the end of the last field.  */
			      if (p)
				s = p - 1;
			      else
				s = strchr (s, '\0');
			      regno = r;
			    }
			  else
			    goto notreg;
			}
		      else
			goto notreg;
		    }
		  if (regno == AT
		      && ! mips_opts.noat
		      && *args != 'E'
		      && *args != 'G')
		    as_warn (_("Used $at without \".set noat\""));
		  c = *args;
		  if (*s == ' ')
		    ++s;
		  if (args[1] != *s)
		    {
		      if (c == 'r' || c == 'v' || c == 'w')
			{
			  regno = lastregno;
			  s = s_reset;
			  ++args;
			}
		    }
		  /* 'z' only matches $0.  */
		  if (c == 'z' && regno != 0)
		    break;

	/* Now that we have assembled one operand, we use the args string
	 * to figure out where it goes in the instruction.  */
		  switch (c)
		    {
		    case 'r':
		    case 's':
		    case 'v':
		    case 'b':
		      ip->insn_opcode |= regno << OP_SH_RS;
		      break;
		    case 'd':
		    case 'G':
		      ip->insn_opcode |= regno << OP_SH_RD;
		      break;
		    case 'U':
		      ip->insn_opcode |= regno << OP_SH_RD;
		      ip->insn_opcode |= regno << OP_SH_RT;
		      break;
		    case 'w':
		    case 't':
		    case 'E':
		      ip->insn_opcode |= regno << OP_SH_RT;
		      break;
		    case 'x':
		      /* This case exists because on the r3000 trunc
			 expands into a macro which requires a gp
			 register.  On the r6000 or r4000 it is
			 assembled into a single instruction which
			 ignores the register.  Thus the insn version
			 is MIPS_ISA2 and uses 'x', and the macro
			 version is MIPS_ISA1 and uses 't'.  */
		      break;
		    case 'z':
		      /* This case is for the div instruction, which
			 acts differently if the destination argument
			 is $0.  This only matches $0, and is checked
			 outside the switch.  */
		      break;
		    case 'D':
		      /* Itbl operand; not yet implemented. FIXME ?? */
		      break;
		      /* What about all other operands like 'i', which
			 can be specified in the opcode table? */
		    }
		  lastregno = regno;
		  continue;
		}
	    notreg:
	      switch (*args++)
		{
		case 'r':
		case 'v':
		  ip->insn_opcode |= lastregno << OP_SH_RS;
		  continue;
		case 'w':
		  ip->insn_opcode |= lastregno << OP_SH_RT;
		  continue;
		}
	      break;

	    case 'O':		/* MDMX alignment immediate constant.  */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number > OP_MASK_ALN)
		{
		  as_warn ("Improper align amount (%ld), using low bits",
			   (long) imm_expr.X_add_number);
		  imm_expr.X_add_number &= OP_MASK_ALN;
		}
	      ip->insn_opcode |= imm_expr.X_add_number << OP_SH_ALN;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'Q':		/* MDMX vector, element sel, or const.  */
	      if (s[0] != '$')
		{
		  /* MDMX Immediate.  */
		  my_getExpression (&imm_expr, s);
		  check_absolute_expr (ip, &imm_expr);
		  if ((unsigned long) imm_expr.X_add_number > OP_MASK_FT)
		    {
		      as_warn (_("Invalid MDMX Immediate (%ld)"),
			       (long) imm_expr.X_add_number);
		      imm_expr.X_add_number &= OP_MASK_FT;
		    }
		  imm_expr.X_add_number &= OP_MASK_FT;
		  if (ip->insn_opcode & (OP_MASK_VSEL << OP_SH_VSEL))
		    ip->insn_opcode |= MDMX_FMTSEL_IMM_QH << OP_SH_VSEL;
		  else
		    ip->insn_opcode |= MDMX_FMTSEL_IMM_OB << OP_SH_VSEL;
		  ip->insn_opcode |= imm_expr.X_add_number << OP_SH_FT;
		  imm_expr.X_op = O_absent;
		  s = expr_end;
		  continue;
		}
	      /* Not MDMX Immediate.  Fall through.  */
	    case 'X':           /* MDMX destination register.  */
	    case 'Y':           /* MDMX source register.  */
	    case 'Z':           /* MDMX target register.  */
	      is_mdmx = 1;
	    case 'D':		/* floating point destination register */
	    case 'S':		/* floating point source register */
	    case 'T':		/* floating point target register */
	    case 'R':		/* floating point source register */
	    case 'V':
	    case 'W':
	      s_reset = s;
	      /* Accept $fN for FP and MDMX register numbers, and in
                 addition accept $vN for MDMX register numbers.  */
	      if ((s[0] == '$' && s[1] == 'f' && ISDIGIT (s[2]))
		  || (is_mdmx != 0 && s[0] == '$' && s[1] == 'v'
		      && ISDIGIT (s[2])))
		{
		  s += 2;
		  regno = 0;
		  do
		    {
		      regno *= 10;
		      regno += *s - '0';
		      ++s;
		    }
		  while (ISDIGIT (*s));

		  if (regno > 31)
		    as_bad (_("Invalid float register number (%d)"), regno);

		  if ((regno & 1) != 0
		      && HAVE_32BIT_FPRS
		      && ! (strcmp (str, "mtc1") == 0
			    || strcmp (str, "mfc1") == 0
			    || strcmp (str, "lwc1") == 0
			    || strcmp (str, "swc1") == 0
			    || strcmp (str, "l.s") == 0
			    || strcmp (str, "s.s") == 0))
		    as_warn (_("Float register should be even, was %d"),
			     regno);

		  c = *args;
		  if (*s == ' ')
		    ++s;
		  if (args[1] != *s)
		    {
		      if (c == 'V' || c == 'W')
			{
			  regno = lastregno;
			  s = s_reset;
			  ++args;
			}
		    }
		  switch (c)
		    {
		    case 'D':
		    case 'X':
		      ip->insn_opcode |= regno << OP_SH_FD;
		      break;
		    case 'V':
		    case 'S':
		    case 'Y':
		      ip->insn_opcode |= regno << OP_SH_FS;
		      break;
		    case 'Q':
		      /* This is like 'Z', but also needs to fix the MDMX
			 vector/scalar select bits.  Note that the
			 scalar immediate case is handled above.  */
		      if (*s == '[')
			{
			  int is_qh = (ip->insn_opcode & (1 << OP_SH_VSEL));
			  int max_el = (is_qh ? 3 : 7);
			  s++;
			  my_getExpression(&imm_expr, s);
			  check_absolute_expr (ip, &imm_expr);
			  s = expr_end;
			  if (imm_expr.X_add_number > max_el)
			    as_bad(_("Bad element selector %ld"),
				   (long) imm_expr.X_add_number);
			  imm_expr.X_add_number &= max_el;
			  ip->insn_opcode |= (imm_expr.X_add_number
					      << (OP_SH_VSEL +
						  (is_qh ? 2 : 1)));
			  if (*s != ']')
			    as_warn(_("Expecting ']' found '%s'"), s);
			  else
			    s++;
			}
		      else
                        {
                          if (ip->insn_opcode & (OP_MASK_VSEL << OP_SH_VSEL))
                            ip->insn_opcode |= (MDMX_FMTSEL_VEC_QH
						<< OP_SH_VSEL);
			  else
			    ip->insn_opcode |= (MDMX_FMTSEL_VEC_OB <<
						OP_SH_VSEL);
			}
                      /* Fall through */
		    case 'W':
		    case 'T':
		    case 'Z':
		      ip->insn_opcode |= regno << OP_SH_FT;
		      break;
		    case 'R':
		      ip->insn_opcode |= regno << OP_SH_FR;
		      break;
		    }
		  lastregno = regno;
		  continue;
		}

	      switch (*args++)
		{
		case 'V':
		  ip->insn_opcode |= lastregno << OP_SH_FS;
		  continue;
		case 'W':
		  ip->insn_opcode |= lastregno << OP_SH_FT;
		  continue;
		}
	      break;

	    case 'I':
	      my_getExpression (&imm_expr, s);
	      if (imm_expr.X_op != O_big
		  && imm_expr.X_op != O_constant)
		insn_error = _("absolute expression required");
	      s = expr_end;
	      continue;

	    case 'A':
	      my_getExpression (&offset_expr, s);
	      *imm_reloc = BFD_RELOC_32;
	      s = expr_end;
	      continue;

	    case 'F':
	    case 'L':
	    case 'f':
	    case 'l':
	      {
		int f64;
		int using_gprs;
		char *save_in;
		char *err;
		unsigned char temp[8];
		int len;
		unsigned int length;
		segT seg;
		subsegT subseg;
		char *p;

		/* These only appear as the last operand in an
		   instruction, and every instruction that accepts
		   them in any variant accepts them in all variants.
		   This means we don't have to worry about backing out
		   any changes if the instruction does not match.

		   The difference between them is the size of the
		   floating point constant and where it goes.  For 'F'
		   and 'L' the constant is 64 bits; for 'f' and 'l' it
		   is 32 bits.  Where the constant is placed is based
		   on how the MIPS assembler does things:
		    F -- .rdata
		    L -- .lit8
		    f -- immediate value
		    l -- .lit4

		    The .lit4 and .lit8 sections are only used if
		    permitted by the -G argument.

		    When generating embedded PIC code, we use the
		    .lit8 section but not the .lit4 section (we can do
		    .lit4 inline easily; we need to put .lit8
		    somewhere in the data segment, and using .lit8
		    permits the linker to eventually combine identical
		    .lit8 entries).

		    The code below needs to know whether the target register
		    is 32 or 64 bits wide.  It relies on the fact 'f' and
		    'F' are used with GPR-based instructions and 'l' and
		    'L' are used with FPR-based instructions.  */

		f64 = *args == 'F' || *args == 'L';
		using_gprs = *args == 'F' || *args == 'f';

		save_in = input_line_pointer;
		input_line_pointer = s;
		err = md_atof (f64 ? 'd' : 'f', (char *) temp, &len);
		length = len;
		s = input_line_pointer;
		input_line_pointer = save_in;
		if (err != NULL && *err != '\0')
		  {
		    as_bad (_("Bad floating point constant: %s"), err);
		    memset (temp, '\0', sizeof temp);
		    length = f64 ? 8 : 4;
		  }

		assert (length == (unsigned) (f64 ? 8 : 4));

		if (*args == 'f'
		    || (*args == 'l'
			&& (! USE_GLOBAL_POINTER_OPT
			    || mips_pic == EMBEDDED_PIC
			    || g_switch_value < 4
			    || (temp[0] == 0 && temp[1] == 0)
			    || (temp[2] == 0 && temp[3] == 0))))
		  {
		    imm_expr.X_op = O_constant;
		    if (! target_big_endian)
		      imm_expr.X_add_number = bfd_getl32 (temp);
		    else
		      imm_expr.X_add_number = bfd_getb32 (temp);
		  }
		else if (length > 4
			 && ! mips_disable_float_construction
			 /* Constants can only be constructed in GPRs and
			    copied to FPRs if the GPRs are at least as wide
			    as the FPRs.  Force the constant into memory if
			    we are using 64-bit FPRs but the GPRs are only
			    32 bits wide.  */
			 && (using_gprs
			     || ! (HAVE_64BIT_FPRS && HAVE_32BIT_GPRS))
			 && ((temp[0] == 0 && temp[1] == 0)
			     || (temp[2] == 0 && temp[3] == 0))
			 && ((temp[4] == 0 && temp[5] == 0)
			     || (temp[6] == 0 && temp[7] == 0)))
		  {
		    /* The value is simple enough to load with a couple of
                       instructions.  If using 32-bit registers, set
                       imm_expr to the high order 32 bits and offset_expr to
                       the low order 32 bits.  Otherwise, set imm_expr to
                       the entire 64 bit constant.  */
		    if (using_gprs ? HAVE_32BIT_GPRS : HAVE_32BIT_FPRS)
		      {
			imm_expr.X_op = O_constant;
			offset_expr.X_op = O_constant;
			if (! target_big_endian)
			  {
			    imm_expr.X_add_number = bfd_getl32 (temp + 4);
			    offset_expr.X_add_number = bfd_getl32 (temp);
			  }
			else
			  {
			    imm_expr.X_add_number = bfd_getb32 (temp);
			    offset_expr.X_add_number = bfd_getb32 (temp + 4);
			  }
			if (offset_expr.X_add_number == 0)
			  offset_expr.X_op = O_absent;
		      }
		    else if (sizeof (imm_expr.X_add_number) > 4)
		      {
			imm_expr.X_op = O_constant;
			if (! target_big_endian)
			  imm_expr.X_add_number = bfd_getl64 (temp);
			else
			  imm_expr.X_add_number = bfd_getb64 (temp);
		      }
		    else
		      {
			imm_expr.X_op = O_big;
			imm_expr.X_add_number = 4;
			if (! target_big_endian)
			  {
			    generic_bignum[0] = bfd_getl16 (temp);
			    generic_bignum[1] = bfd_getl16 (temp + 2);
			    generic_bignum[2] = bfd_getl16 (temp + 4);
			    generic_bignum[3] = bfd_getl16 (temp + 6);
			  }
			else
			  {
			    generic_bignum[0] = bfd_getb16 (temp + 6);
			    generic_bignum[1] = bfd_getb16 (temp + 4);
			    generic_bignum[2] = bfd_getb16 (temp + 2);
			    generic_bignum[3] = bfd_getb16 (temp);
			  }
		      }
		  }
		else
		  {
		    const char *newname;
		    segT new_seg;

		    /* Switch to the right section.  */
		    seg = now_seg;
		    subseg = now_subseg;
		    switch (*args)
		      {
		      default: /* unused default case avoids warnings.  */
		      case 'L':
			newname = RDATA_SECTION_NAME;
			if ((USE_GLOBAL_POINTER_OPT && g_switch_value >= 8)
			    || mips_pic == EMBEDDED_PIC)
			  newname = ".lit8";
			break;
		      case 'F':
			if (mips_pic == EMBEDDED_PIC)
			  newname = ".lit8";
			else
			  newname = RDATA_SECTION_NAME;
			break;
		      case 'l':
			assert (!USE_GLOBAL_POINTER_OPT
				|| g_switch_value >= 4);
			newname = ".lit4";
			break;
		      }
		    new_seg = subseg_new (newname, (subsegT) 0);
		    if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
		      bfd_set_section_flags (stdoutput, new_seg,
					     (SEC_ALLOC
					      | SEC_LOAD
					      | SEC_READONLY
					      | SEC_DATA));
		    frag_align (*args == 'l' ? 2 : 3, 0, 0);
		    if (OUTPUT_FLAVOR == bfd_target_elf_flavour
			&& strcmp (TARGET_OS, "elf") != 0)
		      record_alignment (new_seg, 4);
		    else
		      record_alignment (new_seg, *args == 'l' ? 2 : 3);
		    if (seg == now_seg)
		      as_bad (_("Can't use floating point insn in this section"));

		    /* Set the argument to the current address in the
		       section.  */
		    offset_expr.X_op = O_symbol;
		    offset_expr.X_add_symbol =
		      symbol_new ("L0\001", now_seg,
				  (valueT) frag_now_fix (), frag_now);
		    offset_expr.X_add_number = 0;

		    /* Put the floating point number into the section.  */
		    p = frag_more ((int) length);
		    memcpy (p, temp, length);

		    /* Switch back to the original section.  */
		    subseg_set (seg, subseg);
		  }
	      }
	      continue;

	    case 'i':		/* 16 bit unsigned immediate */
	    case 'j':		/* 16 bit signed immediate */
	      *imm_reloc = BFD_RELOC_LO16;
	      c = my_getSmallExpression (&imm_expr, s);
	      if (c != S_EX_NONE)
		{
		  if (c != S_EX_LO)
		    {
		      if (c == S_EX_HI)
			{
			  *imm_reloc = BFD_RELOC_HI16_S;
			  imm_unmatched_hi = true;
			}
#ifdef OBJ_ELF
		      else if (c == S_EX_HIGHEST)
			*imm_reloc = BFD_RELOC_MIPS_HIGHEST;
		      else if (c == S_EX_HIGHER)
			*imm_reloc = BFD_RELOC_MIPS_HIGHER;
		      else if (c == S_EX_GP_REL)
			{
			  /* This occurs in NewABI only.  */
			  c = my_getSmallExpression (&imm_expr, s);
			  if (c != S_EX_NEG)
			    as_bad (_("bad composition of relocations"));
			  else
			    {
			      c = my_getSmallExpression (&imm_expr, s);
			      if (c != S_EX_LO)
				as_bad (_("bad composition of relocations"));
			      else
				{
				  imm_reloc[0] = BFD_RELOC_GPREL16;
				  imm_reloc[1] = BFD_RELOC_MIPS_SUB;
				  imm_reloc[2] = BFD_RELOC_LO16;
				}
			    }
			}
#endif
		      else
			*imm_reloc = BFD_RELOC_HI16;
		    }
		  else if (imm_expr.X_op == O_constant)
		    imm_expr.X_add_number &= 0xffff;
		}
	      if (*args == 'i')
		{
		  if ((c == S_EX_NONE && imm_expr.X_op != O_constant)
		      || ((imm_expr.X_add_number < 0
			   || imm_expr.X_add_number >= 0x10000)
			  && imm_expr.X_op == O_constant))
		    {
		      if (insn + 1 < &mips_opcodes[NUMOPCODES] &&
			  !strcmp (insn->name, insn[1].name))
			break;
		      if (imm_expr.X_op == O_constant
			  || imm_expr.X_op == O_big)
			as_bad (_("16 bit expression not in range 0..65535"));
		    }
		}
	      else
		{
		  int more;
		  offsetT max;

		  /* The upper bound should be 0x8000, but
		     unfortunately the MIPS assembler accepts numbers
		     from 0x8000 to 0xffff and sign extends them, and
		     we want to be compatible.  We only permit this
		     extended range for an instruction which does not
		     provide any further alternates, since those
		     alternates may handle other cases.  People should
		     use the numbers they mean, rather than relying on
		     a mysterious sign extension.  */
		  more = (insn + 1 < &mips_opcodes[NUMOPCODES] &&
			  strcmp (insn->name, insn[1].name) == 0);
		  if (more)
		    max = 0x8000;
		  else
		    max = 0x10000;
		  if ((c == S_EX_NONE && imm_expr.X_op != O_constant)
		      || ((imm_expr.X_add_number < -0x8000
			   || imm_expr.X_add_number >= max)
			  && imm_expr.X_op == O_constant)
		      || (more
			  && imm_expr.X_add_number < 0
			  && HAVE_64BIT_GPRS
			  && imm_expr.X_unsigned
			  && sizeof (imm_expr.X_add_number) <= 4))
		    {
		      if (more)
			break;
		      if (imm_expr.X_op == O_constant
			  || imm_expr.X_op == O_big)
			as_bad (_("16 bit expression not in range -32768..32767"));
		    }
		}
	      s = expr_end;
	      continue;

	    case 'o':		/* 16 bit offset */
	      c = my_getSmallExpression (&offset_expr, s);

	      /* If this value won't fit into a 16 bit offset, then go
		 find a macro that will generate the 32 bit offset
		 code pattern.  */
	      if (c == S_EX_NONE
		  && (offset_expr.X_op != O_constant
		      || offset_expr.X_add_number >= 0x8000
		      || offset_expr.X_add_number < -0x8000))
		break;

	      if (c == S_EX_HI)
		{
		  if (offset_expr.X_op != O_constant)
		    break;
		  offset_expr.X_add_number =
		    (offset_expr.X_add_number >> 16) & 0xffff;
		}
	      *offset_reloc = BFD_RELOC_LO16;
	      s = expr_end;
	      continue;

	    case 'p':		/* pc relative offset */
	      if (mips_pic == EMBEDDED_PIC)
		*offset_reloc = BFD_RELOC_16_PCREL_S2;
	      else
		*offset_reloc = BFD_RELOC_16_PCREL;
	      my_getExpression (&offset_expr, s);
	      s = expr_end;
	      continue;

	    case 'u':		/* upper 16 bits */
	      c = my_getSmallExpression (&imm_expr, s);
	      *imm_reloc = BFD_RELOC_LO16;
	      if (c != S_EX_NONE)
		{
		  if (c != S_EX_LO)
		    {
		      if (c == S_EX_HI)
			{
			  *imm_reloc = BFD_RELOC_HI16_S;
			  imm_unmatched_hi = true;
			}
#ifdef OBJ_ELF
		      else if (c == S_EX_HIGHEST)
			*imm_reloc = BFD_RELOC_MIPS_HIGHEST;
		      else if (c == S_EX_GP_REL)
			{
			  /* This occurs in NewABI only.  */
			  c = my_getSmallExpression (&imm_expr, s);
			  if (c != S_EX_NEG)
			    as_bad (_("bad composition of relocations"));
			  else
			    {
			      c = my_getSmallExpression (&imm_expr, s);
			      if (c != S_EX_HI)
				as_bad (_("bad composition of relocations"));
			      else
				{
				  imm_reloc[0] = BFD_RELOC_GPREL16;
				  imm_reloc[1] = BFD_RELOC_MIPS_SUB;
				  imm_reloc[2] = BFD_RELOC_HI16_S;
				}
			    }
			}
#endif
		      else
			*imm_reloc = BFD_RELOC_HI16;
		    }
		  else if (imm_expr.X_op == O_constant)
		    imm_expr.X_add_number &= 0xffff;
		}
	      else if (imm_expr.X_op == O_constant
		       && (imm_expr.X_add_number < 0
			   || imm_expr.X_add_number >= 0x10000))
		as_bad (_("lui expression not in range 0..65535"));
	      s = expr_end;
	      continue;

	    case 'a':		/* 26 bit address */
	      my_getExpression (&offset_expr, s);
	      s = expr_end;
	      *offset_reloc = BFD_RELOC_MIPS_JMP;
	      continue;

	    case 'N':		/* 3 bit branch condition code */
	    case 'M':		/* 3 bit compare condition code */
	      if (strncmp (s, "$fcc", 4) != 0)
		break;
	      s += 4;
	      regno = 0;
	      do
		{
		  regno *= 10;
		  regno += *s - '0';
		  ++s;
		}
	      while (ISDIGIT (*s));
	      if (regno > 7)
		as_bad (_("invalid condition code register $fcc%d"), regno);
	      if (*args == 'N')
		ip->insn_opcode |= regno << OP_SH_BCC;
	      else
		ip->insn_opcode |= regno << OP_SH_CCC;
	      continue;

	    case 'H':
	      if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		s += 2;
	      if (ISDIGIT (*s))
		{
		  c = 0;
		  do
		    {
		      c *= 10;
		      c += *s - '0';
		      ++s;
		    }
		  while (ISDIGIT (*s));
		}
	      else
		c = 8; /* Invalid sel value.  */

	      if (c > 7)
		as_bad (_("invalid coprocessor sub-selection value (0-7)"));
	      ip->insn_opcode |= c;
	      continue;

	    case 'e':
	      /* Must be at least one digit.  */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);

	      if ((unsigned long) imm_expr.X_add_number
		  > (unsigned long) OP_MASK_VECBYTE)
		{
		  as_bad (_("bad byte vector index (%ld)"),
			   (long) imm_expr.X_add_number);
		  imm_expr.X_add_number = 0;
		}

	      ip->insn_opcode |= imm_expr.X_add_number << OP_SH_VECBYTE;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case '%':
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);

	      if ((unsigned long) imm_expr.X_add_number
		  > (unsigned long) OP_MASK_VECALIGN)
		{
		  as_bad (_("bad byte vector index (%ld)"),
			   (long) imm_expr.X_add_number);
		  imm_expr.X_add_number = 0;
		}

	      ip->insn_opcode |= imm_expr.X_add_number << OP_SH_VECALIGN;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    default:
	      as_bad (_("bad char = '%c'\n"), *args);
	      internalError ();
	    }
	  break;
	}
      /* Args don't match.  */
      if (insn + 1 < &mips_opcodes[NUMOPCODES] &&
	  !strcmp (insn->name, insn[1].name))
	{
	  ++insn;
	  s = argsStart;
	  insn_error = _("illegal operands");
	  continue;
	}
      if (save_c)
	*(--s) = save_c;
      insn_error = _("illegal operands");
      return;
    }
}

/* This routine assembles an instruction into its binary format when
   assembling for the mips16.  As a side effect, it sets one of the
   global variables imm_reloc or offset_reloc to the type of
   relocation to do if one of the operands is an address expression.
   It also sets mips16_small and mips16_ext if the user explicitly
   requested a small or extended instruction.  */

static void
mips16_ip (str, ip)
     char *str;
     struct mips_cl_insn *ip;
{
  char *s;
  const char *args;
  struct mips_opcode *insn;
  char *argsstart;
  unsigned int regno;
  unsigned int lastregno = 0;
  char *s_reset;

  insn_error = NULL;

  mips16_small = false;
  mips16_ext = false;

  for (s = str; ISLOWER (*s); ++s)
    ;
  switch (*s)
    {
    case '\0':
      break;

    case ' ':
      *s++ = '\0';
      break;

    case '.':
      if (s[1] == 't' && s[2] == ' ')
	{
	  *s = '\0';
	  mips16_small = true;
	  s += 3;
	  break;
	}
      else if (s[1] == 'e' && s[2] == ' ')
	{
	  *s = '\0';
	  mips16_ext = true;
	  s += 3;
	  break;
	}
      /* Fall through.  */
    default:
      insn_error = _("unknown opcode");
      return;
    }

  if (mips_opts.noautoextend && ! mips16_ext)
    mips16_small = true;

  if ((insn = (struct mips_opcode *) hash_find (mips16_op_hash, str)) == NULL)
    {
      insn_error = _("unrecognized opcode");
      return;
    }

  argsstart = s;
  for (;;)
    {
      assert (strcmp (insn->name, str) == 0);

      ip->insn_mo = insn;
      ip->insn_opcode = insn->match;
      ip->use_extend = false;
      imm_expr.X_op = O_absent;
      imm_reloc[0] = BFD_RELOC_UNUSED;
      imm_reloc[1] = BFD_RELOC_UNUSED;
      imm_reloc[2] = BFD_RELOC_UNUSED;
      offset_expr.X_op = O_absent;
      offset_reloc[0] = BFD_RELOC_UNUSED;
      offset_reloc[1] = BFD_RELOC_UNUSED;
      offset_reloc[2] = BFD_RELOC_UNUSED;
      for (args = insn->args; 1; ++args)
	{
	  int c;

	  if (*s == ' ')
	    ++s;

	  /* In this switch statement we call break if we did not find
             a match, continue if we did find a match, or return if we
             are done.  */

	  c = *args;
	  switch (c)
	    {
	    case '\0':
	      if (*s == '\0')
		{
		  /* Stuff the immediate value in now, if we can.  */
		  if (imm_expr.X_op == O_constant
		      && *imm_reloc > BFD_RELOC_UNUSED
		      && insn->pinfo != INSN_MACRO)
		    {
		      mips16_immed (NULL, 0, *imm_reloc - BFD_RELOC_UNUSED,
				    imm_expr.X_add_number, true, mips16_small,
				    mips16_ext, &ip->insn_opcode,
				    &ip->use_extend, &ip->extend);
		      imm_expr.X_op = O_absent;
		      *imm_reloc = BFD_RELOC_UNUSED;
		    }

		  return;
		}
	      break;

	    case ',':
	      if (*s++ == c)
		continue;
	      s--;
	      switch (*++args)
		{
		case 'v':
		  ip->insn_opcode |= lastregno << MIPS16OP_SH_RX;
		  continue;
		case 'w':
		  ip->insn_opcode |= lastregno << MIPS16OP_SH_RY;
		  continue;
		}
	      break;

	    case '(':
	    case ')':
	      if (*s++ == c)
		continue;
	      break;

	    case 'v':
	    case 'w':
	      if (s[0] != '$')
		{
		  if (c == 'v')
		    ip->insn_opcode |= lastregno << MIPS16OP_SH_RX;
		  else
		    ip->insn_opcode |= lastregno << MIPS16OP_SH_RY;
		  ++args;
		  continue;
		}
	      /* Fall through.  */
	    case 'x':
	    case 'y':
	    case 'z':
	    case 'Z':
	    case '0':
	    case 'S':
	    case 'R':
	    case 'X':
	    case 'Y':
	      if (s[0] != '$')
		break;
	      s_reset = s;
	      if (ISDIGIT (s[1]))
		{
		  ++s;
		  regno = 0;
		  do
		    {
		      regno *= 10;
		      regno += *s - '0';
		      ++s;
		    }
		  while (ISDIGIT (*s));
		  if (regno > 31)
		    {
		      as_bad (_("invalid register number (%d)"), regno);
		      regno = 2;
		    }
		}
	      else
		{
		  if (s[1] == 'r' && s[2] == 'a')
		    {
		      s += 3;
		      regno = RA;
		    }
		  else if (s[1] == 'f' && s[2] == 'p')
		    {
		      s += 3;
		      regno = FP;
		    }
		  else if (s[1] == 's' && s[2] == 'p')
		    {
		      s += 3;
		      regno = SP;
		    }
		  else if (s[1] == 'g' && s[2] == 'p')
		    {
		      s += 3;
		      regno = GP;
		    }
		  else if (s[1] == 'a' && s[2] == 't')
		    {
		      s += 3;
		      regno = AT;
		    }
		  else if (s[1] == 'k' && s[2] == 't' && s[3] == '0')
		    {
		      s += 4;
		      regno = KT0;
		    }
		  else if (s[1] == 'k' && s[2] == 't' && s[3] == '1')
		    {
		      s += 4;
		      regno = KT1;
		    }
		  else if (s[1] == 'z' && s[2] == 'e' && s[3] == 'r' && s[4] == 'o')
		    {
		      s += 5;
		      regno = ZERO;
		    }
		  else
		    break;
		}

	      if (*s == ' ')
		++s;
	      if (args[1] != *s)
		{
		  if (c == 'v' || c == 'w')
		    {
		      regno = mips16_to_32_reg_map[lastregno];
		      s = s_reset;
		      ++args;
		    }
		}

	      switch (c)
		{
		case 'x':
		case 'y':
		case 'z':
		case 'v':
		case 'w':
		case 'Z':
		  regno = mips32_to_16_reg_map[regno];
		  break;

		case '0':
		  if (regno != 0)
		    regno = ILLEGAL_REG;
		  break;

		case 'S':
		  if (regno != SP)
		    regno = ILLEGAL_REG;
		  break;

		case 'R':
		  if (regno != RA)
		    regno = ILLEGAL_REG;
		  break;

		case 'X':
		case 'Y':
		  if (regno == AT && ! mips_opts.noat)
		    as_warn (_("used $at without \".set noat\""));
		  break;

		default:
		  internalError ();
		}

	      if (regno == ILLEGAL_REG)
		break;

	      switch (c)
		{
		case 'x':
		case 'v':
		  ip->insn_opcode |= regno << MIPS16OP_SH_RX;
		  break;
		case 'y':
		case 'w':
		  ip->insn_opcode |= regno << MIPS16OP_SH_RY;
		  break;
		case 'z':
		  ip->insn_opcode |= regno << MIPS16OP_SH_RZ;
		  break;
		case 'Z':
		  ip->insn_opcode |= regno << MIPS16OP_SH_MOVE32Z;
		case '0':
		case 'S':
		case 'R':
		  break;
		case 'X':
		  ip->insn_opcode |= regno << MIPS16OP_SH_REGR32;
		  break;
		case 'Y':
		  regno = ((regno & 7) << 2) | ((regno & 0x18) >> 3);
		  ip->insn_opcode |= regno << MIPS16OP_SH_REG32R;
		  break;
		default:
		  internalError ();
		}

	      lastregno = regno;
	      continue;

	    case 'P':
	      if (strncmp (s, "$pc", 3) == 0)
		{
		  s += 3;
		  continue;
		}
	      break;

	    case '<':
	    case '>':
	    case '[':
	    case ']':
	    case '4':
	    case '5':
	    case 'H':
	    case 'W':
	    case 'D':
	    case 'j':
	    case '8':
	    case 'V':
	    case 'C':
	    case 'U':
	    case 'k':
	    case 'K':
	      if (s[0] == '%'
		  && strncmp (s + 1, "gprel(", sizeof "gprel(" - 1) == 0)
		{
		  /* This is %gprel(SYMBOL).  We need to read SYMBOL,
                     and generate the appropriate reloc.  If the text
                     inside %gprel is not a symbol name with an
                     optional offset, then we generate a normal reloc
                     and will probably fail later.  */
		  my_getExpression (&imm_expr, s + sizeof "%gprel" - 1);
		  if (imm_expr.X_op == O_symbol)
		    {
		      mips16_ext = true;
		      *imm_reloc = BFD_RELOC_MIPS16_GPREL;
		      s = expr_end;
		      ip->use_extend = true;
		      ip->extend = 0;
		      continue;
		    }
		}
	      else
		{
		  /* Just pick up a normal expression.  */
		  my_getExpression (&imm_expr, s);
		}

	      if (imm_expr.X_op == O_register)
		{
		  /* What we thought was an expression turned out to
                     be a register.  */

		  if (s[0] == '(' && args[1] == '(')
		    {
		      /* It looks like the expression was omitted
			 before a register indirection, which means
			 that the expression is implicitly zero.  We
			 still set up imm_expr, so that we handle
			 explicit extensions correctly.  */
		      imm_expr.X_op = O_constant;
		      imm_expr.X_add_number = 0;
		      *imm_reloc = (int) BFD_RELOC_UNUSED + c;
		      continue;
		    }

		  break;
		}

	      /* We need to relax this instruction.  */
	      *imm_reloc = (int) BFD_RELOC_UNUSED + c;
	      s = expr_end;
	      continue;

	    case 'p':
	    case 'q':
	    case 'A':
	    case 'B':
	    case 'E':
	      /* We use offset_reloc rather than imm_reloc for the PC
                 relative operands.  This lets macros with both
                 immediate and address operands work correctly.  */
	      my_getExpression (&offset_expr, s);

	      if (offset_expr.X_op == O_register)
		break;

	      /* We need to relax this instruction.  */
	      *offset_reloc = (int) BFD_RELOC_UNUSED + c;
	      s = expr_end;
	      continue;

	    case '6':		/* break code */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number > 63)
		{
		  as_warn (_("Invalid value for `%s' (%lu)"),
			   ip->insn_mo->name,
			   (unsigned long) imm_expr.X_add_number);
		  imm_expr.X_add_number &= 0x3f;
		}
	      ip->insn_opcode |= imm_expr.X_add_number << MIPS16OP_SH_IMM6;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    case 'a':		/* 26 bit address */
	      my_getExpression (&offset_expr, s);
	      s = expr_end;
	      *offset_reloc = BFD_RELOC_MIPS16_JMP;
	      ip->insn_opcode <<= 16;
	      continue;

	    case 'l':		/* register list for entry macro */
	    case 'L':		/* register list for exit macro */
	      {
		int mask;

		if (c == 'l')
		  mask = 0;
		else
		  mask = 7 << 3;
		while (*s != '\0')
		  {
		    int freg, reg1, reg2;

		    while (*s == ' ' || *s == ',')
		      ++s;
		    if (*s != '$')
		      {
			as_bad (_("can't parse register list"));
			break;
		      }
		    ++s;
		    if (*s != 'f')
		      freg = 0;
		    else
		      {
			freg = 1;
			++s;
		      }
		    reg1 = 0;
		    while (ISDIGIT (*s))
		      {
			reg1 *= 10;
			reg1 += *s - '0';
			++s;
		      }
		    if (*s == ' ')
		      ++s;
		    if (*s != '-')
		      reg2 = reg1;
		    else
		      {
			++s;
			if (*s != '$')
			  break;
			++s;
			if (freg)
			  {
			    if (*s == 'f')
			      ++s;
			    else
			      {
				as_bad (_("invalid register list"));
				break;
			      }
			  }
			reg2 = 0;
			while (ISDIGIT (*s))
			  {
			    reg2 *= 10;
			    reg2 += *s - '0';
			    ++s;
			  }
		      }
		    if (freg && reg1 == 0 && reg2 == 0 && c == 'L')
		      {
			mask &= ~ (7 << 3);
			mask |= 5 << 3;
		      }
		    else if (freg && reg1 == 0 && reg2 == 1 && c == 'L')
		      {
			mask &= ~ (7 << 3);
			mask |= 6 << 3;
		      }
		    else if (reg1 == 4 && reg2 >= 4 && reg2 <= 7 && c != 'L')
		      mask |= (reg2 - 3) << 3;
		    else if (reg1 == 16 && reg2 >= 16 && reg2 <= 17)
		      mask |= (reg2 - 15) << 1;
		    else if (reg1 == RA && reg2 == RA)
		      mask |= 1;
		    else
		      {
			as_bad (_("invalid register list"));
			break;
		      }
		  }
		/* The mask is filled in in the opcode table for the
                   benefit of the disassembler.  We remove it before
                   applying the actual mask.  */
		ip->insn_opcode &= ~ ((7 << 3) << MIPS16OP_SH_IMM6);
		ip->insn_opcode |= mask << MIPS16OP_SH_IMM6;
	      }
	    continue;

	    case 'e':		/* extend code */
	      my_getExpression (&imm_expr, s);
	      check_absolute_expr (ip, &imm_expr);
	      if ((unsigned long) imm_expr.X_add_number > 0x7ff)
		{
		  as_warn (_("Invalid value for `%s' (%lu)"),
			   ip->insn_mo->name,
			   (unsigned long) imm_expr.X_add_number);
		  imm_expr.X_add_number &= 0x7ff;
		}
	      ip->insn_opcode |= imm_expr.X_add_number;
	      imm_expr.X_op = O_absent;
	      s = expr_end;
	      continue;

	    default:
	      internalError ();
	    }
	  break;
	}

      /* Args don't match.  */
      if (insn + 1 < &mips16_opcodes[bfd_mips16_num_opcodes] &&
	  strcmp (insn->name, insn[1].name) == 0)
	{
	  ++insn;
	  s = argsstart;
	  continue;
	}

      insn_error = _("illegal operands");

      return;
    }
}

/* This structure holds information we know about a mips16 immediate
   argument type.  */

struct mips16_immed_operand
{
  /* The type code used in the argument string in the opcode table.  */
  int type;
  /* The number of bits in the short form of the opcode.  */
  int nbits;
  /* The number of bits in the extended form of the opcode.  */
  int extbits;
  /* The amount by which the short form is shifted when it is used;
     for example, the sw instruction has a shift count of 2.  */
  int shift;
  /* The amount by which the short form is shifted when it is stored
     into the instruction code.  */
  int op_shift;
  /* Non-zero if the short form is unsigned.  */
  int unsp;
  /* Non-zero if the extended form is unsigned.  */
  int extu;
  /* Non-zero if the value is PC relative.  */
  int pcrel;
};

/* The mips16 immediate operand types.  */

static const struct mips16_immed_operand mips16_immed_operands[] =
{
  { '<',  3,  5, 0, MIPS16OP_SH_RZ,   1, 1, 0 },
  { '>',  3,  5, 0, MIPS16OP_SH_RX,   1, 1, 0 },
  { '[',  3,  6, 0, MIPS16OP_SH_RZ,   1, 1, 0 },
  { ']',  3,  6, 0, MIPS16OP_SH_RX,   1, 1, 0 },
  { '4',  4, 15, 0, MIPS16OP_SH_IMM4, 0, 0, 0 },
  { '5',  5, 16, 0, MIPS16OP_SH_IMM5, 1, 0, 0 },
  { 'H',  5, 16, 1, MIPS16OP_SH_IMM5, 1, 0, 0 },
  { 'W',  5, 16, 2, MIPS16OP_SH_IMM5, 1, 0, 0 },
  { 'D',  5, 16, 3, MIPS16OP_SH_IMM5, 1, 0, 0 },
  { 'j',  5, 16, 0, MIPS16OP_SH_IMM5, 0, 0, 0 },
  { '8',  8, 16, 0, MIPS16OP_SH_IMM8, 1, 0, 0 },
  { 'V',  8, 16, 2, MIPS16OP_SH_IMM8, 1, 0, 0 },
  { 'C',  8, 16, 3, MIPS16OP_SH_IMM8, 1, 0, 0 },
  { 'U',  8, 16, 0, MIPS16OP_SH_IMM8, 1, 1, 0 },
  { 'k',  8, 16, 0, MIPS16OP_SH_IMM8, 0, 0, 0 },
  { 'K',  8, 16, 3, MIPS16OP_SH_IMM8, 0, 0, 0 },
  { 'p',  8, 16, 0, MIPS16OP_SH_IMM8, 0, 0, 1 },
  { 'q', 11, 16, 0, MIPS16OP_SH_IMM8, 0, 0, 1 },
  { 'A',  8, 16, 2, MIPS16OP_SH_IMM8, 1, 0, 1 },
  { 'B',  5, 16, 3, MIPS16OP_SH_IMM5, 1, 0, 1 },
  { 'E',  5, 16, 2, MIPS16OP_SH_IMM5, 1, 0, 1 }
};

#define MIPS16_NUM_IMMED \
  (sizeof mips16_immed_operands / sizeof mips16_immed_operands[0])

/* Handle a mips16 instruction with an immediate value.  This or's the
   small immediate value into *INSN.  It sets *USE_EXTEND to indicate
   whether an extended value is needed; if one is needed, it sets
   *EXTEND to the value.  The argument type is TYPE.  The value is VAL.
   If SMALL is true, an unextended opcode was explicitly requested.
   If EXT is true, an extended opcode was explicitly requested.  If
   WARN is true, warn if EXT does not match reality.  */

static void
mips16_immed (file, line, type, val, warn, small, ext, insn, use_extend,
	      extend)
     char *file;
     unsigned int line;
     int type;
     offsetT val;
     boolean warn;
     boolean small;
     boolean ext;
     unsigned long *insn;
     boolean *use_extend;
     unsigned short *extend;
{
  register const struct mips16_immed_operand *op;
  int mintiny, maxtiny;
  boolean needext;

  op = mips16_immed_operands;
  while (op->type != type)
    {
      ++op;
      assert (op < mips16_immed_operands + MIPS16_NUM_IMMED);
    }

  if (op->unsp)
    {
      if (type == '<' || type == '>' || type == '[' || type == ']')
	{
	  mintiny = 1;
	  maxtiny = 1 << op->nbits;
	}
      else
	{
	  mintiny = 0;
	  maxtiny = (1 << op->nbits) - 1;
	}
    }
  else
    {
      mintiny = - (1 << (op->nbits - 1));
      maxtiny = (1 << (op->nbits - 1)) - 1;
    }

  /* Branch offsets have an implicit 0 in the lowest bit.  */
  if (type == 'p' || type == 'q')
    val /= 2;

  if ((val & ((1 << op->shift) - 1)) != 0
      || val < (mintiny << op->shift)
      || val > (maxtiny << op->shift))
    needext = true;
  else
    needext = false;

  if (warn && ext && ! needext)
    as_warn_where (file, line,
		   _("extended operand requested but not required"));
  if (small && needext)
    as_bad_where (file, line, _("invalid unextended operand value"));

  if (small || (! ext && ! needext))
    {
      int insnval;

      *use_extend = false;
      insnval = ((val >> op->shift) & ((1 << op->nbits) - 1));
      insnval <<= op->op_shift;
      *insn |= insnval;
    }
  else
    {
      long minext, maxext;
      int extval;

      if (op->extu)
	{
	  minext = 0;
	  maxext = (1 << op->extbits) - 1;
	}
      else
	{
	  minext = - (1 << (op->extbits - 1));
	  maxext = (1 << (op->extbits - 1)) - 1;
	}
      if (val < minext || val > maxext)
	as_bad_where (file, line,
		      _("operand value out of range for instruction"));

      *use_extend = true;
      if (op->extbits == 16)
	{
	  extval = ((val >> 11) & 0x1f) | (val & 0x7e0);
	  val &= 0x1f;
	}
      else if (op->extbits == 15)
	{
	  extval = ((val >> 11) & 0xf) | (val & 0x7f0);
	  val &= 0xf;
	}
      else
	{
	  extval = ((val & 0x1f) << 6) | (val & 0x20);
	  val = 0;
	}

      *extend = (unsigned short) extval;
      *insn |= val;
    }
}

static struct percent_op_match
{
   const char *str;
   const enum small_ex_type type;
} percent_op[] =
{
  {"%lo", S_EX_LO},
#ifdef OBJ_ELF
  {"%call_hi", S_EX_CALL_HI},
  {"%call_lo", S_EX_CALL_LO},
  {"%call16", S_EX_CALL16},
  {"%got_disp", S_EX_GOT_DISP},
  {"%got_page", S_EX_GOT_PAGE},
  {"%got_ofst", S_EX_GOT_OFST},
  {"%got_hi", S_EX_GOT_HI},
  {"%got_lo", S_EX_GOT_LO},
  {"%got", S_EX_GOT},
  {"%gp_rel", S_EX_GP_REL},
  {"%half", S_EX_HALF},
  {"%highest", S_EX_HIGHEST},
  {"%higher", S_EX_HIGHER},
  {"%neg", S_EX_NEG},
#endif
  {"%hi", S_EX_HI}
};

/* Parse small expression input.  STR gets adjusted to eat up whitespace.
   It detects valid "%percent_op(...)" and "($reg)" strings.  Percent_op's
   can be nested, this is handled by blanking the innermost, parsing the
   rest by subsequent calls.  */

static int
my_getSmallParser (str, len, nestlevel)
     char **str;
     unsigned int *len;
     int *nestlevel;
{
  *len = 0;
  *str += strspn (*str, " \t");
  /* Check for expression in parentheses.  */
  if (**str == '(')
    {
      char *b = *str + 1 + strspn (*str + 1, " \t");
      char *e;

      /* Check for base register.  */
      if (b[0] == '$')
	{
	  if (strchr (b, ')')
	      && (e = b + strcspn (b, ") \t"))
	      && e - b > 1 && e - b < 4)
	    {
	      if ((e - b == 3
		   && ((b[1] == 'f' && b[2] == 'p')
		       || (b[1] == 's' && b[2] == 'p')
		       || (b[1] == 'g' && b[2] == 'p')
		       || (b[1] == 'a' && b[2] == 't')
		       || (ISDIGIT (b[1])
			   && ISDIGIT (b[2]))))
		  || (ISDIGIT (b[1])))
		{
		  *len = strcspn (*str, ")") + 1;
		  return S_EX_REGISTER;
		}
	    }
	}
      /* Check for percent_op (in parentheses).  */
      else if (b[0] == '%')
	{
	  *str = b;
	  return my_getPercentOp (str, len, nestlevel);
	}

      /* Some other expression in the parentheses, which can contain
	 parentheses itself. Attempt to find the matching one.  */
      {
	int pcnt = 1;
	char *s;

	*len = 1;
	for (s = *str + 1; *s && pcnt; s++, (*len)++)
	  {
	    if (*s == '(')
	      ++pcnt;
	    else if (*s == ')')
	      --pcnt;
	  }
      }
    }
  /* Check for percent_op (outside of parentheses).  */
  else if (*str[0] == '%')
    return my_getPercentOp (str, len, nestlevel);

  /* Any other expression.  */
  return S_EX_NONE;
}

static int
my_getPercentOp (str, len, nestlevel)
     char **str;
     unsigned int *len;
     int *nestlevel;
{
  char *tmp = *str + 1;
  unsigned int i = 0;

  while (ISALPHA (*tmp) || *tmp == '_')
    {
      *tmp = TOLOWER (*tmp);
      tmp++;
    }
  while (i < (sizeof (percent_op) / sizeof (struct percent_op_match)))
    {
      if (strncmp (*str, percent_op[i].str, strlen (percent_op[i].str)))
	i++;
      else
	{
	  int type = percent_op[i].type;

	  /* Only %hi and %lo are allowed for OldABI.  */
	  if (! HAVE_NEWABI && type != S_EX_HI && type != S_EX_LO)
	    return S_EX_NONE;

	  *len = strlen (percent_op[i].str);
	  ++(*nestlevel);
	  return type;
	}
    }
  return S_EX_NONE;
}

static int
my_getSmallExpression (ep, str)
     expressionS *ep;
     char *str;
{
  static char *oldstr = NULL;
  int c = S_EX_NONE;
  int oldc;
  int nestlevel = -1;
  unsigned int len;

  /* Don't update oldstr if the last call had nested percent_op's. We need
     it to parse the outer ones later.  */
  if (! oldstr)
    oldstr = str;

  do
    {
      oldc = c;
      c = my_getSmallParser (&str, &len, &nestlevel);
      if (c != S_EX_NONE && c != S_EX_REGISTER)
	str += len;
    }
  while (c != S_EX_NONE && c != S_EX_REGISTER);

  if (nestlevel >= 0)
    {
      /* A percent_op was encountered.  Don't try to get an expression if
	 it is already blanked out.  */
      if (*(str + strspn (str + 1, " )")) != ')')
	{
	  char save;

	  /* Let my_getExpression() stop at the closing parenthesis.  */
	  save = *(str + len);
	  *(str + len) = '\0';
	  my_getExpression (ep, str);
	  *(str + len) = save;
	}
      if (nestlevel > 0)
	{
	  /* Blank out including the % sign and the proper matching
	     parenthesis.  */
	  int pcnt = 1;
	  char *s = strrchr (oldstr, '%');
	  char *end;

	  for (end = strchr (s, '(') + 1; *end && pcnt; end++)
	    {
	      if (*end == '(')
		++pcnt;
	      else if (*end == ')')
		--pcnt;
	    }

	  memset (s, ' ', end - s);
	  str = oldstr;
	}
      else
	expr_end = str + len;

      c = oldc;
    }
  else if (c == S_EX_NONE)
    {
      my_getExpression (ep, str);
    }
  else if (c == S_EX_REGISTER)
    {
      ep->X_op = O_constant;
      expr_end = str;
      ep->X_add_symbol = NULL;
      ep->X_op_symbol = NULL;
      ep->X_add_number = 0;
    }
  else
    {
      as_fatal (_("internal error"));
    }

  if (nestlevel <= 0)
    /* All percent_op's have been handled.  */
    oldstr = NULL;

  return c;
}

static void
my_getExpression (ep, str)
     expressionS *ep;
     char *str;
{
  char *save_in;
  valueT val;

  save_in = input_line_pointer;
  input_line_pointer = str;
  expression (ep);
  expr_end = input_line_pointer;
  input_line_pointer = save_in;

  /* If we are in mips16 mode, and this is an expression based on `.',
     then we bump the value of the symbol by 1 since that is how other
     text symbols are handled.  We don't bother to handle complex
     expressions, just `.' plus or minus a constant.  */
  if (mips_opts.mips16
      && ep->X_op == O_symbol
      && strcmp (S_GET_NAME (ep->X_add_symbol), FAKE_LABEL_NAME) == 0
      && S_GET_SEGMENT (ep->X_add_symbol) == now_seg
      && symbol_get_frag (ep->X_add_symbol) == frag_now
      && symbol_constant_p (ep->X_add_symbol)
      && (val = S_GET_VALUE (ep->X_add_symbol)) == frag_now_fix ())
    S_SET_VALUE (ep->X_add_symbol, val + 1);
}

/* Turn a string in input_line_pointer into a floating point constant
   of type TYPE, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (type, litP, sizeP)
     int type;
     char *litP;
     int *sizeP;
{
  int prec;
  LITTLENUM_TYPE words[4];
  char *t;
  int i;

  switch (type)
    {
    case 'f':
      prec = 2;
      break;

    case 'd':
      prec = 4;
      break;

    default:
      *sizeP = 0;
      return _("bad call to md_atof");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * 2;

  if (! target_big_endian)
    {
      for (i = prec - 1; i >= 0; i--)
	{
	  md_number_to_chars (litP, (valueT) words[i], 2);
	  litP += 2;
	}
    }
  else
    {
      for (i = 0; i < prec; i++)
	{
	  md_number_to_chars (litP, (valueT) words[i], 2);
	  litP += 2;
	}
    }

  return NULL;
}

void
md_number_to_chars (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

#ifdef OBJ_ELF
static int support_64bit_objects(void)
{
  const char **list, **l;
  int yes;

  list = bfd_target_list ();
  for (l = list; *l != NULL; l++)
#ifdef TE_TMIPS
    /* This is traditional mips */
    if (strcmp (*l, "elf64-tradbigmips") == 0
	|| strcmp (*l, "elf64-tradlittlemips") == 0)
#else
    if (strcmp (*l, "elf64-bigmips") == 0
	|| strcmp (*l, "elf64-littlemips") == 0)
#endif
      break;
  yes = (*l != NULL);
  free (list);
  return yes;
}
#endif /* OBJ_ELF */

const char *md_shortopts = "nO::g::G:";

struct option md_longopts[] =
{
#define OPTION_MIPS1 (OPTION_MD_BASE + 1)
  {"mips0", no_argument, NULL, OPTION_MIPS1},
  {"mips1", no_argument, NULL, OPTION_MIPS1},
#define OPTION_MIPS2 (OPTION_MD_BASE + 2)
  {"mips2", no_argument, NULL, OPTION_MIPS2},
#define OPTION_MIPS3 (OPTION_MD_BASE + 3)
  {"mips3", no_argument, NULL, OPTION_MIPS3},
#define OPTION_MIPS4 (OPTION_MD_BASE + 4)
  {"mips4", no_argument, NULL, OPTION_MIPS4},
#define OPTION_MIPS5 (OPTION_MD_BASE + 5)
  {"mips5", no_argument, NULL, OPTION_MIPS5},
#define OPTION_MIPS32 (OPTION_MD_BASE + 6)
  {"mips32", no_argument, NULL, OPTION_MIPS32},
#define OPTION_MIPS64 (OPTION_MD_BASE + 7)
  {"mips64", no_argument, NULL, OPTION_MIPS64},
#define OPTION_MEMBEDDED_PIC (OPTION_MD_BASE + 8)
  {"membedded-pic", no_argument, NULL, OPTION_MEMBEDDED_PIC},
#define OPTION_TRAP (OPTION_MD_BASE + 9)
  {"trap", no_argument, NULL, OPTION_TRAP},
  {"no-break", no_argument, NULL, OPTION_TRAP},
#define OPTION_BREAK (OPTION_MD_BASE + 10)
  {"break", no_argument, NULL, OPTION_BREAK},
  {"no-trap", no_argument, NULL, OPTION_BREAK},
#define OPTION_EB (OPTION_MD_BASE + 11)
  {"EB", no_argument, NULL, OPTION_EB},
#define OPTION_EL (OPTION_MD_BASE + 12)
  {"EL", no_argument, NULL, OPTION_EL},
#define OPTION_MIPS16 (OPTION_MD_BASE + 13)
  {"mips16", no_argument, NULL, OPTION_MIPS16},
#define OPTION_NO_MIPS16 (OPTION_MD_BASE + 14)
  {"no-mips16", no_argument, NULL, OPTION_NO_MIPS16},
#define OPTION_M7000_HILO_FIX (OPTION_MD_BASE + 15)
  {"mfix7000", no_argument, NULL, OPTION_M7000_HILO_FIX},
#define OPTION_MNO_7000_HILO_FIX (OPTION_MD_BASE + 16)
  {"no-fix-7000", no_argument, NULL, OPTION_MNO_7000_HILO_FIX},
  {"mno-fix7000", no_argument, NULL, OPTION_MNO_7000_HILO_FIX},
#define OPTION_FP32 (OPTION_MD_BASE + 17)
  {"mfp32", no_argument, NULL, OPTION_FP32},
#define OPTION_GP32 (OPTION_MD_BASE + 18)
  {"mgp32", no_argument, NULL, OPTION_GP32},
#define OPTION_CONSTRUCT_FLOATS (OPTION_MD_BASE + 19)
  {"construct-floats", no_argument, NULL, OPTION_CONSTRUCT_FLOATS},
#define OPTION_NO_CONSTRUCT_FLOATS (OPTION_MD_BASE + 20)
  {"no-construct-floats", no_argument, NULL, OPTION_NO_CONSTRUCT_FLOATS},
#define OPTION_MARCH (OPTION_MD_BASE + 21)
  {"march", required_argument, NULL, OPTION_MARCH},
#define OPTION_MTUNE (OPTION_MD_BASE + 22)
  {"mtune", required_argument, NULL, OPTION_MTUNE},
#define OPTION_FP64 (OPTION_MD_BASE + 23)
  {"mfp64", no_argument, NULL, OPTION_FP64},
#define OPTION_M4650 (OPTION_MD_BASE + 24)
  {"m4650", no_argument, NULL, OPTION_M4650},
#define OPTION_NO_M4650 (OPTION_MD_BASE + 25)
  {"no-m4650", no_argument, NULL, OPTION_NO_M4650},
#define OPTION_M4010 (OPTION_MD_BASE + 26)
  {"m4010", no_argument, NULL, OPTION_M4010},
#define OPTION_NO_M4010 (OPTION_MD_BASE + 27)
  {"no-m4010", no_argument, NULL, OPTION_NO_M4010},
#define OPTION_M4100 (OPTION_MD_BASE + 28)
  {"m4100", no_argument, NULL, OPTION_M4100},
#define OPTION_NO_M4100 (OPTION_MD_BASE + 29)
  {"no-m4100", no_argument, NULL, OPTION_NO_M4100},
#define OPTION_M3900 (OPTION_MD_BASE + 30)
  {"m3900", no_argument, NULL, OPTION_M3900},
#define OPTION_NO_M3900 (OPTION_MD_BASE + 31)
  {"no-m3900", no_argument, NULL, OPTION_NO_M3900},
#define OPTION_GP64 (OPTION_MD_BASE + 32)
  {"mgp64", no_argument, NULL, OPTION_GP64},
#define OPTION_MIPS3D (OPTION_MD_BASE + 33)
  {"mips3d", no_argument, NULL, OPTION_MIPS3D},
#define OPTION_NO_MIPS3D (OPTION_MD_BASE + 34)
  {"no-mips3d", no_argument, NULL, OPTION_NO_MIPS3D},
#define OPTION_MDMX (OPTION_MD_BASE + 35)
  {"mdmx", no_argument, NULL, OPTION_MDMX},
#define OPTION_NO_MDMX (OPTION_MD_BASE + 36)
  {"no-mdmx", no_argument, NULL, OPTION_NO_MDMX},
#define OPTION_FIX_VR4122 (OPTION_MD_BASE + 37)
#define OPTION_NO_FIX_VR4122 (OPTION_MD_BASE + 38)
  {"mfix-vr4122-bugs",    no_argument, NULL, OPTION_FIX_VR4122},
  {"no-mfix-vr4122-bugs", no_argument, NULL, OPTION_NO_FIX_VR4122},
#define OPTION_RELAX_BRANCH (OPTION_MD_BASE + 39)
#define OPTION_NO_RELAX_BRANCH (OPTION_MD_BASE + 40)
  {"relax-branch", no_argument, NULL, OPTION_RELAX_BRANCH},
  {"no-relax-branch", no_argument, NULL, OPTION_NO_RELAX_BRANCH},
#ifdef OBJ_ELF
#define OPTION_ELF_BASE    (OPTION_MD_BASE + 41)
#define OPTION_CALL_SHARED (OPTION_ELF_BASE + 0)
  {"KPIC",        no_argument, NULL, OPTION_CALL_SHARED},
  {"call_shared", no_argument, NULL, OPTION_CALL_SHARED},
#define OPTION_NON_SHARED  (OPTION_ELF_BASE + 1)
  {"non_shared",  no_argument, NULL, OPTION_NON_SHARED},
#define OPTION_XGOT        (OPTION_ELF_BASE + 2)
  {"xgot",        no_argument, NULL, OPTION_XGOT},
#define OPTION_MABI        (OPTION_ELF_BASE + 3)
  {"mabi", required_argument, NULL, OPTION_MABI},
#define OPTION_32 	   (OPTION_ELF_BASE + 4)
  {"32",          no_argument, NULL, OPTION_32},
#define OPTION_N32 	   (OPTION_ELF_BASE + 5)
  {"n32",         no_argument, NULL, OPTION_N32},
#define OPTION_64          (OPTION_ELF_BASE + 6)
  {"64",          no_argument, NULL, OPTION_64},
#define OPTION_MDEBUG      (OPTION_ELF_BASE + 7)
  {"mdebug", no_argument, NULL, OPTION_MDEBUG},
#define OPTION_NO_MDEBUG   (OPTION_ELF_BASE + 8)
  {"no-mdebug", no_argument, NULL, OPTION_NO_MDEBUG},
#endif /* OBJ_ELF */
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

/* Set STRING_PTR (either &mips_arch_string or &mips_tune_string) to
   NEW_VALUE.  Warn if another value was already specified.  Note:
   we have to defer parsing the -march and -mtune arguments in order
   to handle 'from-abi' correctly, since the ABI might be specified
   in a later argument.  */

static void
mips_set_option_string (string_ptr, new_value)
     const char **string_ptr, *new_value;
{
  if (*string_ptr != 0 && strcasecmp (*string_ptr, new_value) != 0)
    as_warn (_("A different %s was already specified, is now %s"),
	     string_ptr == &mips_arch_string ? "-march" : "-mtune",
	     new_value);

  *string_ptr = new_value;
}

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case OPTION_CONSTRUCT_FLOATS:
      mips_disable_float_construction = 0;
      break;

    case OPTION_NO_CONSTRUCT_FLOATS:
      mips_disable_float_construction = 1;
      break;

    case OPTION_TRAP:
      mips_trap = 1;
      break;

    case OPTION_BREAK:
      mips_trap = 0;
      break;

    case OPTION_EB:
      target_big_endian = 1;
      break;

    case OPTION_EL:
      target_big_endian = 0;
      break;

    case 'n':
      warn_nops = 1;
      break;

    case 'O':
      if (arg && arg[1] == '0')
	mips_optimize = 1;
      else
	mips_optimize = 2;
      break;

    case 'g':
      if (arg == NULL)
	mips_debug = 2;
      else
	mips_debug = atoi (arg);
      /* When the MIPS assembler sees -g or -g2, it does not do
         optimizations which limit full symbolic debugging.  We take
         that to be equivalent to -O0.  */
      if (mips_debug == 2)
	mips_optimize = 1;
      break;

    case OPTION_MIPS1:
      file_mips_isa = ISA_MIPS1;
      break;

    case OPTION_MIPS2:
      file_mips_isa = ISA_MIPS2;
      break;

    case OPTION_MIPS3:
      file_mips_isa = ISA_MIPS3;
      break;

    case OPTION_MIPS4:
      file_mips_isa = ISA_MIPS4;
      break;

    case OPTION_MIPS5:
      file_mips_isa = ISA_MIPS5;
      break;

    case OPTION_MIPS32:
      file_mips_isa = ISA_MIPS32;
      break;

    case OPTION_MIPS64:
      file_mips_isa = ISA_MIPS64;
      break;

    case OPTION_MTUNE:
      mips_set_option_string (&mips_tune_string, arg);
      break;

    case OPTION_MARCH:
      mips_set_option_string (&mips_arch_string, arg);
      break;

    case OPTION_M4650:
      mips_set_option_string (&mips_arch_string, "4650");
      mips_set_option_string (&mips_tune_string, "4650");
      break;

    case OPTION_NO_M4650:
      break;

    case OPTION_M4010:
      mips_set_option_string (&mips_arch_string, "4010");
      mips_set_option_string (&mips_tune_string, "4010");
      break;

    case OPTION_NO_M4010:
      break;

    case OPTION_M4100:
      mips_set_option_string (&mips_arch_string, "4100");
      mips_set_option_string (&mips_tune_string, "4100");
      break;

    case OPTION_NO_M4100:
      break;

    case OPTION_M3900:
      mips_set_option_string (&mips_arch_string, "3900");
      mips_set_option_string (&mips_tune_string, "3900");
      break;

    case OPTION_NO_M3900:
      break;

    case OPTION_MDMX:
      mips_opts.ase_mdmx = 1;
      break;

    case OPTION_NO_MDMX:
      mips_opts.ase_mdmx = 0;
      break;

    case OPTION_MIPS16:
      mips_opts.mips16 = 1;
      mips_no_prev_insn (false);
      break;

    case OPTION_NO_MIPS16:
      mips_opts.mips16 = 0;
      mips_no_prev_insn (false);
      break;

    case OPTION_MIPS3D:
      mips_opts.ase_mips3d = 1;
      break;

    case OPTION_NO_MIPS3D:
      mips_opts.ase_mips3d = 0;
      break;

    case OPTION_MEMBEDDED_PIC:
      mips_pic = EMBEDDED_PIC;
      if (USE_GLOBAL_POINTER_OPT && g_switch_seen)
	{
	  as_bad (_("-G may not be used with embedded PIC code"));
	  return 0;
	}
      g_switch_value = 0x7fffffff;
      break;

    case OPTION_FIX_VR4122:
      mips_fix_4122_bugs = 1;
      break;

    case OPTION_NO_FIX_VR4122:
      mips_fix_4122_bugs = 0;
      break;

    case OPTION_RELAX_BRANCH:
      mips_relax_branch = 1;
      break;

    case OPTION_NO_RELAX_BRANCH:
      mips_relax_branch = 0;
      break;

#ifdef OBJ_ELF
      /* When generating ELF code, we permit -KPIC and -call_shared to
	 select SVR4_PIC, and -non_shared to select no PIC.  This is
	 intended to be compatible with Irix 5.  */
    case OPTION_CALL_SHARED:
      if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
	{
	  as_bad (_("-call_shared is supported only for ELF format"));
	  return 0;
	}
      mips_pic = SVR4_PIC;
      if (g_switch_seen && g_switch_value != 0)
	{
	  as_bad (_("-G may not be used with SVR4 PIC code"));
	  return 0;
	}
      g_switch_value = 0;
      break;

    case OPTION_NON_SHARED:
      if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
	{
	  as_bad (_("-non_shared is supported only for ELF format"));
	  return 0;
	}
      mips_pic = NO_PIC;
      break;

      /* The -xgot option tells the assembler to use 32 offsets when
         accessing the got in SVR4_PIC mode.  It is for Irix
         compatibility.  */
    case OPTION_XGOT:
      mips_big_got = 1;
      break;
#endif /* OBJ_ELF */

    case 'G':
      if (! USE_GLOBAL_POINTER_OPT)
	{
	  as_bad (_("-G is not supported for this configuration"));
	  return 0;
	}
      else if (mips_pic == SVR4_PIC || mips_pic == EMBEDDED_PIC)
	{
	  as_bad (_("-G may not be used with SVR4 or embedded PIC code"));
	  return 0;
	}
      else
	g_switch_value = atoi (arg);
      g_switch_seen = 1;
      break;

#ifdef OBJ_ELF
      /* The -32, -n32 and -64 options are shortcuts for -mabi=32, -mabi=n32
	 and -mabi=64.  */
    case OPTION_32:
      if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
	{
	  as_bad (_("-32 is supported for ELF format only"));
	  return 0;
	}
      mips_abi = O32_ABI;
      break;

    case OPTION_N32:
      if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
	{
	  as_bad (_("-n32 is supported for ELF format only"));
	  return 0;
	}
      mips_abi = N32_ABI;
      break;

    case OPTION_64:
      if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
	{
	  as_bad (_("-64 is supported for ELF format only"));
	  return 0;
	}
      mips_abi = N64_ABI;
      if (! support_64bit_objects())
	as_fatal (_("No compiled in support for 64 bit object file format"));
      break;
#endif /* OBJ_ELF */

    case OPTION_GP32:
      file_mips_gp32 = 1;
      break;

    case OPTION_GP64:
      file_mips_gp32 = 0;
      break;

    case OPTION_FP32:
      file_mips_fp32 = 1;
      break;

    case OPTION_FP64:
      file_mips_fp32 = 0;
      break;

#ifdef OBJ_ELF
    case OPTION_MABI:
      if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
	{
	  as_bad (_("-mabi is supported for ELF format only"));
	  return 0;
	}
      if (strcmp (arg, "32") == 0)
	mips_abi = O32_ABI;
      else if (strcmp (arg, "o64") == 0)
	mips_abi = O64_ABI;
      else if (strcmp (arg, "n32") == 0)
	mips_abi = N32_ABI;
      else if (strcmp (arg, "64") == 0)
	{
	  mips_abi = N64_ABI;
	  if (! support_64bit_objects())
	    as_fatal (_("No compiled in support for 64 bit object file "
			"format"));
	}
      else if (strcmp (arg, "eabi") == 0)
	mips_abi = EABI_ABI;
      else
	{
	  as_fatal (_("invalid abi -mabi=%s"), arg);
	  return 0;
	}
      break;
#endif /* OBJ_ELF */

    case OPTION_M7000_HILO_FIX:
      mips_7000_hilo_fix = true;
      break;

    case OPTION_MNO_7000_HILO_FIX:
      mips_7000_hilo_fix = false;
      break;

#ifdef OBJ_ELF
    case OPTION_MDEBUG:
      mips_flag_mdebug = true;
      break;

    case OPTION_NO_MDEBUG:
      mips_flag_mdebug = false;
      break;
#endif /* OBJ_ELF */

    default:
      return 0;
    }

  return 1;
}

/* Set up globals to generate code for the ISA or processor
   described by INFO.  */

static void
mips_set_architecture (info)
     const struct mips_cpu_info *info;
{
  if (info != 0)
    {
      mips_arch_info = info;
      mips_arch = info->cpu;
      mips_opts.isa = info->isa;
    }
}


/* Likewise for tuning.  */

static void
mips_set_tune (info)
     const struct mips_cpu_info *info;
{
  if (info != 0)
    {
      mips_tune_info = info;
      mips_tune = info->cpu;
    }
}


void
mips_after_parse_args ()
{
  /* GP relative stuff not working for PE */
  if (strncmp (TARGET_OS, "pe", 2) == 0
      && g_switch_value != 0)
    {
      if (g_switch_seen)
	as_bad (_("-G not supported in this configuration."));
      g_switch_value = 0;
    }

  /* The following code determines the architecture and register size.
     Similar code was added to GCC 3.3 (see override_options() in
     config/mips/mips.c).  The GAS and GCC code should be kept in sync
     as much as possible.  */

  if (mips_arch_string != 0)
    mips_set_architecture (mips_parse_cpu ("-march", mips_arch_string));

  if (mips_tune_string != 0)
    mips_set_tune (mips_parse_cpu ("-mtune", mips_tune_string));

  if (file_mips_isa != ISA_UNKNOWN)
    {
      /* Handle -mipsN.  At this point, file_mips_isa contains the
	 ISA level specified by -mipsN, while mips_opts.isa contains
	 the -march selection (if any).  */
      if (mips_arch_info != 0)
	{
	  /* -march takes precedence over -mipsN, since it is more descriptive.
	     There's no harm in specifying both as long as the ISA levels
	     are the same.  */
	  if (file_mips_isa != mips_opts.isa)
	    as_bad (_("-%s conflicts with the other architecture options, which imply -%s"),
		    mips_cpu_info_from_isa (file_mips_isa)->name,
		    mips_cpu_info_from_isa (mips_opts.isa)->name);
	}
      else
	mips_set_architecture (mips_cpu_info_from_isa (file_mips_isa));
    }

  if (mips_arch_info == 0)
    mips_set_architecture (mips_parse_cpu ("default CPU",
					   MIPS_CPU_STRING_DEFAULT));

  if (ABI_NEEDS_64BIT_REGS (mips_abi) && !ISA_HAS_64BIT_REGS (mips_opts.isa))
    as_bad ("-march=%s is not compatible with the selected ABI",
	    mips_arch_info->name);

  /* Optimize for mips_arch, unless -mtune selects a different processor.  */
  if (mips_tune_info == 0)
    mips_set_tune (mips_arch_info);

  if (file_mips_gp32 >= 0)
    {
      /* The user specified the size of the integer registers.  Make sure
	 it agrees with the ABI and ISA.  */
      if (file_mips_gp32 == 0 && !ISA_HAS_64BIT_REGS (mips_opts.isa))
	as_bad (_("-mgp64 used with a 32-bit processor"));
      else if (file_mips_gp32 == 1 && ABI_NEEDS_64BIT_REGS (mips_abi))
	as_bad (_("-mgp32 used with a 64-bit ABI"));
      else if (file_mips_gp32 == 0 && ABI_NEEDS_32BIT_REGS (mips_abi))
	as_bad (_("-mgp64 used with a 32-bit ABI"));
    }
  else
    {
      /* Infer the integer register size from the ABI and processor.
	 Restrict ourselves to 32-bit registers if that's all the
	 processor has, or if the ABI cannot handle 64-bit registers.  */
      file_mips_gp32 = (ABI_NEEDS_32BIT_REGS (mips_abi)
			|| !ISA_HAS_64BIT_REGS (mips_opts.isa));
    }

  /* ??? GAS treats single-float processors as though they had 64-bit
     float registers (although it complains when double-precision
     instructions are used).  As things stand, saying they have 32-bit
     registers would lead to spurious "register must be even" messages.
     So here we assume float registers are always the same size as
     integer ones, unless the user says otherwise.  */
  if (file_mips_fp32 < 0)
    file_mips_fp32 = file_mips_gp32;

  /* End of GCC-shared inference code.  */

  /* ??? When do we want this flag to be set?   Who uses it?  */
  if (file_mips_gp32 == 1
      && mips_abi == NO_ABI
      && ISA_HAS_64BIT_REGS (mips_opts.isa))
    mips_32bitmode = 1;

  if (mips_opts.isa == ISA_MIPS1 && mips_trap)
    as_bad (_("trap exception not supported at ISA 1"));

  /* If the selected architecture includes support for ASEs, enable
     generation of code for them.  */
  if (mips_opts.mips16 == -1)
    mips_opts.mips16 = (CPU_HAS_MIPS16 (mips_arch)) ? 1 : 0;
  if (mips_opts.ase_mips3d == -1)
    mips_opts.ase_mips3d = (CPU_HAS_MIPS3D (mips_arch)) ? 1 : 0;
  if (mips_opts.ase_mdmx == -1)
    mips_opts.ase_mdmx = (CPU_HAS_MDMX (mips_arch)) ? 1 : 0;

  file_mips_isa = mips_opts.isa;
  file_ase_mips16 = mips_opts.mips16;
  file_ase_mips3d = mips_opts.ase_mips3d;
  file_ase_mdmx = mips_opts.ase_mdmx;
  mips_opts.gp32 = file_mips_gp32;
  mips_opts.fp32 = file_mips_fp32;

  if (mips_flag_mdebug < 0)
    {
#ifdef OBJ_MAYBE_ECOFF
      if (OUTPUT_FLAVOR == bfd_target_ecoff_flavour)
	mips_flag_mdebug = 1;
      else
#endif /* OBJ_MAYBE_ECOFF */
	mips_flag_mdebug = 0;
    }
}

void
mips_init_after_args ()
{
  /* initialize opcodes */
  bfd_mips_num_opcodes = bfd_mips_num_builtin_opcodes;
  mips_opcodes = (struct mips_opcode *) mips_builtin_opcodes;
}

long
md_pcrel_from (fixP)
     fixS *fixP;
{
  if (OUTPUT_FLAVOR != bfd_target_aout_flavour
      && fixP->fx_addsy != (symbolS *) NULL
      && ! S_IS_DEFINED (fixP->fx_addsy))
    {
      /* This makes a branch to an undefined symbol be a branch to the
	 current location.  */
      if (mips_pic == EMBEDDED_PIC)
	return 4;
      else
	return 1;
    }

  /* Return the address of the delay slot.  */
  return fixP->fx_size + fixP->fx_where + fixP->fx_frag->fr_address;
}

/* This is called before the symbol table is processed.  In order to
   work with gcc when using mips-tfile, we must keep all local labels.
   However, in other cases, we want to discard them.  If we were
   called with -g, but we didn't see any debugging information, it may
   mean that gcc is smuggling debugging information through to
   mips-tfile, in which case we must generate all local labels.  */

void
mips_frob_file_before_adjust ()
{
#ifndef NO_ECOFF_DEBUGGING
  if (ECOFF_DEBUGGING
      && mips_debug != 0
      && ! ecoff_debugging_seen)
    flag_keep_locals = 1;
#endif
}

/* Sort any unmatched HI16_S relocs so that they immediately precede
   the corresponding LO reloc.  This is called before md_apply_fix3 and
   tc_gen_reloc.  Unmatched HI16_S relocs can only be generated by
   explicit use of the %hi modifier.  */

void
mips_frob_file ()
{
  struct mips_hi_fixup *l;

  for (l = mips_hi_fixup_list; l != NULL; l = l->next)
    {
      segment_info_type *seginfo;
      int pass;

      assert (l->fixp->fx_r_type == BFD_RELOC_HI16_S);

      /* Check quickly whether the next fixup happens to be a matching
         %lo.  */
      if (l->fixp->fx_next != NULL
	  && l->fixp->fx_next->fx_r_type == BFD_RELOC_LO16
	  && l->fixp->fx_addsy == l->fixp->fx_next->fx_addsy
	  && l->fixp->fx_offset == l->fixp->fx_next->fx_offset)
	continue;

      /* Look through the fixups for this segment for a matching %lo.
         When we find one, move the %hi just in front of it.  We do
         this in two passes.  In the first pass, we try to find a
         unique %lo.  In the second pass, we permit multiple %hi
         relocs for a single %lo (this is a GNU extension).  */
      seginfo = seg_info (l->seg);
      for (pass = 0; pass < 2; pass++)
	{
	  fixS *f, *prev;

	  prev = NULL;
	  for (f = seginfo->fix_root; f != NULL; f = f->fx_next)
	    {
	      /* Check whether this is a %lo fixup which matches l->fixp.  */
	      if (f->fx_r_type == BFD_RELOC_LO16
		  && f->fx_addsy == l->fixp->fx_addsy
		  && f->fx_offset == l->fixp->fx_offset
		  && (pass == 1
		      || prev == NULL
		      || prev->fx_r_type != BFD_RELOC_HI16_S
		      || prev->fx_addsy != f->fx_addsy
		      || prev->fx_offset !=  f->fx_offset))
		{
		  fixS **pf;

		  /* Move l->fixp before f.  */
		  for (pf = &seginfo->fix_root;
		       *pf != l->fixp;
		       pf = &(*pf)->fx_next)
		    assert (*pf != NULL);

		  *pf = l->fixp->fx_next;

		  l->fixp->fx_next = f;
		  if (prev == NULL)
		    seginfo->fix_root = l->fixp;
		  else
		    prev->fx_next = l->fixp;

		  break;
		}

	      prev = f;
	    }

	  if (f != NULL)
	    break;

#if 0 /* GCC code motion plus incomplete dead code elimination
	 can leave a %hi without a %lo.  */
	  if (pass == 1)
	    as_warn_where (l->fixp->fx_file, l->fixp->fx_line,
			   _("Unmatched %%hi reloc"));
#endif
	}
    }
}

/* When generating embedded PIC code we need to use a special
   relocation to represent the difference of two symbols in the .text
   section (switch tables use a difference of this sort).  See
   include/coff/mips.h for details.  This macro checks whether this
   fixup requires the special reloc.  */
#define SWITCH_TABLE(fixp) \
  ((fixp)->fx_r_type == BFD_RELOC_32 \
   && OUTPUT_FLAVOR != bfd_target_elf_flavour \
   && (fixp)->fx_addsy != NULL \
   && (fixp)->fx_subsy != NULL \
   && S_GET_SEGMENT ((fixp)->fx_addsy) == text_section \
   && S_GET_SEGMENT ((fixp)->fx_subsy) == text_section)

/* When generating embedded PIC code we must keep all PC relative
   relocations, in case the linker has to relax a call.  We also need
   to keep relocations for switch table entries.

   We may have combined relocations without symbols in the N32/N64 ABI.
   We have to prevent gas from dropping them.  */

int
mips_force_relocation (fixp)
     fixS *fixp;
{
  if (fixp->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY
      || S_FORCE_RELOC (fixp->fx_addsy))
    return 1;

  if (HAVE_NEWABI
      && S_GET_SEGMENT (fixp->fx_addsy) == bfd_abs_section_ptr
      && (fixp->fx_r_type == BFD_RELOC_MIPS_SUB
	  || fixp->fx_r_type == BFD_RELOC_HI16_S
	  || fixp->fx_r_type == BFD_RELOC_LO16))
    return 1;

  return (mips_pic == EMBEDDED_PIC
	  && (fixp->fx_pcrel
	      || SWITCH_TABLE (fixp)
	      || fixp->fx_r_type == BFD_RELOC_PCREL_HI16_S
	      || fixp->fx_r_type == BFD_RELOC_PCREL_LO16));
}

#ifdef OBJ_ELF
static int
mips_need_elf_addend_fixup (fixP)
     fixS *fixP;
{
  if (S_GET_OTHER (fixP->fx_addsy) == STO_MIPS16)
    return 1;
  if (mips_pic == EMBEDDED_PIC
      && S_IS_WEAK (fixP->fx_addsy))
    return 1;
  if (mips_pic != EMBEDDED_PIC
      && (S_IS_WEAK (fixP->fx_addsy)
	  || S_IS_EXTERNAL (fixP->fx_addsy))
      && !S_IS_COMMON (fixP->fx_addsy))
    return 1;
  if (symbol_used_in_reloc_p (fixP->fx_addsy)
      && (((bfd_get_section_flags (stdoutput,
				   S_GET_SEGMENT (fixP->fx_addsy))
	    & (SEC_LINK_ONCE | SEC_MERGE)) != 0)
	  || !strncmp (segment_name (S_GET_SEGMENT (fixP->fx_addsy)),
		       ".gnu.linkonce",
		       sizeof (".gnu.linkonce") - 1)))
    return 1;
  return 0;
}
#endif

/* Apply a fixup to the object file.  */

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT *valP;
     segT seg ATTRIBUTE_UNUSED;
{
  bfd_byte *buf;
  long insn;
  valueT value;
  static int previous_fx_r_type = 0;

  /* FIXME: Maybe just return for all reloc types not listed below?
     Eric Christopher says: "This is stupid, please rewrite md_apply_fix3. */
  if (fixP->fx_r_type == BFD_RELOC_8)
      return;

  assert (fixP->fx_size == 4
	  || fixP->fx_r_type == BFD_RELOC_16
	  || fixP->fx_r_type == BFD_RELOC_32
	  || fixP->fx_r_type == BFD_RELOC_MIPS_JMP
	  || fixP->fx_r_type == BFD_RELOC_HI16_S
	  || fixP->fx_r_type == BFD_RELOC_LO16
	  || fixP->fx_r_type == BFD_RELOC_GPREL16
	  || fixP->fx_r_type == BFD_RELOC_MIPS_LITERAL
	  || fixP->fx_r_type == BFD_RELOC_GPREL32
	  || fixP->fx_r_type == BFD_RELOC_64
	  || fixP->fx_r_type == BFD_RELOC_CTOR
	  || fixP->fx_r_type == BFD_RELOC_MIPS_SUB
	  || fixP->fx_r_type == BFD_RELOC_MIPS_HIGHEST
	  || fixP->fx_r_type == BFD_RELOC_MIPS_HIGHER
	  || fixP->fx_r_type == BFD_RELOC_MIPS_SCN_DISP
	  || fixP->fx_r_type == BFD_RELOC_MIPS_REL16
	  || fixP->fx_r_type == BFD_RELOC_MIPS_RELGOT
	  || fixP->fx_r_type == BFD_RELOC_VTABLE_INHERIT
	  || fixP->fx_r_type == BFD_RELOC_VTABLE_ENTRY
	  || fixP->fx_r_type == BFD_RELOC_MIPS_JALR);

  value = *valP;

  /* If we aren't adjusting this fixup to be against the section
     symbol, we need to adjust the value.  */
#ifdef OBJ_ELF
  if (fixP->fx_addsy != NULL && OUTPUT_FLAVOR == bfd_target_elf_flavour)
    {
      if (mips_need_elf_addend_fixup (fixP))
	{
	  reloc_howto_type *howto;
	  valueT symval = S_GET_VALUE (fixP->fx_addsy);

	  value -= symval;

	  howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);
	  if (value != 0 && howto->partial_inplace
	      && (! fixP->fx_pcrel || howto->pcrel_offset))
	    {
	      /* In this case, the bfd_install_relocation routine will
		 incorrectly add the symbol value back in.  We just want
		 the addend to appear in the object file.
		 
		 howto->pcrel_offset is added for R_MIPS_PC16, which is
		 generated for code like
		 
		 	globl g1 .text
			.text
			.space 20
		 g1:
		 x:
		 	bal g1
	       */
	      value -= symval;

	      /* Make sure the addend is still non-zero.  If it became zero
		 after the last operation, set it to a spurious value and
		 subtract the same value from the object file's contents.  */
	      if (value == 0)
		{
		  value = 8;

		  /* The in-place addends for LO16 relocations are signed;
		     leave the matching HI16 in-place addends as zero.  */
		  if (fixP->fx_r_type != BFD_RELOC_HI16_S)
		    {
		      bfd_vma contents, mask, field;

		      contents = bfd_get_bits (fixP->fx_frag->fr_literal
					       + fixP->fx_where,
					       fixP->fx_size * 8,
					       target_big_endian);

		      /* MASK has bits set where the relocation should go.
			 FIELD is -value, shifted into the appropriate place
			 for this relocation.  */
		      mask = 1 << (howto->bitsize - 1);
		      mask = (((mask - 1) << 1) | 1) << howto->bitpos;
		      field = (-value >> howto->rightshift) << howto->bitpos;

		      bfd_put_bits ((field & mask) | (contents & ~mask),
				    fixP->fx_frag->fr_literal + fixP->fx_where,
				    fixP->fx_size * 8,
				    target_big_endian);
		    }
		}
	    }
	}

      /* This code was generated using trial and error and so is
	 fragile and not trustworthy.  If you change it, you should
	 rerun the elf-rel, elf-rel2, and empic testcases and ensure
	 they still pass.  */
      if (fixP->fx_pcrel || fixP->fx_subsy != NULL)
	{
	  value += fixP->fx_frag->fr_address + fixP->fx_where;

	  /* BFD's REL handling, for MIPS, is _very_ weird.
	     This gives the right results, but it can't possibly
	     be the way things are supposed to work.  */
	  if ((fixP->fx_r_type != BFD_RELOC_16_PCREL
	       && fixP->fx_r_type != BFD_RELOC_16_PCREL_S2)
	      || S_GET_SEGMENT (fixP->fx_addsy) != undefined_section)
	    value += fixP->fx_frag->fr_address + fixP->fx_where;
	}
    }
#endif

  fixP->fx_addnumber = value;	/* Remember value for tc_gen_reloc.  */

  /* We are not done if this is a composite relocation to set up gp.  */
  if (fixP->fx_addsy == NULL && ! fixP->fx_pcrel
      && !(fixP->fx_r_type == BFD_RELOC_MIPS_SUB
	   || (fixP->fx_r_type == BFD_RELOC_64
	       && (previous_fx_r_type == BFD_RELOC_GPREL32
		   || previous_fx_r_type == BFD_RELOC_GPREL16))
	   || (previous_fx_r_type == BFD_RELOC_MIPS_SUB
	       && (fixP->fx_r_type == BFD_RELOC_HI16_S
		   || fixP->fx_r_type == BFD_RELOC_LO16))))
    fixP->fx_done = 1;
  previous_fx_r_type = fixP->fx_r_type;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_MIPS_JMP:
    case BFD_RELOC_MIPS_SHIFT5:
    case BFD_RELOC_MIPS_SHIFT6:
    case BFD_RELOC_MIPS_GOT_DISP:
    case BFD_RELOC_MIPS_GOT_PAGE:
    case BFD_RELOC_MIPS_GOT_OFST:
    case BFD_RELOC_MIPS_SUB:
    case BFD_RELOC_MIPS_INSERT_A:
    case BFD_RELOC_MIPS_INSERT_B:
    case BFD_RELOC_MIPS_DELETE:
    case BFD_RELOC_MIPS_HIGHEST:
    case BFD_RELOC_MIPS_HIGHER:
    case BFD_RELOC_MIPS_SCN_DISP:
    case BFD_RELOC_MIPS_REL16:
    case BFD_RELOC_MIPS_RELGOT:
    case BFD_RELOC_MIPS_JALR:
    case BFD_RELOC_HI16:
    case BFD_RELOC_HI16_S:
    case BFD_RELOC_GPREL16:
    case BFD_RELOC_MIPS_LITERAL:
    case BFD_RELOC_MIPS_CALL16:
    case BFD_RELOC_MIPS_GOT16:
    case BFD_RELOC_GPREL32:
    case BFD_RELOC_MIPS_GOT_HI16:
    case BFD_RELOC_MIPS_GOT_LO16:
    case BFD_RELOC_MIPS_CALL_HI16:
    case BFD_RELOC_MIPS_CALL_LO16:
    case BFD_RELOC_MIPS16_GPREL:
      if (fixP->fx_pcrel)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Invalid PC relative reloc"));
      /* Nothing needed to do. The value comes from the reloc entry */
      break;

    case BFD_RELOC_MIPS16_JMP:
      /* We currently always generate a reloc against a symbol, which
         means that we don't want an addend even if the symbol is
         defined.  */
      fixP->fx_addnumber = 0;
      break;

    case BFD_RELOC_PCREL_HI16_S:
      /* The addend for this is tricky if it is internal, so we just
	 do everything here rather than in bfd_install_relocation.  */
      if (OUTPUT_FLAVOR == bfd_target_elf_flavour
	  && !fixP->fx_done
	  && value != 0)
	break;
      if (fixP->fx_addsy
	  && (symbol_get_bfdsym (fixP->fx_addsy)->flags & BSF_SECTION_SYM) == 0)
	{
	  /* For an external symbol adjust by the address to make it
	     pcrel_offset.  We use the address of the RELLO reloc
	     which follows this one.  */
	  value += (fixP->fx_next->fx_frag->fr_address
		    + fixP->fx_next->fx_where);
	}
      value = ((value + 0x8000) >> 16) & 0xffff;
      buf = (bfd_byte *) fixP->fx_frag->fr_literal + fixP->fx_where;
      if (target_big_endian)
	buf += 2;
      md_number_to_chars ((char *) buf, value, 2);
      break;

    case BFD_RELOC_PCREL_LO16:
      /* The addend for this is tricky if it is internal, so we just
	 do everything here rather than in bfd_install_relocation.  */
      if (OUTPUT_FLAVOR == bfd_target_elf_flavour
	  && !fixP->fx_done
	  && value != 0)
	break;
      if (fixP->fx_addsy
	  && (symbol_get_bfdsym (fixP->fx_addsy)->flags & BSF_SECTION_SYM) == 0)
	value += fixP->fx_frag->fr_address + fixP->fx_where;
      buf = (bfd_byte *) fixP->fx_frag->fr_literal + fixP->fx_where;
      if (target_big_endian)
	buf += 2;
      md_number_to_chars ((char *) buf, value, 2);
      break;

    case BFD_RELOC_64:
      /* This is handled like BFD_RELOC_32, but we output a sign
         extended value if we are only 32 bits.  */
      if (fixP->fx_done
	  || (mips_pic == EMBEDDED_PIC && SWITCH_TABLE (fixP)))
	{
	  if (8 <= sizeof (valueT))
	    md_number_to_chars (fixP->fx_frag->fr_literal + fixP->fx_where,
				value, 8);
	  else
	    {
	      long w1, w2;
	      long hiv;

	      w1 = w2 = fixP->fx_where;
	      if (target_big_endian)
		w1 += 4;
	      else
		w2 += 4;
	      md_number_to_chars (fixP->fx_frag->fr_literal + w1, value, 4);
	      if ((value & 0x80000000) != 0)
		hiv = 0xffffffff;
	      else
		hiv = 0;
	      md_number_to_chars (fixP->fx_frag->fr_literal + w2, hiv, 4);
	    }
	}
      break;

    case BFD_RELOC_RVA:
    case BFD_RELOC_32:
      /* If we are deleting this reloc entry, we must fill in the
	 value now.  This can happen if we have a .word which is not
	 resolved when it appears but is later defined.  We also need
	 to fill in the value if this is an embedded PIC switch table
	 entry.  */
      if (fixP->fx_done
	  || (mips_pic == EMBEDDED_PIC && SWITCH_TABLE (fixP)))
	md_number_to_chars (fixP->fx_frag->fr_literal + fixP->fx_where,
			    value, 4);
      break;

    case BFD_RELOC_16:
      /* If we are deleting this reloc entry, we must fill in the
         value now.  */
      assert (fixP->fx_size == 2);
      if (fixP->fx_done)
	md_number_to_chars (fixP->fx_frag->fr_literal + fixP->fx_where,
			    value, 2);
      break;

    case BFD_RELOC_LO16:
      /* When handling an embedded PIC switch statement, we can wind
	 up deleting a LO16 reloc.  See the 'o' case in mips_ip.  */
      if (fixP->fx_done)
	{
	  if (value + 0x8000 > 0xffff)
	    as_bad_where (fixP->fx_file, fixP->fx_line,
			  _("relocation overflow"));
	  buf = (bfd_byte *) fixP->fx_frag->fr_literal + fixP->fx_where;
	  if (target_big_endian)
	    buf += 2;
	  md_number_to_chars ((char *) buf, value, 2);
	}
      break;

    case BFD_RELOC_16_PCREL_S2:
      if ((value & 0x3) != 0)
	as_bad_where (fixP->fx_file, fixP->fx_line,
		      _("Branch to odd address (%lx)"), (long) value);

      /* Fall through.  */

    case BFD_RELOC_16_PCREL:
      /*
       * We need to save the bits in the instruction since fixup_segment()
       * might be deleting the relocation entry (i.e., a branch within
       * the current segment).
       */
      if (!fixP->fx_done && value != 0)
	break;
      /* If 'value' is zero, the remaining reloc code won't actually
	 do the store, so it must be done here.  This is probably
	 a bug somewhere.  */
      if (!fixP->fx_done
	  && (fixP->fx_r_type != BFD_RELOC_16_PCREL_S2
	      || fixP->fx_addsy == NULL			/* ??? */
	      || ! S_IS_DEFINED (fixP->fx_addsy)))
	value -= fixP->fx_frag->fr_address + fixP->fx_where;

      value = (offsetT) value >> 2;

      /* update old instruction data */
      buf = (bfd_byte *) (fixP->fx_where + fixP->fx_frag->fr_literal);
      if (target_big_endian)
	insn = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
      else
	insn = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];

      if (value + 0x8000 <= 0xffff)
	insn |= value & 0xffff;
      else
	{
	  /* The branch offset is too large.  If this is an
             unconditional branch, and we are not generating PIC code,
             we can convert it to an absolute jump instruction.  */
	  if (mips_pic == NO_PIC
	      && fixP->fx_done
	      && fixP->fx_frag->fr_address >= text_section->vma
	      && (fixP->fx_frag->fr_address
		  < text_section->vma + text_section->_raw_size)
	      && ((insn & 0xffff0000) == 0x10000000	 /* beq $0,$0 */
		  || (insn & 0xffff0000) == 0x04010000	 /* bgez $0 */
		  || (insn & 0xffff0000) == 0x04110000)) /* bgezal $0 */
	    {
	      if ((insn & 0xffff0000) == 0x04110000)	 /* bgezal $0 */
		insn = 0x0c000000;	/* jal */
	      else
		insn = 0x08000000;	/* j */
	      fixP->fx_r_type = BFD_RELOC_MIPS_JMP;
	      fixP->fx_done = 0;
	      fixP->fx_addsy = section_symbol (text_section);
	      fixP->fx_addnumber = (value << 2) + md_pcrel_from (fixP);
	    }
	  else
	    {
	      /* If we got here, we have branch-relaxation disabled,
		 and there's nothing we can do to fix this instruction
		 without turning it into a longer sequence.  */
	      as_bad_where (fixP->fx_file, fixP->fx_line,
			    _("Branch out of range"));
	    }
	}

      md_number_to_chars ((char *) buf, (valueT) insn, 4);
      break;

    case BFD_RELOC_VTABLE_INHERIT:
      fixP->fx_done = 0;
      if (fixP->fx_addsy
          && !S_IS_DEFINED (fixP->fx_addsy)
          && !S_IS_WEAK (fixP->fx_addsy))
        S_SET_WEAK (fixP->fx_addsy);
      break;

    case BFD_RELOC_VTABLE_ENTRY:
      fixP->fx_done = 0;
      break;

    default:
      internalError ();
    }
}

#if 0
void
printInsn (oc)
     unsigned long oc;
{
  const struct mips_opcode *p;
  int treg, sreg, dreg, shamt;
  short imm;
  const char *args;
  int i;

  for (i = 0; i < NUMOPCODES; ++i)
    {
      p = &mips_opcodes[i];
      if (((oc & p->mask) == p->match) && (p->pinfo != INSN_MACRO))
	{
	  printf ("%08lx %s\t", oc, p->name);
	  treg = (oc >> 16) & 0x1f;
	  sreg = (oc >> 21) & 0x1f;
	  dreg = (oc >> 11) & 0x1f;
	  shamt = (oc >> 6) & 0x1f;
	  imm = oc;
	  for (args = p->args;; ++args)
	    {
	      switch (*args)
		{
		case '\0':
		  printf ("\n");
		  break;

		case ',':
		case '(':
		case ')':
		  printf ("%c", *args);
		  continue;

		case 'r':
		  assert (treg == sreg);
		  printf ("$%d,$%d", treg, sreg);
		  continue;

		case 'd':
		case 'G':
		  printf ("$%d", dreg);
		  continue;

		case 't':
		case 'E':
		  printf ("$%d", treg);
		  continue;

		case 'k':
		  printf ("0x%x", treg);
		  continue;

		case 'b':
		case 's':
		  printf ("$%d", sreg);
		  continue;

		case 'a':
		  printf ("0x%08lx", oc & 0x1ffffff);
		  continue;

		case 'i':
		case 'j':
		case 'o':
		case 'u':
		  printf ("%d", imm);
		  continue;

		case '<':
		case '>':
		  printf ("$%d", shamt);
		  continue;

		default:
		  internalError ();
		}
	      break;
	    }
	  return;
	}
    }
  printf (_("%08lx  UNDEFINED\n"), oc);
}
#endif

static symbolS *
get_symbol ()
{
  int c;
  char *name;
  symbolS *p;

  name = input_line_pointer;
  c = get_symbol_end ();
  p = (symbolS *) symbol_find_or_make (name);
  *input_line_pointer = c;
  return p;
}

/* Align the current frag to a given power of two.  The MIPS assembler
   also automatically adjusts any preceding label.  */

static void
mips_align (to, fill, label)
     int to;
     int fill;
     symbolS *label;
{
  mips_emit_delays (false);
  frag_align (to, fill, 0);
  record_alignment (now_seg, to);
  if (label != NULL)
    {
      assert (S_GET_SEGMENT (label) == now_seg);
      symbol_set_frag (label, frag_now);
      S_SET_VALUE (label, (valueT) frag_now_fix ());
    }
}

/* Align to a given power of two.  .align 0 turns off the automatic
   alignment used by the data creating pseudo-ops.  */

static void
s_align (x)
     int x ATTRIBUTE_UNUSED;
{
  register int temp;
  register long temp_fill;
  long max_alignment = 15;

  /*

    o  Note that the assembler pulls down any immediately preceeding label
       to the aligned address.
    o  It's not documented but auto alignment is reinstated by
       a .align pseudo instruction.
    o  Note also that after auto alignment is turned off the mips assembler
       issues an error on attempt to assemble an improperly aligned data item.
       We don't.

    */

  temp = get_absolute_expression ();
  if (temp > max_alignment)
    as_bad (_("Alignment too large: %d. assumed."), temp = max_alignment);
  else if (temp < 0)
    {
      as_warn (_("Alignment negative: 0 assumed."));
      temp = 0;
    }
  if (*input_line_pointer == ',')
    {
      ++input_line_pointer;
      temp_fill = get_absolute_expression ();
    }
  else
    temp_fill = 0;
  if (temp)
    {
      auto_align = 1;
      mips_align (temp, (int) temp_fill,
		  insn_labels != NULL ? insn_labels->label : NULL);
    }
  else
    {
      auto_align = 0;
    }

  demand_empty_rest_of_line ();
}

void
mips_flush_pending_output ()
{
  mips_emit_delays (false);
  mips_clear_insn_labels ();
}

static void
s_change_sec (sec)
     int sec;
{
  segT seg;

  /* When generating embedded PIC code, we only use the .text, .lit8,
     .sdata and .sbss sections.  We change the .data and .rdata
     pseudo-ops to use .sdata.  */
  if (mips_pic == EMBEDDED_PIC
      && (sec == 'd' || sec == 'r'))
    sec = 's';

#ifdef OBJ_ELF
  /* The ELF backend needs to know that we are changing sections, so
     that .previous works correctly.  We could do something like check
     for an obj_section_change_hook macro, but that might be confusing
     as it would not be appropriate to use it in the section changing
     functions in read.c, since obj-elf.c intercepts those.  FIXME:
     This should be cleaner, somehow.  */
  obj_elf_section_change_hook ();
#endif

  mips_emit_delays (false);
  switch (sec)
    {
    case 't':
      s_text (0);
      break;
    case 'd':
      s_data (0);
      break;
    case 'b':
      subseg_set (bss_section, (subsegT) get_absolute_expression ());
      demand_empty_rest_of_line ();
      break;

    case 'r':
      if (USE_GLOBAL_POINTER_OPT)
	{
	  seg = subseg_new (RDATA_SECTION_NAME,
			    (subsegT) get_absolute_expression ());
	  if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
	    {
	      bfd_set_section_flags (stdoutput, seg,
				     (SEC_ALLOC
				      | SEC_LOAD
				      | SEC_READONLY
				      | SEC_RELOC
				      | SEC_DATA));
	      if (strcmp (TARGET_OS, "elf") != 0)
		record_alignment (seg, 4);
	    }
	  demand_empty_rest_of_line ();
	}
      else
	{
	  as_bad (_("No read only data section in this object file format"));
	  demand_empty_rest_of_line ();
	  return;
	}
      break;

    case 's':
      if (USE_GLOBAL_POINTER_OPT)
	{
	  seg = subseg_new (".sdata", (subsegT) get_absolute_expression ());
	  if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
	    {
	      bfd_set_section_flags (stdoutput, seg,
				     SEC_ALLOC | SEC_LOAD | SEC_RELOC
				     | SEC_DATA);
	      if (strcmp (TARGET_OS, "elf") != 0)
		record_alignment (seg, 4);
	    }
	  demand_empty_rest_of_line ();
	  break;
	}
      else
	{
	  as_bad (_("Global pointers not supported; recompile -G 0"));
	  demand_empty_rest_of_line ();
	  return;
	}
    }

  auto_align = 1;
}
  
void
s_change_section (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
#ifdef OBJ_ELF
  char *section_name;
  char c;
  char next_c;
  int section_type;
  int section_flag;
  int section_entry_size;
  int section_alignment;
  
  if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
    return;

  section_name = input_line_pointer;
  c = get_symbol_end ();
  next_c = *(input_line_pointer + 1);

  /* Do we have .section Name<,"flags">?  */
  if (c != ',' || (c == ',' && next_c == '"'))
    {
      /* just after name is now '\0'.  */
      *input_line_pointer = c;
      input_line_pointer = section_name;
      obj_elf_section (ignore);
      return;
    }
  input_line_pointer++;

  /* Do we have .section Name<,type><,flag><,entry_size><,alignment>  */
  if (c == ',')
    section_type = get_absolute_expression ();
  else
    section_type = 0;
  if (*input_line_pointer++ == ',')
    section_flag = get_absolute_expression ();
  else
    section_flag = 0;
  if (*input_line_pointer++ == ',')
    section_entry_size = get_absolute_expression ();
  else
    section_entry_size = 0;
  if (*input_line_pointer++ == ',')
    section_alignment = get_absolute_expression ();
  else
    section_alignment = 0;

  obj_elf_change_section (section_name, section_type, section_flag,
			  section_entry_size, 0, 0, 0);
#endif /* OBJ_ELF */
}

void
mips_enable_auto_align ()
{
  auto_align = 1;
}

static void
s_cons (log_size)
     int log_size;
{
  symbolS *label;

  label = insn_labels != NULL ? insn_labels->label : NULL;
  mips_emit_delays (false);
  if (log_size > 0 && auto_align)
    mips_align (log_size, 0, label);
  mips_clear_insn_labels ();
  cons (1 << log_size);
}

static void
s_float_cons (type)
     int type;
{
  symbolS *label;

  label = insn_labels != NULL ? insn_labels->label : NULL;

  mips_emit_delays (false);

  if (auto_align)
    {
      if (type == 'd')
	mips_align (3, 0, label);
      else
	mips_align (2, 0, label);
    }

  mips_clear_insn_labels ();

  float_cons (type);
}

/* Handle .globl.  We need to override it because on Irix 5 you are
   permitted to say
       .globl foo .text
   where foo is an undefined symbol, to mean that foo should be
   considered to be the address of a function.  */

static void
s_mips_globl (x)
     int x ATTRIBUTE_UNUSED;
{
  char *name;
  int c;
  symbolS *symbolP;
  flagword flag;

  name = input_line_pointer;
  c = get_symbol_end ();
  symbolP = symbol_find_or_make (name);
  *input_line_pointer = c;
  SKIP_WHITESPACE ();

  /* On Irix 5, every global symbol that is not explicitly labelled as
     being a function is apparently labelled as being an object.  */
  flag = BSF_OBJECT;

  if (! is_end_of_line[(unsigned char) *input_line_pointer])
    {
      char *secname;
      asection *sec;

      secname = input_line_pointer;
      c = get_symbol_end ();
      sec = bfd_get_section_by_name (stdoutput, secname);
      if (sec == NULL)
	as_bad (_("%s: no such section"), secname);
      *input_line_pointer = c;

      if (sec != NULL && (sec->flags & SEC_CODE) != 0)
	flag = BSF_FUNCTION;
    }

  symbol_get_bfdsym (symbolP)->flags |= flag;

  S_SET_EXTERNAL (symbolP);
  demand_empty_rest_of_line ();
}

static void
s_option (x)
     int x ATTRIBUTE_UNUSED;
{
  char *opt;
  char c;

  opt = input_line_pointer;
  c = get_symbol_end ();

  if (*opt == 'O')
    {
      /* FIXME: What does this mean?  */
    }
  else if (strncmp (opt, "pic", 3) == 0)
    {
      int i;

      i = atoi (opt + 3);
      if (i == 0)
	mips_pic = NO_PIC;
      else if (i == 2)
	mips_pic = SVR4_PIC;
      else
	as_bad (_(".option pic%d not supported"), i);

      if (USE_GLOBAL_POINTER_OPT && mips_pic == SVR4_PIC)
	{
	  if (g_switch_seen && g_switch_value != 0)
	    as_warn (_("-G may not be used with SVR4 PIC code"));
	  g_switch_value = 0;
	  bfd_set_gp_size (stdoutput, 0);
	}
    }
  else
    as_warn (_("Unrecognized option \"%s\""), opt);

  *input_line_pointer = c;
  demand_empty_rest_of_line ();
}

/* This structure is used to hold a stack of .set values.  */

struct mips_option_stack
{
  struct mips_option_stack *next;
  struct mips_set_options options;
};

static struct mips_option_stack *mips_opts_stack;

/* Handle the .set pseudo-op.  */

static void
s_mipsset (x)
     int x ATTRIBUTE_UNUSED;
{
  char *name = input_line_pointer, ch;

  while (!is_end_of_line[(unsigned char) *input_line_pointer])
    ++input_line_pointer;
  ch = *input_line_pointer;
  *input_line_pointer = '\0';

  if (strcmp (name, "reorder") == 0)
    {
      if (mips_opts.noreorder && prev_nop_frag != NULL)
	{
	  /* If we still have pending nops, we can discard them.  The
	     usual nop handling will insert any that are still
	     needed.  */
	  prev_nop_frag->fr_fix -= (prev_nop_frag_holds
				    * (mips_opts.mips16 ? 2 : 4));
	  prev_nop_frag = NULL;
	}
      mips_opts.noreorder = 0;
    }
  else if (strcmp (name, "noreorder") == 0)
    {
      mips_emit_delays (true);
      mips_opts.noreorder = 1;
      mips_any_noreorder = 1;
    }
  else if (strcmp (name, "at") == 0)
    {
      mips_opts.noat = 0;
    }
  else if (strcmp (name, "noat") == 0)
    {
      mips_opts.noat = 1;
    }
  else if (strcmp (name, "macro") == 0)
    {
      mips_opts.warn_about_macros = 0;
    }
  else if (strcmp (name, "nomacro") == 0)
    {
      if (mips_opts.noreorder == 0)
	as_bad (_("`noreorder' must be set before `nomacro'"));
      mips_opts.warn_about_macros = 1;
    }
  else if (strcmp (name, "move") == 0 || strcmp (name, "novolatile") == 0)
    {
      mips_opts.nomove = 0;
    }
  else if (strcmp (name, "nomove") == 0 || strcmp (name, "volatile") == 0)
    {
      mips_opts.nomove = 1;
    }
  else if (strcmp (name, "bopt") == 0)
    {
      mips_opts.nobopt = 0;
    }
  else if (strcmp (name, "nobopt") == 0)
    {
      mips_opts.nobopt = 1;
    }
  else if (strcmp (name, "mips16") == 0
	   || strcmp (name, "MIPS-16") == 0)
    mips_opts.mips16 = 1;
  else if (strcmp (name, "nomips16") == 0
	   || strcmp (name, "noMIPS-16") == 0)
    mips_opts.mips16 = 0;
  else if (strcmp (name, "mips3d") == 0)
    mips_opts.ase_mips3d = 1;
  else if (strcmp (name, "nomips3d") == 0)
    mips_opts.ase_mips3d = 0;
  else if (strcmp (name, "mdmx") == 0)
    mips_opts.ase_mdmx = 1;
  else if (strcmp (name, "nomdmx") == 0)
    mips_opts.ase_mdmx = 0;
  else if (strncmp (name, "mips", 4) == 0)
    {
      int isa;

      /* Permit the user to change the ISA on the fly.  Needless to
	 say, misuse can cause serious problems.  */
      isa = atoi (name + 4);
      switch (isa)
	{
	case  0:
	  mips_opts.gp32 = file_mips_gp32;
	  mips_opts.fp32 = file_mips_fp32;
	  break;
	case  1:
	case  2:
	case 32:
	  mips_opts.gp32 = 1;
	  mips_opts.fp32 = 1;
	  break;
	case  3:
	case  4:
	case  5:
	case 64:
	  mips_opts.gp32 = 0;
	  mips_opts.fp32 = 0;
	  break;
	default:
	  as_bad (_("unknown ISA level %s"), name + 4);
	  break;
	}

      switch (isa)
	{
	case  0: mips_opts.isa = file_mips_isa;   break;
	case  1: mips_opts.isa = ISA_MIPS1;       break;
	case  2: mips_opts.isa = ISA_MIPS2;       break;
	case  3: mips_opts.isa = ISA_MIPS3;       break;
	case  4: mips_opts.isa = ISA_MIPS4;       break;
	case  5: mips_opts.isa = ISA_MIPS5;       break;
	case 32: mips_opts.isa = ISA_MIPS32;      break;
	case 64: mips_opts.isa = ISA_MIPS64;      break;
	default: as_bad (_("unknown ISA level %s"), name + 4); break;
	}
    }
  else if (strcmp (name, "autoextend") == 0)
    mips_opts.noautoextend = 0;
  else if (strcmp (name, "noautoextend") == 0)
    mips_opts.noautoextend = 1;
  else if (strcmp (name, "push") == 0)
    {
      struct mips_option_stack *s;

      s = (struct mips_option_stack *) xmalloc (sizeof *s);
      s->next = mips_opts_stack;
      s->options = mips_opts;
      mips_opts_stack = s;
    }
  else if (strcmp (name, "pop") == 0)
    {
      struct mips_option_stack *s;

      s = mips_opts_stack;
      if (s == NULL)
	as_bad (_(".set pop with no .set push"));
      else
	{
	  /* If we're changing the reorder mode we need to handle
             delay slots correctly.  */
	  if (s->options.noreorder && ! mips_opts.noreorder)
	    mips_emit_delays (true);
	  else if (! s->options.noreorder && mips_opts.noreorder)
	    {
	      if (prev_nop_frag != NULL)
		{
		  prev_nop_frag->fr_fix -= (prev_nop_frag_holds
					    * (mips_opts.mips16 ? 2 : 4));
		  prev_nop_frag = NULL;
		}
	    }

	  mips_opts = s->options;
	  mips_opts_stack = s->next;
	  free (s);
	}
    }
  else
    {
      as_warn (_("Tried to set unrecognized symbol: %s\n"), name);
    }
  *input_line_pointer = ch;
  demand_empty_rest_of_line ();
}

/* Handle the .abicalls pseudo-op.  I believe this is equivalent to
   .option pic2.  It means to generate SVR4 PIC calls.  */

static void
s_abicalls (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  mips_pic = SVR4_PIC;
  if (USE_GLOBAL_POINTER_OPT)
    {
      if (g_switch_seen && g_switch_value != 0)
	as_warn (_("-G may not be used with SVR4 PIC code"));
      g_switch_value = 0;
    }
  bfd_set_gp_size (stdoutput, 0);
  demand_empty_rest_of_line ();
}

/* Handle the .cpload pseudo-op.  This is used when generating SVR4
   PIC code.  It sets the $gp register for the function based on the
   function address, which is in the register named in the argument.
   This uses a relocation against _gp_disp, which is handled specially
   by the linker.  The result is:
	lui	$gp,%hi(_gp_disp)
	addiu	$gp,$gp,%lo(_gp_disp)
	addu	$gp,$gp,.cpload argument
   The .cpload argument is normally $25 == $t9.  */

static void
s_cpload (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  expressionS ex;
  int icnt = 0;

  /* If we are not generating SVR4 PIC code, or if this is NewABI code,
     .cpload is ignored.  */
  if (mips_pic != SVR4_PIC || HAVE_NEWABI)
    {
      s_ignore (0);
      return;
    }

  /* .cpload should be in a .set noreorder section.  */
  if (mips_opts.noreorder == 0)
    as_warn (_(".cpload not in noreorder section"));

  ex.X_op = O_symbol;
  ex.X_add_symbol = symbol_find_or_make ("_gp_disp");
  ex.X_op_symbol = NULL;
  ex.X_add_number = 0;

  /* In ELF, this symbol is implicitly an STT_OBJECT symbol.  */
  symbol_get_bfdsym (ex.X_add_symbol)->flags |= BSF_OBJECT;

  macro_build_lui (NULL, &icnt, &ex, mips_gp_register);
  macro_build ((char *) NULL, &icnt, &ex, "addiu", "t,r,j",
	       mips_gp_register, mips_gp_register, (int) BFD_RELOC_LO16);

  macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "addu", "d,v,t",
	       mips_gp_register, mips_gp_register, tc_get_register (0));

  demand_empty_rest_of_line ();
}

/* Handle the .cpsetup pseudo-op defined for NewABI PIC code.  The syntax is:
     .cpsetup $reg1, offset|$reg2, label

   If offset is given, this results in:
     sd		$gp, offset($sp)
     lui	$gp, %hi(%neg(%gp_rel(label)))
     addiu	$gp, $gp, %lo(%neg(%gp_rel(label)))
     daddu	$gp, $gp, $reg1

   If $reg2 is given, this results in:
     daddu	$reg2, $gp, $0
     lui	$gp, %hi(%neg(%gp_rel(label)))
     addiu	$gp, $gp, %lo(%neg(%gp_rel(label)))
     daddu	$gp, $gp, $reg1
   $reg1 is normally $25 == $t9.  */
static void
s_cpsetup (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  expressionS ex_off;
  expressionS ex_sym;
  int reg1;
  int icnt = 0;
  char *f;

  /* If we are not generating SVR4 PIC code, .cpsetup is ignored.
     We also need NewABI support.  */
  if (mips_pic != SVR4_PIC || ! HAVE_NEWABI)
    {
      s_ignore (0);
      return;
    }

  reg1 = tc_get_register (0);
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      as_bad (_("missing argument separator ',' for .cpsetup"));
      return;
    }
  else
    ++input_line_pointer;
  SKIP_WHITESPACE ();
  if (*input_line_pointer == '$')
    {
      mips_cpreturn_register = tc_get_register (0);
      mips_cpreturn_offset = -1;
    }
  else
    {
      mips_cpreturn_offset = get_absolute_expression ();
      mips_cpreturn_register = -1;
    }
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      as_bad (_("missing argument separator ',' for .cpsetup"));
      return;
    }
  else
    ++input_line_pointer;
  SKIP_WHITESPACE ();
  expression (&ex_sym);

  if (mips_cpreturn_register == -1)
    {
      ex_off.X_op = O_constant;
      ex_off.X_add_symbol = NULL;
      ex_off.X_op_symbol = NULL;
      ex_off.X_add_number = mips_cpreturn_offset;

      macro_build ((char *) NULL, &icnt, &ex_off, "sd", "t,o(b)",
		   mips_gp_register, (int) BFD_RELOC_LO16, SP);
    }
  else
    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "daddu",
		 "d,v,t", mips_cpreturn_register, mips_gp_register, 0);

  /* Ensure there's room for the next two instructions, so that `f'
     doesn't end up with an address in the wrong frag.  */
  frag_grow (8);
  f = frag_more (0);
  macro_build ((char *) NULL, &icnt, &ex_sym, "lui", "t,u", mips_gp_register,
	       (int) BFD_RELOC_GPREL16);
  fix_new (frag_now, f - frag_now->fr_literal,
	   0, NULL, 0, 0, BFD_RELOC_MIPS_SUB);
  fix_new (frag_now, f - frag_now->fr_literal,
	   0, NULL, 0, 0, BFD_RELOC_HI16_S);

  f = frag_more (0);
  macro_build ((char *) NULL, &icnt, &ex_sym, "addiu", "t,r,j",
	       mips_gp_register, mips_gp_register, (int) BFD_RELOC_GPREL16);
  fix_new (frag_now, f - frag_now->fr_literal,
	   0, NULL, 0, 0, BFD_RELOC_MIPS_SUB);
  fix_new (frag_now, f - frag_now->fr_literal,
	   0, NULL, 0, 0, BFD_RELOC_LO16);

  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
	       HAVE_64BIT_ADDRESSES ? "daddu" : "addu", "d,v,t",
	       mips_gp_register, mips_gp_register, reg1);

  demand_empty_rest_of_line ();
}

static void
s_cplocal (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* If we are not generating SVR4 PIC code, or if this is not NewABI code,
   .cplocal is ignored.  */
  if (mips_pic != SVR4_PIC || ! HAVE_NEWABI)
    {
      s_ignore (0);
      return;
    }

  mips_gp_register = tc_get_register (0);
  demand_empty_rest_of_line ();
}

/* Handle the .cprestore pseudo-op.  This stores $gp into a given
   offset from $sp.  The offset is remembered, and after making a PIC
   call $gp is restored from that location.  */

static void
s_cprestore (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  expressionS ex;
  int icnt = 0;

  /* If we are not generating SVR4 PIC code, or if this is NewABI code,
     .cprestore is ignored.  */
  if (mips_pic != SVR4_PIC || HAVE_NEWABI)
    {
      s_ignore (0);
      return;
    }

  mips_cprestore_offset = get_absolute_expression ();
  mips_cprestore_valid = 1;

  ex.X_op = O_constant;
  ex.X_add_symbol = NULL;
  ex.X_op_symbol = NULL;
  ex.X_add_number = mips_cprestore_offset;

  macro_build_ldst_constoffset ((char *) NULL, &icnt, &ex,
				HAVE_32BIT_ADDRESSES ? "sw" : "sd",
				mips_gp_register, SP);

  demand_empty_rest_of_line ();
}

/* Handle the .cpreturn pseudo-op defined for NewABI PIC code. If an offset
   was given in the preceeding .gpsetup, it results in:
     ld		$gp, offset($sp)

   If a register $reg2 was given there, it results in:
     daddiu	$gp, $gp, $reg2
 */
static void
s_cpreturn (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  expressionS ex;
  int icnt = 0;

  /* If we are not generating SVR4 PIC code, .cpreturn is ignored.
     We also need NewABI support.  */
  if (mips_pic != SVR4_PIC || ! HAVE_NEWABI)
    {
      s_ignore (0);
      return;
    }

  if (mips_cpreturn_register == -1)
    {
      ex.X_op = O_constant;
      ex.X_add_symbol = NULL;
      ex.X_op_symbol = NULL;
      ex.X_add_number = mips_cpreturn_offset;

      macro_build ((char *) NULL, &icnt, &ex, "ld", "t,o(b)",
		   mips_gp_register, (int) BFD_RELOC_LO16, SP);
    }
  else
    macro_build ((char *) NULL, &icnt, (expressionS *) NULL, "daddu",
		 "d,v,t", mips_gp_register, mips_cpreturn_register, 0);

  demand_empty_rest_of_line ();
}

/* Handle the .gpvalue pseudo-op.  This is used when generating NewABI PIC
   code.  It sets the offset to use in gp_rel relocations.  */

static void
s_gpvalue (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  /* If we are not generating SVR4 PIC code, .gpvalue is ignored.
     We also need NewABI support.  */
  if (mips_pic != SVR4_PIC || ! HAVE_NEWABI)
    {
      s_ignore (0);
      return;
    }

  mips_gprel_offset = get_absolute_expression ();

  demand_empty_rest_of_line ();
}

/* Handle the .gpword pseudo-op.  This is used when generating PIC
   code.  It generates a 32 bit GP relative reloc.  */

static void
s_gpword (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  symbolS *label;
  expressionS ex;
  char *p;

  /* When not generating PIC code, this is treated as .word.  */
  if (mips_pic != SVR4_PIC)
    {
      s_cons (2);
      return;
    }

  label = insn_labels != NULL ? insn_labels->label : NULL;
  mips_emit_delays (true);
  if (auto_align)
    mips_align (2, 0, label);
  mips_clear_insn_labels ();

  expression (&ex);

  if (ex.X_op != O_symbol || ex.X_add_number != 0)
    {
      as_bad (_("Unsupported use of .gpword"));
      ignore_rest_of_line ();
    }

  p = frag_more (4);
  md_number_to_chars (p, (valueT) 0, 4);
  fix_new_exp (frag_now, p - frag_now->fr_literal, 4, &ex, false,
	       BFD_RELOC_GPREL32);

  demand_empty_rest_of_line ();
}

static void
s_gpdword (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  symbolS *label;
  expressionS ex;
  char *p;

  /* When not generating PIC code, this is treated as .dword.  */
  if (mips_pic != SVR4_PIC)
    {
      s_cons (3);
      return;
    }

  label = insn_labels != NULL ? insn_labels->label : NULL;
  mips_emit_delays (true);
  if (auto_align)
    mips_align (3, 0, label);
  mips_clear_insn_labels ();

  expression (&ex);

  if (ex.X_op != O_symbol || ex.X_add_number != 0)
    {
      as_bad (_("Unsupported use of .gpdword"));
      ignore_rest_of_line ();
    }

  p = frag_more (8);
  md_number_to_chars (p, (valueT) 0, 8);
  fix_new_exp (frag_now, p - frag_now->fr_literal, 8, &ex, false,
	       BFD_RELOC_GPREL32);

  /* GPREL32 composed with 64 gives a 64-bit GP offset.  */
  ex.X_op = O_absent;
  ex.X_add_symbol = 0;
  ex.X_add_number = 0;
  fix_new_exp (frag_now, p - frag_now->fr_literal, 8, &ex, false,
	       BFD_RELOC_64);

  demand_empty_rest_of_line ();
}

/* Handle the .cpadd pseudo-op.  This is used when dealing with switch
   tables in SVR4 PIC code.  */

static void
s_cpadd (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  int icnt = 0;
  int reg;

  /* This is ignored when not generating SVR4 PIC code.  */
  if (mips_pic != SVR4_PIC)
    {
      s_ignore (0);
      return;
    }

  /* Add $gp to the register named as an argument.  */
  reg = tc_get_register (0);
  macro_build ((char *) NULL, &icnt, (expressionS *) NULL,
	       HAVE_32BIT_ADDRESSES ? "addu" : "daddu",
	       "d,v,t", reg, reg, mips_gp_register);

  demand_empty_rest_of_line ();
}

/* Handle the .insn pseudo-op.  This marks instruction labels in
   mips16 mode.  This permits the linker to handle them specially,
   such as generating jalx instructions when needed.  We also make
   them odd for the duration of the assembly, in order to generate the
   right sort of code.  We will make them even in the adjust_symtab
   routine, while leaving them marked.  This is convenient for the
   debugger and the disassembler.  The linker knows to make them odd
   again.  */

static void
s_insn (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  mips16_mark_labels ();

  demand_empty_rest_of_line ();
}

/* Handle a .stabn directive.  We need these in order to mark a label
   as being a mips16 text label correctly.  Sometimes the compiler
   will emit a label, followed by a .stabn, and then switch sections.
   If the label and .stabn are in mips16 mode, then the label is
   really a mips16 text label.  */

static void
s_mips_stab (type)
     int type;
{
  if (type == 'n')
    mips16_mark_labels ();

  s_stab (type);
}

/* Handle the .weakext pseudo-op as defined in Kane and Heinrich.
 */

static void
s_mips_weakext (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *name;
  int c;
  symbolS *symbolP;
  expressionS exp;

  name = input_line_pointer;
  c = get_symbol_end ();
  symbolP = symbol_find_or_make (name);
  S_SET_WEAK (symbolP);
  *input_line_pointer = c;

  SKIP_WHITESPACE ();

  if (! is_end_of_line[(unsigned char) *input_line_pointer])
    {
      if (S_IS_DEFINED (symbolP))
	{
	  as_bad ("ignoring attempt to redefine symbol %s",
		  S_GET_NAME (symbolP));
	  ignore_rest_of_line ();
	  return;
	}

      if (*input_line_pointer == ',')
	{
	  ++input_line_pointer;
	  SKIP_WHITESPACE ();
	}

      expression (&exp);
      if (exp.X_op != O_symbol)
	{
	  as_bad ("bad .weakext directive");
	  ignore_rest_of_line ();
	  return;
	}
      symbol_set_value_expression (symbolP, &exp);
    }

  demand_empty_rest_of_line ();
}

/* Parse a register string into a number.  Called from the ECOFF code
   to parse .frame.  The argument is non-zero if this is the frame
   register, so that we can record it in mips_frame_reg.  */

int
tc_get_register (frame)
     int frame;
{
  int reg;

  SKIP_WHITESPACE ();
  if (*input_line_pointer++ != '$')
    {
      as_warn (_("expected `$'"));
      reg = ZERO;
    }
  else if (ISDIGIT (*input_line_pointer))
    {
      reg = get_absolute_expression ();
      if (reg < 0 || reg >= 32)
	{
	  as_warn (_("Bad register number"));
	  reg = ZERO;
	}
    }
  else
    {
      if (strncmp (input_line_pointer, "ra", 2) == 0)
	{
	  reg = RA;
	  input_line_pointer += 2;
	}
      else if (strncmp (input_line_pointer, "fp", 2) == 0)
	{
	  reg = FP;
	  input_line_pointer += 2;
	}
      else if (strncmp (input_line_pointer, "sp", 2) == 0)
	{
	  reg = SP;
	  input_line_pointer += 2;
	}
      else if (strncmp (input_line_pointer, "gp", 2) == 0)
	{
	  reg = GP;
	  input_line_pointer += 2;
	}
      else if (strncmp (input_line_pointer, "at", 2) == 0)
	{
	  reg = AT;
	  input_line_pointer += 2;
	}
      else if (strncmp (input_line_pointer, "kt0", 3) == 0)
	{
	  reg = KT0;
	  input_line_pointer += 3;
	}
      else if (strncmp (input_line_pointer, "kt1", 3) == 0)
	{
	  reg = KT1;
	  input_line_pointer += 3;
	}
      else if (strncmp (input_line_pointer, "zero", 4) == 0)
	{
	  reg = ZERO;
	  input_line_pointer += 4;
	}
      else
	{
	  as_warn (_("Unrecognized register name"));
	  reg = ZERO;
	  while (ISALNUM(*input_line_pointer))
	   input_line_pointer++;
	}
    }
  if (frame)
    {
      mips_frame_reg = reg != 0 ? reg : SP;
      mips_frame_reg_valid = 1;
      mips_cprestore_valid = 0;
    }
  return reg;
}

valueT
md_section_align (seg, addr)
     asection *seg;
     valueT addr;
{
  int align = bfd_get_section_alignment (stdoutput, seg);

#ifdef OBJ_ELF
  /* We don't need to align ELF sections to the full alignment.
     However, Irix 5 may prefer that we align them at least to a 16
     byte boundary.  We don't bother to align the sections if we are
     targeted for an embedded system.  */
  if (strcmp (TARGET_OS, "elf") == 0)
    return addr;
  if (align > 4)
    align = 4;
#endif

  return ((addr + (1 << align) - 1) & (-1 << align));
}

/* Utility routine, called from above as well.  If called while the
   input file is still being read, it's only an approximation.  (For
   example, a symbol may later become defined which appeared to be
   undefined earlier.)  */

static int
nopic_need_relax (sym, before_relaxing)
     symbolS *sym;
     int before_relaxing;
{
  if (sym == 0)
    return 0;

  if (USE_GLOBAL_POINTER_OPT && g_switch_value > 0)
    {
      const char *symname;
      int change;

      /* Find out whether this symbol can be referenced off the $gp
	 register.  It can be if it is smaller than the -G size or if
	 it is in the .sdata or .sbss section.  Certain symbols can
	 not be referenced off the $gp, although it appears as though
	 they can.  */
      symname = S_GET_NAME (sym);
      if (symname != (const char *) NULL
	  && (strcmp (symname, "eprol") == 0
	      || strcmp (symname, "etext") == 0
	      || strcmp (symname, "_gp") == 0
	      || strcmp (symname, "edata") == 0
	      || strcmp (symname, "_fbss") == 0
	      || strcmp (symname, "_fdata") == 0
	      || strcmp (symname, "_ftext") == 0
	      || strcmp (symname, "end") == 0
	      || strcmp (symname, "_gp_disp") == 0))
	change = 1;
      else if ((! S_IS_DEFINED (sym) || S_IS_COMMON (sym))
	       && (0
#ifndef NO_ECOFF_DEBUGGING
		   || (symbol_get_obj (sym)->ecoff_extern_size != 0
		       && (symbol_get_obj (sym)->ecoff_extern_size
			   <= g_switch_value))
#endif
		   /* We must defer this decision until after the whole
		      file has been read, since there might be a .extern
		      after the first use of this symbol.  */
		   || (before_relaxing
#ifndef NO_ECOFF_DEBUGGING
		       && symbol_get_obj (sym)->ecoff_extern_size == 0
#endif
		       && S_GET_VALUE (sym) == 0)
		   || (S_GET_VALUE (sym) != 0
		       && S_GET_VALUE (sym) <= g_switch_value)))
	change = 0;
      else
	{
	  const char *segname;

	  segname = segment_name (S_GET_SEGMENT (sym));
	  assert (strcmp (segname, ".lit8") != 0
		  && strcmp (segname, ".lit4") != 0);
	  change = (strcmp (segname, ".sdata") != 0
		    && strcmp (segname, ".sbss") != 0
		    && strncmp (segname, ".sdata.", 7) != 0
		    && strncmp (segname, ".gnu.linkonce.s.", 16) != 0);
	}
      return change;
    }
  else
    /* We are not optimizing for the $gp register.  */
    return 1;
}

/* Given a mips16 variant frag FRAGP, return non-zero if it needs an
   extended opcode.  SEC is the section the frag is in.  */

static int
mips16_extended_frag (fragp, sec, stretch)
     fragS *fragp;
     asection *sec;
     long stretch;
{
  int type;
  register const struct mips16_immed_operand *op;
  offsetT val;
  int mintiny, maxtiny;
  segT symsec;
  fragS *sym_frag;

  if (RELAX_MIPS16_USER_SMALL (fragp->fr_subtype))
    return 0;
  if (RELAX_MIPS16_USER_EXT (fragp->fr_subtype))
    return 1;

  type = RELAX_MIPS16_TYPE (fragp->fr_subtype);
  op = mips16_immed_operands;
  while (op->type != type)
    {
      ++op;
      assert (op < mips16_immed_operands + MIPS16_NUM_IMMED);
    }

  if (op->unsp)
    {
      if (type == '<' || type == '>' || type == '[' || type == ']')
	{
	  mintiny = 1;
	  maxtiny = 1 << op->nbits;
	}
      else
	{
	  mintiny = 0;
	  maxtiny = (1 << op->nbits) - 1;
	}
    }
  else
    {
      mintiny = - (1 << (op->nbits - 1));
      maxtiny = (1 << (op->nbits - 1)) - 1;
    }

  sym_frag = symbol_get_frag (fragp->fr_symbol);
  val = S_GET_VALUE (fragp->fr_symbol);
  symsec = S_GET_SEGMENT (fragp->fr_symbol);

  if (op->pcrel)
    {
      addressT addr;

      /* We won't have the section when we are called from
         mips_relax_frag.  However, we will always have been called
         from md_estimate_size_before_relax first.  If this is a
         branch to a different section, we mark it as such.  If SEC is
         NULL, and the frag is not marked, then it must be a branch to
         the same section.  */
      if (sec == NULL)
	{
	  if (RELAX_MIPS16_LONG_BRANCH (fragp->fr_subtype))
	    return 1;
	}
      else
	{
	  /* Must have been called from md_estimate_size_before_relax.  */
	  if (symsec != sec)
	    {
	      fragp->fr_subtype =
		RELAX_MIPS16_MARK_LONG_BRANCH (fragp->fr_subtype);

	      /* FIXME: We should support this, and let the linker
                 catch branches and loads that are out of range.  */
	      as_bad_where (fragp->fr_file, fragp->fr_line,
			    _("unsupported PC relative reference to different section"));

	      return 1;
	    }
	  if (fragp != sym_frag && sym_frag->fr_address == 0)
	    /* Assume non-extended on the first relaxation pass.
	       The address we have calculated will be bogus if this is
	       a forward branch to another frag, as the forward frag
	       will have fr_address == 0.  */
	    return 0;
	}

      /* In this case, we know for sure that the symbol fragment is in
	 the same section.  If the relax_marker of the symbol fragment
	 differs from the relax_marker of this fragment, we have not
	 yet adjusted the symbol fragment fr_address.  We want to add
	 in STRETCH in order to get a better estimate of the address.
	 This particularly matters because of the shift bits.  */
      if (stretch != 0
	  && sym_frag->relax_marker != fragp->relax_marker)
	{
	  fragS *f;

	  /* Adjust stretch for any alignment frag.  Note that if have
             been expanding the earlier code, the symbol may be
             defined in what appears to be an earlier frag.  FIXME:
             This doesn't handle the fr_subtype field, which specifies
             a maximum number of bytes to skip when doing an
             alignment.  */
	  for (f = fragp; f != NULL && f != sym_frag; f = f->fr_next)
	    {
	      if (f->fr_type == rs_align || f->fr_type == rs_align_code)
		{
		  if (stretch < 0)
		    stretch = - ((- stretch)
				 & ~ ((1 << (int) f->fr_offset) - 1));
		  else
		    stretch &= ~ ((1 << (int) f->fr_offset) - 1);
		  if (stretch == 0)
		    break;
		}
	    }
	  if (f != NULL)
	    val += stretch;
	}

      addr = fragp->fr_address + fragp->fr_fix;

      /* The base address rules are complicated.  The base address of
         a branch is the following instruction.  The base address of a
         PC relative load or add is the instruction itself, but if it
         is in a delay slot (in which case it can not be extended) use
         the address of the instruction whose delay slot it is in.  */
      if (type == 'p' || type == 'q')
	{
	  addr += 2;

	  /* If we are currently assuming that this frag should be
	     extended, then, the current address is two bytes
	     higher.  */
	  if (RELAX_MIPS16_EXTENDED (fragp->fr_subtype))
	    addr += 2;

	  /* Ignore the low bit in the target, since it will be set
             for a text label.  */
	  if ((val & 1) != 0)
	    --val;
	}
      else if (RELAX_MIPS16_JAL_DSLOT (fragp->fr_subtype))
	addr -= 4;
      else if (RELAX_MIPS16_DSLOT (fragp->fr_subtype))
	addr -= 2;

      val -= addr & ~ ((1 << op->shift) - 1);

      /* Branch offsets have an implicit 0 in the lowest bit.  */
      if (type == 'p' || type == 'q')
	val /= 2;

      /* If any of the shifted bits are set, we must use an extended
         opcode.  If the address depends on the size of this
         instruction, this can lead to a loop, so we arrange to always
         use an extended opcode.  We only check this when we are in
         the main relaxation loop, when SEC is NULL.  */
      if ((val & ((1 << op->shift) - 1)) != 0 && sec == NULL)
	{
	  fragp->fr_subtype =
	    RELAX_MIPS16_MARK_LONG_BRANCH (fragp->fr_subtype);
	  return 1;
	}

      /* If we are about to mark a frag as extended because the value
         is precisely maxtiny + 1, then there is a chance of an
         infinite loop as in the following code:
	     la	$4,foo
	     .skip	1020
	     .align	2
	   foo:
	 In this case when the la is extended, foo is 0x3fc bytes
	 away, so the la can be shrunk, but then foo is 0x400 away, so
	 the la must be extended.  To avoid this loop, we mark the
	 frag as extended if it was small, and is about to become
	 extended with a value of maxtiny + 1.  */
      if (val == ((maxtiny + 1) << op->shift)
	  && ! RELAX_MIPS16_EXTENDED (fragp->fr_subtype)
	  && sec == NULL)
	{
	  fragp->fr_subtype =
	    RELAX_MIPS16_MARK_LONG_BRANCH (fragp->fr_subtype);
	  return 1;
	}
    }
  else if (symsec != absolute_section && sec != NULL)
    as_bad_where (fragp->fr_file, fragp->fr_line, _("unsupported relocation"));

  if ((val & ((1 << op->shift) - 1)) != 0
      || val < (mintiny << op->shift)
      || val > (maxtiny << op->shift))
    return 1;
  else
    return 0;
}

/* Compute the length of a branch sequence, and adjust the
   RELAX_BRANCH_TOOFAR bit accordingly.  If FRAGP is NULL, the
   worst-case length is computed, with UPDATE being used to indicate
   whether an unconditional (-1), branch-likely (+1) or regular (0)
   branch is to be computed.  */
static int
relaxed_branch_length (fragp, sec, update)
     fragS *fragp;
     asection *sec;
     int update;
{
  boolean toofar;
  int length;

  if (fragp
      && S_IS_DEFINED (fragp->fr_symbol)
      && sec == S_GET_SEGMENT (fragp->fr_symbol))
    {
      addressT addr;
      offsetT val;

      val = S_GET_VALUE (fragp->fr_symbol) + fragp->fr_offset;

      addr = fragp->fr_address + fragp->fr_fix + 4;

      val -= addr;

      toofar = val < - (0x8000 << 2) || val >= (0x8000 << 2);
    }
  else if (fragp)
    /* If the symbol is not defined or it's in a different segment,
       assume the user knows what's going on and emit a short
       branch.  */
    toofar = false;
  else
    toofar = true;

  if (fragp && update && toofar != RELAX_BRANCH_TOOFAR (fragp->fr_subtype))
    fragp->fr_subtype
      = RELAX_BRANCH_ENCODE (RELAX_BRANCH_RELOC_S2 (fragp->fr_subtype),
			     RELAX_BRANCH_UNCOND (fragp->fr_subtype),
			     RELAX_BRANCH_LIKELY (fragp->fr_subtype),
			     RELAX_BRANCH_LINK (fragp->fr_subtype),
			     toofar);

  length = 4;
  if (toofar)
    {
      if (fragp ? RELAX_BRANCH_LIKELY (fragp->fr_subtype) : (update > 0))
	length += 8;

      if (mips_pic != NO_PIC)
	{
	  /* Additional space for PIC loading of target address.  */
	  length += 8;
	  if (mips_opts.isa == ISA_MIPS1)
	    /* Additional space for $at-stabilizing nop.  */
	    length += 4;
	}

      /* If branch is conditional.  */
      if (fragp ? !RELAX_BRANCH_UNCOND (fragp->fr_subtype) : (update >= 0))
	length += 8;
    }
  
  return length;
}

/* Estimate the size of a frag before relaxing.  Unless this is the
   mips16, we are not really relaxing here, and the final size is
   encoded in the subtype information.  For the mips16, we have to
   decide whether we are using an extended opcode or not.  */

int
md_estimate_size_before_relax (fragp, segtype)
     fragS *fragp;
     asection *segtype;
{
  int change = 0;
  boolean linkonce = false;

  if (RELAX_BRANCH_P (fragp->fr_subtype))
    {

      fragp->fr_var = relaxed_branch_length (fragp, segtype, false);
      
      return fragp->fr_var;
    }

  if (RELAX_MIPS16_P (fragp->fr_subtype))
    /* We don't want to modify the EXTENDED bit here; it might get us
       into infinite loops.  We change it only in mips_relax_frag().  */
    return (RELAX_MIPS16_EXTENDED (fragp->fr_subtype) ? 4 : 2);

  if (mips_pic == NO_PIC)
    {
      change = nopic_need_relax (fragp->fr_symbol, 0);
    }
  else if (mips_pic == SVR4_PIC)
    {
      symbolS *sym;
      asection *symsec;

      sym = fragp->fr_symbol;

      /* Handle the case of a symbol equated to another symbol.  */
      while (symbol_equated_reloc_p (sym))
	{
	  symbolS *n;

	  /* It's possible to get a loop here in a badly written
             program.  */
	  n = symbol_get_value_expression (sym)->X_add_symbol;
	  if (n == sym)
	    break;
	  sym = n;
	}

      symsec = S_GET_SEGMENT (sym);

      /* duplicate the test for LINK_ONCE sections as in adjust_reloc_syms */
      if (symsec != segtype && ! S_IS_LOCAL (sym))
	{
	  if ((bfd_get_section_flags (stdoutput, symsec) & SEC_LINK_ONCE)
	      != 0)
	    linkonce = true;

	  /* The GNU toolchain uses an extension for ELF: a section
	     beginning with the magic string .gnu.linkonce is a linkonce
	     section.  */
	  if (strncmp (segment_name (symsec), ".gnu.linkonce",
		       sizeof ".gnu.linkonce" - 1) == 0)
	    linkonce = true;
	}

      /* This must duplicate the test in adjust_reloc_syms.  */
      change = (symsec != &bfd_und_section
		&& symsec != &bfd_abs_section
		&& ! bfd_is_com_section (symsec)
		&& !linkonce
#ifdef OBJ_ELF
		/* A global or weak symbol is treated as external.  */
	  	&& (OUTPUT_FLAVOR != bfd_target_elf_flavour
		    || (! S_IS_WEAK (sym)
			&& (! S_IS_EXTERNAL (sym)
			    || mips_pic == EMBEDDED_PIC)))
#endif
		);
    }
  else
    abort ();

  if (change)
    {
      /* Record the offset to the first reloc in the fr_opcode field.
	 This lets md_convert_frag and tc_gen_reloc know that the code
	 must be expanded.  */
      fragp->fr_opcode = (fragp->fr_literal
			  + fragp->fr_fix
			  - RELAX_OLD (fragp->fr_subtype)
			  + RELAX_RELOC1 (fragp->fr_subtype));
      /* FIXME: This really needs as_warn_where.  */
      if (RELAX_WARN (fragp->fr_subtype))
	as_warn (_("AT used after \".set noat\" or macro used after "
		   "\".set nomacro\""));

      return RELAX_NEW (fragp->fr_subtype) - RELAX_OLD (fragp->fr_subtype);
    }

  return 0;
}

/* This is called to see whether a reloc against a defined symbol
   should be converted into a reloc against a section.  Don't adjust
   MIPS16 jump relocations, so we don't have to worry about the format
   of the offset in the .o file.  Don't adjust relocations against
   mips16 symbols, so that the linker can find them if it needs to set
   up a stub.  */

int
mips_fix_adjustable (fixp)
     fixS *fixp;
{
  if (fixp->fx_r_type == BFD_RELOC_MIPS16_JMP)
    return 0;

  if (fixp->fx_r_type == BFD_RELOC_VTABLE_INHERIT
      || fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    return 0;

  if (fixp->fx_addsy == NULL)
    return 1;

#ifdef OBJ_ELF
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour
      && S_GET_OTHER (fixp->fx_addsy) == STO_MIPS16
      && fixp->fx_subsy == NULL)
    return 0;
#endif

  return 1;
}

/* Translate internal representation of relocation info to BFD target
   format.  */

arelent **
tc_gen_reloc (section, fixp)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixp;
{
  static arelent *retval[4];
  arelent *reloc;
  bfd_reloc_code_real_type code;

  reloc = retval[0] = (arelent *) xmalloc (sizeof (arelent));
  retval[1] = NULL;

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  if (mips_pic == EMBEDDED_PIC
      && SWITCH_TABLE (fixp))
    {
      /* For a switch table entry we use a special reloc.  The addend
	 is actually the difference between the reloc address and the
	 subtrahend.  */
      reloc->addend = reloc->address - S_GET_VALUE (fixp->fx_subsy);
      if (OUTPUT_FLAVOR != bfd_target_ecoff_flavour)
	as_fatal (_("Double check fx_r_type in tc-mips.c:tc_gen_reloc"));
      fixp->fx_r_type = BFD_RELOC_GPREL32;
    }
  else if (fixp->fx_r_type == BFD_RELOC_PCREL_LO16)
    {
      if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
	reloc->addend = fixp->fx_addnumber;
      else
	{
	  /* We use a special addend for an internal RELLO reloc.  */
	  if (symbol_section_p (fixp->fx_addsy))
	    reloc->addend = reloc->address - S_GET_VALUE (fixp->fx_subsy);
	  else
	    reloc->addend = fixp->fx_addnumber + reloc->address;
	}
    }
  else if (fixp->fx_r_type == BFD_RELOC_PCREL_HI16_S)
    {
      assert (fixp->fx_next != NULL
	      && fixp->fx_next->fx_r_type == BFD_RELOC_PCREL_LO16);

      /* The reloc is relative to the RELLO; adjust the addend
	 accordingly.  */
      if (OUTPUT_FLAVOR == bfd_target_elf_flavour)
	reloc->addend = fixp->fx_next->fx_addnumber;
      else
	{
	  /* We use a special addend for an internal RELHI reloc.  */
	  if (symbol_section_p (fixp->fx_addsy))
	    reloc->addend = (fixp->fx_next->fx_frag->fr_address
			     + fixp->fx_next->fx_where
			     - S_GET_VALUE (fixp->fx_subsy));
	  else
	    reloc->addend = (fixp->fx_addnumber
			     + fixp->fx_next->fx_frag->fr_address
			     + fixp->fx_next->fx_where);
	}
    }
  else if (fixp->fx_pcrel == 0 || OUTPUT_FLAVOR == bfd_target_elf_flavour)
    reloc->addend = fixp->fx_addnumber;
  else
    {
      if (OUTPUT_FLAVOR != bfd_target_aout_flavour)
	/* A gruesome hack which is a result of the gruesome gas reloc
	   handling.  */
	reloc->addend = reloc->address;
      else
	reloc->addend = -reloc->address;
    }

  /* If this is a variant frag, we may need to adjust the existing
     reloc and generate a new one.  */
  if (fixp->fx_frag->fr_opcode != NULL
      && ((fixp->fx_r_type == BFD_RELOC_GPREL16
	   && ! HAVE_NEWABI)
	  || fixp->fx_r_type == BFD_RELOC_MIPS_GOT16
	  || fixp->fx_r_type == BFD_RELOC_MIPS_CALL16
	  || fixp->fx_r_type == BFD_RELOC_MIPS_GOT_HI16
	  || fixp->fx_r_type == BFD_RELOC_MIPS_GOT_LO16
	  || fixp->fx_r_type == BFD_RELOC_MIPS_CALL_HI16
	  || fixp->fx_r_type == BFD_RELOC_MIPS_CALL_LO16)
    )
    {
      arelent *reloc2;

      assert (! RELAX_MIPS16_P (fixp->fx_frag->fr_subtype));

      /* If this is not the last reloc in this frag, then we have two
	 GPREL relocs, or a GOT_HI16/GOT_LO16 pair, or a
	 CALL_HI16/CALL_LO16, both of which are being replaced.  Let
	 the second one handle all of them.  */
      if (fixp->fx_next != NULL
	  && fixp->fx_frag == fixp->fx_next->fx_frag)
	{
	  assert ((fixp->fx_r_type == BFD_RELOC_GPREL16
		   && fixp->fx_next->fx_r_type == BFD_RELOC_GPREL16)
		  || (fixp->fx_r_type == BFD_RELOC_MIPS_GOT_HI16
		      && (fixp->fx_next->fx_r_type
			  == BFD_RELOC_MIPS_GOT_LO16))
		  || (fixp->fx_r_type == BFD_RELOC_MIPS_CALL_HI16
		      && (fixp->fx_next->fx_r_type
			  == BFD_RELOC_MIPS_CALL_LO16)));
	  retval[0] = NULL;
	  return retval;
	}

      fixp->fx_where = fixp->fx_frag->fr_opcode - fixp->fx_frag->fr_literal;
      reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;
      reloc2 = retval[1] = (arelent *) xmalloc (sizeof (arelent));
      retval[2] = NULL;
      reloc2->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
      *reloc2->sym_ptr_ptr = symbol_get_bfdsym (fixp->fx_addsy);
      reloc2->address = (reloc->address
			 + (RELAX_RELOC2 (fixp->fx_frag->fr_subtype)
			    - RELAX_RELOC1 (fixp->fx_frag->fr_subtype)));
      reloc2->addend = fixp->fx_addnumber;
      reloc2->howto = bfd_reloc_type_lookup (stdoutput, BFD_RELOC_LO16);
      assert (reloc2->howto != NULL);

      if (RELAX_RELOC3 (fixp->fx_frag->fr_subtype))
	{
	  arelent *reloc3;

	  reloc3 = retval[2] = (arelent *) xmalloc (sizeof (arelent));
	  retval[3] = NULL;
	  *reloc3 = *reloc2;
	  reloc3->address += 4;
	}

      if (mips_pic == NO_PIC)
	{
	  assert (fixp->fx_r_type == BFD_RELOC_GPREL16);
	  fixp->fx_r_type = BFD_RELOC_HI16_S;
	}
      else if (mips_pic == SVR4_PIC)
	{
	  switch (fixp->fx_r_type)
	    {
	    default:
	      abort ();
	    case BFD_RELOC_MIPS_GOT16:
	      break;
	    case BFD_RELOC_MIPS_GOT_LO16:
	    case BFD_RELOC_MIPS_CALL_LO16:
	      fixp->fx_r_type = BFD_RELOC_MIPS_GOT16;
	      break;
	    case BFD_RELOC_MIPS_CALL16:
	      if (HAVE_NEWABI)
		{
		  /* BFD_RELOC_MIPS_GOT16;*/
		  fixp->fx_r_type = BFD_RELOC_MIPS_GOT_PAGE;
		  reloc2->howto = bfd_reloc_type_lookup
		    (stdoutput, BFD_RELOC_MIPS_GOT_OFST);
		}
	      else
		fixp->fx_r_type = BFD_RELOC_MIPS_GOT16;
	      break;
	    }
	}
      else
	abort ();

      /* newabi uses R_MIPS_GOT_DISP for local symbols */
      if (HAVE_NEWABI && BFD_RELOC_MIPS_GOT_LO16)
	{
	  fixp->fx_r_type = BFD_RELOC_MIPS_GOT_DISP;
	  retval[1] = NULL;
	}
    }

  /* Since the old MIPS ELF ABI uses Rel instead of Rela, encode the vtable
     entry to be used in the relocation's section offset.  */
  if (! HAVE_NEWABI && fixp->fx_r_type == BFD_RELOC_VTABLE_ENTRY)
    {
      reloc->address = reloc->addend;
      reloc->addend = 0;
    }

  /* Since DIFF_EXPR_OK is defined in tc-mips.h, it is possible that
     fixup_segment converted a non-PC relative reloc into a PC
     relative reloc.  In such a case, we need to convert the reloc
     code.  */
  code = fixp->fx_r_type;
  if (fixp->fx_pcrel)
    {
      switch (code)
	{
	case BFD_RELOC_8:
	  code = BFD_RELOC_8_PCREL;
	  break;
	case BFD_RELOC_16:
	  code = BFD_RELOC_16_PCREL;
	  break;
	case BFD_RELOC_32:
	  code = BFD_RELOC_32_PCREL;
	  break;
	case BFD_RELOC_64:
	  code = BFD_RELOC_64_PCREL;
	  break;
	case BFD_RELOC_8_PCREL:
	case BFD_RELOC_16_PCREL:
	case BFD_RELOC_32_PCREL:
	case BFD_RELOC_64_PCREL:
	case BFD_RELOC_16_PCREL_S2:
	case BFD_RELOC_PCREL_HI16_S:
	case BFD_RELOC_PCREL_LO16:
	  break;
	default:
	  as_bad_where (fixp->fx_file, fixp->fx_line,
			_("Cannot make %s relocation PC relative"),
			bfd_get_reloc_code_name (code));
	}
    }

#ifdef OBJ_ELF
  /* md_apply_fix3 has a double-subtraction hack to get
     bfd_install_relocation to behave nicely.  GPREL relocations are
     handled correctly without this hack, so undo it here.  We can't
     stop md_apply_fix3 from subtracting twice in the first place since
     the fake addend is required for variant frags above.  */
  if (fixp->fx_addsy != NULL && OUTPUT_FLAVOR == bfd_target_elf_flavour
      && (code == BFD_RELOC_GPREL16 || code == BFD_RELOC_MIPS16_GPREL)
      && reloc->addend != 0
      && mips_need_elf_addend_fixup (fixp))
    reloc->addend += S_GET_VALUE (fixp->fx_addsy);
#endif

  /* To support a PC relative reloc when generating embedded PIC code
     for ECOFF, we use a Cygnus extension.  We check for that here to
     make sure that we don't let such a reloc escape normally.  */
  if ((OUTPUT_FLAVOR == bfd_target_ecoff_flavour
       || OUTPUT_FLAVOR == bfd_target_elf_flavour)
      && code == BFD_RELOC_16_PCREL_S2
      && mips_pic != EMBEDDED_PIC)
    reloc->howto = NULL;
  else
    reloc->howto = bfd_reloc_type_lookup (stdoutput, code);

  if (reloc->howto == NULL)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    _("Can not represent %s relocation in this object file format"),
		    bfd_get_reloc_code_name (code));
      retval[0] = NULL;
    }

  return retval;
}

/* Relax a machine dependent frag.  This returns the amount by which
   the current size of the frag should change.  */

int
mips_relax_frag (sec, fragp, stretch)
     asection *sec;
     fragS *fragp;
     long stretch;
{
  if (RELAX_BRANCH_P (fragp->fr_subtype))
    {
      offsetT old_var = fragp->fr_var;
      
      fragp->fr_var = relaxed_branch_length (fragp, sec, true);

      return fragp->fr_var - old_var;
    }

  if (! RELAX_MIPS16_P (fragp->fr_subtype))
    return 0;

  if (mips16_extended_frag (fragp, NULL, stretch))
    {
      if (RELAX_MIPS16_EXTENDED (fragp->fr_subtype))
	return 0;
      fragp->fr_subtype = RELAX_MIPS16_MARK_EXTENDED (fragp->fr_subtype);
      return 2;
    }
  else
    {
      if (! RELAX_MIPS16_EXTENDED (fragp->fr_subtype))
	return 0;
      fragp->fr_subtype = RELAX_MIPS16_CLEAR_EXTENDED (fragp->fr_subtype);
      return -2;
    }

  return 0;
}

/* Convert a machine dependent frag.  */

void
md_convert_frag (abfd, asec, fragp)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT asec;
     fragS *fragp;
{
  int old, new;
  char *fixptr;

  if (RELAX_BRANCH_P (fragp->fr_subtype))
    {
      bfd_byte *buf;
      unsigned long insn;
      expressionS exp;
      fixS *fixp;
      
      buf = (bfd_byte *)fragp->fr_literal + fragp->fr_fix;

      if (target_big_endian)
	insn = bfd_getb32 (buf);
      else
	insn = bfd_getl32 (buf);
	  
      if (!RELAX_BRANCH_TOOFAR (fragp->fr_subtype))
	{
	  /* We generate a fixup instead of applying it right now
	     because, if there are linker relaxations, we're going to
	     need the relocations.  */
	  exp.X_op = O_symbol;
	  exp.X_add_symbol = fragp->fr_symbol;
	  exp.X_add_number = fragp->fr_offset;

	  fixp = fix_new_exp (fragp, buf - (bfd_byte *)fragp->fr_literal,
			      4, &exp, 1,
			      RELAX_BRANCH_RELOC_S2 (fragp->fr_subtype)
			      ? BFD_RELOC_16_PCREL_S2
			      : BFD_RELOC_16_PCREL);
	  fixp->fx_file = fragp->fr_file;
	  fixp->fx_line = fragp->fr_line;
	  
	  md_number_to_chars ((char *)buf, insn, 4);
	  buf += 4;
	}
      else
	{
	  int i;

	  as_warn_where (fragp->fr_file, fragp->fr_line,
			 _("relaxed out-of-range branch into a jump"));

	  if (RELAX_BRANCH_UNCOND (fragp->fr_subtype))
	    goto uncond;

	  if (!RELAX_BRANCH_LIKELY (fragp->fr_subtype))
	    {
	      /* Reverse the branch.  */
	      switch ((insn >> 28) & 0xf)
		{
		case 4:
		  /* bc[0-3][tf]l? and bc1any[24][ft] instructions can
		     have the condition reversed by tweaking a single
		     bit, and their opcodes all have 0x4???????.  */
		  assert ((insn & 0xf1000000) == 0x41000000);
		  insn ^= 0x00010000;
		  break;

		case 0:
		  /* bltz	0x04000000	bgez	0x04010000
		     bltzal	0x04100000	bgezal	0x04110000 */
		  assert ((insn & 0xfc0e0000) == 0x04000000);
		  insn ^= 0x00010000;
		  break;
		  
		case 1:
		  /* beq	0x10000000	bne	0x14000000
		     blez	0x18000000	bgtz	0x1c000000 */
		  insn ^= 0x04000000;
		  break;

		default:
		  abort ();
		}
	    }

	  if (RELAX_BRANCH_LINK (fragp->fr_subtype))
	    {
	      /* Clear the and-link bit.  */
	      assert ((insn & 0xfc1c0000) == 0x04100000);

	      /* bltzal	0x04100000	bgezal	0x04110000
		bltzall	0x04120000     bgezall	0x04130000 */
	      insn &= ~0x00100000;
	    }

	  /* Branch over the branch (if the branch was likely) or the
	     full jump (not likely case).  Compute the offset from the
	     current instruction to branch to.  */
	  if (RELAX_BRANCH_LIKELY (fragp->fr_subtype))
	    i = 16;
	  else
	    {
	      /* How many bytes in instructions we've already emitted?  */
	      i = buf - (bfd_byte *)fragp->fr_literal - fragp->fr_fix;
	      /* How many bytes in instructions from here to the end?  */
	      i = fragp->fr_var - i;
	    }
	  /* Convert to instruction count.  */
	  i >>= 2;
	  /* Branch counts from the next instruction.  */
	  i--; 
	  insn |= i;
	  /* Branch over the jump.  */
	  md_number_to_chars ((char *)buf, insn, 4);
	  buf += 4;

	  /* Nop */
	  md_number_to_chars ((char*)buf, 0, 4);
	  buf += 4;

	  if (RELAX_BRANCH_LIKELY (fragp->fr_subtype))
	    {
	      /* beql $0, $0, 2f */
	      insn = 0x50000000;
	      /* Compute the PC offset from the current instruction to
		 the end of the variable frag.  */
	      /* How many bytes in instructions we've already emitted?  */
	      i = buf - (bfd_byte *)fragp->fr_literal - fragp->fr_fix;
	      /* How many bytes in instructions from here to the end?  */
	      i = fragp->fr_var - i;
	      /* Convert to instruction count.  */
	      i >>= 2;
	      /* Don't decrement i, because we want to branch over the
		 delay slot.  */

	      insn |= i;
	      md_number_to_chars ((char *)buf, insn, 4);
	      buf += 4;

	      md_number_to_chars ((char *)buf, 0, 4);
	      buf += 4;
	    }

	uncond:
	  if (mips_pic == NO_PIC)
	    {
	      /* j or jal.  */
	      insn = (RELAX_BRANCH_LINK (fragp->fr_subtype)
		      ? 0x0c000000 : 0x08000000);
	      exp.X_op = O_symbol;
	      exp.X_add_symbol = fragp->fr_symbol;
	      exp.X_add_number = fragp->fr_offset;

	      fixp = fix_new_exp (fragp, buf - (bfd_byte *)fragp->fr_literal,
				  4, &exp, 0, BFD_RELOC_MIPS_JMP);
	      fixp->fx_file = fragp->fr_file;
	      fixp->fx_line = fragp->fr_line;

	      md_number_to_chars ((char*)buf, insn, 4);
	      buf += 4;
	    }
	  else
	    {
	      /* lw/ld $at, <sym>($gp)  R_MIPS_GOT16 */
	      insn = HAVE_64BIT_ADDRESSES ? 0xdf810000 : 0x8f810000;
	      exp.X_op = O_symbol;
	      exp.X_add_symbol = fragp->fr_symbol;
	      exp.X_add_number = fragp->fr_offset;

	      if (fragp->fr_offset)
		{
		  exp.X_add_symbol = make_expr_symbol (&exp);
		  exp.X_add_number = 0;
		}

	      fixp = fix_new_exp (fragp, buf - (bfd_byte *)fragp->fr_literal,
				  4, &exp, 0, BFD_RELOC_MIPS_GOT16);
	      fixp->fx_file = fragp->fr_file;
	      fixp->fx_line = fragp->fr_line;

	      md_number_to_chars ((char*)buf, insn, 4);
	      buf += 4;
	      
	      if (mips_opts.isa == ISA_MIPS1)
		{
		  /* nop */
		  md_number_to_chars ((char*)buf, 0, 4);
		  buf += 4;
		}

	      /* d/addiu $at, $at, <sym>  R_MIPS_LO16 */
	      insn = HAVE_64BIT_ADDRESSES ? 0x64210000 : 0x24210000;

	      fixp = fix_new_exp (fragp, buf - (bfd_byte *)fragp->fr_literal,
				  4, &exp, 0, BFD_RELOC_LO16);
	      fixp->fx_file = fragp->fr_file;
	      fixp->fx_line = fragp->fr_line;
	      
	      md_number_to_chars ((char*)buf, insn, 4);
	      buf += 4;

	      /* j(al)r $at.  */
	      if (RELAX_BRANCH_LINK (fragp->fr_subtype))
		insn = 0x0020f809;
	      else
		insn = 0x00200008;

	      md_number_to_chars ((char*)buf, insn, 4);
	      buf += 4;
	    }
	}

      assert (buf == (bfd_byte *)fragp->fr_literal
	      + fragp->fr_fix + fragp->fr_var);

      fragp->fr_fix += fragp->fr_var;

      return;
    }

  if (RELAX_MIPS16_P (fragp->fr_subtype))
    {
      int type;
      register const struct mips16_immed_operand *op;
      boolean small, ext;
      offsetT val;
      bfd_byte *buf;
      unsigned long insn;
      boolean use_extend;
      unsigned short extend;

      type = RELAX_MIPS16_TYPE (fragp->fr_subtype);
      op = mips16_immed_operands;
      while (op->type != type)
	++op;

      if (RELAX_MIPS16_EXTENDED (fragp->fr_subtype))
	{
	  small = false;
	  ext = true;
	}
      else
	{
	  small = true;
	  ext = false;
	}

      resolve_symbol_value (fragp->fr_symbol);
      val = S_GET_VALUE (fragp->fr_symbol);
      if (op->pcrel)
	{
	  addressT addr;

	  addr = fragp->fr_address + fragp->fr_fix;

	  /* The rules for the base address of a PC relative reloc are
             complicated; see mips16_extended_frag.  */
	  if (type == 'p' || type == 'q')
	    {
	      addr += 2;
	      if (ext)
		addr += 2;
	      /* Ignore the low bit in the target, since it will be
                 set for a text label.  */
	      if ((val & 1) != 0)
		--val;
	    }
	  else if (RELAX_MIPS16_JAL_DSLOT (fragp->fr_subtype))
	    addr -= 4;
	  else if (RELAX_MIPS16_DSLOT (fragp->fr_subtype))
	    addr -= 2;

	  addr &= ~ (addressT) ((1 << op->shift) - 1);
	  val -= addr;

	  /* Make sure the section winds up with the alignment we have
             assumed.  */
	  if (op->shift > 0)
	    record_alignment (asec, op->shift);
	}

      if (ext
	  && (RELAX_MIPS16_JAL_DSLOT (fragp->fr_subtype)
	      || RELAX_MIPS16_DSLOT (fragp->fr_subtype)))
	as_warn_where (fragp->fr_file, fragp->fr_line,
		       _("extended instruction in delay slot"));

      buf = (bfd_byte *) (fragp->fr_literal + fragp->fr_fix);

      if (target_big_endian)
	insn = bfd_getb16 (buf);
      else
	insn = bfd_getl16 (buf);

      mips16_immed (fragp->fr_file, fragp->fr_line, type, val,
		    RELAX_MIPS16_USER_EXT (fragp->fr_subtype),
		    small, ext, &insn, &use_extend, &extend);

      if (use_extend)
	{
	  md_number_to_chars ((char *) buf, 0xf000 | extend, 2);
	  fragp->fr_fix += 2;
	  buf += 2;
	}

      md_number_to_chars ((char *) buf, insn, 2);
      fragp->fr_fix += 2;
      buf += 2;
    }
  else
    {
      if (fragp->fr_opcode == NULL)
	return;

      old = RELAX_OLD (fragp->fr_subtype);
      new = RELAX_NEW (fragp->fr_subtype);
      fixptr = fragp->fr_literal + fragp->fr_fix;

      if (new > 0)
	memcpy (fixptr - old, fixptr, new);

      fragp->fr_fix += new - old;
    }
}

#ifdef OBJ_ELF

/* This function is called after the relocs have been generated.
   We've been storing mips16 text labels as odd.  Here we convert them
   back to even for the convenience of the debugger.  */

void
mips_frob_file_after_relocs ()
{
  asymbol **syms;
  unsigned int count, i;

  if (OUTPUT_FLAVOR != bfd_target_elf_flavour)
    return;

  syms = bfd_get_outsymbols (stdoutput);
  count = bfd_get_symcount (stdoutput);
  for (i = 0; i < count; i++, syms++)
    {
      if (elf_symbol (*syms)->internal_elf_sym.st_other == STO_MIPS16
	  && ((*syms)->value & 1) != 0)
	{
	  (*syms)->value &= ~1;
	  /* If the symbol has an odd size, it was probably computed
	     incorrectly, so adjust that as well.  */
	  if ((elf_symbol (*syms)->internal_elf_sym.st_size & 1) != 0)
	    ++elf_symbol (*syms)->internal_elf_sym.st_size;
	}
    }
}

#endif

/* This function is called whenever a label is defined.  It is used
   when handling branch delays; if a branch has a label, we assume we
   can not move it.  */

void
mips_define_label (sym)
     symbolS *sym;
{
  struct insn_label_list *l;

  if (free_insn_labels == NULL)
    l = (struct insn_label_list *) xmalloc (sizeof *l);
  else
    {
      l = free_insn_labels;
      free_insn_labels = l->next;
    }

  l->label = sym;
  l->next = insn_labels;
  insn_labels = l;
}

#if defined (OBJ_ELF) || defined (OBJ_MAYBE_ELF)

/* Some special processing for a MIPS ELF file.  */

void
mips_elf_final_processing ()
{
  /* Write out the register information.  */
  if (mips_abi != N64_ABI)
    {
      Elf32_RegInfo s;

      s.ri_gprmask = mips_gprmask;
      s.ri_cprmask[0] = mips_cprmask[0];
      s.ri_cprmask[1] = mips_cprmask[1];
      s.ri_cprmask[2] = mips_cprmask[2];
      s.ri_cprmask[3] = mips_cprmask[3];
      /* The gp_value field is set by the MIPS ELF backend.  */

      bfd_mips_elf32_swap_reginfo_out (stdoutput, &s,
				       ((Elf32_External_RegInfo *)
					mips_regmask_frag));
    }
  else
    {
      Elf64_Internal_RegInfo s;

      s.ri_gprmask = mips_gprmask;
      s.ri_pad = 0;
      s.ri_cprmask[0] = mips_cprmask[0];
      s.ri_cprmask[1] = mips_cprmask[1];
      s.ri_cprmask[2] = mips_cprmask[2];
      s.ri_cprmask[3] = mips_cprmask[3];
      /* The gp_value field is set by the MIPS ELF backend.  */

      bfd_mips_elf64_swap_reginfo_out (stdoutput, &s,
				       ((Elf64_External_RegInfo *)
					mips_regmask_frag));
    }

  /* Set the MIPS ELF flag bits.  FIXME: There should probably be some
     sort of BFD interface for this.  */
  if (mips_any_noreorder)
    elf_elfheader (stdoutput)->e_flags |= EF_MIPS_NOREORDER;
  if (mips_pic != NO_PIC)
    elf_elfheader (stdoutput)->e_flags |= EF_MIPS_PIC;

  /* Set MIPS ELF flags for ASEs.  */
  if (file_ase_mips16)
    elf_elfheader (stdoutput)->e_flags |= EF_MIPS_ARCH_ASE_M16;
#if 0 /* XXX FIXME */
  if (file_ase_mips3d)
    elf_elfheader (stdoutput)->e_flags |= ???;
#endif
  if (file_ase_mdmx)
    elf_elfheader (stdoutput)->e_flags |= EF_MIPS_ARCH_ASE_MDMX;

  /* Set the MIPS ELF ABI flags.  */
  if (mips_abi == O32_ABI && USE_E_MIPS_ABI_O32)
    elf_elfheader (stdoutput)->e_flags |= E_MIPS_ABI_O32;
  else if (mips_abi == O64_ABI)
    elf_elfheader (stdoutput)->e_flags |= E_MIPS_ABI_O64;
  else if (mips_abi == EABI_ABI)
    {
      if (!file_mips_gp32)
	elf_elfheader (stdoutput)->e_flags |= E_MIPS_ABI_EABI64;
      else
	elf_elfheader (stdoutput)->e_flags |= E_MIPS_ABI_EABI32;
    }
  else if (mips_abi == N32_ABI)
    elf_elfheader (stdoutput)->e_flags |= EF_MIPS_ABI2;

  /* Nothing to do for N64_ABI.  */

  if (mips_32bitmode)
    elf_elfheader (stdoutput)->e_flags |= EF_MIPS_32BITMODE;
}

#endif /* OBJ_ELF || OBJ_MAYBE_ELF */

typedef struct proc {
  symbolS *isym;
  unsigned long reg_mask;
  unsigned long reg_offset;
  unsigned long fpreg_mask;
  unsigned long fpreg_offset;
  unsigned long frame_offset;
  unsigned long frame_reg;
  unsigned long pc_reg;
} procS;

static procS cur_proc;
static procS *cur_proc_ptr;
static int numprocs;

/* Fill in an rs_align_code fragment.  */

void
mips_handle_align (fragp)
     fragS *fragp;
{
  if (fragp->fr_type != rs_align_code)
    return;

  if (mips_opts.mips16)
    {
      static const unsigned char be_nop[] = { 0x65, 0x00 };
      static const unsigned char le_nop[] = { 0x00, 0x65 };

      int bytes;
      char *p;

      bytes = fragp->fr_next->fr_address - fragp->fr_address - fragp->fr_fix;
      p = fragp->fr_literal + fragp->fr_fix;

      if (bytes & 1)
	{
	  *p++ = 0;
	  fragp->fr_fix++;
	}

      memcpy (p, (target_big_endian ? be_nop : le_nop), 2);
      fragp->fr_var = 2;
    }

  /* For mips32, a nop is a zero, which we trivially get by doing nothing.  */
}

static void
md_obj_begin ()
{
}

static void
md_obj_end ()
{
  /* check for premature end, nesting errors, etc */
  if (cur_proc_ptr)
    as_warn (_("missing .end at end of assembly"));
}

static long
get_number ()
{
  int negative = 0;
  long val = 0;

  if (*input_line_pointer == '-')
    {
      ++input_line_pointer;
      negative = 1;
    }
  if (!ISDIGIT (*input_line_pointer))
    as_bad (_("expected simple number"));
  if (input_line_pointer[0] == '0')
    {
      if (input_line_pointer[1] == 'x')
	{
	  input_line_pointer += 2;
	  while (ISXDIGIT (*input_line_pointer))
	    {
	      val <<= 4;
	      val |= hex_value (*input_line_pointer++);
	    }
	  return negative ? -val : val;
	}
      else
	{
	  ++input_line_pointer;
	  while (ISDIGIT (*input_line_pointer))
	    {
	      val <<= 3;
	      val |= *input_line_pointer++ - '0';
	    }
	  return negative ? -val : val;
	}
    }
  if (!ISDIGIT (*input_line_pointer))
    {
      printf (_(" *input_line_pointer == '%c' 0x%02x\n"),
	      *input_line_pointer, *input_line_pointer);
      as_warn (_("invalid number"));
      return -1;
    }
  while (ISDIGIT (*input_line_pointer))
    {
      val *= 10;
      val += *input_line_pointer++ - '0';
    }
  return negative ? -val : val;
}

/* The .file directive; just like the usual .file directive, but there
   is an initial number which is the ECOFF file index.  In the non-ECOFF
   case .file implies DWARF-2.  */

static void
s_mips_file (x)
     int x ATTRIBUTE_UNUSED;
{
  static int first_file_directive = 0;

  if (ECOFF_DEBUGGING)
    {
      get_number ();
      s_app_file (0);
    }
  else
    {
      char *filename;

      filename = dwarf2_directive_file (0);

      /* Versions of GCC up to 3.1 start files with a ".file"
	 directive even for stabs output.  Make sure that this
	 ".file" is handled.  Note that you need a version of GCC
         after 3.1 in order to support DWARF-2 on MIPS.  */
      if (filename != NULL && ! first_file_directive)
	{
	  (void) new_logical_line (filename, -1);
	  s_app_file_string (filename);
	}
      first_file_directive = 1;
    }
}

/* The .loc directive, implying DWARF-2.  */

static void
s_mips_loc (x)
     int x ATTRIBUTE_UNUSED;
{
  if (!ECOFF_DEBUGGING)
    dwarf2_directive_loc (0);
}

/* The .end directive.  */

static void
s_mips_end (x)
     int x ATTRIBUTE_UNUSED;
{
  symbolS *p;
  int maybe_text;

  /* Following functions need their own .frame and .cprestore directives.  */
  mips_frame_reg_valid = 0;
  mips_cprestore_valid = 0;

  if (!is_end_of_line[(unsigned char) *input_line_pointer])
    {
      p = get_symbol ();
      demand_empty_rest_of_line ();
    }
  else
    p = NULL;

#ifdef BFD_ASSEMBLER
  if ((bfd_get_section_flags (stdoutput, now_seg) & SEC_CODE) != 0)
    maybe_text = 1;
  else
    maybe_text = 0;
#else
  if (now_seg != data_section && now_seg != bss_section)
    maybe_text = 1;
  else
    maybe_text = 0;
#endif

  if (!maybe_text)
    as_warn (_(".end not in text section"));

  if (!cur_proc_ptr)
    {
      as_warn (_(".end directive without a preceding .ent directive."));
      demand_empty_rest_of_line ();
      return;
    }

  if (p != NULL)
    {
      assert (S_GET_NAME (p));
      if (strcmp (S_GET_NAME (p), S_GET_NAME (cur_proc_ptr->isym)))
	as_warn (_(".end symbol does not match .ent symbol."));

      if (debug_type == DEBUG_STABS)
	stabs_generate_asm_endfunc (S_GET_NAME (p),
				    S_GET_NAME (p));
    }
  else
    as_warn (_(".end directive missing or unknown symbol"));

#ifdef OBJ_ELF
  /* Generate a .pdr section.  */
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour && ! ECOFF_DEBUGGING)
    {
      segT saved_seg = now_seg;
      subsegT saved_subseg = now_subseg;
      valueT dot;
      expressionS exp;
      char *fragp;

      dot = frag_now_fix ();

#ifdef md_flush_pending_output
      md_flush_pending_output ();
#endif

      assert (pdr_seg);
      subseg_set (pdr_seg, 0);

      /* Write the symbol.  */
      exp.X_op = O_symbol;
      exp.X_add_symbol = p;
      exp.X_add_number = 0;
      emit_expr (&exp, 4);

      fragp = frag_more (7 * 4);

      md_number_to_chars (fragp,      (valueT) cur_proc_ptr->reg_mask, 4);
      md_number_to_chars (fragp +  4, (valueT) cur_proc_ptr->reg_offset, 4);
      md_number_to_chars (fragp +  8, (valueT) cur_proc_ptr->fpreg_mask, 4);
      md_number_to_chars (fragp + 12, (valueT) cur_proc_ptr->fpreg_offset, 4);
      md_number_to_chars (fragp + 16, (valueT) cur_proc_ptr->frame_offset, 4);
      md_number_to_chars (fragp + 20, (valueT) cur_proc_ptr->frame_reg, 4);
      md_number_to_chars (fragp + 24, (valueT) cur_proc_ptr->pc_reg, 4);

      subseg_set (saved_seg, saved_subseg);
    }
#endif /* OBJ_ELF */

  cur_proc_ptr = NULL;
}

/* The .aent and .ent directives.  */

static void
s_mips_ent (aent)
     int aent;
{
  symbolS *symbolP;
  int maybe_text;

  symbolP = get_symbol ();
  if (*input_line_pointer == ',')
    ++input_line_pointer;
  SKIP_WHITESPACE ();
  if (ISDIGIT (*input_line_pointer)
      || *input_line_pointer == '-')
    get_number ();

#ifdef BFD_ASSEMBLER
  if ((bfd_get_section_flags (stdoutput, now_seg) & SEC_CODE) != 0)
    maybe_text = 1;
  else
    maybe_text = 0;
#else
  if (now_seg != data_section && now_seg != bss_section)
    maybe_text = 1;
  else
    maybe_text = 0;
#endif

  if (!maybe_text)
    as_warn (_(".ent or .aent not in text section."));

  if (!aent && cur_proc_ptr)
    as_warn (_("missing .end"));

  if (!aent)
    {
      /* This function needs its own .frame and .cprestore directives.  */
      mips_frame_reg_valid = 0;
      mips_cprestore_valid = 0;

      cur_proc_ptr = &cur_proc;
      memset (cur_proc_ptr, '\0', sizeof (procS));

      cur_proc_ptr->isym = symbolP;

      symbol_get_bfdsym (symbolP)->flags |= BSF_FUNCTION;

      ++numprocs;

      if (debug_type == DEBUG_STABS)
        stabs_generate_asm_func (S_GET_NAME (symbolP),
				 S_GET_NAME (symbolP));
    }

  demand_empty_rest_of_line ();
}

/* The .frame directive. If the mdebug section is present (IRIX 5 native)
   then ecoff.c (ecoff_directive_frame) is used. For embedded targets,
   s_mips_frame is used so that we can set the PDR information correctly.
   We can't use the ecoff routines because they make reference to the ecoff
   symbol table (in the mdebug section).  */

static void
s_mips_frame (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
#ifdef OBJ_ELF
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour && ! ECOFF_DEBUGGING)
    {
      long val;

      if (cur_proc_ptr == (procS *) NULL)
	{
	  as_warn (_(".frame outside of .ent"));
	  demand_empty_rest_of_line ();
	  return;
	}

      cur_proc_ptr->frame_reg = tc_get_register (1);

      SKIP_WHITESPACE ();
      if (*input_line_pointer++ != ','
	  || get_absolute_expression_and_terminator (&val) != ',')
	{
	  as_warn (_("Bad .frame directive"));
	  --input_line_pointer;
	  demand_empty_rest_of_line ();
	  return;
	}

      cur_proc_ptr->frame_offset = val;
      cur_proc_ptr->pc_reg = tc_get_register (0);

      demand_empty_rest_of_line ();
    }
  else
#endif /* OBJ_ELF */
    s_ignore (ignore);
}

/* The .fmask and .mask directives. If the mdebug section is present
   (IRIX 5 native) then ecoff.c (ecoff_directive_mask) is used. For
   embedded targets, s_mips_mask is used so that we can set the PDR
   information correctly. We can't use the ecoff routines because they
   make reference to the ecoff symbol table (in the mdebug section).  */

static void
s_mips_mask (reg_type)
     char reg_type;
{
#ifdef OBJ_ELF
  if (OUTPUT_FLAVOR == bfd_target_elf_flavour && ! ECOFF_DEBUGGING)
    {
      long mask, off;

      if (cur_proc_ptr == (procS *) NULL)
	{
	  as_warn (_(".mask/.fmask outside of .ent"));
	  demand_empty_rest_of_line ();
	  return;
	}

      if (get_absolute_expression_and_terminator (&mask) != ',')
	{
	  as_warn (_("Bad .mask/.fmask directive"));
	  --input_line_pointer;
	  demand_empty_rest_of_line ();
	  return;
	}

      off = get_absolute_expression ();

      if (reg_type == 'F')
	{
	  cur_proc_ptr->fpreg_mask = mask;
	  cur_proc_ptr->fpreg_offset = off;
	}
      else
	{
	  cur_proc_ptr->reg_mask = mask;
	  cur_proc_ptr->reg_offset = off;
	}

      demand_empty_rest_of_line ();
    }
  else
#endif /* OBJ_ELF */
    s_ignore (reg_type);
}

/* The .loc directive.  */

#if 0
static void
s_loc (x)
     int x;
{
  symbolS *symbolP;
  int lineno;
  int addroff;

  assert (now_seg == text_section);

  lineno = get_number ();
  addroff = frag_now_fix ();

  symbolP = symbol_new ("", N_SLINE, addroff, frag_now);
  S_SET_TYPE (symbolP, N_SLINE);
  S_SET_OTHER (symbolP, 0);
  S_SET_DESC (symbolP, lineno);
  symbolP->sy_segment = now_seg;
}
#endif

/* A table describing all the processors gas knows about.  Names are
   matched in the order listed.

   To ease comparison, please keep this table in the same order as
   gcc's mips_cpu_info_table[].  */
static const struct mips_cpu_info mips_cpu_info_table[] =
{
  /* Entries for generic ISAs */
  { "mips1",          1,      ISA_MIPS1,      CPU_R3000 },
  { "mips2",          1,      ISA_MIPS2,      CPU_R6000 },
  { "mips3",          1,      ISA_MIPS3,      CPU_R4000 },
  { "mips4",          1,      ISA_MIPS4,      CPU_R8000 },
  { "mips5",          1,      ISA_MIPS5,      CPU_MIPS5 },
  { "mips32",         1,      ISA_MIPS32,     CPU_MIPS32 },
  { "mips64",         1,      ISA_MIPS64,     CPU_MIPS64 },

  /* MIPS I */
  { "r3000",          0,      ISA_MIPS1,      CPU_R3000 },
  { "r2000",          0,      ISA_MIPS1,      CPU_R3000 },
  { "r3900",          0,      ISA_MIPS1,      CPU_R3900 },

  /* MIPS II */
  { "r6000",          0,      ISA_MIPS2,      CPU_R6000 },

  /* MIPS III */
  { "r4000",          0,      ISA_MIPS3,      CPU_R4000 },
  { "r4010",          0,      ISA_MIPS2,      CPU_R4010 },
  { "vr4100",         0,      ISA_MIPS3,      CPU_VR4100 },
  { "vr4111",         0,      ISA_MIPS3,      CPU_R4111 },
  { "vr4120",         0,      ISA_MIPS3,      CPU_VR4120 },
  { "vr4130",         0,      ISA_MIPS3,      CPU_VR4120 },
  { "vr4181",         0,      ISA_MIPS3,      CPU_R4111 },
  { "vr4300",         0,      ISA_MIPS3,      CPU_R4300 },
  { "r4400",          0,      ISA_MIPS3,      CPU_R4400 },
  { "r4600",          0,      ISA_MIPS3,      CPU_R4600 },
  { "orion",          0,      ISA_MIPS3,      CPU_R4600 },
  { "r4650",          0,      ISA_MIPS3,      CPU_R4650 },

  /* MIPS IV */
  { "r8000",          0,      ISA_MIPS4,      CPU_R8000 },
  { "r10000",         0,      ISA_MIPS4,      CPU_R10000 },
  { "r12000",         0,      ISA_MIPS4,      CPU_R12000 },
  { "vr5000",         0,      ISA_MIPS4,      CPU_R5000 },
  { "vr5400",         0,      ISA_MIPS4,      CPU_VR5400 },
  { "vr5500",         0,      ISA_MIPS4,      CPU_VR5500 },
  { "rm5200",         0,      ISA_MIPS4,      CPU_R5000 },
  { "rm5230",         0,      ISA_MIPS4,      CPU_R5000 },
  { "rm5231",         0,      ISA_MIPS4,      CPU_R5000 },
  { "rm5261",         0,      ISA_MIPS4,      CPU_R5000 },
  { "rm5721",         0,      ISA_MIPS4,      CPU_R5000 },
  { "r7000",          0,      ISA_MIPS4,      CPU_R5000 },

  /* MIPS 32 */
  { "4kc",            0,      ISA_MIPS32,     CPU_MIPS32, },
  { "4km",            0,      ISA_MIPS32,     CPU_MIPS32 },
  { "4kp",            0,      ISA_MIPS32,     CPU_MIPS32 },

  /* MIPS 64 */
  { "5kc",            0,      ISA_MIPS64,     CPU_MIPS64 },
  { "20kc",           0,      ISA_MIPS64,     CPU_MIPS64 },

  /* Broadcom SB-1 CPU core */
  { "sb1",            0,      ISA_MIPS64,     CPU_SB1 },

  /* End marker */
  { NULL, 0, 0, 0 }
};


/* Return true if GIVEN is the same as CANONICAL, or if it is CANONICAL
   with a final "000" replaced by "k".  Ignore case.

   Note: this function is shared between GCC and GAS.  */

static boolean
mips_strict_matching_cpu_name_p (canonical, given)
     const char *canonical, *given;
{
  while (*given != 0 && TOLOWER (*given) == TOLOWER (*canonical))
    given++, canonical++;

  return ((*given == 0 && *canonical == 0)
	  || (strcmp (canonical, "000") == 0 && strcasecmp (given, "k") == 0));
}


/* Return true if GIVEN matches CANONICAL, where GIVEN is a user-supplied
   CPU name.  We've traditionally allowed a lot of variation here.

   Note: this function is shared between GCC and GAS.  */

static boolean
mips_matching_cpu_name_p (canonical, given)
     const char *canonical, *given;
{
  /* First see if the name matches exactly, or with a final "000"
     turned into "k".  */
  if (mips_strict_matching_cpu_name_p (canonical, given))
    return true;

  /* If not, try comparing based on numerical designation alone.
     See if GIVEN is an unadorned number, or 'r' followed by a number.  */
  if (TOLOWER (*given) == 'r')
    given++;
  if (!ISDIGIT (*given))
    return false;

  /* Skip over some well-known prefixes in the canonical name,
     hoping to find a number there too.  */
  if (TOLOWER (canonical[0]) == 'v' && TOLOWER (canonical[1]) == 'r')
    canonical += 2;
  else if (TOLOWER (canonical[0]) == 'r' && TOLOWER (canonical[1]) == 'm')
    canonical += 2;
  else if (TOLOWER (canonical[0]) == 'r')
    canonical += 1;

  return mips_strict_matching_cpu_name_p (canonical, given);
}


/* Parse an option that takes the name of a processor as its argument.
   OPTION is the name of the option and CPU_STRING is the argument.
   Return the corresponding processor enumeration if the CPU_STRING is
   recognized, otherwise report an error and return null.

   A similar function exists in GCC.  */

static const struct mips_cpu_info *
mips_parse_cpu (option, cpu_string)
     const char *option, *cpu_string;
{
  const struct mips_cpu_info *p;

  /* 'from-abi' selects the most compatible architecture for the given
     ABI: MIPS I for 32-bit ABIs and MIPS III for 64-bit ABIs.  For the
     EABIs, we have to decide whether we're using the 32-bit or 64-bit
     version.  Look first at the -mgp options, if given, otherwise base
     the choice on MIPS_DEFAULT_64BIT.

     Treat NO_ABI like the EABIs.  One reason to do this is that the
     plain 'mips' and 'mips64' configs have 'from-abi' as their default
     architecture.  This code picks MIPS I for 'mips' and MIPS III for
     'mips64', just as we did in the days before 'from-abi'.  */
  if (strcasecmp (cpu_string, "from-abi") == 0)
    {
      if (ABI_NEEDS_32BIT_REGS (mips_abi))
	return mips_cpu_info_from_isa (ISA_MIPS1);

      if (ABI_NEEDS_64BIT_REGS (mips_abi))
	return mips_cpu_info_from_isa (ISA_MIPS3);

      if (file_mips_gp32 >= 0)
	return mips_cpu_info_from_isa (file_mips_gp32 ? ISA_MIPS1 : ISA_MIPS3);

      return mips_cpu_info_from_isa (MIPS_DEFAULT_64BIT
				     ? ISA_MIPS3
				     : ISA_MIPS1);
    }

  /* 'default' has traditionally been a no-op.  Probably not very useful.  */
  if (strcasecmp (cpu_string, "default") == 0)
    return 0;

  for (p = mips_cpu_info_table; p->name != 0; p++)
    if (mips_matching_cpu_name_p (p->name, cpu_string))
      return p;

  as_bad ("Bad value (%s) for %s", cpu_string, option);
  return 0;
}

/* Return the canonical processor information for ISA (a member of the
   ISA_MIPS* enumeration).  */

static const struct mips_cpu_info *
mips_cpu_info_from_isa (isa)
     int isa;
{
  int i;

  for (i = 0; mips_cpu_info_table[i].name != NULL; i++)
    if (mips_cpu_info_table[i].is_isa
	&& isa == mips_cpu_info_table[i].isa)
      return (&mips_cpu_info_table[i]);

  return NULL;
}

static void
show (stream, string, col_p, first_p)
     FILE *stream;
     const char *string;
     int *col_p;
     int *first_p;
{
  if (*first_p)
    {
      fprintf (stream, "%24s", "");
      *col_p = 24;
    }
  else
    {
      fprintf (stream, ", ");
      *col_p += 2;
    }

  if (*col_p + strlen (string) > 72)
    {
      fprintf (stream, "\n%24s", "");
      *col_p = 24;
    }

  fprintf (stream, "%s", string);
  *col_p += strlen (string);

  *first_p = 0;
}

void
md_show_usage (stream)
     FILE *stream;
{
  int column, first;
  size_t i;

  fprintf (stream, _("\
MIPS options:\n\
-membedded-pic		generate embedded position independent code\n\
-EB			generate big endian output\n\
-EL			generate little endian output\n\
-g, -g2			do not remove unneeded NOPs or swap branches\n\
-G NUM			allow referencing objects up to NUM bytes\n\
			implicitly with the gp register [default 8]\n"));
  fprintf (stream, _("\
-mips1			generate MIPS ISA I instructions\n\
-mips2			generate MIPS ISA II instructions\n\
-mips3			generate MIPS ISA III instructions\n\
-mips4			generate MIPS ISA IV instructions\n\
-mips5                  generate MIPS ISA V instructions\n\
-mips32                 generate MIPS32 ISA instructions\n\
-mips64                 generate MIPS64 ISA instructions\n\
-march=CPU/-mtune=CPU	generate code/schedule for CPU, where CPU is one of:\n"));

  first = 1;

  for (i = 0; mips_cpu_info_table[i].name != NULL; i++)
    show (stream, mips_cpu_info_table[i].name, &column, &first);
  show (stream, "from-abi", &column, &first);
  fputc ('\n', stream);

  fprintf (stream, _("\
-mCPU			equivalent to -march=CPU -mtune=CPU. Deprecated.\n\
-no-mCPU		don't generate code specific to CPU.\n\
			For -mCPU and -no-mCPU, CPU must be one of:\n"));

  first = 1;

  show (stream, "3900", &column, &first);
  show (stream, "4010", &column, &first);
  show (stream, "4100", &column, &first);
  show (stream, "4650", &column, &first);
  fputc ('\n', stream);

  fprintf (stream, _("\
-mips16			generate mips16 instructions\n\
-no-mips16		do not generate mips16 instructions\n"));
  fprintf (stream, _("\
-mgp32			use 32-bit GPRs, regardless of the chosen ISA\n\
-mfp32			use 32-bit FPRs, regardless of the chosen ISA\n\
-O0			remove unneeded NOPs, do not swap branches\n\
-O			remove unneeded NOPs and swap branches\n\
-n			warn about NOPs generated from macros\n\
--[no-]construct-floats [dis]allow floating point values to be constructed\n\
--trap, --no-break	trap exception on div by 0 and mult overflow\n\
--break, --no-trap	break exception on div by 0 and mult overflow\n"));
#ifdef OBJ_ELF
  fprintf (stream, _("\
-KPIC, -call_shared	generate SVR4 position independent code\n\
-non_shared		do not generate position independent code\n\
-xgot			assume a 32 bit GOT\n\
-mabi=ABI		create ABI conformant object file for:\n"));

  first = 1;

  show (stream, "32", &column, &first);
  show (stream, "o64", &column, &first);
  show (stream, "n32", &column, &first);
  show (stream, "64", &column, &first);
  show (stream, "eabi", &column, &first);

  fputc ('\n', stream);

  fprintf (stream, _("\
-32			create o32 ABI object file (default)\n\
-n32			create n32 ABI object file\n\
-64			create 64 ABI object file\n"));
#endif
}
