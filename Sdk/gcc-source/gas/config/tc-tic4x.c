/* tc-c4x.c -- Assemble for the Texas Instruments TMS320C[34]x.
   Copyright (C) 1997,1998, 2002 Free Software Foundation.

   Contributed by Michael P. Hayes (m.hayes@elec.canterbury.ac.nz)

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330, 
   Boston, MA 02111-1307, USA.  */


/* Things not currently implemented:
   > .usect if has symbol on previous line  

   > .sym, .eos, .stag, .etag, .member

   > Evaluation of constant floating point expressions (expr.c needs work!)

   > Warnings issued if parallel load of same register

   Note that this is primarily designed to handle the code generated
   by GCC.  Anything else is a bonus!  */

#include <stdio.h>
#include <ctype.h>

#include "as.h"
#include "opcode/tic4x.h"
#include "subsegs.h"
#include "obstack.h"
#include "symbols.h"
#include "listing.h"

/* OK, we accept a syntax similar to the other well known C30
   assembly tools.  With C4X_ALT_SYNTAX defined we are more
   flexible, allowing a more Unix-like syntax:  `%' in front of
   register names, `#' in front of immediate constants, and
   not requiring `@' in front of direct addresses.  */

#define C4X_ALT_SYNTAX

/* Equal to MAX_PRECISION in atof-ieee.c.  */
#define MAX_LITTLENUMS 6	/* (12 bytes) */

/* Handle of the inst mnemonic hash table.  */
static struct hash_control *c4x_op_hash = NULL;

/* Handle asg pseudo.  */
static struct hash_control *c4x_asg_hash = NULL;

static unsigned int c4x_cpu = 0;	/* Default to TMS320C40.  */
static unsigned int c4x_big_model = 0;	/* Default to small memory model.  */
static unsigned int c4x_reg_args = 0;	/* Default to args passed on stack.  */

typedef enum
  {
    M_UNKNOWN, M_IMMED, M_DIRECT, M_REGISTER, M_INDIRECT,
    M_IMMED_F, M_PARALLEL, M_HI
  }
c4x_addr_mode_t;

typedef struct c4x_operand
  {
    c4x_addr_mode_t mode;	/* Addressing mode.  */
    expressionS expr;		/* Expression.  */
    int disp;			/* Displacement for indirect addressing.  */
    int aregno;			/* Aux. register number.  */
    LITTLENUM_TYPE fwords[MAX_LITTLENUMS];	/* Float immed. number.  */
  }
c4x_operand_t;

typedef struct c4x_insn
  {
    char name[C4X_NAME_MAX];	/* Mnemonic of instruction.  */
    unsigned int in_use;	/* True if in_use.  */
    unsigned int parallel;	/* True if parallel instruction.  */
    unsigned int nchars;	/* This is always 4 for the C30.  */
    unsigned long opcode;	/* Opcode number.  */
    expressionS exp;		/* Expression required for relocation.  */
    int reloc;			/* Relocation type required.  */
    int pcrel;			/* True if relocation PC relative.  */
    char *pname;		/* Name of instruction in parallel.  */
    unsigned int num_operands;	/* Number of operands in total.  */
    c4x_inst_t *inst;		/* Pointer to first template.  */
    c4x_operand_t operands[C4X_OPERANDS_MAX];
  }
c4x_insn_t;

static c4x_insn_t the_insn;	/* Info about our instruction.  */
static c4x_insn_t *insn = &the_insn;

int c4x_gen_to_words
    PARAMS ((FLONUM_TYPE, LITTLENUM_TYPE *, int ));
char *c4x_atof
    PARAMS ((char *, char, LITTLENUM_TYPE * ));
static void c4x_insert_reg
    PARAMS ((char *, int ));
static void c4x_insert_sym
    PARAMS ((char *, int ));
static char *c4x_expression
    PARAMS ((char *, expressionS *));
static char *c4x_expression_abs
    PARAMS ((char *, int *));
static void c4x_emit_char
    PARAMS ((char));
static void c4x_seg_alloc
    PARAMS ((char *, segT, int, symbolS *));
static void c4x_asg
    PARAMS ((int));
static void c4x_bss
    PARAMS ((int));
void c4x_globl
    PARAMS ((int));
static void c4x_cons
    PARAMS ((int));
static void c4x_eval
    PARAMS ((int));
static void c4x_newblock
    PARAMS ((int));
static void c4x_sect
    PARAMS ((int));
static void c4x_set
    PARAMS ((int));
static void c4x_usect
    PARAMS ((int));
static void c4x_version
    PARAMS ((int));
static void c4x_pseudo_ignore
    PARAMS ((int));
static void c4x_init_regtable
    PARAMS ((void));
static void c4x_init_symbols
    PARAMS ((void));
static int c4x_inst_insert
    PARAMS ((c4x_inst_t *));
static c4x_inst_t *c4x_inst_make
    PARAMS ((char *, unsigned long, char *));
static int c4x_inst_add
    PARAMS ((c4x_inst_t *));
void md_begin
    PARAMS ((void));
void c4x_end
    PARAMS ((void));
static int c4x_indirect_parse
    PARAMS ((c4x_operand_t *, const c4x_indirect_t *));
char *c4x_operand_parse
    PARAMS ((char *, c4x_operand_t *));
static int c4x_operands_match
    PARAMS ((c4x_inst_t *, c4x_insn_t *));
void c4x_insn_output
    PARAMS ((c4x_insn_t *));
int c4x_operands_parse
    PARAMS ((char *, c4x_operand_t *, int ));
void md_assemble
    PARAMS ((char *));
void c4x_cleanup
    PARAMS ((void));
char *md_atof
    PARAMS ((int, char *, int *));
void md_apply_fix3
    PARAMS ((fixS *, valueT *, segT ));
void md_convert_frag
    PARAMS ((bfd *, segT, fragS *));
void md_create_short_jump
    PARAMS ((char *, addressT, addressT, fragS *, symbolS *));
void md_create_long_jump
    PARAMS ((char *, addressT, addressT, fragS *, symbolS *));
int md_estimate_size_before_relax
    PARAMS ((register fragS *, segT));
int md_parse_option
    PARAMS ((int, char *));
void md_show_usage
    PARAMS ((FILE *));
int c4x_unrecognized_line
    PARAMS ((int));
symbolS *md_undefined_symbol
    PARAMS ((char *));
void md_operand
    PARAMS ((expressionS *));
valueT md_section_align
    PARAMS ((segT, valueT));
static int c4x_pc_offset
    PARAMS ((unsigned int));
long md_pcrel_from
    PARAMS ((fixS *));
int c4x_do_align
    PARAMS ((int, const char *, int, int));
void c4x_start_line
    PARAMS ((void));
arelent *tc_gen_reloc
    PARAMS ((asection *, fixS *));


const pseudo_typeS
  md_pseudo_table[] =
{
  {"align", s_align_bytes, 32},
  {"ascii", c4x_cons, 1},
  {"asciz", c4x_pseudo_ignore, 0},
  {"asg", c4x_asg, 0},
  {"asect", c4x_pseudo_ignore, 0}, /* Absolute named section.  */
  {"block", s_space, 0},
  {"byte", c4x_cons, 1},
  {"bss", c4x_bss, 0},
  {"comm", c4x_bss, 0},
  {"def", c4x_globl, 0},
  {"endfunc", c4x_pseudo_ignore, 0},
  {"eos", c4x_pseudo_ignore, 0},
  {"etag", c4x_pseudo_ignore, 0},
  {"equ", c4x_set, 0},
  {"eval", c4x_eval, 0},
  {"exitm", s_mexit, 0},
  {"func", c4x_pseudo_ignore, 0},
  {"global", c4x_globl, 0},
  {"globl", c4x_globl, 0},
  {"hword", c4x_cons, 2},
  {"ieee", float_cons, 'i'},
  {"int", c4x_cons, 4},		/* .int allocates 4 bytes.  */
  {"length", c4x_pseudo_ignore, 0},
  {"ldouble", float_cons, 'l'},
  {"member", c4x_pseudo_ignore, 0},
  {"newblock", c4x_newblock, 0},
  {"ref", s_ignore, 0},		/* All undefined treated as external.  */
  {"set", c4x_set, 0},
  {"sect", c4x_sect, 1},	/* Define named section.  */
  {"space", s_space, 4},
  {"stag", c4x_pseudo_ignore, 0},
  {"string", c4x_pseudo_ignore, 0},
  {"sym", c4x_pseudo_ignore, 0},
  {"usect", c4x_usect, 0},	/* Reserve space in uninit. named sect.  */
  {"version", c4x_version, 0},
  {"width", c4x_pseudo_ignore, 0},
  {"word", c4x_cons, 4},	/* .word allocates 4 bytes.  */
  {"xdef", c4x_globl, 0},
  {NULL, 0, 0},
};

int md_short_jump_size = 4;
int md_long_jump_size = 4;
const int md_reloc_size = RELSZ;	/* Coff headers.  */

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful.  */
#ifdef C4X_ALT_SYNTAX
const char comment_chars[] = ";!";
#else
const char comment_chars[] = ";";
#endif

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output. 
   Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. 
   Also note that comments like this one will always work.  */
const char line_comment_chars[] = "#*";

/* We needed an unused char for line separation to work around the
   lack of macros, using sed and such.  */
const char line_separator_chars[] = "&";

/* Chars that can be used to separate mant from exp in floating point nums.  */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant.  */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "fFilsS";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c.  Ideally it shouldn't have to know about it at
   all, but nothing is ideal around here.  */

/* Flonums returned here.  */
extern FLONUM_TYPE generic_floating_point_number;

/* Precision in LittleNums.  */
#define MAX_PRECISION (2)
#define S_PRECISION (1)		/* Short float constants 16-bit.  */
#define F_PRECISION (2)		/* Float and double types 32-bit.  */
#define GUARD (2)


/* Turn generic_floating_point_number into a real short/float/double.  */
int
c4x_gen_to_words (flonum, words, precision)
     FLONUM_TYPE flonum;
     LITTLENUM_TYPE *words;
     int precision;
{
  int return_value = 0;
  LITTLENUM_TYPE *p;		/* Littlenum pointer.  */
  int mantissa_bits;		/* Bits in mantissa field.  */
  int exponent_bits;		/* Bits in exponent field.  */
  int exponent;
  unsigned int sone;		/* Scaled one.  */
  unsigned int sfract;		/* Scaled fraction.  */
  unsigned int smant;		/* Scaled mantissa.  */
  unsigned int tmp;
  int shift;			/* Shift count.  */

  /* Here is how a generic floating point number is stored using
     flonums (an extension of bignums) where p is a pointer to an
     array of LITTLENUMs.

     For example 2e-3 is stored with exp = -4 and
     bits[0] = 0x0000
     bits[1] = 0x0000
     bits[2] = 0x4fde
     bits[3] = 0x978d
     bits[4] = 0x126e
     bits[5] = 0x0083
     with low = &bits[2], high = &bits[5], and leader = &bits[5].

     This number can be written as
     0x0083126e978d4fde.00000000 * 65536**-4  or
     0x0.0083126e978d4fde        * 65536**0   or
     0x0.83126e978d4fde          * 2**-8   = 2e-3

     Note that low points to the 65536**0 littlenum (bits[2]) and
     leader points to the most significant non-zero littlenum
     (bits[5]).

     TMS320C3X floating point numbers are a bit of a strange beast.
     The 32-bit flavour has the 8 MSBs representing the exponent in
     twos complement format (-128 to +127).  There is then a sign bit
     followed by 23 bits of mantissa.  The mantissa is expressed in
     twos complement format with the binary point after the most
     significant non sign bit.  The bit after the binary point is
     suppressed since it is the complement of the sign bit.  The
     effective mantissa is thus 24 bits.  Zero is represented by an
     exponent of -128.

     The 16-bit flavour has the 4 MSBs representing the exponent in
     twos complement format (-8 to +7).  There is then a sign bit
     followed by 11 bits of mantissa.  The mantissa is expressed in
     twos complement format with the binary point after the most
     significant non sign bit.  The bit after the binary point is
     suppressed since it is the complement of the sign bit.  The
     effective mantissa is thus 12 bits.  Zero is represented by an
     exponent of -8.  For example,

     number       norm mant m  x  e  s  i    fraction f
     +0.500 =>  1.00000000000 -1 -1  0  1  .00000000000   (1 + 0) * 2^(-1)
     +0.999 =>  1.11111111111 -1 -1  0  1  .11111111111   (1 + 0.99) * 2^(-1)
     +1.000 =>  1.00000000000  0  0  0  1  .00000000000   (1 + 0) * 2^(0)
     +1.500 =>  1.10000000000  0  0  0  1  .10000000000   (1 + 0.5) * 2^(0)
     +1.999 =>  1.11111111111  0  0  0  1  .11111111111   (1 + 0.9) * 2^(0)
     +2.000 =>  1.00000000000  1  1  0  1  .00000000000   (1 + 0) * 2^(1)
     +4.000 =>  1.00000000000  2  2  0  1  .00000000000   (1 + 0) * 2^(2)
     -0.500 =>  1.00000000000 -1 -1  1  0  .10000000000   (-2 + 0) * 2^(-2)
     -1.000 =>  1.00000000000  0 -1  1  0  .00000000000   (-2 + 0) * 2^(-1)
     -1.500 =>  1.10000000000  0  0  1  0  .10000000000   (-2 + 0.5) * 2^(0)
     -1.999 =>  1.11111111111  0  0  1  0  .00000000001   (-2 + 0.11) * 2^(0)
     -2.000 =>  1.00000000000  1  1  1  0  .00000000000   (-2 + 0) * 2^(0)
     -4.000 =>  1.00000000000  2  1  1  0  .00000000000   (-2 + 0) * 2^(1)

     where e is the exponent, s is the sign bit, i is the implied bit,
     and f is the fraction stored in the mantissa field.

     num = (1 + f) * 2^x   =  m * 2^e if s = 0
     num = (-2 + f) * 2^x  = -m * 2^e if s = 1
     where 0 <= f < 1.0  and 1.0 <= m < 2.0

     The fraction (f) and exponent (e) fields for the TMS320C3X format
     can be derived from the normalised mantissa (m) and exponent (x) using:

     f = m - 1, e = x       if s = 0
     f = 2 - m, e = x       if s = 1 and m != 1.0
     f = 0,     e = x - 1   if s = 1 and m = 1.0
     f = 0,     e = -8      if m = 0


     OK, the other issue we have to consider is rounding since the
     mantissa has a much higher potential precision than what we can
     represent.  To do this we add half the smallest storable fraction.
     We then have to renormalise the number to allow for overflow.

     To convert a generic flonum into a TMS320C3X floating point
     number, here's what we try to do....

     The first thing is to generate a normalised mantissa (m) where
     1.0 <= m < 2 and to convert the exponent from base 16 to base 2.
     We desire the binary point to be placed after the most significant
     non zero bit.  This process is done in two steps: firstly, the
     littlenum with the most significant non zero bit is located (this
     is done for us since leader points to this littlenum) and the
     binary point (which is currently after the LSB of the littlenum
     pointed to by low) is moved to before the MSB of the littlenum
     pointed to by leader.  This requires the exponent to be adjusted
     by leader - low + 1.  In the earlier example, the new exponent is
     thus -4 + (5 - 2 + 1) = 0 (base 65536).  We now need to convert
     the exponent to base 2 by multiplying the exponent by 16 (log2
     65536).  The exponent base 2 is thus also zero.

     The second step is to hunt for the most significant non zero bit
     in the leader littlenum.  We do this by left shifting a copy of
     the leader littlenum until bit 16 is set (0x10000) and counting
     the number of shifts, S, required.  The number of shifts then has to
     be added to correct the exponent (base 2).  For our example, this
     will require 9 shifts and thus our normalised exponent (base 2) is
     0 + 9 = 9.  Note that the worst case scenario is when the leader
     littlenum is 1, thus requiring 16 shifts.

     We now have to left shift the other littlenums by the same amount,
     propagating the shifted bits into the more significant littlenums.
     To save a lot of unecessary shifting we only have to consider
     two or three littlenums, since the greatest number of mantissa
     bits required is 24 + 1 rounding bit.  While two littlenums
     provide 32 bits of precision, the most significant littlenum
     may only contain a single significant bit  and thus an extra
     littlenum is required.

     Denoting the number of bits in the fraction field as F, we require
     G = F + 2 bits (one extra bit is for rounding, the other gets
     suppressed).  Say we required S shifts to find the most
     significant bit in the leader littlenum, the number of left shifts
     required to move this bit into bit position G - 1 is L = G + S - 17.
     Note that this shift count may be negative for the short floating
     point flavour (where F = 11 and thus G = 13 and potentially S < 3).
     If L > 0 we have to shunt the next littlenum into position.  Bit
     15 (the MSB) of the next littlenum needs to get moved into position
     L - 1 (If L > 15 we need all the bits of this littlenum and
     some more from the next one.).  We subtract 16 from L and use this
     as the left shift count;  the resultant value we or with the
     previous result.  If L > 0, we repeat this operation.   */

  if (precision != S_PRECISION)
    words[1] = 0x0000;

  /* 0.0e0 seen.  */
  if (flonum.low > flonum.leader)
    {
      words[0] = 0x8000;
      return return_value;
    }

  /* NaN:  We can't do much...  */
  if (flonum.sign == 0)
    {
      as_bad ("Nan, using zero.");
      words[0] = 0x8000;
      return return_value;
    }
  else if (flonum.sign == 'P')
    {
      /* +INF:  Replace with maximum float.  */
      if (precision == S_PRECISION)
	words[0] = 0x77ff;
      else
	{
	  words[0] = 0x7f7f;
	  words[1] = 0xffff;
	}
      return return_value;
    }
  else if (flonum.sign == 'N')
    {
      /* -INF:  Replace with maximum float.  */
      if (precision == S_PRECISION)
	words[0] = 0x7800;
      else
	words[0] = 0x7f80;
      return return_value;
    }

  exponent = (flonum.exponent + flonum.leader - flonum.low + 1) * 16;

  if (!(tmp = *flonum.leader))
    abort ();			/* Hmmm.  */
  shift = 0;			/* Find position of first sig. bit.  */
  while (tmp >>= 1)
    shift++;
  exponent -= (16 - shift);	/* Adjust exponent.  */

  if (precision == S_PRECISION)	/* Allow 1 rounding bit.  */
    {
      exponent_bits = 4;
      mantissa_bits = 12;	/* Include suppr. bit but not rounding bit.  */
    }
  else
    {
      exponent_bits = 8;
      mantissa_bits = 24;
    }

  shift = mantissa_bits - shift;

  smant = 0;
  for (p = flonum.leader; p >= flonum.low && shift > -16; p--)
    {
      tmp = shift >= 0 ? *p << shift : *p >> -shift;
      smant |= tmp;
      shift -= 16;
    }

  /* OK, we've got our scaled mantissa so let's round it up
     and drop the rounding bit.  */
  smant++;
  smant >>= 1;

  /* The number may be unnormalised so renormalise it...  */
  if (smant >> mantissa_bits)
    {
      smant >>= 1;
      exponent++;
    }

  /* The binary point is now between bit positions 11 and 10 or 23 and 22,
     i.e., between mantissa_bits - 1 and mantissa_bits - 2 and the
     bit at mantissa_bits - 1 should be set.  */
  if (!(smant >> (mantissa_bits - 1)))
    abort ();			/* Ooops.  */

  sone = (1 << (mantissa_bits - 1));
  if (flonum.sign == '+')
    sfract = smant - sone;	/* smant - 1.0.  */
  else
    {
      /* This seems to work.  */
      if (smant == sone)
	{
	  exponent--;
	  sfract = 0;
	}
      else
	sfract = (sone << 1) - smant;	/* 2.0 - smant.  */
      sfract |= sone;		/* Insert sign bit.  */
    }

  if (abs (exponent) >= (1 << (exponent_bits - 1)))
    as_bad ("Cannot represent exponent in %d bits", exponent_bits);

  /* Force exponent to fit in desired field width.  */
  exponent &= (1 << (exponent_bits)) - 1;
  sfract |= exponent << mantissa_bits;

  if (precision == S_PRECISION)
    words[0] = sfract;
  else
    {
      words[0] = sfract >> 16;
      words[1] = sfract & 0xffff;
    }

  return return_value;
}

/* Returns pointer past text consumed.  */
char *
c4x_atof (str, what_kind, words)
     char *str;
     char what_kind;
     LITTLENUM_TYPE *words;
{
  /* Extra bits for zeroed low-order bits.  The 1st MAX_PRECISION are
     zeroed, the last contain flonum bits.  */
  static LITTLENUM_TYPE bits[MAX_PRECISION + MAX_PRECISION + GUARD];
  char *return_value;
  /* Number of 16-bit words in the format.  */
  int precision;
  FLONUM_TYPE save_gen_flonum;

  /* We have to save the generic_floating_point_number because it
     contains storage allocation about the array of LITTLENUMs where
     the value is actually stored.  We will allocate our own array of
     littlenums below, but have to restore the global one on exit.  */
  save_gen_flonum = generic_floating_point_number;

  return_value = str;
  generic_floating_point_number.low = bits + MAX_PRECISION;
  generic_floating_point_number.high = NULL;
  generic_floating_point_number.leader = NULL;
  generic_floating_point_number.exponent = 0;
  generic_floating_point_number.sign = '\0';

  /* Use more LittleNums than seems necessary: the highest flonum may
     have 15 leading 0 bits, so could be useless.  */

  memset (bits, '\0', sizeof (LITTLENUM_TYPE) * MAX_PRECISION);

  switch (what_kind)
    {
    case 's':
    case 'S':
      precision = S_PRECISION;
      break;

    case 'd':
    case 'D':
    case 'f':
    case 'F':
      precision = F_PRECISION;
      break;

    default:
      as_bad ("Invalid floating point number");
      return (NULL);
    }

  generic_floating_point_number.high
    = generic_floating_point_number.low + precision - 1 + GUARD;

  if (atof_generic (&return_value, ".", EXP_CHARS,
		    &generic_floating_point_number))
    {
      as_bad ("Invalid floating point number");
      return (NULL);
    }

  c4x_gen_to_words (generic_floating_point_number,
		    words, precision);

  /* Restore the generic_floating_point_number's storage alloc (and
     everything else).  */
  generic_floating_point_number = save_gen_flonum;

  return return_value;
}

static void 
c4x_insert_reg (regname, regnum)
     char *regname;
     int regnum;
{
  char buf[32];
  int i;

  symbol_table_insert (symbol_new (regname, reg_section, (valueT) regnum,
				   &zero_address_frag));
  for (i = 0; regname[i]; i++)
    buf[i] = islower (regname[i]) ? toupper (regname[i]) : regname[i];
  buf[i] = '\0';

  symbol_table_insert (symbol_new (buf, reg_section, (valueT) regnum,
				   &zero_address_frag));
}

static void 
c4x_insert_sym (symname, value)
     char *symname;
     int value;
{
  symbolS *symbolP;

  symbolP = symbol_new (symname, absolute_section,
			(valueT) value, &zero_address_frag);
  SF_SET_LOCAL (symbolP);
  symbol_table_insert (symbolP);
}

static char *
c4x_expression (str, exp)
     char *str;
     expressionS *exp;
{
  char *s;
  char *t;

  t = input_line_pointer;	/* Save line pointer.  */
  input_line_pointer = str;
  expression (exp);
  s = input_line_pointer;
  input_line_pointer = t;	/* Restore line pointer.  */
  return s;			/* Return pointer to where parsing stopped.  */
}

static char *
c4x_expression_abs (str, value)
     char *str;
     int *value;
{
  char *s;
  char *t;

  t = input_line_pointer;	/* Save line pointer.  */
  input_line_pointer = str;
  *value = get_absolute_expression ();
  s = input_line_pointer;
  input_line_pointer = t;	/* Restore line pointer.  */
  return s;
}

static void 
c4x_emit_char (c)
     char c;
{
  expressionS exp;

  exp.X_op = O_constant;
  exp.X_add_number = c;
  emit_expr (&exp, 4);
}

static void 
c4x_seg_alloc (name, seg, size, symbolP)
     char *name ATTRIBUTE_UNUSED;
     segT seg ATTRIBUTE_UNUSED;
     int size;
     symbolS *symbolP;
{
  /* Note that the size is in words
     so we multiply it by 4 to get the number of bytes to allocate.  */

  /* If we have symbol:  .usect  ".fred", size etc.,
     the symbol needs to point to the first location reserved
     by the pseudo op.  */

  if (size)
    {
      char *p;

      p = frag_var (rs_fill, 1, 1, (relax_substateT) 0,
		    (symbolS *) symbolP,
		    size * OCTETS_PER_BYTE, (char *) 0);
      *p = 0;
    }
}

/* .asg ["]character-string["], symbol */
static void 
c4x_asg (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *name;
  char *str;
  char *tmp;

  SKIP_WHITESPACE ();
  str = input_line_pointer;

  /* Skip string expression.  */
  while (*input_line_pointer != ',' && *input_line_pointer)
    input_line_pointer++;
  if (*input_line_pointer != ',')
    {
      as_bad ("Comma expected\n");
      return;
    }
  *input_line_pointer++ = '\0';
  name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  tmp = xmalloc (strlen (str) + 1);
  strcpy (tmp, str);
  str = tmp;
  tmp = xmalloc (strlen (name) + 1);
  strcpy (tmp, name);
  name = tmp;
  if (hash_find (c4x_asg_hash, name))
    hash_replace (c4x_asg_hash, name, (PTR) str);
  else
    hash_insert (c4x_asg_hash, name, (PTR) str);
  *input_line_pointer = c;
  demand_empty_rest_of_line ();
}

/* .bss symbol, size  */
static void 
c4x_bss (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *name;
  char *p;
  int size;
  segT current_seg;
  subsegT current_subseg;
  symbolS *symbolP;

  current_seg = now_seg;	/* Save current seg.  */
  current_subseg = now_subseg;	/* Save current subseg.  */

  SKIP_WHITESPACE ();
  name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  if (c != ',')
    {
      as_bad (".bss size argument missing\n");
      return;
    }

  input_line_pointer =
    c4x_expression_abs (++input_line_pointer, &size);
  if (size < 0)
    {
      as_bad (".bss size %d < 0!", size);
      return;
    }
  subseg_set (bss_section, 0);
  symbolP = symbol_find_or_make (name);

  if (S_GET_SEGMENT (symbolP) == bss_section)
    symbol_get_frag (symbolP)->fr_symbol = 0;

  symbol_set_frag (symbolP, frag_now);

  p = frag_var (rs_org, 1, 1, (relax_substateT) 0, symbolP,
		size * OCTETS_PER_BYTE, (char *) 0);
  *p = 0;			/* Fill char.  */

  S_SET_SEGMENT (symbolP, bss_section);

  /* The symbol may already have been created with a preceding
     ".globl" directive -- be careful not to step on storage class
     in that case.  Otherwise, set it to static.  */
  if (S_GET_STORAGE_CLASS (symbolP) != C_EXT)
    S_SET_STORAGE_CLASS (symbolP, C_STAT);

  subseg_set (current_seg, current_subseg); /* Restore current seg.  */
  demand_empty_rest_of_line ();
}

void
c4x_globl (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *name;
  int c;
  symbolS *symbolP;

  do
    {
      name = input_line_pointer;
      c = get_symbol_end ();
      symbolP = symbol_find_or_make (name);
      *input_line_pointer = c;
      SKIP_WHITESPACE ();
      S_SET_STORAGE_CLASS (symbolP, C_EXT);
      if (c == ',')
	{
	  input_line_pointer++;
	  SKIP_WHITESPACE ();
	  if (*input_line_pointer == '\n')
	    c = '\n';
	}
    }
  while (c == ',');

  demand_empty_rest_of_line ();
}

/* Handle .byte, .word. .int, .long */
static void 
c4x_cons (bytes)
     int bytes;
{
  register unsigned int c;
  do
    {
      SKIP_WHITESPACE ();
      if (*input_line_pointer == '"')
	{
	  input_line_pointer++;
	  while (is_a_char (c = next_char_of_string ()))
	    c4x_emit_char (c);
	  know (input_line_pointer[-1] == '\"');
	}
      else
	{
	  expressionS exp;

	  input_line_pointer = c4x_expression (input_line_pointer, &exp);
	  if (exp.X_op == O_constant)
	    {
	      switch (bytes)
		{
		case 1:
		  exp.X_add_number &= 255;
		  break;
		case 2:
		  exp.X_add_number &= 65535;
		  break;
		}
	    }
	  /* Perhaps we should disallow .byte and .hword with
	     a non constant expression that will require relocation.  */
	  emit_expr (&exp, 4);
	}
    }
  while (*input_line_pointer++ == ',');

  input_line_pointer--;		/* Put terminator back into stream.  */
  demand_empty_rest_of_line ();
}

/* .eval expression, symbol */
static void 
c4x_eval (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  int value;
  char *name;

  SKIP_WHITESPACE ();
  input_line_pointer =
    c4x_expression_abs (input_line_pointer, &value);
  if (*input_line_pointer++ != ',')
    {
      as_bad ("Symbol missing\n");
      return;
    }
  name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  demand_empty_rest_of_line ();
  c4x_insert_sym (name, value);
}

/* Reset local labels.  */
static void 
c4x_newblock (x)
     int x ATTRIBUTE_UNUSED;
{
  dollar_label_clear ();
}

/* .sect "section-name" [, value] */
/* .sect ["]section-name[:subsection-name]["] [, value] */
static void 
c4x_sect (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *section_name;
  char *subsection_name;
  char *name;
  segT seg;
  int num;

  SKIP_WHITESPACE ();
  if (*input_line_pointer == '"')
    input_line_pointer++;
  section_name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  input_line_pointer++;		/* Skip null symbol terminator.  */
  name = xmalloc (input_line_pointer - section_name + 1);
  strcpy (name, section_name);

  /* TI C from version 5.0 allows a section name to contain a
     subsection name as well. The subsection name is separated by a
     ':' from the section name.  Currently we scan the subsection
     name and discard it.
     Volker Kuhlmann  <v.kuhlmann@elec.canterbury.ac.nz>.  */
  if (c == ':')
    {
      subsection_name = input_line_pointer;
      c = get_symbol_end ();	/* Get terminator.  */
      input_line_pointer++;	/* Skip null symbol terminator.  */
      as_warn (".sect: subsection name ignored");
    }

  /* We might still have a '"' to discard, but the character after a
     symbol name will be overwritten with a \0 by get_symbol_end()
     [VK].  */

  if (c == ',')
    input_line_pointer =
      c4x_expression_abs (input_line_pointer, &num);
  else if (*input_line_pointer == ',')
    {
      input_line_pointer =
	c4x_expression_abs (++input_line_pointer, &num);
    }
  else
    num = 0;

  seg = subseg_new (name, num);
  if (line_label != NULL)
    {
      S_SET_SEGMENT (line_label, seg);
      symbol_set_frag (line_label, frag_now);
    }

  if (bfd_get_section_flags (stdoutput, seg) == SEC_NO_FLAGS)
    {
      if (!bfd_set_section_flags (stdoutput, seg, SEC_DATA))
	as_warn ("Error setting flags for \"%s\": %s", name,
		 bfd_errmsg (bfd_get_error ()));
    }

  /* If the last character overwritten by get_symbol_end() was an
     end-of-line, we must restore it or the end of the line will not be
     recognised and scanning extends into the next line, stopping with
     an error (blame Volker Kuhlmann <v.kuhlmann@elec.canterbury.ac.nz>
     if this is not true).  */
  if (is_end_of_line[(unsigned char) c])
    *(--input_line_pointer) = c;

  demand_empty_rest_of_line ();
}

/* symbol[:] .set value  or  .set symbol, value */
static void 
c4x_set (x)
     int x ATTRIBUTE_UNUSED;
{
  symbolS *symbolP;

  SKIP_WHITESPACE ();
  if ((symbolP = line_label) == NULL)
    {
      char c;
      char *name;

      name = input_line_pointer;
      c = get_symbol_end ();	/* Get terminator.  */
      if (c != ',')
	{
	  as_bad (".set syntax invalid\n");
	  ignore_rest_of_line ();
	  return;
	}
      symbolP = symbol_find_or_make (name);
    }
  else
    symbol_table_insert (symbolP);

  pseudo_set (symbolP);
  demand_empty_rest_of_line ();
}

/* [symbol] .usect ["]section-name["], size-in-words [, alignment-flag] */
static void 
c4x_usect (x)
     int x ATTRIBUTE_UNUSED;
{
  char c;
  char *name;
  char *section_name;
  segT seg;
  int size, alignment_flag;
  segT current_seg;
  subsegT current_subseg;

  current_seg = now_seg;	/* save current seg.  */
  current_subseg = now_subseg;	/* save current subseg.  */

  SKIP_WHITESPACE ();
  if (*input_line_pointer == '"')
    input_line_pointer++;
  section_name = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  input_line_pointer++;		/* Skip null symbol terminator.  */
  name = xmalloc (input_line_pointer - section_name + 1);
  strcpy (name, section_name);

  if (c == ',')
    input_line_pointer =
      c4x_expression_abs (input_line_pointer, &size);
  else if (*input_line_pointer == ',')
    {
      input_line_pointer =
	c4x_expression_abs (++input_line_pointer, &size);
    }
  else
    size = 0;

  /* Read a possibly present third argument (alignment flag) [VK].  */
  if (*input_line_pointer == ',')
    {
      input_line_pointer =
	c4x_expression_abs (++input_line_pointer, &alignment_flag);
    }
  else
    alignment_flag = 0;
  if (alignment_flag)
    as_warn (".usect: non-zero alignment flag ignored");

  seg = subseg_new (name, 0);
  if (line_label != NULL)
    {
      S_SET_SEGMENT (line_label, seg);
      symbol_set_frag (line_label, frag_now);
      S_SET_VALUE (line_label, frag_now_fix ());
    }
  seg_info (seg)->bss = 1;	/* Uninitialised data.  */
  if (!bfd_set_section_flags (stdoutput, seg, SEC_ALLOC))
    as_warn ("Error setting flags for \"%s\": %s", name,
	     bfd_errmsg (bfd_get_error ()));
  c4x_seg_alloc (name, seg, size, line_label);

  if (S_GET_STORAGE_CLASS (line_label) != C_EXT)
    S_SET_STORAGE_CLASS (line_label, C_STAT);

  subseg_set (current_seg, current_subseg);	/* Restore current seg.  */
  demand_empty_rest_of_line ();
}

/* .version cpu-version.  */
static void 
c4x_version (x)
     int x ATTRIBUTE_UNUSED;
{
  unsigned int temp;

  input_line_pointer =
    c4x_expression_abs (input_line_pointer, &temp);
  if (!IS_CPU_C3X (temp) && !IS_CPU_C4X (temp))
    as_bad ("This assembler does not support processor generation %d\n",
	    temp);

  if (c4x_cpu && temp != c4x_cpu)
    as_warn ("Changing processor generation on fly not supported...\n");
  c4x_cpu = temp;
  demand_empty_rest_of_line ();
}

static void 
c4x_pseudo_ignore (x)
     int x ATTRIBUTE_UNUSED;
{
  /* We could print warning message here...  */

  /* Ignore everything until end of line.  */
  while (!is_end_of_line[(unsigned char) *input_line_pointer++]);
}

static void 
c4x_init_regtable ()
{
  unsigned int i;

  for (i = 0; i < c3x_num_registers; i++)
    c4x_insert_reg (c3x_registers[i].name,
		    c3x_registers[i].regno);

  if (IS_CPU_C4X (c4x_cpu))
    {
      /* Add additional C4x registers, overriding some C3x ones.  */
      for (i = 0; i < c4x_num_registers; i++)
	c4x_insert_reg (c4x_registers[i].name,
			c4x_registers[i].regno);
    }
}

static void 
c4x_init_symbols ()
{
  /* The TI tools accept case insensitive versions of these symbols,
     we don't !

     For TI C/Asm 5.0

     .TMS320xx       30,31,32,40,or 44       set according to -v flag
     .C3X or .C3x    1 or 0                  1 if -v30,-v31,or -v32
     .C30            1 or 0                  1 if -v30
     .C31            1 or 0                  1 if -v31
     .C32            1 or 0                  1 if -v32
     .C4X or .C4x    1 or 0                  1 if -v40, or -v44
     .C40            1 or 0                  1 if -v40
     .C44            1 or 0                  1 if -v44

     .REGPARM 1 or 0                  1 if -mr option used
     .BIGMODEL        1 or 0                  1 if -mb option used

     These symbols are currently supported but will be removed in a
     later version:
     .TMS320C30      1 or 0                  1 if -v30,-v31,or -v32
     .TMS320C31      1 or 0                  1 if -v31
     .TMS320C32      1 or 0                  1 if -v32
     .TMS320C40      1 or 0                  1 if -v40, or -v44
     .TMS320C44      1 or 0                  1 if -v44

     Source: TI: TMS320C3x/C4x Assembly Language Tools User's Guide,
     1997, SPRU035C, p. 3-17/3-18.  */
  c4x_insert_sym (".REGPARM", c4x_reg_args);
  c4x_insert_sym (".MEMPARM", !c4x_reg_args);	
  c4x_insert_sym (".BIGMODEL", c4x_big_model);
  c4x_insert_sym (".C30INTERRUPT", 0);
  c4x_insert_sym (".TMS320xx", c4x_cpu == 0 ? 40 : c4x_cpu);
  c4x_insert_sym (".C3X", c4x_cpu == 30 || c4x_cpu == 31 || c4x_cpu == 32);
  c4x_insert_sym (".C3x", c4x_cpu == 30 || c4x_cpu == 31 || c4x_cpu == 32);
  c4x_insert_sym (".C4X", c4x_cpu == 0 || c4x_cpu == 40 || c4x_cpu == 44);
  c4x_insert_sym (".C4x", c4x_cpu == 0 || c4x_cpu == 40 || c4x_cpu == 44);
  /* Do we need to have the following symbols also in lower case?  */
  c4x_insert_sym (".TMS320C30", c4x_cpu == 30 || c4x_cpu == 31 || c4x_cpu == 32);
  c4x_insert_sym (".tms320C30", c4x_cpu == 30 || c4x_cpu == 31 || c4x_cpu == 32);
  c4x_insert_sym (".TMS320C31", c4x_cpu == 31);
  c4x_insert_sym (".tms320C31", c4x_cpu == 31);
  c4x_insert_sym (".TMS320C32", c4x_cpu == 32);
  c4x_insert_sym (".tms320C32", c4x_cpu == 32);
  c4x_insert_sym (".TMS320C40", c4x_cpu == 40 || c4x_cpu == 44 || c4x_cpu == 0);
  c4x_insert_sym (".tms320C40", c4x_cpu == 40 || c4x_cpu == 44 || c4x_cpu == 0);
  c4x_insert_sym (".TMS320C44", c4x_cpu == 44);
  c4x_insert_sym (".tms320C44", c4x_cpu == 44);
  c4x_insert_sym (".TMX320C40", 0);	/* C40 first pass silicon ?  */
  c4x_insert_sym (".tmx320C40", 0);
}

/* Insert a new instruction template into hash table.  */
static int 
c4x_inst_insert (inst)
     c4x_inst_t *inst;
{
  static char prev_name[16];
  const char *retval = NULL;

  /* Only insert the first name if have several similar entries.  */
  if (!strcmp (inst->name, prev_name) || inst->name[0] == '\0')
    return 1;

  retval = hash_insert (c4x_op_hash, inst->name, (PTR) inst);
  if (retval != NULL)
    fprintf (stderr, "internal error: can't hash `%s': %s\n",
	     inst->name, retval);
  else
    strcpy (prev_name, inst->name);
  return retval == NULL;
}

/* Make a new instruction template.  */
static c4x_inst_t *
c4x_inst_make (name, opcode, args)
     char *name;
     unsigned long opcode;
     char *args;
{
  static c4x_inst_t *insts = NULL;
  static char *names = NULL;
  static int index = 0;

  if (insts == NULL)
    {
      /* Allocate memory to store name strings.  */
      names = (char *) xmalloc (sizeof (char) * 8192);
      /* Allocate memory for additional insts.  */
      insts = (c4x_inst_t *)
	xmalloc (sizeof (c4x_inst_t) * 1024);
    }
  insts[index].name = names;
  insts[index].opcode = opcode;
  insts[index].opmask = 0xffffffff;
  insts[index].args = args;
  index++;

  do
    *names++ = *name++;
  while (*name);
  *names++ = '\0';

  return &insts[index - 1];
}

/* Add instruction template, creating dynamic templates as required.  */
static int 
c4x_inst_add (insts)
     c4x_inst_t *insts;
{
  char *s = insts->name;
  char *d;
  unsigned int i;
  int ok = 1;
  char name[16];

  d = name;

  while (1)
    {
      switch (*s)
	{
	case 'B':
	case 'C':
	  /* Dynamically create all the conditional insts.  */
	  for (i = 0; i < num_conds; i++)
	    {
	      c4x_inst_t *inst;
	      int k = 0;
	      char *c = c4x_conds[i].name;
	      char *e = d;

	      while (*c)
		*e++ = *c++;
	      c = s + 1;
	      while (*c)
		*e++ = *c++;
	      *e = '\0';

	      /* If instruction found then have already processed it.  */
	      if (hash_find (c4x_op_hash, name))
		return 1;

	      do
		{
		  inst = c4x_inst_make (name, insts[k].opcode +
					(c4x_conds[i].cond <<
					 (*s == 'B' ? 16 : 23)),
					insts[k].args);
		  if (k == 0)	/* Save strcmp() with following func.  */
		    ok &= c4x_inst_insert (inst);
		  k++;
		}
	      while (!strcmp (insts->name,
			      insts[k].name));
	    }
	  return ok;
	  break;

	case '\0':
	  return c4x_inst_insert (insts);
	  break;

	default:
	  *d++ = *s++;
	  break;
	}
    }
}

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc., that the MD part of the assembler will
   need.  */
void 
md_begin ()
{
  int ok = 1;
  unsigned int i;

  /* Create hash table for mnemonics.  */
  c4x_op_hash = hash_new ();

  /* Create hash table for asg pseudo.  */
  c4x_asg_hash = hash_new ();

  /* Add mnemonics to hash table, expanding conditional mnemonics on fly.  */
  for (i = 0; i < c3x_num_insts; i++)
    ok &= c4x_inst_add ((void *) &c3x_insts[i]);

  if (IS_CPU_C4X (c4x_cpu))
    {
      for (i = 0; i < c4x_num_insts; i++)
	ok &= c4x_inst_add ((void *) &c4x_insts[i]);
    }

  /* Create dummy inst to avoid errors accessing end of table.  */
  c4x_inst_make ("", 0, "");

  if (!ok)
    as_fatal ("Broken assembler.  No assembly attempted.");

  /* Add registers to symbol table.  */
  c4x_init_regtable ();

  /* Add predefined symbols to symbol table.  */
  c4x_init_symbols ();
}

void 
c4x_end ()
{
  bfd_set_arch_mach (stdoutput, bfd_arch_tic4x, 
		     IS_CPU_C4X (c4x_cpu) ? bfd_mach_c4x : bfd_mach_c3x);
}

static int 
c4x_indirect_parse (operand, indirect)
     c4x_operand_t *operand;
     const c4x_indirect_t *indirect;
{
  char *n = indirect->name;
  char *s = input_line_pointer;
  char *b;
  symbolS *symbolP;
  char name[32];

  operand->disp = 0;
  for (; *n; n++)
    {
      switch (*n)
	{
	case 'a':		/* Need to match aux register.  */
	  b = name;
#ifdef C4X_ALT_SYNTAX
	  if (*s == '%')
	    s++;
#endif
	  while (isalnum (*s))
	    *b++ = *s++;
	  *b++ = '\0';
	  if (!(symbolP = symbol_find (name)))
	    return 0;

	  if (S_GET_SEGMENT (symbolP) != reg_section)
	    return 0;

	  operand->aregno = S_GET_VALUE (symbolP);
	  if (operand->aregno >= REG_AR0 && operand->aregno <= REG_AR7)
	    break;

	  as_bad ("Auxiliary register AR0--AR7 required for indirect");
	  return -1;

	case 'd':		/* Need to match constant for disp.  */
#ifdef C4X_ALT_SYNTAX
	  if (*s == '%')	/* expr() will die if we don't skip this.  */
	    s++;
#endif
	  s = c4x_expression (s, &operand->expr);
	  if (operand->expr.X_op != O_constant)
	    return 0;
	  operand->disp = operand->expr.X_add_number;
	  if (operand->disp < 0 || operand->disp > 255)
	    {
	      as_bad ("Bad displacement %d (require 0--255)\n",
		      operand->disp);
	      return -1;
	    }
	  break;

	case 'y':		/* Need to match IR0.  */
	case 'z':		/* Need to match IR1.  */
#ifdef C4X_ALT_SYNTAX
	  if (*s == '%')
	    s++;
#endif
	  s = c4x_expression (s, &operand->expr);
	  if (operand->expr.X_op != O_register)
	    return 0;
	  if (operand->expr.X_add_number != REG_IR0
	      && operand->expr.X_add_number != REG_IR1)
	    {
	      as_bad ("Index register IR0,IR1 required for displacement");
	      return -1;
	    }

	  if (*n == 'y' && operand->expr.X_add_number == REG_IR0)
	    break;
	  if (*n == 'z' && operand->expr.X_add_number == REG_IR1)
	    break;
	  return 0;

	case '(':
	  if (*s != '(')	/* No displacement, assume to be 1.  */
	    {
	      operand->disp = 1;
	      while (*n != ')')
		n++;
	    }
	  else
	    s++;
	  break;

	default:
	  if (tolower (*s) != *n)
	    return 0;
	  s++;
	}
    }
  if (*s != ' ' && *s != ',' && *s != '\0')
    return 0;
  input_line_pointer = s;
  return 1;
}

char *
c4x_operand_parse (s, operand)
     char *s;
     c4x_operand_t *operand;
{
  unsigned int i;
  char c;
  int ret;
  expressionS *exp = &operand->expr;
  char *save = input_line_pointer;
  char *str;
  char *new;
  struct hash_entry *entry = NULL;

  input_line_pointer = s;
  SKIP_WHITESPACE ();

  str = input_line_pointer;
  c = get_symbol_end ();	/* Get terminator.  */
  new = input_line_pointer;
  if (strlen (str) && (entry = hash_find (c4x_asg_hash, str)) != NULL)
    {
      *input_line_pointer = c;
      input_line_pointer = (char *) entry;
    }
  else
    {
      *input_line_pointer = c;
      input_line_pointer = str;
    }

  operand->mode = M_UNKNOWN;
  switch (*input_line_pointer)
    {
#ifdef C4X_ALT_SYNTAX
    case '%':
      input_line_pointer = c4x_expression (++input_line_pointer, exp);
      if (exp->X_op != O_register)
	as_bad ("Expecting a register name");
      operand->mode = M_REGISTER;
      break;

    case '^':
      /* Denotes high 16 bits.  */
      input_line_pointer = c4x_expression (++input_line_pointer, exp);
      if (exp->X_op == O_constant)
	operand->mode = M_IMMED;
      else if (exp->X_op == O_big)
	{
	  if (exp->X_add_number)
	    as_bad ("Number too large");	/* bignum required */
	  else
	    {
	      c4x_gen_to_words (generic_floating_point_number,
				operand->fwords, S_PRECISION);
	      operand->mode = M_IMMED_F;
	    }
	}
      /* Allow ori ^foo, ar0 to be equivalent to ldi .hi.foo, ar0  */
      /* WARNING : The TI C40 assembler cannot do this.  */
      else if (exp->X_op == O_symbol)
	{
	  operand->mode = M_HI;
	  break;
	}

    case '#':
      input_line_pointer = c4x_expression (++input_line_pointer, exp);
      if (exp->X_op == O_constant)
	operand->mode = M_IMMED;
      else if (exp->X_op == O_big)
	{
	  if (exp->X_add_number > 0)
	    as_bad ("Number too large");	/* bignum required.  */
	  else
	    {
	      c4x_gen_to_words (generic_floating_point_number,
				operand->fwords, S_PRECISION);
	      operand->mode = M_IMMED_F;
	    }
	}
      /* Allow ori foo, ar0 to be equivalent to ldi .lo.foo, ar0  */
      /* WARNING : The TI C40 assembler cannot do this.  */
      else if (exp->X_op == O_symbol)
	{
	  operand->mode = M_IMMED;
	  break;
	}

      else
	as_bad ("Expecting a constant value");
      break;
    case '\\':
#endif
    case '@':
      input_line_pointer = c4x_expression (++input_line_pointer, exp);
      if (exp->X_op != O_constant && exp->X_op != O_symbol)
	as_bad ("Bad direct addressing construct %s", s);
      if (exp->X_op == O_constant)
	{
	  if (exp->X_add_number < 0)
	    as_bad ("Direct value of %ld is not suitable",
		    (long) exp->X_add_number);
	}
      operand->mode = M_DIRECT;
      break;

    case '*':
      ret = -1;
      for (i = 0; i < num_indirects; i++)
	if ((ret = c4x_indirect_parse (operand, &c4x_indirects[i])))
	  break;
      if (ret < 0)
	break;
      if (i < num_indirects)
	{
	  operand->mode = M_INDIRECT;
	  /* Indirect addressing mode number.  */
	  operand->expr.X_add_number = c4x_indirects[i].modn;
	  /* Convert *+ARn(0) to *ARn etc.  Maybe we should
	     squeal about silly ones?  */
	  if (operand->expr.X_add_number < 0x08 && !operand->disp)
	    operand->expr.X_add_number = 0x18;
	}
      else
	as_bad ("Unknown indirect addressing mode");
      break;

    default:
      operand->mode = M_IMMED;	/* Assume immediate.  */
      str = input_line_pointer;
      input_line_pointer = c4x_expression (input_line_pointer, exp);
      if (exp->X_op == O_register)
	{
	  know (exp->X_add_symbol == 0);
	  know (exp->X_op_symbol == 0);
	  operand->mode = M_REGISTER;
	  break;
	}
      else if (exp->X_op == O_big)
	{
	  if (exp->X_add_number > 0)
	    as_bad ("Number too large");	/* bignum required.  */
	  else
	    {
	      c4x_gen_to_words (generic_floating_point_number,
				operand->fwords, S_PRECISION);
	      operand->mode = M_IMMED_F;
	    }
	  break;
	}
#ifdef C4X_ALT_SYNTAX
      /* Allow ldi foo, ar0 to be equivalent to ldi @foo, ar0.  */
      else if (exp->X_op == O_symbol)
	{
	  operand->mode = M_DIRECT;
	  break;
	}
#endif
    }
  if (entry == NULL)
    new = input_line_pointer;
  input_line_pointer = save;
  return new;
}

static int 
c4x_operands_match (inst, insn)
     c4x_inst_t *inst;
     c4x_insn_t *insn;
{
  const char *args = inst->args;
  unsigned long opcode = inst->opcode;
  int num_operands = insn->num_operands;
  c4x_operand_t *operand = insn->operands;
  expressionS *exp = &operand->expr;
  int ret = 1;
  int reg;

  /* Build the opcode, checking as we go to make sure that the
     operands match.

     If an operand matches, we modify insn or opcode appropriately,
     and do a "continue".  If an operand fails to match, we "break".  */

  insn->nchars = 4;		/* Instructions always 4 bytes.  */
  insn->reloc = NO_RELOC;
  insn->pcrel = 0;

  if (*args == '\0')
    {
      insn->opcode = opcode;
      return num_operands == 0;
    }

  for (;; ++args)
    {
      switch (*args)
	{

	case '\0':		/* End of args.  */
	  if (num_operands == 1)
	    {
	      insn->opcode = opcode;
	      return ret;
	    }
	  break;		/* Too many operands.  */

	case '#':		/* This is only used for ldp.  */
	  if (operand->mode != M_DIRECT && operand->mode != M_IMMED)
	    break;
	  /* While this looks like a direct addressing mode, we actually
	     use an immediate mode form of ldiu or ldpk instruction.  */
	  if (exp->X_op == O_constant)
	    {
	      /* Maybe for C3x we should check for 8 bit number.  */
	      INSERTS (opcode, exp->X_add_number, 15, 0);
	      continue;
	    }
	  else if (exp->X_op == O_symbol)
	    {
	      insn->reloc = BFD_RELOC_HI16;
	      insn->exp = *exp;
	      continue;
	    }
	  break;		/* Not direct (dp) addressing.  */

	case '@':		/* direct.  */
	  if (operand->mode != M_DIRECT)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      /* Store only the 16 LSBs of the number.  */
	      INSERTS (opcode, exp->X_add_number, 15, 0);
	      continue;
	    }
	  else if (exp->X_op == O_symbol)
	    {
	      insn->reloc = BFD_RELOC_LO16;
	      insn->exp = *exp;
	      continue;
	    }
	  break;		/* Not direct addressing.  */

	case 'A':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_AR0 && reg <= REG_AR7)
	    INSERTU (opcode, reg - REG_AR0, 24, 22);
	  else
	    {
	      as_bad ("Destination register must be ARn");
	      ret = -1;
	    }
	  continue;

	case 'B':		/* Unsigned integer immediate.  */
	  /* Allow br label or br @label.  */
	  if (operand->mode != M_IMMED && operand->mode != M_DIRECT)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number < (1 << 24))
		{
		  INSERTU (opcode, exp->X_add_number, 23, 0);
		  continue;
		}
	      else
		{
		  as_bad ("Immediate value of %ld is too large",
			  (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  if (IS_CPU_C4X (c4x_cpu))
	    {
	      insn->reloc = BFD_RELOC_24_PCREL;
	      insn->pcrel = 1;
	    }
	  else
	    {
	      insn->reloc = BFD_RELOC_24;
	      insn->pcrel = 0;
	    }
	  insn->exp = *exp;
	  continue;

	case 'C':
	  if (!IS_CPU_C4X (c4x_cpu))
	    break;
	  if (operand->mode != M_INDIRECT)
	    break;
	  if (operand->expr.X_add_number != 0
	      && operand->expr.X_add_number != 0x18)
	    {
	      as_bad ("Invalid indirect addressing mode");
	      ret = -1;
	      continue;
	    }
	  INSERTU (opcode, operand->aregno - REG_AR0, 2, 0);
	  INSERTU (opcode, operand->disp, 7, 3);
	  continue;

	case 'E':
	  if (!(operand->mode == M_REGISTER))
	    break;
	  INSERTU (opcode, exp->X_add_number, 7, 0);
	  continue;

	case 'F':
	  if (operand->mode != M_IMMED_F
	      && !(operand->mode == M_IMMED && exp->X_op == O_constant))
	    break;

	  if (operand->mode != M_IMMED_F)
	    {
	      /* OK, we 've got something like cmpf 0, r0
	         Why can't they stick in a bloody decimal point ?!  */
	      char string[16];

	      /* Create floating point number string.  */
	      sprintf (string, "%d.0", (int) exp->X_add_number);
	      c4x_atof (string, 's', operand->fwords);
	    }

	  INSERTU (opcode, operand->fwords[0], 15, 0);
	  continue;

	case 'G':
	  if (operand->mode != M_REGISTER)
	    break;
	  INSERTU (opcode, exp->X_add_number, 15, 8);
	  continue;

	case 'H':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_R0 && reg <= REG_R7)
	    INSERTU (opcode, reg - REG_R0, 18, 16);
	  else
	    {
	      as_bad ("Register must be R0--R7");
	      ret = -1;
	    }
	  continue;

	case 'I':
	  if (operand->mode != M_INDIRECT)
	    break;
	  if (operand->disp != 0 && operand->disp != 1)
	    {
	      if (IS_CPU_C4X (c4x_cpu))
		break;
	      as_bad ("Invalid indirect addressing mode displacement %d",
		      operand->disp);
	      ret = -1;
	      continue;
	    }
	  INSERTU (opcode, operand->aregno - REG_AR0, 2, 0);
	  INSERTU (opcode, operand->expr.X_add_number, 7, 3);
	  continue;

	case 'J':
	  if (operand->mode != M_INDIRECT)
	    break;
	  if (operand->disp != 0 && operand->disp != 1)
	    {
	      if (IS_CPU_C4X (c4x_cpu))
		break;
	      as_bad ("Invalid indirect addressing mode displacement %d",
		      operand->disp);
	      ret = -1;
	      continue;
	    }
	  INSERTU (opcode, operand->aregno - REG_AR0, 10, 8);
	  INSERTU (opcode, operand->expr.X_add_number, 15, 11);
	  continue;

	case 'K':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_R0 && reg <= REG_R7)
	    INSERTU (opcode, reg - REG_R0, 21, 19);
	  else
	    {
	      as_bad ("Register must be R0--R7");
	      ret = -1;
	    }
	  continue;

	case 'L':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_R0 && reg <= REG_R7)
	    INSERTU (opcode, reg - REG_R0, 24, 22);
	  else
	    {
	      as_bad ("Register must be R0--R7");
	      ret = -1;
	    }
	  continue;

	case 'M':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg == REG_R2 || reg == REG_R3)
	    INSERTU (opcode, reg - REG_R2, 22, 22);
	  else
	    {
	      as_bad ("Destination register must be R2 or R3");
	      ret = -1;
	    }
	  continue;

	case 'N':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg == REG_R0 || reg == REG_R1)
	    INSERTU (opcode, reg - REG_R0, 23, 23);
	  else
	    {
	      as_bad ("Destination register must be R0 or R1");
	      ret = -1;
	    }
	  continue;

	case 'O':
	  if (!IS_CPU_C4X (c4x_cpu))
	    break;
	  if (operand->mode != M_INDIRECT)
	    break;
	  /* Require either *+ARn(disp) or *ARn.  */
	  if (operand->expr.X_add_number != 0
	      && operand->expr.X_add_number != 0x18)
	    {
	      as_bad ("Invalid indirect addressing mode");
	      ret = -1;
	      continue;
	    }
	  INSERTU (opcode, operand->aregno - REG_AR0, 10, 8);
	  INSERTU (opcode, operand->disp, 15, 11);
	  continue;

	case 'P':		/* PC relative displacement.  */
	  /* Allow br label or br @label.  */
	  if (operand->mode != M_IMMED && operand->mode != M_DIRECT)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number >= -32768 && exp->X_add_number <= 32767)
		{
		  INSERTS (opcode, exp->X_add_number, 15, 0);
		  continue;
		}
	      else
		{
		  as_bad ("Displacement value of %ld is too large",
			  (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  insn->reloc = BFD_RELOC_16_PCREL;
	  insn->pcrel = 1;
	  insn->exp = *exp;
	  continue;

	case 'Q':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  INSERTU (opcode, reg, 15, 0);
	  continue;

	case 'R':
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  INSERTU (opcode, reg, 20, 16);
	  continue;

	case 'S':		/* Short immediate int.  */
	  if (operand->mode != M_IMMED && operand->mode != M_HI)
	    break;
	  if (exp->X_op == O_big)
	    {
	      as_bad ("Floating point number not valid in expression");
	      ret = -1;
	      continue;
	    }
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number >= -32768 && exp->X_add_number <= 65535)
		{
		  INSERTS (opcode, exp->X_add_number, 15, 0);
		  continue;
		}
	      else
		{
		  as_bad ("Signed immediate value %ld too large",
			  (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  else if (exp->X_op == O_symbol)
	    {
	      if (operand->mode == M_HI)
		{
		  insn->reloc = BFD_RELOC_HI16;
		}
	      else
		{
		  insn->reloc = BFD_RELOC_LO16;
		}
	      insn->exp = *exp;
	      continue;
	    }
	  /* Handle cases like ldi foo - $, ar0  where foo
	     is a forward reference.  Perhaps we should check
	     for X_op == O_symbol and disallow things like
	     ldi foo, ar0.  */
	  insn->reloc = BFD_RELOC_16;
	  insn->exp = *exp;
	  continue;

	case 'T':		/* 5-bit immediate value for c4x stik.  */
	  if (!IS_CPU_C4X (c4x_cpu))
	    break;
	  if (operand->mode != M_IMMED)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number < 16 && exp->X_add_number >= -16)
		{
		  INSERTS (opcode, exp->X_add_number, 20, 16);
		  continue;
		}
	      else
		{
		  as_bad ("Immediate value of %ld is too large",
			  (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  break;		/* No relocations allowed.  */

	case 'U':		/* Unsigned integer immediate.  */
	  if (operand->mode != M_IMMED && operand->mode != M_HI)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number < (1 << 16) && exp->X_add_number >= 0)
		{
		  INSERTU (opcode, exp->X_add_number, 15, 0);
		  continue;
		}
	      else
		{
		  as_bad ("Unsigned immediate value %ld too large",
			  (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  else if (exp->X_op == O_symbol)
	    {
	      if (operand->mode == M_HI)
		insn->reloc = BFD_RELOC_HI16;
	      else
		insn->reloc = BFD_RELOC_LO16;

	      insn->exp = *exp;
	      continue;
	    }
	  insn->reloc = BFD_RELOC_16;
	  insn->exp = *exp;
	  continue;

	case 'V':		/* Trap numbers (immediate field).  */
	  if (operand->mode != M_IMMED)
	    break;
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number < 512 && IS_CPU_C4X (c4x_cpu))
		{
		  INSERTU (opcode, exp->X_add_number, 8, 0);
		  continue;
		}
	      else if (exp->X_add_number < 32 && IS_CPU_C3X (c4x_cpu))
		{
		  INSERTU (opcode, exp->X_add_number | 0x20, 4, 0);
		  continue;
		}
	      else
		{
		  as_bad ("Immediate value of %ld is too large",
			  (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  break;		/* No relocations allowed.  */

	case 'W':		/* Short immediate int (0--7).  */
	  if (!IS_CPU_C4X (c4x_cpu))
	    break;
	  if (operand->mode != M_IMMED)
	    break;
	  if (exp->X_op == O_big)
	    {
	      as_bad ("Floating point number not valid in expression");
	      ret = -1;
	      continue;
	    }
	  if (exp->X_op == O_constant)
	    {
	      if (exp->X_add_number >= -256 && exp->X_add_number <= 127)
		{
		  INSERTS (opcode, exp->X_add_number, 7, 0);
		  continue;
		}
	      else
		{
		  as_bad ("Immediate value %ld too large",
			  (long) exp->X_add_number);
		  ret = -1;
		  continue;
		}
	    }
	  insn->reloc = BFD_RELOC_16;
	  insn->exp = *exp;
	  continue;

	case 'X':		/* Expansion register for c4x.  */
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_IVTP && reg <= REG_TVTP)
	    INSERTU (opcode, reg - REG_IVTP, 4, 0);
	  else
	    {
	      as_bad ("Register must be ivtp or tvtp");
	      ret = -1;
	    }
	  continue;

	case 'Y':		/* Address register for c4x lda.  */
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_AR0 && reg <= REG_SP)
	    INSERTU (opcode, reg, 20, 16);
	  else
	    {
	      as_bad ("Register must be address register");
	      ret = -1;
	    }
	  continue;

	case 'Z':		/* Expansion register for c4x.  */
	  if (operand->mode != M_REGISTER)
	    break;
	  reg = exp->X_add_number;
	  if (reg >= REG_IVTP && reg <= REG_TVTP)
	    INSERTU (opcode, reg - REG_IVTP, 20, 16);
	  else
	    {
	      as_bad ("Register must be ivtp or tvtp");
	      ret = -1;
	    }
	  continue;

	case '*':
	  if (operand->mode != M_INDIRECT)
	    break;
	  INSERTS (opcode, operand->disp, 7, 0);
	  INSERTU (opcode, operand->aregno - REG_AR0, 10, 8);
	  INSERTU (opcode, operand->expr.X_add_number, 15, 11);
	  continue;

	case '|':		/* treat as `,' if have ldi_ldi form.  */
	  if (insn->parallel)
	    {
	      if (--num_operands < 0)
		break;		/* Too few operands.  */
	      operand++;
	      if (operand->mode != M_PARALLEL)
		break;
	    }
	  /* Fall through.  */

	case ',':		/* Another operand.  */
	  if (--num_operands < 0)
	    break;		/* Too few operands.  */
	  operand++;
	  exp = &operand->expr;
	  continue;

	case ';':		/* Another optional operand.  */
	  if (num_operands == 1 || operand[1].mode == M_PARALLEL)
	    continue;
	  if (--num_operands < 0)
	    break;		/* Too few operands.  */
	  operand++;
	  exp = &operand->expr;
	  continue;

	default:
	  BAD_CASE (*args);
	}
      return 0;
    }
}

void 
c4x_insn_output (insn)
     c4x_insn_t *insn;
{
  char *dst;

  /* Grab another fragment for opcode.  */
  dst = frag_more (insn->nchars);

  /* Put out opcode word as a series of bytes in little endian order.  */
  md_number_to_chars (dst, insn->opcode, insn->nchars);

  /* Put out the symbol-dependent stuff.  */
  if (insn->reloc != NO_RELOC)
    {
      /* Where is the offset into the fragment for this instruction.  */
      fix_new_exp (frag_now,
		   dst - frag_now->fr_literal,	/* where */
		   insn->nchars,	/* size */
		   &insn->exp,
		   insn->pcrel,
		   insn->reloc);
    }
}

/* Parse the operands.  */
int 
c4x_operands_parse (s, operands, num_operands)
     char *s;
     c4x_operand_t *operands;
     int num_operands;
{
  if (!*s)
    return num_operands;

  do
    s = c4x_operand_parse (s, &operands[num_operands++]);
  while (num_operands < C4X_OPERANDS_MAX && *s++ == ',');

  if (num_operands > C4X_OPERANDS_MAX)
    {
      as_bad ("Too many operands scanned");
      return -1;
    }
  return num_operands;
}

/* Assemble a single instruction.  Its label has already been handled
   by the generic front end.  We just parse mnemonic and operands, and
   produce the bytes of data and relocation.  */
void 
md_assemble (str)
     char *str;
{
  int ok = 0;
  char *s;
  int i;
  int parsed = 0;
  c4x_inst_t *inst;		/* Instruction template.  */

  if (str && insn->parallel)
    {
      int star;

      /* Find mnemonic (second part of parallel instruction).  */
      s = str;
      /* Skip past instruction mnemonic.  */
      while (*s && *s != ' ' && *s != '*')
	s++;
      star = *s == '*';
      if (*s)			/* Null terminate for hash_find.  */
	*s++ = '\0';		/* and skip past null.  */
      strcat (insn->name, "_");
      strncat (insn->name, str, C4X_NAME_MAX - strlen (insn->name));

      /* Kludge to overcome problems with scrubber removing
         space between mnemonic and indirect operand (starting with *)
         on second line of parallel instruction.  */
      if (star)
	*--s = '*';

      insn->operands[insn->num_operands++].mode = M_PARALLEL;

      if ((i = c4x_operands_parse
	   (s, insn->operands, insn->num_operands)) < 0)
	{
	  insn->parallel = 0;
	  insn->in_use = 0;
	  return;
	}
      insn->num_operands = i;
      parsed = 1;
    }

  if (insn->in_use)
    {
      if ((insn->inst = (struct c4x_inst *)
	   hash_find (c4x_op_hash, insn->name)) == NULL)
	{
	  as_bad ("Unknown opcode `%s'.", insn->name);
	  insn->parallel = 0;
	  insn->in_use = 0;
	  return;
	}

      /* FIXME:  The list of templates should be scanned
         for the candidates with the desired number of operands.
         We shouldn't issue error messages until we have
         whittled the list of candidate templates to the most
         likely one...  We could cache a parsed form of the templates
         to reduce the time required to match a template.  */

      inst = insn->inst;

      do
	ok = c4x_operands_match (inst, insn);
      while (!ok && !strcmp (inst->name, inst[1].name) && inst++);

      if (ok > 0)
	c4x_insn_output (insn);
      else if (!ok)
	as_bad ("Invalid operands for %s", insn->name);
      else
	as_bad ("Invalid instruction %s", insn->name);
    }

  if (str && !parsed)
    {
      /* Find mnemonic.  */
      s = str;
      while (*s && *s != ' ')	/* Skip past instruction mnemonic.  */
	s++;
      if (*s)			/* Null terminate for hash_find.  */
	*s++ = '\0';		/* and skip past null.  */
      strncpy (insn->name, str, C4X_NAME_MAX - 3);

      if ((i = c4x_operands_parse (s, insn->operands, 0)) < 0)
	{
	  insn->inst = NULL;	/* Flag that error occured.  */
	  insn->parallel = 0;
	  insn->in_use = 0;
	  return;
	}
      insn->num_operands = i;
      insn->in_use = 1;
    }
  else
    insn->in_use = 0;
  insn->parallel = 0;
}

void
c4x_cleanup ()
{
  if (insn->in_use)
    md_assemble (NULL);
}

/* Turn a string in input_line_pointer into a floating point constant
   of type type, and store the appropriate bytes in *litP.  The number
   of LITTLENUMS emitted is stored in *sizeP.  An error message is
   returned, or NULL on OK.  */

char *
md_atof (type, litP, sizeP)
     int type;
     char *litP;
     int *sizeP;
{
  int prec;
  int ieee;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  unsigned char *t;

  switch (type)
    {
    case 's':			/* .single */
    case 'S':
      ieee = 0;
      prec = 1;
      break;

    case 'd':			/* .double */
    case 'D':
    case 'f':			/* .float or .single */
    case 'F':
      ieee = 0;
      prec = 2;			/* 1 32-bit word */
      break;

    case 'i':			/* .ieee */
      prec = 2;
      ieee = 1;
      break;

    case 'l':			/* .ldouble */
      prec = 4;			/* 2 32-bit words */
      ieee = 1;
      break;

    default:
      *sizeP = 0;
      return "Bad call to md_atof()";
    }

  if (ieee)
    t = atof_ieee (input_line_pointer, type, words);
  else
    t = c4x_atof (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = prec * sizeof (LITTLENUM_TYPE);
  t = litP;
  /* This loops outputs the LITTLENUMs in REVERSE order; in accord with
     little endian byte order.  */
  for (wordP = words + prec - 1; prec--;)
    {
      md_number_to_chars (litP, (valueT) (*wordP--),
			  sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
  return 0;
}

void 
md_apply_fix3 (fixP, value, seg)
     fixS *fixP;
     valueT *value;
     segT seg ATTRIBUTE_UNUSED;
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  valueT val = *value;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_HI16:
      val >>= 16;
      break;

    case BFD_RELOC_LO16:
      val &= 0xffff;
      break;
    default:
      break;
    }

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_32:
      buf[3] = val >> 24;
    case BFD_RELOC_24:
    case BFD_RELOC_24_PCREL:
      buf[2] = val >> 16;
    case BFD_RELOC_16:
    case BFD_RELOC_16_PCREL:
    case BFD_RELOC_LO16:
    case BFD_RELOC_HI16:
      buf[1] = val >> 8;
      buf[0] = val;
      break;

    case NO_RELOC:
    default:
      as_bad ("Bad relocation type: 0x%02x", fixP->fx_r_type);
      break;
    }

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0) fixP->fx_done = 1;
}

/* Should never be called for c4x.  */
void 
md_convert_frag (headers, sec, fragP)
     bfd *headers ATTRIBUTE_UNUSED;
     segT sec ATTRIBUTE_UNUSED;
     fragS *fragP ATTRIBUTE_UNUSED;
{
  as_fatal ("md_convert_frag");
}

/* Should never be called for c4x.  */
void
md_create_short_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr ATTRIBUTE_UNUSED;
     addressT from_addr ATTRIBUTE_UNUSED;
     addressT to_addr ATTRIBUTE_UNUSED;
     fragS *frag ATTRIBUTE_UNUSED;
     symbolS *to_symbol ATTRIBUTE_UNUSED;
{
  as_fatal ("md_create_short_jmp\n");
}

/* Should never be called for c4x.  */
void
md_create_long_jump (ptr, from_addr, to_addr, frag, to_symbol)
     char *ptr ATTRIBUTE_UNUSED;
     addressT from_addr ATTRIBUTE_UNUSED;
     addressT to_addr ATTRIBUTE_UNUSED;
     fragS *frag ATTRIBUTE_UNUSED;
     symbolS *to_symbol ATTRIBUTE_UNUSED;
{
  as_fatal ("md_create_long_jump\n");
}

/* Should never be called for c4x.  */
int
md_estimate_size_before_relax (fragP, segtype)
     register fragS *fragP ATTRIBUTE_UNUSED;
     segT segtype ATTRIBUTE_UNUSED;
{
  as_fatal ("md_estimate_size_before_relax\n");
  return 0;
}

CONST char *md_shortopts = "bm:prs";
struct option md_longopts[] =
{
  {NULL, no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case 'b':			/* big model */
      c4x_big_model = 1;
      break;
    case 'm':			/* -m[c][34]x */
      if (tolower (*arg) == 'c')
	arg++;
      c4x_cpu = atoi (arg);
      if (!IS_CPU_C3X (c4x_cpu) && !IS_CPU_C4X (c4x_cpu))
	as_warn ("Unsupported processor generation %d\n", c4x_cpu);
      break;
    case 'p':			/* push args */
      c4x_reg_args = 0;
      break;
    case 'r':			/* register args */
      c4x_reg_args = 1;
      break;
    case 's':			/* small model */
      c4x_big_model = 0;
      break;
    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (stream)
     FILE *stream;
{
  fputs ("\
C[34]x options:\n\
-m30 | -m31 | -m32 | -m40 | -m44\n\
			specify variant of architecture\n\
-b                      big memory model\n\
-p                      pass arguments on stack\n\
-r                      pass arguments in registers (default)\n\
-s                      small memory model (default)\n",
	 stream);
}

/* This is called when a line is unrecognized.  This is used to handle
   definitions of TI C3x tools style local labels $n where n is a single
   decimal digit.  */
int 
c4x_unrecognized_line (c)
     int c;
{
  int lab;
  char *s;

  if (c != '$' || !isdigit (input_line_pointer[0]))
    return 0;

  s = input_line_pointer;

  /* Let's allow multiple digit local labels.  */
  lab = 0;
  while (isdigit (*s))
    {
      lab = lab * 10 + *s - '0';
      s++;
    }

  if (dollar_label_defined (lab))
    {
      as_bad ("Label \"$%d\" redefined", lab);
      return 0;
    }

  define_dollar_label (lab);
  colon (dollar_label_name (lab, 0));
  input_line_pointer = s + 1;

  return 1;
}

/* Handle local labels peculiar to us referred to in an expression.  */
symbolS *
md_undefined_symbol (name)
     char *name;
{
  /* Look for local labels of the form $n.  */
  if (name[0] == '$' && isdigit (name[1]))
    {
      symbolS *symbolP;
      char *s = name + 1;
      int lab = 0;

      while (isdigit ((unsigned char) *s))
	{
	  lab = lab * 10 + *s - '0';
	  s++;
	}
      if (dollar_label_defined (lab))
	{
	  name = dollar_label_name (lab, 0);
	  symbolP = symbol_find (name);
	}
      else
	{
	  name = dollar_label_name (lab, 1);
	  symbolP = symbol_find_or_make (name);
	}

      return symbolP;
    }
  return NULL;
}

/* Parse an operand that is machine-specific.  */
void
md_operand (expressionP)
     expressionS *expressionP ATTRIBUTE_UNUSED;
{
}

/* Round up a section size to the appropriate boundary---do we need this?  */
valueT
md_section_align (segment, size)
     segT segment ATTRIBUTE_UNUSED;
     valueT size;
{
  return size;			/* Byte (i.e., 32-bit) alignment is fine?  */
}

static int 
c4x_pc_offset (op)
     unsigned int op;
{
  /* Determine the PC offset for a C[34]x instruction.
     This could be simplified using some boolean algebra
     but at the expense of readability.  */
  switch (op >> 24)
    {
    case 0x60:			/* br */
    case 0x62:			/* call  (C4x) */
    case 0x64:			/* rptb  (C4x) */
      return 1;
    case 0x61:			/* brd */
    case 0x63:			/* laj */
    case 0x65:			/* rptbd (C4x) */
      return 3;
    case 0x66:			/* swi */
    case 0x67:
      return 0;
    default:
      break;
    }

  switch ((op & 0xffe00000) >> 20)
    {
    case 0x6a0:		/* bB */
    case 0x720:		/* callB */
    case 0x740:		/* trapB */
      return 1;

    case 0x6a2:		/* bBd */
    case 0x6a6:		/* bBat */
    case 0x6aa:		/* bBaf */
    case 0x722:		/* lajB */
    case 0x748:		/* latB */
    case 0x798:		/* rptbd */
      return 3;

    default:
      break;
    }

  switch ((op & 0xfe200000) >> 20)
    {
    case 0x6e0:		/* dbB */
      return 1;

    case 0x6e2:		/* dbBd */
      return 3;

    default:
      break;
    }

  return 0;
}

/* Exactly what point is a PC-relative offset relative TO?
   With the C3x we have the following:
   DBcond,  Bcond   disp + PC + 1 => PC
   DBcondD, BcondD  disp + PC + 3 => PC
 */
long
md_pcrel_from (fixP)
     fixS *fixP;
{
  unsigned char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  unsigned int op;

  op = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];

  return ((fixP->fx_where + fixP->fx_frag->fr_address) >> 2) +
    c4x_pc_offset (op);
}

/* This is probably not necessary, if we have played our cards right,
   since everything should be already aligned on a 4-byte boundary.  */
int 
c4x_do_align (alignment, fill, len, max)
     int alignment ATTRIBUTE_UNUSED;
     const char *fill ATTRIBUTE_UNUSED;
     int len ATTRIBUTE_UNUSED;
     int max ATTRIBUTE_UNUSED;
{
  char *p;

  p = frag_var (rs_align, 1, 1, (relax_substateT) 0,
		(symbolS *) 0, (long) 2, (char *) 0);

  /* We could use frag_align_pattern (n, nop_pattern, sizeof (nop_pattern));
     to fill with our 32-bit nop opcode.  */
  return 1;
}

/* Look for and remove parallel instruction operator ||.  */
void 
c4x_start_line ()
{
  char *s = input_line_pointer;

  SKIP_WHITESPACE ();

  /* If parallel instruction prefix found at start of line, skip it.  */
  if (*input_line_pointer == '|' && input_line_pointer[1] == '|')
    {
      if (insn->in_use)
	{
	  insn->parallel = 1;
	  input_line_pointer += 2;
	  /* So line counters get bumped.  */
	  input_line_pointer[-1] = '\n';
	}
    }
  else
    {
      if (insn->in_use)
	md_assemble (NULL);
      input_line_pointer = s;
    }
}

arelent *
tc_gen_reloc (seg, fixP)
     asection *seg ATTRIBUTE_UNUSED;
     fixS *fixP;
{
  arelent *reloc;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  reloc->address = fixP->fx_frag->fr_address + fixP->fx_where;
  reloc->address /= OCTETS_PER_BYTE;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    "reloc %d not supported by object file format",
		    (int) fixP->fx_r_type);
      return NULL;
    }

  if (fixP->fx_r_type == BFD_RELOC_HI16)
    reloc->addend = fixP->fx_offset;
  else
    reloc->addend = fixP->fx_addnumber;

  return reloc;
}
