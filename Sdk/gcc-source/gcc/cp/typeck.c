/* Build expressions with type checking for C++ compiler.
   Copyright (C) 1987, 1988, 1989, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Hacked by Michael Tiemann (tiemann@cygnus.com)

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


/* This file is part of the C++ front end.
   It contains routines to build C++ expressions given their operands,
   including computing the types of the result, C and C++ specific error
   checks, and some optimization.

   There are also routines to build RETURN_STMT nodes and CASE_STMT nodes,
   and to process initializations in declarations (since they work
   like a strange sort of assignment).  */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "rtl.h"
#include "expr.h"
#include "cp-tree.h"
#include "tm_p.h"
#include "flags.h"
#include "output.h"
#include "toplev.h"
#include "diagnostic.h"
#include "target.h"

static tree convert_for_assignment PARAMS ((tree, tree, const char *, tree,
					  int));
static tree cp_pointer_int_sum PARAMS ((enum tree_code, tree, tree));
static tree rationalize_conditional_expr PARAMS ((enum tree_code, tree));
static int comp_target_parms PARAMS ((tree, tree));
static int comp_ptr_ttypes_real PARAMS ((tree, tree, int));
static int comp_ptr_ttypes_const PARAMS ((tree, tree));
static int comp_ptr_ttypes_reinterpret PARAMS ((tree, tree));
static int comp_except_types PARAMS ((tree, tree, int));
static int comp_array_types PARAMS ((int (*) (tree, tree, int), tree,
				   tree, int));
static tree common_base_type PARAMS ((tree, tree));
static tree lookup_anon_field PARAMS ((tree, tree));
static tree pointer_diff PARAMS ((tree, tree, tree));
static tree qualify_type_recursive PARAMS ((tree, tree));
static tree get_delta_difference PARAMS ((tree, tree, int));
static int comp_cv_target_types PARAMS ((tree, tree, int));
static void casts_away_constness_r PARAMS ((tree *, tree *));
static int casts_away_constness PARAMS ((tree, tree));
static void maybe_warn_about_returning_address_of_local PARAMS ((tree));
static tree strip_all_pointer_quals PARAMS ((tree));

/* Return the target type of TYPE, which means return T for:
   T*, T&, T[], T (...), and otherwise, just T.  */

tree
target_type (type)
     tree type;
{
  if (TREE_CODE (type) == REFERENCE_TYPE)
    type = TREE_TYPE (type);
  while (TREE_CODE (type) == POINTER_TYPE
	 || TREE_CODE (type) == ARRAY_TYPE
	 || TREE_CODE (type) == FUNCTION_TYPE
	 || TREE_CODE (type) == METHOD_TYPE
	 || TREE_CODE (type) == OFFSET_TYPE)
    type = TREE_TYPE (type);
  return type;
}

/* Do `exp = require_complete_type (exp);' to make sure exp
   does not have an incomplete type.  (That includes void types.)
   Returns the error_mark_node if the VALUE does not have
   complete type when this function returns.  */

tree
require_complete_type (value)
     tree value;
{
  tree type;

  if (processing_template_decl || value == error_mark_node)
    return value;

  if (TREE_CODE (value) == OVERLOAD)
    type = unknown_type_node;
  else
    type = TREE_TYPE (value);

  /* First, detect a valid value with a complete type.  */
  if (COMPLETE_TYPE_P (type))
    return value;

  /* If we see X::Y, we build an OFFSET_TYPE which has
     not been laid out.  Try to avoid an error by interpreting
     it as this->X::Y, if reasonable.  */
  if (TREE_CODE (value) == OFFSET_REF
      && current_class_ref != 0
      && TREE_OPERAND (value, 0) == current_class_ref)
    {
      value = resolve_offset_ref (value);
      return require_complete_type (value);
    }

  if (complete_type_or_else (type, value))
    return value;
  else
    return error_mark_node;
}

/* Try to complete TYPE, if it is incomplete.  For example, if TYPE is
   a template instantiation, do the instantiation.  Returns TYPE,
   whether or not it could be completed, unless something goes
   horribly wrong, in which case the error_mark_node is returned.  */

tree
complete_type (type)
     tree type;
{
  if (type == NULL_TREE)
    /* Rather than crash, we return something sure to cause an error
       at some point.  */
    return error_mark_node;

  if (type == error_mark_node || COMPLETE_TYPE_P (type))
    ;
  else if (TREE_CODE (type) == ARRAY_TYPE && TYPE_DOMAIN (type))
    {
      tree t = complete_type (TREE_TYPE (type));
      if (COMPLETE_TYPE_P (t) && ! processing_template_decl)
	layout_type (type);
      TYPE_NEEDS_CONSTRUCTING (type)
	= TYPE_NEEDS_CONSTRUCTING (TYPE_MAIN_VARIANT (t));
      TYPE_HAS_NONTRIVIAL_DESTRUCTOR (type)
	= TYPE_HAS_NONTRIVIAL_DESTRUCTOR (TYPE_MAIN_VARIANT (t));
    }
  else if (CLASS_TYPE_P (type) && CLASSTYPE_TEMPLATE_INSTANTIATION (type))
    instantiate_class_template (TYPE_MAIN_VARIANT (type));

  return type;
}

/* Like complete_type, but issue an error if the TYPE cannot be completed.
   VALUE is used for informative diagnostics.  DIAG_TYPE indicates the type
   of diagnostic: 0 for an error, 1 for a warning, 2 for a pedwarn.
   Returns NULL_TREE if the type cannot be made complete.  */

tree
complete_type_or_diagnostic (type, value, diag_type)
     tree type;
     tree value;
     int diag_type;
{
  type = complete_type (type);
  if (type == error_mark_node)
    /* We already issued an error.  */
    return NULL_TREE;
  else if (!COMPLETE_TYPE_P (type))
    {
      cxx_incomplete_type_diagnostic (value, type, diag_type);
      return NULL_TREE;
    }
  else
    return type;
}

/* Return truthvalue of whether type of EXP is instantiated.  */

int
type_unknown_p (exp)
     tree exp;
{
  return (TREE_CODE (exp) == OVERLOAD
          || TREE_CODE (exp) == TREE_LIST
	  || TREE_TYPE (exp) == unknown_type_node
	  || (TREE_CODE (TREE_TYPE (exp)) == OFFSET_TYPE
	      && TREE_TYPE (TREE_TYPE (exp)) == unknown_type_node));
}

/* Return a pointer or pointer to member type similar to T1, with a
   cv-qualification signature that is the union of the cv-qualification
   signatures of T1 and T2: [expr.rel], [expr.eq].  */

static tree
qualify_type_recursive (t1, t2)
     tree t1, t2;
{
  if ((TYPE_PTR_P (t1) && TYPE_PTR_P (t2))
      || (TYPE_PTRMEM_P (t1) && TYPE_PTRMEM_P (t2)))
    {
      tree tt1;
      tree tt2;
      tree b1;
      int type_quals;
      tree tgt;
      tree attributes = (*targetm.merge_type_attributes) (t1, t2);

      if (TYPE_PTRMEM_P (t1))
	{
	  b1 = TYPE_PTRMEM_CLASS_TYPE (t1);
	  tt1 = TYPE_PTRMEM_POINTED_TO_TYPE (t1);
	  tt2 = TYPE_PTRMEM_POINTED_TO_TYPE (t2);
	}
      else
	{
	  b1 = NULL_TREE;
	  tt1 = TREE_TYPE (t1);
	  tt2 = TREE_TYPE (t2);
	}

      type_quals = (cp_type_quals (tt1) | cp_type_quals (tt2));
      tgt = qualify_type_recursive (tt1, tt2);
      tgt = cp_build_qualified_type (tgt, type_quals);
      if (b1)
	t1 = build_ptrmem_type (b1, tgt);
      else
	t1 = build_pointer_type (tgt);
      t1 = build_type_attribute_variant (t1, attributes);
    }
  return t1;
}

/* Return the common type of two parameter lists.
   We assume that comptypes has already been done and returned 1;
   if that isn't so, this may crash.

   As an optimization, free the space we allocate if the parameter
   lists are already common.  */

tree
commonparms (p1, p2)
     tree p1, p2;
{
  tree oldargs = p1, newargs, n;
  int i, len;
  int any_change = 0;

  len = list_length (p1);
  newargs = tree_last (p1);

  if (newargs == void_list_node)
    i = 1;
  else
    {
      i = 0;
      newargs = 0;
    }

  for (; i < len; i++)
    newargs = tree_cons (NULL_TREE, NULL_TREE, newargs);

  n = newargs;

  for (i = 0; p1;
       p1 = TREE_CHAIN (p1), p2 = TREE_CHAIN (p2), n = TREE_CHAIN (n), i++)
    {
      if (TREE_PURPOSE (p1) && !TREE_PURPOSE (p2))
	{
	  TREE_PURPOSE (n) = TREE_PURPOSE (p1);
	  any_change = 1;
	}
      else if (! TREE_PURPOSE (p1))
	{
	  if (TREE_PURPOSE (p2))
	    {
	      TREE_PURPOSE (n) = TREE_PURPOSE (p2);
	      any_change = 1;
	    }
	}
      else
	{
	  if (1 != simple_cst_equal (TREE_PURPOSE (p1), TREE_PURPOSE (p2)))
	    any_change = 1;
	  TREE_PURPOSE (n) = TREE_PURPOSE (p2);
	}
      if (TREE_VALUE (p1) != TREE_VALUE (p2))
	{
	  any_change = 1;
	  TREE_VALUE (n) = merge_types (TREE_VALUE (p1), TREE_VALUE (p2));
	}
      else
	TREE_VALUE (n) = TREE_VALUE (p1);
    }
  if (! any_change)
    return oldargs;

  return newargs;
}

/* Given a type, perhaps copied for a typedef,
   find the "original" version of it.  */
tree
original_type (t)
     tree t;
{
  while (TYPE_NAME (t) != NULL_TREE)
    {
      tree x = TYPE_NAME (t);
      if (TREE_CODE (x) != TYPE_DECL)
	break;
      x = DECL_ORIGINAL_TYPE (x);
      if (x == NULL_TREE)
	break;
      t = x;
    }
  return t;
}

/* T1 and T2 are arithmetic or enumeration types.  Return the type
   that will result from the "usual arithmetic conversions" on T1 and
   T2 as described in [expr].  */

tree
type_after_usual_arithmetic_conversions (t1, t2)
     tree t1;
     tree t2;
{
  enum tree_code code1 = TREE_CODE (t1);
  enum tree_code code2 = TREE_CODE (t2);
  tree attributes;

  /* FIXME: Attributes.  */
  my_friendly_assert (ARITHMETIC_TYPE_P (t1) 
		      || TREE_CODE (t1) == COMPLEX_TYPE
		      || TREE_CODE (t1) == ENUMERAL_TYPE,
		      19990725);
  my_friendly_assert (ARITHMETIC_TYPE_P (t2) 
		      || TREE_CODE (t2) == COMPLEX_TYPE
		      || TREE_CODE (t2) == ENUMERAL_TYPE,
		      19990725);

  /* In what follows, we slightly generalize the rules given in [expr] so
     as to deal with `long long' and `complex'.  First, merge the
     attributes.  */
  attributes = (*targetm.merge_type_attributes) (t1, t2);

  /* If one type is complex, form the common type of the non-complex
     components, then make that complex.  Use T1 or T2 if it is the
     required type.  */
  if (code1 == COMPLEX_TYPE || code2 == COMPLEX_TYPE)
    {
      tree subtype1 = code1 == COMPLEX_TYPE ? TREE_TYPE (t1) : t1;
      tree subtype2 = code2 == COMPLEX_TYPE ? TREE_TYPE (t2) : t2;
      tree subtype
	= type_after_usual_arithmetic_conversions (subtype1, subtype2);

      if (code1 == COMPLEX_TYPE && TREE_TYPE (t1) == subtype)
	return build_type_attribute_variant (t1, attributes);
      else if (code2 == COMPLEX_TYPE && TREE_TYPE (t2) == subtype)
	return build_type_attribute_variant (t2, attributes);
      else
	return build_type_attribute_variant (build_complex_type (subtype),
					     attributes);
    }

  /* If only one is real, use it as the result.  */
  if (code1 == REAL_TYPE && code2 != REAL_TYPE)
    return build_type_attribute_variant (t1, attributes);
  if (code2 == REAL_TYPE && code1 != REAL_TYPE)
    return build_type_attribute_variant (t2, attributes);

  /* Perform the integral promotions.  */
  if (code1 != REAL_TYPE)
    {
      t1 = type_promotes_to (t1);
      t2 = type_promotes_to (t2);
    }

  /* Both real or both integers; use the one with greater precision.  */
  if (TYPE_PRECISION (t1) > TYPE_PRECISION (t2))
    return build_type_attribute_variant (t1, attributes);
  else if (TYPE_PRECISION (t2) > TYPE_PRECISION (t1))
    return build_type_attribute_variant (t2, attributes);

  /* The types are the same; no need to do anything fancy.  */
  if (TYPE_MAIN_VARIANT (t1) == TYPE_MAIN_VARIANT (t2))
    return build_type_attribute_variant (t1, attributes);

  if (code1 != REAL_TYPE)
    {
      /* If one is a sizetype, use it so size_binop doesn't blow up.  */
      if (TYPE_IS_SIZETYPE (t1) > TYPE_IS_SIZETYPE (t2))
	return build_type_attribute_variant (t1, attributes);
      if (TYPE_IS_SIZETYPE (t2) > TYPE_IS_SIZETYPE (t1))
	return build_type_attribute_variant (t2, attributes);

      /* If one is unsigned long long, then convert the other to unsigned
	 long long.  */
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_long_unsigned_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_long_unsigned_type_node))
	return build_type_attribute_variant (long_long_unsigned_type_node,
					     attributes);
      /* If one is a long long, and the other is an unsigned long, and
	 long long can represent all the values of an unsigned long, then
	 convert to a long long.  Otherwise, convert to an unsigned long
	 long.  Otherwise, if either operand is long long, convert the
	 other to long long.
	 
	 Since we're here, we know the TYPE_PRECISION is the same;
	 therefore converting to long long cannot represent all the values
	 of an unsigned long, so we choose unsigned long long in that
	 case.  */
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_long_integer_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_long_integer_type_node))
	{
	  tree t = ((TREE_UNSIGNED (t1) || TREE_UNSIGNED (t2))
		    ? long_long_unsigned_type_node 
		    : long_long_integer_type_node);
	  return build_type_attribute_variant (t, attributes);
	}
      
      /* Go through the same procedure, but for longs.  */
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_unsigned_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_unsigned_type_node))
	return build_type_attribute_variant (long_unsigned_type_node,
					     attributes);
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_integer_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_integer_type_node))
	{
	  tree t = ((TREE_UNSIGNED (t1) || TREE_UNSIGNED (t2))
		    ? long_unsigned_type_node : long_integer_type_node);
	  return build_type_attribute_variant (t, attributes);
	}
      /* Otherwise prefer the unsigned one.  */
      if (TREE_UNSIGNED (t1))
	return build_type_attribute_variant (t1, attributes);
      else
	return build_type_attribute_variant (t2, attributes);
    }
  else
    {
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_double_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_double_type_node))
	return build_type_attribute_variant (long_double_type_node,
					     attributes);
      if (same_type_p (TYPE_MAIN_VARIANT (t1), double_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), double_type_node))
	return build_type_attribute_variant (double_type_node,
					     attributes);
      if (same_type_p (TYPE_MAIN_VARIANT (t1), float_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), float_type_node))
	return build_type_attribute_variant (float_type_node,
					     attributes);
      
      /* Two floating-point types whose TYPE_MAIN_VARIANTs are none of
         the standard C++ floating-point types.  Logic earlier in this
         function has already eliminated the possibility that
         TYPE_PRECISION (t2) != TYPE_PRECISION (t1), so there's no
         compelling reason to choose one or the other.  */
      return build_type_attribute_variant (t1, attributes);
    }
}

/* Return the composite pointer type (see [expr.rel]) for T1 and T2.
   ARG1 and ARG2 are the values with those types.  The LOCATION is a
   string describing the current location, in case an error occurs.  */

tree 
composite_pointer_type (t1, t2, arg1, arg2, location)
     tree t1;
     tree t2;
     tree arg1;
     tree arg2;
     const char* location;
{
  tree result_type;
  tree attributes;

  /* [expr.rel]

     If one operand is a null pointer constant, the composite pointer
     type is the type of the other operand.  */
  if (null_ptr_cst_p (arg1))
    return t2;
  if (null_ptr_cst_p (arg2))
    return t1;
 
  /* Deal with pointer-to-member functions in the same way as we deal
     with pointers to functions.  */
  if (TYPE_PTRMEMFUNC_P (t1))
    t1 = TYPE_PTRMEMFUNC_FN_TYPE (t1);
  if (TYPE_PTRMEMFUNC_P (t2))
    t2 = TYPE_PTRMEMFUNC_FN_TYPE (t2);
  
  /* Merge the attributes.  */
  attributes = (*targetm.merge_type_attributes) (t1, t2);

  /* We have:

       [expr.rel]

       If one of the operands has type "pointer to cv1 void*", then
       the other has type "pointer to cv2T", and the composite pointer
       type is "pointer to cv12 void", where cv12 is the union of cv1
       and cv2.

    If either type is a pointer to void, make sure it is T1.  */
  if (VOID_TYPE_P (TREE_TYPE (t2)))
    {
      tree t;
      t = t1;
      t1 = t2;
      t2 = t;
    }
  /* Now, if T1 is a pointer to void, merge the qualifiers.  */
  if (VOID_TYPE_P (TREE_TYPE (t1)))
    {
      if (pedantic && TYPE_PTRFN_P (t2))
	pedwarn ("ISO C++ forbids %s between pointer of type `void *' and pointer-to-function", location);
      t1 = TREE_TYPE (t1);
      t2 = TREE_TYPE (t2);
      result_type = cp_build_qualified_type (void_type_node,
					     (cp_type_quals (t1)
					      | cp_type_quals (t2)));
      result_type = build_pointer_type (result_type);
    }
  else
    {
      tree full1 = qualify_type_recursive (t1, t2);
      tree full2 = qualify_type_recursive (t2, t1);

      int val = comp_target_types (full1, full2, 1);

      if (val > 0)
	result_type = full1;
      else if (val < 0)
	result_type = full2;
      else
	{
	  pedwarn ("%s between distinct pointer types `%T' and `%T' lacks a cast",
		      location, t1, t2);
	  result_type = ptr_type_node;
	}
    }

  return build_type_attribute_variant (result_type, attributes);
}

/* Return the merged type of two types.
   We assume that comptypes has already been done and returned 1;
   if that isn't so, this may crash.

   This just combines attributes and default arguments; any other
   differences would cause the two types to compare unalike.  */

tree
merge_types (t1, t2)
     tree t1, t2;
{
  register enum tree_code code1;
  register enum tree_code code2;
  tree attributes;

  /* Save time if the two types are the same.  */
  if (t1 == t2)
    return t1;
  if (original_type (t1) == original_type (t2))
    return t1;

  /* If one type is nonsense, use the other.  */
  if (t1 == error_mark_node)
    return t2;
  if (t2 == error_mark_node)
    return t1;

  /* Merge the attributes.  */
  attributes = (*targetm.merge_type_attributes) (t1, t2);

  /* Treat an enum type as the unsigned integer type of the same width.  */

  if (TYPE_PTRMEMFUNC_P (t1))
    t1 = TYPE_PTRMEMFUNC_FN_TYPE (t1);
  if (TYPE_PTRMEMFUNC_P (t2))
    t2 = TYPE_PTRMEMFUNC_FN_TYPE (t2);

  code1 = TREE_CODE (t1);
  code2 = TREE_CODE (t2);

  switch (code1)
    {
    case POINTER_TYPE:
    case REFERENCE_TYPE:
      /* For two pointers, do this recursively on the target type.  */
      {
	tree target = merge_types (TREE_TYPE (t1), TREE_TYPE (t2));
	int quals = cp_type_quals (t1);

	if (code1 == POINTER_TYPE)
	  t1 = build_pointer_type (target);
	else
	  t1 = build_reference_type (target);
	t1 = build_type_attribute_variant (t1, attributes);
	t1 = cp_build_qualified_type (t1, quals);

	if (TREE_CODE (target) == METHOD_TYPE)
	  t1 = build_ptrmemfunc_type (t1);

	return t1;
      }

    case OFFSET_TYPE:
      {
	tree base = TYPE_OFFSET_BASETYPE (t1);
	tree target = merge_types (TREE_TYPE (t1), TREE_TYPE (t2));
	t1 = build_offset_type (base, target);
	break;
      }

    case ARRAY_TYPE:
      {
	tree elt = merge_types (TREE_TYPE (t1), TREE_TYPE (t2));
	/* Save space: see if the result is identical to one of the args.  */
	if (elt == TREE_TYPE (t1) && TYPE_DOMAIN (t1))
	  return build_type_attribute_variant (t1, attributes);
	if (elt == TREE_TYPE (t2) && TYPE_DOMAIN (t2))
	  return build_type_attribute_variant (t2, attributes);
	/* Merge the element types, and have a size if either arg has one.  */
	t1 = build_cplus_array_type
	  (elt, TYPE_DOMAIN (TYPE_DOMAIN (t1) ? t1 : t2));
	break;
      }

    case FUNCTION_TYPE:
      /* Function types: prefer the one that specified arg types.
	 If both do, merge the arg types.  Also merge the return types.  */
      {
	tree valtype = merge_types (TREE_TYPE (t1), TREE_TYPE (t2));
	tree p1 = TYPE_ARG_TYPES (t1);
	tree p2 = TYPE_ARG_TYPES (t2);
	tree rval, raises;

	/* Save space: see if the result is identical to one of the args.  */
	if (valtype == TREE_TYPE (t1) && ! p2)
	  return build_type_attribute_variant (t1, attributes);
	if (valtype == TREE_TYPE (t2) && ! p1)
	  return build_type_attribute_variant (t2, attributes);

	/* Simple way if one arg fails to specify argument types.  */
	if (p1 == NULL_TREE || TREE_VALUE (p1) == void_type_node)
	  {
	    rval = build_function_type (valtype, p2);
	    if ((raises = TYPE_RAISES_EXCEPTIONS (t2)))
	      rval = build_exception_variant (rval, raises);
	    return build_type_attribute_variant (rval, attributes);
	  }
	raises = TYPE_RAISES_EXCEPTIONS (t1);
	if (p2 == NULL_TREE || TREE_VALUE (p2) == void_type_node)
	  {
	    rval = build_function_type (valtype, p1);
	    if (raises)
	      rval = build_exception_variant (rval, raises);
	    return build_type_attribute_variant (rval, attributes);
	  }

	rval = build_function_type (valtype, commonparms (p1, p2));
	t1 = build_exception_variant (rval, raises);
	break;
      }

    case METHOD_TYPE:
      {
	/* Get this value the long way, since TYPE_METHOD_BASETYPE
	   is just the main variant of this.  */
	tree basetype = TREE_TYPE (TREE_VALUE (TYPE_ARG_TYPES (t2)));
	tree raises = TYPE_RAISES_EXCEPTIONS (t1);
	tree t3;

	/* If this was a member function type, get back to the
	   original type of type member function (i.e., without
	   the class instance variable up front.  */
	t1 = build_function_type (TREE_TYPE (t1),
				  TREE_CHAIN (TYPE_ARG_TYPES (t1)));
	t2 = build_function_type (TREE_TYPE (t2),
				  TREE_CHAIN (TYPE_ARG_TYPES (t2)));
	t3 = merge_types (t1, t2);
	t3 = build_cplus_method_type (basetype, TREE_TYPE (t3),
				      TYPE_ARG_TYPES (t3));
	t1 = build_exception_variant (t3, raises);
	break;
      }

    default:;
    }
  return build_type_attribute_variant (t1, attributes);
}

/* Return the common type of two types.
   We assume that comptypes has already been done and returned 1;
   if that isn't so, this may crash.

   This is the type for the result of most arithmetic operations
   if the operands have the given two types.  */

tree
common_type (t1, t2)
     tree t1, t2;
{
  enum tree_code code1;
  enum tree_code code2;

  /* If one type is nonsense, bail.  */
  if (t1 == error_mark_node || t2 == error_mark_node)
    return error_mark_node;

  code1 = TREE_CODE (t1);
  code2 = TREE_CODE (t2);

  if ((ARITHMETIC_TYPE_P (t1) || code1 == ENUMERAL_TYPE
       || code1 == COMPLEX_TYPE)
      && (ARITHMETIC_TYPE_P (t2) || code2 == ENUMERAL_TYPE
	  || code2 == COMPLEX_TYPE))
    return type_after_usual_arithmetic_conversions (t1, t2);

  else if ((TYPE_PTR_P (t1) && TYPE_PTR_P (t2))
	   || (TYPE_PTRMEM_P (t1) && TYPE_PTRMEM_P (t2))
	   || (TYPE_PTRMEMFUNC_P (t1) && TYPE_PTRMEMFUNC_P (t2)))
    return composite_pointer_type (t1, t2, error_mark_node, error_mark_node,
				   "conversion");

  else
    abort ();
}

/* Compare two exception specifier types for exactness or subsetness, if
   allowed. Returns 0 for mismatch, 1 for same, 2 if B is allowed by A.
 
   [except.spec] "If a class X ... objects of class X or any class publicly
   and unambigously derrived from X. Similarly, if a pointer type Y * ...
   exceptions of type Y * or that are pointers to any type publicly and
   unambigously derrived from Y. Otherwise a function only allows exceptions
   that have the same type ..."
   This does not mention cv qualifiers and is different to what throw
   [except.throw] and catch [except.catch] will do. They will ignore the
   top level cv qualifiers, and allow qualifiers in the pointer to class
   example.
   
   We implement the letter of the standard.  */

static int
comp_except_types (a, b, exact)
     tree a, b;
     int exact;
{
  if (same_type_p (a, b))
    return 1;
  else if (!exact)
    {
      if (cp_type_quals (a) || cp_type_quals (b))
        return 0;
      
      if (TREE_CODE (a) == POINTER_TYPE
          && TREE_CODE (b) == POINTER_TYPE)
        {
          a = TREE_TYPE (a);
          b = TREE_TYPE (b);
          if (cp_type_quals (a) || cp_type_quals (b))
            return 0;
        }
      
      if (TREE_CODE (a) != RECORD_TYPE
          || TREE_CODE (b) != RECORD_TYPE)
        return 0;
      
      if (ACCESSIBLY_UNIQUELY_DERIVED_P (a, b))
        return 2;
    }
  return 0;
}

/* Return 1 if TYPE1 and TYPE2 are equivalent exception specifiers.
   If EXACT is 0, T2 can be stricter than T1 (according to 15.4/7),
   otherwise it must be exact. Exception lists are unordered, but
   we've already filtered out duplicates. Most lists will be in order,
   we should try to make use of that.  */

int
comp_except_specs (t1, t2, exact)
     tree t1, t2;
     int exact;
{
  tree probe;
  tree base;
  int  length = 0;

  if (t1 == t2)
    return 1;
  
  if (t1 == NULL_TREE)              /* T1 is ...  */
    return t2 == NULL_TREE || !exact;
  if (!TREE_VALUE (t1)) /* t1 is EMPTY */
    return t2 != NULL_TREE && !TREE_VALUE (t2);
  if (t2 == NULL_TREE)              /* T2 is ...  */
    return 0;
  if (TREE_VALUE (t1) && !TREE_VALUE (t2)) /* T2 is EMPTY, T1 is not */
    return !exact;
  
  /* Neither set is ... or EMPTY, make sure each part of T2 is in T1.
     Count how many we find, to determine exactness. For exact matching and
     ordered T1, T2, this is an O(n) operation, otherwise its worst case is
     O(nm).  */
  for (base = t1; t2 != NULL_TREE; t2 = TREE_CHAIN (t2))
    {
      for (probe = base; probe != NULL_TREE; probe = TREE_CHAIN (probe))
        {
          tree a = TREE_VALUE (probe);
          tree b = TREE_VALUE (t2);
          
          if (comp_except_types (a, b, exact))
            {
              if (probe == base && exact)
                base = TREE_CHAIN (probe);
              length++;
              break;
            }
        }
      if (probe == NULL_TREE)
        return 0;
    }
  return !exact || base == NULL_TREE || length == list_length (t1);
}

/* Compare the array types T1 and T2, using CMP as the type comparison
   function for the element types.  STRICT is as for comptypes.  */

static int
comp_array_types (cmp, t1, t2, strict)
     register int (*cmp) PARAMS ((tree, tree, int));
     tree t1, t2;
     int strict;
{
  tree d1;
  tree d2;

  if (t1 == t2)
    return 1;

  /* The type of the array elements must be the same.  */
  if (!(TREE_TYPE (t1) == TREE_TYPE (t2)
	|| (*cmp) (TREE_TYPE (t1), TREE_TYPE (t2), 
		   strict & ~COMPARE_REDECLARATION)))
    return 0;

  d1 = TYPE_DOMAIN (t1);
  d2 = TYPE_DOMAIN (t2);

  if (d1 == d2)
    return 1;

  /* If one of the arrays is dimensionless, and the other has a
     dimension, they are of different types.  However, it is valid to
     write:

       extern int a[];
       int a[3];

     by [basic.link]: 

       declarations for an array object can specify
       array types that differ by the presence or absence of a major
       array bound (_dcl.array_).  */
  if (!d1 || !d2)
    return strict & COMPARE_REDECLARATION;

  /* Check that the dimensions are the same.  */
  return (cp_tree_equal (TYPE_MIN_VALUE (d1),
			 TYPE_MIN_VALUE (d2))
	  && cp_tree_equal (TYPE_MAX_VALUE (d1),
			    TYPE_MAX_VALUE (d2)));
}

/* Return 1 if T1 and T2 are compatible types for assignment or
   various other operations.  STRICT is a bitwise-or of the COMPARE_*
   flags.  */

int
comptypes (t1, t2, strict)
     tree t1;
     tree t2;
     int strict;
{
  int attrval, val;
  int orig_strict = strict;

  /* The special exemption for redeclaring array types without an
     array bound only applies at the top level:

       extern int (*i)[];
       int (*i)[8];

     is invalid, for example.  */
  strict &= ~COMPARE_REDECLARATION;

  /* Suppress errors caused by previously reported errors */
  if (t1 == t2)
    return 1;

  /* This should never happen.  */
  my_friendly_assert (t1 != error_mark_node, 307);

  if (t2 == error_mark_node)
    return 0;

  /* If either type is the internal version of sizetype, return the
     language version.  */
  if (TREE_CODE (t1) == INTEGER_TYPE && TYPE_IS_SIZETYPE (t1)
      && TYPE_DOMAIN (t1) != 0)
    t1 = TYPE_DOMAIN (t1);

  if (TREE_CODE (t2) == INTEGER_TYPE && TYPE_IS_SIZETYPE (t2)
      && TYPE_DOMAIN (t2) != 0)
    t2 = TYPE_DOMAIN (t2);

  if (strict & COMPARE_RELAXED)
    {
      /* Treat an enum type as the unsigned integer type of the same width.  */

      if (TREE_CODE (t1) == ENUMERAL_TYPE)
	t1 = c_common_type_for_size (TYPE_PRECISION (t1), 1);
      if (TREE_CODE (t2) == ENUMERAL_TYPE)
	t2 = c_common_type_for_size (TYPE_PRECISION (t2), 1);

      if (t1 == t2)
	return 1;
    }

  if (TYPE_PTRMEMFUNC_P (t1))
    t1 = TYPE_PTRMEMFUNC_FN_TYPE (t1);
  if (TYPE_PTRMEMFUNC_P (t2))
    t2 = TYPE_PTRMEMFUNC_FN_TYPE (t2);

  /* Different classes of types can't be compatible.  */
  if (TREE_CODE (t1) != TREE_CODE (t2))
    return 0;

  /* Qualifiers must match.  */
  if (cp_type_quals (t1) != cp_type_quals (t2))
    return 0;
  if (strict == COMPARE_STRICT 
      && TYPE_FOR_JAVA (t1) != TYPE_FOR_JAVA (t2))
    return 0;

  /* Allow for two different type nodes which have essentially the same
     definition.  Note that we already checked for equality of the type
     qualifiers (just above).  */

  if (TYPE_MAIN_VARIANT (t1) == TYPE_MAIN_VARIANT (t2))
    return 1;

  if (strict & COMPARE_NO_ATTRIBUTES)
    attrval = 1;
  /* 1 if no need for warning yet, 2 if warning cause has been seen.  */
  else if (! (attrval = (*targetm.comp_type_attributes) (t1, t2)))
     return 0;

  /* 1 if no need for warning yet, 2 if warning cause has been seen.  */
  val = 0;

  switch (TREE_CODE (t1))
    {
    case TEMPLATE_TEMPLATE_PARM:
    case BOUND_TEMPLATE_TEMPLATE_PARM:
      if (TEMPLATE_TYPE_IDX (t1) != TEMPLATE_TYPE_IDX (t2)
	  || TEMPLATE_TYPE_LEVEL (t1) != TEMPLATE_TYPE_LEVEL (t2))
	return 0;
      if (! comp_template_parms
	      (DECL_TEMPLATE_PARMS (TEMPLATE_TEMPLATE_PARM_TEMPLATE_DECL (t1)),
	       DECL_TEMPLATE_PARMS (TEMPLATE_TEMPLATE_PARM_TEMPLATE_DECL (t2))))
	return 0;
      if (TREE_CODE (t1) == TEMPLATE_TEMPLATE_PARM)
	return 1;
      /* Don't check inheritance.  */
      strict = COMPARE_STRICT;
      /* fall through */

    case RECORD_TYPE:
    case UNION_TYPE:
      if (TYPE_TEMPLATE_INFO (t1) && TYPE_TEMPLATE_INFO (t2)
	  && (TYPE_TI_TEMPLATE (t1) == TYPE_TI_TEMPLATE (t2)
	      || TREE_CODE (t1) == BOUND_TEMPLATE_TEMPLATE_PARM))
	val = comp_template_args (TYPE_TI_ARGS (t1),
				  TYPE_TI_ARGS (t2));
    look_hard:
      if ((strict & COMPARE_BASE) && DERIVED_FROM_P (t1, t2))
	val = 1;
      else if ((strict & COMPARE_RELAXED) && DERIVED_FROM_P (t2, t1))
	val = 1;
      break;

    case OFFSET_TYPE:
      val = (comptypes (build_pointer_type (TYPE_OFFSET_BASETYPE (t1)),
			build_pointer_type (TYPE_OFFSET_BASETYPE (t2)), strict)
	     && comptypes (TREE_TYPE (t1), TREE_TYPE (t2), strict));
      break;

    case POINTER_TYPE:
    case REFERENCE_TYPE:
      t1 = TREE_TYPE (t1);
      t2 = TREE_TYPE (t2);
      /* first, check whether the referred types match with the
         required level of strictness */
      val = comptypes (t1, t2, strict);
      if (val)
	break;
      if (TREE_CODE (t1) == RECORD_TYPE 
	  && TREE_CODE (t2) == RECORD_TYPE)
	goto look_hard;
      break;

    case METHOD_TYPE:
    case FUNCTION_TYPE:
      val = ((TREE_TYPE (t1) == TREE_TYPE (t2)
	      || comptypes (TREE_TYPE (t1), TREE_TYPE (t2), strict))
	     && compparms (TYPE_ARG_TYPES (t1), TYPE_ARG_TYPES (t2)));
      break;

    case ARRAY_TYPE:
      /* Target types must match incl. qualifiers.  We use ORIG_STRICT
	 here since this is the one place where
	 COMPARE_REDECLARATION should be used.  */
      val = comp_array_types (comptypes, t1, t2, orig_strict);
      break;

    case TEMPLATE_TYPE_PARM:
      return TEMPLATE_TYPE_IDX (t1) == TEMPLATE_TYPE_IDX (t2)
	&& TEMPLATE_TYPE_LEVEL (t1) == TEMPLATE_TYPE_LEVEL (t2);

    case TYPENAME_TYPE:
      if (cp_tree_equal (TYPENAME_TYPE_FULLNAME (t1),
                         TYPENAME_TYPE_FULLNAME (t2)) < 1)
        return 0;
      return same_type_p (TYPE_CONTEXT (t1), TYPE_CONTEXT (t2));

    case UNBOUND_CLASS_TEMPLATE:
      if (cp_tree_equal (TYPE_IDENTIFIER (t1),
                         TYPE_IDENTIFIER (t2)) < 1)
        return 0;
      return same_type_p (TYPE_CONTEXT (t1), TYPE_CONTEXT (t2));

    case COMPLEX_TYPE:
      return same_type_p (TREE_TYPE (t1), TREE_TYPE (t2));

    default:
      break;
    }
  return attrval == 2 && val == 1 ? 2 : val;
}

/* Subroutine of comp_target-types.  Make sure that the cv-quals change
   only in the same direction as the target type.  */

static int
comp_cv_target_types (ttl, ttr, nptrs)
     tree ttl, ttr;
     int nptrs;
{
  int t;

  if (!at_least_as_qualified_p (ttl, ttr)
      && !at_least_as_qualified_p (ttr, ttl))
    /* The qualifications are incomparable.  */
    return 0;

  if (TYPE_MAIN_VARIANT (ttl) == TYPE_MAIN_VARIANT (ttr))
    return more_qualified_p (ttr, ttl) ? -1 : 1;

  t = comp_target_types (ttl, ttr, nptrs);
  if ((t == 1 && at_least_as_qualified_p (ttl, ttr)) 
      || (t == -1 && at_least_as_qualified_p (ttr, ttl)))
    return t;

  return 0;
}

/* Return 1 or -1 if TTL and TTR are pointers to types that are equivalent,
   ignoring their qualifiers, 0 if not. Return 1 means that TTR can be
   converted to TTL. Return -1 means that TTL can be converted to TTR but
   not vice versa.

   NPTRS is the number of pointers we can strip off and keep cool.
   This is used to permit (for aggr A, aggr B) A, B* to convert to A*,
   but to not permit B** to convert to A**.

   This should go away.  Callers should use can_convert or something
   similar instead.  (jason 17 Apr 1997)  */

int
comp_target_types (ttl, ttr, nptrs)
     tree ttl, ttr;
     int nptrs;
{
  ttl = TYPE_MAIN_VARIANT (ttl);
  ttr = TYPE_MAIN_VARIANT (ttr);
  if (same_type_p (ttl, ttr))
    return 1;

  if (TREE_CODE (ttr) != TREE_CODE (ttl))
    return 0;

  if ((TREE_CODE (ttr) == POINTER_TYPE
       || TREE_CODE (ttr) == REFERENCE_TYPE)
      /* If we get a pointer with nptrs == 0, we don't allow any tweaking
	 of the type pointed to.  This is necessary for reference init
	 semantics.  We won't get here from a previous call with nptrs == 1;
	 for multi-level pointers we end up in comp_ptr_ttypes.  */
      && nptrs > 0)
    {
      int is_ptr = TREE_CODE (ttr) == POINTER_TYPE;

      ttl = TREE_TYPE (ttl);
      ttr = TREE_TYPE (ttr);

      if (is_ptr)
	{
	  if (TREE_CODE (ttl) == UNKNOWN_TYPE
	      || TREE_CODE (ttr) == UNKNOWN_TYPE)
	    return 1;
	  else if (TREE_CODE (ttl) == VOID_TYPE
		   && TREE_CODE (ttr) != FUNCTION_TYPE
		   && TREE_CODE (ttr) != METHOD_TYPE
		   && TREE_CODE (ttr) != OFFSET_TYPE)
	    return 1;
	  else if (TREE_CODE (ttr) == VOID_TYPE
		   && TREE_CODE (ttl) != FUNCTION_TYPE
		   && TREE_CODE (ttl) != METHOD_TYPE
		   && TREE_CODE (ttl) != OFFSET_TYPE)
	    return -1;
	  else if (TREE_CODE (ttl) == POINTER_TYPE
		   || TREE_CODE (ttl) == ARRAY_TYPE)
	    {
	      if (comp_ptr_ttypes (ttl, ttr))
		return 1;
	      else if (comp_ptr_ttypes (ttr, ttl))
		return -1;
	      return 0;
	    }
	}

      /* Const and volatile mean something different for function types,
	 so the usual checks are not appropriate.  */
      if (TREE_CODE (ttl) == FUNCTION_TYPE || TREE_CODE (ttl) == METHOD_TYPE)
	return comp_target_types (ttl, ttr, nptrs - 1);

      return comp_cv_target_types (ttl, ttr, nptrs - 1);
    }

  if (TREE_CODE (ttr) == ARRAY_TYPE)
    return comp_array_types (comp_target_types, ttl, ttr, COMPARE_STRICT);
  else if (TREE_CODE (ttr) == FUNCTION_TYPE || TREE_CODE (ttr) == METHOD_TYPE)
    {
      tree argsl, argsr;
      int saw_contra = 0;

      if (pedantic)
	{
	  if (!same_type_p (TREE_TYPE (ttl), TREE_TYPE (ttr)))
	    return 0;
	}
      else
	{
	  switch (comp_target_types (TREE_TYPE (ttl), TREE_TYPE (ttr), -1))
	    {
	    case 0:
	      return 0;
	    case -1:
	      saw_contra = 1;
	    }
	}

      argsl = TYPE_ARG_TYPES (ttl);
      argsr = TYPE_ARG_TYPES (ttr);

      /* Compare 'this' here, not in comp_target_parms.  */
      if (TREE_CODE (ttr) == METHOD_TYPE)
	{
	  tree tl = TYPE_METHOD_BASETYPE (ttl);
	  tree tr = TYPE_METHOD_BASETYPE (ttr);

	  if (!same_or_base_type_p (tr, tl))
	    {
	      if (same_or_base_type_p (tl, tr))
		saw_contra = 1;
	      else
		return 0;
	    }

	  argsl = TREE_CHAIN (argsl);
	  argsr = TREE_CHAIN (argsr);
	}

	switch (comp_target_parms (argsl, argsr))
	  {
	  case 0:
	    return 0;
	  case -1:
	    saw_contra = 1;
	  }

	return saw_contra ? -1 : 1;
    }
  /* for C++ */
  else if (TREE_CODE (ttr) == OFFSET_TYPE)
    {
      int base;

      /* Contravariance: we can assign a pointer to base member to a pointer
	 to derived member.  Note difference from simple pointer case, where
	 we can pass a pointer to derived to a pointer to base.  */
      if (same_or_base_type_p (TYPE_OFFSET_BASETYPE (ttr),
			       TYPE_OFFSET_BASETYPE (ttl)))
	base = 1;
      else if (same_or_base_type_p (TYPE_OFFSET_BASETYPE (ttl),
				    TYPE_OFFSET_BASETYPE (ttr)))
	{
	  tree tmp = ttl;
	  ttl = ttr;
	  ttr = tmp;
	  base = -1;
	}
      else
	return 0;

      ttl = TREE_TYPE (ttl);
      ttr = TREE_TYPE (ttr);

      if (TREE_CODE (ttl) == POINTER_TYPE
	  || TREE_CODE (ttl) == ARRAY_TYPE)
	{
	  if (comp_ptr_ttypes (ttl, ttr))
	    return base;
	  return 0;
	}
      else
	{
	  if (comp_cv_target_types (ttl, ttr, nptrs) == 1)
	    return base;
	  return 0;
	}
    }
  else if (IS_AGGR_TYPE (ttl))
    {
      if (nptrs < 0)
	return 0;
      if (same_or_base_type_p (build_pointer_type (ttl), 
			       build_pointer_type (ttr)))
	return 1;
      if (same_or_base_type_p (build_pointer_type (ttr), 
			       build_pointer_type (ttl)))
	return -1;
      return 0;
    }

  return 0;
}

/* Returns 1 if TYPE1 is at least as qualified as TYPE2.  */

int
at_least_as_qualified_p (type1, type2)
     tree type1;
     tree type2;
{
  /* All qualifiers for TYPE2 must also appear in TYPE1.  */
  return ((cp_type_quals (type1) & cp_type_quals (type2))
	  == cp_type_quals (type2));
}

/* Returns 1 if TYPE1 is more qualified than TYPE2.  */

int
more_qualified_p (type1, type2)
     tree type1;
     tree type2;
{
  return (cp_type_quals (type1) != cp_type_quals (type2)
	  && at_least_as_qualified_p (type1, type2));
}

/* Returns 1 if TYPE1 is more cv-qualified than TYPE2, -1 if TYPE2 is
   more cv-qualified that TYPE1, and 0 otherwise.  */

int
comp_cv_qualification (type1, type2)
     tree type1;
     tree type2;
{
  if (cp_type_quals (type1) == cp_type_quals (type2))
    return 0;

  if (at_least_as_qualified_p (type1, type2))
    return 1;

  else if (at_least_as_qualified_p (type2, type1))
    return -1;

  return 0;
}

/* Returns 1 if the cv-qualification signature of TYPE1 is a proper
   subset of the cv-qualification signature of TYPE2, and the types
   are similar.  Returns -1 if the other way 'round, and 0 otherwise.  */

int
comp_cv_qual_signature (type1, type2)
     tree type1;
     tree type2;
{
  if (comp_ptr_ttypes_real (type2, type1, -1))
    return 1;
  else if (comp_ptr_ttypes_real (type1, type2, -1))
    return -1;
  else
    return 0;
}

/* If two types share a common base type, return that basetype.
   If there is not a unique most-derived base type, this function
   returns ERROR_MARK_NODE.  */

static tree
common_base_type (tt1, tt2)
     tree tt1, tt2;
{
  tree best = NULL_TREE;
  int i;

  /* If one is a baseclass of another, that's good enough.  */
  if (UNIQUELY_DERIVED_FROM_P (tt1, tt2))
    return tt1;
  if (UNIQUELY_DERIVED_FROM_P (tt2, tt1))
    return tt2;

  /* Otherwise, try to find a unique baseclass of TT1
     that is shared by TT2, and follow that down.  */
  for (i = CLASSTYPE_N_BASECLASSES (tt1)-1; i >= 0; i--)
    {
      tree basetype = TYPE_BINFO_BASETYPE (tt1, i);
      tree trial = common_base_type (basetype, tt2);
      if (trial)
	{
	  if (trial == error_mark_node)
	    return trial;
	  if (best == NULL_TREE)
	    best = trial;
	  else if (best != trial)
	    return error_mark_node;
	}
    }

  /* Same for TT2.  */
  for (i = CLASSTYPE_N_BASECLASSES (tt2)-1; i >= 0; i--)
    {
      tree basetype = TYPE_BINFO_BASETYPE (tt2, i);
      tree trial = common_base_type (tt1, basetype);
      if (trial)
	{
	  if (trial == error_mark_node)
	    return trial;
	  if (best == NULL_TREE)
	    best = trial;
	  else if (best != trial)
	    return error_mark_node;
	}
    }
  return best;
}

/* Subroutines of `comptypes'.  */

/* Return 1 if two parameter type lists PARMS1 and PARMS2 are
   equivalent in the sense that functions with those parameter types
   can have equivalent types.  The two lists must be equivalent,
   element by element.

   C++: See comment above about TYPE1, TYPE2.  */

int
compparms (parms1, parms2)
     tree parms1, parms2;
{
  register tree t1 = parms1, t2 = parms2;

  /* An unspecified parmlist matches any specified parmlist
     whose argument types don't need default promotions.  */

  while (1)
    {
      if (t1 == 0 && t2 == 0)
	return 1;
      /* If one parmlist is shorter than the other,
	 they fail to match.  */
      if (t1 == 0 || t2 == 0)
	return 0;
      if (!same_type_p (TREE_VALUE (t2), TREE_VALUE (t1)))
	return 0;

      t1 = TREE_CHAIN (t1);
      t2 = TREE_CHAIN (t2);
    }
}

/* This really wants return whether or not parameter type lists
   would make their owning functions assignment compatible or not.

   The return value is like for comp_target_types.

   This should go away, possibly with the exception of the empty parmlist
   conversion; there are no conversions between function types in C++.
   (jason 17 Apr 1997)  */

static int
comp_target_parms (parms1, parms2)
     tree parms1, parms2;
{
  register tree t1 = parms1, t2 = parms2;
  int warn_contravariance = 0;

  /* In C, an unspecified parmlist matches any specified parmlist
     whose argument types don't need default promotions.  This is not
     true for C++, but let's do it anyway for unfixed headers.  */

  if (t1 == 0 && t2 != 0)
    {
      pedwarn ("ISO C++ prohibits conversion from `%#T' to `(...)'",
		  parms2);
      return self_promoting_args_p (t2);
    }
  if (t2 == 0)
    return self_promoting_args_p (t1);

  for (; t1 || t2; t1 = TREE_CHAIN (t1), t2 = TREE_CHAIN (t2))
    {
      tree p1, p2;

      /* If one parmlist is shorter than the other,
	 they fail to match, unless STRICT is <= 0.  */
      if (t1 == 0 || t2 == 0)
	return 0;
      p1 = TREE_VALUE (t1);
      p2 = TREE_VALUE (t2);
      if (same_type_p (p1, p2))
	continue;

      if (pedantic)
	return 0;

      if ((TREE_CODE (p1) == POINTER_TYPE && TREE_CODE (p2) == POINTER_TYPE)
	  || (TREE_CODE (p1) == REFERENCE_TYPE
	      && TREE_CODE (p2) == REFERENCE_TYPE))
	{
	  /* The following is wrong for contravariance,
	     but many programs depend on it.  */
	  if (TREE_TYPE (p1) == void_type_node)
	    continue;
	  if (TREE_TYPE (p2) == void_type_node)
	    {
	      warn_contravariance = 1;
	      continue;
	    }
	  if (IS_AGGR_TYPE (TREE_TYPE (p1))
	      && !same_type_ignoring_top_level_qualifiers_p (TREE_TYPE (p1),
							     TREE_TYPE (p2)))
	    return 0;
	}
      /* Note backwards order due to contravariance.  */
      if (comp_target_types (p2, p1, 1) <= 0)
	{
	  if (comp_target_types (p1, p2, 1) > 0)
	    {
	      warn_contravariance = 1;
	      continue;
	    }
	  return 0;
	}
    }
  return warn_contravariance ? -1 : 1;
}

tree
cxx_sizeof_or_alignof_type (type, op, complain)
     tree type;
     enum tree_code op;
     int complain;
{
  enum tree_code type_code;
  tree value;
  const char *op_name;

  my_friendly_assert (op == SIZEOF_EXPR || op == ALIGNOF_EXPR, 20020720);
  if (processing_template_decl)
    return build_min_nt (op, type);
  
  op_name = operator_name_info[(int) op].name;
  
  if (TREE_CODE (type) == REFERENCE_TYPE)
    type = TREE_TYPE (type);
  type_code = TREE_CODE (type);

  if (type_code == METHOD_TYPE)
    {
      if (complain && (pedantic || warn_pointer_arith))
	pedwarn ("invalid application of `%s' to a member function", op_name);
      value = size_one_node;
    }
  else if (type_code == OFFSET_TYPE)
    {
      if (complain)
	error ("invalid application of `%s' to non-static member", op_name);
      value = size_zero_node;
    }
  else
    value = c_sizeof_or_alignof_type (complete_type (type), op, complain);

  return value;
}

tree
expr_sizeof (e)
     tree e;
{
  if (processing_template_decl)
    return build_min_nt (SIZEOF_EXPR, e);

  if (TREE_CODE (e) == COMPONENT_REF
      && DECL_C_BIT_FIELD (TREE_OPERAND (e, 1)))
    error ("sizeof applied to a bit-field");
  if (is_overloaded_fn (e))
    {
      pedwarn ("ISO C++ forbids applying `sizeof' to an expression of function type");
      return c_sizeof (char_type_node);
    }
  else if (type_unknown_p (e))
    {
      cxx_incomplete_type_error (e, TREE_TYPE (e));
      return c_sizeof (char_type_node);
    }
  /* It's invalid to say `sizeof (X::i)' for `i' a non-static data
     member unless you're in a non-static member of X.  So hand off to
     resolve_offset_ref.  [expr.prim]  */
  else if (TREE_CODE (e) == OFFSET_REF)
    e = resolve_offset_ref (e);

  if (e == error_mark_node)
    return e;

  return cxx_sizeof (TREE_TYPE (e));
}
  

/* Perform the array-to-pointer and function-to-pointer conversions
   for EXP.  

   In addition, references are converted to lvalues and manifest
   constants are replaced by their values.  */

tree
decay_conversion (exp)
     tree exp;
{
  register tree type;
  register enum tree_code code;

  if (TREE_CODE (exp) == OFFSET_REF)
    exp = resolve_offset_ref (exp);

  type = TREE_TYPE (exp);
  code = TREE_CODE (type);

  if (code == REFERENCE_TYPE)
    {
      exp = convert_from_reference (exp);
      type = TREE_TYPE (exp);
      code = TREE_CODE (type);
    }

  if (type == error_mark_node)
    return error_mark_node;

  if (type_unknown_p (exp))
    {
      cxx_incomplete_type_error (exp, TREE_TYPE (exp));
      return error_mark_node;
    }
  
  /* Constants can be used directly unless they're not loadable.  */
  if (TREE_CODE (exp) == CONST_DECL)
    exp = DECL_INITIAL (exp);
  /* Replace a nonvolatile const static variable with its value.  We
     don't do this for arrays, though; we want the address of the
     first element of the array, not the address of the first element
     of its initializing constant.  */
  else if (code != ARRAY_TYPE)
    {
      exp = decl_constant_value (exp);
      type = TREE_TYPE (exp);
    }

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Leave such NOP_EXPRs, since RHS is being used in non-lvalue context.  */

  if (code == VOID_TYPE)
    {
      error ("void value not ignored as it ought to be");
      return error_mark_node;
    }
  if (code == METHOD_TYPE)
    abort ();
  if (code == FUNCTION_TYPE || is_overloaded_fn (exp))
    return build_unary_op (ADDR_EXPR, exp, 0);
  if (code == ARRAY_TYPE)
    {
      register tree adr;
      tree ptrtype;

      if (TREE_CODE (exp) == INDIRECT_REF)
	{
	  /* Stripping away the INDIRECT_REF is not the right
	     thing to do for references...  */
	  tree inner = TREE_OPERAND (exp, 0);
	  if (TREE_CODE (TREE_TYPE (inner)) == REFERENCE_TYPE)
	    {
	      inner = build1 (CONVERT_EXPR,
			      build_pointer_type (TREE_TYPE
						  (TREE_TYPE (inner))),
			      inner);
	      TREE_CONSTANT (inner) = TREE_CONSTANT (TREE_OPERAND (inner, 0));
	    }
	  return cp_convert (build_pointer_type (TREE_TYPE (type)), inner);
	}

      if (TREE_CODE (exp) == COMPOUND_EXPR)
	{
	  tree op1 = decay_conversion (TREE_OPERAND (exp, 1));
	  return build (COMPOUND_EXPR, TREE_TYPE (op1),
			TREE_OPERAND (exp, 0), op1);
	}

      if (!lvalue_p (exp)
	  && ! (TREE_CODE (exp) == CONSTRUCTOR && TREE_STATIC (exp)))
	{
	  error ("invalid use of non-lvalue array");
	  return error_mark_node;
	}

      ptrtype = build_pointer_type (TREE_TYPE (type));

      if (TREE_CODE (exp) == VAR_DECL)
	{
	  /* ??? This is not really quite correct
	     in that the type of the operand of ADDR_EXPR
	     is not the target type of the type of the ADDR_EXPR itself.
	     Question is, can this lossage be avoided?  */
	  adr = build1 (ADDR_EXPR, ptrtype, exp);
	  if (!cxx_mark_addressable (exp))
	    return error_mark_node;
	  TREE_CONSTANT (adr) = staticp (exp);
	  TREE_SIDE_EFFECTS (adr) = 0;   /* Default would be, same as EXP.  */
	  return adr;
	}
      /* This way is better for a COMPONENT_REF since it can
	 simplify the offset for a component.  */
      adr = build_unary_op (ADDR_EXPR, exp, 1);
      return cp_convert (ptrtype, adr);
    }

  /* [basic.lval]: Class rvalues can have cv-qualified types; non-class
     rvalues always have cv-unqualified types.  */
  if (! CLASS_TYPE_P (type))
    exp = cp_convert (TYPE_MAIN_VARIANT (type), exp);

  return exp;
}

tree
default_conversion (exp)
     tree exp;
{
  tree type;
  enum tree_code code;

  exp = decay_conversion (exp);

  type = TREE_TYPE (exp);
  code = TREE_CODE (type);

  if (INTEGRAL_CODE_P (code))
    {
      tree t = type_promotes_to (type);
      if (t != type)
	return cp_convert (t, exp);
    }

  return exp;
}

/* Take the address of an inline function without setting TREE_ADDRESSABLE
   or TREE_USED.  */

tree
inline_conversion (exp)
     tree exp;
{
  if (TREE_CODE (exp) == FUNCTION_DECL)
    exp = build1 (ADDR_EXPR, build_pointer_type (TREE_TYPE (exp)), exp);

  return exp;
}

/* Returns nonzero iff exp is a STRING_CST or the result of applying
   decay_conversion to one.  */

int
string_conv_p (totype, exp, warn)
     tree totype, exp;
     int warn;
{
  tree t;

  if (! flag_const_strings || TREE_CODE (totype) != POINTER_TYPE)
    return 0;

  t = TREE_TYPE (totype);
  if (!same_type_p (t, char_type_node)
      && !same_type_p (t, wchar_type_node))
    return 0;

  if (TREE_CODE (exp) == STRING_CST)
    {
      /* Make sure that we don't try to convert between char and wchar_t.  */
      if (!same_type_p (TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (exp))), t))
	return 0;
    }
  else
    {
      /* Is this a string constant which has decayed to 'const char *'?  */
      t = build_pointer_type (build_qualified_type (t, TYPE_QUAL_CONST));
      if (!same_type_p (TREE_TYPE (exp), t))
	return 0;
      STRIP_NOPS (exp);
      if (TREE_CODE (exp) != ADDR_EXPR
	  || TREE_CODE (TREE_OPERAND (exp, 0)) != STRING_CST)
	return 0;
    }

  /* This warning is not very useful, as it complains about printf.  */
  if (warn && warn_write_strings)
    warning ("deprecated conversion from string constant to `%T'", totype);

  return 1;
}

/* Given a COND_EXPR, MIN_EXPR, or MAX_EXPR in T, return it in a form that we
   can, for example, use as an lvalue.  This code used to be in
   unary_complex_lvalue, but we needed it to deal with `a = (d == c) ? b : c'
   expressions, where we're dealing with aggregates.  But now it's again only
   called from unary_complex_lvalue.  The case (in particular) that led to
   this was with CODE == ADDR_EXPR, since it's not an lvalue when we'd
   get it there.  */

static tree
rationalize_conditional_expr (code, t)
     enum tree_code code;
     tree t;
{
  /* For MIN_EXPR or MAX_EXPR, fold-const.c has arranged things so that
     the first operand is always the one to be used if both operands
     are equal, so we know what conditional expression this used to be.  */
  if (TREE_CODE (t) == MIN_EXPR || TREE_CODE (t) == MAX_EXPR)
    {
      return
	build_conditional_expr (build_x_binary_op ((TREE_CODE (t) == MIN_EXPR
						    ? LE_EXPR : GE_EXPR),
						   TREE_OPERAND (t, 0),
						   TREE_OPERAND (t, 1)),
			    build_unary_op (code, TREE_OPERAND (t, 0), 0),
			    build_unary_op (code, TREE_OPERAND (t, 1), 0));
    }

  return
    build_conditional_expr (TREE_OPERAND (t, 0),
			    build_unary_op (code, TREE_OPERAND (t, 1), 0),
			    build_unary_op (code, TREE_OPERAND (t, 2), 0));
}

/* Given the TYPE of an anonymous union field inside T, return the
   FIELD_DECL for the field.  If not found return NULL_TREE.  Because
   anonymous unions can nest, we must also search all anonymous unions
   that are directly reachable.  */

static tree
lookup_anon_field (t, type)
     tree t, type;
{
  tree field;

  for (field = TYPE_FIELDS (t); field; field = TREE_CHAIN (field))
    {
      if (TREE_STATIC (field))
	continue;
      if (TREE_CODE (field) != FIELD_DECL || DECL_ARTIFICIAL (field))
	continue;

      /* If we find it directly, return the field.  */
      if (DECL_NAME (field) == NULL_TREE
	  && type == TYPE_MAIN_VARIANT (TREE_TYPE (field)))
	{
	  return field;
	}

      /* Otherwise, it could be nested, search harder.  */
      if (DECL_NAME (field) == NULL_TREE
	  && ANON_AGGR_TYPE_P (TREE_TYPE (field)))
	{
	  tree subfield = lookup_anon_field (TREE_TYPE (field), type);
	  if (subfield)
	    return subfield;
	}
    }
  return NULL_TREE;
}

/* Build an expression representing OBJECT.MEMBER.  OBJECT is an
   expression; MEMBER is a DECL or baselink.  If ACCESS_PATH is
   non-NULL, it indicates the path to the base used to name MEMBER.
   If PRESERVE_REFERENCE is true, the expression returned will have
   REFERENCE_TYPE if the MEMBER does.  Otherwise, the expression
   returned will have the type referred to by the reference. 

   This function does not perform access control; that is either done
   earlier by the parser when the name of MEMBER is resolved to MEMBER
   itself, or later when overload resolution selects one of the
   functions indicated by MEMBER.  */

tree
build_class_member_access_expr (tree object, tree member, 
				tree access_path, bool preserve_reference)
{
  tree object_type;
  tree member_scope;
  tree result = NULL_TREE;

  if (object == error_mark_node || member == error_mark_node)
    return error_mark_node;

  my_friendly_assert (DECL_P (member) || BASELINK_P (member),
		      20020801);

  /* Transform `(a, b).x' into `a, b.x' and `(a ? b : c).x' into 
     `a ? b.x : c.x'.  These transformations should not really be
     necessary, but they are.  */
  if (TREE_CODE (object) == COMPOUND_EXPR)
    {
      result = build_class_member_access_expr (TREE_OPERAND (object, 1),
					       member, access_path, 
					       preserve_reference);
      return build (COMPOUND_EXPR, TREE_TYPE (result), 
		    TREE_OPERAND (object, 0), result);
    }
  else if (TREE_CODE (object) == COND_EXPR)
    return (build_conditional_expr
	    (TREE_OPERAND (object, 0),
	     build_class_member_access_expr (TREE_OPERAND (object, 1),
					     member, access_path,
					     preserve_reference),
	     build_class_member_access_expr (TREE_OPERAND (object, 2),
					     member, access_path,
					     preserve_reference)));

  /* [expr.ref]

     The type of the first expression shall be "class object" (of a
     complete type).  */
  object_type = TREE_TYPE (object);
  if (!complete_type_or_else (object_type, object))
    return error_mark_node;
  if (!CLASS_TYPE_P (object_type))
    {
      error ("request for member `%D' in `%E', which is of non-class type `%T'", 
	     member, object, object_type);
      return error_mark_node;
    }

  /* The standard does not seem to actually say that MEMBER must be a
     member of OBJECT_TYPE.  However, that is clearly what is
     intended.  */
  if (DECL_P (member))
    {
      member_scope = DECL_CLASS_CONTEXT (member);
      mark_used (member);
      if (TREE_DEPRECATED (member))
	warn_deprecated_use (member);
    }
  else
    member_scope = BINFO_TYPE (BASELINK_BINFO (member));
  /* If MEMBER is from an anonymous aggregate, MEMBER_SCOPE will
     presently be the anonymous union.  Go outwards until we find a
     type related to OBJECT_TYPE.  */
  while (ANON_AGGR_TYPE_P (member_scope)
	 && !same_type_ignoring_top_level_qualifiers_p (member_scope,
							object_type))
    member_scope = TYPE_CONTEXT (member_scope);
  if (!member_scope || !DERIVED_FROM_P (member_scope, object_type))
    {
      error ("`%D' is not a member of `%T'", member, object_type);
      return error_mark_node;
    }

  /* In [expr.ref], there is an explicit list of the valid choices for
     MEMBER.  We check for each of those cases here.  */
  if (TREE_CODE (member) == VAR_DECL)
    {
      /* A static data member.  */
      result = member;
      /* If OBJECT has side-effects, they are supposed to occur.  */
      if (TREE_SIDE_EFFECTS (object))
	result = build (COMPOUND_EXPR, TREE_TYPE (result), object, result);
    }
  else if (TREE_CODE (member) == FIELD_DECL)
    {
      /* A non-static data member.  */
      bool null_object_p;
      int type_quals;
      tree member_type;

      null_object_p = (TREE_CODE (object) == INDIRECT_REF
		       && integer_zerop (TREE_OPERAND (object, 0)));

      /* Convert OBJECT to the type of MEMBER.  */
      if (!same_type_p (TYPE_MAIN_VARIANT (object_type),
			TYPE_MAIN_VARIANT (member_scope)))
	{
	  tree binfo;
	  base_kind kind;

	  binfo = lookup_base (access_path ? access_path : object_type,
			       member_scope, ba_ignore,  &kind);
	  if (binfo == error_mark_node)
	    return error_mark_node;

	  /* It is invalid to try to get to a virtual base of a
	     NULL object.  The most common cause is invalid use of
	     offsetof macro.  */
	  if (null_object_p && kind == bk_via_virtual)
	    {
	      error ("invalid access to non-static data member `%D' of NULL object",
		     member);
	      error ("(perhaps the `offsetof' macro was used incorrectly)");
	      return error_mark_node;
	    }

	  /* Convert to the base.  */
	  object = build_base_path (PLUS_EXPR, object, binfo, 
				    /*nonnull=*/1);
	  /* If we found the base successfully then we should be able
	     to convert to it successfully.  */
	  my_friendly_assert (object != error_mark_node,
			      20020801);
	}

      /* Complain about other invalid uses of offsetof, even though they will
	 give the right answer.  Note that we complain whether or not they
	 actually used the offsetof macro, since there's no way to know at this
	 point.  So we just give a warning, instead of a pedwarn.  */
      if (null_object_p && CLASSTYPE_NON_POD_P (object_type))
	{
	  warning ("invalid access to non-static data member `%D' of NULL object", 
		   member);
	  warning  ("(perhaps the `offsetof' macro was used incorrectly)");
	}

      /* If MEMBER is from an anonymous aggregate, we have converted
	 OBJECT so that it refers to the class containing the
	 anonymous union.  Generate a reference to the anonymous union
	 itself, and recur to find MEMBER.  */
      if (ANON_AGGR_TYPE_P (DECL_CONTEXT (member))
	  /* When this code is called from build_field_call, the
	     object already has the type of the anonymous union.
	     That is because the COMPONENT_REF was already
	     constructed, and was then disassembled before calling
	     build_field_call.  After the function-call code is
	     cleaned up, this waste can be eliminated.  */
	  && (!same_type_ignoring_top_level_qualifiers_p 
	      (TREE_TYPE (object), DECL_CONTEXT (member))))
	{
	  tree anonymous_union;

	  anonymous_union = lookup_anon_field (TREE_TYPE (object),
					       DECL_CONTEXT (member));
	  object = build_class_member_access_expr (object,
						   anonymous_union,
						   /*access_path=*/NULL_TREE,
						   preserve_reference);
	}

      /* Compute the type of the field, as described in [expr.ref].  */
      type_quals = TYPE_UNQUALIFIED;
      member_type = TREE_TYPE (member);
      if (TREE_CODE (member_type) != REFERENCE_TYPE)
	{
	  type_quals = (cp_type_quals (member_type)  
			| cp_type_quals (object_type));
	  
	  /* A field is const (volatile) if the enclosing object, or the
	     field itself, is const (volatile).  But, a mutable field is
	     not const, even within a const object.  */
	  if (DECL_MUTABLE_P (member))
	    type_quals &= ~TYPE_QUAL_CONST;
	  member_type = cp_build_qualified_type (member_type, type_quals);
	}

      result = fold (build (COMPONENT_REF, member_type, object, member));

      /* Mark the expression const or volatile, as appropriate.  Even
	 though we've dealt with the type above, we still have to mark the
	 expression itself.  */
      if (type_quals & TYPE_QUAL_CONST)
	TREE_READONLY (result) = 1;
      else if (type_quals & TYPE_QUAL_VOLATILE)
	TREE_THIS_VOLATILE (result) = 1;
    }
  else if (BASELINK_P (member))
    {
      /* The member is a (possibly overloaded) member function.  */
      tree functions;

      /* If the MEMBER is exactly one static member function, then we
	 know the type of the expression.  Otherwise, we must wait
	 until overload resolution has been performed.  */
      functions = BASELINK_FUNCTIONS (member);
      if (TREE_CODE (functions) == FUNCTION_DECL
	  && DECL_STATIC_FUNCTION_P (functions))
	{
	  /* A static member function.  */
	  result = functions;
	  mark_used (result);
	  /* If OBJECT has side-effects, they are supposed to occur.  */
	  if (TREE_SIDE_EFFECTS (object))
	    result = build (COMPOUND_EXPR, TREE_TYPE (result),
			    object, result);
	}
      else
	/* Note that we do not convert OBJECT to the BASELINK_BINFO
	   base.  That will happen when the function is called.  */
	result = build (COMPONENT_REF, unknown_type_node, object, member);
    }
  else if (TREE_CODE (member) == CONST_DECL)
    {
      /* The member is an enumerator.  */
      result = member;
      /* If OBJECT has side-effects, they are supposed to occur.  */
      if (TREE_SIDE_EFFECTS (object))
	result = build (COMPOUND_EXPR, TREE_TYPE (result),
			object, result);
    }
  else
    {
      error ("invalid use of `%D'", member);
      return error_mark_node;
    }

  if (!preserve_reference)
    /* [expr.ref]
       
       If E2 is declared to have type "reference to T", then ... the
       type of E1.E2 is T.  */
    result = convert_from_reference (result);

  return result;
}

/* This function is called by the parser to process a class member
   access expression of the form OBJECT.NAME.  NAME is a node used by
   the parser to represent a name; it is not yet a DECL.  It may,
   however, be a BASELINK where the BASELINK_FUNCTIONS is a
   TEMPLATE_ID_EXPR.  Templates must be looked up by the parser, and
   there is no reason to do the lookup twice, so the parser keeps the
   BASELINK.  */

tree
finish_class_member_access_expr (tree object, tree name)
{
  tree object_type;
  tree member;
  tree access_path = NULL_TREE;

  if (object == error_mark_node || name == error_mark_node)
    return error_mark_node;

  if (processing_template_decl)
    return build_min_nt (COMPONENT_REF, object, name);
  
  if (TREE_CODE (object) == OFFSET_REF)
    object = resolve_offset_ref (object);

  object_type = TREE_TYPE (object);
  if (TREE_CODE (object_type) == REFERENCE_TYPE)
    {
      object = convert_from_reference (object);
      object_type = TREE_TYPE (object);
    }

  /* [expr.ref]

     The type of the first expression shall be "class object" (of a
     complete type).  */
  if (!complete_type_or_else (object_type, object))
    return error_mark_node;
  if (!CLASS_TYPE_P (object_type))
    {
      error ("request for member `%D' in `%E', which is of non-class type `%T'", 
	     name, object, object_type);
      return error_mark_node;
    }

  if (BASELINK_P (name))
    {
      /* A member function that has already been looked up.  */
      my_friendly_assert ((TREE_CODE (BASELINK_FUNCTIONS (name)) 
			   == TEMPLATE_ID_EXPR), 
			  20020805);
      member = name;
    }
  else
    {
      bool is_template_id = false;
      tree template_args = NULL_TREE;

      if (TREE_CODE (name) == TEMPLATE_ID_EXPR)
	{
	  is_template_id = true;
	  template_args = TREE_OPERAND (name, 1);
	  name = TREE_OPERAND (name, 0);
	}

      if (TREE_CODE (name) == SCOPE_REF)
	{
	  tree scope;

	  /* A qualified name.  The qualifying class or namespace `S' has
	     already been looked up; it is either a TYPE or a
	     NAMESPACE_DECL.  The member name is either an IDENTIFIER_NODE
	     or a BIT_NOT_EXPR.  */
	  scope = TREE_OPERAND (name, 0);
	  name = TREE_OPERAND (name, 1);
	  my_friendly_assert ((CLASS_TYPE_P (scope) 
			       || TREE_CODE (scope) == NAMESPACE_DECL),
			      20020804);
	  my_friendly_assert ((TREE_CODE (name) == IDENTIFIER_NODE
			       || TREE_CODE (name) == BIT_NOT_EXPR),
			      20020804);

	  /* If SCOPE is a namespace, then the qualified name does not
	     name a member of OBJECT_TYPE.  */
	  if (TREE_CODE (scope) == NAMESPACE_DECL)
	    {
	      error ("`%D::%D' is not a member of `%T'", 
		     scope, name, object_type);
	      return error_mark_node;
	    }

	  /* Find the base of OBJECT_TYPE corresponding to SCOPE.  */
	  access_path = lookup_base (object_type, scope, ba_check, NULL);
	  if (!access_path || access_path == error_mark_node)
	    return error_mark_node;

	  /* Look up the member.  */
	  member = lookup_member (access_path, name, /*protect=*/1, 
				  /*want_type=*/0);
	  if (member == NULL_TREE)
	    {
	      error ("'%D' has no member named '%E'", object_type, name);
	      return error_mark_node;
	    }
	  else if (member == error_mark_node)
	    return error_mark_node;
	}
      else if (TREE_CODE (name) == BIT_NOT_EXPR)
	{
	  /* A destructor.  */
	  if (TYPE_IDENTIFIER (object_type) != TREE_OPERAND (name, 0))
	    {
	      error ("destructor specifier `%T::~%T' must have matching names",
		     object_type, TREE_OPERAND (name, 0));
	      return error_mark_node;
	    }
	  if (! TYPE_HAS_DESTRUCTOR (object_type))
	    {
	      error ("type `%T' has no destructor", object_type);
	      return error_mark_node;
	    }
	  member = CLASSTYPE_DESTRUCTORS (object_type);
	}
      else if (TREE_CODE (name) == IDENTIFIER_NODE)
	{
	  /* An unqualified name.  */
	  member = lookup_member (object_type, name, /*protect=*/1, 
				  /*want_type=*/0);
	  if (member == NULL_TREE)
	    {
	      error ("'%D' has no member named '%E'", object_type, name);
	      return error_mark_node;
	    }
	  else if (member == error_mark_node)
	    return error_mark_node;
	}
      else
	{
	  /* The YACC parser sometimes gives us things that are not names.
	     These always indicate errors.  The recursive-descent parser
	     does not do this, so this code can go away once that parser
	     replaces the YACC parser.  */
	  error ("invalid use of `%D'", name);
	  return error_mark_node;
	}
      
      if (is_template_id)
	{
	  tree template = member;
	  
	  if (BASELINK_P (template))
	    BASELINK_FUNCTIONS (template) 
	      = build_nt (TEMPLATE_ID_EXPR,
			  BASELINK_FUNCTIONS (template),
			  template_args);
	  else
	    {
	      error ("`%D' is not a member template function", name);
	      return error_mark_node;
	    }
	}
    }

  return build_class_member_access_expr (object, member, access_path,
					 /*preserve_reference=*/false);
}

/* Return an expression for the MEMBER_NAME field in the internal
   representation of PTRMEM, a pointer-to-member function.  (Each
   pointer-to-member function type gets its own RECORD_TYPE so it is
   more convenient to access the fields by name than by FIELD_DECL.)
   This routine converts the NAME to a FIELD_DECL and then creates the
   node for the complete expression.  */

tree
build_ptrmemfunc_access_expr (tree ptrmem, tree member_name)
{
  tree ptrmem_type;
  tree member;
  tree member_type;

  /* This code is a stripped down version of
     build_class_member_access_expr.  It does not work to use that
     routine directly because it expects the object to be of class
     type.  */
  ptrmem_type = TREE_TYPE (ptrmem);
  my_friendly_assert (TYPE_PTRMEMFUNC_P (ptrmem_type), 20020804);
  member = lookup_member (ptrmem_type, member_name, /*protect=*/0,
			  /*want_type=*/0);
  member_type = cp_build_qualified_type (TREE_TYPE (member),
					 cp_type_quals (ptrmem_type));
  return fold (build (COMPONENT_REF, member_type, ptrmem, member));
}

/* Given an expression PTR for a pointer, return an expression
   for the value pointed to.
   ERRORSTRING is the name of the operator to appear in error messages.

   This function may need to overload OPERATOR_FNNAME.
   Must also handle REFERENCE_TYPEs for C++.  */

tree
build_x_indirect_ref (ptr, errorstring)
     tree ptr;
     const char *errorstring;
{
  tree rval;

  if (processing_template_decl)
    return build_min_nt (INDIRECT_REF, ptr);

  rval = build_opfncall (INDIRECT_REF, LOOKUP_NORMAL, ptr, NULL_TREE,
			 NULL_TREE);
  if (rval)
    return rval;
  return build_indirect_ref (ptr, errorstring);
}

tree
build_indirect_ref (ptr, errorstring)
     tree ptr;
     const char *errorstring;
{
  register tree pointer, type;

  if (ptr == error_mark_node)
    return error_mark_node;

  if (ptr == current_class_ptr)
    return current_class_ref;

  pointer = (TREE_CODE (TREE_TYPE (ptr)) == REFERENCE_TYPE
	     ? ptr : default_conversion (ptr));
  type = TREE_TYPE (pointer);

  if (TYPE_PTR_P (type) || TREE_CODE (type) == REFERENCE_TYPE)
    {
      /* [expr.unary.op]
	 
	 If the type of the expression is "pointer to T," the type
	 of  the  result  is  "T."   

         We must use the canonical variant because certain parts of
	 the back end, like fold, do pointer comparisons between
	 types.  */
      tree t = canonical_type_variant (TREE_TYPE (type));

      if (VOID_TYPE_P (t))
        {
          /* A pointer to incomplete type (other than cv void) can be
             dereferenced [expr.unary.op]/1  */
          error ("`%T' is not a pointer-to-object type", type);
          return error_mark_node;
        }
      else if (TREE_CODE (pointer) == ADDR_EXPR
	       && !flag_volatile
	       && same_type_p (t, TREE_TYPE (TREE_OPERAND (pointer, 0))))
	/* The POINTER was something like `&x'.  We simplify `*&x' to
	   `x'.  */
	return TREE_OPERAND (pointer, 0);
      else
	{
	  tree ref = build1 (INDIRECT_REF, t, pointer);

	  /* We *must* set TREE_READONLY when dereferencing a pointer to const,
	     so that we get the proper error message if the result is used
	     to assign to.  Also, &* is supposed to be a no-op.  */
	  TREE_READONLY (ref) = CP_TYPE_CONST_P (t);
	  TREE_THIS_VOLATILE (ref) = CP_TYPE_VOLATILE_P (t);
	  TREE_SIDE_EFFECTS (ref)
	    = (TREE_THIS_VOLATILE (ref) || TREE_SIDE_EFFECTS (pointer)
	       || flag_volatile);
	  return ref;
	}
    }
  /* `pointer' won't be an error_mark_node if we were given a
     pointer to member, so it's cool to check for this here.  */
  else if (TYPE_PTRMEM_P (type) || TYPE_PTRMEMFUNC_P (type))
    error ("invalid use of `%s' on pointer to member", errorstring);
  else if (pointer != error_mark_node)
    {
      if (errorstring)
	error ("invalid type argument of `%s'", errorstring);
      else
	error ("invalid type argument");
    }
  return error_mark_node;
}

/* This handles expressions of the form "a[i]", which denotes
   an array reference.

   This is logically equivalent in C to *(a+i), but we may do it differently.
   If A is a variable or a member, we generate a primitive ARRAY_REF.
   This avoids forcing the array out of registers, and can work on
   arrays that are not lvalues (for example, members of structures returned
   by functions).

   If INDEX is of some user-defined type, it must be converted to
   integer type.  Otherwise, to make a compatible PLUS_EXPR, it
   will inherit the type of the array, which will be some pointer type.  */

tree
build_array_ref (array, idx)
     tree array, idx;
{
  if (idx == 0)
    {
      error ("subscript missing in array reference");
      return error_mark_node;
    }

  if (TREE_TYPE (array) == error_mark_node
      || TREE_TYPE (idx) == error_mark_node)
    return error_mark_node;

  /* If ARRAY is a COMPOUND_EXPR or COND_EXPR, move our reference
     inside it.  */
  switch (TREE_CODE (array))
    {
    case COMPOUND_EXPR:
      {
	tree value = build_array_ref (TREE_OPERAND (array, 1), idx);
	return build (COMPOUND_EXPR, TREE_TYPE (value),
		      TREE_OPERAND (array, 0), value);
      }

    case COND_EXPR:
      return build_conditional_expr
	(TREE_OPERAND (array, 0),
	 build_array_ref (TREE_OPERAND (array, 1), idx),
	 build_array_ref (TREE_OPERAND (array, 2), idx));

    default:
      break;
    }

  if (TREE_CODE (TREE_TYPE (array)) == ARRAY_TYPE
      && TREE_CODE (array) != INDIRECT_REF)
    {
      tree rval, type;

      /* Subscripting with type char is likely to lose
	 on a machine where chars are signed.
	 So warn on any machine, but optionally.
	 Don't warn for unsigned char since that type is safe.
	 Don't warn for signed char because anyone who uses that
	 must have done so deliberately.  */
      if (warn_char_subscripts
	  && TYPE_MAIN_VARIANT (TREE_TYPE (idx)) == char_type_node)
	warning ("array subscript has type `char'");

      /* Apply default promotions *after* noticing character types.  */
      idx = default_conversion (idx);

      if (TREE_CODE (TREE_TYPE (idx)) != INTEGER_TYPE)
	{
	  error ("array subscript is not an integer");
	  return error_mark_node;
	}

      /* An array that is indexed by a non-constant
	 cannot be stored in a register; we must be able to do
	 address arithmetic on its address.
	 Likewise an array of elements of variable size.  */
      if (TREE_CODE (idx) != INTEGER_CST
	  || (COMPLETE_TYPE_P (TREE_TYPE (TREE_TYPE (array)))
	      && (TREE_CODE (TYPE_SIZE (TREE_TYPE (TREE_TYPE (array))))
		  != INTEGER_CST)))
	{
	  if (!cxx_mark_addressable (array))
	    return error_mark_node;
	}

      /* An array that is indexed by a constant value which is not within
	 the array bounds cannot be stored in a register either; because we
	 would get a crash in store_bit_field/extract_bit_field when trying
	 to access a non-existent part of the register.  */
      if (TREE_CODE (idx) == INTEGER_CST
	  && TYPE_VALUES (TREE_TYPE (array))
	  && ! int_fits_type_p (idx, TYPE_VALUES (TREE_TYPE (array))))
	{
	  if (!cxx_mark_addressable (array))
	    return error_mark_node;
	}

      if (pedantic && !lvalue_p (array))
	pedwarn ("ISO C++ forbids subscripting non-lvalue array");

      /* Note in C++ it is valid to subscript a `register' array, since
	 it is valid to take the address of something with that
	 storage specification.  */
      if (extra_warnings)
	{
	  tree foo = array;
	  while (TREE_CODE (foo) == COMPONENT_REF)
	    foo = TREE_OPERAND (foo, 0);
	  if (TREE_CODE (foo) == VAR_DECL && DECL_REGISTER (foo))
	    warning ("subscripting array declared `register'");
	}

      type = TREE_TYPE (TREE_TYPE (array));
      rval = build (ARRAY_REF, type, array, idx);
      /* Array ref is const/volatile if the array elements are
	 or if the array is..  */
      TREE_READONLY (rval)
	|= (CP_TYPE_CONST_P (type) | TREE_READONLY (array));
      TREE_SIDE_EFFECTS (rval)
	|= (CP_TYPE_VOLATILE_P (type) | TREE_SIDE_EFFECTS (array));
      TREE_THIS_VOLATILE (rval)
	|= (CP_TYPE_VOLATILE_P (type) | TREE_THIS_VOLATILE (array));
      return require_complete_type (fold (rval));
    }

  {
    tree ar = default_conversion (array);
    tree ind = default_conversion (idx);

    /* Put the integer in IND to simplify error checking.  */
    if (TREE_CODE (TREE_TYPE (ar)) == INTEGER_TYPE)
      {
	tree temp = ar;
	ar = ind;
	ind = temp;
      }

    if (ar == error_mark_node)
      return ar;

    if (TREE_CODE (TREE_TYPE (ar)) != POINTER_TYPE)
      {
	error ("subscripted value is neither array nor pointer");
	return error_mark_node;
      }
    if (TREE_CODE (TREE_TYPE (ind)) != INTEGER_TYPE)
      {
	error ("array subscript is not an integer");
	return error_mark_node;
      }

    return build_indirect_ref (cp_build_binary_op (PLUS_EXPR, ar, ind),
			       "array indexing");
  }
}

/* Resolve a pointer to member function.  INSTANCE is the object
   instance to use, if the member points to a virtual member.

   This used to avoid checking for virtual functions if basetype
   has no virtual functions, according to an earlier ANSI draft.
   With the final ISO C++ rules, such an optimization is
   incorrect: A pointer to a derived member can be static_cast
   to pointer-to-base-member, as long as the dynamic object
   later has the right member.  */

tree
get_member_function_from_ptrfunc (instance_ptrptr, function)
     tree *instance_ptrptr;
     tree function;
{
  if (TREE_CODE (function) == OFFSET_REF)
    function = TREE_OPERAND (function, 1);

  if (TYPE_PTRMEMFUNC_P (TREE_TYPE (function)))
    {
      tree idx, delta, e1, e2, e3, vtbl, basetype;
      tree fntype = TYPE_PTRMEMFUNC_FN_TYPE (TREE_TYPE (function));

      tree instance_ptr = *instance_ptrptr;
      tree instance_save_expr = 0;
      if (instance_ptr == error_mark_node)
	{
	  if (TREE_CODE (function) == PTRMEM_CST)
	    {
	      /* Extracting the function address from a pmf is only
		 allowed with -Wno-pmf-conversions. It only works for
		 pmf constants.  */
	      e1 = build_addr_func (PTRMEM_CST_MEMBER (function));
	      e1 = convert (fntype, e1);
	      return e1;
	    }
	  else
	    {
	      error ("object missing in use of `%E'", function);
	      return error_mark_node;
	    }
	}

      if (TREE_SIDE_EFFECTS (instance_ptr))
	instance_ptr = instance_save_expr = save_expr (instance_ptr);

      if (TREE_SIDE_EFFECTS (function))
	function = save_expr (function);

      /* Start by extracting all the information from the PMF itself.  */
      e3 = PFN_FROM_PTRMEMFUNC (function);
      delta = build_ptrmemfunc_access_expr (function, delta_identifier);
      idx = build1 (NOP_EXPR, vtable_index_type, e3);
      switch (TARGET_PTRMEMFUNC_VBIT_LOCATION)
	{
	case ptrmemfunc_vbit_in_pfn:
	  e1 = cp_build_binary_op (BIT_AND_EXPR, idx, integer_one_node);
	  idx = cp_build_binary_op (MINUS_EXPR, idx, integer_one_node);
	  break;

	case ptrmemfunc_vbit_in_delta:
	  e1 = cp_build_binary_op (BIT_AND_EXPR, delta, integer_one_node);
	  delta = cp_build_binary_op (RSHIFT_EXPR, delta, integer_one_node);
	  break;

	default:
	  abort ();
	}

      /* Convert down to the right base before using the instance.  First
         use the type...  */
      basetype = TYPE_METHOD_BASETYPE (TREE_TYPE (fntype));
      basetype = lookup_base (TREE_TYPE (TREE_TYPE (instance_ptr)),
			      basetype, ba_check, NULL);
      instance_ptr = build_base_path (PLUS_EXPR, instance_ptr, basetype, 1);
      if (instance_ptr == error_mark_node)
	return error_mark_node;
      /* ...and then the delta in the PMF.  */
      instance_ptr = build (PLUS_EXPR, TREE_TYPE (instance_ptr),
			    instance_ptr, delta);

      /* Hand back the adjusted 'this' argument to our caller.  */
      *instance_ptrptr = instance_ptr;

      /* Next extract the vtable pointer from the object.  */
      vtbl = build1 (NOP_EXPR, build_pointer_type (vtbl_ptr_type_node),
		     instance_ptr);
      vtbl = build_indirect_ref (vtbl, NULL);

      /* Finally, extract the function pointer from the vtable.  */
      e2 = fold (build (PLUS_EXPR, TREE_TYPE (vtbl), vtbl, idx));
      e2 = build_indirect_ref (e2, NULL);
      TREE_CONSTANT (e2) = 1;

      /* When using function descriptors, the address of the
	 vtable entry is treated as a function pointer.  */
      if (TARGET_VTABLE_USES_DESCRIPTORS)
	e2 = build1 (NOP_EXPR, TREE_TYPE (e2),
		     build_unary_op (ADDR_EXPR, e2, /*noconvert=*/1));

      TREE_TYPE (e2) = TREE_TYPE (e3);
      e1 = build_conditional_expr (e1, e2, e3);
      
      /* Make sure this doesn't get evaluated first inside one of the
	 branches of the COND_EXPR.  */
      if (instance_save_expr)
	e1 = build (COMPOUND_EXPR, TREE_TYPE (e1),
		    instance_save_expr, e1);

      function = e1;
    }
  return function;
}

tree
build_function_call_real (function, params, require_complete, flags)
     tree function, params;
     int require_complete, flags;
{
  register tree fntype, fndecl;
  register tree coerced_params;
  tree result;
  tree name = NULL_TREE, assembler_name = NULL_TREE;
  int is_method;
  tree original = function;

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs, since FUNCTION is used in non-lvalue context.  */
  if (TREE_CODE (function) == NOP_EXPR
      && TREE_TYPE (function) == TREE_TYPE (TREE_OPERAND (function, 0)))
    function = TREE_OPERAND (function, 0);

  if (TREE_CODE (function) == FUNCTION_DECL)
    {
      name = DECL_NAME (function);
      assembler_name = DECL_ASSEMBLER_NAME (function);

      mark_used (function);
      fndecl = function;

      /* Convert anything with function type to a pointer-to-function.  */
      if (pedantic && DECL_MAIN_P (function))
	pedwarn ("ISO C++ forbids calling `::main' from within program");

      /* Differs from default_conversion by not setting TREE_ADDRESSABLE
	 (because calling an inline function does not mean the function
	 needs to be separately compiled).  */
      
      if (DECL_INLINE (function))
	function = inline_conversion (function);
      else
	function = build_addr_func (function);
    }
  else
    {
      fndecl = NULL_TREE;

      function = build_addr_func (function);
    }

  if (function == error_mark_node)
    return error_mark_node;

  fntype = TREE_TYPE (function);

  if (TYPE_PTRMEMFUNC_P (fntype))
    {
      error ("must use .* or ->* to call pointer-to-member function in `%E (...)'",
		original);
      return error_mark_node;
    }

  is_method = (TREE_CODE (fntype) == POINTER_TYPE
	       && TREE_CODE (TREE_TYPE (fntype)) == METHOD_TYPE);

  if (!((TREE_CODE (fntype) == POINTER_TYPE
	 && TREE_CODE (TREE_TYPE (fntype)) == FUNCTION_TYPE)
	|| is_method
	|| TREE_CODE (function) == TEMPLATE_ID_EXPR))
    {
      error ("`%E' cannot be used as a function", original);
      return error_mark_node;
    }

  /* fntype now gets the type of function pointed to.  */
  fntype = TREE_TYPE (fntype);

  /* Convert the parameters to the types declared in the
     function prototype, or apply default promotions.  */

  if (flags & LOOKUP_COMPLAIN)
    coerced_params = convert_arguments (TYPE_ARG_TYPES (fntype),
					params, fndecl, LOOKUP_NORMAL);
  else
    coerced_params = convert_arguments (TYPE_ARG_TYPES (fntype),
					params, fndecl, 0);

  if (coerced_params == error_mark_node)
    {
      if (flags & LOOKUP_SPECULATIVELY)
	return NULL_TREE;
      else
	return error_mark_node;
    }

  /* Check for errors in format strings.  */

  if (warn_format)
    check_function_format (NULL, TYPE_ATTRIBUTES (fntype), coerced_params);

  /* Recognize certain built-in functions so we can make tree-codes
     other than CALL_EXPR.  We do this when it enables fold-const.c
     to do something useful.  */

  if (TREE_CODE (function) == ADDR_EXPR
      && TREE_CODE (TREE_OPERAND (function, 0)) == FUNCTION_DECL
      && DECL_BUILT_IN (TREE_OPERAND (function, 0)))
    {
      result = expand_tree_builtin (TREE_OPERAND (function, 0),
				    params, coerced_params);
      if (result)
	return result;
    }

  return build_cxx_call (function, params, coerced_params);
}

tree
build_function_call (function, params)
     tree function, params;
{
  return build_function_call_real (function, params, 1, LOOKUP_NORMAL);
}

/* Convert the actual parameter expressions in the list VALUES
   to the types in the list TYPELIST.
   If parmdecls is exhausted, or when an element has NULL as its type,
   perform the default conversions.

   NAME is an IDENTIFIER_NODE or 0.  It is used only for error messages.

   This is also where warnings about wrong number of args are generated.
   
   Return a list of expressions for the parameters as converted.

   Both VALUES and the returned value are chains of TREE_LIST nodes
   with the elements of the list in the TREE_VALUE slots of those nodes.

   In C++, unspecified trailing parameters can be filled in with their
   default arguments, if such were specified.  Do so here.  */

tree
convert_arguments (typelist, values, fndecl, flags)
     tree typelist, values, fndecl;
     int flags;
{
  register tree typetail, valtail;
  register tree result = NULL_TREE;
  const char *called_thing = 0;
  int i = 0;

  /* Argument passing is always copy-initialization.  */
  flags |= LOOKUP_ONLYCONVERTING;

  if (fndecl)
    {
      if (TREE_CODE (TREE_TYPE (fndecl)) == METHOD_TYPE)
	{
	  if (DECL_NAME (fndecl) == NULL_TREE
	      || IDENTIFIER_HAS_TYPE_VALUE (DECL_NAME (fndecl)))
	    called_thing = "constructor";
	  else
	    called_thing = "member function";
	}
      else
	called_thing = "function";
    }

  for (valtail = values, typetail = typelist;
       valtail;
       valtail = TREE_CHAIN (valtail), i++)
    {
      register tree type = typetail ? TREE_VALUE (typetail) : 0;
      register tree val = TREE_VALUE (valtail);

      if (val == error_mark_node)
	return error_mark_node;

      if (type == void_type_node)
	{
	  if (fndecl)
	    {
	      cp_error_at ("too many arguments to %s `%+#D'", called_thing,
			   fndecl);
	      error ("at this point in file");
	    }
	  else
	    error ("too many arguments to function");
	  /* In case anybody wants to know if this argument
	     list is valid.  */
	  if (result)
	    TREE_TYPE (tree_last (result)) = error_mark_node;
	  break;
	}

      if (TREE_CODE (val) == OFFSET_REF)
	val = resolve_offset_ref (val);

      /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
	 Strip such NOP_EXPRs, since VAL is used in non-lvalue context.  */
      if (TREE_CODE (val) == NOP_EXPR
	  && TREE_TYPE (val) == TREE_TYPE (TREE_OPERAND (val, 0))
	  && (type == 0 || TREE_CODE (type) != REFERENCE_TYPE))
	val = TREE_OPERAND (val, 0);

      if (type == 0 || TREE_CODE (type) != REFERENCE_TYPE)
	{
	  if (TREE_CODE (TREE_TYPE (val)) == ARRAY_TYPE
	      || TREE_CODE (TREE_TYPE (val)) == FUNCTION_TYPE
	      || TREE_CODE (TREE_TYPE (val)) == METHOD_TYPE)
	    val = default_conversion (val);
	}

      if (val == error_mark_node)
	return error_mark_node;

      if (type != 0)
	{
	  /* Formal parm type is specified by a function prototype.  */
	  tree parmval;

	  if (!COMPLETE_TYPE_P (complete_type (type)))
	    {
	      error ("parameter type of called function is incomplete");
	      parmval = val;
	    }
	  else
	    {
	      parmval = convert_for_initialization
		(NULL_TREE, type, val, flags,
		 "argument passing", fndecl, i);
	      parmval = convert_for_arg_passing (type, parmval);
	    }

	  if (parmval == error_mark_node)
	    return error_mark_node;

	  result = tree_cons (NULL_TREE, parmval, result);
	}
      else
	{
	  if (TREE_CODE (TREE_TYPE (val)) == REFERENCE_TYPE)
	    val = convert_from_reference (val);

	  if (fndecl && DECL_BUILT_IN (fndecl)
	      && DECL_FUNCTION_CODE (fndecl) == BUILT_IN_CONSTANT_P)
	    /* Don't do ellipsis conversion for __built_in_constant_p
	       as this will result in spurious warnings for non-POD
	       types.  */
	    val = require_complete_type (val);
	  else
	    val = convert_arg_to_ellipsis (val);

	  result = tree_cons (NULL_TREE, val, result);
	}

      if (typetail)
	typetail = TREE_CHAIN (typetail);
    }

  if (typetail != 0 && typetail != void_list_node)
    {
      /* See if there are default arguments that can be used */
      if (TREE_PURPOSE (typetail))
	{
	  for (; typetail != void_list_node; ++i)
	    {
	      tree parmval 
		= convert_default_arg (TREE_VALUE (typetail), 
				       TREE_PURPOSE (typetail), 
				       fndecl, i);

	      if (parmval == error_mark_node)
		return error_mark_node;

	      result = tree_cons (0, parmval, result);
	      typetail = TREE_CHAIN (typetail);
	      /* ends with `...'.  */
	      if (typetail == NULL_TREE)
		break;
	    }
	}
      else
	{
	  if (fndecl)
	    {
	      cp_error_at ("too few arguments to %s `%+#D'",
	                   called_thing, fndecl);
	      error ("at this point in file");
	    }
	  else
	    error ("too few arguments to function");
	  return error_mark_list;
	}
    }

  return nreverse (result);
}

/* Build a binary-operation expression, after performing default
   conversions on the operands.  CODE is the kind of expression to build.  */

tree
build_x_binary_op (code, arg1, arg2)
     enum tree_code code;
     tree arg1, arg2;
{
  if (processing_template_decl)
    return build_min_nt (code, arg1, arg2);

  return build_new_op (code, LOOKUP_NORMAL, arg1, arg2, NULL_TREE);
}

/* Build a binary-operation expression without default conversions.
   CODE is the kind of expression to build.
   This function differs from `build' in several ways:
   the data type of the result is computed and recorded in it,
   warnings are generated if arg data types are invalid,
   special handling for addition and subtraction of pointers is known,
   and some optimization is done (operations on narrow ints
   are done in the narrower type when that gives the same result).
   Constant folding is also done before the result is returned.

   Note that the operands will never have enumeral types
   because either they have just had the default conversions performed
   or they have both just been converted to some other type in which
   the arithmetic is to be done.

   C++: must do special pointer arithmetic when implementing
   multiple inheritance, and deal with pointer to member functions.  */

tree
build_binary_op (code, orig_op0, orig_op1, convert_p)
     enum tree_code code;
     tree orig_op0, orig_op1;
     int convert_p ATTRIBUTE_UNUSED;
{
  tree op0, op1;
  register enum tree_code code0, code1;
  tree type0, type1;

  /* Expression code to give to the expression when it is built.
     Normally this is CODE, which is what the caller asked for,
     but in some special cases we change it.  */
  register enum tree_code resultcode = code;

  /* Data type in which the computation is to be performed.
     In the simplest cases this is the common type of the arguments.  */
  register tree result_type = NULL;

  /* Nonzero means operands have already been type-converted
     in whatever way is necessary.
     Zero means they need to be converted to RESULT_TYPE.  */
  int converted = 0;

  /* Nonzero means create the expression with this type, rather than
     RESULT_TYPE.  */
  tree build_type = 0;

  /* Nonzero means after finally constructing the expression
     convert it to this type.  */
  tree final_type = 0;

  /* Nonzero if this is an operation like MIN or MAX which can
     safely be computed in short if both args are promoted shorts.
     Also implies COMMON.
     -1 indicates a bitwise operation; this makes a difference
     in the exact conditions for when it is safe to do the operation
     in a narrower mode.  */
  int shorten = 0;

  /* Nonzero if this is a comparison operation;
     if both args are promoted shorts, compare the original shorts.
     Also implies COMMON.  */
  int short_compare = 0;

  /* Nonzero if this is a right-shift operation, which can be computed on the
     original short and then promoted if the operand is a promoted short.  */
  int short_shift = 0;

  /* Nonzero means set RESULT_TYPE to the common type of the args.  */
  int common = 0;

  /* Apply default conversions.  */
  op0 = orig_op0;
  op1 = orig_op1;
  
  if (code == TRUTH_AND_EXPR || code == TRUTH_ANDIF_EXPR
      || code == TRUTH_OR_EXPR || code == TRUTH_ORIF_EXPR
      || code == TRUTH_XOR_EXPR)
    {
      if (!really_overloaded_fn (op0))
	op0 = decay_conversion (op0);
      if (!really_overloaded_fn (op1))
	op1 = decay_conversion (op1);
    }
  else
    {
      if (!really_overloaded_fn (op0))
	op0 = default_conversion (op0);
      if (!really_overloaded_fn (op1))
	op1 = default_conversion (op1);
    }

  /* Strip NON_LVALUE_EXPRs, etc., since we aren't using as an lvalue.  */
  STRIP_TYPE_NOPS (op0);
  STRIP_TYPE_NOPS (op1);

  /* DTRT if one side is an overloaded function, but complain about it.  */
  if (type_unknown_p (op0))
    {
      tree t = instantiate_type (TREE_TYPE (op1), op0, tf_none);
      if (t != error_mark_node)
	{
	  pedwarn ("assuming cast to type `%T' from overloaded function",
		      TREE_TYPE (t));
	  op0 = t;
	}
    }
  if (type_unknown_p (op1))
    {
      tree t = instantiate_type (TREE_TYPE (op0), op1, tf_none);
      if (t != error_mark_node)
	{
	  pedwarn ("assuming cast to type `%T' from overloaded function",
		      TREE_TYPE (t));
	  op1 = t;
	}
    }

  type0 = TREE_TYPE (op0);
  type1 = TREE_TYPE (op1);

  /* The expression codes of the data types of the arguments tell us
     whether the arguments are integers, floating, pointers, etc.  */
  code0 = TREE_CODE (type0);
  code1 = TREE_CODE (type1);

  /* If an error was already reported for one of the arguments,
     avoid reporting another error.  */

  if (code0 == ERROR_MARK || code1 == ERROR_MARK)
    return error_mark_node;

  switch (code)
    {
    case PLUS_EXPR:
      /* Handle the pointer + int case.  */
      if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	return cp_pointer_int_sum (PLUS_EXPR, op0, op1);
      else if (code1 == POINTER_TYPE && code0 == INTEGER_TYPE)
	return cp_pointer_int_sum (PLUS_EXPR, op1, op0);
      else
	common = 1;
      break;

    case MINUS_EXPR:
      /* Subtraction of two similar pointers.
	 We must subtract them as integers, then divide by object size.  */
      if (code0 == POINTER_TYPE && code1 == POINTER_TYPE
	  && comp_target_types (type0, type1, 1))
	return pointer_diff (op0, op1, common_type (type0, type1));
      /* Handle pointer minus int.  Just like pointer plus int.  */
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	return cp_pointer_int_sum (MINUS_EXPR, op0, op1);
      else
	common = 1;
      break;

    case MULT_EXPR:
      common = 1;
      break;

    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case EXACT_DIV_EXPR:
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE
	   || code0 == COMPLEX_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == REAL_TYPE
	      || code1 == COMPLEX_TYPE))
	{
	  if (TREE_CODE (op1) == INTEGER_CST && integer_zerop (op1))
	    warning ("division by zero in `%E / 0'", op0);
	  else if (TREE_CODE (op1) == REAL_CST && real_zerop (op1))
	    warning ("division by zero in `%E / 0.'", op0);
	      
	  if (!(code0 == INTEGER_TYPE && code1 == INTEGER_TYPE))
	    resultcode = RDIV_EXPR;
	  else
	    /* When dividing two signed integers, we have to promote to int.
	       unless we divide by a constant != -1.  Note that default
	       conversion will have been performed on the operands at this
	       point, so we have to dig out the original type to find out if
	       it was unsigned.  */
	    shorten = ((TREE_CODE (op0) == NOP_EXPR
			&& TREE_UNSIGNED (TREE_TYPE (TREE_OPERAND (op0, 0))))
		       || (TREE_CODE (op1) == INTEGER_CST
			   && ! integer_all_onesp (op1)));

	  common = 1;
	}
      break;

    case BIT_AND_EXPR:
    case BIT_ANDTC_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	shorten = -1;
      break;

    case TRUNC_MOD_EXPR:
    case FLOOR_MOD_EXPR:
      if (code1 == INTEGER_TYPE && integer_zerop (op1))
	warning ("division by zero in `%E %% 0'", op0);
      else if (code1 == REAL_TYPE && real_zerop (op1))
	warning ("division by zero in `%E %% 0.'", op0);
      
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  /* Although it would be tempting to shorten always here, that loses
	     on some targets, since the modulo instruction is undefined if the
	     quotient can't be represented in the computation mode.  We shorten
	     only if unsigned or if dividing by something we know != -1.  */
	  shorten = ((TREE_CODE (op0) == NOP_EXPR
		      && TREE_UNSIGNED (TREE_TYPE (TREE_OPERAND (op0, 0))))
		     || (TREE_CODE (op1) == INTEGER_CST
			 && ! integer_all_onesp (op1)));
	  common = 1;
	}
      break;

    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case TRUTH_AND_EXPR:
    case TRUTH_OR_EXPR:
      result_type = boolean_type_node;
      break;

      /* Shift operations: result has same type as first operand;
	 always convert second operand to int.
	 Also set SHORT_SHIFT if shifting rightward.  */

    case RSHIFT_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  if (TREE_CODE (op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_lt (op1, integer_zero_node))
		warning ("right shift count is negative");
	      else
		{
		  if (! integer_zerop (op1))
		    short_shift = 1;
		  if (compare_tree_int (op1, TYPE_PRECISION (type0)) >= 0)
		    warning ("right shift count >= width of type");
		}
	    }
	  /* Convert the shift-count to an integer, regardless of
	     size of value being shifted.  */
	  if (TYPE_MAIN_VARIANT (TREE_TYPE (op1)) != integer_type_node)
	    op1 = cp_convert (integer_type_node, op1);
	  /* Avoid converting op1 to result_type later.  */
	  converted = 1;
	}
      break;

    case LSHIFT_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  if (TREE_CODE (op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_lt (op1, integer_zero_node))
		warning ("left shift count is negative");
	      else if (compare_tree_int (op1, TYPE_PRECISION (type0)) >= 0)
		warning ("left shift count >= width of type");
	    }
	  /* Convert the shift-count to an integer, regardless of
	     size of value being shifted.  */
	  if (TYPE_MAIN_VARIANT (TREE_TYPE (op1)) != integer_type_node)
	    op1 = cp_convert (integer_type_node, op1);
	  /* Avoid converting op1 to result_type later.  */
	  converted = 1;
	}
      break;

    case RROTATE_EXPR:
    case LROTATE_EXPR:
      if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  if (TREE_CODE (op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_lt (op1, integer_zero_node))
		warning ("%s rotate count is negative",
			 (code == LROTATE_EXPR) ? "left" : "right");
	      else if (compare_tree_int (op1, TYPE_PRECISION (type0)) >= 0)
		warning ("%s rotate count >= width of type",
			 (code == LROTATE_EXPR) ? "left" : "right");
	    }
	  /* Convert the shift-count to an integer, regardless of
	     size of value being shifted.  */
	  if (TYPE_MAIN_VARIANT (TREE_TYPE (op1)) != integer_type_node)
	    op1 = cp_convert (integer_type_node, op1);
	}
      break;

    case EQ_EXPR:
    case NE_EXPR:
      if (warn_float_equal && (code0 == REAL_TYPE || code1 == REAL_TYPE))
	warning ("comparing floating point with == or != is unsafe");

      build_type = boolean_type_node; 
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE
	   || code0 == COMPLEX_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == REAL_TYPE
	      || code1 == COMPLEX_TYPE))
	short_compare = 1;
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	result_type = composite_pointer_type (type0, type1, op0, op1,
					      "comparison");
      else if (code0 == POINTER_TYPE && null_ptr_cst_p (op1))
	result_type = type0;
      else if (code1 == POINTER_TYPE && null_ptr_cst_p (op0))
	result_type = type1;
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  error ("ISO C++ forbids comparison between pointer and integer");
	}
      else if (code0 == INTEGER_TYPE && code1 == POINTER_TYPE)
	{
	  result_type = type1;
	  error ("ISO C++ forbids comparison between pointer and integer");
	}
      else if (TYPE_PTRMEMFUNC_P (type0) && null_ptr_cst_p (op1))
	{
	  op0 = build_ptrmemfunc_access_expr (op0, pfn_identifier);
	  op1 = cp_convert (TREE_TYPE (op0), integer_zero_node);
	  result_type = TREE_TYPE (op0);
	}
      else if (TYPE_PTRMEMFUNC_P (type1) && null_ptr_cst_p (op0))
	return cp_build_binary_op (code, op1, op0);
      else if (TYPE_PTRMEMFUNC_P (type0) && TYPE_PTRMEMFUNC_P (type1)
	       && same_type_p (type0, type1))
	{
	  /* E will be the final comparison.  */
	  tree e;
	  /* E1 and E2 are for scratch.  */
	  tree e1;
	  tree e2;
	  tree pfn0;
	  tree pfn1;
	  tree delta0;
	  tree delta1;

	  if (TREE_SIDE_EFFECTS (op0))
	    op0 = save_expr (op0);
	  if (TREE_SIDE_EFFECTS (op1))
	    op1 = save_expr (op1);

	  /* We generate:

	     (op0.pfn == op1.pfn 
	      && (!op0.pfn || op0.delta == op1.delta))
	     
	     The reason for the `!op0.pfn' bit is that a NULL
	     pointer-to-member is any member with a zero PFN; the
	     DELTA field is unspecified.  */
	  pfn0 = pfn_from_ptrmemfunc (op0);
	  pfn1 = pfn_from_ptrmemfunc (op1);
	  delta0 = build_ptrmemfunc_access_expr (op0,
						 delta_identifier);
	  delta1 = build_ptrmemfunc_access_expr (op1,
						 delta_identifier);
	  e1 = cp_build_binary_op (EQ_EXPR, delta0, delta1);
	  e2 = cp_build_binary_op (EQ_EXPR, 
				   pfn0,
				   cp_convert (TREE_TYPE (pfn0),
					       integer_zero_node));
	  e1 = cp_build_binary_op (TRUTH_ORIF_EXPR, e1, e2);
	  e2 = build (EQ_EXPR, boolean_type_node, pfn0, pfn1);
	  e = cp_build_binary_op (TRUTH_ANDIF_EXPR, e2, e1);
	  if (code == EQ_EXPR)
	    return e;
	  return cp_build_binary_op (EQ_EXPR, e, integer_zero_node);
	}
      else if ((TYPE_PTRMEMFUNC_P (type0)
		&& same_type_p (TYPE_PTRMEMFUNC_FN_TYPE (type0), type1))
	       || (TYPE_PTRMEMFUNC_P (type1)
		   && same_type_p (TYPE_PTRMEMFUNC_FN_TYPE (type1), type0)))
	abort ();
      break;

    case MAX_EXPR:
    case MIN_EXPR:
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE)
	   && (code1 == INTEGER_TYPE || code1 == REAL_TYPE))
	shorten = 1;
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	result_type = composite_pointer_type (type0, type1, op0, op1,
					      "comparison");
      break;

    case LE_EXPR:
    case GE_EXPR:
    case LT_EXPR:
    case GT_EXPR:
      build_type = boolean_type_node;
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE)
	   && (code1 == INTEGER_TYPE || code1 == REAL_TYPE))
	short_compare = 1;
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	result_type = composite_pointer_type (type0, type1, op0, op1,
					      "comparison");
      else if (code0 == POINTER_TYPE && TREE_CODE (op1) == INTEGER_CST
	       && integer_zerop (op1))
	result_type = type0;
      else if (code1 == POINTER_TYPE && TREE_CODE (op0) == INTEGER_CST
	       && integer_zerop (op0))
	result_type = type1;
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  pedwarn ("ISO C++ forbids comparison between pointer and integer");
	}
      else if (code0 == INTEGER_TYPE && code1 == POINTER_TYPE)
	{
	  result_type = type1;
	  pedwarn ("ISO C++ forbids comparison between pointer and integer");
	}
      break;

    case UNORDERED_EXPR:
    case ORDERED_EXPR:
    case UNLT_EXPR:
    case UNLE_EXPR:
    case UNGT_EXPR:
    case UNGE_EXPR:
    case UNEQ_EXPR:
      build_type = integer_type_node;
      if (code0 != REAL_TYPE || code1 != REAL_TYPE)
	{
	  error ("unordered comparison on non-floating point argument");
	  return error_mark_node;
	}
      common = 1;
      break;

    default:
      break;
    }

  if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE || code0 == COMPLEX_TYPE)
      &&
      (code1 == INTEGER_TYPE || code1 == REAL_TYPE || code1 == COMPLEX_TYPE))
    {
      int none_complex = (code0 != COMPLEX_TYPE && code1 != COMPLEX_TYPE);

      if (shorten || common || short_compare)
	result_type = common_type (type0, type1);

      /* For certain operations (which identify themselves by shorten != 0)
	 if both args were extended from the same smaller type,
	 do the arithmetic in that type and then extend.

	 shorten !=0 and !=1 indicates a bitwise operation.
	 For them, this optimization is safe only if
	 both args are zero-extended or both are sign-extended.
	 Otherwise, we might change the result.
	 Eg, (short)-1 | (unsigned short)-1 is (int)-1
	 but calculated in (unsigned short) it would be (unsigned short)-1.  */

      if (shorten && none_complex)
	{
	  int unsigned0, unsigned1;
	  tree arg0 = get_narrower (op0, &unsigned0);
	  tree arg1 = get_narrower (op1, &unsigned1);
	  /* UNS is 1 if the operation to be done is an unsigned one.  */
	  int uns = TREE_UNSIGNED (result_type);
	  tree type;

	  final_type = result_type;

	  /* Handle the case that OP0 does not *contain* a conversion
	     but it *requires* conversion to FINAL_TYPE.  */

	  if (op0 == arg0 && TREE_TYPE (op0) != final_type)
	    unsigned0 = TREE_UNSIGNED (TREE_TYPE (op0));
	  if (op1 == arg1 && TREE_TYPE (op1) != final_type)
	    unsigned1 = TREE_UNSIGNED (TREE_TYPE (op1));

	  /* Now UNSIGNED0 is 1 if ARG0 zero-extends to FINAL_TYPE.  */

	  /* For bitwise operations, signedness of nominal type
	     does not matter.  Consider only how operands were extended.  */
	  if (shorten == -1)
	    uns = unsigned0;

	  /* Note that in all three cases below we refrain from optimizing
	     an unsigned operation on sign-extended args.
	     That would not be valid.  */

	  /* Both args variable: if both extended in same way
	     from same width, do it in that width.
	     Do it unsigned if args were zero-extended.  */
	  if ((TYPE_PRECISION (TREE_TYPE (arg0))
	       < TYPE_PRECISION (result_type))
	      && (TYPE_PRECISION (TREE_TYPE (arg1))
		  == TYPE_PRECISION (TREE_TYPE (arg0)))
	      && unsigned0 == unsigned1
	      && (unsigned0 || !uns))
	    result_type = c_common_signed_or_unsigned_type
	      (unsigned0, common_type (TREE_TYPE (arg0), TREE_TYPE (arg1)));
	  else if (TREE_CODE (arg0) == INTEGER_CST
		   && (unsigned1 || !uns)
		   && (TYPE_PRECISION (TREE_TYPE (arg1))
		       < TYPE_PRECISION (result_type))
		   && (type = c_common_signed_or_unsigned_type
		       (unsigned1, TREE_TYPE (arg1)),
		       int_fits_type_p (arg0, type)))
	    result_type = type;
	  else if (TREE_CODE (arg1) == INTEGER_CST
		   && (unsigned0 || !uns)
		   && (TYPE_PRECISION (TREE_TYPE (arg0))
		       < TYPE_PRECISION (result_type))
		   && (type = c_common_signed_or_unsigned_type
		       (unsigned0, TREE_TYPE (arg0)),
		       int_fits_type_p (arg1, type)))
	    result_type = type;
	}

      /* Shifts can be shortened if shifting right.  */

      if (short_shift)
	{
	  int unsigned_arg;
	  tree arg0 = get_narrower (op0, &unsigned_arg);

	  final_type = result_type;

	  if (arg0 == op0 && final_type == TREE_TYPE (op0))
	    unsigned_arg = TREE_UNSIGNED (TREE_TYPE (op0));

	  if (TYPE_PRECISION (TREE_TYPE (arg0)) < TYPE_PRECISION (result_type)
	      /* We can shorten only if the shift count is less than the
		 number of bits in the smaller type size.  */
	      && compare_tree_int (op1, TYPE_PRECISION (TREE_TYPE (arg0))) < 0
	      /* If arg is sign-extended and then unsigned-shifted,
		 we can simulate this with a signed shift in arg's type
		 only if the extended result is at least twice as wide
		 as the arg.  Otherwise, the shift could use up all the
		 ones made by sign-extension and bring in zeros.
		 We can't optimize that case at all, but in most machines
		 it never happens because available widths are 2**N.  */
	      && (!TREE_UNSIGNED (final_type)
		  || unsigned_arg
		  || (((unsigned) 2 * TYPE_PRECISION (TREE_TYPE (arg0)))
		      <= TYPE_PRECISION (result_type))))
	    {
	      /* Do an unsigned shift if the operand was zero-extended.  */
	      result_type
		= c_common_signed_or_unsigned_type (unsigned_arg,
						    TREE_TYPE (arg0));
	      /* Convert value-to-be-shifted to that type.  */
	      if (TREE_TYPE (op0) != result_type)
		op0 = cp_convert (result_type, op0);
	      converted = 1;
	    }
	}

      /* Comparison operations are shortened too but differently.
	 They identify themselves by setting short_compare = 1.  */

      if (short_compare)
	{
	  /* Don't write &op0, etc., because that would prevent op0
	     from being kept in a register.
	     Instead, make copies of the our local variables and
	     pass the copies by reference, then copy them back afterward.  */
	  tree xop0 = op0, xop1 = op1, xresult_type = result_type;
	  enum tree_code xresultcode = resultcode;
	  tree val 
	    = shorten_compare (&xop0, &xop1, &xresult_type, &xresultcode);
	  if (val != 0)
	    return cp_convert (boolean_type_node, val);
	  op0 = xop0, op1 = xop1;
	  converted = 1;
	  resultcode = xresultcode;
	}

      if ((short_compare || code == MIN_EXPR || code == MAX_EXPR)
	  && warn_sign_compare)
	{
	  int op0_signed = ! TREE_UNSIGNED (TREE_TYPE (orig_op0));
	  int op1_signed = ! TREE_UNSIGNED (TREE_TYPE (orig_op1));

	  int unsignedp0, unsignedp1;
	  tree primop0 = get_narrower (op0, &unsignedp0);
	  tree primop1 = get_narrower (op1, &unsignedp1);

	  /* Check for comparison of different enum types.  */
	  if (TREE_CODE (TREE_TYPE (orig_op0)) == ENUMERAL_TYPE 
	      && TREE_CODE (TREE_TYPE (orig_op1)) == ENUMERAL_TYPE 
	      && TYPE_MAIN_VARIANT (TREE_TYPE (orig_op0))
	         != TYPE_MAIN_VARIANT (TREE_TYPE (orig_op1)))
	    {
	      warning ("comparison between types `%#T' and `%#T'", 
			  TREE_TYPE (orig_op0), TREE_TYPE (orig_op1));
	    }

	  /* Give warnings for comparisons between signed and unsigned
	     quantities that may fail.  */
	  /* Do the checking based on the original operand trees, so that
	     casts will be considered, but default promotions won't be.  */

	  /* Do not warn if the comparison is being done in a signed type,
	     since the signed type will only be chosen if it can represent
	     all the values of the unsigned type.  */
	  if (! TREE_UNSIGNED (result_type))
	    /* OK */;
	  /* Do not warn if both operands are unsigned.  */
	  else if (op0_signed == op1_signed)
	    /* OK */;
	  /* Do not warn if the signed quantity is an unsuffixed
	     integer literal (or some static constant expression
	     involving such literals or a conditional expression
	     involving such literals) and it is non-negative.  */
	  else if ((op0_signed && tree_expr_nonnegative_p (orig_op0))
		   || (op1_signed && tree_expr_nonnegative_p (orig_op1)))
	    /* OK */;
	  /* Do not warn if the comparison is an equality operation,
	     the unsigned quantity is an integral constant and it does
	     not use the most significant bit of result_type.  */
	  else if ((resultcode == EQ_EXPR || resultcode == NE_EXPR)
		   && ((op0_signed && TREE_CODE (orig_op1) == INTEGER_CST
			&& int_fits_type_p (orig_op1, c_common_signed_type
					    (result_type)))
			|| (op1_signed && TREE_CODE (orig_op0) == INTEGER_CST
			    && int_fits_type_p (orig_op0, c_common_signed_type
						(result_type)))))
	    /* OK */;
	  else
	    warning ("comparison between signed and unsigned integer expressions");

	  /* Warn if two unsigned values are being compared in a size
	     larger than their original size, and one (and only one) is the
	     result of a `~' operator.  This comparison will always fail.

	     Also warn if one operand is a constant, and the constant does not
	     have all bits set that are set in the ~ operand when it is
	     extended.  */

	  if ((TREE_CODE (primop0) == BIT_NOT_EXPR)
	      ^ (TREE_CODE (primop1) == BIT_NOT_EXPR))
	    {
	      if (TREE_CODE (primop0) == BIT_NOT_EXPR)
		primop0 = get_narrower (TREE_OPERAND (op0, 0), &unsignedp0);
	      if (TREE_CODE (primop1) == BIT_NOT_EXPR)
		primop1 = get_narrower (TREE_OPERAND (op1, 0), &unsignedp1);
	      
	      if (host_integerp (primop0, 0) || host_integerp (primop1, 0))
		{
		  tree primop;
		  HOST_WIDE_INT constant, mask;
		  int unsignedp;
		  unsigned int bits;

		  if (host_integerp (primop0, 0))
		    {
		      primop = primop1;
		      unsignedp = unsignedp1;
		      constant = tree_low_cst (primop0, 0);
		    }
		  else
		    {
		      primop = primop0;
		      unsignedp = unsignedp0;
		      constant = tree_low_cst (primop1, 0);
		    }

		  bits = TYPE_PRECISION (TREE_TYPE (primop));
		  if (bits < TYPE_PRECISION (result_type)
		      && bits < HOST_BITS_PER_LONG && unsignedp)
		    {
		      mask = (~ (HOST_WIDE_INT) 0) << bits;
		      if ((mask & constant) != mask)
			warning ("comparison of promoted ~unsigned with constant");
		    }
		}
	      else if (unsignedp0 && unsignedp1
		       && (TYPE_PRECISION (TREE_TYPE (primop0))
			   < TYPE_PRECISION (result_type))
		       && (TYPE_PRECISION (TREE_TYPE (primop1))
			   < TYPE_PRECISION (result_type)))
		warning ("comparison of promoted ~unsigned with unsigned");
	    }
	}
    }

  /* At this point, RESULT_TYPE must be nonzero to avoid an error message.
     If CONVERTED is zero, both args will be converted to type RESULT_TYPE.
     Then the expression will be built.
     It will be given type FINAL_TYPE if that is nonzero;
     otherwise, it will be given type RESULT_TYPE.  */

  if (!result_type)
    {
      error ("invalid operands of types `%T' and `%T' to binary `%O'",
		TREE_TYPE (orig_op0), TREE_TYPE (orig_op1), code);
      return error_mark_node;
    }

  /* Issue warnings about peculiar, but valid, uses of NULL.  */
  if (/* It's reasonable to use pointer values as operands of &&
	 and ||, so NULL is no exception.  */
      !(code == TRUTH_ANDIF_EXPR || code == TRUTH_ORIF_EXPR)
      && (/* If OP0 is NULL and OP1 is not a pointer, or vice versa.  */
	  (orig_op0 == null_node
	   && TREE_CODE (TREE_TYPE (op1)) != POINTER_TYPE)
	  /* Or vice versa.  */
	  || (orig_op1 == null_node
	      && TREE_CODE (TREE_TYPE (op0)) != POINTER_TYPE)
	  /* Or, both are NULL and the operation was not a comparison.  */
	  || (orig_op0 == null_node && orig_op1 == null_node 
	      && code != EQ_EXPR && code != NE_EXPR)))
    /* Some sort of arithmetic operation involving NULL was
       performed.  Note that pointer-difference and pointer-addition
       have already been handled above, and so we don't end up here in
       that case.  */
    warning ("NULL used in arithmetic");

  if (! converted)
    {
      if (TREE_TYPE (op0) != result_type)
	op0 = cp_convert (result_type, op0); 
      if (TREE_TYPE (op1) != result_type)
	op1 = cp_convert (result_type, op1); 

      if (op0 == error_mark_node || op1 == error_mark_node)
	return error_mark_node;
    }

  if (build_type == NULL_TREE)
    build_type = result_type;

  {
    register tree result = build (resultcode, build_type, op0, op1);
    register tree folded;

    folded = fold (result);
    if (folded == result)
      TREE_CONSTANT (folded) = TREE_CONSTANT (op0) & TREE_CONSTANT (op1);
    if (final_type != 0)
      return cp_convert (final_type, folded);
    return folded;
  }
}

/* Return a tree for the sum or difference (RESULTCODE says which)
   of pointer PTROP and integer INTOP.  */

static tree
cp_pointer_int_sum (resultcode, ptrop, intop)
     enum tree_code resultcode;
     register tree ptrop, intop;
{
  tree res_type = TREE_TYPE (ptrop);

  /* pointer_int_sum() uses size_in_bytes() on the TREE_TYPE(res_type)
     in certain circumstance (when it's valid to do so).  So we need
     to make sure it's complete.  We don't need to check here, if we
     can actually complete it at all, as those checks will be done in
     pointer_int_sum() anyway.  */
  complete_type (TREE_TYPE (res_type));

  return pointer_int_sum (resultcode, ptrop, fold (intop));
}

/* Return a tree for the difference of pointers OP0 and OP1.
   The resulting tree has type int.  */

static tree
pointer_diff (op0, op1, ptrtype)
     register tree op0, op1;
     register tree ptrtype;
{
  register tree result, folded;
  tree restype = ptrdiff_type_node;
  tree target_type = TREE_TYPE (ptrtype);

  if (!complete_type_or_else (target_type, NULL_TREE))
    return error_mark_node;

  if (pedantic || warn_pointer_arith)
    {
      if (TREE_CODE (target_type) == VOID_TYPE)
	pedwarn ("ISO C++ forbids using pointer of type `void *' in subtraction");
      if (TREE_CODE (target_type) == FUNCTION_TYPE)
	pedwarn ("ISO C++ forbids using pointer to a function in subtraction");
      if (TREE_CODE (target_type) == METHOD_TYPE)
	pedwarn ("ISO C++ forbids using pointer to a method in subtraction");
      if (TREE_CODE (target_type) == OFFSET_TYPE)
	pedwarn ("ISO C++ forbids using pointer to a member in subtraction");
    }

  /* First do the subtraction as integers;
     then drop through to build the divide operator.  */

  op0 = cp_build_binary_op (MINUS_EXPR, 
			    cp_convert (restype, op0),
			    cp_convert (restype, op1));

  /* This generates an error if op1 is a pointer to an incomplete type.  */
  if (!COMPLETE_TYPE_P (TREE_TYPE (TREE_TYPE (op1))))
    error ("invalid use of a pointer to an incomplete type in pointer arithmetic");

  op1 = ((TREE_CODE (target_type) == VOID_TYPE
	  || TREE_CODE (target_type) == FUNCTION_TYPE
	  || TREE_CODE (target_type) == METHOD_TYPE
	  || TREE_CODE (target_type) == OFFSET_TYPE)
	 ? integer_one_node
	 : size_in_bytes (target_type));

  /* Do the division.  */

  result = build (EXACT_DIV_EXPR, restype, op0, cp_convert (restype, op1));

  folded = fold (result);
  if (folded == result)
    TREE_CONSTANT (folded) = TREE_CONSTANT (op0) & TREE_CONSTANT (op1);
  return folded;
}

/* Construct and perhaps optimize a tree representation
   for a unary operation.  CODE, a tree_code, specifies the operation
   and XARG is the operand.  */

tree
build_x_unary_op (code, xarg)
     enum tree_code code;
     tree xarg;
{
  tree exp;
  int ptrmem = 0;
  
  if (processing_template_decl)
    return build_min_nt (code, xarg, NULL_TREE);

  /* & rec, on incomplete RECORD_TYPEs is the simple opr &, not an
     error message.  */
  if (code == ADDR_EXPR
      && TREE_CODE (xarg) != TEMPLATE_ID_EXPR
      && ((IS_AGGR_TYPE_CODE (TREE_CODE (TREE_TYPE (xarg)))
	   && !COMPLETE_TYPE_P (TREE_TYPE (xarg)))
	  || (TREE_CODE (xarg) == OFFSET_REF)))
    /* don't look for a function */;
  else
    {
      tree rval;

      rval = build_new_op (code, LOOKUP_NORMAL, xarg,
			   NULL_TREE, NULL_TREE);
      if (rval || code != ADDR_EXPR)
	return rval;
    }
  if (code == ADDR_EXPR)
    {
      /*  A pointer to member-function can be formed only by saying
	  &X::mf.  */
      if (!flag_ms_extensions && TREE_CODE (TREE_TYPE (xarg)) == METHOD_TYPE
	  && (TREE_CODE (xarg) != OFFSET_REF || !PTRMEM_OK_P (xarg)))
	{
	  if (TREE_CODE (xarg) != OFFSET_REF)
	    {
	      error ("invalid use of '%E' to form a pointer-to-member-function.  Use a qualified-id.",
		     xarg);
	      return error_mark_node;
	    }
	  else
	    {
	      error ("parenthesis around '%E' cannot be used to form a pointer-to-member-function",
		     xarg);
	      PTRMEM_OK_P (xarg) = 1;
	    }
	}
      
      if (TREE_CODE (xarg) == OFFSET_REF)
        {
          ptrmem = PTRMEM_OK_P (xarg);
          
          if (!ptrmem && !flag_ms_extensions
              && TREE_CODE (TREE_TYPE (TREE_OPERAND (xarg, 1))) == METHOD_TYPE)
	    {
	      /* A single non-static member, make sure we don't allow a
                 pointer-to-member.  */
	      xarg = build (OFFSET_REF, TREE_TYPE (xarg),
			    TREE_OPERAND (xarg, 0),
			    ovl_cons (TREE_OPERAND (xarg, 1), NULL_TREE));
	      PTRMEM_OK_P (xarg) = ptrmem;
	    }
	      
        }
      else if (TREE_CODE (xarg) == TARGET_EXPR)
	warning ("taking address of temporary");
    }
  exp = build_unary_op (code, xarg, 0);
  if (TREE_CODE (exp) == ADDR_EXPR)
    PTRMEM_OK_P (exp) = ptrmem;

  return exp;
}

/* Like c_common_truthvalue_conversion, but handle pointer-to-member
   constants, where a null value is represented by an INTEGER_CST of
   -1.  */

tree
cp_truthvalue_conversion (expr)
     tree expr;
{
  tree type = TREE_TYPE (expr);
  if (TYPE_PTRMEM_P (type))
    return build_binary_op (NE_EXPR, expr, integer_zero_node, 1);
  else
    return c_common_truthvalue_conversion (expr);
}

/* Just like cp_truthvalue_conversion, but we want a CLEANUP_POINT_EXPR.  */
   
tree
condition_conversion (expr)
     tree expr;
{
  tree t;
  if (processing_template_decl)
    return expr;
  if (TREE_CODE (expr) == OFFSET_REF)
    expr = resolve_offset_ref (expr);
  t = perform_implicit_conversion (boolean_type_node, expr);
  t = fold (build1 (CLEANUP_POINT_EXPR, boolean_type_node, t));
  return t;
}
		
/* Return an ADDR_EXPR giving the address of T.  This function
   attempts no optimizations or simplifications; it is a low-level
   primitive.  */

tree
build_address (tree t)
{
  tree addr;

  if (error_operand_p (t) || !cxx_mark_addressable (t))
    return error_mark_node;

  addr = build1 (ADDR_EXPR, 
		 build_pointer_type (TREE_TYPE (t)),
		 t);
  if (staticp (t))
    TREE_CONSTANT (addr) = 1;

  return addr;
}

/* Return a NOP_EXPR converting EXPR to TYPE.  */

tree
build_nop (tree type, tree expr)
{
  tree nop;

  if (type == error_mark_node || error_operand_p (expr))
    return expr;
    
  nop = build1 (NOP_EXPR, type, expr);
  if (TREE_CONSTANT (expr))
    TREE_CONSTANT (nop) = 1;
  
  return nop;
}

/* C++: Must handle pointers to members.

   Perhaps type instantiation should be extended to handle conversion
   from aggregates to types we don't yet know we want?  (Or are those
   cases typically errors which should be reported?)

   NOCONVERT nonzero suppresses the default promotions
   (such as from short to int).  */

tree
build_unary_op (code, xarg, noconvert)
     enum tree_code code;
     tree xarg;
     int noconvert;
{
  /* No default_conversion here.  It causes trouble for ADDR_EXPR.  */
  register tree arg = xarg;
  register tree argtype = 0;
  const char *errstring = NULL;
  tree val;

  if (arg == error_mark_node)
    return error_mark_node;

  switch (code)
    {
    case CONVERT_EXPR:
      /* This is used for unary plus, because a CONVERT_EXPR
	 is enough to prevent anybody from looking inside for
	 associativity, but won't generate any code.  */
      if (!(arg = build_expr_type_conversion
	    (WANT_ARITH | WANT_ENUM | WANT_POINTER, arg, 1)))
	errstring = "wrong type argument to unary plus";
      else
	{
	  if (!noconvert)
	   arg = default_conversion (arg);
	  arg = build1 (NON_LVALUE_EXPR, TREE_TYPE (arg), arg);
	  TREE_CONSTANT (arg) = TREE_CONSTANT (TREE_OPERAND (arg, 0));
	}
      break;

    case NEGATE_EXPR:
      if (!(arg = build_expr_type_conversion (WANT_ARITH | WANT_ENUM, arg, 1)))
	errstring = "wrong type argument to unary minus";
      else if (!noconvert)
	arg = default_conversion (arg);
      break;

    case BIT_NOT_EXPR:
      if (TREE_CODE (TREE_TYPE (arg)) == COMPLEX_TYPE)
	{
	  code = CONJ_EXPR;
	  if (!noconvert)
	    arg = default_conversion (arg);
	}
      else if (!(arg = build_expr_type_conversion (WANT_INT | WANT_ENUM,
						   arg, 1)))
	errstring = "wrong type argument to bit-complement";
      else if (!noconvert)
	arg = default_conversion (arg);
      break;

    case ABS_EXPR:
      if (!(arg = build_expr_type_conversion (WANT_ARITH | WANT_ENUM, arg, 1)))
	errstring = "wrong type argument to abs";
      else if (!noconvert)
	arg = default_conversion (arg);
      break;

    case CONJ_EXPR:
      /* Conjugating a real value is a no-op, but allow it anyway.  */
      if (!(arg = build_expr_type_conversion (WANT_ARITH | WANT_ENUM, arg, 1)))
	errstring = "wrong type argument to conjugation";
      else if (!noconvert)
	arg = default_conversion (arg);
      break;

    case TRUTH_NOT_EXPR:
      arg = cp_convert (boolean_type_node, arg);
      val = invert_truthvalue (arg);
      if (arg != error_mark_node)
	return val;
      errstring = "in argument to unary !";
      break;

    case NOP_EXPR:
      break;
      
    case REALPART_EXPR:
      if (TREE_CODE (arg) == COMPLEX_CST)
	return TREE_REALPART (arg);
      else if (TREE_CODE (TREE_TYPE (arg)) == COMPLEX_TYPE)
	return fold (build1 (REALPART_EXPR, TREE_TYPE (TREE_TYPE (arg)), arg));
      else
	return arg;

    case IMAGPART_EXPR:
      if (TREE_CODE (arg) == COMPLEX_CST)
	return TREE_IMAGPART (arg);
      else if (TREE_CODE (TREE_TYPE (arg)) == COMPLEX_TYPE)
	return fold (build1 (IMAGPART_EXPR, TREE_TYPE (TREE_TYPE (arg)), arg));
      else
	return cp_convert (TREE_TYPE (arg), integer_zero_node);
      
    case PREINCREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case POSTDECREMENT_EXPR:
      /* Handle complex lvalues (when permitted)
	 by reduction to simpler cases.  */

      val = unary_complex_lvalue (code, arg);
      if (val != 0)
	return val;

      /* Increment or decrement the real part of the value,
	 and don't change the imaginary part.  */
      if (TREE_CODE (TREE_TYPE (arg)) == COMPLEX_TYPE)
	{
	  tree real, imag;

	  arg = stabilize_reference (arg);
	  real = build_unary_op (REALPART_EXPR, arg, 1);
	  imag = build_unary_op (IMAGPART_EXPR, arg, 1);
	  return build (COMPLEX_EXPR, TREE_TYPE (arg),
			build_unary_op (code, real, 1), imag);
	}

      /* Report invalid types.  */

      if (!(arg = build_expr_type_conversion (WANT_ARITH | WANT_POINTER,
					      arg, 1)))
	{
	  if (code == PREINCREMENT_EXPR)
	    errstring ="no pre-increment operator for type";
	  else if (code == POSTINCREMENT_EXPR)
	    errstring ="no post-increment operator for type";
	  else if (code == PREDECREMENT_EXPR)
	    errstring ="no pre-decrement operator for type";
	  else
	    errstring ="no post-decrement operator for type";
	  break;
	}

      /* Report something read-only.  */

      if (CP_TYPE_CONST_P (TREE_TYPE (arg))
	  || TREE_READONLY (arg))
	readonly_error (arg, ((code == PREINCREMENT_EXPR
			       || code == POSTINCREMENT_EXPR)
			      ? "increment" : "decrement"),
			0);

      {
	register tree inc;
	tree result_type = TREE_TYPE (arg);

	arg = get_unwidened (arg, 0);
	argtype = TREE_TYPE (arg);

	/* ARM $5.2.5 last annotation says this should be forbidden.  */
	if (TREE_CODE (argtype) == ENUMERAL_TYPE)
	  pedwarn ("ISO C++ forbids %sing an enum",
		   (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
		   ? "increment" : "decrement");
	    
	/* Compute the increment.  */

	if (TREE_CODE (argtype) == POINTER_TYPE)
	  {
	    enum tree_code tmp = TREE_CODE (TREE_TYPE (argtype));
	    tree type = complete_type (TREE_TYPE (argtype));
	    
	    if (!COMPLETE_OR_VOID_TYPE_P (type))
	      error ("cannot %s a pointer to incomplete type `%T'",
			((code == PREINCREMENT_EXPR
			  || code == POSTINCREMENT_EXPR)
			 ? "increment" : "decrement"), TREE_TYPE (argtype));
	    else if ((pedantic || warn_pointer_arith)
		     && (tmp == FUNCTION_TYPE || tmp == METHOD_TYPE
			 || tmp == VOID_TYPE || tmp == OFFSET_TYPE))
	      pedwarn ("ISO C++ forbids %sing a pointer of type `%T'",
			  ((code == PREINCREMENT_EXPR
			    || code == POSTINCREMENT_EXPR)
			   ? "increment" : "decrement"), argtype);
	    inc = cxx_sizeof_nowarn (TREE_TYPE (argtype));
	  }
	else
	  inc = integer_one_node;

	inc = cp_convert (argtype, inc);

	/* Handle incrementing a cast-expression.  */

	switch (TREE_CODE (arg))
	  {
	  case NOP_EXPR:
	  case CONVERT_EXPR:
	  case FLOAT_EXPR:
	  case FIX_TRUNC_EXPR:
	  case FIX_FLOOR_EXPR:
	  case FIX_ROUND_EXPR:
	  case FIX_CEIL_EXPR:
	    {
	      tree incremented, modify, value, compound;
	      if (! lvalue_p (arg) && pedantic)
		pedwarn ("cast to non-reference type used as lvalue");
	      arg = stabilize_reference (arg);
	      if (code == PREINCREMENT_EXPR || code == PREDECREMENT_EXPR)
		value = arg;
	      else
		value = save_expr (arg);
	      incremented = build (((code == PREINCREMENT_EXPR
				     || code == POSTINCREMENT_EXPR)
				    ? PLUS_EXPR : MINUS_EXPR),
				   argtype, value, inc);

	      modify = build_modify_expr (arg, NOP_EXPR, incremented);
	      compound = build (COMPOUND_EXPR, TREE_TYPE (arg), modify, value);

	      /* Eliminate warning about unused result of + or -.  */
	      TREE_NO_UNUSED_WARNING (compound) = 1;
	      return compound;
	    }

	  default:
	    break;
	  }

	/* Complain about anything else that is not a true lvalue.  */
	if (!lvalue_or_else (arg, ((code == PREINCREMENT_EXPR
				    || code == POSTINCREMENT_EXPR)
				   ? "increment" : "decrement")))
	  return error_mark_node;

	/* Forbid using -- on `bool'.  */
	if (TREE_TYPE (arg) == boolean_type_node)
	  {
	    if (code == POSTDECREMENT_EXPR || code == PREDECREMENT_EXPR)
	      {
		error ("invalid use of `--' on bool variable `%D'", arg);
		return error_mark_node;
	      }
#if 0
	    /* This will only work if someone can convince Kenner to accept
	       my patch to expand_increment. (jason)  */
	    val = build (code, TREE_TYPE (arg), arg, inc);
#else
	    val = boolean_increment (code, arg);
#endif
	  }
	else
	  val = build (code, TREE_TYPE (arg), arg, inc);

	TREE_SIDE_EFFECTS (val) = 1;
	return cp_convert (result_type, val);
      }

    case ADDR_EXPR:
      /* Note that this operation never does default_conversion
	 regardless of NOCONVERT.  */

      argtype = lvalue_type (arg);
      if (TREE_CODE (argtype) == REFERENCE_TYPE)
	{
	  arg = build1
	    (CONVERT_EXPR,
	     build_pointer_type (TREE_TYPE (argtype)), arg);
	  TREE_CONSTANT (arg) = TREE_CONSTANT (TREE_OPERAND (arg, 0));
	  return arg;
	}
      else if (pedantic && DECL_MAIN_P (arg))
	/* ARM $3.4 */
	pedwarn ("ISO C++ forbids taking address of function `::main'");

      /* Let &* cancel out to simplify resulting code.  */
      if (TREE_CODE (arg) == INDIRECT_REF)
	{
	  /* We don't need to have `current_class_ptr' wrapped in a
	     NON_LVALUE_EXPR node.  */
	  if (arg == current_class_ref)
	    return current_class_ptr;

	  arg = TREE_OPERAND (arg, 0);
	  if (TREE_CODE (TREE_TYPE (arg)) == REFERENCE_TYPE)
	    {
	      arg = build1
		(CONVERT_EXPR,
		 build_pointer_type (TREE_TYPE (TREE_TYPE (arg))), arg);
	      TREE_CONSTANT (arg) = TREE_CONSTANT (TREE_OPERAND (arg, 0));
	    }
	  else if (lvalue_p (arg))
	    /* Don't let this be an lvalue.  */
	    return non_lvalue (arg);
	  return arg;
	}

      /* For &x[y], return x+y */
      if (TREE_CODE (arg) == ARRAY_REF)
	{
	  if (!cxx_mark_addressable (TREE_OPERAND (arg, 0)))
	    return error_mark_node;
	  return cp_build_binary_op (PLUS_EXPR, TREE_OPERAND (arg, 0),
				     TREE_OPERAND (arg, 1));
	}

      /* Uninstantiated types are all functions.  Taking the
	 address of a function is a no-op, so just return the
	 argument.  */

      if (TREE_CODE (arg) == IDENTIFIER_NODE
	  && IDENTIFIER_OPNAME_P (arg))
	{
	  abort ();
	  /* We don't know the type yet, so just work around the problem.
	     We know that this will resolve to an lvalue.  */
	  return build1 (ADDR_EXPR, unknown_type_node, arg);
	}

      if (TREE_CODE (arg) == COMPONENT_REF && type_unknown_p (arg)
	  && !really_overloaded_fn (TREE_OPERAND (arg, 1)))
        {
	  /* They're trying to take the address of a unique non-static
	     member function.  This is ill-formed (except in MS-land),
	     but let's try to DTRT.
	     Note: We only handle unique functions here because we don't
	     want to complain if there's a static overload; non-unique
	     cases will be handled by instantiate_type.  But we need to
	     handle this case here to allow casts on the resulting PMF.
	     We could defer this in non-MS mode, but it's easier to give
	     a useful error here.  */

	  tree base = TREE_TYPE (TREE_OPERAND (arg, 0));
	  tree name = DECL_NAME (get_first_fn (TREE_OPERAND (arg, 1)));

	  if (! flag_ms_extensions)
	    {
	      if (current_class_type
		  && TREE_OPERAND (arg, 0) == current_class_ref)
		/* An expression like &memfn.  */
		pedwarn ("ISO C++ forbids taking the address of an unqualified non-static member function to form a pointer to member function.  Say `&%T::%D'", base, name);
	      else
		pedwarn ("ISO C++ forbids taking the address of a bound member function to form a pointer to member function.  Say `&%T::%D'", base, name);
	    }
	  arg = build_offset_ref (base, name);
        }
        
      if (type_unknown_p (arg))
	return build1 (ADDR_EXPR, unknown_type_node, arg);
	
      /* Handle complex lvalues (when permitted)
	 by reduction to simpler cases.  */
      val = unary_complex_lvalue (code, arg);
      if (val != 0)
	return val;

      switch (TREE_CODE (arg))
	{
	case NOP_EXPR:
	case CONVERT_EXPR:
	case FLOAT_EXPR:
	case FIX_TRUNC_EXPR:
	case FIX_FLOOR_EXPR:
	case FIX_ROUND_EXPR:
	case FIX_CEIL_EXPR:
	  if (! lvalue_p (arg) && pedantic)
	    pedwarn ("ISO C++ forbids taking the address of a cast to a non-lvalue expression");
	  break;
	  
	default:
	  break;
	}

      /* Allow the address of a constructor if all the elements
	 are constant.  */
      if (TREE_CODE (arg) == CONSTRUCTOR && TREE_HAS_CONSTRUCTOR (arg)
	  && TREE_CONSTANT (arg))
	;
      /* Anything not already handled and not a true memory reference
	 is an error.  */
      else if (TREE_CODE (argtype) != FUNCTION_TYPE
	       && TREE_CODE (argtype) != METHOD_TYPE
	       && !non_cast_lvalue_or_else (arg, "unary `&'"))
	return error_mark_node;

      if (argtype != error_mark_node)
	argtype = build_pointer_type (argtype);

      {
	tree addr;

	if (TREE_CODE (arg) != COMPONENT_REF)
	  addr = build_address (arg);
	else if (DECL_C_BIT_FIELD (TREE_OPERAND (arg, 1)))
	  {
	    error ("attempt to take address of bit-field structure member `%D'",
		   TREE_OPERAND (arg, 1));
	    return error_mark_node;
	  }
	else
	  {
	    /* Unfortunately we cannot just build an address
	       expression here, because we would not handle
	       address-constant-expressions or offsetof correctly.  */
	    tree field = TREE_OPERAND (arg, 1);
	    tree rval = build_unary_op (ADDR_EXPR, TREE_OPERAND (arg, 0), 0);
	    tree binfo = lookup_base (TREE_TYPE (TREE_TYPE (rval)),
				      decl_type_context (field),
				      ba_check, NULL);
	    
	    rval = build_base_path (PLUS_EXPR, rval, binfo, 1);
	    rval = build1 (NOP_EXPR, argtype, rval);
	    TREE_CONSTANT (rval) = TREE_CONSTANT (TREE_OPERAND (rval, 0));
	    addr = fold (build (PLUS_EXPR, argtype, rval,
				cp_convert (argtype, byte_position (field))));
	  }

	if (TREE_CODE (argtype) == POINTER_TYPE
	    && TREE_CODE (TREE_TYPE (argtype)) == METHOD_TYPE)
	  {
	    build_ptrmemfunc_type (argtype);
	    addr = build_ptrmemfunc (argtype, addr, 0);
	  }

	return addr;
      }

    default:
      break;
    }

  if (!errstring)
    {
      if (argtype == 0)
	argtype = TREE_TYPE (arg);
      return fold (build1 (code, argtype, arg));
    }

  error ("%s", errstring);
  return error_mark_node;
}

/* Apply unary lvalue-demanding operator CODE to the expression ARG
   for certain kinds of expressions which are not really lvalues
   but which we can accept as lvalues.

   If ARG is not a kind of expression we can handle, return zero.  */
   
tree
unary_complex_lvalue (code, arg)
     enum tree_code code;
     tree arg;
{
  /* Handle (a, b) used as an "lvalue".  */
  if (TREE_CODE (arg) == COMPOUND_EXPR)
    {
      tree real_result = build_unary_op (code, TREE_OPERAND (arg, 1), 0);
      return build (COMPOUND_EXPR, TREE_TYPE (real_result),
		    TREE_OPERAND (arg, 0), real_result);
    }

  /* Handle (a ? b : c) used as an "lvalue".  */
  if (TREE_CODE (arg) == COND_EXPR
      || TREE_CODE (arg) == MIN_EXPR || TREE_CODE (arg) == MAX_EXPR)
    return rationalize_conditional_expr (code, arg);

  /* Handle (a = b), (++a), and (--a) used as an "lvalue".  */
  if (TREE_CODE (arg) == MODIFY_EXPR
      || TREE_CODE (arg) == PREINCREMENT_EXPR
      || TREE_CODE (arg) == PREDECREMENT_EXPR)
    {
      tree lvalue = TREE_OPERAND (arg, 0);
      if (TREE_SIDE_EFFECTS (lvalue))
	{
	  lvalue = stabilize_reference (lvalue);
	  arg = build (TREE_CODE (arg), TREE_TYPE (arg),
		       lvalue, TREE_OPERAND (arg, 1));
	}
      return unary_complex_lvalue
	(code, build (COMPOUND_EXPR, TREE_TYPE (lvalue), arg, lvalue));
    }

  if (code != ADDR_EXPR)
    return 0;

  /* Handle (a = b) used as an "lvalue" for `&'.  */
  if (TREE_CODE (arg) == MODIFY_EXPR
      || TREE_CODE (arg) == INIT_EXPR)
    {
      tree real_result = build_unary_op (code, TREE_OPERAND (arg, 0), 0);
      arg = build (COMPOUND_EXPR, TREE_TYPE (real_result), arg, real_result);
      TREE_NO_UNUSED_WARNING (arg) = 1;
      return arg;
    }

  if (TREE_CODE (TREE_TYPE (arg)) == FUNCTION_TYPE
      || TREE_CODE (TREE_TYPE (arg)) == METHOD_TYPE
      || TREE_CODE (TREE_TYPE (arg)) == OFFSET_TYPE)
    {
      /* The representation of something of type OFFSET_TYPE
	 is really the representation of a pointer to it.
	 Here give the representation its true type.  */
      tree t;

      my_friendly_assert (TREE_CODE (arg) != SCOPE_REF, 313);

      if (TREE_CODE (arg) != OFFSET_REF)
	return 0;

      t = TREE_OPERAND (arg, 1);

      /* Check all this code for right semantics.  */	
      if (TREE_CODE (t) == FUNCTION_DECL)
	{
	  if (DECL_DESTRUCTOR_P (t))
	    error ("taking address of destructor");
	  return build_unary_op (ADDR_EXPR, t, 0);
	}
      if (TREE_CODE (t) == VAR_DECL)
	return build_unary_op (ADDR_EXPR, t, 0);
      else
	{
	  tree type;

	  if (TREE_OPERAND (arg, 0)
	      && ! is_dummy_object (TREE_OPERAND (arg, 0))
	      && TREE_CODE (t) != FIELD_DECL)
	    {
	      error ("taking address of bound pointer-to-member expression");
	      return error_mark_node;
	    }
	  if (!PTRMEM_OK_P (arg))
	    {
	      /* This cannot form a pointer to method, so we must
	         resolve the offset ref, and take the address of the
		 result.  For instance,
		 	&(C::m)	      */
	      arg = resolve_offset_ref (arg);

	      return build_unary_op (code, arg, 0);
	    }
	  
	  if (TREE_CODE (TREE_TYPE (t)) == REFERENCE_TYPE)
	    {
	      error ("cannot create pointer to reference member `%D'", t);
	      return error_mark_node;
	    }

	  type = build_ptrmem_type (DECL_FIELD_CONTEXT (t), TREE_TYPE (t));
	  t = make_ptrmem_cst (type, TREE_OPERAND (arg, 1));
	  return t;
	}
    }

  
  /* We permit compiler to make function calls returning
     objects of aggregate type look like lvalues.  */
  {
    tree targ = arg;

    if (TREE_CODE (targ) == SAVE_EXPR)
      targ = TREE_OPERAND (targ, 0);

    if (TREE_CODE (targ) == CALL_EXPR && IS_AGGR_TYPE (TREE_TYPE (targ)))
      {
	if (TREE_CODE (arg) == SAVE_EXPR)
	  targ = arg;
	else
	  targ = build_cplus_new (TREE_TYPE (arg), arg);
	return build1 (ADDR_EXPR, build_pointer_type (TREE_TYPE (arg)), targ);
      }

    if (TREE_CODE (arg) == SAVE_EXPR && TREE_CODE (targ) == INDIRECT_REF)
      return build (SAVE_EXPR, build_pointer_type (TREE_TYPE (arg)),
		     TREE_OPERAND (targ, 0), current_function_decl, NULL);
  }

  /* Don't let anything else be handled specially.  */
  return 0;
}

/* Mark EXP saying that we need to be able to take the
   address of it; it should not be allocated in a register.
   Value is true if successful.

   C++: we do not allow `current_class_ptr' to be addressable.  */

bool
cxx_mark_addressable (exp)
     tree exp;
{
  register tree x = exp;

  while (1)
    switch (TREE_CODE (x))
      {
      case ADDR_EXPR:
      case COMPONENT_REF:
      case ARRAY_REF:
      case REALPART_EXPR:
      case IMAGPART_EXPR:
	x = TREE_OPERAND (x, 0);
	break;

      case PARM_DECL:
	if (x == current_class_ptr)
	  {
            error ("cannot take the address of `this', which is an rvalue expression");
	    TREE_ADDRESSABLE (x) = 1; /* so compiler doesn't die later */
	    return true;
	  }
	/* FALLTHRU */

      case VAR_DECL:
	/* Caller should not be trying to mark initialized
	   constant fields addressable.  */
	my_friendly_assert (DECL_LANG_SPECIFIC (x) == 0
			    || DECL_IN_AGGR_P (x) == 0
			    || TREE_STATIC (x)
			    || DECL_EXTERNAL (x), 314);
	/* FALLTHRU */

      case CONST_DECL:
      case RESULT_DECL:
	if (DECL_REGISTER (x) && !TREE_ADDRESSABLE (x)
	    && !DECL_ARTIFICIAL (x) && extra_warnings)
	  warning ("address requested for `%D', which is declared `register'",
		      x);
	TREE_ADDRESSABLE (x) = 1;
	put_var_into_stack (x, /*rescan=*/true);
	return true;

      case FUNCTION_DECL:
	TREE_ADDRESSABLE (x) = 1;
	TREE_ADDRESSABLE (DECL_ASSEMBLER_NAME (x)) = 1;
	return true;

      case CONSTRUCTOR:
	TREE_ADDRESSABLE (x) = 1;
	return true;

      case TARGET_EXPR:
	TREE_ADDRESSABLE (x) = 1;
	cxx_mark_addressable (TREE_OPERAND (x, 0));
	return true;

      default:
	return true;
    }
}

/* Build and return a conditional expression IFEXP ? OP1 : OP2.  */

tree
build_x_conditional_expr (ifexp, op1, op2)
     tree ifexp, op1, op2;
{
  if (processing_template_decl)
    return build_min_nt (COND_EXPR, ifexp, op1, op2);

  return build_conditional_expr (ifexp, op1, op2);
}

/* Handle overloading of the ',' operator when needed.  Otherwise,
   this function just builds an expression list.  */

tree
build_x_compound_expr (list)
     tree list;
{
  tree rest = TREE_CHAIN (list);
  tree result;

  if (processing_template_decl)
    return build_min_nt (COMPOUND_EXPR, list, NULL_TREE);

  if (rest == NULL_TREE)
    return build_compound_expr (list);

  result = build_opfncall (COMPOUND_EXPR, LOOKUP_NORMAL,
			   TREE_VALUE (list), TREE_VALUE (rest), NULL_TREE);
  if (result)
    return build_x_compound_expr (tree_cons (NULL_TREE, result,
						  TREE_CHAIN (rest)));

  if (! TREE_SIDE_EFFECTS (TREE_VALUE (list)))
    {
      /* FIXME: This test should be in the implicit cast to void of the LHS.  */
      /* the left-hand operand of a comma expression is like an expression
         statement: we should warn if it doesn't have any side-effects,
         unless it was explicitly cast to (void).  */
      if ((extra_warnings || warn_unused_value)
           && !(TREE_CODE (TREE_VALUE(list)) == CONVERT_EXPR
                && VOID_TYPE_P (TREE_TYPE (TREE_VALUE(list)))))
        warning("left-hand operand of comma expression has no effect");
    }
#if 0 /* this requires a gcc backend patch to export warn_if_unused_value */
  else if (warn_unused_value)
    warn_if_unused_value (TREE_VALUE(list));
#endif

  return build_compound_expr
    (tree_cons (NULL_TREE, TREE_VALUE (list),
		     build_tree_list (NULL_TREE,
				      build_x_compound_expr (rest))));
}

/* Given a list of expressions, return a compound expression
   that performs them all and returns the value of the last of them.  */

tree
build_compound_expr (list)
     tree list;
{
  register tree rest;
  tree first;

  TREE_VALUE (list) = decl_constant_value (TREE_VALUE (list));

  if (TREE_CHAIN (list) == 0)
    {
      /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
	 Strip such NOP_EXPRs, since LIST is used in non-lvalue context.  */
      if (TREE_CODE (list) == NOP_EXPR
	  && TREE_TYPE (list) == TREE_TYPE (TREE_OPERAND (list, 0)))
	list = TREE_OPERAND (list, 0);
	
      return TREE_VALUE (list);
    }

  first = TREE_VALUE (list);
  first = convert_to_void (first, "left-hand operand of comma");
  if (first == error_mark_node)
    return error_mark_node;
  
  rest = build_compound_expr (TREE_CHAIN (list));
  if (rest == error_mark_node)
    return error_mark_node;

  /* When pedantic, a compound expression cannot be a constant expression.  */
  if (! TREE_SIDE_EFFECTS (first) && ! pedantic)
    return rest;

  return build (COMPOUND_EXPR, TREE_TYPE (rest), first, rest);
}

tree
build_static_cast (type, expr)
   tree type, expr;
{
  tree intype;
  int ok;

  if (type == error_mark_node || expr == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (expr) == OFFSET_REF)
    expr = resolve_offset_ref (expr);

  if (processing_template_decl)
    {
      tree t = build_min (STATIC_CAST_EXPR, type, expr); 
      return t;
    }

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs if VALUE is being used in non-lvalue context.  */
  if (TREE_CODE (type) != REFERENCE_TYPE
      && TREE_CODE (expr) == NOP_EXPR
      && TREE_TYPE (expr) == TREE_TYPE (TREE_OPERAND (expr, 0)))
    expr = TREE_OPERAND (expr, 0);

  if (TREE_CODE (type) == VOID_TYPE)
    {
      expr = convert_to_void (expr, /*implicit=*/NULL);
      return expr;
    }

  if (TREE_CODE (type) == REFERENCE_TYPE)
    return (convert_from_reference
	    (convert_to_reference (type, expr, CONV_STATIC|CONV_IMPLICIT,
				   LOOKUP_COMPLAIN, NULL_TREE)));

  if (IS_AGGR_TYPE (type))
    return build_cplus_new (type, (build_special_member_call
				   (NULL_TREE, complete_ctor_identifier, 
				    build_tree_list (NULL_TREE, expr),
				    TYPE_BINFO (type), LOOKUP_NORMAL)));
  
  intype = TREE_TYPE (expr);

  /* FIXME handle casting to array type.  */

  ok = 0;
  if (IS_AGGR_TYPE (intype)
      ? can_convert_arg (type, intype, expr)
      : can_convert_arg (strip_all_pointer_quals (type),
                         strip_all_pointer_quals (intype), expr))
    /* This is a standard conversion.  */
    ok = 1;
  else if (TYPE_PTROB_P (type) && TYPE_PTROB_P (intype))
    {
      /* They're pointers to objects. They must be aggregates that
         are related non-virtually.  */
      base_kind kind;
      
      if (IS_AGGR_TYPE (TREE_TYPE (type)) && IS_AGGR_TYPE (TREE_TYPE (intype))
	  && lookup_base (TREE_TYPE (type), TREE_TYPE (intype),
			  ba_ignore | ba_quiet, &kind)
	  && kind != bk_via_virtual)
	ok = 1;
    }
  else if (TYPE_PTRMEM_P (type) && TYPE_PTRMEM_P (intype))
    {
      /* They're pointers to members. The pointed to objects must be
	 the same (ignoring CV qualifiers), and the containing classes
	 must be related non-virtually.  */
      base_kind kind;
      
      if (same_type_p
	  (strip_all_pointer_quals (TREE_TYPE (TREE_TYPE (type))),
	   strip_all_pointer_quals (TREE_TYPE (TREE_TYPE (intype))))
 	  && (lookup_base (TYPE_OFFSET_BASETYPE (TREE_TYPE (intype)),
			   TYPE_OFFSET_BASETYPE (TREE_TYPE (type)),
			   ba_ignore | ba_quiet, &kind))
 	  && kind != bk_via_virtual)
  	ok = 1;
    }
  else if (TREE_CODE (intype) != BOOLEAN_TYPE
	   && TREE_CODE (type) != ARRAY_TYPE
	   && TREE_CODE (type) != FUNCTION_TYPE
	   && can_convert (intype, strip_all_pointer_quals (type)))
    ok = 1;
  else if (TREE_CODE (intype) == ENUMERAL_TYPE
           && TREE_CODE (type) == ENUMERAL_TYPE)
    /* DR 128: "A value of integral _or enumeration_ type can be explicitly
       converted to an enumeration type."
       The integral to enumeration will be accepted by the previous clause.
       We need to explicitly check for enumeration to enumeration.  */
    ok = 1;

  /* [expr.static.cast]

     The static_cast operator shall not be used to cast away
     constness.  */
  if (ok && casts_away_constness (intype, type))
    {
      error ("static_cast from type `%T' to type `%T' casts away constness",
		intype, type);
      return error_mark_node;
    }

  if (ok)
    return build_c_cast (type, expr);

  error ("invalid static_cast from type `%T' to type `%T'", intype, type);
  return error_mark_node;
}

tree
build_reinterpret_cast (type, expr)
   tree type, expr;
{
  tree intype;

  if (type == error_mark_node || expr == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (expr) == OFFSET_REF)
    expr = resolve_offset_ref (expr);

  if (processing_template_decl)
    {
      tree t = build_min (REINTERPRET_CAST_EXPR, type, expr);
      return t;
    }

  if (TREE_CODE (type) != REFERENCE_TYPE)
    {
      expr = decay_conversion (expr);

      /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
	 Strip such NOP_EXPRs if VALUE is being used in non-lvalue context.  */
      if (TREE_CODE (expr) == NOP_EXPR
	  && TREE_TYPE (expr) == TREE_TYPE (TREE_OPERAND (expr, 0)))
	expr = TREE_OPERAND (expr, 0);
    }

  intype = TREE_TYPE (expr);

  if (TREE_CODE (type) == REFERENCE_TYPE)
    {
      if (! real_lvalue_p (expr))
	{
	  error ("invalid reinterpret_cast of an rvalue expression of type `%T' to type `%T'", intype, type);
	  return error_mark_node;
	}
      expr = build_unary_op (ADDR_EXPR, expr, 0);
      if (expr != error_mark_node)
	expr = build_reinterpret_cast
	  (build_pointer_type (TREE_TYPE (type)), expr);
      if (expr != error_mark_node)
	expr = build_indirect_ref (expr, 0);
      return expr;
    }
  else if (same_type_ignoring_top_level_qualifiers_p (intype, type))
    return build_static_cast (type, expr);

  if (TYPE_PTR_P (type) && (TREE_CODE (intype) == INTEGER_TYPE
			    || TREE_CODE (intype) == ENUMERAL_TYPE))
    /* OK */;
  else if (TREE_CODE (type) == INTEGER_TYPE && TYPE_PTR_P (intype))
    {
      if (TYPE_PRECISION (type) < TYPE_PRECISION (intype))
	pedwarn ("reinterpret_cast from `%T' to `%T' loses precision",
		    intype, type);
    }
  else if ((TYPE_PTRFN_P (type) && TYPE_PTRFN_P (intype))
	   || (TYPE_PTRMEMFUNC_P (type) && TYPE_PTRMEMFUNC_P (intype)))
    {
      expr = decl_constant_value (expr);
      return fold (build1 (NOP_EXPR, type, expr));
    }
  else if ((TYPE_PTRMEM_P (type) && TYPE_PTRMEM_P (intype))
	   || (TYPE_PTROBV_P (type) && TYPE_PTROBV_P (intype)))
    {
      if (! comp_ptr_ttypes_reinterpret (TREE_TYPE (type), TREE_TYPE (intype)))
	pedwarn ("reinterpret_cast from `%T' to `%T' casts away const (or volatile)",
		    intype, type);

      expr = decl_constant_value (expr);
      return fold (build1 (NOP_EXPR, type, expr));
    }
  else if ((TYPE_PTRFN_P (type) && TYPE_PTROBV_P (intype))
	   || (TYPE_PTRFN_P (intype) && TYPE_PTROBV_P (type)))
    {
      pedwarn ("ISO C++ forbids casting between pointer-to-function and pointer-to-object");
      expr = decl_constant_value (expr);
      return fold (build1 (NOP_EXPR, type, expr));
    }
  else
    {
      error ("invalid reinterpret_cast from type `%T' to type `%T'",
                intype, type);
      return error_mark_node;
    }
      
  return cp_convert (type, expr);
}

tree
build_const_cast (type, expr)
   tree type, expr;
{
  tree intype;

  if (type == error_mark_node || expr == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (expr) == OFFSET_REF)
    expr = resolve_offset_ref (expr);

  if (processing_template_decl)
    {
      tree t = build_min (CONST_CAST_EXPR, type, expr);
      return t;
    }

  if (!POINTER_TYPE_P (type))
    error ("invalid use of const_cast with type `%T', which is not a pointer, reference, nor a pointer-to-data-member type", type);
  else if (TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE)
    {
      error ("invalid use of const_cast with type `%T', which is a pointer or reference to a function type", type);
      return error_mark_node;
    }

  if (TREE_CODE (type) != REFERENCE_TYPE)
    {
      expr = decay_conversion (expr);

      /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
	 Strip such NOP_EXPRs if VALUE is being used in non-lvalue context.  */
      if (TREE_CODE (expr) == NOP_EXPR
	  && TREE_TYPE (expr) == TREE_TYPE (TREE_OPERAND (expr, 0)))
	expr = TREE_OPERAND (expr, 0);
    }

  intype = TREE_TYPE (expr);
  
  if (same_type_ignoring_top_level_qualifiers_p (intype, type))
    return build_static_cast (type, expr);
  else if (TREE_CODE (type) == REFERENCE_TYPE)
    {
      if (! real_lvalue_p (expr))
	{
	  error ("invalid const_cast of an rvalue of type `%T' to type `%T'", intype, type);
	  return error_mark_node;
	}

      if (comp_ptr_ttypes_const (TREE_TYPE (type), intype))
	{
	  expr = build_unary_op (ADDR_EXPR, expr, 0);
	  expr = build1 (NOP_EXPR, type, expr);
	  return convert_from_reference (expr);
	}
    }
  else if (TREE_CODE (type) == POINTER_TYPE
	   && TREE_CODE (intype) == POINTER_TYPE
	   && comp_ptr_ttypes_const (TREE_TYPE (type), TREE_TYPE (intype)))
    return cp_convert (type, expr);

  error ("invalid const_cast from type `%T' to type `%T'", intype, type);
  return error_mark_node;
}

/* Build an expression representing a cast to type TYPE of expression EXPR.

   ALLOW_NONCONVERTING is true if we should allow non-converting constructors
   when doing the cast.  */

tree
build_c_cast (type, expr)
     tree type, expr;
{
  register tree value = expr;
  tree otype;

  if (type == error_mark_node || expr == error_mark_node)
    return error_mark_node;

  if (processing_template_decl)
    {
      tree t = build_min (CAST_EXPR, type,
			  tree_cons (NULL_TREE, value, NULL_TREE));
      return t;
    }

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs if VALUE is being used in non-lvalue context.  */
  if (TREE_CODE (type) != REFERENCE_TYPE
      && TREE_CODE (value) == NOP_EXPR
      && TREE_TYPE (value) == TREE_TYPE (TREE_OPERAND (value, 0)))
    value = TREE_OPERAND (value, 0);

  if (TREE_CODE (value) == OFFSET_REF)
    value = resolve_offset_ref (value);

  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      /* Allow casting from T1* to T2[] because Cfront allows it.
	 NIHCL uses it. It is not valid ISO C++ however.  */
      if (TREE_CODE (TREE_TYPE (expr)) == POINTER_TYPE)
	{
	  pedwarn ("ISO C++ forbids casting to an array type `%T'", type);
	  type = build_pointer_type (TREE_TYPE (type));
	}
      else
	{
	  error ("ISO C++ forbids casting to an array type `%T'", type);
	  return error_mark_node;
	}
    }

  if (TREE_CODE (type) == FUNCTION_TYPE
      || TREE_CODE (type) == METHOD_TYPE)
    {
      error ("invalid cast to function type `%T'", type);
      return error_mark_node;
    }

  if (TREE_CODE (type) == VOID_TYPE)
    {
      /* Conversion to void does not cause any of the normal function to
       * pointer, array to pointer and lvalue to rvalue decays.  */
      
      value = convert_to_void (value, /*implicit=*/NULL);
      return value;
    }
  /* Convert functions and arrays to pointers and
     convert references to their expanded types,
     but don't convert any other types.  If, however, we are
     casting to a class type, there's no reason to do this: the
     cast will only succeed if there is a converting constructor,
     and the default conversions will be done at that point.  In
     fact, doing the default conversion here is actually harmful
     in cases like this:

     typedef int A[2];
     struct S { S(const A&); };

     since we don't want the array-to-pointer conversion done.  */
  if (!IS_AGGR_TYPE (type))
    {
      if (TREE_CODE (TREE_TYPE (value)) == FUNCTION_TYPE
	  || (TREE_CODE (TREE_TYPE (value)) == METHOD_TYPE
	      /* Don't do the default conversion on a ->* expression.  */
	      && ! (TREE_CODE (type) == POINTER_TYPE
		    && bound_pmf_p (value)))
	  || TREE_CODE (TREE_TYPE (value)) == ARRAY_TYPE
	  || TREE_CODE (TREE_TYPE (value)) == REFERENCE_TYPE)
	value = default_conversion (value);
    }
  else if (TREE_CODE (TREE_TYPE (value)) == REFERENCE_TYPE)
    /* However, even for class types, we still need to strip away
       the reference type, since the call to convert_force below
       does not expect the input expression to be of reference
       type.  */
    value = convert_from_reference (value);
	
  otype = TREE_TYPE (value);

  /* Optionally warn about potentially worrisome casts.  */

  if (warn_cast_qual
      && TREE_CODE (type) == POINTER_TYPE
      && TREE_CODE (otype) == POINTER_TYPE
      && !at_least_as_qualified_p (TREE_TYPE (type),
				   TREE_TYPE (otype)))
    warning ("cast from `%T' to `%T' discards qualifiers from pointer target type",
                otype, type);

  if (TREE_CODE (type) == INTEGER_TYPE
      && TREE_CODE (otype) == POINTER_TYPE
      && TYPE_PRECISION (type) != TYPE_PRECISION (otype))
    warning ("cast from pointer to integer of different size");

  if (TREE_CODE (type) == POINTER_TYPE
      && TREE_CODE (otype) == INTEGER_TYPE
      && TYPE_PRECISION (type) != TYPE_PRECISION (otype)
      /* Don't warn about converting any constant.  */
      && !TREE_CONSTANT (value))
    warning ("cast to pointer from integer of different size");

  if (TREE_CODE (type) == REFERENCE_TYPE)
    value = (convert_from_reference
	     (convert_to_reference (type, value, CONV_C_CAST,
				    LOOKUP_COMPLAIN, NULL_TREE)));
  else
    {
      tree ovalue;

      value = decl_constant_value (value);

      ovalue = value;
      value = convert_force (type, value, CONV_C_CAST);

      /* Ignore any integer overflow caused by the cast.  */
      if (TREE_CODE (value) == INTEGER_CST)
	{
	  TREE_OVERFLOW (value) = TREE_OVERFLOW (ovalue);
	  TREE_CONSTANT_OVERFLOW (value) = TREE_CONSTANT_OVERFLOW (ovalue);
	}
    }

  /* Warn about possible alignment problems.  Do this here when we will have
     instantiated any necessary template types.  */
  if (STRICT_ALIGNMENT && warn_cast_align
      && TREE_CODE (type) == POINTER_TYPE
      && TREE_CODE (otype) == POINTER_TYPE
      && TREE_CODE (TREE_TYPE (otype)) != VOID_TYPE
      && TREE_CODE (TREE_TYPE (otype)) != FUNCTION_TYPE
      && COMPLETE_TYPE_P (TREE_TYPE (otype))
      && COMPLETE_TYPE_P (TREE_TYPE (type))
      && TYPE_ALIGN (TREE_TYPE (type)) > TYPE_ALIGN (TREE_TYPE (otype)))
    warning ("cast from `%T' to `%T' increases required alignment of target type",
                otype, type);

    /* Always produce some operator for an explicit cast,
       so we can tell (for -pedantic) that the cast is no lvalue.  */
  if (TREE_CODE (type) != REFERENCE_TYPE && value == expr
      && real_lvalue_p (value))
    value = non_lvalue (value);

  return value;
}

/* Build an assignment expression of lvalue LHS from value RHS.
   MODIFYCODE is the code for a binary operator that we use
   to combine the old value of LHS with RHS to get the new value.
   Or else MODIFYCODE is NOP_EXPR meaning do a simple assignment.

   C++: If MODIFYCODE is INIT_EXPR, then leave references unbashed.  */

tree
build_modify_expr (lhs, modifycode, rhs)
     tree lhs;
     enum tree_code modifycode;
     tree rhs;
{
  register tree result;
  tree newrhs = rhs;
  tree lhstype = TREE_TYPE (lhs);
  tree olhstype = lhstype;
  tree olhs = lhs;

  /* Avoid duplicate error messages from operands that had errors.  */
  if (lhs == error_mark_node || rhs == error_mark_node)
    return error_mark_node;

  /* Handle control structure constructs used as "lvalues".  */
  switch (TREE_CODE (lhs))
    {
      /* Handle --foo = 5; as these are valid constructs in C++ */
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
      if (TREE_SIDE_EFFECTS (TREE_OPERAND (lhs, 0)))
	lhs = build (TREE_CODE (lhs), TREE_TYPE (lhs),
		     stabilize_reference (TREE_OPERAND (lhs, 0)),
		     TREE_OPERAND (lhs, 1));
      return build (COMPOUND_EXPR, lhstype,
		    lhs,
		    build_modify_expr (TREE_OPERAND (lhs, 0),
				       modifycode, rhs));

      /* Handle (a, b) used as an "lvalue".  */
    case COMPOUND_EXPR:
      newrhs = build_modify_expr (TREE_OPERAND (lhs, 1),
				  modifycode, rhs);
      if (newrhs == error_mark_node)
	return error_mark_node;
      return build (COMPOUND_EXPR, lhstype,
		    TREE_OPERAND (lhs, 0), newrhs);

    case MODIFY_EXPR:
      newrhs = build_modify_expr (TREE_OPERAND (lhs, 0), modifycode, rhs);
      if (newrhs == error_mark_node)
	return error_mark_node;
      return build (COMPOUND_EXPR, lhstype, lhs, newrhs);

      /* Handle (a ? b : c) used as an "lvalue".  */
    case COND_EXPR:
      {
	/* Produce (a ? (b = rhs) : (c = rhs))
	   except that the RHS goes through a save-expr
	   so the code to compute it is only emitted once.  */
	tree cond;
	tree preeval = NULL_TREE;

	rhs = stabilize_expr (rhs, &preeval);
	
	/* Check this here to avoid odd errors when trying to convert
	   a throw to the type of the COND_EXPR.  */
	if (!lvalue_or_else (lhs, "assignment"))
	  return error_mark_node;

	cond = build_conditional_expr
	  (TREE_OPERAND (lhs, 0),
	   build_modify_expr (cp_convert (TREE_TYPE (lhs),
					  TREE_OPERAND (lhs, 1)),
			      modifycode, rhs),
	   build_modify_expr (cp_convert (TREE_TYPE (lhs),
					  TREE_OPERAND (lhs, 2)),
			      modifycode, rhs));

	if (cond == error_mark_node)
	  return cond;
	/* Make sure the code to compute the rhs comes out
	   before the split.  */
	return build (COMPOUND_EXPR, TREE_TYPE (lhs), preeval, cond);
      }
      
    case OFFSET_REF:
      lhs = resolve_offset_ref (lhs);
      if (lhs == error_mark_node)
	return error_mark_node;
      olhstype = lhstype = TREE_TYPE (lhs);
    
    default:
      break;
    }

  if (modifycode == INIT_EXPR)
    {
      if (TREE_CODE (rhs) == CONSTRUCTOR)
	{
	  my_friendly_assert (same_type_p (TREE_TYPE (rhs), lhstype),
			      20011220);
	  result = build (INIT_EXPR, lhstype, lhs, rhs);
	  TREE_SIDE_EFFECTS (result) = 1;
	  return result;
	}
      else if (! IS_AGGR_TYPE (lhstype))
	/* Do the default thing */;
      else
	{
	  result = build_special_member_call (lhs, complete_ctor_identifier,
					      build_tree_list (NULL_TREE, rhs),
					      TYPE_BINFO (lhstype), 
					      LOOKUP_NORMAL);
	  if (result == NULL_TREE)
	    return error_mark_node;
	  return result;
	}
    }
  else
    {
      if (TREE_CODE (lhstype) == REFERENCE_TYPE)
	{
	  lhs = convert_from_reference (lhs);
	  olhstype = lhstype = TREE_TYPE (lhs);
	}
      lhs = require_complete_type (lhs);
      if (lhs == error_mark_node)
	return error_mark_node;

      if (modifycode == NOP_EXPR)
	{
	  /* `operator=' is not an inheritable operator.  */
	  if (! IS_AGGR_TYPE (lhstype))
	    /* Do the default thing */;
	  else
	    {
	      result = build_opfncall (MODIFY_EXPR, LOOKUP_NORMAL,
				       lhs, rhs, make_node (NOP_EXPR));
	      if (result == NULL_TREE)
		return error_mark_node;
	      return result;
	    }
	  lhstype = olhstype;
	}
      else
	{
	  /* A binary op has been requested.  Combine the old LHS
     	     value with the RHS producing the value we should actually
     	     store into the LHS.  */

	  my_friendly_assert (!PROMOTES_TO_AGGR_TYPE (lhstype, REFERENCE_TYPE),
			      978652);
	  lhs = stabilize_reference (lhs);
	  newrhs = cp_build_binary_op (modifycode, lhs, rhs);
	  if (newrhs == error_mark_node)
	    {
	      error ("  in evaluation of `%Q(%#T, %#T)'", modifycode,
		     TREE_TYPE (lhs), TREE_TYPE (rhs));
	      return error_mark_node;
	    }
	  
	  /* Now it looks like a plain assignment.  */
	  modifycode = NOP_EXPR;
	}
      my_friendly_assert (TREE_CODE (lhstype) != REFERENCE_TYPE, 20011220);
      my_friendly_assert (TREE_CODE (TREE_TYPE (newrhs)) != REFERENCE_TYPE,
			  20011220);
    }

  /* Handle a cast used as an "lvalue".
     We have already performed any binary operator using the value as cast.
     Now convert the result to the cast type of the lhs,
     and then true type of the lhs and store it there;
     then convert result back to the cast type to be the value
     of the assignment.  */

  switch (TREE_CODE (lhs))
    {
    case NOP_EXPR:
    case CONVERT_EXPR:
    case FLOAT_EXPR:
    case FIX_TRUNC_EXPR:
    case FIX_FLOOR_EXPR:
    case FIX_ROUND_EXPR:
    case FIX_CEIL_EXPR:
      {
	tree inner_lhs = TREE_OPERAND (lhs, 0);
	tree result;

	if (TREE_CODE (TREE_TYPE (newrhs)) == ARRAY_TYPE
	    || TREE_CODE (TREE_TYPE (newrhs)) == FUNCTION_TYPE
	    || TREE_CODE (TREE_TYPE (newrhs)) == METHOD_TYPE
	    || TREE_CODE (TREE_TYPE (newrhs)) == OFFSET_TYPE)
	  newrhs = default_conversion (newrhs);
	
	/* ISO C++ 5.4/1: The result is an lvalue if T is a reference
	   type, otherwise the result is an rvalue.  */
	if (! lvalue_p (lhs))
	  pedwarn ("ISO C++ forbids cast to non-reference type used as lvalue");

	result = build_modify_expr (inner_lhs, NOP_EXPR,
				    cp_convert (TREE_TYPE (inner_lhs),
						cp_convert (lhstype, newrhs)));
	if (result == error_mark_node)
	  return result;
	return cp_convert (TREE_TYPE (lhs), result);
      }

    default:
      break;
    }

  /* Now we have handled acceptable kinds of LHS that are not truly lvalues.
     Reject anything strange now.  */

  if (!lvalue_or_else (lhs, "assignment"))
    return error_mark_node;

  /* Warn about modifying something that is `const'.  Don't warn if
     this is initialization.  */
  if (modifycode != INIT_EXPR
      && (TREE_READONLY (lhs) || CP_TYPE_CONST_P (lhstype)
	  /* Functions are not modifiable, even though they are
	     lvalues.  */
	  || TREE_CODE (TREE_TYPE (lhs)) == FUNCTION_TYPE
	  || TREE_CODE (TREE_TYPE (lhs)) == METHOD_TYPE
	  /* If it's an aggregate and any field is const, then it is
	     effectively const.  */
	  || (IS_AGGR_TYPE_CODE (TREE_CODE (lhstype))
	      && C_TYPE_FIELDS_READONLY (lhstype))))
    readonly_error (lhs, "assignment", 0);

  /* If storing into a structure or union member, it has probably been
     given type `int'.  Compute the type that would go with the actual
     amount of storage the member occupies.  */

  if (TREE_CODE (lhs) == COMPONENT_REF
      && (TREE_CODE (lhstype) == INTEGER_TYPE
	  || TREE_CODE (lhstype) == REAL_TYPE
	  || TREE_CODE (lhstype) == ENUMERAL_TYPE))
    {
      lhstype = TREE_TYPE (get_unwidened (lhs, 0));

      /* If storing in a field that is in actuality a short or narrower
	 than one, we must store in the field in its actual type.  */

      if (lhstype != TREE_TYPE (lhs))
	{
	  lhs = copy_node (lhs);
	  TREE_TYPE (lhs) = lhstype;
	}
    }

  if (TREE_CODE (lhstype) != REFERENCE_TYPE)
    {
      if (TREE_SIDE_EFFECTS (lhs))
	lhs = stabilize_reference (lhs);
      if (TREE_SIDE_EFFECTS (newrhs))
	newrhs = stabilize_reference (newrhs);
    }

  /* Convert new value to destination type.  */

  if (TREE_CODE (lhstype) == ARRAY_TYPE)
    {
      int from_array;
      
      if (!same_or_base_type_p (TYPE_MAIN_VARIANT (lhstype),
				TYPE_MAIN_VARIANT (TREE_TYPE (rhs))))
	{
	  error ("incompatible types in assignment of `%T' to `%T'",
		 TREE_TYPE (rhs), lhstype);
	  return error_mark_node;
	}

      /* Allow array assignment in compiler-generated code.  */
      if (! DECL_ARTIFICIAL (current_function_decl))
	pedwarn ("ISO C++ forbids assignment of arrays");

      from_array = TREE_CODE (TREE_TYPE (newrhs)) == ARRAY_TYPE
	           ? 1 + (modifycode != INIT_EXPR): 0;
      return build_vec_init (lhs, NULL_TREE, newrhs, from_array);
    }

  if (modifycode == INIT_EXPR)
    newrhs = convert_for_initialization (lhs, lhstype, newrhs, LOOKUP_NORMAL,
					 "initialization", NULL_TREE, 0);
  else
    {
      /* Avoid warnings on enum bit fields.  */
      if (TREE_CODE (olhstype) == ENUMERAL_TYPE
	  && TREE_CODE (lhstype) == INTEGER_TYPE)
	{
	  newrhs = convert_for_assignment (olhstype, newrhs, "assignment",
					   NULL_TREE, 0);
	  newrhs = convert_force (lhstype, newrhs, 0);
	}
      else
	newrhs = convert_for_assignment (lhstype, newrhs, "assignment",
					 NULL_TREE, 0);
      if (TREE_CODE (newrhs) == CALL_EXPR
	  && TYPE_NEEDS_CONSTRUCTING (lhstype))
	newrhs = build_cplus_new (lhstype, newrhs);

      /* Can't initialize directly from a TARGET_EXPR, since that would
	 cause the lhs to be constructed twice, and possibly result in
	 accidental self-initialization.  So we force the TARGET_EXPR to be
	 expanded without a target.  */
      if (TREE_CODE (newrhs) == TARGET_EXPR)
	newrhs = build (COMPOUND_EXPR, TREE_TYPE (newrhs), newrhs,
			TREE_OPERAND (newrhs, 0));
    }

  if (newrhs == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (newrhs) == COND_EXPR)
    {
      tree lhs1;
      tree cond = TREE_OPERAND (newrhs, 0);

      if (TREE_SIDE_EFFECTS (lhs))
	cond = build_compound_expr (tree_cons
				    (NULL_TREE, lhs,
				     build_tree_list (NULL_TREE, cond)));

      /* Cannot have two identical lhs on this one tree (result) as preexpand
	 calls will rip them out and fill in RTL for them, but when the
	 rtl is generated, the calls will only be in the first side of the
	 condition, not on both, or before the conditional jump! (mrs) */
      lhs1 = break_out_calls (lhs);

      if (lhs == lhs1)
	/* If there's no change, the COND_EXPR behaves like any other rhs.  */
	result = build (modifycode == NOP_EXPR ? MODIFY_EXPR : INIT_EXPR,
			lhstype, lhs, newrhs);
      else
	{
	  tree result_type = TREE_TYPE (newrhs);
	  /* We have to convert each arm to the proper type because the
	     types may have been munged by constant folding.  */
	  result
	    = build (COND_EXPR, result_type, cond,
		     build_modify_expr (lhs, modifycode,
					cp_convert (result_type,
						    TREE_OPERAND (newrhs, 1))),
		     build_modify_expr (lhs1, modifycode,
					cp_convert (result_type,
						    TREE_OPERAND (newrhs, 2))));
	}
    }
  else
    result = build (modifycode == NOP_EXPR ? MODIFY_EXPR : INIT_EXPR,
		    lhstype, lhs, newrhs);

  TREE_SIDE_EFFECTS (result) = 1;

  /* If we got the LHS in a different type for storing in,
     convert the result back to the nominal type of LHS
     so that the value we return always has the same type
     as the LHS argument.  */

  if (olhstype == TREE_TYPE (result))
    return result;
  /* Avoid warnings converting integral types back into enums
     for enum bit fields.  */
  if (TREE_CODE (TREE_TYPE (result)) == INTEGER_TYPE
      && TREE_CODE (olhstype) == ENUMERAL_TYPE)
    {
      result = build (COMPOUND_EXPR, olhstype, result, olhs);
      TREE_NO_UNUSED_WARNING (result) = 1;
      return result;
    }
  return convert_for_assignment (olhstype, result, "assignment",
				 NULL_TREE, 0);
}

tree
build_x_modify_expr (lhs, modifycode, rhs)
     tree lhs;
     enum tree_code modifycode;
     tree rhs;
{
  if (processing_template_decl)
    return build_min_nt (MODOP_EXPR, lhs,
			 build_min_nt (modifycode, NULL_TREE, NULL_TREE), rhs);

  if (modifycode != NOP_EXPR)
    {
      tree rval = build_opfncall (MODIFY_EXPR, LOOKUP_NORMAL, lhs, rhs,
				  make_node (modifycode));
      if (rval)
	return rval;
    }
  return build_modify_expr (lhs, modifycode, rhs);
}


/* Get difference in deltas for different pointer to member function
   types.  Return integer_zero_node, if FROM cannot be converted to a
   TO type.  If FORCE is true, then allow reverse conversions as well.

   Note that the naming of FROM and TO is kind of backwards; the return
   value is what we add to a TO in order to get a FROM.  They are named
   this way because we call this function to find out how to convert from
   a pointer to member of FROM to a pointer to member of TO.  */

static tree
get_delta_difference (from, to, force)
     tree from, to;
     int force;
{
  tree delta = integer_zero_node;
  tree binfo;
  tree virt_binfo;
  base_kind kind;
  
  binfo = lookup_base (to, from, ba_check, &kind);
  if (kind == bk_inaccessible || kind == bk_ambig)
    {
      error ("   in pointer to member function conversion");
      return delta;
    }
  if (!binfo)
    {
      if (!force)
	{
	  error_not_base_type (from, to);
	  error ("   in pointer to member conversion");
	  return delta;
	}
      binfo = lookup_base (from, to, ba_check, &kind);
      if (binfo == 0)
	return delta;
      virt_binfo = binfo_from_vbase (binfo);
      
      if (virt_binfo)
        {
          /* This is a reinterpret cast, we choose to do nothing.  */
          warning ("pointer to member cast via virtual base `%T' of `%T'",
	              BINFO_TYPE (virt_binfo),
	              BINFO_TYPE (BINFO_INHERITANCE_CHAIN (virt_binfo)));
          return delta;
        }
      delta = BINFO_OFFSET (binfo);
      delta = cp_convert (ptrdiff_type_node, delta);
      delta = cp_build_binary_op (MINUS_EXPR,
				 integer_zero_node,
				 delta);

      return delta;
    }

  virt_binfo = binfo_from_vbase (binfo);
  if (virt_binfo)
    {
      /* This is a reinterpret cast, we choose to do nothing.  */
      if (force)
        warning ("pointer to member cast via virtual base `%T' of `%T'",
                    BINFO_TYPE (virt_binfo),
                    BINFO_TYPE (BINFO_INHERITANCE_CHAIN (virt_binfo)));
      else
	error ("pointer to member conversion via virtual base `%T' of `%T'",
		  BINFO_TYPE (virt_binfo),
                  BINFO_TYPE (BINFO_INHERITANCE_CHAIN (virt_binfo)));
      return delta;
    }
  delta = BINFO_OFFSET (binfo);

  return cp_convert (ptrdiff_type_node, delta);
}

/* Return a constructor for the pointer-to-member-function TYPE using
   the other components as specified.  */

tree
build_ptrmemfunc1 (type, delta, pfn)
     tree type, delta, pfn;
{
  tree u = NULL_TREE;
  tree delta_field;
  tree pfn_field;

  /* Pull the FIELD_DECLs out of the type.  */
  pfn_field = TYPE_FIELDS (type);
  delta_field = TREE_CHAIN (pfn_field);

  /* Make sure DELTA has the type we want.  */
  delta = convert_and_check (delta_type_node, delta);

  /* Finish creating the initializer.  */
  u = tree_cons (pfn_field, pfn,
		 build_tree_list (delta_field, delta));
  u = build (CONSTRUCTOR, type, NULL_TREE, u);
  TREE_CONSTANT (u) = TREE_CONSTANT (pfn) && TREE_CONSTANT (delta);
  TREE_STATIC (u) = (TREE_CONSTANT (u)
		     && (initializer_constant_valid_p (pfn, TREE_TYPE (pfn))
			 != NULL_TREE)
		     && (initializer_constant_valid_p (delta, TREE_TYPE (delta)) 
			 != NULL_TREE));
  return u;
}

/* Build a constructor for a pointer to member function.  It can be
   used to initialize global variables, local variable, or used
   as a value in expressions.  TYPE is the POINTER to METHOD_TYPE we
   want to be.

   If FORCE is nonzero, then force this conversion, even if
   we would rather not do it.  Usually set when using an explicit
   cast.

   Return error_mark_node, if something goes wrong.  */

tree
build_ptrmemfunc (type, pfn, force)
     tree type, pfn;
     int force;
{
  tree fn;
  tree pfn_type;
  tree to_type;

  if (error_operand_p (pfn))
    return error_mark_node;

  pfn_type = TREE_TYPE (pfn);
  to_type = build_ptrmemfunc_type (type);

  /* Handle multiple conversions of pointer to member functions.  */
  if (TYPE_PTRMEMFUNC_P (pfn_type))
    {
      tree delta = NULL_TREE;
      tree npfn = NULL_TREE;
      tree n;

      if (!force 
	  && !can_convert_arg (to_type, TREE_TYPE (pfn), pfn))
	error ("invalid conversion to type `%T' from type `%T'", 
		  to_type, pfn_type);

      n = get_delta_difference (TYPE_PTRMEMFUNC_OBJECT_TYPE (pfn_type),
				TYPE_PTRMEMFUNC_OBJECT_TYPE (to_type),
				force);

      /* We don't have to do any conversion to convert a
	 pointer-to-member to its own type.  But, we don't want to
	 just return a PTRMEM_CST if there's an explicit cast; that
	 cast should make the expression an invalid template argument.  */
      if (TREE_CODE (pfn) != PTRMEM_CST)
	{
	  if (same_type_p (to_type, pfn_type))
	    return pfn;
	  else if (integer_zerop (n))
	    return build_reinterpret_cast (to_type, pfn);
	}

      if (TREE_SIDE_EFFECTS (pfn))
	pfn = save_expr (pfn);

      /* Obtain the function pointer and the current DELTA.  */
      if (TREE_CODE (pfn) == PTRMEM_CST)
	expand_ptrmemfunc_cst (pfn, &delta, &npfn);
      else
	{
	  npfn = build_ptrmemfunc_access_expr (pfn, pfn_identifier);
	  delta = build_ptrmemfunc_access_expr (pfn, delta_identifier);
	}

      /* Just adjust the DELTA field.  */
      delta = cp_convert (ptrdiff_type_node, delta);
      if (TARGET_PTRMEMFUNC_VBIT_LOCATION == ptrmemfunc_vbit_in_delta)
	n = cp_build_binary_op (LSHIFT_EXPR, n, integer_one_node);
      delta = cp_build_binary_op (PLUS_EXPR, delta, n);
      return build_ptrmemfunc1 (to_type, delta, npfn);
    }

  /* Handle null pointer to member function conversions.  */
  if (integer_zerop (pfn))
    {
      pfn = build_c_cast (type, integer_zero_node);
      return build_ptrmemfunc1 (to_type,
				integer_zero_node, 
				pfn);
    }

  if (type_unknown_p (pfn))
    return instantiate_type (type, pfn, tf_error | tf_warning);

  fn = TREE_OPERAND (pfn, 0);
  my_friendly_assert (TREE_CODE (fn) == FUNCTION_DECL, 0);
  return make_ptrmem_cst (to_type, fn);
}

/* Return the DELTA, IDX, PFN, and DELTA2 values for the PTRMEM_CST
   given by CST.

   ??? There is no consistency as to the types returned for the above
   values.  Some code acts as if its a sizetype and some as if its
   integer_type_node.  */

void
expand_ptrmemfunc_cst (cst, delta, pfn)
     tree cst;
     tree *delta;
     tree *pfn;
{
  tree type = TREE_TYPE (cst);
  tree fn = PTRMEM_CST_MEMBER (cst);
  tree ptr_class, fn_class;

  my_friendly_assert (TREE_CODE (fn) == FUNCTION_DECL, 0);

  /* The class that the function belongs to.  */
  fn_class = DECL_CONTEXT (fn);

  /* The class that we're creating a pointer to member of.  */
  ptr_class = TYPE_PTRMEMFUNC_OBJECT_TYPE (type);

  /* First, calculate the adjustment to the function's class.  */
  *delta = get_delta_difference (fn_class, ptr_class, /*force=*/0);

  if (!DECL_VIRTUAL_P (fn))
    *pfn = convert (TYPE_PTRMEMFUNC_FN_TYPE (type), build_addr_func (fn));
  else
    {
      /* If we're dealing with a virtual function, we have to adjust 'this'
         again, to point to the base which provides the vtable entry for
         fn; the call will do the opposite adjustment.  */
      tree orig_class = DECL_CONTEXT (fn);
      tree binfo = binfo_or_else (orig_class, fn_class);
      *delta = fold (build (PLUS_EXPR, TREE_TYPE (*delta),
			    *delta, BINFO_OFFSET (binfo)));

      /* We set PFN to the vtable offset at which the function can be
	 found, plus one (unless ptrmemfunc_vbit_in_delta, in which
	 case delta is shifted left, and then incremented).  */
      *pfn = DECL_VINDEX (fn);
      *pfn = fold (build (MULT_EXPR, integer_type_node, *pfn,
			  TYPE_SIZE_UNIT (vtable_entry_type)));

      switch (TARGET_PTRMEMFUNC_VBIT_LOCATION)
	{
	case ptrmemfunc_vbit_in_pfn:
	  *pfn = fold (build (PLUS_EXPR, integer_type_node, *pfn,
			      integer_one_node));
	  break;

	case ptrmemfunc_vbit_in_delta:
	  *delta = fold (build (LSHIFT_EXPR, TREE_TYPE (*delta),
				*delta, integer_one_node));
	  *delta = fold (build (PLUS_EXPR, TREE_TYPE (*delta),
				*delta, integer_one_node));
	  break;

	default:
	  abort ();
	}

      *pfn = fold (build1 (NOP_EXPR, TYPE_PTRMEMFUNC_FN_TYPE (type),
			   *pfn));
    }
}

/* Return an expression for PFN from the pointer-to-member function
   given by T.  */

tree
pfn_from_ptrmemfunc (t)
     tree t;
{
  if (TREE_CODE (t) == PTRMEM_CST)
    {
      tree delta;
      tree pfn;
      
      expand_ptrmemfunc_cst (t, &delta, &pfn);
      if (pfn)
	return pfn;
    }

  return build_ptrmemfunc_access_expr (t, pfn_identifier);
}

/* Expression EXPR is about to be implicitly converted to TYPE.  Warn
   if this is a potentially dangerous thing to do.  Returns a possibly
   marked EXPR.  */

tree
dubious_conversion_warnings (type, expr, errtype, fndecl, parmnum)
     tree type;
     tree expr;
     const char *errtype;
     tree fndecl;
     int parmnum;
{
  if (TREE_CODE (type) == REFERENCE_TYPE)
    type = TREE_TYPE (type);
  
  /* Issue warnings about peculiar, but valid, uses of NULL.  */
  if (ARITHMETIC_TYPE_P (type) && expr == null_node)
    {
      if (fndecl)
        warning ("passing NULL used for non-pointer %s %P of `%D'",
                    errtype, parmnum, fndecl);
      else
        warning ("%s to non-pointer type `%T' from NULL", errtype, type);
    }
  
  /* Warn about assigning a floating-point type to an integer type.  */
  if (TREE_CODE (TREE_TYPE (expr)) == REAL_TYPE
      && TREE_CODE (type) == INTEGER_TYPE)
    {
      if (fndecl)
	warning ("passing `%T' for %s %P of `%D'",
		    TREE_TYPE (expr), errtype, parmnum, fndecl);
      else
	warning ("%s to `%T' from `%T'", errtype, type, TREE_TYPE (expr));
    }
  /* And warn about assigning a negative value to an unsigned
     variable.  */
  else if (TREE_UNSIGNED (type) && TREE_CODE (type) != BOOLEAN_TYPE)
    {
      if (TREE_CODE (expr) == INTEGER_CST
	  && TREE_NEGATED_INT (expr))
	{
	  if (fndecl)
	    warning ("passing negative value `%E' for %s %P of `%D'",
			expr, errtype, parmnum, fndecl);
	  else
	    warning ("%s of negative value `%E' to `%T'",
			errtype, expr, type);
	}

      overflow_warning (expr);

      if (TREE_CONSTANT (expr))
	expr = fold (expr);
    }
  return expr;
}

/* Convert value RHS to type TYPE as preparation for an assignment to
   an lvalue of type TYPE.  ERRTYPE is a string to use in error
   messages: "assignment", "return", etc.  If FNDECL is non-NULL, we
   are doing the conversion in order to pass the PARMNUMth argument of
   FNDECL.  */

static tree
convert_for_assignment (type, rhs, errtype, fndecl, parmnum)
     tree type, rhs;
     const char *errtype;
     tree fndecl;
     int parmnum;
{
  register enum tree_code codel = TREE_CODE (type);
  register tree rhstype;
  register enum tree_code coder;

  if (codel == OFFSET_TYPE)
    abort ();

  if (TREE_CODE (rhs) == OFFSET_REF)
    rhs = resolve_offset_ref (rhs);

  /* Strip NON_LVALUE_EXPRs since we aren't using as an lvalue.  */
  if (TREE_CODE (rhs) == NON_LVALUE_EXPR)
    rhs = TREE_OPERAND (rhs, 0);

  rhstype = TREE_TYPE (rhs);
  coder = TREE_CODE (rhstype);

  if (rhs == error_mark_node || rhstype == error_mark_node)
    return error_mark_node;
  if (TREE_CODE (rhs) == TREE_LIST && TREE_VALUE (rhs) == error_mark_node)
    return error_mark_node;

  rhs = dubious_conversion_warnings (type, rhs, errtype, fndecl, parmnum);

  /* The RHS of an assignment cannot have void type.  */
  if (coder == VOID_TYPE)
    {
      error ("void value not ignored as it ought to be");
      return error_mark_node;
    }

  /* Simplify the RHS if possible.  */
  if (TREE_CODE (rhs) == CONST_DECL)
    rhs = DECL_INITIAL (rhs);
  
  /* We do not use decl_constant_value here because of this case:

       const char* const s = "s";
 
     The conversion rules for a string literal are more lax than for a
     variable; in particular, a string literal can be converted to a
     "char *" but the variable "s" cannot be converted in the same
     way.  If the conversion is allowed, the optimization should be
     performed while creating the converted expression.  */

  /* [expr.ass]

     The expression is implicitly converted (clause _conv_) to the
     cv-unqualified type of the left operand.

     We allow bad conversions here because by the time we get to this point
     we are committed to doing the conversion.  If we end up doing a bad
     conversion, convert_like will complain.  */
  if (!can_convert_arg_bad (type, rhstype, rhs))
    {
      /* When -Wno-pmf-conversions is use, we just silently allow
	 conversions from pointers-to-members to plain pointers.  If
	 the conversion doesn't work, cp_convert will complain.  */
      if (!warn_pmf2ptr 
	  && TYPE_PTR_P (type) 
	  && TYPE_PTRMEMFUNC_P (rhstype))
	rhs = cp_convert (strip_top_quals (type), rhs);
      else
	{
	  /* If the right-hand side has unknown type, then it is an
	     overloaded function.  Call instantiate_type to get error
	     messages.  */
	  if (rhstype == unknown_type_node)
	    instantiate_type (type, rhs, tf_error | tf_warning);
	  else if (fndecl)
	    error ("cannot convert `%T' to `%T' for argument `%P' to `%D'",
		      rhstype, type, parmnum, fndecl);
	  else
	    error ("cannot convert `%T' to `%T' in %s", rhstype, type, 
		      errtype);
	  return error_mark_node;
	}
    }
  return perform_implicit_conversion (strip_top_quals (type), rhs);
}

/* Convert RHS to be of type TYPE.
   If EXP is nonzero, it is the target of the initialization.
   ERRTYPE is a string to use in error messages.

   Two major differences between the behavior of
   `convert_for_assignment' and `convert_for_initialization'
   are that references are bashed in the former, while
   copied in the latter, and aggregates are assigned in
   the former (operator=) while initialized in the
   latter (X(X&)).

   If using constructor make sure no conversion operator exists, if one does
   exist, an ambiguity exists.

   If flags doesn't include LOOKUP_COMPLAIN, don't complain about anything.  */

tree
convert_for_initialization (exp, type, rhs, flags, errtype, fndecl, parmnum)
     tree exp, type, rhs;
     int flags;
     const char *errtype;
     tree fndecl;
     int parmnum;
{
  register enum tree_code codel = TREE_CODE (type);
  register tree rhstype;
  register enum tree_code coder;

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs, since RHS is used in non-lvalue context.  */
  if (TREE_CODE (rhs) == NOP_EXPR
      && TREE_TYPE (rhs) == TREE_TYPE (TREE_OPERAND (rhs, 0))
      && codel != REFERENCE_TYPE)
    rhs = TREE_OPERAND (rhs, 0);

  if (rhs == error_mark_node
      || (TREE_CODE (rhs) == TREE_LIST && TREE_VALUE (rhs) == error_mark_node))
    return error_mark_node;

  if (TREE_CODE (rhs) == OFFSET_REF)
    {
      rhs = resolve_offset_ref (rhs);
      if (rhs == error_mark_node)
	return error_mark_node;
    }

  if (TREE_CODE (TREE_TYPE (rhs)) == REFERENCE_TYPE)
    rhs = convert_from_reference (rhs);

  if ((TREE_CODE (TREE_TYPE (rhs)) == ARRAY_TYPE
       && TREE_CODE (type) != ARRAY_TYPE
       && (TREE_CODE (type) != REFERENCE_TYPE
	   || TREE_CODE (TREE_TYPE (type)) != ARRAY_TYPE))
      || (TREE_CODE (TREE_TYPE (rhs)) == FUNCTION_TYPE
	  && (TREE_CODE (type) != REFERENCE_TYPE
	      || TREE_CODE (TREE_TYPE (type)) != FUNCTION_TYPE))
      || TREE_CODE (TREE_TYPE (rhs)) == METHOD_TYPE)
    rhs = default_conversion (rhs);

  rhstype = TREE_TYPE (rhs);
  coder = TREE_CODE (rhstype);

  if (coder == ERROR_MARK)
    return error_mark_node;

  /* We accept references to incomplete types, so we can
     return here before checking if RHS is of complete type.  */
     
  if (codel == REFERENCE_TYPE)
    {
      /* This should eventually happen in convert_arguments.  */
      int savew = 0, savee = 0;

      if (fndecl)
	savew = warningcount, savee = errorcount;
      rhs = initialize_reference (type, rhs, /*decl=*/NULL_TREE);
      if (fndecl)
	{
	  if (warningcount > savew)
	    cp_warning_at ("in passing argument %P of `%+D'", parmnum, fndecl);
	  else if (errorcount > savee)
	    cp_error_at ("in passing argument %P of `%+D'", parmnum, fndecl);
	}
      return rhs;
    }      

  if (exp != 0)
    exp = require_complete_type (exp);
  if (exp == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (rhstype) == REFERENCE_TYPE)
    rhstype = TREE_TYPE (rhstype);

  type = complete_type (type);

  if (IS_AGGR_TYPE (type))
    return ocp_convert (type, rhs, CONV_IMPLICIT|CONV_FORCE_TEMP, flags);

  return convert_for_assignment (type, rhs, errtype, fndecl, parmnum);
}

/* Expand an ASM statement with operands, handling output operands
   that are not variables or INDIRECT_REFS by transforming such
   cases into cases that expand_asm_operands can handle.

   Arguments are same as for expand_asm_operands.

   We don't do default conversions on all inputs, because it can screw
   up operands that are expected to be in memory.  */

void
c_expand_asm_operands (string, outputs, inputs, clobbers, vol, filename, line)
     tree string, outputs, inputs, clobbers;
     int vol;
     const char *filename;
     int line;
{
  int noutputs = list_length (outputs);
  register int i;
  /* o[I] is the place that output number I should be written.  */
  register tree *o = (tree *) alloca (noutputs * sizeof (tree));
  register tree tail;

  /* Record the contents of OUTPUTS before it is modified.  */
  for (i = 0, tail = outputs; tail; tail = TREE_CHAIN (tail), i++)
    o[i] = TREE_VALUE (tail);

  /* Generate the ASM_OPERANDS insn;
     store into the TREE_VALUEs of OUTPUTS some trees for
     where the values were actually stored.  */
  expand_asm_operands (string, outputs, inputs, clobbers, vol, filename, line);

  /* Copy all the intermediate outputs into the specified outputs.  */
  for (i = 0, tail = outputs; tail; tail = TREE_CHAIN (tail), i++)
    {
      if (o[i] != TREE_VALUE (tail))
	{
	  expand_expr (build_modify_expr (o[i], NOP_EXPR, TREE_VALUE (tail)),
		       const0_rtx, VOIDmode, EXPAND_NORMAL);
	  free_temp_slots ();

	  /* Restore the original value so that it's correct the next
	     time we expand this function.  */
	  TREE_VALUE (tail) = o[i];
	}
      /* Detect modification of read-only values.
	 (Otherwise done by build_modify_expr.)  */
      else
	{
	  tree type = TREE_TYPE (o[i]);
	  if (type != error_mark_node
	      && (CP_TYPE_CONST_P (type)
		  || (IS_AGGR_TYPE_CODE (TREE_CODE (type))
		      && C_TYPE_FIELDS_READONLY (type))))
	    readonly_error (o[i], "modification by `asm'", 1);
	}
    }

  /* Those MODIFY_EXPRs could do autoincrements.  */
  emit_queue ();
}

/* If RETVAL is the address of, or a reference to, a local variable or
   temporary give an appropraite warning.  */

static void
maybe_warn_about_returning_address_of_local (retval)
     tree retval;
{
  tree valtype = TREE_TYPE (DECL_RESULT (current_function_decl));
  tree whats_returned = retval;

  for (;;)
    {
      if (TREE_CODE (whats_returned) == COMPOUND_EXPR)
	whats_returned = TREE_OPERAND (whats_returned, 1);
      else if (TREE_CODE (whats_returned) == CONVERT_EXPR
	       || TREE_CODE (whats_returned) == NON_LVALUE_EXPR
	       || TREE_CODE (whats_returned) == NOP_EXPR)
	whats_returned = TREE_OPERAND (whats_returned, 0);
      else
	break;
    }

  if (TREE_CODE (whats_returned) != ADDR_EXPR)
    return;
  whats_returned = TREE_OPERAND (whats_returned, 0);      

  if (TREE_CODE (valtype) == REFERENCE_TYPE)
    {
      if (TREE_CODE (whats_returned) == AGGR_INIT_EXPR
	  || TREE_CODE (whats_returned) == TARGET_EXPR)
	{
	  /* Get the target.  */
	  whats_returned = TREE_OPERAND (whats_returned, 0);
	  warning ("returning reference to temporary");
	  return;
	}
      if (TREE_CODE (whats_returned) == VAR_DECL 
	  && DECL_NAME (whats_returned)
	  && TEMP_NAME_P (DECL_NAME (whats_returned)))
	{
	  warning ("reference to non-lvalue returned");
	  return;
	}
    }

  if (TREE_CODE (whats_returned) == VAR_DECL
      && DECL_NAME (whats_returned)
      && DECL_FUNCTION_SCOPE_P (whats_returned)
      && !(TREE_STATIC (whats_returned)
	   || TREE_PUBLIC (whats_returned)))
    {
      if (TREE_CODE (valtype) == REFERENCE_TYPE)
	cp_warning_at ("reference to local variable `%D' returned", 
		       whats_returned);
      else
	cp_warning_at ("address of local variable `%D' returned", 
		       whats_returned);
      return;
    }
}

/* Check that returning RETVAL from the current function is valid.
   Return an expression explicitly showing all conversions required to
   change RETVAL into the function return type, and to assign it to
   the DECL_RESULT for the function.  */

tree
check_return_expr (retval)
     tree retval;
{
  tree result;
  /* The type actually returned by the function, after any
     promotions.  */
  tree valtype;
  int fn_returns_value_p;

  /* A `volatile' function is one that isn't supposed to return, ever.
     (This is a G++ extension, used to get better code for functions
     that call the `volatile' function.)  */
  if (TREE_THIS_VOLATILE (current_function_decl))
    warning ("function declared `noreturn' has a `return' statement");

  /* Check for various simple errors.  */
  if (DECL_DESTRUCTOR_P (current_function_decl))
    {
      if (retval)
	error ("returning a value from a destructor");
      return NULL_TREE;
    }
  else if (DECL_CONSTRUCTOR_P (current_function_decl))
    {
      if (in_function_try_handler)
	/* If a return statement appears in a handler of the
	   function-try-block of a constructor, the program is ill-formed.  */
	error ("cannot return from a handler of a function-try-block of a constructor");
      else if (retval)
	/* You can't return a value from a constructor.  */
	error ("returning a value from a constructor");
      return NULL_TREE;
    }

  /* When no explicit return-value is given in a function with a named
     return value, the named return value is used.  */
  result = DECL_RESULT (current_function_decl);
  valtype = TREE_TYPE (result);
  my_friendly_assert (valtype != NULL_TREE, 19990924);
  fn_returns_value_p = !VOID_TYPE_P (valtype);
  if (!retval && DECL_NAME (result) && fn_returns_value_p)
    retval = result;

  /* Check for a return statement with no return value in a function
     that's supposed to return a value.  */
  if (!retval && fn_returns_value_p)
    {
      pedwarn ("return-statement with no value, in function declared with a non-void return type");
      /* Clear this, so finish_function won't say that we reach the
	 end of a non-void function (which we don't, we gave a
	 return!).  */
      current_function_returns_null = 0;
    }
  /* Check for a return statement with a value in a function that
     isn't supposed to return a value.  */
  else if (retval && !fn_returns_value_p)
    {     
      if (VOID_TYPE_P (TREE_TYPE (retval)))
	/* You can return a `void' value from a function of `void'
	   type.  In that case, we have to evaluate the expression for
	   its side-effects.  */
	  finish_expr_stmt (retval);
      else
	pedwarn ("return-statement with a value, in function declared with a void return type");

      current_function_returns_null = 1;

      /* There's really no value to return, after all.  */
      return NULL_TREE;
    }
  else if (!retval)
    /* Remember that this function can sometimes return without a
       value.  */
    current_function_returns_null = 1;
  else
    /* Remember that this function did return a value.  */
    current_function_returns_value = 1;

  /* Only operator new(...) throw(), can return NULL [expr.new/13].  */
  if ((DECL_OVERLOADED_OPERATOR_P (current_function_decl) == NEW_EXPR
       || DECL_OVERLOADED_OPERATOR_P (current_function_decl) == VEC_NEW_EXPR)
      && !TYPE_NOTHROW_P (TREE_TYPE (current_function_decl))
      && ! flag_check_new
      && null_ptr_cst_p (retval))
    warning ("`operator new' must not return NULL unless it is declared `throw()' (or -fcheck-new is in effect)");

  /* Effective C++ rule 15.  See also start_function.  */
  if (warn_ecpp
      && DECL_NAME (current_function_decl) == ansi_assopname(NOP_EXPR)
      && retval != current_class_ref)
    warning ("`operator=' should return a reference to `*this'");

  /* The fabled Named Return Value optimization, as per [class.copy]/15:

     [...]      For  a function with a class return type, if the expression
     in the return statement is the name of a local  object,  and  the  cv-
     unqualified  type  of  the  local  object  is the same as the function
     return type, an implementation is permitted to omit creating the  tem-
     porary  object  to  hold  the function return value [...]

     So, if this is a value-returning function that always returns the same
     local variable, remember it.

     It might be nice to be more flexible, and choose the first suitable
     variable even if the function sometimes returns something else, but
     then we run the risk of clobbering the variable we chose if the other
     returned expression uses the chosen variable somehow.  And people expect
     this restriction, anyway.  (jason 2000-11-19)

     See finish_function, genrtl_start_function, and declare_return_variable
     for other pieces of this optimization.  */

  if (fn_returns_value_p && flag_elide_constructors)
    {
      if (retval != NULL_TREE
	  && (current_function_return_value == NULL_TREE
	      || current_function_return_value == retval)
	  && TREE_CODE (retval) == VAR_DECL
	  && DECL_CONTEXT (retval) == current_function_decl
	  && ! TREE_STATIC (retval)
	  && (DECL_ALIGN (retval)
	      >= DECL_ALIGN (DECL_RESULT (current_function_decl)))
	  && same_type_p ((TYPE_MAIN_VARIANT
			   (TREE_TYPE (retval))),
			  (TYPE_MAIN_VARIANT
			   (TREE_TYPE (TREE_TYPE (current_function_decl))))))
	current_function_return_value = retval;
      else
	current_function_return_value = error_mark_node;
    }

  /* We don't need to do any conversions when there's nothing being
     returned.  */
  if (!retval || retval == error_mark_node)
    return retval;

  /* Do any required conversions.  */
  if (retval == result || DECL_CONSTRUCTOR_P (current_function_decl))
    /* No conversions are required.  */
    ;
  else
    {
      /* The type the function is declared to return.  */
      tree functype = TREE_TYPE (TREE_TYPE (current_function_decl));

      /* First convert the value to the function's return type, then
	 to the type of return value's location to handle the
         case that functype is smaller than the valtype.  */
      retval = convert_for_initialization
	(NULL_TREE, functype, retval, LOOKUP_NORMAL|LOOKUP_ONLYCONVERTING,
	 "return", NULL_TREE, 0);
      retval = convert (valtype, retval);

      /* If the conversion failed, treat this just like `return;'.  */
      if (retval == error_mark_node)
	return retval;
      /* We can't initialize a register from a AGGR_INIT_EXPR.  */
      else if (! current_function_returns_struct
	       && TREE_CODE (retval) == TARGET_EXPR
	       && TREE_CODE (TREE_OPERAND (retval, 1)) == AGGR_INIT_EXPR)
	retval = build (COMPOUND_EXPR, TREE_TYPE (retval), retval,
			TREE_OPERAND (retval, 0));
      else
	maybe_warn_about_returning_address_of_local (retval);
    }
  
  /* Actually copy the value returned into the appropriate location.  */
  if (retval && retval != result)
    retval = build (INIT_EXPR, TREE_TYPE (result), result, retval);

  return retval;
}


/* Returns nonzero if the pointer-type FROM can be converted to the
   pointer-type TO via a qualification conversion.  If CONSTP is -1,
   then we return nonzero if the pointers are similar, and the
   cv-qualification signature of FROM is a proper subset of that of TO.

   If CONSTP is positive, then all outer pointers have been
   const-qualified.  */

static int
comp_ptr_ttypes_real (to, from, constp)
     tree to, from;
     int constp;
{
  int to_more_cv_qualified = 0;

  for (; ; to = TREE_TYPE (to), from = TREE_TYPE (from))
    {
      if (TREE_CODE (to) != TREE_CODE (from))
	return 0;

      if (TREE_CODE (from) == OFFSET_TYPE
	  && same_type_p (TYPE_OFFSET_BASETYPE (from),
			  TYPE_OFFSET_BASETYPE (to)))
	  continue;

      /* Const and volatile mean something different for function types,
	 so the usual checks are not appropriate.  */
      if (TREE_CODE (to) != FUNCTION_TYPE && TREE_CODE (to) != METHOD_TYPE)
	{
	  if (!at_least_as_qualified_p (to, from))
	    return 0;

	  if (!at_least_as_qualified_p (from, to))
	    {
	      if (constp == 0)
		return 0;
	      else
		++to_more_cv_qualified;
	    }

	  if (constp > 0)
	    constp &= TYPE_READONLY (to);
	}

      if (TREE_CODE (to) != POINTER_TYPE)
	return 
	  same_type_ignoring_top_level_qualifiers_p (to, from)
	  && (constp >= 0 || to_more_cv_qualified);
    }
}

/* When comparing, say, char ** to char const **, this function takes the
   'char *' and 'char const *'.  Do not pass non-pointer types to this
   function.  */

int
comp_ptr_ttypes (to, from)
     tree to, from;
{
  return comp_ptr_ttypes_real (to, from, 1);
}

/* Returns 1 if to and from are (possibly multi-level) pointers to the same
   type or inheritance-related types, regardless of cv-quals.  */

int
ptr_reasonably_similar (to, from)
     tree to, from;
{
  for (; ; to = TREE_TYPE (to), from = TREE_TYPE (from))
    {
      /* Any target type is similar enough to void.  */
      if (TREE_CODE (to) == VOID_TYPE
	  || TREE_CODE (from) == VOID_TYPE)
	return 1;

      if (TREE_CODE (to) != TREE_CODE (from))
	return 0;

      if (TREE_CODE (from) == OFFSET_TYPE
	  && comptypes (TYPE_OFFSET_BASETYPE (to),
			TYPE_OFFSET_BASETYPE (from), 
			COMPARE_BASE | COMPARE_RELAXED))
	continue;

      if (TREE_CODE (to) == INTEGER_TYPE
	  && TYPE_PRECISION (to) == TYPE_PRECISION (from))
	return 1;

      if (TREE_CODE (to) == FUNCTION_TYPE)
	return 1;

      if (TREE_CODE (to) != POINTER_TYPE)
	return comptypes
	  (TYPE_MAIN_VARIANT (to), TYPE_MAIN_VARIANT (from), 
	   COMPARE_BASE | COMPARE_RELAXED);
    }
}

/* Like comp_ptr_ttypes, for const_cast.  */

static int
comp_ptr_ttypes_const (to, from)
     tree to, from;
{
  for (; ; to = TREE_TYPE (to), from = TREE_TYPE (from))
    {
      if (TREE_CODE (to) != TREE_CODE (from))
	return 0;

      if (TREE_CODE (from) == OFFSET_TYPE
	  && same_type_p (TYPE_OFFSET_BASETYPE (from),
			  TYPE_OFFSET_BASETYPE (to)))
	  continue;

      if (TREE_CODE (to) != POINTER_TYPE)
	return same_type_ignoring_top_level_qualifiers_p (to, from);
    }
}

/* Like comp_ptr_ttypes, for reinterpret_cast.  */

static int
comp_ptr_ttypes_reinterpret (to, from)
     tree to, from;
{
  int constp = 1;

  for (; ; to = TREE_TYPE (to), from = TREE_TYPE (from))
    {
      if (TREE_CODE (from) == OFFSET_TYPE)
	from = TREE_TYPE (from);
      if (TREE_CODE (to) == OFFSET_TYPE)
	to = TREE_TYPE (to);

      /* Const and volatile mean something different for function types,
	 so the usual checks are not appropriate.  */
      if (TREE_CODE (from) != FUNCTION_TYPE && TREE_CODE (from) != METHOD_TYPE
	  && TREE_CODE (to) != FUNCTION_TYPE && TREE_CODE (to) != METHOD_TYPE)
	{
	  if (!at_least_as_qualified_p (to, from))
	    return 0;

	  if (! constp
	      && !at_least_as_qualified_p (from, to))
	    return 0;
	  constp &= TYPE_READONLY (to);
	}

      if (TREE_CODE (from) != POINTER_TYPE
	  || TREE_CODE (to) != POINTER_TYPE)
	return 1;
    }
}

/* Returns the type qualifiers for this type, including the qualifiers on the
   elements for an array type.  */

int
cp_type_quals (type)
     tree type;
{
  type = strip_array_types (type);
  if (type == error_mark_node)
    return TYPE_UNQUALIFIED;
  return TYPE_QUALS (type);
}

/* Returns nonzero if the TYPE contains a mutable member */

int
cp_has_mutable_p (type)
     tree type;
{
  type = strip_array_types (type);

  return CLASS_TYPE_P (type) && CLASSTYPE_HAS_MUTABLE (type);
}

/* Subroutine of casts_away_constness.  Make T1 and T2 point at
   exemplar types such that casting T1 to T2 is casting away castness
   if and only if there is no implicit conversion from T1 to T2.  */

static void
casts_away_constness_r (t1, t2)
     tree *t1;
     tree *t2;
{
  int quals1;
  int quals2;

  /* [expr.const.cast]

     For multi-level pointer to members and multi-level mixed pointers
     and pointers to members (conv.qual), the "member" aspect of a
     pointer to member level is ignored when determining if a const
     cv-qualifier has been cast away.  */
  if (TYPE_PTRMEM_P (*t1))
    *t1 = build_pointer_type (TREE_TYPE (TREE_TYPE (*t1)));
  if (TYPE_PTRMEM_P (*t2))
    *t2 = build_pointer_type (TREE_TYPE (TREE_TYPE (*t2)));

  /* [expr.const.cast]

     For  two  pointer types:

            X1 is T1cv1,1 * ... cv1,N *   where T1 is not a pointer type
            X2 is T2cv2,1 * ... cv2,M *   where T2 is not a pointer type
            K is min(N,M)

     casting from X1 to X2 casts away constness if, for a non-pointer
     type T there does not exist an implicit conversion (clause
     _conv_) from:

            Tcv1,(N-K+1) * cv1,(N-K+2) * ... cv1,N *
      
     to

            Tcv2,(M-K+1) * cv2,(M-K+2) * ... cv2,M *.  */

  if (TREE_CODE (*t1) != POINTER_TYPE
      || TREE_CODE (*t2) != POINTER_TYPE)
    {
      *t1 = cp_build_qualified_type (void_type_node,
				     cp_type_quals (*t1));
      *t2 = cp_build_qualified_type (void_type_node,
				     cp_type_quals (*t2));
      return;
    }
  
  quals1 = cp_type_quals (*t1);
  quals2 = cp_type_quals (*t2);
  *t1 = TREE_TYPE (*t1);
  *t2 = TREE_TYPE (*t2);
  casts_away_constness_r (t1, t2);
  *t1 = build_pointer_type (*t1);
  *t2 = build_pointer_type (*t2);
  *t1 = cp_build_qualified_type (*t1, quals1);
  *t2 = cp_build_qualified_type (*t2, quals2);
}

/* Returns nonzero if casting from TYPE1 to TYPE2 casts away
   constness.  */

static int
casts_away_constness (t1, t2)
     tree t1;
     tree t2;
{
  if (TREE_CODE (t2) == REFERENCE_TYPE)
    {
      /* [expr.const.cast]
	 
	 Casting from an lvalue of type T1 to an lvalue of type T2
	 using a reference cast casts away constness if a cast from an
	 rvalue of type "pointer to T1" to the type "pointer to T2"
	 casts away constness.  */
      t1 = (TREE_CODE (t1) == REFERENCE_TYPE
	    ? TREE_TYPE (t1) : t1);
      return casts_away_constness (build_pointer_type (t1),
				   build_pointer_type (TREE_TYPE (t2)));
    }

  if (TYPE_PTRMEM_P (t1) && TYPE_PTRMEM_P (t2))
    /* [expr.const.cast]
       
       Casting from an rvalue of type "pointer to data member of X
       of type T1" to the type "pointer to data member of Y of type
       T2" casts away constness if a cast from an rvalue of type
       "pointer to T1" to the type "pointer to T2" casts away
       constness.  */
    return casts_away_constness
      (build_pointer_type (TREE_TYPE (TREE_TYPE (t1))),
       build_pointer_type (TREE_TYPE (TREE_TYPE (t2))));

  /* Casting away constness is only something that makes sense for
     pointer or reference types.  */
  if (TREE_CODE (t1) != POINTER_TYPE 
      || TREE_CODE (t2) != POINTER_TYPE)
    return 0;

  /* Top-level qualifiers don't matter.  */
  t1 = TYPE_MAIN_VARIANT (t1);
  t2 = TYPE_MAIN_VARIANT (t2);
  casts_away_constness_r (&t1, &t2);
  if (!can_convert (t2, t1))
    return 1;

  return 0;
}

/* Returns TYPE with its cv qualifiers removed
   TYPE is T cv* .. *cv where T is not a pointer type,
   returns T * .. *. (If T is an array type, then the cv qualifiers
   above are those of the array members.)  */

static tree
strip_all_pointer_quals (type)
     tree type;
{
  if (TREE_CODE (type) == POINTER_TYPE)
    return build_pointer_type (strip_all_pointer_quals (TREE_TYPE (type)));
  else if (TREE_CODE (type) == OFFSET_TYPE)
    return build_offset_type (TYPE_OFFSET_BASETYPE (type),
			      strip_all_pointer_quals (TREE_TYPE (type)));
  else
    return TYPE_MAIN_VARIANT (type);
}
