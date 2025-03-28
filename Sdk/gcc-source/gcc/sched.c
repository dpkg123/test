/* Instruction scheduling pass.
   Copyright (C) 1992, 93-96, 1997 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com)
   Enhanced by, and currently maintained by, Jim Wilson (wilson@cygnus.com)

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

/* Instruction scheduling pass.

   This pass implements list scheduling within basic blocks.  It is
   run after flow analysis, but before register allocation.  The
   scheduler works as follows:

   We compute insn priorities based on data dependencies.  Flow
   analysis only creates a fraction of the data-dependencies we must
   observe: namely, only those dependencies which the combiner can be
   expected to use.  For this pass, we must therefore create the
   remaining dependencies we need to observe: register dependencies,
   memory dependencies, dependencies to keep function calls in order,
   and the dependence between a conditional branch and the setting of
   condition codes are all dealt with here.

   The scheduler first traverses the data flow graph, starting with
   the last instruction, and proceeding to the first, assigning
   values to insn_priority as it goes.  This sorts the instructions
   topologically by data dependence.

   Once priorities have been established, we order the insns using
   list scheduling.  This works as follows: starting with a list of
   all the ready insns, and sorted according to priority number, we
   schedule the insn from the end of the list by placing its
   predecessors in the list according to their priority order.  We
   consider this insn scheduled by setting the pointer to the "end" of
   the list to point to the previous insn.  When an insn has no
   predecessors, we either queue it until sufficient time has elapsed
   or add it to the ready list.  As the instructions are scheduled or
   when stalls are introduced, the queue advances and dumps insns into
   the ready list.  When all insns down to the lowest priority have
   been scheduled, the critical path of the basic block has been made
   as short as possible.  The remaining insns are then scheduled in
   remaining slots.

   Function unit conflicts are resolved during reverse list scheduling
   by tracking the time when each insn is committed to the schedule
   and from that, the time the function units it uses must be free.
   As insns on the ready list are considered for scheduling, those
   that would result in a blockage of the already committed insns are
   queued until no blockage will result.  Among the remaining insns on
   the ready list to be considered, the first one with the largest
   potential for causing a subsequent blockage is chosen.

   The following list shows the order in which we want to break ties
   among insns in the ready list:

	1.  choose insn with lowest conflict cost, ties broken by
	2.  choose insn with the longest path to end of bb, ties broken by
	3.  choose insn that kills the most registers, ties broken by
	4.  choose insn that conflicts with the most ready insns, or finally
	5.  choose insn with lowest UID.

   Memory references complicate matters.  Only if we can be certain
   that memory references are not part of the data dependency graph
   (via true, anti, or output dependence), can we move operations past
   memory references.  To first approximation, reads can be done
   independently, while writes introduce dependencies.  Better
   approximations will yield fewer dependencies.

   Dependencies set up by memory references are treated in exactly the
   same way as other dependencies, by using LOG_LINKS.

   Having optimized the critical path, we may have also unduly
   extended the lifetimes of some registers.  If an operation requires
   that constants be loaded into registers, it is certainly desirable
   to load those constants as early as necessary, but no earlier.
   I.e., it will not do to load up a bunch of registers at the
   beginning of a basic block only to use them at the end, if they
   could be loaded later, since this may result in excessive register
   utilization.

   Note that since branches are never in basic blocks, but only end
   basic blocks, this pass will not do any branch scheduling.  But
   that is ok, since we can use GNU's delayed branch scheduling
   pass to take care of this case.

   Also note that no further optimizations based on algebraic identities
   are performed, so this pass would be a good one to perform instruction
   splitting, such as breaking up a multiply instruction into shifts
   and adds where that is profitable.

   Given the memory aliasing analysis that this pass should perform,
   it should be possible to remove redundant stores to memory, and to
   load values from registers instead of hitting memory.

   This pass must update information that subsequent passes expect to be
   correct.  Namely: reg_n_refs, reg_n_sets, reg_n_deaths,
   reg_n_calls_crossed, and reg_live_length.  Also, basic_block_head,
   basic_block_end.

   The information in the line number notes is carefully retained by
   this pass.  Notes that refer to the starting and ending of
   exception regions are also carefully retained by this pass.  All
   other NOTE insns are grouped in their same relative order at the
   beginning of basic blocks that have been scheduled.  */

#include <stdio.h>
#include "config.h"
#include "rtl.h"
#include "basic-block.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "flags.h"
#include "insn-config.h"
#include "insn-attr.h"

extern char *reg_known_equiv_p;
extern rtx *reg_known_value;

#define SIZE_FOR_MODE(X) (GET_MODE_SIZE (GET_MODE (X)))

#ifdef INSN_SCHEDULING
/* Arrays set up by scheduling for the same respective purposes as
   similar-named arrays set up by flow analysis.  We work with these
   arrays during the scheduling pass so we can compare values against
   unscheduled code.

   Values of these arrays are copied at the end of this pass into the
   arrays set up by flow analysis.  */
static short *sched_reg_n_deaths;
static int *sched_reg_n_calls_crossed;
static int *sched_reg_live_length;

/* Element N is the next insn that sets (hard or pseudo) register
   N within the current basic block; or zero, if there is no
   such insn.  Needed for new registers which may be introduced
   by splitting insns.  */
static rtx *reg_last_uses;
static int reg_last_uses_size;
static rtx *reg_last_sets;
static regset reg_pending_sets;
static int reg_pending_sets_all;

/* Vector indexed by INSN_UID giving the original ordering of the insns.  */
static int *insn_luid;
#define INSN_LUID(INSN) (insn_luid[INSN_UID (INSN)])

/* Vector indexed by INSN_UID giving each instruction a priority.  */
static int *insn_priority;
#define INSN_PRIORITY(INSN) (insn_priority[INSN_UID (INSN)])

static short *insn_costs;
#define INSN_COST(INSN)	insn_costs[INSN_UID (INSN)]

/* Vector indexed by INSN_UID giving an encoding of the function units
   used.  */
static short *insn_units;
#define INSN_UNIT(INSN)	insn_units[INSN_UID (INSN)]

/* Vector indexed by INSN_UID giving an encoding of the blockage range
   function.  The unit and the range are encoded.  */
static unsigned int *insn_blockage;
#define INSN_BLOCKAGE(INSN) insn_blockage[INSN_UID (INSN)]
#define UNIT_BITS 5
#define BLOCKAGE_MASK ((1 << BLOCKAGE_BITS) - 1)
#define ENCODE_BLOCKAGE(U,R)				\
  ((((U) << UNIT_BITS) << BLOCKAGE_BITS			\
    | MIN_BLOCKAGE_COST (R)) << BLOCKAGE_BITS		\
   | MAX_BLOCKAGE_COST (R))
#define UNIT_BLOCKED(B) ((B) >> (2 * BLOCKAGE_BITS))
#define BLOCKAGE_RANGE(B) \
  (((((B) >> BLOCKAGE_BITS) & BLOCKAGE_MASK) << (HOST_BITS_PER_INT / 2)) \
   | (B) & BLOCKAGE_MASK)

/* Encodings of the `<name>_unit_blockage_range' function.  */
#define MIN_BLOCKAGE_COST(R) ((R) >> (HOST_BITS_PER_INT / 2))
#define MAX_BLOCKAGE_COST(R) ((R) & ((1 << (HOST_BITS_PER_INT / 2)) - 1))

#define DONE_PRIORITY	-1
#define MAX_PRIORITY	0x7fffffff
#define TAIL_PRIORITY	0x7ffffffe
#define LAUNCH_PRIORITY	0x7f000001
#define DONE_PRIORITY_P(INSN) (INSN_PRIORITY (INSN) < 0)
#define LOW_PRIORITY_P(INSN) ((INSN_PRIORITY (INSN) & 0x7f000000) == 0)

/* Vector indexed by INSN_UID giving number of insns referring to this insn.  */
static int *insn_ref_count;
#define INSN_REF_COUNT(INSN) (insn_ref_count[INSN_UID (INSN)])

/* Vector indexed by INSN_UID giving line-number note in effect for each
   insn.  For line-number notes, this indicates whether the note may be
   reused.  */
static rtx *line_note;
#define LINE_NOTE(INSN) (line_note[INSN_UID (INSN)])

/* Vector indexed by basic block number giving the starting line-number
   for each basic block.  */
static rtx *line_note_head;

/* List of important notes we must keep around.  This is a pointer to the
   last element in the list.  */
static rtx note_list;

/* Regsets telling whether a given register is live or dead before the last
   scheduled insn.  Must scan the instructions once before scheduling to
   determine what registers are live or dead at the end of the block.  */
static regset bb_dead_regs;
static regset bb_live_regs;

/* Regset telling whether a given register is live after the insn currently
   being scheduled.  Before processing an insn, this is equal to bb_live_regs
   above.  This is used so that we can find registers that are newly born/dead
   after processing an insn.  */
static regset old_live_regs;

/* The chain of REG_DEAD notes.  REG_DEAD notes are removed from all insns
   during the initial scan and reused later.  If there are not exactly as
   many REG_DEAD notes in the post scheduled code as there were in the
   prescheduled code then we trigger an abort because this indicates a bug.  */
static rtx dead_notes;

/* Queues, etc.  */

/* An instruction is ready to be scheduled when all insns following it
   have already been scheduled.  It is important to ensure that all
   insns which use its result will not be executed until its result
   has been computed.  An insn is maintained in one of four structures:

   (P) the "Pending" set of insns which cannot be scheduled until
   their dependencies have been satisfied.
   (Q) the "Queued" set of insns that can be scheduled when sufficient
   time has passed.
   (R) the "Ready" list of unscheduled, uncommitted insns.
   (S) the "Scheduled" list of insns.

   Initially, all insns are either "Pending" or "Ready" depending on
   whether their dependencies are satisfied.

   Insns move from the "Ready" list to the "Scheduled" list as they
   are committed to the schedule.  As this occurs, the insns in the
   "Pending" list have their dependencies satisfied and move to either
   the "Ready" list or the "Queued" set depending on whether
   sufficient time has passed to make them ready.  As time passes,
   insns move from the "Queued" set to the "Ready" list.  Insns may
   move from the "Ready" list to the "Queued" set if they are blocked
   due to a function unit conflict.

   The "Pending" list (P) are the insns in the LOG_LINKS of the unscheduled
   insns, i.e., those that are ready, queued, and pending.
   The "Queued" set (Q) is implemented by the variable `insn_queue'.
   The "Ready" list (R) is implemented by the variables `ready' and
   `n_ready'.
   The "Scheduled" list (S) is the new insn chain built by this pass.

   The transition (R->S) is implemented in the scheduling loop in
   `schedule_block' when the best insn to schedule is chosen.
   The transition (R->Q) is implemented in `schedule_select' when an
   insn is found to to have a function unit conflict with the already
   committed insns.
   The transitions (P->R and P->Q) are implemented in `schedule_insn' as
   insns move from the ready list to the scheduled list.
   The transition (Q->R) is implemented at the top of the scheduling
   loop in `schedule_block' as time passes or stalls are introduced.  */

/* Implement a circular buffer to delay instructions until sufficient
   time has passed.  INSN_QUEUE_SIZE is a power of two larger than
   MAX_BLOCKAGE and MAX_READY_COST computed by genattr.c.  This is the
   longest time an isnsn may be queued.  */
static rtx insn_queue[INSN_QUEUE_SIZE];
static int q_ptr = 0;
static int q_size = 0;
#define NEXT_Q(X) (((X)+1) & (INSN_QUEUE_SIZE-1))
#define NEXT_Q_AFTER(X,C) (((X)+C) & (INSN_QUEUE_SIZE-1))

/* Vector indexed by INSN_UID giving the minimum clock tick at which
   the insn becomes ready.  This is used to note timing constraints for
   insns in the pending list.  */
static int *insn_tick;
#define INSN_TICK(INSN) (insn_tick[INSN_UID (INSN)])

/* Data structure for keeping track of register information
   during that register's life.  */

struct sometimes
{
  short offset; short bit;
  short live_length; short calls_crossed;
};

/* Forward declarations.  */
static void add_dependence		PROTO((rtx, rtx, enum reg_note));
static void remove_dependence		PROTO((rtx, rtx));
static rtx find_insn_list		PROTO((rtx, rtx));
static int insn_unit			PROTO((rtx));
static unsigned int blockage_range	PROTO((int, rtx));
static void clear_units			PROTO((void));
static void prepare_unit		PROTO((int));
static int actual_hazard_this_instance	PROTO((int, int, rtx, int, int));
static void schedule_unit		PROTO((int, rtx, int));
static int actual_hazard		PROTO((int, rtx, int, int));
static int potential_hazard		PROTO((int, rtx, int));
static int insn_cost			PROTO((rtx, rtx, rtx));
static int priority			PROTO((rtx));
static void free_pending_lists		PROTO((void));
static void add_insn_mem_dependence	PROTO((rtx *, rtx *, rtx, rtx));
static void flush_pending_lists		PROTO((rtx, int));
static void sched_analyze_1		PROTO((rtx, rtx));
static void sched_analyze_2		PROTO((rtx, rtx));
static void sched_analyze_insn		PROTO((rtx, rtx, rtx));
static int sched_analyze		PROTO((rtx, rtx));
static void sched_note_set		PROTO((int, rtx, int));
static int rank_for_schedule		PROTO((rtx *, rtx *));
static void swap_sort			PROTO((rtx *, int));
static void queue_insn			PROTO((rtx, int));
static int birthing_insn		PROTO((rtx));
static void adjust_priority		PROTO((rtx));
static int schedule_insn		PROTO((rtx, rtx *, int, int));
static int schedule_select		PROTO((rtx *, int, int, FILE *));
static void create_reg_dead_note	PROTO((rtx, rtx));
static void attach_deaths		PROTO((rtx, rtx, int));
static void attach_deaths_insn		PROTO((rtx));
static rtx unlink_notes			PROTO((rtx, rtx));
static int new_sometimes_live		PROTO((struct sometimes *, int, int,
					       int));
static void finish_sometimes_live	PROTO((struct sometimes *, int));
static rtx reemit_notes			PROTO((rtx, rtx));
static void schedule_block		PROTO((int, FILE *));
static rtx regno_use_in			PROTO((int, rtx));
static void split_hard_reg_notes	PROTO((rtx, rtx, rtx, rtx));
static void new_insn_dead_notes		PROTO((rtx, rtx, rtx, rtx));
static void update_n_sets		PROTO((rtx, int));
static void update_flow_info		PROTO((rtx, rtx, rtx, rtx));

/* Main entry point of this file.  */
void schedule_insns	PROTO((FILE *));

#endif /* INSN_SCHEDULING */

/* Helper functions for instruction scheduling.  */

/* Add ELEM wrapped in an INSN_LIST with reg note kind DEP_TYPE to the
   LOG_LINKS of INSN, if not already there.  DEP_TYPE indicates the type
   of dependence that this link represents.  */

static void
add_dependence (insn, elem, dep_type)
     rtx insn;
     rtx elem;
     enum reg_note dep_type;
{
  rtx link, next;

  /* Don't depend an insn on itself.  */
  if (insn == elem)
    return;

  /* If elem is part of a sequence that must be scheduled together, then
     make the dependence point to the last insn of the sequence.
     When HAVE_cc0, it is possible for NOTEs to exist between users and
     setters of the condition codes, so we must skip past notes here.
     Otherwise, NOTEs are impossible here.  */

  next = NEXT_INSN (elem);

#ifdef HAVE_cc0
  while (next && GET_CODE (next) == NOTE)
    next = NEXT_INSN (next);
#endif

  if (next && SCHED_GROUP_P (next)
      && GET_CODE (next) != CODE_LABEL)
    {
      /* Notes will never intervene here though, so don't bother checking
	 for them.  */
      /* We must reject CODE_LABELs, so that we don't get confused by one
	 that has LABEL_PRESERVE_P set, which is represented by the same
	 bit in the rtl as SCHED_GROUP_P.  A CODE_LABEL can never be
	 SCHED_GROUP_P.  */
      while (NEXT_INSN (next) && SCHED_GROUP_P (NEXT_INSN (next))
	     && GET_CODE (NEXT_INSN (next)) != CODE_LABEL)
	next = NEXT_INSN (next);

      /* Again, don't depend an insn on itself.  */
      if (insn == next)
	return;

      /* Make the dependence to NEXT, the last insn of the group, instead
	 of the original ELEM.  */
      elem = next;
    }

  /* Check that we don't already have this dependence.  */
  for (link = LOG_LINKS (insn); link; link = XEXP (link, 1))
    if (XEXP (link, 0) == elem)
      {
	/* If this is a more restrictive type of dependence than the existing
	   one, then change the existing dependence to this type.  */
	if ((int) dep_type < (int) REG_NOTE_KIND (link))
	  PUT_REG_NOTE_KIND (link, dep_type);
	return;
      }
  /* Might want to check one level of transitivity to save conses.  */

  link = rtx_alloc (INSN_LIST);
  /* Insn dependency, not data dependency.  */
  PUT_REG_NOTE_KIND (link, dep_type);
  XEXP (link, 0) = elem;
  XEXP (link, 1) = LOG_LINKS (insn);
  LOG_LINKS (insn) = link;
}

/* Remove ELEM wrapped in an INSN_LIST from the LOG_LINKS
   of INSN.  Abort if not found.  */

static void
remove_dependence (insn, elem)
     rtx insn;
     rtx elem;
{
  rtx prev, link;
  int found = 0;

  for (prev = 0, link = LOG_LINKS (insn); link;
       prev = link, link = XEXP (link, 1))
    {
      if (XEXP (link, 0) == elem)
	{
	  if (prev)
	    XEXP (prev, 1) = XEXP (link, 1);
	  else
	    LOG_LINKS (insn) = XEXP (link, 1);
	  found = 1;
	}
    }

  if (! found)
    abort ();
  return;
}

#ifndef INSN_SCHEDULING
void
schedule_insns (dump_file)
     FILE *dump_file;
{
}
#else
#ifndef __GNUC__
#define __inline
#endif

/* Computation of memory dependencies.  */

/* The *_insns and *_mems are paired lists.  Each pending memory operation
   will have a pointer to the MEM rtx on one list and a pointer to the
   containing insn on the other list in the same place in the list.  */

/* We can't use add_dependence like the old code did, because a single insn
   may have multiple memory accesses, and hence needs to be on the list
   once for each memory access.  Add_dependence won't let you add an insn
   to a list more than once.  */

/* An INSN_LIST containing all insns with pending read operations.  */
static rtx pending_read_insns;

/* An EXPR_LIST containing all MEM rtx's which are pending reads.  */
static rtx pending_read_mems;

/* An INSN_LIST containing all insns with pending write operations.  */
static rtx pending_write_insns;

/* An EXPR_LIST containing all MEM rtx's which are pending writes.  */
static rtx pending_write_mems;

/* Indicates the combined length of the two pending lists.  We must prevent
   these lists from ever growing too large since the number of dependencies
   produced is at least O(N*N), and execution time is at least O(4*N*N), as
   a function of the length of these pending lists.  */

static int pending_lists_length;

/* An INSN_LIST containing all INSN_LISTs allocated but currently unused.  */

static rtx unused_insn_list;

/* An EXPR_LIST containing all EXPR_LISTs allocated but currently unused.  */

static rtx unused_expr_list;

/* The last insn upon which all memory references must depend.
   This is an insn which flushed the pending lists, creating a dependency
   between it and all previously pending memory references.  This creates
   a barrier (or a checkpoint) which no memory reference is allowed to cross.

   This includes all non constant CALL_INSNs.  When we do interprocedural
   alias analysis, this restriction can be relaxed.
   This may also be an INSN that writes memory if the pending lists grow
   too large.  */

static rtx last_pending_memory_flush;

/* The last function call we have seen.  All hard regs, and, of course,
   the last function call, must depend on this.  */

static rtx last_function_call;

/* The LOG_LINKS field of this is a list of insns which use a pseudo register
   that does not already cross a call.  We create dependencies between each
   of those insn and the next call insn, to ensure that they won't cross a call
   after scheduling is done.  */

static rtx sched_before_next_call;

/* Pointer to the last instruction scheduled.  Used by rank_for_schedule,
   so that insns independent of the last scheduled insn will be preferred
   over dependent instructions.  */

static rtx last_scheduled_insn;

/* Process an insn's memory dependencies.  There are four kinds of
   dependencies:

   (0) read dependence: read follows read
   (1) true dependence: read follows write
   (2) anti dependence: write follows read
   (3) output dependence: write follows write

   We are careful to build only dependencies which actually exist, and
   use transitivity to avoid building too many links.  */

/* Return the INSN_LIST containing INSN in LIST, or NULL
   if LIST does not contain INSN.  */

__inline static rtx
find_insn_list (insn, list)
     rtx insn;
     rtx list;
{
  while (list)
    {
      if (XEXP (list, 0) == insn)
	return list;
      list = XEXP (list, 1);
    }
  return 0;
}

/* Compute the function units used by INSN.  This caches the value
   returned by function_units_used.  A function unit is encoded as the
   unit number if the value is non-negative and the compliment of a
   mask if the value is negative.  A function unit index is the
   non-negative encoding.  */

__inline static int
insn_unit (insn)
     rtx insn;
{
  register int unit = INSN_UNIT (insn);

  if (unit == 0)
    {
      recog_memoized (insn);

      /* A USE insn, or something else we don't need to understand.
	 We can't pass these directly to function_units_used because it will
	 trigger a fatal error for unrecognizable insns.  */
      if (INSN_CODE (insn) < 0)
	unit = -1;
      else
	{
	  unit = function_units_used (insn);
	  /* Increment non-negative values so we can cache zero.  */
	  if (unit >= 0) unit++;
	}
      /* We only cache 16 bits of the result, so if the value is out of
	 range, don't cache it.  */
      if (FUNCTION_UNITS_SIZE < HOST_BITS_PER_SHORT
	  || unit >= 0
	  || (~unit & ((1 << (HOST_BITS_PER_SHORT - 1)) - 1)) == 0)
      INSN_UNIT (insn) = unit;
    }
  return (unit > 0 ? unit - 1 : unit);
}

/* Compute the blockage range for executing INSN on UNIT.  This caches
   the value returned by the blockage_range_function for the unit.
   These values are encoded in an int where the upper half gives the
   minimum value and the lower half gives the maximum value.  */

__inline static unsigned int
blockage_range (unit, insn)
     int unit;
     rtx insn;
{
  unsigned int blockage = INSN_BLOCKAGE (insn);
  unsigned int range;

  if (UNIT_BLOCKED (blockage) != unit + 1)
    {
      range = function_units[unit].blockage_range_function (insn);
      /* We only cache the blockage range for one unit and then only if
	 the values fit.  */
      if (HOST_BITS_PER_INT >= UNIT_BITS + 2 * BLOCKAGE_BITS)
	INSN_BLOCKAGE (insn) = ENCODE_BLOCKAGE (unit + 1, range);
    }
  else
    range = BLOCKAGE_RANGE (blockage);

  return range;
}

/* A vector indexed by function unit instance giving the last insn to use
   the unit.  The value of the function unit instance index for unit U
   instance I is (U + I * FUNCTION_UNITS_SIZE).  */
static rtx unit_last_insn[FUNCTION_UNITS_SIZE * MAX_MULTIPLICITY];

/* A vector indexed by function unit instance giving the minimum time when
   the unit will unblock based on the maximum blockage cost.  */
static int unit_tick[FUNCTION_UNITS_SIZE * MAX_MULTIPLICITY];

/* A vector indexed by function unit number giving the number of insns
   that remain to use the unit.  */
static int unit_n_insns[FUNCTION_UNITS_SIZE];

/* Reset the function unit state to the null state.  */

static void
clear_units ()
{
  bzero ((char *) unit_last_insn, sizeof (unit_last_insn));
  bzero ((char *) unit_tick, sizeof (unit_tick));
  bzero ((char *) unit_n_insns, sizeof (unit_n_insns));
}

/* Record an insn as one that will use the units encoded by UNIT.  */

__inline static void
prepare_unit (unit)
     int unit;
{
  int i;

  if (unit >= 0)
    unit_n_insns[unit]++;
  else
    for (i = 0, unit = ~unit; unit; i++, unit >>= 1)
      if ((unit & 1) != 0)
	prepare_unit (i);
}

/* Return the actual hazard cost of executing INSN on the unit UNIT,
   instance INSTANCE at time CLOCK if the previous actual hazard cost
   was COST.  */

__inline static int
actual_hazard_this_instance (unit, instance, insn, clock, cost)
     int unit, instance, clock, cost;
     rtx insn;
{
  int tick = unit_tick[instance];

  if (tick - clock > cost)
    {
      /* The scheduler is operating in reverse, so INSN is the executing
	 insn and the unit's last insn is the candidate insn.  We want a
	 more exact measure of the blockage if we execute INSN at CLOCK
	 given when we committed the execution of the unit's last insn.

	 The blockage value is given by either the unit's max blockage
	 constant, blockage range function, or blockage function.  Use
	 the most exact form for the given unit.  */

      if (function_units[unit].blockage_range_function)
	{
	  if (function_units[unit].blockage_function)
	    tick += (function_units[unit].blockage_function
		     (insn, unit_last_insn[instance])
		     - function_units[unit].max_blockage);
	  else
	    tick += ((int) MAX_BLOCKAGE_COST (blockage_range (unit, insn))
		     - function_units[unit].max_blockage);
	}
      if (tick - clock > cost)
	cost = tick - clock;
    }
  return cost;
}

/* Record INSN as having begun execution on the units encoded by UNIT at
   time CLOCK.  */

__inline static void
schedule_unit (unit, insn, clock)
     int unit, clock;
     rtx insn;
{
  int i;

  if (unit >= 0)
    {
      int instance = unit;
#if MAX_MULTIPLICITY > 1
      /* Find the first free instance of the function unit and use that
	 one.  We assume that one is free.  */
      for (i = function_units[unit].multiplicity - 1; i > 0; i--)
	{
	  if (! actual_hazard_this_instance (unit, instance, insn, clock, 0))
	    break;
	  instance += FUNCTION_UNITS_SIZE;
	}
#endif
      unit_last_insn[instance] = insn;
      unit_tick[instance] = (clock + function_units[unit].max_blockage);
    }
  else
    for (i = 0, unit = ~unit; unit; i++, unit >>= 1)
      if ((unit & 1) != 0)
	schedule_unit (i, insn, clock);
}

/* Return the actual hazard cost of executing INSN on the units encoded by
   UNIT at time CLOCK if the previous actual hazard cost was COST.  */

__inline static int
actual_hazard (unit, insn, clock, cost)
     int unit, clock, cost;
     rtx insn;
{
  int i;

  if (unit >= 0)
    {
      /* Find the instance of the function unit with the minimum hazard.  */
      int instance = unit;
      int best_cost = actual_hazard_this_instance (unit, instance, insn,
						   clock, cost);
      int this_cost;

#if MAX_MULTIPLICITY > 1
      if (best_cost > cost)
	{
	  for (i = function_units[unit].multiplicity - 1; i > 0; i--)
	    {
	      instance += FUNCTION_UNITS_SIZE;
	      this_cost = actual_hazard_this_instance (unit, instance, insn,
						       clock, cost);
	      if (this_cost < best_cost)
		{
		  best_cost = this_cost;
		  if (this_cost <= cost)
		    break;
		}
	    }
	}
#endif
      cost = MAX (cost, best_cost);
    }
  else
    for (i = 0, unit = ~unit; unit; i++, unit >>= 1)
      if ((unit & 1) != 0)
	cost = actual_hazard (i, insn, clock, cost);

  return cost;
}

/* Return the potential hazard cost of executing an instruction on the
   units encoded by UNIT if the previous potential hazard cost was COST.
   An insn with a large blockage time is chosen in preference to one
   with a smaller time; an insn that uses a unit that is more likely
   to be used is chosen in preference to one with a unit that is less
   used.  We are trying to minimize a subsequent actual hazard.  */

__inline static int
potential_hazard (unit, insn, cost)
     int unit, cost;
     rtx insn;
{
  int i, ncost;
  unsigned int minb, maxb;

  if (unit >= 0)
    {
      minb = maxb = function_units[unit].max_blockage;
      if (maxb > 1)
	{
	  if (function_units[unit].blockage_range_function)
	    {
	      maxb = minb = blockage_range (unit, insn);
	      maxb = MAX_BLOCKAGE_COST (maxb);
	      minb = MIN_BLOCKAGE_COST (minb);
	    }

	  if (maxb > 1)
	    {
	      /* Make the number of instructions left dominate.  Make the
		 minimum delay dominate the maximum delay.  If all these
		 are the same, use the unit number to add an arbitrary
		 ordering.  Other terms can be added.  */
	      ncost = minb * 0x40 + maxb;
	      ncost *= (unit_n_insns[unit] - 1) * 0x1000 + unit;
	      if (ncost > cost)
		cost = ncost;
	    }
	}
    }
  else
    for (i = 0, unit = ~unit; unit; i++, unit >>= 1)
      if ((unit & 1) != 0)
	cost = potential_hazard (i, insn, cost);

  return cost;
}

/* Compute cost of executing INSN given the dependence LINK on the insn USED.
   This is the number of virtual cycles taken between instruction issue and
   instruction results.  */

__inline static int
insn_cost (insn, link, used)
     rtx insn, link, used;
{
  register int cost = INSN_COST (insn);

  if (cost == 0)
    {
      recog_memoized (insn);

      /* A USE insn, or something else we don't need to understand.
	 We can't pass these directly to result_ready_cost because it will
	 trigger a fatal error for unrecognizable insns.  */
      if (INSN_CODE (insn) < 0)
	{
	  INSN_COST (insn) = 1;
	  return 1;
	}
      else
	{
	  cost = result_ready_cost (insn);

	  if (cost < 1)
	    cost = 1;

	  INSN_COST (insn) = cost;
	}
    }

  /* A USE insn should never require the value used to be computed.  This
     allows the computation of a function's result and parameter values to
     overlap the return and call.  */
  recog_memoized (used);
  if (INSN_CODE (used) < 0)
    LINK_COST_FREE (link) = 1;

  /* If some dependencies vary the cost, compute the adjustment.  Most
     commonly, the adjustment is complete: either the cost is ignored
     (in the case of an output- or anti-dependence), or the cost is
     unchanged.  These values are cached in the link as LINK_COST_FREE
     and LINK_COST_ZERO.  */

  if (LINK_COST_FREE (link))
    cost = 1;
#ifdef ADJUST_COST
  else if (! LINK_COST_ZERO (link))
    {
      int ncost = cost;

      ADJUST_COST (used, link, insn, ncost);
      if (ncost <= 1)
	LINK_COST_FREE (link) = ncost = 1;
      if (cost == ncost)
	LINK_COST_ZERO (link) = 1;
      cost = ncost;
    }
#endif
  return cost;
}

/* Compute the priority number for INSN.  */

static int
priority (insn)
     rtx insn;
{
  if (insn && GET_RTX_CLASS (GET_CODE (insn)) == 'i')
    {
      int prev_priority;
      int max_priority;
      int this_priority = INSN_PRIORITY (insn);
      rtx prev;

      if (this_priority > 0)
	return this_priority;

      max_priority = 1;

      /* Nonzero if these insns must be scheduled together.  */
      if (SCHED_GROUP_P (insn))
	{
	  prev = insn;
	  while (SCHED_GROUP_P (prev))
	    {
	      prev = PREV_INSN (prev);
	      INSN_REF_COUNT (prev) += 1;
	    }
	}

      for (prev = LOG_LINKS (insn); prev; prev = XEXP (prev, 1))
	{
	  rtx x = XEXP (prev, 0);

	  /* A dependence pointing to a note or deleted insn is always
	     obsolete, because sched_analyze_insn will have created any
	     necessary new dependences which replace it.  Notes and deleted
	     insns can be created when instructions are deleted by insn
	     splitting, or by register allocation.  */
	  if (GET_CODE (x) == NOTE || INSN_DELETED_P (x))
	    {
	      remove_dependence (insn, x);
	      continue;
	    }

	  /* Clear the link cost adjustment bits.  */
	  LINK_COST_FREE (prev) = 0;
#ifdef ADJUST_COST
	  LINK_COST_ZERO (prev) = 0;
#endif

	  /* This priority calculation was chosen because it results in the
	     least instruction movement, and does not hurt the performance
	     of the resulting code compared to the old algorithm.
	     This makes the sched algorithm more stable, which results
	     in better code, because there is less register pressure,
	     cross jumping is more likely to work, and debugging is easier.

	     When all instructions have a latency of 1, there is no need to
	     move any instructions.  Subtracting one here ensures that in such
	     cases all instructions will end up with a priority of one, and
	     hence no scheduling will be done.

	     The original code did not subtract the one, and added the
	     insn_cost of the current instruction to its priority (e.g.
	     move the insn_cost call down to the end).  */

	  prev_priority = priority (x) + insn_cost (x, prev, insn) - 1;

	  if (prev_priority > max_priority)
	    max_priority = prev_priority;
	  INSN_REF_COUNT (x) += 1;
	}

      prepare_unit (insn_unit (insn));
      INSN_PRIORITY (insn) = max_priority;
      return INSN_PRIORITY (insn);
    }
  return 0;
}

/* Remove all INSN_LISTs and EXPR_LISTs from the pending lists and add
   them to the unused_*_list variables, so that they can be reused.  */

static void
free_pending_lists ()
{
  register rtx link, prev_link;

  if (pending_read_insns)
    {
      prev_link = pending_read_insns;
      link = XEXP (prev_link, 1);

      while (link)
	{
	  prev_link = link;
	  link = XEXP (link, 1);
	}

      XEXP (prev_link, 1) = unused_insn_list;
      unused_insn_list = pending_read_insns;
      pending_read_insns = 0;
    }

  if (pending_write_insns)
    {
      prev_link = pending_write_insns;
      link = XEXP (prev_link, 1);

      while (link)
	{
	  prev_link = link;
	  link = XEXP (link, 1);
	}

      XEXP (prev_link, 1) = unused_insn_list;
      unused_insn_list = pending_write_insns;
      pending_write_insns = 0;
    }

  if (pending_read_mems)
    {
      prev_link = pending_read_mems;
      link = XEXP (prev_link, 1);

      while (link)
	{
	  prev_link = link;
	  link = XEXP (link, 1);
	}

      XEXP (prev_link, 1) = unused_expr_list;
      unused_expr_list = pending_read_mems;
      pending_read_mems = 0;
    }

  if (pending_write_mems)
    {
      prev_link = pending_write_mems;
      link = XEXP (prev_link, 1);

      while (link)
	{
	  prev_link = link;
	  link = XEXP (link, 1);
	}

      XEXP (prev_link, 1) = unused_expr_list;
      unused_expr_list = pending_write_mems;
      pending_write_mems = 0;
    }
}

/* Add an INSN and MEM reference pair to a pending INSN_LIST and MEM_LIST.
   The MEM is a memory reference contained within INSN, which we are saving
   so that we can do memory aliasing on it.  */

static void
add_insn_mem_dependence (insn_list, mem_list, insn, mem)
     rtx *insn_list, *mem_list, insn, mem;
{
  register rtx link;

  if (unused_insn_list)
    {
      link = unused_insn_list;
      unused_insn_list = XEXP (link, 1);
    }
  else
    link = rtx_alloc (INSN_LIST);
  XEXP (link, 0) = insn;
  XEXP (link, 1) = *insn_list;
  *insn_list = link;

  if (unused_expr_list)
    {
      link = unused_expr_list;
      unused_expr_list = XEXP (link, 1);
    }
  else
    link = rtx_alloc (EXPR_LIST);
  XEXP (link, 0) = mem;
  XEXP (link, 1) = *mem_list;
  *mem_list = link;

  pending_lists_length++;
}

/* Make a dependency between every memory reference on the pending lists
   and INSN, thus flushing the pending lists.  If ONLY_WRITE, don't flush
   the read list.  */

static void
flush_pending_lists (insn, only_write)
     rtx insn;
     int only_write;
{
  rtx link;

  while (pending_read_insns && ! only_write)
    {
      add_dependence (insn, XEXP (pending_read_insns, 0), REG_DEP_ANTI);

      link = pending_read_insns;
      pending_read_insns = XEXP (pending_read_insns, 1);
      XEXP (link, 1) = unused_insn_list;
      unused_insn_list = link;

      link = pending_read_mems;
      pending_read_mems = XEXP (pending_read_mems, 1);
      XEXP (link, 1) = unused_expr_list;
      unused_expr_list = link;
    }
  while (pending_write_insns)
    {
      add_dependence (insn, XEXP (pending_write_insns, 0), REG_DEP_ANTI);

      link = pending_write_insns;
      pending_write_insns = XEXP (pending_write_insns, 1);
      XEXP (link, 1) = unused_insn_list;
      unused_insn_list = link;

      link = pending_write_mems;
      pending_write_mems = XEXP (pending_write_mems, 1);
      XEXP (link, 1) = unused_expr_list;
      unused_expr_list = link;
    }
  pending_lists_length = 0;

  if (last_pending_memory_flush)
    add_dependence (insn, last_pending_memory_flush, REG_DEP_ANTI);

  last_pending_memory_flush = insn;
}

/* Analyze a single SET or CLOBBER rtx, X, creating all dependencies generated
   by the write to the destination of X, and reads of everything mentioned.  */

static void
sched_analyze_1 (x, insn)
     rtx x;
     rtx insn;
{
  register int regno;
  register rtx dest = SET_DEST (x);

  if (dest == 0)
    return;

  while (GET_CODE (dest) == STRICT_LOW_PART || GET_CODE (dest) == SUBREG
	 || GET_CODE (dest) == ZERO_EXTRACT || GET_CODE (dest) == SIGN_EXTRACT)
    {
      if (GET_CODE (dest) == ZERO_EXTRACT || GET_CODE (dest) == SIGN_EXTRACT)
	{
	  /* The second and third arguments are values read by this insn.  */
	  sched_analyze_2 (XEXP (dest, 1), insn);
	  sched_analyze_2 (XEXP (dest, 2), insn);
	}
      dest = SUBREG_REG (dest);
    }

  if (GET_CODE (dest) == REG)
    {
      register int i;

      regno = REGNO (dest);

      /* A hard reg in a wide mode may really be multiple registers.
	 If so, mark all of them just like the first.  */
      if (regno < FIRST_PSEUDO_REGISTER)
	{
	  i = HARD_REGNO_NREGS (regno, GET_MODE (dest));
	  while (--i >= 0)
	    {
	      rtx u;

	      for (u = reg_last_uses[regno+i]; u; u = XEXP (u, 1))
		add_dependence (insn, XEXP (u, 0), REG_DEP_ANTI);
	      reg_last_uses[regno + i] = 0;
	      if (reg_last_sets[regno + i])
		add_dependence (insn, reg_last_sets[regno + i],
				REG_DEP_OUTPUT);
	      reg_pending_sets[(regno + i) / REGSET_ELT_BITS]
		|= (REGSET_ELT_TYPE) 1 << ((regno + i) % REGSET_ELT_BITS);
	      if ((call_used_regs[i] || global_regs[i])
		  && last_function_call)
		/* Function calls clobber all call_used regs.  */
		add_dependence (insn, last_function_call, REG_DEP_ANTI);
	    }
	}
      else
	{
	  rtx u;

	  for (u = reg_last_uses[regno]; u; u = XEXP (u, 1))
	    add_dependence (insn, XEXP (u, 0), REG_DEP_ANTI);
	  reg_last_uses[regno] = 0;
	  if (reg_last_sets[regno])
	    add_dependence (insn, reg_last_sets[regno], REG_DEP_OUTPUT);
	  reg_pending_sets[regno / REGSET_ELT_BITS]
	    |= (REGSET_ELT_TYPE) 1 << (regno % REGSET_ELT_BITS);

	  /* Pseudos that are REG_EQUIV to something may be replaced
	     by that during reloading.  We need only add dependencies for
	     the address in the REG_EQUIV note.  */
	  if (! reload_completed
	      && reg_known_equiv_p[regno]
	      && GET_CODE (reg_known_value[regno]) == MEM)
	    sched_analyze_2 (XEXP (reg_known_value[regno], 0), insn);

	  /* Don't let it cross a call after scheduling if it doesn't
	     already cross one.  */
	  if (reg_n_calls_crossed[regno] == 0 && last_function_call)
	    add_dependence (insn, last_function_call, REG_DEP_ANTI);
	}
    }
  else if (GET_CODE (dest) == MEM)
    {
      /* Writing memory.  */

      if (pending_lists_length > 32)
	{
	  /* Flush all pending reads and writes to prevent the pending lists
	     from getting any larger.  Insn scheduling runs too slowly when
	     these lists get long.  The number 32 was chosen because it
	     seems like a reasonable number.  When compiling GCC with itself,
	     this flush occurs 8 times for sparc, and 10 times for m88k using
	     the number 32.  */
	  flush_pending_lists (insn, 0);
	}
      else
	{
	  rtx pending, pending_mem;

	  pending = pending_read_insns;
	  pending_mem = pending_read_mems;
	  while (pending)
	    {
	      /* If a dependency already exists, don't create a new one.  */
	      if (! find_insn_list (XEXP (pending, 0), LOG_LINKS (insn)))
		if (anti_dependence (XEXP (pending_mem, 0), dest))
		  add_dependence (insn, XEXP (pending, 0), REG_DEP_ANTI);

	      pending = XEXP (pending, 1);
	      pending_mem = XEXP (pending_mem, 1);
	    }

	  pending = pending_write_insns;
	  pending_mem = pending_write_mems;
	  while (pending)
	    {
	      /* If a dependency already exists, don't create a new one.  */
	      if (! find_insn_list (XEXP (pending, 0), LOG_LINKS (insn)))
		if (output_dependence (XEXP (pending_mem, 0), dest))
		  add_dependence (insn, XEXP (pending, 0), REG_DEP_OUTPUT);

	      pending = XEXP (pending, 1);
	      pending_mem = XEXP (pending_mem, 1);
	    }

	  if (last_pending_memory_flush)
	    add_dependence (insn, last_pending_memory_flush, REG_DEP_ANTI);

	  add_insn_mem_dependence (&pending_write_insns, &pending_write_mems,
				   insn, dest);
	}
      sched_analyze_2 (XEXP (dest, 0), insn);
    }

  /* Analyze reads.  */
  if (GET_CODE (x) == SET)
    sched_analyze_2 (SET_SRC (x), insn);
}

/* Analyze the uses of memory and registers in rtx X in INSN.  */

static void
sched_analyze_2 (x, insn)
     rtx x;
     rtx insn;
{
  register int i;
  register int j;
  register enum rtx_code code;
  register char *fmt;

  if (x == 0)
    return;

  code = GET_CODE (x);

  switch (code)
    {
    case CONST_INT:
    case CONST_DOUBLE:
    case SYMBOL_REF:
    case CONST:
    case LABEL_REF:
      /* Ignore constants.  Note that we must handle CONST_DOUBLE here
	 because it may have a cc0_rtx in its CONST_DOUBLE_CHAIN field, but
	 this does not mean that this insn is using cc0.  */
      return;

#ifdef HAVE_cc0
    case CC0:
      {
	rtx link, prev;

	/* User of CC0 depends on immediately preceding insn.  */
	SCHED_GROUP_P (insn) = 1;

	/* There may be a note before this insn now, but all notes will
	   be removed before we actually try to schedule the insns, so
	   it won't cause a problem later.  We must avoid it here though.  */
	prev = prev_nonnote_insn (insn);

	/* Make a copy of all dependencies on the immediately previous insn,
	   and add to this insn.  This is so that all the dependencies will
	   apply to the group.  Remove an explicit dependence on this insn
	   as SCHED_GROUP_P now represents it.  */

	if (find_insn_list (prev, LOG_LINKS (insn)))
	  remove_dependence (insn, prev);

	for (link = LOG_LINKS (prev); link; link = XEXP (link, 1))
	  add_dependence (insn, XEXP (link, 0), REG_NOTE_KIND (link));

	return;
      }
#endif

    case REG:
      {
	int regno = REGNO (x);
	if (regno < FIRST_PSEUDO_REGISTER)
	  {
	    int i;

	    i = HARD_REGNO_NREGS (regno, GET_MODE (x));
	    while (--i >= 0)
	      {
		reg_last_uses[regno + i]
		  = gen_rtx (INSN_LIST, VOIDmode,
			     insn, reg_last_uses[regno + i]);
		if (reg_last_sets[regno + i])
		  add_dependence (insn, reg_last_sets[regno + i], 0);
		if ((call_used_regs[regno + i] || global_regs[regno + i])
		    && last_function_call)
		  /* Function calls clobber all call_used regs.  */
		  add_dependence (insn, last_function_call, REG_DEP_ANTI);
	      }
	  }
	else
	  {
	    reg_last_uses[regno]
	      = gen_rtx (INSN_LIST, VOIDmode, insn, reg_last_uses[regno]);
	    if (reg_last_sets[regno])
	      add_dependence (insn, reg_last_sets[regno], 0);

	    /* Pseudos that are REG_EQUIV to something may be replaced
	       by that during reloading.  We need only add dependencies for
	       the address in the REG_EQUIV note.  */
	    if (! reload_completed
		&& reg_known_equiv_p[regno]
		&& GET_CODE (reg_known_value[regno]) == MEM)
	      sched_analyze_2 (XEXP (reg_known_value[regno], 0), insn);

	    /* If the register does not already cross any calls, then add this
	       insn to the sched_before_next_call list so that it will still
	       not cross calls after scheduling.  */
	    if (reg_n_calls_crossed[regno] == 0)
	      add_dependence (sched_before_next_call, insn, REG_DEP_ANTI);
	  }
	return;
      }

    case MEM:
      {
	/* Reading memory.  */

	rtx pending, pending_mem;

	pending = pending_read_insns;
	pending_mem = pending_read_mems;
	while (pending)
	  {
	    /* If a dependency already exists, don't create a new one.  */
	    if (! find_insn_list (XEXP (pending, 0), LOG_LINKS (insn)))
	      if (read_dependence (XEXP (pending_mem, 0), x))
		add_dependence (insn, XEXP (pending, 0), REG_DEP_ANTI);

	    pending = XEXP (pending, 1);
	    pending_mem = XEXP (pending_mem, 1);
	  }

	pending = pending_write_insns;
	pending_mem = pending_write_mems;
	while (pending)
	  {
	    /* If a dependency already exists, don't create a new one.  */
	    if (! find_insn_list (XEXP (pending, 0), LOG_LINKS (insn)))
	      if (true_dependence (XEXP (pending_mem, 0), VOIDmode,
				   x, rtx_varies_p))
		add_dependence (insn, XEXP (pending, 0), 0);

	    pending = XEXP (pending, 1);
	    pending_mem = XEXP (pending_mem, 1);
	  }
	if (last_pending_memory_flush)
	  add_dependence (insn, last_pending_memory_flush, REG_DEP_ANTI);

	/* Always add these dependencies to pending_reads, since
	   this insn may be followed by a write.  */
	add_insn_mem_dependence (&pending_read_insns, &pending_read_mems,
				 insn, x);

	/* Take advantage of tail recursion here.  */
	sched_analyze_2 (XEXP (x, 0), insn);
	return;
      }

    case ASM_OPERANDS:
    case ASM_INPUT:
    case UNSPEC_VOLATILE:
    case TRAP_IF:
      {
	rtx u;

	/* Traditional and volatile asm instructions must be considered to use
	   and clobber all hard registers, all pseudo-registers and all of
	   memory.  So must TRAP_IF and UNSPEC_VOLATILE operations.

	   Consider for instance a volatile asm that changes the fpu rounding
	   mode.  An insn should not be moved across this even if it only uses
	   pseudo-regs because it might give an incorrectly rounded result.  */
	if (code != ASM_OPERANDS || MEM_VOLATILE_P (x))
	  {
	    int max_reg = max_reg_num ();
	    for (i = 0; i < max_reg; i++)
	      {
		for (u = reg_last_uses[i]; u; u = XEXP (u, 1))
		  add_dependence (insn, XEXP (u, 0), REG_DEP_ANTI);
		reg_last_uses[i] = 0;
		if (reg_last_sets[i])
		  add_dependence (insn, reg_last_sets[i], 0);
	      }
	    reg_pending_sets_all = 1;

	    flush_pending_lists (insn, 0);
	  }

	/* For all ASM_OPERANDS, we must traverse the vector of input operands.
	   We can not just fall through here since then we would be confused
	   by the ASM_INPUT rtx inside ASM_OPERANDS, which do not indicate
	   traditional asms unlike their normal usage.  */

	if (code == ASM_OPERANDS)
	  {
	    for (j = 0; j < ASM_OPERANDS_INPUT_LENGTH (x); j++)
	      sched_analyze_2 (ASM_OPERANDS_INPUT (x, j), insn);
	    return;
	  }
	break;
      }

    case PRE_DEC:
    case POST_DEC:
    case PRE_INC:
    case POST_INC:
      /* These both read and modify the result.  We must handle them as writes
	 to get proper dependencies for following instructions.  We must handle
	 them as reads to get proper dependencies from this to previous
	 instructions.  Thus we need to pass them to both sched_analyze_1
	 and sched_analyze_2.  We must call sched_analyze_2 first in order
	 to get the proper antecedent for the read.  */
      sched_analyze_2 (XEXP (x, 0), insn);
      sched_analyze_1 (x, insn);
      return;
    }

  /* Other cases: walk the insn.  */
  fmt = GET_RTX_FORMAT (code);
  for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	sched_analyze_2 (XEXP (x, i), insn);
      else if (fmt[i] == 'E')
	for (j = 0; j < XVECLEN (x, i); j++)
	  sched_analyze_2 (XVECEXP (x, i, j), insn);
    }
}

/* Analyze an INSN with pattern X to find all dependencies.  */

static void
sched_analyze_insn (x, insn, loop_notes)
     rtx x, insn;
     rtx loop_notes;
{
  register RTX_CODE code = GET_CODE (x);
  rtx link;
  int maxreg = reg_last_uses_size;
  int i;

  if (code == SET || code == CLOBBER)
    sched_analyze_1 (x, insn);
  else if (code == PARALLEL)
    {
      register int i;
      for (i = XVECLEN (x, 0) - 1; i >= 0; i--)
	{
	  code = GET_CODE (XVECEXP (x, 0, i));
	  if (code == SET || code == CLOBBER)
	    sched_analyze_1 (XVECEXP (x, 0, i), insn);
	  else
	    sched_analyze_2 (XVECEXP (x, 0, i), insn);
	}
    }
  else
    sched_analyze_2 (x, insn);

  /* Mark registers CLOBBERED or used by called function.  */
  if (GET_CODE (insn) == CALL_INSN)
    for (link = CALL_INSN_FUNCTION_USAGE (insn); link; link = XEXP (link, 1))
      {
	if (GET_CODE (XEXP (link, 0)) == CLOBBER)
	  sched_analyze_1 (XEXP (link, 0), insn);
	else
	  sched_analyze_2 (XEXP (link, 0), insn);
      }

  /* If there is a {LOOP,EHREGION}_{BEG,END} note in the middle of a basic block, then
     we must be sure that no instructions are scheduled across it.
     Otherwise, the reg_n_refs info (which depends on loop_depth) would
     become incorrect.  */

  if (loop_notes)
    {
      int max_reg = reg_last_uses_size;
      rtx link;

      for (i = 0; i < max_reg; i++)
	{
	  rtx u;
	  for (u = reg_last_uses[i]; u; u = XEXP (u, 1))
	    add_dependence (insn, XEXP (u, 0), REG_DEP_ANTI);
	  reg_last_uses[i] = 0;
	  if (reg_last_sets[i])
	    add_dependence (insn, reg_last_sets[i], 0);
	}
      reg_pending_sets_all = 1;

      flush_pending_lists (insn, 0);

      link = loop_notes;
      while (XEXP (link, 1))
	link = XEXP (link, 1);
      XEXP (link, 1) = REG_NOTES (insn);
      REG_NOTES (insn) = loop_notes;
    }

  /* After reload, it is possible for an instruction to have a REG_DEAD note
     for a register that actually dies a few instructions earlier.  For
     example, this can happen with SECONDARY_MEMORY_NEEDED reloads.
     In this case, we must consider the insn to use the register mentioned
     in the REG_DEAD note.  Otherwise, we may accidentally move this insn
     after another insn that sets the register, thus getting obviously invalid
     rtl.  This confuses reorg which believes that REG_DEAD notes are still
     meaningful.

     ??? We would get better code if we fixed reload to put the REG_DEAD
     notes in the right places, but that may not be worth the effort.  */

  if (reload_completed)
    {
      rtx note;

      for (note = REG_NOTES (insn); note; note = XEXP (note, 1))
	if (REG_NOTE_KIND (note) == REG_DEAD)
	  sched_analyze_2 (XEXP (note, 0), insn);
    }

  for (i = 0; i < regset_size; i++)
    {
      REGSET_ELT_TYPE sets = reg_pending_sets[i];
      if (sets)
	{
	  register int bit;
	  for (bit = 0; bit < REGSET_ELT_BITS; bit++)
	    if (sets & ((REGSET_ELT_TYPE) 1 << bit))
	      reg_last_sets[i * REGSET_ELT_BITS + bit] = insn;
	  reg_pending_sets[i] = 0;
	}
    }
  if (reg_pending_sets_all)
    {
      for (i = 0; i < maxreg; i++)
	reg_last_sets[i] = insn;
      reg_pending_sets_all = 0;
    }

  /* Handle function calls and function returns created by the epilogue
     threading code.  */
  if (GET_CODE (insn) == CALL_INSN || GET_CODE (insn) == JUMP_INSN)
    {
      rtx dep_insn;
      rtx prev_dep_insn;

      /* When scheduling instructions, we make sure calls don't lose their
	 accompanying USE insns by depending them one on another in order.

	 Also, we must do the same thing for returns created by the epilogue
	 threading code.  Note this code works only in this special case,
	 because other passes make no guarantee that they will never emit
	 an instruction between a USE and a RETURN.  There is such a guarantee
	 for USE instructions immediately before a call.  */

      prev_dep_insn = insn;
      dep_insn = PREV_INSN (insn);
      while (GET_CODE (dep_insn) == INSN
	     && GET_CODE (PATTERN (dep_insn)) == USE
	     && GET_CODE (XEXP (PATTERN (dep_insn), 0)) == REG)
	{
	  SCHED_GROUP_P (prev_dep_insn) = 1;

	  /* Make a copy of all dependencies on dep_insn, and add to insn.
	     This is so that all of the dependencies will apply to the
	     group.  */

	  for (link = LOG_LINKS (dep_insn); link; link = XEXP (link, 1))
	    add_dependence (insn, XEXP (link, 0), REG_NOTE_KIND (link));

	  prev_dep_insn = dep_insn;
	  dep_insn = PREV_INSN (dep_insn);
	}
    }
}

/* Analyze every insn between HEAD and TAIL inclusive, creating LOG_LINKS
   for every dependency.  */

static int
sched_analyze (head, tail)
     rtx head, tail;
{
  register rtx insn;
  register int n_insns = 0;
  register rtx u;
  register int luid = 0;
  rtx loop_notes = 0;

  for (insn = head; ; insn = NEXT_INSN (insn))
    {
      INSN_LUID (insn) = luid++;

      if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN)
	{
	  sched_analyze_insn (PATTERN (insn), insn, loop_notes);
	  loop_notes = 0;
	  n_insns += 1;
	}
      else if (GET_CODE (insn) == CALL_INSN)
	{
	  rtx x;
	  register int i;

	  /* Any instruction using a hard register which may get clobbered
	     by a call needs to be marked as dependent on this call.
	     This prevents a use of a hard return reg from being moved
	     past a void call (i.e. it does not explicitly set the hard
	     return reg).  */

	  /* If this call is followed by a NOTE_INSN_SETJMP, then assume that
	     all registers, not just hard registers, may be clobbered by this
	     call.  */

	  /* Insn, being a CALL_INSN, magically depends on
	     `last_function_call' already.  */

	  if (NEXT_INSN (insn) && GET_CODE (NEXT_INSN (insn)) == NOTE
	      && NOTE_LINE_NUMBER (NEXT_INSN (insn)) == NOTE_INSN_SETJMP)
	    {
	      for (i = 0; i < reg_last_uses_size; i++)
		{
		  for (u = reg_last_uses[i]; u; u = XEXP (u, 1))
		    add_dependence (insn, XEXP (u, 0), REG_DEP_ANTI);
		  reg_last_uses[i] = 0;
		  if (reg_last_sets[i])
		    add_dependence (insn, reg_last_sets[i], 0);
		}
	      reg_pending_sets_all = 1;

	      /* Add a pair of fake REG_NOTEs which we will later
		 convert back into a NOTE_INSN_SETJMP note.  See
		 reemit_notes for why we use a pair of of NOTEs.  */

	      REG_NOTES (insn) = gen_rtx (EXPR_LIST, REG_DEAD,
					  GEN_INT (0),
					  REG_NOTES (insn));
	      REG_NOTES (insn) = gen_rtx (EXPR_LIST, REG_DEAD,
					  GEN_INT (NOTE_INSN_SETJMP),
					  REG_NOTES (insn));
	    }
	  else
	    {
	      for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
		if (call_used_regs[i] || global_regs[i])
		  {
		    for (u = reg_last_uses[i]; u; u = XEXP (u, 1))
		      add_dependence (insn, XEXP (u, 0), REG_DEP_ANTI);
		    reg_last_uses[i] = 0;
		    if (reg_last_sets[i])
		      add_dependence (insn, reg_last_sets[i], REG_DEP_ANTI);
		    reg_pending_sets[i / REGSET_ELT_BITS]
		      |= (REGSET_ELT_TYPE) 1 << (i % REGSET_ELT_BITS);
		  }
	    }

	  /* For each insn which shouldn't cross a call, add a dependence
	     between that insn and this call insn.  */
	  x = LOG_LINKS (sched_before_next_call);
	  while (x)
	    {
	      add_dependence (insn, XEXP (x, 0), REG_DEP_ANTI);
	      x = XEXP (x, 1);
	    }
	  LOG_LINKS (sched_before_next_call) = 0;

	  sched_analyze_insn (PATTERN (insn), insn, loop_notes);
	  loop_notes = 0;

	  /* In the absence of interprocedural alias analysis, we must flush
	     all pending reads and writes, and start new dependencies starting
	     from here.  But only flush writes for constant calls (which may
	     be passed a pointer to something we haven't written yet).  */
	  flush_pending_lists (insn, CONST_CALL_P (insn));

	  /* Depend this function call (actually, the user of this
	     function call) on all hard register clobberage.  */
	  last_function_call = insn;
	  n_insns += 1;
	}

      /* See comments on reemit_notes as to why we do this.  */
      else if (GET_CODE (insn) == NOTE
	       && (NOTE_LINE_NUMBER (insn) == NOTE_INSN_LOOP_BEG
		   || NOTE_LINE_NUMBER (insn) == NOTE_INSN_LOOP_END
		   || NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_BEG
		   || NOTE_LINE_NUMBER (insn) == NOTE_INSN_EH_REGION_END
		   || (NOTE_LINE_NUMBER (insn) == NOTE_INSN_SETJMP
		       && GET_CODE (PREV_INSN (insn)) != CALL_INSN)))
	{
	  loop_notes = gen_rtx (EXPR_LIST, REG_DEAD,
				GEN_INT (NOTE_BLOCK_NUMBER (insn)), loop_notes);
	  loop_notes = gen_rtx (EXPR_LIST, REG_DEAD,
				GEN_INT (NOTE_LINE_NUMBER (insn)), loop_notes);
	  CONST_CALL_P (loop_notes) = CONST_CALL_P (insn);
	}

      if (insn == tail)
	return n_insns;
    }

  abort ();
}

/* Called when we see a set of a register.  If death is true, then we are
   scanning backwards.  Mark that register as unborn.  If nobody says
   otherwise, that is how things will remain.  If death is false, then we
   are scanning forwards.  Mark that register as being born.  */

static void
sched_note_set (b, x, death)
     int b;
     rtx x;
     int death;
{
  register int regno;
  register rtx reg = SET_DEST (x);
  int subreg_p = 0;

  if (reg == 0)
    return;

  while (GET_CODE (reg) == SUBREG || GET_CODE (reg) == STRICT_LOW_PART
	 || GET_CODE (reg) == SIGN_EXTRACT || GET_CODE (reg) == ZERO_EXTRACT)
    {
      /* Must treat modification of just one hardware register of a multi-reg
	 value or just a byte field of a register exactly the same way that
	 mark_set_1 in flow.c does, i.e. anything except a paradoxical subreg
	 does not kill the entire register.  */
      if (GET_CODE (reg) != SUBREG
	  || REG_SIZE (SUBREG_REG (reg)) > REG_SIZE (reg))
	subreg_p = 1;

      reg = SUBREG_REG (reg);
    }

  if (GET_CODE (reg) != REG)
    return;

  /* Global registers are always live, so the code below does not apply
     to them.  */

  regno = REGNO (reg);
  if (regno >= FIRST_PSEUDO_REGISTER || ! global_regs[regno])
    {
      register int offset = regno / REGSET_ELT_BITS;
      register REGSET_ELT_TYPE bit
	= (REGSET_ELT_TYPE) 1 << (regno % REGSET_ELT_BITS);

      if (death)
	{
	  /* If we only set part of the register, then this set does not
	     kill it.  */
	  if (subreg_p)
	    return;

	  /* Try killing this register.  */
	  if (regno < FIRST_PSEUDO_REGISTER)
	    {
	      int j = HARD_REGNO_NREGS (regno, GET_MODE (reg));
	      while (--j >= 0)
		{
		  offset = (regno + j) / REGSET_ELT_BITS;
		  bit = (REGSET_ELT_TYPE) 1 << ((regno + j) % REGSET_ELT_BITS);
		  
		  bb_live_regs[offset] &= ~bit;
		  bb_dead_regs[offset] |= bit;
		}
	    }
	  else
	    {
	      bb_live_regs[offset] &= ~bit;
	      bb_dead_regs[offset] |= bit;
	    }
	}
      else
	{
	  /* Make the register live again.  */
	  if (regno < FIRST_PSEUDO_REGISTER)
	    {
	      int j = HARD_REGNO_NREGS (regno, GET_MODE (reg));
	      while (--j >= 0)
		{
		  offset = (regno + j) / REGSET_ELT_BITS;
		  bit = (REGSET_ELT_TYPE) 1 << ((regno + j) % REGSET_ELT_BITS);
		  
		  bb_live_regs[offset] |= bit;
		  bb_dead_regs[offset] &= ~bit;
		}
	    }
	  else
	    {
	      bb_live_regs[offset] |= bit;
	      bb_dead_regs[offset] &= ~bit;
	    }
	}
    }
}

/* Macros and functions for keeping the priority queue sorted, and
   dealing with queueing and dequeueing of instructions.  */

#define SCHED_SORT(READY, NEW_READY, OLD_READY) \
  if ((NEW_READY) - (OLD_READY) == 1)					\
    swap_sort (READY, NEW_READY);					\
  else if ((NEW_READY) - (OLD_READY) > 1)				\
    qsort (READY, NEW_READY, sizeof (rtx), rank_for_schedule); else	\

/* Returns a positive value if y is preferred; returns a negative value if
   x is preferred.  Should never return 0, since that will make the sort
   unstable.  */

static int
rank_for_schedule (x, y)
     rtx *x, *y;
{
  rtx tmp = *y;
  rtx tmp2 = *x;
  rtx link;
  int tmp_class, tmp2_class;
  int value;

  /* Choose the instruction with the highest priority, if different.  */
  if (value = INSN_PRIORITY (tmp) - INSN_PRIORITY (tmp2))
    return value;

  if (last_scheduled_insn)
    {
      /* Classify the instructions into three classes:
	 1) Data dependent on last schedule insn.
	 2) Anti/Output dependent on last scheduled insn.
	 3) Independent of last scheduled insn, or has latency of one.
	 Choose the insn from the highest numbered class if different.  */
      link = find_insn_list (tmp, LOG_LINKS (last_scheduled_insn));
      if (link == 0 || insn_cost (tmp, link, last_scheduled_insn) == 1)
	tmp_class = 3;
      else if (REG_NOTE_KIND (link) == 0) /* Data dependence.  */
	tmp_class = 1;
      else
	tmp_class = 2;

      link = find_insn_list (tmp2, LOG_LINKS (last_scheduled_insn));
      if (link == 0 || insn_cost (tmp2, link, last_scheduled_insn) == 1)
	tmp2_class = 3;
      else if (REG_NOTE_KIND (link) == 0) /* Data dependence.  */
	tmp2_class = 1;
      else
	tmp2_class = 2;

      if (value = tmp_class - tmp2_class)
	return value;
    }

  /* If insns are equally good, sort by INSN_LUID (original insn order),
     so that we make the sort stable.  This minimizes instruction movement,
     thus minimizing sched's effect on debugging and cross-jumping.  */
  return INSN_LUID (tmp) - INSN_LUID (tmp2);
}

/* Resort the array A in which only element at index N may be out of order.  */

__inline static void
swap_sort (a, n)
     rtx *a;
     int n;
{
  rtx insn = a[n-1];
  int i = n-2;

  while (i >= 0 && rank_for_schedule (a+i, &insn) >= 0)
    {
      a[i+1] = a[i];
      i -= 1;
    }
  a[i+1] = insn;
}

static int max_priority;

/* Add INSN to the insn queue so that it fires at least N_CYCLES
   before the currently executing insn.  */

__inline static void
queue_insn (insn, n_cycles)
     rtx insn;
     int n_cycles;
{
  int next_q = NEXT_Q_AFTER (q_ptr, n_cycles);
  NEXT_INSN (insn) = insn_queue[next_q];
  insn_queue[next_q] = insn;
  q_size += 1;
}

/* Return nonzero if PAT is the pattern of an insn which makes a
   register live.  */

__inline static int
birthing_insn_p (pat)
     rtx pat;
{
  int j;

  if (reload_completed == 1)
    return 0;

  if (GET_CODE (pat) == SET
      && GET_CODE (SET_DEST (pat)) == REG)
    {
      rtx dest = SET_DEST (pat);
      int i = REGNO (dest);
      int offset = i / REGSET_ELT_BITS;
      REGSET_ELT_TYPE bit = (REGSET_ELT_TYPE) 1 << (i % REGSET_ELT_BITS);

      /* It would be more accurate to use refers_to_regno_p or
	 reg_mentioned_p to determine when the dest is not live before this
	 insn.  */

      if (bb_live_regs[offset] & bit)
	return (reg_n_sets[i] == 1);

      return 0;
    }
  if (GET_CODE (pat) == PARALLEL)
    {
      for (j = 0; j < XVECLEN (pat, 0); j++)
	if (birthing_insn_p (XVECEXP (pat, 0, j)))
	  return 1;
    }
  return 0;
}

/* PREV is an insn that is ready to execute.  Adjust its priority if that
   will help shorten register lifetimes.  */

__inline static void
adjust_priority (prev)
     rtx prev;
{
  /* Trying to shorten register lives after reload has completed
     is useless and wrong.  It gives inaccurate schedules.  */
  if (reload_completed == 0)
    {
      rtx note;
      int n_deaths = 0;

      /* ??? This code has no effect, because REG_DEAD notes are removed
	 before we ever get here.  */
      for (note = REG_NOTES (prev); note; note = XEXP (note, 1))
	if (REG_NOTE_KIND (note) == REG_DEAD)
	  n_deaths += 1;

      /* Defer scheduling insns which kill registers, since that
	 shortens register lives.  Prefer scheduling insns which
	 make registers live for the same reason.  */
      switch (n_deaths)
	{
	default:
	  INSN_PRIORITY (prev) >>= 3;
	  break;
	case 3:
	  INSN_PRIORITY (prev) >>= 2;
	  break;
	case 2:
	case 1:
	  INSN_PRIORITY (prev) >>= 1;
	  break;
	case 0:
	  if (birthing_insn_p (PATTERN (prev)))
	    {
	      int max = max_priority;

	      if (max > INSN_PRIORITY (prev))
		INSN_PRIORITY (prev) = max;
	    }
	  break;
	}
#ifdef ADJUST_PRIORITY
      ADJUST_PRIORITY (prev);
#endif
    }
}

/* INSN is the "currently executing insn".  Launch each insn which was
   waiting on INSN (in the backwards dataflow sense).  READY is a
   vector of insns which are ready to fire.  N_READY is the number of
   elements in READY.  CLOCK is the current virtual cycle.  */

static int
schedule_insn (insn, ready, n_ready, clock)
     rtx insn;
     rtx *ready;
     int n_ready;
     int clock;
{
  rtx link;
  int new_ready = n_ready;

  if (MAX_BLOCKAGE > 1)
    schedule_unit (insn_unit (insn), insn, clock);

  if (LOG_LINKS (insn) == 0)
    return n_ready;

  /* This is used by the function adjust_priority above.  */
  if (n_ready > 0)
    max_priority = MAX (INSN_PRIORITY (ready[0]), INSN_PRIORITY (insn));
  else
    max_priority = INSN_PRIORITY (insn);

  for (link = LOG_LINKS (insn); link != 0; link = XEXP (link, 1))
    {
      rtx prev = XEXP (link, 0);
      int cost = insn_cost (prev, link, insn);

      if ((INSN_REF_COUNT (prev) -= 1) != 0)
	{
	  /* We satisfied one requirement to fire PREV.  Record the earliest
	     time when PREV can fire.  No need to do this if the cost is 1,
	     because PREV can fire no sooner than the next cycle.  */
	  if (cost > 1)
	    INSN_TICK (prev) = MAX (INSN_TICK (prev), clock + cost);
	}
      else
	{
	  /* We satisfied the last requirement to fire PREV.  Ensure that all
	     timing requirements are satisfied.  */
	  if (INSN_TICK (prev) - clock > cost)
	    cost = INSN_TICK (prev) - clock;

	  /* Adjust the priority of PREV and either put it on the ready
	     list or queue it.  */
	  adjust_priority (prev);
	  if (cost <= 1)
	    ready[new_ready++] = prev;
	  else
	    queue_insn (prev, cost);
	}
    }

  return new_ready;
}

/* Given N_READY insns in the ready list READY at time CLOCK, queue
   those that are blocked due to function unit hazards and rearrange
   the remaining ones to minimize subsequent function unit hazards.  */

static int
schedule_select (ready, n_ready, clock, file)
     rtx *ready;
     int n_ready, clock;
     FILE *file;
{
  int pri = INSN_PRIORITY (ready[0]);
  int i, j, k, q, cost, best_cost, best_insn = 0, new_ready = n_ready;
  rtx insn;

  /* Work down the ready list in groups of instructions with the same
     priority value.  Queue insns in the group that are blocked and
     select among those that remain for the one with the largest
     potential hazard.  */
  for (i = 0; i < n_ready; i = j)
    {
      int opri = pri;
      for (j = i + 1; j < n_ready; j++)
	if ((pri = INSN_PRIORITY (ready[j])) != opri)
	  break;

      /* Queue insns in the group that are blocked.  */
      for (k = i, q = 0; k < j; k++)
	{
	  insn = ready[k];
	  if ((cost = actual_hazard (insn_unit (insn), insn, clock, 0)) != 0)
	    {
	      q++;
	      ready[k] = 0;
	      queue_insn (insn, cost);
	      if (file)
		fprintf (file, "\n;; blocking insn %d for %d cycles",
			 INSN_UID (insn), cost);
	    }
	}
      new_ready -= q;

      /* Check the next group if all insns were queued.  */
      if (j - i - q == 0)
	continue;

      /* If more than one remains, select the first one with the largest
	 potential hazard.  */
      else if (j - i - q > 1)
	{
	  best_cost = -1;
	  for (k = i; k < j; k++)
	    {
	      if ((insn = ready[k]) == 0)
		continue;
	      if ((cost = potential_hazard (insn_unit (insn), insn, 0))
		  > best_cost)
		{
		  best_cost = cost;
		  best_insn = k;
		}
	    }
	}
      /* We have found a suitable insn to schedule.  */
      break;
    }

  /* Move the best insn to be front of the ready list.  */
  if (best_insn != 0)
    {
      if (file)
	{
	  fprintf (file, ", now");
	  for (i = 0; i < n_ready; i++)
	    if (ready[i])
	      fprintf (file, " %d", INSN_UID (ready[i]));
	  fprintf (file, "\n;; insn %d has a greater potential hazard",
		   INSN_UID (ready[best_insn]));
	}
      for (i = best_insn; i > 0; i--)
	{
	  insn = ready[i-1];
	  ready[i-1] = ready[i];
	  ready[i] = insn;
	}
    }

  /* Compact the ready list.  */
  if (new_ready < n_ready)
    for (i = j = 0; i < n_ready; i++)
      if (ready[i])
	ready[j++] = ready[i];

  return new_ready;
}

/* Add a REG_DEAD note for REG to INSN, reusing a REG_DEAD note from the
   dead_notes list.  */

static void
create_reg_dead_note (reg, insn)
     rtx reg, insn;
{
  rtx link;
		
  /* The number of registers killed after scheduling must be the same as the
     number of registers killed before scheduling.  The number of REG_DEAD
     notes may not be conserved, i.e. two SImode hard register REG_DEAD notes
     might become one DImode hard register REG_DEAD note, but the number of
     registers killed will be conserved.
     
     We carefully remove REG_DEAD notes from the dead_notes list, so that
     there will be none left at the end.  If we run out early, then there
     is a bug somewhere in flow, combine and/or sched.  */

  if (dead_notes == 0)
    {
#if 1
      abort ();
#else
      link = rtx_alloc (EXPR_LIST);
      PUT_REG_NOTE_KIND (link, REG_DEAD);
#endif
    }
  else
    {
      /* Number of regs killed by REG.  */
      int regs_killed = (REGNO (reg) >= FIRST_PSEUDO_REGISTER ? 1
			 : HARD_REGNO_NREGS (REGNO (reg), GET_MODE (reg)));
      /* Number of regs killed by REG_DEAD notes taken off the list.  */
      int reg_note_regs;

      link = dead_notes;
      reg_note_regs = (REGNO (XEXP (link, 0)) >= FIRST_PSEUDO_REGISTER ? 1
		       : HARD_REGNO_NREGS (REGNO (XEXP (link, 0)),
					   GET_MODE (XEXP (link, 0))));
      while (reg_note_regs < regs_killed)
	{
	  link = XEXP (link, 1);
	  reg_note_regs += (REGNO (XEXP (link, 0)) >= FIRST_PSEUDO_REGISTER ? 1
			    : HARD_REGNO_NREGS (REGNO (XEXP (link, 0)),
						GET_MODE (XEXP (link, 0))));
	}
      dead_notes = XEXP (link, 1);

      /* If we took too many regs kills off, put the extra ones back.  */
      while (reg_note_regs > regs_killed)
	{
	  rtx temp_reg, temp_link;

	  temp_reg = gen_rtx (REG, word_mode, 0);
	  temp_link = rtx_alloc (EXPR_LIST);
	  PUT_REG_NOTE_KIND (temp_link, REG_DEAD);
	  XEXP (temp_link, 0) = temp_reg;
	  XEXP (temp_link, 1) = dead_notes;
	  dead_notes = temp_link;
	  reg_note_regs--;
	}
    }

  XEXP (link, 0) = reg;
  XEXP (link, 1) = REG_NOTES (insn);
  REG_NOTES (insn) = link;
}

/* Subroutine on attach_deaths_insn--handles the recursive search
   through INSN.  If SET_P is true, then x is being modified by the insn.  */

static void
attach_deaths (x, insn, set_p)
     rtx x;
     rtx insn;
     int set_p;
{
  register int i;
  register int j;
  register enum rtx_code code;
  register char *fmt;

  if (x == 0)
    return;

  code = GET_CODE (x);

  switch (code)
    {
    case CONST_INT:
    case CONST_DOUBLE:
    case LABEL_REF:
    case SYMBOL_REF:
    case CONST:
    case CODE_LABEL:
    case PC:
    case CC0:
      /* Get rid of the easy cases first.  */
      return;

    case REG:
      {
	/* If the register dies in this insn, queue that note, and mark
	   this register as needing to die.  */
	/* This code is very similar to mark_used_1 (if set_p is false)
	   and mark_set_1 (if set_p is true) in flow.c.  */

	register int regno = REGNO (x);
	register int offset = regno / REGSET_ELT_BITS;
	register REGSET_ELT_TYPE bit
	  = (REGSET_ELT_TYPE) 1 << (regno % REGSET_ELT_BITS);
	REGSET_ELT_TYPE all_needed = (old_live_regs[offset] & bit);
	REGSET_ELT_TYPE some_needed = (old_live_regs[offset] & bit);

	if (set_p)
	  return;

	if (regno < FIRST_PSEUDO_REGISTER)
	  {
	    int n;

	    n = HARD_REGNO_NREGS (regno, GET_MODE (x));
	    while (--n > 0)
	      {
		some_needed |= (old_live_regs[(regno + n) / REGSET_ELT_BITS]
				& ((REGSET_ELT_TYPE) 1
				   << ((regno + n) % REGSET_ELT_BITS)));
		all_needed &= (old_live_regs[(regno + n) / REGSET_ELT_BITS]
			       & ((REGSET_ELT_TYPE) 1
				  << ((regno + n) % REGSET_ELT_BITS)));
	      }
	  }

	/* If it wasn't live before we started, then add a REG_DEAD note.
	   We must check the previous lifetime info not the current info,
	   because we may have to execute this code several times, e.g.
	   once for a clobber (which doesn't add a note) and later
	   for a use (which does add a note).
	   
	   Always make the register live.  We must do this even if it was
	   live before, because this may be an insn which sets and uses
	   the same register, in which case the register has already been
	   killed, so we must make it live again.

	   Global registers are always live, and should never have a REG_DEAD
	   note added for them, so none of the code below applies to them.  */

	if (regno >= FIRST_PSEUDO_REGISTER || ! global_regs[regno])
	  {
	    /* Never add REG_DEAD notes for the FRAME_POINTER_REGNUM or the
	       STACK_POINTER_REGNUM, since these are always considered to be
	       live.  Similarly for ARG_POINTER_REGNUM if it is fixed.  */
	    if (regno != FRAME_POINTER_REGNUM
#if HARD_FRAME_POINTER_REGNUM != FRAME_POINTER_REGNUM
		&& ! (regno == HARD_FRAME_POINTER_REGNUM)
#endif
#if ARG_POINTER_REGNUM != FRAME_POINTER_REGNUM
		&& ! (regno == ARG_POINTER_REGNUM && fixed_regs[regno])
#endif
		&& regno != STACK_POINTER_REGNUM)
	      {
		/* ??? It is perhaps a dead_or_set_p bug that it does
		   not check for REG_UNUSED notes itself.  This is necessary
		   for the case where the SET_DEST is a subreg of regno, as
		   dead_or_set_p handles subregs specially.  */
		if (! all_needed && ! dead_or_set_p (insn, x)
		    && ! find_reg_note (insn, REG_UNUSED, x))
		  {
		    /* Check for the case where the register dying partially
		       overlaps the register set by this insn.  */
		    if (regno < FIRST_PSEUDO_REGISTER
			&& HARD_REGNO_NREGS (regno, GET_MODE (x)) > 1)
		      {
			int n = HARD_REGNO_NREGS (regno, GET_MODE (x));
			while (--n >= 0)
			  some_needed |= dead_or_set_regno_p (insn, regno + n);
		      }

		    /* If none of the words in X is needed, make a REG_DEAD
		       note.  Otherwise, we must make partial REG_DEAD
		       notes.  */
		    if (! some_needed)
		      create_reg_dead_note (x, insn);
		    else
		      {
			int i;

			/* Don't make a REG_DEAD note for a part of a
			   register that is set in the insn.  */
			for (i = HARD_REGNO_NREGS (regno, GET_MODE (x)) - 1;
			     i >= 0; i--)
			  if ((old_live_regs[(regno + i) / REGSET_ELT_BITS]
			       & ((REGSET_ELT_TYPE) 1
				  << ((regno +i) % REGSET_ELT_BITS))) == 0
			      && ! dead_or_set_regno_p (insn, regno + i))
			    create_reg_dead_note (gen_rtx (REG,
							   reg_raw_mode[regno + i],
							   regno + i),
						  insn);
		      }
		  }
	      }

	    if (regno < FIRST_PSEUDO_REGISTER)
	      {
		int j = HARD_REGNO_NREGS (regno, GET_MODE (x));
		while (--j >= 0)
		  {
		    offset = (regno + j) / REGSET_ELT_BITS;
		    bit
		      = (REGSET_ELT_TYPE) 1 << ((regno + j) % REGSET_ELT_BITS);

		    bb_dead_regs[offset] &= ~bit;
		    bb_live_regs[offset] |= bit;
		  }
	      }
	    else
	      {
		bb_dead_regs[offset] &= ~bit;
		bb_live_regs[offset] |= bit;
	      }
	  }
	return;
      }

    case MEM:
      /* Handle tail-recursive case.  */
      attach_deaths (XEXP (x, 0), insn, 0);
      return;

    case SUBREG:
    case STRICT_LOW_PART:
      /* These two cases preserve the value of SET_P, so handle them
	 separately.  */
      attach_deaths (XEXP (x, 0), insn, set_p);
      return;

    case ZERO_EXTRACT:
    case SIGN_EXTRACT:
      /* This case preserves the value of SET_P for the first operand, but
	 clears it for the other two.  */
      attach_deaths (XEXP (x, 0), insn, set_p);
      attach_deaths (XEXP (x, 1), insn, 0);
      attach_deaths (XEXP (x, 2), insn, 0);
      return;

    default:
      /* Other cases: walk the insn.  */
      fmt = GET_RTX_FORMAT (code);
      for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--)
	{
	  if (fmt[i] == 'e')
	    attach_deaths (XEXP (x, i), insn, 0);
	  else if (fmt[i] == 'E')
	    for (j = 0; j < XVECLEN (x, i); j++)
	      attach_deaths (XVECEXP (x, i, j), insn, 0);
	}
    }
}

/* After INSN has executed, add register death notes for each register
   that is dead after INSN.  */

static void
attach_deaths_insn (insn)
     rtx insn;
{
  rtx x = PATTERN (insn);
  register RTX_CODE code = GET_CODE (x);
  rtx link;

  if (code == SET)
    {
      attach_deaths (SET_SRC (x), insn, 0);

      /* A register might die here even if it is the destination, e.g.
	 it is the target of a volatile read and is otherwise unused.
	 Hence we must always call attach_deaths for the SET_DEST.  */
      attach_deaths (SET_DEST (x), insn, 1);
    }
  else if (code == PARALLEL)
    {
      register int i;
      for (i = XVECLEN (x, 0) - 1; i >= 0; i--)
	{
	  code = GET_CODE (XVECEXP (x, 0, i));
	  if (code == SET)
	    {
	      attach_deaths (SET_SRC (XVECEXP (x, 0, i)), insn, 0);

	      attach_deaths (SET_DEST (XVECEXP (x, 0, i)), insn, 1);
	    }
	  /* Flow does not add REG_DEAD notes to registers that die in
	     clobbers, so we can't either.  */
	  else if (code != CLOBBER)
	    attach_deaths (XVECEXP (x, 0, i), insn, 0);
	}
    }
  /* If this is a CLOBBER, only add REG_DEAD notes to registers inside a
     MEM being clobbered, just like flow.  */
  else if (code == CLOBBER && GET_CODE (XEXP (x, 0)) == MEM)
    attach_deaths (XEXP (XEXP (x, 0), 0), insn, 0);
  /* Otherwise don't add a death note to things being clobbered.  */
  else if (code != CLOBBER)
    attach_deaths (x, insn, 0);

  /* Make death notes for things used in the called function.  */
  if (GET_CODE (insn) == CALL_INSN)
    for (link = CALL_INSN_FUNCTION_USAGE (insn); link; link = XEXP (link, 1))
      attach_deaths (XEXP (XEXP (link, 0), 0), insn,
		     GET_CODE (XEXP (link, 0)) == CLOBBER);
}

/* Delete notes beginning with INSN and maybe put them in the chain
   of notes ended by NOTE_LIST.
   Returns the insn following the notes.  */

static rtx
unlink_notes (insn, tail)
     rtx insn, tail;
{
  rtx prev = PREV_INSN (insn);

  while (insn != tail && GET_CODE (insn) == NOTE)
    {
      rtx next = NEXT_INSN (insn);
      /* Delete the note from its current position.  */
      if (prev)
	NEXT_INSN (prev) = next;
      if (next)
	PREV_INSN (next) = prev;

      if (write_symbols != NO_DEBUG && NOTE_LINE_NUMBER (insn) > 0)
	/* Record line-number notes so they can be reused.  */
	LINE_NOTE (insn) = insn;

      /* Don't save away NOTE_INSN_SETJMPs, because they must remain
	 immediately after the call they follow.  We use a fake
	 (REG_DEAD (const_int -1)) note to remember them.
	 Likewise with NOTE_INSN_{LOOP,EHREGION}_{BEG, END}.  */
      else if (NOTE_LINE_NUMBER (insn) != NOTE_INSN_SETJMP
	       && NOTE_LINE_NUMBER (insn) != NOTE_INSN_LOOP_BEG
	       && NOTE_LINE_NUMBER (insn) != NOTE_INSN_LOOP_END
	       && NOTE_LINE_NUMBER (insn) != NOTE_INSN_EH_REGION_BEG
	       && NOTE_LINE_NUMBER (insn) != NOTE_INSN_EH_REGION_END)
	{
	  /* Insert the note at the end of the notes list.  */
	  PREV_INSN (insn) = note_list;
	  if (note_list)
	    NEXT_INSN (note_list) = insn;
	  note_list = insn;
	}

      insn = next;
    }
  return insn;
}

/* Constructor for `sometimes' data structure.  */

static int
new_sometimes_live (regs_sometimes_live, offset, bit, sometimes_max)
     struct sometimes *regs_sometimes_live;
     int offset, bit;
     int sometimes_max;
{
  register struct sometimes *p;
  register int regno = offset * REGSET_ELT_BITS + bit;

  /* There should never be a register greater than max_regno here.  If there
     is, it means that a define_split has created a new pseudo reg.  This
     is not allowed, since there will not be flow info available for any
     new register, so catch the error here.  */
  if (regno >= max_regno)
    abort ();

  p = &regs_sometimes_live[sometimes_max];
  p->offset = offset;
  p->bit = bit;
  p->live_length = 0;
  p->calls_crossed = 0;
  sometimes_max++;
  return sometimes_max;
}

/* Count lengths of all regs we are currently tracking,
   and find new registers no longer live.  */

static void
finish_sometimes_live (regs_sometimes_live, sometimes_max)
     struct sometimes *regs_sometimes_live;
     int sometimes_max;
{
  int i;

  for (i = 0; i < sometimes_max; i++)
    {
      register struct sometimes *p = &regs_sometimes_live[i];
      int regno;

      regno = p->offset * REGSET_ELT_BITS + p->bit;

      sched_reg_live_length[regno] += p->live_length;
      sched_reg_n_calls_crossed[regno] += p->calls_crossed;
    }
}

/* Search INSN for fake REG_DEAD note pairs for NOTE_INSN_SETJMP,
   NOTE_INSN_{LOOP,EHREGION}_{BEG,END}; and convert them back into
   NOTEs.  The REG_DEAD note following first one is contains the saved
   value for NOTE_BLOCK_NUMBER which is useful for
   NOTE_INSN_EH_REGION_{BEG,END} NOTEs.  LAST is the last instruction
   output by the instruction scheduler.  Return the new value of LAST.  */

static rtx
reemit_notes (insn, last)
     rtx insn;
     rtx last;
{
  rtx note;

  for (note = REG_NOTES (insn); note; note = XEXP (note, 1))
    {
      if (REG_NOTE_KIND (note) == REG_DEAD
	  && GET_CODE (XEXP (note, 0)) == CONST_INT)
	{
	  if (INTVAL (XEXP (note, 0)) == NOTE_INSN_SETJMP)
	    {
	      CONST_CALL_P (emit_note_after (INTVAL (XEXP (note, 0)), insn))
		= CONST_CALL_P (note);
	      remove_note (insn, note);
	      note = XEXP (note, 1);
	    }
	  else
	    {
	      last = emit_note_before (INTVAL (XEXP (note, 0)), last);
	      remove_note (insn, note);
	      note = XEXP (note, 1);
	      NOTE_BLOCK_NUMBER (last) = INTVAL (XEXP (note, 0));
	    }
	  remove_note (insn, note);
	}
    }
  return last;
}

/* Use modified list scheduling to rearrange insns in basic block
   B.  FILE, if nonzero, is where we dump interesting output about
   this pass.  */

static void
schedule_block (b, file)
     int b;
     FILE *file;
{
  rtx insn, last;
  rtx *ready, link;
  int i, j, n_ready = 0, new_ready, n_insns;
  int sched_n_insns = 0;
  int clock;
#define NEED_NOTHING	0
#define NEED_HEAD	1
#define NEED_TAIL	2
  int new_needs;

  /* HEAD and TAIL delimit the region being scheduled.  */
  rtx head = basic_block_head[b];
  rtx tail = basic_block_end[b];
  /* PREV_HEAD and NEXT_TAIL are the boundaries of the insns
     being scheduled.  When the insns have been ordered,
     these insns delimit where the new insns are to be
     spliced back into the insn chain.  */
  rtx next_tail;
  rtx prev_head;

  /* Keep life information accurate.  */
  register struct sometimes *regs_sometimes_live;
  int sometimes_max;

  if (file)
    fprintf (file, ";;\t -- basic block number %d from %d to %d --\n",
	     b, INSN_UID (basic_block_head[b]), INSN_UID (basic_block_end[b]));

  reg_last_uses_size = i = max_reg_num ();
  reg_last_uses = (rtx *) alloca (i * sizeof (rtx));
  bzero ((char *) reg_last_uses, i * sizeof (rtx));
  reg_last_sets = (rtx *) alloca (i * sizeof (rtx));
  bzero ((char *) reg_last_sets, i * sizeof (rtx));
  reg_pending_sets = (regset) alloca (regset_bytes);
  bzero ((char *) reg_pending_sets, regset_bytes);
  reg_pending_sets_all = 0;
  clear_units ();

  /* Remove certain insns at the beginning from scheduling,
     by advancing HEAD.  */

  /* At the start of a function, before reload has run, don't delay getting
     parameters from hard registers into pseudo registers.  */
  if (reload_completed == 0 && b == 0)
    {
      while (head != tail
	     && GET_CODE (head) == NOTE
	     && NOTE_LINE_NUMBER (head) != NOTE_INSN_FUNCTION_BEG)
	head = NEXT_INSN (head);
      while (head != tail
	     && GET_CODE (head) == INSN
	     && GET_CODE (PATTERN (head)) == SET)
	{
	  rtx src = SET_SRC (PATTERN (head));
	  while (GET_CODE (src) == SUBREG
		 || GET_CODE (src) == SIGN_EXTEND
		 || GET_CODE (src) == ZERO_EXTEND
		 || GET_CODE (src) == SIGN_EXTRACT
		 || GET_CODE (src) == ZERO_EXTRACT)
	    src = XEXP (src, 0);
	  if (GET_CODE (src) != REG
	      || REGNO (src) >= FIRST_PSEUDO_REGISTER)
	    break;
	  /* Keep this insn from ever being scheduled.  */
	  INSN_REF_COUNT (head) = 1;
	  head = NEXT_INSN (head);
	}
    }

  /* Don't include any notes or labels at the beginning of the
     basic block, or notes at the ends of basic blocks.  */
  while (head != tail)
    {
      if (GET_CODE (head) == NOTE)
	head = NEXT_INSN (head);
      else if (GET_CODE (tail) == NOTE)
	tail = PREV_INSN (tail);
      else if (GET_CODE (head) == CODE_LABEL)
	head = NEXT_INSN (head);
      else break;
    }
  /* If the only insn left is a NOTE or a CODE_LABEL, then there is no need
     to schedule this block.  */
  if (head == tail
      && (GET_CODE (head) == NOTE || GET_CODE (head) == CODE_LABEL))
    return;

#if 0
  /* This short-cut doesn't work.  It does not count call insns crossed by
     registers in reg_sometimes_live.  It does not mark these registers as
     dead if they die in this block.  It does not mark these registers live
     (or create new reg_sometimes_live entries if necessary) if they are born
     in this block.

     The easy solution is to just always schedule a block.  This block only
     has one insn, so this won't slow down this pass by much.  */

  if (head == tail)
    return;
#endif

  /* Now HEAD through TAIL are the insns actually to be rearranged;
     Let PREV_HEAD and NEXT_TAIL enclose them.  */
  prev_head = PREV_INSN (head);
  next_tail = NEXT_INSN (tail);

  /* Initialize basic block data structures.  */
  dead_notes = 0;
  pending_read_insns = 0;
  pending_read_mems = 0;
  pending_write_insns = 0;
  pending_write_mems = 0;
  pending_lists_length = 0;
  last_pending_memory_flush = 0;
  last_function_call = 0;
  last_scheduled_insn = 0;

  LOG_LINKS (sched_before_next_call) = 0;

  n_insns = sched_analyze (head, tail);
  if (n_insns == 0)
    {
      free_pending_lists ();
      return;
    }

  /* Allocate vector to hold insns to be rearranged (except those
     insns which are controlled by an insn with SCHED_GROUP_P set).
     All these insns are included between ORIG_HEAD and ORIG_TAIL,
     as those variables ultimately are set up.  */
  ready = (rtx *) alloca ((n_insns+1) * sizeof (rtx));

  /* TAIL is now the last of the insns to be rearranged.
     Put those insns into the READY vector.  */
  insn = tail;

  /* For all branches, calls, uses, and cc0 setters, force them to remain
     in order at the end of the block by adding dependencies and giving
     the last a high priority.  There may be notes present, and prev_head
     may also be a note.

     Branches must obviously remain at the end.  Calls should remain at the
     end since moving them results in worse register allocation.  Uses remain
     at the end to ensure proper register allocation.  cc0 setters remaim
     at the end because they can't be moved away from their cc0 user.  */
  last = 0;
  while (GET_CODE (insn) == CALL_INSN || GET_CODE (insn) == JUMP_INSN
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
#ifdef HAVE_cc0
		 || sets_cc0_p (PATTERN (insn))
#endif
		 ))
	 || GET_CODE (insn) == NOTE)
    {
      if (GET_CODE (insn) != NOTE)
	{
	  priority (insn);
	  if (last == 0)
	    {
	      ready[n_ready++] = insn;
	      INSN_PRIORITY (insn) = TAIL_PRIORITY - i;
	      INSN_REF_COUNT (insn) = 0;
	    }
	  else if (! find_insn_list (insn, LOG_LINKS (last)))
	    {
	      add_dependence (last, insn, REG_DEP_ANTI);
	      INSN_REF_COUNT (insn)++;
	    }
	  last = insn;

	  /* Skip over insns that are part of a group.  */
	  while (SCHED_GROUP_P (insn))
	    {
	      insn = prev_nonnote_insn (insn);
	      priority (insn);
	    }
	}

      insn = PREV_INSN (insn);
      /* Don't overrun the bounds of the basic block.  */
      if (insn == prev_head)
	break;
    }

  /* Assign priorities to instructions.  Also check whether they
     are in priority order already.  If so then I will be nonnegative.
     We use this shortcut only before reloading.  */
#if 0
  i = reload_completed ? DONE_PRIORITY : MAX_PRIORITY;
#endif

  for (; insn != prev_head; insn = PREV_INSN (insn))
    {
      if (GET_RTX_CLASS (GET_CODE (insn)) == 'i')
	{
	  priority (insn);
	  if (INSN_REF_COUNT (insn) == 0)
	    {
	      if (last == 0)
		ready[n_ready++] = insn;
	      else
		{
		  /* Make this dependent on the last of the instructions
		     that must remain in order at the end of the block.  */
		  add_dependence (last, insn, REG_DEP_ANTI);
		  INSN_REF_COUNT (insn) = 1;
		}
	    }
	  if (SCHED_GROUP_P (insn))
	    {
	      while (SCHED_GROUP_P (insn))
		{
		  insn = prev_nonnote_insn (insn);
		  priority (insn);
		}
	      continue;
	    }
#if 0
	  if (i < 0)
	    continue;
	  if (INSN_PRIORITY (insn) < i)
	    i = INSN_PRIORITY (insn);
	  else if (INSN_PRIORITY (insn) > i)
	    i = DONE_PRIORITY;
#endif
	}
    }

#if 0
  /* This short-cut doesn't work.  It does not count call insns crossed by
     registers in reg_sometimes_live.  It does not mark these registers as
     dead if they die in this block.  It does not mark these registers live
     (or create new reg_sometimes_live entries if necessary) if they are born
     in this block.

     The easy solution is to just always schedule a block.  These blocks tend
     to be very short, so this doesn't slow down this pass by much.  */

  /* If existing order is good, don't bother to reorder.  */
  if (i != DONE_PRIORITY)
    {
      if (file)
	fprintf (file, ";; already scheduled\n");

      if (reload_completed == 0)
	{
	  for (i = 0; i < sometimes_max; i++)
	    regs_sometimes_live[i].live_length += n_insns;

	  finish_sometimes_live (regs_sometimes_live, sometimes_max);
	}
      free_pending_lists ();
      return;
    }
#endif

  /* Scan all the insns to be scheduled, removing NOTE insns
     and register death notes.
     Line number NOTE insns end up in NOTE_LIST.
     Register death notes end up in DEAD_NOTES.

     Recreate the register life information for the end of this basic
     block.  */

  if (reload_completed == 0)
    {
      bcopy ((char *) basic_block_live_at_start[b], (char *) bb_live_regs,
	     regset_bytes);
      bzero ((char *) bb_dead_regs, regset_bytes);

      if (b == 0)
	{
	  /* This is the first block in the function.  There may be insns
	     before head that we can't schedule.   We still need to examine
	     them though for accurate register lifetime analysis.  */

	  /* We don't want to remove any REG_DEAD notes as the code below
	     does.  */

	  for (insn = basic_block_head[b]; insn != head;
	       insn = NEXT_INSN (insn))
	    if (GET_RTX_CLASS (GET_CODE (insn)) == 'i')
	      {
		/* See if the register gets born here.  */
		/* We must check for registers being born before we check for
		   registers dying.  It is possible for a register to be born
		   and die in the same insn, e.g. reading from a volatile
		   memory location into an otherwise unused register.  Such
		   a register must be marked as dead after this insn.  */
		if (GET_CODE (PATTERN (insn)) == SET
		    || GET_CODE (PATTERN (insn)) == CLOBBER)
		  sched_note_set (b, PATTERN (insn), 0);
		else if (GET_CODE (PATTERN (insn)) == PARALLEL)
		  {
		    int j;
		    for (j = XVECLEN (PATTERN (insn), 0) - 1; j >= 0; j--)
		      if (GET_CODE (XVECEXP (PATTERN (insn), 0, j)) == SET
			  || GET_CODE (XVECEXP (PATTERN (insn), 0, j)) == CLOBBER)
			sched_note_set (b, XVECEXP (PATTERN (insn), 0, j), 0);

		    /* ??? This code is obsolete and should be deleted.  It
		       is harmless though, so we will leave it in for now.  */
		    for (j = XVECLEN (PATTERN (insn), 0) - 1; j >= 0; j--)
		      if (GET_CODE (XVECEXP (PATTERN (insn), 0, j)) == USE)
			sched_note_set (b, XVECEXP (PATTERN (insn), 0, j), 0);
		  }

		/* Each call clobbers (makes live) all call-clobbered regs
		   that are not global or fixed.  Note that the function-value
		   reg is a call_clobbered reg.  */

		if (GET_CODE (insn) == CALL_INSN)
		  {
		    int j;
		    for (j = 0; j < FIRST_PSEUDO_REGISTER; j++)
		      if (call_used_regs[j] && ! global_regs[j]
			  && ! fixed_regs[j])
			{
			  register int offset = j / REGSET_ELT_BITS;
			  register REGSET_ELT_TYPE bit
			    = (REGSET_ELT_TYPE) 1 << (j % REGSET_ELT_BITS);

			  bb_live_regs[offset] |= bit;
			  bb_dead_regs[offset] &= ~bit;
			}
		  }

		for (link = REG_NOTES (insn); link; link = XEXP (link, 1))
		  {
		    if ((REG_NOTE_KIND (link) == REG_DEAD
			 || REG_NOTE_KIND (link) == REG_UNUSED)
			/* Verify that the REG_NOTE has a valid value.  */
			&& GET_CODE (XEXP (link, 0)) == REG)
		      {
			register int regno = REGNO (XEXP (link, 0));
			register int offset = regno / REGSET_ELT_BITS;
			register REGSET_ELT_TYPE bit
			  = (REGSET_ELT_TYPE) 1 << (regno % REGSET_ELT_BITS);

			if (regno < FIRST_PSEUDO_REGISTER)
			  {
			    int j = HARD_REGNO_NREGS (regno,
						      GET_MODE (XEXP (link, 0)));
			    while (--j >= 0)
			      {
				offset = (regno + j) / REGSET_ELT_BITS;
				bit = ((REGSET_ELT_TYPE) 1
				       << ((regno + j) % REGSET_ELT_BITS));

				bb_live_regs[offset] &= ~bit;
				bb_dead_regs[offset] |= bit;
			      }
			  }
			else
			  {
			    bb_live_regs[offset] &= ~bit;
			    bb_dead_regs[offset] |= bit;
			  }
		      }
		  }
	      }
	}
    }

  /* If debugging information is being produced, keep track of the line
     number notes for each insn.  */
  if (write_symbols != NO_DEBUG)
    {
      /* We must use the true line number for the first insn in the block
	 that was computed and saved at the start of this pass.  We can't
	 use the current line number, because scheduling of the previous
	 block may have changed the current line number.  */
      rtx line = line_note_head[b];

      for (insn = basic_block_head[b];
	   insn != next_tail;
	   insn = NEXT_INSN (insn))
	if (GET_CODE (insn) == NOTE && NOTE_LINE_NUMBER (insn) > 0)
	  line = insn;
	else
	  LINE_NOTE (insn) = line;
    }

  for (insn = head; insn != next_tail; insn = NEXT_INSN (insn))
    {
      rtx prev, next, link;

      /* Farm out notes.  This is needed to keep the debugger from
	 getting completely deranged.  */
      if (GET_CODE (insn) == NOTE)
	{
	  prev = insn;
	  insn = unlink_notes (insn, next_tail);
	  if (prev == tail)
	    abort ();
	  if (prev == head)
	    abort ();
	  if (insn == next_tail)
	    abort ();
	}

      if (reload_completed == 0
	  && GET_RTX_CLASS (GET_CODE (insn)) == 'i')
	{
	  /* See if the register gets born here.  */
	  /* We must check for registers being born before we check for
	     registers dying.  It is possible for a register to be born and
	     die in the same insn, e.g. reading from a volatile memory
	     location into an otherwise unused register.  Such a register
	     must be marked as dead after this insn.  */
	  if (GET_CODE (PATTERN (insn)) == SET
	      || GET_CODE (PATTERN (insn)) == CLOBBER)
	    sched_note_set (b, PATTERN (insn), 0);
	  else if (GET_CODE (PATTERN (insn)) == PARALLEL)
	    {
	      int j;
	      for (j = XVECLEN (PATTERN (insn), 0) - 1; j >= 0; j--)
		if (GET_CODE (XVECEXP (PATTERN (insn), 0, j)) == SET
		    || GET_CODE (XVECEXP (PATTERN (insn), 0, j)) == CLOBBER)
		  sched_note_set (b, XVECEXP (PATTERN (insn), 0, j), 0);

	      /* ??? This code is obsolete and should be deleted.  It
		 is harmless though, so we will leave it in for now.  */
	      for (j = XVECLEN (PATTERN (insn), 0) - 1; j >= 0; j--)
		if (GET_CODE (XVECEXP (PATTERN (insn), 0, j)) == USE)
		  sched_note_set (b, XVECEXP (PATTERN (insn), 0, j), 0);
	    }

	  /* Each call clobbers (makes live) all call-clobbered regs that are
	     not global or fixed.  Note that the function-value reg is a
	     call_clobbered reg.  */

	  if (GET_CODE (insn) == CALL_INSN)
	    {
	      int j;
	      for (j = 0; j < FIRST_PSEUDO_REGISTER; j++)
		if (call_used_regs[j] && ! global_regs[j]
		    && ! fixed_regs[j])
		  {
		    register int offset = j / REGSET_ELT_BITS;
		    register REGSET_ELT_TYPE bit
		      = (REGSET_ELT_TYPE) 1 << (j % REGSET_ELT_BITS);

		    bb_live_regs[offset] |= bit;
		    bb_dead_regs[offset] &= ~bit;
		  }
	    }

	  /* Need to know what registers this insn kills.  */
	  for (prev = 0, link = REG_NOTES (insn); link; link = next)
	    {
	      next = XEXP (link, 1);
	      if ((REG_NOTE_KIND (link) == REG_DEAD
		   || REG_NOTE_KIND (link) == REG_UNUSED)
		  /* Verify that the REG_NOTE has a valid value.  */
		  && GET_CODE (XEXP (link, 0)) == REG)
		{
		  register int regno = REGNO (XEXP (link, 0));
		  register int offset = regno / REGSET_ELT_BITS;
		  register REGSET_ELT_TYPE bit
		    = (REGSET_ELT_TYPE) 1 << (regno % REGSET_ELT_BITS);

		  /* Only unlink REG_DEAD notes; leave REG_UNUSED notes
		     alone.  */
		  if (REG_NOTE_KIND (link) == REG_DEAD)
		    {
		      if (prev)
			XEXP (prev, 1) = next;
		      else
			REG_NOTES (insn) = next;
		      XEXP (link, 1) = dead_notes;
		      dead_notes = link;
		    }
		  else
		    prev = link;

		  if (regno < FIRST_PSEUDO_REGISTER)
		    {
		      int j = HARD_REGNO_NREGS (regno,
						GET_MODE (XEXP (link, 0)));
		      while (--j >= 0)
			{
			  offset = (regno + j) / REGSET_ELT_BITS;
			  bit = ((REGSET_ELT_TYPE) 1
				 << ((regno + j) % REGSET_ELT_BITS));

			  bb_live_regs[offset] &= ~bit;
			  bb_dead_regs[offset] |= bit;
			}
		    }
		  else
		    {
		      bb_live_regs[offset] &= ~bit;
		      bb_dead_regs[offset] |= bit;
		    }
		}
	      else
		prev = link;
	    }
	}
    }

  if (reload_completed == 0)
    {
      /* Keep track of register lives.  */
      old_live_regs = (regset) alloca (regset_bytes);
      regs_sometimes_live
	= (struct sometimes *) alloca (max_regno * sizeof (struct sometimes));
      sometimes_max = 0;

      /* Start with registers live at end.  */
      for (j = 0; j < regset_size; j++)
	{
	  REGSET_ELT_TYPE live = bb_live_regs[j];
	  old_live_regs[j] = live;
	  if (live)
	    {
	      register int bit;
	      for (bit = 0; bit < REGSET_ELT_BITS; bit++)
		if (live & ((REGSET_ELT_TYPE) 1 << bit))
		  sometimes_max = new_sometimes_live (regs_sometimes_live, j,
						      bit, sometimes_max);
	    }
	}
    }

  SCHED_SORT (ready, n_ready, 1);

  if (file)
    {
      fprintf (file, ";; ready list initially:\n;; ");
      for (i = 0; i < n_ready; i++)
	fprintf (file, "%d ", INSN_UID (ready[i]));
      fprintf (file, "\n\n");

      for (insn = head; insn != next_tail; insn = NEXT_INSN (insn))
	if (INSN_PRIORITY (insn) > 0)
	  fprintf (file, ";; insn[%4d]: priority = %4d, ref_count = %4d\n",
		   INSN_UID (insn), INSN_PRIORITY (insn),
		   INSN_REF_COUNT (insn));
    }

  /* Now HEAD and TAIL are going to become disconnected
     entirely from the insn chain.  */
  tail = 0;

  /* Q_SIZE will always be zero here.  */
  q_ptr = 0; clock = 0;
  bzero ((char *) insn_queue, sizeof (insn_queue));

  /* Now, perform list scheduling.  */

  /* Where we start inserting insns is after TAIL.  */
  last = next_tail;

  new_needs = (NEXT_INSN (prev_head) == basic_block_head[b]
	       ? NEED_HEAD : NEED_NOTHING);
  if (PREV_INSN (next_tail) == basic_block_end[b])
    new_needs |= NEED_TAIL;

  new_ready = n_ready;
  while (sched_n_insns < n_insns)
    {
      q_ptr = NEXT_Q (q_ptr); clock++;

      /* Add all pending insns that can be scheduled without stalls to the
	 ready list.  */
      for (insn = insn_queue[q_ptr]; insn; insn = NEXT_INSN (insn))
	{
	  if (file)
	    fprintf (file, ";; launching %d before %d with no stalls at T-%d\n",
		     INSN_UID (insn), INSN_UID (last), clock);
	  ready[new_ready++] = insn;
	  q_size -= 1;
	}
      insn_queue[q_ptr] = 0;

      /* If there are no ready insns, stall until one is ready and add all
	 of the pending insns at that point to the ready list.  */
      if (new_ready == 0)
	{
	  register int stalls;

	  for (stalls = 1; stalls < INSN_QUEUE_SIZE; stalls++)
	    if (insn = insn_queue[NEXT_Q_AFTER (q_ptr, stalls)])
	      {
		for (; insn; insn = NEXT_INSN (insn))
		  {
		    if (file)
		      fprintf (file, ";; launching %d before %d with %d stalls at T-%d\n",
			       INSN_UID (insn), INSN_UID (last), stalls, clock);
		    ready[new_ready++] = insn;
		    q_size -= 1;
		  }
		insn_queue[NEXT_Q_AFTER (q_ptr, stalls)] = 0;
		break;
	      }

	  q_ptr = NEXT_Q_AFTER (q_ptr, stalls); clock += stalls;
	}

      /* There should be some instructions waiting to fire.  */
      if (new_ready == 0)
	abort ();

      if (file)
	{
	  fprintf (file, ";; ready list at T-%d:", clock);
	  for (i = 0; i < new_ready; i++)
	    fprintf (file, " %d (%x)",
		     INSN_UID (ready[i]), INSN_PRIORITY (ready[i]));
	}

      /* Sort the ready list and choose the best insn to schedule.  Select
	 which insn should issue in this cycle and queue those that are
	 blocked by function unit hazards.

	 N_READY holds the number of items that were scheduled the last time,
	 minus the one instruction scheduled on the last loop iteration; it
	 is not modified for any other reason in this loop.  */

      SCHED_SORT (ready, new_ready, n_ready);
      if (MAX_BLOCKAGE > 1)
	{
	  new_ready = schedule_select (ready, new_ready, clock, file);
	  if (new_ready == 0)
	    {
	      if (file)
		fprintf (file, "\n");
	      /* We must set n_ready here, to ensure that sorting always
		 occurs when we come back to the SCHED_SORT line above.  */
	      n_ready = 0;
	      continue;
	    }
	}
      n_ready = new_ready;
      last_scheduled_insn = insn = ready[0];

      /* The first insn scheduled becomes the new tail.  */
      if (tail == 0)
	tail = insn;

      if (file)
	{
	  fprintf (file, ", now");
	  for (i = 0; i < n_ready; i++)
	    fprintf (file, " %d", INSN_UID (ready[i]));
	  fprintf (file, "\n");
	}

      if (DONE_PRIORITY_P (insn))
	abort ();

      if (reload_completed == 0)
	{
	  /* Process this insn, and each insn linked to this one which must
	     be immediately output after this insn.  */
	  do
	    {
	      /* First we kill registers set by this insn, and then we
		 make registers used by this insn live.  This is the opposite
		 order used above because we are traversing the instructions
		 backwards.  */

	      /* Strictly speaking, we should scan REG_UNUSED notes and make
		 every register mentioned there live, however, we will just
		 kill them again immediately below, so there doesn't seem to
		 be any reason why we bother to do this.  */

	      /* See if this is the last notice we must take of a register.  */
	      if (GET_CODE (PATTERN (insn)) == SET
		  || GET_CODE (PATTERN (insn)) == CLOBBER)
		sched_note_set (b, PATTERN (insn), 1);
	      else if (GET_CODE (PATTERN (insn)) == PARALLEL)
		{
		  int j;
		  for (j = XVECLEN (PATTERN (insn), 0) - 1; j >= 0; j--)
		    if (GET_CODE (XVECEXP (PATTERN (insn), 0, j)) == SET
			|| GET_CODE (XVECEXP (PATTERN (insn), 0, j)) == CLOBBER)
		      sched_note_set (b, XVECEXP (PATTERN (insn), 0, j), 1);
		}
	      
	      /* This code keeps life analysis information up to date.  */
	      if (GET_CODE (insn) == CALL_INSN)
		{
		  register struct sometimes *p;

		  /* A call kills all call used registers that are not
		     global or fixed, except for those mentioned in the call
		     pattern which will be made live again later.  */
		  for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
		    if (call_used_regs[i] && ! global_regs[i]
			&& ! fixed_regs[i])
		      {
			register int offset = i / REGSET_ELT_BITS;
			register REGSET_ELT_TYPE bit
			  = (REGSET_ELT_TYPE) 1 << (i % REGSET_ELT_BITS);

			bb_live_regs[offset] &= ~bit;
			bb_dead_regs[offset] |= bit;
		      }

		  /* Regs live at the time of a call instruction must not
		     go in a register clobbered by calls.  Record this for
		     all regs now live.  Note that insns which are born or
		     die in a call do not cross a call, so this must be done
		     after the killings (above) and before the births
		     (below).  */
		  p = regs_sometimes_live;
		  for (i = 0; i < sometimes_max; i++, p++)
		    if (bb_live_regs[p->offset]
			& ((REGSET_ELT_TYPE) 1 << p->bit))
		      p->calls_crossed += 1;
		}

	      /* Make every register used live, and add REG_DEAD notes for
		 registers which were not live before we started.  */
	      attach_deaths_insn (insn);

	      /* Find registers now made live by that instruction.  */
	      for (i = 0; i < regset_size; i++)
		{
		  REGSET_ELT_TYPE diff = bb_live_regs[i] & ~old_live_regs[i];
		  if (diff)
		    {
		      register int bit;
		      old_live_regs[i] |= diff;
		      for (bit = 0; bit < REGSET_ELT_BITS; bit++)
			if (diff & ((REGSET_ELT_TYPE) 1 << bit))
			  sometimes_max
			    = new_sometimes_live (regs_sometimes_live, i, bit,
						  sometimes_max);
		    }
		}

	      /* Count lengths of all regs we are worrying about now,
		 and handle registers no longer live.  */

	      for (i = 0; i < sometimes_max; i++)
		{
		  register struct sometimes *p = &regs_sometimes_live[i];
		  int regno = p->offset*REGSET_ELT_BITS + p->bit;

		  p->live_length += 1;

		  if ((bb_live_regs[p->offset]
		       & ((REGSET_ELT_TYPE) 1 << p->bit)) == 0)
		    {
		      /* This is the end of one of this register's lifetime
			 segments.  Save the lifetime info collected so far,
			 and clear its bit in the old_live_regs entry.  */
		      sched_reg_live_length[regno] += p->live_length;
		      sched_reg_n_calls_crossed[regno] += p->calls_crossed;
		      old_live_regs[p->offset]
			&= ~((REGSET_ELT_TYPE) 1 << p->bit);

		      /* Delete the reg_sometimes_live entry for this reg by
			 copying the last entry over top of it.  */
		      *p = regs_sometimes_live[--sometimes_max];
		      /* ...and decrement i so that this newly copied entry
			 will be processed.  */
		      i--;
		    }
		}

	      link = insn;
	      insn = PREV_INSN (insn);
	    }
	  while (SCHED_GROUP_P (link));

	  /* Set INSN back to the insn we are scheduling now.  */
	  insn = ready[0];
	}

      /* Schedule INSN.  Remove it from the ready list.  */
      ready += 1;
      n_ready -= 1;

      sched_n_insns += 1;
      NEXT_INSN (insn) = last;
      PREV_INSN (last) = insn;

      /* Everything that precedes INSN now either becomes "ready", if
	 it can execute immediately before INSN, or "pending", if
	 there must be a delay.  Give INSN high enough priority that
	 at least one (maybe more) reg-killing insns can be launched
	 ahead of all others.  Mark INSN as scheduled by changing its
	 priority to -1.  */
      INSN_PRIORITY (insn) = LAUNCH_PRIORITY;
      new_ready = schedule_insn (insn, ready, n_ready, clock);
      INSN_PRIORITY (insn) = DONE_PRIORITY;

      /* Schedule all prior insns that must not be moved.  */
      if (SCHED_GROUP_P (insn))
	{
	  /* Disable these insns from being launched, in case one of the
	     insns in the group has a dependency on an earlier one.  */
	  link = insn;
	  while (SCHED_GROUP_P (link))
	    {
	      /* Disable these insns from being launched by anybody.  */
	      link = PREV_INSN (link);
	      INSN_REF_COUNT (link) = 0;
	    }

	  /* Now handle each group insn like the main insn was handled
	     above.  */
	  link = insn;
	  while (SCHED_GROUP_P (link))
	    {
	      link = PREV_INSN (link);

	      sched_n_insns += 1;

	      /* ??? Why don't we set LAUNCH_PRIORITY here?  */
	      new_ready = schedule_insn (link, ready, new_ready, clock);
	      INSN_PRIORITY (link) = DONE_PRIORITY;
	    }
	}

      /* Put back NOTE_INSN_SETJMP,
         NOTE_INSN_{LOOP,EHREGION}_{BEGIN,END} notes.  */

      /* To prime the loop.  We need to handle INSN and all the insns in the
         sched group.  */
      last = NEXT_INSN (insn);
      do
	{
	  insn = PREV_INSN (last);

	  /* Maintain a valid chain so emit_note_before works.
	     This is necessary because PREV_INSN (insn) isn't valid
	     (if ! SCHED_GROUP_P) and if it points to an insn already
	     scheduled, a circularity will result.  */
	  if (! SCHED_GROUP_P (insn))
	    {
	      NEXT_INSN (prev_head) = insn;
	      PREV_INSN (insn) = prev_head;
	    }

	  last = reemit_notes (insn, insn);
	}
      while (SCHED_GROUP_P (insn));
    }
  if (q_size != 0)
    abort ();

  if (reload_completed == 0)
    finish_sometimes_live (regs_sometimes_live, sometimes_max);

  /* HEAD is now the first insn in the chain of insns that
     been scheduled by the loop above.
     TAIL is the last of those insns.  */
  head = last;

  /* NOTE_LIST is the end of a chain of notes previously found
     among the insns.  Insert them at the beginning of the insns.  */
  if (note_list != 0)
    {
      rtx note_head = note_list;
      while (PREV_INSN (note_head))
	note_head = PREV_INSN (note_head);

      PREV_INSN (head) = note_list;
      NEXT_INSN (note_list) = head;
      head = note_head;
    }

  /* There should be no REG_DEAD notes leftover at the end.
     In practice, this can occur as the result of bugs in flow, combine.c,
     and/or sched.c.  The values of the REG_DEAD notes remaining are
     meaningless, because dead_notes is just used as a free list.  */
#if 1
  if (dead_notes != 0)
    abort ();
#endif

  if (new_needs & NEED_HEAD)
    basic_block_head[b] = head;
  PREV_INSN (head) = prev_head;
  NEXT_INSN (prev_head) = head;

  if (new_needs & NEED_TAIL)
    basic_block_end[b] = tail;
  NEXT_INSN (tail) = next_tail;
  PREV_INSN (next_tail) = tail;

  /* Restore the line-number notes of each insn.  */
  if (write_symbols != NO_DEBUG)
    {
      rtx line, note, prev, new;
      int notes = 0;

      head = basic_block_head[b];
      next_tail = NEXT_INSN (basic_block_end[b]);

      /* Determine the current line-number.  We want to know the current
	 line number of the first insn of the block here, in case it is
	 different from the true line number that was saved earlier.  If
	 different, then we need a line number note before the first insn
	 of this block.  If it happens to be the same, then we don't want to
	 emit another line number note here.  */
      for (line = head; line; line = PREV_INSN (line))
	if (GET_CODE (line) == NOTE && NOTE_LINE_NUMBER (line) > 0)
	  break;

      /* Walk the insns keeping track of the current line-number and inserting
	 the line-number notes as needed.  */
      for (insn = head; insn != next_tail; insn = NEXT_INSN (insn))
	if (GET_CODE (insn) == NOTE && NOTE_LINE_NUMBER (insn) > 0)
	  line = insn;
      /* This used to emit line number notes before every non-deleted note.
	 However, this confuses a debugger, because line notes not separated
	 by real instructions all end up at the same address.  I can find no
	 use for line number notes before other notes, so none are emitted.  */
	else if (GET_CODE (insn) != NOTE
		 && (note = LINE_NOTE (insn)) != 0
		 && note != line
		 && (line == 0
		     || NOTE_LINE_NUMBER (note) != NOTE_LINE_NUMBER (line)
		     || NOTE_SOURCE_FILE (note) != NOTE_SOURCE_FILE (line)))
	  {
	    line = note;
	    prev = PREV_INSN (insn);
	    if (LINE_NOTE (note))
	      {
		/* Re-use the original line-number note.  */
		LINE_NOTE (note) = 0;
		PREV_INSN (note) = prev;
		NEXT_INSN (prev) = note;
		PREV_INSN (insn) = note;
		NEXT_INSN (note) = insn;
	      }
	    else
	      {
		notes++;
		new = emit_note_after (NOTE_LINE_NUMBER (note), prev);
		NOTE_SOURCE_FILE (new) = NOTE_SOURCE_FILE (note);
		RTX_INTEGRATED_P (new) = RTX_INTEGRATED_P (note);
	      }
	  }
      if (file && notes)
	fprintf (file, ";; added %d line-number notes\n", notes);
    }

  if (file)
    {
      fprintf (file, ";; total time = %d\n;; new basic block head = %d\n;; new basic block end = %d\n\n",
	       clock, INSN_UID (basic_block_head[b]), INSN_UID (basic_block_end[b]));
    }

  /* Yow! We're done!  */
  free_pending_lists ();

  return;
}

/* Subroutine of split_hard_reg_notes.  Searches X for any reference to
   REGNO, returning the rtx of the reference found if any.  Otherwise,
   returns 0.  */

static rtx
regno_use_in (regno, x)
     int regno;
     rtx x;
{
  register char *fmt;
  int i, j;
  rtx tem;

  if (GET_CODE (x) == REG && REGNO (x) == regno)
    return x;

  fmt = GET_RTX_FORMAT (GET_CODE (x));
  for (i = GET_RTX_LENGTH (GET_CODE (x)) - 1; i >= 0; i--)
    {
      if (fmt[i] == 'e')
	{
	  if (tem = regno_use_in (regno, XEXP (x, i)))
	    return tem;
	}
      else if (fmt[i] == 'E')
	for (j = XVECLEN (x, i) - 1; j >= 0; j--)
	  if (tem = regno_use_in (regno , XVECEXP (x, i, j)))
	    return tem;
    }

  return 0;
}

/* Subroutine of update_flow_info.  Determines whether any new REG_NOTEs are
   needed for the hard register mentioned in the note.  This can happen
   if the reference to the hard register in the original insn was split into
   several smaller hard register references in the split insns.  */

static void
split_hard_reg_notes (note, first, last, orig_insn)
     rtx note, first, last, orig_insn;
{
  rtx reg, temp, link;
  int n_regs, i, new_reg;
  rtx insn;

  /* Assume that this is a REG_DEAD note.  */
  if (REG_NOTE_KIND (note) != REG_DEAD)
    abort ();

  reg = XEXP (note, 0);

  n_regs = HARD_REGNO_NREGS (REGNO (reg), GET_MODE (reg));

  for (i = 0; i < n_regs; i++)
    {
      new_reg = REGNO (reg) + i;

      /* Check for references to new_reg in the split insns.  */
      for (insn = last; ; insn = PREV_INSN (insn))
	{
	  if (GET_RTX_CLASS (GET_CODE (insn)) == 'i'
	      && (temp = regno_use_in (new_reg, PATTERN (insn))))
	    {
	      /* Create a new reg dead note here.  */
	      link = rtx_alloc (EXPR_LIST);
	      PUT_REG_NOTE_KIND (link, REG_DEAD);
	      XEXP (link, 0) = temp;
	      XEXP (link, 1) = REG_NOTES (insn);
	      REG_NOTES (insn) = link;

	      /* If killed multiple registers here, then add in the excess.  */
	      i += HARD_REGNO_NREGS (REGNO (temp), GET_MODE (temp)) - 1;

	      break;
	    }
	  /* It isn't mentioned anywhere, so no new reg note is needed for
	     this register.  */
	  if (insn == first)
	    break;
	}
    }
}

/* Subroutine of update_flow_info.  Determines whether a SET or CLOBBER in an
   insn created by splitting needs a REG_DEAD or REG_UNUSED note added.  */

static void
new_insn_dead_notes (pat, insn, last, orig_insn)
     rtx pat, insn, last, orig_insn;
{
  rtx dest, tem, set;

  /* PAT is either a CLOBBER or a SET here.  */
  dest = XEXP (pat, 0);

  while (GET_CODE (dest) == ZERO_EXTRACT || GET_CODE (dest) == SUBREG
	 || GET_CODE (dest) == STRICT_LOW_PART
	 || GET_CODE (dest) == SIGN_EXTRACT)
    dest = XEXP (dest, 0);

  if (GET_CODE (dest) == REG)
    {
      for (tem = last; tem != insn; tem = PREV_INSN (tem))
	{
	  if (GET_RTX_CLASS (GET_CODE (tem)) == 'i'
	      && reg_overlap_mentioned_p (dest, PATTERN (tem))
	      && (set = single_set (tem)))
	    {
	      rtx tem_dest = SET_DEST (set);

	      while (GET_CODE (tem_dest) == ZERO_EXTRACT
		     || GET_CODE (tem_dest) == SUBREG
		     || GET_CODE (tem_dest) == STRICT_LOW_PART
		     || GET_CODE (tem_dest) == SIGN_EXTRACT)
		tem_dest = XEXP (tem_dest, 0);

	      if (! rtx_equal_p (tem_dest, dest))
		{
		  /* Use the same scheme as combine.c, don't put both REG_DEAD
		     and REG_UNUSED notes on the same insn.  */
		  if (! find_regno_note (tem, REG_UNUSED, REGNO (dest))
		      && ! find_regno_note (tem, REG_DEAD, REGNO (dest)))
		    {
		      rtx note = rtx_alloc (EXPR_LIST);
		      PUT_REG_NOTE_KIND (note, REG_DEAD);
		      XEXP (note, 0) = dest;
		      XEXP (note, 1) = REG_NOTES (tem);
		      REG_NOTES (tem) = note;
		    }
		  /* The reg only dies in one insn, the last one that uses
		     it.  */
		  break;
		}
	      else if (reg_overlap_mentioned_p (dest, SET_SRC (set)))
		/* We found an instruction that both uses the register,
		   and sets it, so no new REG_NOTE is needed for this set.  */
		break;
	    }
	}
      /* If this is a set, it must die somewhere, unless it is the dest of
	 the original insn, and hence is live after the original insn.  Abort
	 if it isn't supposed to be live after the original insn.

	 If this is a clobber, then just add a REG_UNUSED note.  */
      if (tem == insn)
	{
	  int live_after_orig_insn = 0;
	  rtx pattern = PATTERN (orig_insn);
	  int i;

	  if (GET_CODE (pat) == CLOBBER)
	    {
	      rtx note = rtx_alloc (EXPR_LIST);
	      PUT_REG_NOTE_KIND (note, REG_UNUSED);
	      XEXP (note, 0) = dest;
	      XEXP (note, 1) = REG_NOTES (insn);
	      REG_NOTES (insn) = note;
	      return;
	    }

	  /* The original insn could have multiple sets, so search the
	     insn for all sets.  */
	  if (GET_CODE (pattern) == SET)
	    {
	      if (reg_overlap_mentioned_p (dest, SET_DEST (pattern)))
		live_after_orig_insn = 1;
	    }
	  else if (GET_CODE (pattern) == PARALLEL)
	    {
	      for (i = 0; i < XVECLEN (pattern, 0); i++)
		if (GET_CODE (XVECEXP (pattern, 0, i)) == SET
		    && reg_overlap_mentioned_p (dest,
						SET_DEST (XVECEXP (pattern,
								   0, i))))
		  live_after_orig_insn = 1;
	    }

	  if (! live_after_orig_insn)
	    abort ();
	}
    }
}

/* Subroutine of update_flow_info.  Update the value of reg_n_sets for all
   registers modified by X.  INC is -1 if the containing insn is being deleted,
   and is 1 if the containing insn is a newly generated insn.  */

static void
update_n_sets (x, inc)
     rtx x;
     int inc;
{
  rtx dest = SET_DEST (x);

  while (GET_CODE (dest) == STRICT_LOW_PART || GET_CODE (dest) == SUBREG
	 || GET_CODE (dest) == ZERO_EXTRACT || GET_CODE (dest) == SIGN_EXTRACT)
    dest = SUBREG_REG (dest);
	  
  if (GET_CODE (dest) == REG)
    {
      int regno = REGNO (dest);
      
      if (regno < FIRST_PSEUDO_REGISTER)
	{
	  register int i;
	  int endregno = regno + HARD_REGNO_NREGS (regno, GET_MODE (dest));
	  
	  for (i = regno; i < endregno; i++)
	    reg_n_sets[i] += inc;
	}
      else
	reg_n_sets[regno] += inc;
    }
}

/* Updates all flow-analysis related quantities (including REG_NOTES) for
   the insns from FIRST to LAST inclusive that were created by splitting
   ORIG_INSN.  NOTES are the original REG_NOTES.  */

static void
update_flow_info (notes, first, last, orig_insn)
     rtx notes;
     rtx first, last;
     rtx orig_insn;
{
  rtx insn, note;
  rtx next;
  rtx orig_dest, temp;
  rtx set;

  /* Get and save the destination set by the original insn.  */

  orig_dest = single_set (orig_insn);
  if (orig_dest)
    orig_dest = SET_DEST (orig_dest);

  /* Move REG_NOTES from the original insn to where they now belong.  */

  for (note = notes; note; note = next)
    {
      next = XEXP (note, 1);
      switch (REG_NOTE_KIND (note))
	{
	case REG_DEAD:
	case REG_UNUSED:
	  /* Move these notes from the original insn to the last new insn where
	     the register is now set.  */

	  for (insn = last; ; insn = PREV_INSN (insn))
	    {
	      if (GET_RTX_CLASS (GET_CODE (insn)) == 'i'
		  && reg_mentioned_p (XEXP (note, 0), PATTERN (insn)))
		{
		  /* If this note refers to a multiple word hard register, it
		     may have been split into several smaller hard register
		     references, so handle it specially.  */
		  temp = XEXP (note, 0);
		  if (REG_NOTE_KIND (note) == REG_DEAD
		      && GET_CODE (temp) == REG
		      && REGNO (temp) < FIRST_PSEUDO_REGISTER
		      && HARD_REGNO_NREGS (REGNO (temp), GET_MODE (temp)) > 1)
		    split_hard_reg_notes (note, first, last, orig_insn);
		  else
		    {
		      XEXP (note, 1) = REG_NOTES (insn);
		      REG_NOTES (insn) = note;
		    }

		  /* Sometimes need to convert REG_UNUSED notes to REG_DEAD
		     notes.  */
		  /* ??? This won't handle multiple word registers correctly,
		     but should be good enough for now.  */
		  if (REG_NOTE_KIND (note) == REG_UNUSED
		      && ! dead_or_set_p (insn, XEXP (note, 0)))
		    PUT_REG_NOTE_KIND (note, REG_DEAD);

		  /* The reg only dies in one insn, the last one that uses
		     it.  */
		  break;
		}
	      /* It must die somewhere, fail it we couldn't find where it died.

		 If this is a REG_UNUSED note, then it must be a temporary
		 register that was not needed by this instantiation of the
		 pattern, so we can safely ignore it.  */
	      if (insn == first)
		{
		  /* After reload, REG_DEAD notes come sometimes an
		     instruction after the register actually dies.  */
		  if (reload_completed && REG_NOTE_KIND (note) == REG_DEAD)
		    {
		      XEXP (note, 1) = REG_NOTES (insn);
		      REG_NOTES (insn) = note;
		      break;
		    }
			
		  if (REG_NOTE_KIND (note) != REG_UNUSED)
		    abort ();

		  break;
		}
	    }
	  break;

	case REG_WAS_0:
	  /* This note applies to the dest of the original insn.  Find the
	     first new insn that now has the same dest, and move the note
	     there.  */

	  if (! orig_dest)
	    abort ();

	  for (insn = first; ; insn = NEXT_INSN (insn))
	    {
	      if (GET_RTX_CLASS (GET_CODE (insn)) == 'i'
		  && (temp = single_set (insn))
		  && rtx_equal_p (SET_DEST (temp), orig_dest))
		{
		  XEXP (note, 1) = REG_NOTES (insn);
		  REG_NOTES (insn) = note;
		  /* The reg is only zero before one insn, the first that
		     uses it.  */
		  break;
		}
	      /* If this note refers to a multiple word hard
		 register, it may have been split into several smaller
		 hard register references.  We could split the notes,
		 but simply dropping them is good enough.  */
	      if (GET_CODE (orig_dest) == REG
		  && REGNO (orig_dest) < FIRST_PSEUDO_REGISTER
		  && HARD_REGNO_NREGS (REGNO (orig_dest),
				       GET_MODE (orig_dest)) > 1)
		    break;
	      /* It must be set somewhere, fail if we couldn't find where it
		 was set.  */
	      if (insn == last)
		abort ();
	    }
	  break;

	case REG_EQUAL:
	case REG_EQUIV:
	  /* A REG_EQUIV or REG_EQUAL note on an insn with more than one
	     set is meaningless.  Just drop the note.  */
	  if (! orig_dest)
	    break;

	case REG_NO_CONFLICT:
	  /* These notes apply to the dest of the original insn.  Find the last
	     new insn that now has the same dest, and move the note there.  */

	  if (! orig_dest)
	    abort ();

	  for (insn = last; ; insn = PREV_INSN (insn))
	    {
	      if (GET_RTX_CLASS (GET_CODE (insn)) == 'i'
		  && (temp = single_set (insn))
		  && rtx_equal_p (SET_DEST (temp), orig_dest))
		{
		  XEXP (note, 1) = REG_NOTES (insn);
		  REG_NOTES (insn) = note;
		  /* Only put this note on one of the new insns.  */
		  break;
		}

	      /* The original dest must still be set someplace.  Abort if we
		 couldn't find it.  */
	      if (insn == first)
		{
		  /* However, if this note refers to a multiple word hard
		     register, it may have been split into several smaller
		     hard register references.  We could split the notes,
		     but simply dropping them is good enough.  */
		  if (GET_CODE (orig_dest) == REG
		      && REGNO (orig_dest) < FIRST_PSEUDO_REGISTER
		      && HARD_REGNO_NREGS (REGNO (orig_dest),
					   GET_MODE (orig_dest)) > 1)
		    break;
		  /* Likewise for multi-word memory references.  */
		  if (GET_CODE (orig_dest) == MEM
		      && SIZE_FOR_MODE (orig_dest) > MOVE_MAX)
		    break;
		  abort ();
		}
	    }
	  break;

	case REG_LIBCALL:
	  /* Move a REG_LIBCALL note to the first insn created, and update
	     the corresponding REG_RETVAL note.  */
	  XEXP (note, 1) = REG_NOTES (first);
	  REG_NOTES (first) = note;

	  insn = XEXP (note, 0);
	  note = find_reg_note (insn, REG_RETVAL, NULL_RTX);
	  if (note)
	    XEXP (note, 0) = first;
	  break;

	case REG_EXEC_COUNT:
	  /* Move a REG_EXEC_COUNT note to the first insn created.  */
	  XEXP (note, 1) = REG_NOTES (first);
	  REG_NOTES (first) = note;
	  break;

	case REG_RETVAL:
	  /* Move a REG_RETVAL note to the last insn created, and update
	     the corresponding REG_LIBCALL note.  */
	  XEXP (note, 1) = REG_NOTES (last);
	  REG_NOTES (last) = note;

	  insn = XEXP (note, 0);
	  note = find_reg_note (insn, REG_LIBCALL, NULL_RTX);
	  if (note)
	    XEXP (note, 0) = last;
	  break;

	case REG_NONNEG:
	case REG_BR_PROB:
	  /* This should be moved to whichever instruction is a JUMP_INSN.  */

	  for (insn = last; ; insn = PREV_INSN (insn))
	    {
	      if (GET_CODE (insn) == JUMP_INSN)
		{
		  XEXP (note, 1) = REG_NOTES (insn);
		  REG_NOTES (insn) = note;
		  /* Only put this note on one of the new insns.  */
		  break;
		}
	      /* Fail if we couldn't find a JUMP_INSN.  */
	      if (insn == first)
		abort ();
	    }
	  break;

	case REG_INC:
	  /* reload sometimes leaves obsolete REG_INC notes around.  */
	  if (reload_completed)
	    break;
	  /* This should be moved to whichever instruction now has the
	     increment operation.  */
	  abort ();

	case REG_LABEL:
	  /* Should be moved to the new insn(s) which use the label.  */
	  for (insn = first; insn != NEXT_INSN (last); insn = NEXT_INSN (insn))
	    if (GET_RTX_CLASS (GET_CODE (insn)) == 'i'
		&& reg_mentioned_p (XEXP (note, 0), PATTERN (insn)))
	      REG_NOTES (insn) = gen_rtx (EXPR_LIST, REG_LABEL,
					  XEXP (note, 0), REG_NOTES (insn));
	  break;

	case REG_CC_SETTER:
	case REG_CC_USER:
	  /* These two notes will never appear until after reorg, so we don't
	     have to handle them here.  */
	default:
	  abort ();
	}
    }

  /* Each new insn created, except the last, has a new set.  If the destination
     is a register, then this reg is now live across several insns, whereas
     previously the dest reg was born and died within the same insn.  To
     reflect this, we now need a REG_DEAD note on the insn where this
     dest reg dies.

     Similarly, the new insns may have clobbers that need REG_UNUSED notes.  */

  for (insn = first; insn != last; insn = NEXT_INSN (insn))
    {
      rtx pat;
      int i;

      pat = PATTERN (insn);
      if (GET_CODE (pat) == SET || GET_CODE (pat) == CLOBBER)
	new_insn_dead_notes (pat, insn, last, orig_insn);
      else if (GET_CODE (pat) == PARALLEL)
	{
	  for (i = 0; i < XVECLEN (pat, 0); i++)
	    if (GET_CODE (XVECEXP (pat, 0, i)) == SET
		|| GET_CODE (XVECEXP (pat, 0, i)) == CLOBBER)
	      new_insn_dead_notes (XVECEXP (pat, 0, i), insn, last, orig_insn);
	}
    }

  /* If any insn, except the last, uses the register set by the last insn,
     then we need a new REG_DEAD note on that insn.  In this case, there
     would not have been a REG_DEAD note for this register in the original
     insn because it was used and set within one insn.

     There is no new REG_DEAD note needed if the last insn uses the register
     that it is setting.  */

  set = single_set (last);
  if (set)
    {
      rtx dest = SET_DEST (set);

      while (GET_CODE (dest) == ZERO_EXTRACT || GET_CODE (dest) == SUBREG
	     || GET_CODE (dest) == STRICT_LOW_PART
	     || GET_CODE (dest) == SIGN_EXTRACT)
	dest = XEXP (dest, 0);

      if (GET_CODE (dest) == REG
	  /* Global registers are always live, so the code below does not
	     apply to them.  */
	  && (REGNO (dest) >= FIRST_PSEUDO_REGISTER
	      || ! global_regs[REGNO (dest)])
	  && ! reg_overlap_mentioned_p (dest, SET_SRC (set)))
	{
	  for (insn = PREV_INSN (last); ; insn = PREV_INSN (insn))
	    {
	      if (GET_RTX_CLASS (GET_CODE (insn)) == 'i'
		  && reg_mentioned_p (dest, PATTERN (insn))
		  && (set = single_set (insn)))
		{
		  rtx insn_dest = SET_DEST (set);

		  while (GET_CODE (insn_dest) == ZERO_EXTRACT
			 || GET_CODE (insn_dest) == SUBREG
			 || GET_CODE (insn_dest) == STRICT_LOW_PART
			 || GET_CODE (insn_dest) == SIGN_EXTRACT)
		    insn_dest = XEXP (insn_dest, 0);

		  if (insn_dest != dest)
		    {
		      note = rtx_alloc (EXPR_LIST);
		      PUT_REG_NOTE_KIND (note, REG_DEAD);
		      XEXP (note, 0) = dest;
		      XEXP (note, 1) = REG_NOTES (insn);
		      REG_NOTES (insn) = note;
		      /* The reg only dies in one insn, the last one
			 that uses it.  */
		      break;
		    }
		}
	      if (insn == first)
		break;
	    }
	}
    }

  /* If the original dest is modifying a multiple register target, and the
     original instruction was split such that the original dest is now set
     by two or more SUBREG sets, then the split insns no longer kill the
     destination of the original insn.

     In this case, if there exists an instruction in the same basic block,
     before the split insn, which uses the original dest, and this use is
     killed by the original insn, then we must remove the REG_DEAD note on
     this insn, because it is now superfluous.

     This does not apply when a hard register gets split, because the code
     knows how to handle overlapping hard registers properly.  */
  if (orig_dest && GET_CODE (orig_dest) == REG)
    {
      int found_orig_dest = 0;
      int found_split_dest = 0;

      for (insn = first; ; insn = NEXT_INSN (insn))
	{
	  set = single_set (insn);
	  if (set)
	    {
	      if (GET_CODE (SET_DEST (set)) == REG
		  && REGNO (SET_DEST (set)) == REGNO (orig_dest))
		{
		  found_orig_dest = 1;
		  break;
		}
	      else if (GET_CODE (SET_DEST (set)) == SUBREG
		       && SUBREG_REG (SET_DEST (set)) == orig_dest)
		{
		  found_split_dest = 1;
		  break;
		}
	    }

	  if (insn == last)
	    break;
	}

      if (found_split_dest)
	{
	  /* Search backwards from FIRST, looking for the first insn that uses
	     the original dest.  Stop if we pass a CODE_LABEL or a JUMP_INSN.
	     If we find an insn, and it has a REG_DEAD note, then delete the
	     note.  */

	  for (insn = first; insn; insn = PREV_INSN (insn))
	    {
	      if (GET_CODE (insn) == CODE_LABEL
		  || GET_CODE (insn) == JUMP_INSN)
		break;
	      else if (GET_RTX_CLASS (GET_CODE (insn)) == 'i'
		       && reg_mentioned_p (orig_dest, insn))
		{
		  note = find_regno_note (insn, REG_DEAD, REGNO (orig_dest));
		  if (note)
		    remove_note (insn, note);
		}
	    }
	}
      else if (! found_orig_dest)
	{
	  /* This should never happen.  */
	  abort ();
	}
    }

  /* Update reg_n_sets.  This is necessary to prevent local alloc from
     converting REG_EQUAL notes to REG_EQUIV when splitting has modified
     a reg from set once to set multiple times.  */

  {
    rtx x = PATTERN (orig_insn);
    RTX_CODE code = GET_CODE (x);

    if (code == SET || code == CLOBBER)
      update_n_sets (x, -1);
    else if (code == PARALLEL)
      {
	int i;
	for (i = XVECLEN (x, 0) - 1; i >= 0; i--)
	  {
	    code = GET_CODE (XVECEXP (x, 0, i));
	    if (code == SET || code == CLOBBER)
	      update_n_sets (XVECEXP (x, 0, i), -1);
	  }
      }

    for (insn = first; ; insn = NEXT_INSN (insn))
      {
	x = PATTERN (insn);
	code = GET_CODE (x);

	if (code == SET || code == CLOBBER)
	  update_n_sets (x, 1);
	else if (code == PARALLEL)
	  {
	    int i;
	    for (i = XVECLEN (x, 0) - 1; i >= 0; i--)
	      {
		code = GET_CODE (XVECEXP (x, 0, i));
		if (code == SET || code == CLOBBER)
		  update_n_sets (XVECEXP (x, 0, i), 1);
	      }
	  }

	if (insn == last)
	  break;
      }
  }
}

/* The one entry point in this file.  DUMP_FILE is the dump file for
   this pass.  */

void
schedule_insns (dump_file)
     FILE *dump_file;
{
  int max_uid = MAX_INSNS_PER_SPLIT * (get_max_uid () + 1);
  int b;
  rtx insn;

  /* Taking care of this degenerate case makes the rest of
     this code simpler.  */
  if (n_basic_blocks == 0)
    return;

  /* Create an insn here so that we can hang dependencies off of it later.  */
  sched_before_next_call
    = gen_rtx (INSN, VOIDmode, 0, NULL_RTX, NULL_RTX,
	       NULL_RTX, 0, NULL_RTX, 0);

  /* Initialize the unused_*_lists.  We can't use the ones left over from
     the previous function, because gcc has freed that memory.  We can use
     the ones left over from the first sched pass in the second pass however,
     so only clear them on the first sched pass.  The first pass is before
     reload if flag_schedule_insns is set, otherwise it is afterwards.  */

  if (reload_completed == 0 || ! flag_schedule_insns)
    {
      unused_insn_list = 0;
      unused_expr_list = 0;
    }

  /* We create no insns here, only reorder them, so we
     remember how far we can cut back the stack on exit.  */

  /* Allocate data for this pass.  See comments, above,
     for what these vectors do.  */
  insn_luid = (int *) alloca (max_uid * sizeof (int));
  insn_priority = (int *) alloca (max_uid * sizeof (int));
  insn_tick = (int *) alloca (max_uid * sizeof (int));
  insn_costs = (short *) alloca (max_uid * sizeof (short));
  insn_units = (short *) alloca (max_uid * sizeof (short));
  insn_blockage = (unsigned int *) alloca (max_uid * sizeof (unsigned int));
  insn_ref_count = (int *) alloca (max_uid * sizeof (int));

  if (reload_completed == 0)
    {
      sched_reg_n_deaths = (short *) alloca (max_regno * sizeof (short));
      sched_reg_n_calls_crossed = (int *) alloca (max_regno * sizeof (int));
      sched_reg_live_length = (int *) alloca (max_regno * sizeof (int));
      bb_dead_regs = (regset) alloca (regset_bytes);
      bb_live_regs = (regset) alloca (regset_bytes);
      bzero ((char *) sched_reg_n_calls_crossed, max_regno * sizeof (int));
      bzero ((char *) sched_reg_live_length, max_regno * sizeof (int));
      bcopy ((char *) reg_n_deaths, (char *) sched_reg_n_deaths,
	     max_regno * sizeof (short));
    }
  else
    {
      sched_reg_n_deaths = 0;
      sched_reg_n_calls_crossed = 0;
      sched_reg_live_length = 0;
      bb_dead_regs = 0;
      bb_live_regs = 0;
    }
  init_alias_analysis ();
#if 0
  if (dump_file)
    {
      extern rtx *reg_base_value;
      extern int reg_base_value_size;
      int i;
      for (i = 0; i < reg_base_value_size; i++)
	if (reg_base_value[i])
	  {
	    fprintf (dump_file, ";; reg_base_value[%d] = ", i);
	    print_rtl (dump_file, reg_base_value[i]);
	    fputc ('\n', dump_file);
	  }
    }
#endif


  if (write_symbols != NO_DEBUG)
    {
      rtx line;

      line_note = (rtx *) alloca (max_uid * sizeof (rtx));
      bzero ((char *) line_note, max_uid * sizeof (rtx));
      line_note_head = (rtx *) alloca (n_basic_blocks * sizeof (rtx));
      bzero ((char *) line_note_head, n_basic_blocks * sizeof (rtx));

      /* Determine the line-number at the start of each basic block.
	 This must be computed and saved now, because after a basic block's
	 predecessor has been scheduled, it is impossible to accurately
	 determine the correct line number for the first insn of the block.  */
	 
      for (b = 0; b < n_basic_blocks; b++)
	for (line = basic_block_head[b]; line; line = PREV_INSN (line))
	  if (GET_CODE (line) == NOTE && NOTE_LINE_NUMBER (line) > 0)
	    {
	      line_note_head[b] = line;
	      break;
	    }
    }

  bzero ((char *) insn_luid, max_uid * sizeof (int));
  bzero ((char *) insn_priority, max_uid * sizeof (int));
  bzero ((char *) insn_tick, max_uid * sizeof (int));
  bzero ((char *) insn_costs, max_uid * sizeof (short));
  bzero ((char *) insn_units, max_uid * sizeof (short));
  bzero ((char *) insn_blockage, max_uid * sizeof (unsigned int));
  bzero ((char *) insn_ref_count, max_uid * sizeof (int));

  /* Schedule each basic block, block by block.  */

  /* ??? Add a NOTE after the last insn of the last basic block.  It is not
     known why this is done.  */
  /* ??? Perhaps it's done to ensure NEXT_TAIL in schedule_block is a
     valid insn.  */

  insn = basic_block_end[n_basic_blocks-1];
  if (NEXT_INSN (insn) == 0
      || (GET_CODE (insn) != NOTE
	  && GET_CODE (insn) != CODE_LABEL
	  /* Don't emit a NOTE if it would end up between an unconditional
	     jump and a BARRIER.  */
	  && ! (GET_CODE (insn) == JUMP_INSN
		&& GET_CODE (NEXT_INSN (insn)) == BARRIER)))
    emit_note_after (NOTE_INSN_DELETED, basic_block_end[n_basic_blocks-1]);

  for (b = 0; b < n_basic_blocks; b++)
    {
      rtx insn, next;

      note_list = 0;

      for (insn = basic_block_head[b]; ; insn = next)
	{
	  rtx prev;
	  rtx set;

	  /* Can't use `next_real_insn' because that
	     might go across CODE_LABELS and short-out basic blocks.  */
	  next = NEXT_INSN (insn);
	  if (GET_CODE (insn) != INSN)
	    {
	      if (insn == basic_block_end[b])
		break;

	      continue;
	    }

	  /* Don't split no-op move insns.  These should silently disappear
	     later in final.  Splitting such insns would break the code
	     that handles REG_NO_CONFLICT blocks.  */
	  set = single_set (insn);
	  if (set && rtx_equal_p (SET_SRC (set), SET_DEST (set)))
	    {
	      if (insn == basic_block_end[b])
		break;

	      /* Nops get in the way while scheduling, so delete them now if
		 register allocation has already been done.  It is too risky
		 to try to do this before register allocation, and there are
		 unlikely to be very many nops then anyways.  */
	      if (reload_completed)
		{
		  PUT_CODE (insn, NOTE);
		  NOTE_LINE_NUMBER (insn) = NOTE_INSN_DELETED;
		  NOTE_SOURCE_FILE (insn) = 0;
		}

	      continue;
	    }

	  /* Split insns here to get max fine-grain parallelism.  */
	  prev = PREV_INSN (insn);
	  /* It is probably not worthwhile to try to split again in the
	     second pass.  However, if flag_schedule_insns is not set,
	     the first and only (if any) scheduling pass is after reload.  */
	  if (reload_completed == 0 || ! flag_schedule_insns)
	    {
	      rtx last, first = PREV_INSN (insn);
	      rtx notes = REG_NOTES (insn);

	      last = try_split (PATTERN (insn), insn, 1);
	      if (last != insn)
		{
		  /* try_split returns the NOTE that INSN became.  */
		  first = NEXT_INSN (first);
		  update_flow_info (notes, first, last, insn);

		  PUT_CODE (insn, NOTE);
		  NOTE_SOURCE_FILE (insn) = 0;
		  NOTE_LINE_NUMBER (insn) = NOTE_INSN_DELETED;
		  if (insn == basic_block_head[b])
		    basic_block_head[b] = first;
		  if (insn == basic_block_end[b])
		    {
		      basic_block_end[b] = last;
		      break;
		    }
		}
	    }

	  if (insn == basic_block_end[b])
	    break;
	}

      schedule_block (b, dump_file);

#ifdef USE_C_ALLOCA
      alloca (0);
#endif
    }

  /* Reposition the prologue and epilogue notes in case we moved the
     prologue/epilogue insns.  */
  if (reload_completed)
    reposition_prologue_and_epilogue_notes (get_insns ());

  if (write_symbols != NO_DEBUG)
    {
      rtx line = 0;
      rtx insn = get_insns ();
      int active_insn = 0;
      int notes = 0;

      /* Walk the insns deleting redundant line-number notes.  Many of these
	 are already present.  The remainder tend to occur at basic
	 block boundaries.  */
      for (insn = get_last_insn (); insn; insn = PREV_INSN (insn))
	if (GET_CODE (insn) == NOTE && NOTE_LINE_NUMBER (insn) > 0)
	  {
	    /* If there are no active insns following, INSN is redundant.  */
	    if (active_insn == 0)
	      {
		notes++;
		NOTE_SOURCE_FILE (insn) = 0;
		NOTE_LINE_NUMBER (insn) = NOTE_INSN_DELETED;
	      }
	    /* If the line number is unchanged, LINE is redundant.  */
	    else if (line
		     && NOTE_LINE_NUMBER (line) == NOTE_LINE_NUMBER (insn)
		     && NOTE_SOURCE_FILE (line) == NOTE_SOURCE_FILE (insn))
	      {
		notes++;
		NOTE_SOURCE_FILE (line) = 0;
		NOTE_LINE_NUMBER (line) = NOTE_INSN_DELETED;
		line = insn;
	      }
	    else
	      line = insn;
	    active_insn = 0;
	  }
	else if (! ((GET_CODE (insn) == NOTE
		     && NOTE_LINE_NUMBER (insn) == NOTE_INSN_DELETED)
		    || (GET_CODE (insn) == INSN
			&& (GET_CODE (PATTERN (insn)) == USE
			    || GET_CODE (PATTERN (insn)) == CLOBBER))))
	  active_insn++;

      if (dump_file && notes)
	fprintf (dump_file, ";; deleted %d line-number notes\n", notes);
    }

  if (reload_completed == 0)
    {
      int regno;
      for (regno = 0; regno < max_regno; regno++)
	if (sched_reg_live_length[regno])
	  {
	    if (dump_file)
	      {
		if (reg_live_length[regno] > sched_reg_live_length[regno])
		  fprintf (dump_file,
			   ";; register %d life shortened from %d to %d\n",
			   regno, reg_live_length[regno],
			   sched_reg_live_length[regno]);
		/* Negative values are special; don't overwrite the current
		   reg_live_length value if it is negative.  */
		else if (reg_live_length[regno] < sched_reg_live_length[regno]
			 && reg_live_length[regno] >= 0)
		  fprintf (dump_file,
			   ";; register %d life extended from %d to %d\n",
			   regno, reg_live_length[regno],
			   sched_reg_live_length[regno]);

		if (! reg_n_calls_crossed[regno]
		    && sched_reg_n_calls_crossed[regno])
		  fprintf (dump_file,
			   ";; register %d now crosses calls\n", regno);
		else if (reg_n_calls_crossed[regno]
			 && ! sched_reg_n_calls_crossed[regno]
			 && reg_basic_block[regno] != REG_BLOCK_GLOBAL)
		  fprintf (dump_file,
			   ";; register %d no longer crosses calls\n", regno);

	      }
	    /* Negative values are special; don't overwrite the current
	       reg_live_length value if it is negative.  */
	    if (reg_live_length[regno] >= 0)
	      reg_live_length[regno] = sched_reg_live_length[regno];

	    /* We can't change the value of reg_n_calls_crossed to zero for
	       pseudos which are live in more than one block.

	       This is because combine might have made an optimization which
	       invalidated basic_block_live_at_start and reg_n_calls_crossed,
	       but it does not update them.  If we update reg_n_calls_crossed
	       here, the two variables are now inconsistent, and this might
	       confuse the caller-save code into saving a register that doesn't
	       need to be saved.  This is only a problem when we zero calls
	       crossed for a pseudo live in multiple basic blocks.

	       Alternatively, we could try to correctly update basic block live
	       at start here in sched, but that seems complicated.  */
	    if (sched_reg_n_calls_crossed[regno]
		|| reg_basic_block[regno] != REG_BLOCK_GLOBAL)
	      reg_n_calls_crossed[regno] = sched_reg_n_calls_crossed[regno];
	  }
    }
}
#endif /* INSN_SCHEDULING */
