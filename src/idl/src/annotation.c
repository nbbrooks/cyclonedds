/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "idl/processor.h"
#include "idl/string.h"
#include "annotation.h"
#include "expression.h"
#include "hashid.h"
#include "tree.h"

static idl_retcode_t
annotate_id(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_literal_t *literal;

  assert(annotation_appl);
  assert(annotation_appl->parameters);
  literal = (idl_literal_t *)annotation_appl->parameters->const_expr;
  assert(idl_mask(literal) == (IDL_LITERAL|IDL_ULONG));

  if (idl_mask(node) & IDL_MEMBER) {
    idl_member_t *member = (idl_member_t *)node;
    if (idl_next(member->declarators)) {
      idl_error(pstate, idl_location(annotation_appl),
        "@id cannot be applied to members with multiple declarators");
      return IDL_RETCODE_SEMANTIC_ERROR;
    } else if (member->id.annotation) {
      idl_error(pstate, idl_location(annotation_appl),
        "@id conflicts with earlier annotation");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    member->id.annotation = annotation_appl;
    member->id.value = literal->value.uint32;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@id cannot be applied to %s elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_hashid(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  const char *name = NULL;

  assert(annotation_appl);
  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_mask(literal) == (IDL_LITERAL|IDL_STRING));
    name = literal->value.str;
  }

  if (idl_mask(node) & IDL_MEMBER) {
    idl_member_t *member = (idl_member_t *)node;
    if (idl_next(member->declarators)) {
      idl_error(pstate, idl_location(annotation_appl),
        "@hashid cannot be applied to members with multiple declarators");
      return IDL_RETCODE_SEMANTIC_ERROR;
    } else if (member->id.annotation) {
      idl_error(pstate, idl_location(annotation_appl),
        "@hashid conflicts with earlier annotation");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    if (!name)
      name = member->declarators->name->identifier;
    member->id.annotation = annotation_appl;
    member->id.value = idl_hashid(name);
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@hashid cannot be applied to '%s' elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_autoid(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_autoid_t autoid = IDL_HASH;
  const char *enumerator;

  assert(annotation_appl);
  assert(annotation_appl->parameters);
  if (annotation_appl->parameters) {
    enumerator = idl_identifier(annotation_appl->parameters->const_expr);
    if (strcmp(enumerator, "HASH") == 0) {
      autoid = IDL_HASH;
    } else if (strcmp(enumerator, "SEQUENTIAL") == 0) {
      autoid = IDL_SEQUENTIAL;
    }
  }

  if (idl_is_struct(node)) {
    ((idl_struct_t *)node)->autoid.annotation = annotation_appl;
    ((idl_struct_t *)node)->autoid.value = autoid;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@autoid cannot be applied to '%s' elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_value(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_type_t type;
  const idl_const_expr_t *const_expr;
  uint32_t value;

  assert(annotation_appl);
  assert(annotation_appl->parameters);
  const_expr = annotation_appl->parameters->const_expr;

  if (!idl_is_enumerator(node)) {
    idl_error(pstate, idl_location(annotation_appl),
      "@value cannot be applied to '%s' elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  type = idl_type(const_expr);
  if (type == IDL_OCTET || (type & IDL_INTEGER_TYPE)) {
    idl_intval_t intval = idl_intval(const_expr);

    if ((intval.type & IDL_UNSIGNED) && intval.value.ullng > UINT32_MAX) {
      idl_error(pstate, idl_location(annotation_appl),
        "@value(%" PRIu64 ") cannot be applied to '%s' element",
        intval.value.ullng, idl_construct(node));
      return IDL_RETCODE_SEMANTIC_ERROR;
    } else if (!(intval.type & IDL_UNSIGNED) && intval.value.llng < 0) {
      idl_error(pstate, idl_location(annotation_appl),
        "@value(%" PRId64 ") cannot be applied to '%s' element",
        intval.value.llng, idl_construct(node));
      return IDL_RETCODE_SEMANTIC_ERROR;
    }

    value = (uint32_t)intval.value.ullng;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@value with %s cannot be applied to '%s' element",
      idl_construct(const_expr), idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  ((idl_enumerator_t *)node)->value.annotation = annotation_appl;
  ((idl_enumerator_t *)node)->value.value = value;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_extensibility(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  const idl_annotation_appl_t **annotationp = NULL;
  idl_extensibility_t *extensibilityp = NULL, extensibility = IDL_FINAL;
  const char *annotation;

  assert(annotation_appl);
  annotation = idl_identifier(annotation_appl);
  assert(annotation);
  if (idl_strcasecmp(annotation, "extensibility") == 0) {
    assert(annotation_appl->parameters);
    assert(annotation_appl->parameters->const_expr);
    annotation = idl_identifier(annotation_appl->parameters->const_expr);
    assert(annotation);
  }

  if (idl_is_struct(node)) {
    annotationp = &((idl_struct_t *)node)->extensibility.annotation;
    extensibilityp = &((idl_struct_t *)node)->extensibility.value;
  } else if (idl_is_union(node)) {
    annotationp = &((idl_union_t *)node)->extensibility.annotation;
    extensibilityp = &((idl_union_t *)node)->extensibility.value;
  } else if (idl_is_enum(node)) {
    annotationp = &((idl_enum_t *)node)->extensibility.annotation;
    extensibilityp = &((idl_enum_t *)node)->extensibility.value;
  }

  assert(!annotationp == !extensibilityp);

  if (!annotationp) {
    idl_error(pstate, idl_location(annotation_appl),
      "@%s can only be applied to constructed types", annotation);
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (*annotationp) {
    idl_error(pstate, idl_location(annotation_appl),
      "@%s clashes with an earlier annotation", annotation);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (idl_strcasecmp(annotation, "appendable") == 0) {
    extensibility = IDL_APPENDABLE;
  } else if (idl_strcasecmp(annotation, "mutable") == 0) {
    extensibility = IDL_MUTABLE;
  } else {
    assert(idl_strcasecmp(annotation, "final") == 0);
    extensibility = IDL_FINAL;
  }

  *annotationp = annotation_appl;
  *extensibilityp = extensibility;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_key(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  bool key = true;

  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_mask(literal) == (IDL_LITERAL|IDL_BOOL));
    key = literal->value.bln;
  }

  if (idl_mask(node) & IDL_MEMBER) {
    ((idl_member_t *)node)->key.annotation = annotation_appl;
    ((idl_member_t *)node)->key.value = key;
  } else if (idl_mask(node) & IDL_SWITCH_TYPE_SPEC) {
    ((idl_switch_type_spec_t *)node)->key.annotation = annotation_appl;
    ((idl_switch_type_spec_t *)node)->key.value = key;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@key can only be applied to members of constructed types");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
set_nested(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node,
  bool nested)
{
  const idl_annotation_appl_t **annotationp = NULL;
  bool *nestedp = NULL;
  const char *annotation;

  annotation = idl_identifier(annotation_appl->annotation);
  assert(annotation);

  if (idl_is_struct(node)) {
    annotationp = &((idl_struct_t *)node)->nested.annotation;
    nestedp = &((idl_struct_t *)node)->nested.value;
  } else if (idl_is_union(node)) {
    annotationp = &((idl_union_t *)node)->nested.annotation;
    nestedp = &((idl_union_t *)node)->nested.value;
  }

  if (!annotationp) {
    idl_error(pstate, idl_location(annotation_appl),
      "@%s cannot be applied to %s elements", annotation, idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (*annotationp) {
    if (annotation_appl->annotation == (*annotationp)->annotation) {
      idl_error(pstate, idl_location(annotation_appl),
        "@%s clashes with an earlier annotation", annotation);
      return IDL_RETCODE_SEMANTIC_ERROR;
    /* @topic overrides @nested, short-circuit on @nested */
    } else if (idl_identifier_is(annotation_appl->annotation, "nested")) {
      return IDL_RETCODE_OK;
    }
  }

  *annotationp = annotation_appl;
  *nestedp = nested;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_nested(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  bool nested = true;

  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_mask(literal) == (IDL_LITERAL|IDL_BOOL));
    nested = literal->value.bln;
  }

  return set_nested(pstate, annotation_appl, node, nested);
}

static idl_retcode_t
annotate_topic(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  const char *platform = "*"; /* default is any platform */
  const idl_annotation_appl_param_t *p;

  for (p = annotation_appl->parameters; p; p = idl_next(p)) {
    if (!idl_identifier_is(p->member, "platform"))
      continue;
    assert(idl_mask(p->const_expr) == (IDL_LITERAL|IDL_STRING));
    platform = ((idl_literal_t *)p->const_expr)->value.str;
    break;
  }

  /* only apply topic annotation if platform equals "*" or "DDS" */
  if (strcmp(platform, "*") != 0 && strcmp(platform, "DDS") != 0)
    return IDL_RETCODE_OK;

  return set_nested(pstate, annotation_appl, node, false);
}

static idl_retcode_t
set_implicitly_nested(void *node)
{
  idl_retcode_t ret = IDL_RETCODE_OK;

  for (; ret == IDL_RETCODE_OK && node; node = idl_next(node)) {
    if (idl_is_struct(node)) {
      /* skip if annotated with @nested or @topic before */
      if (!((idl_struct_t *)node)->nested.annotation)
        ((idl_struct_t *)node)->nested.value = true;
    } else if (idl_is_union(node)) {
      /* skip if annotated with @nested or @topic before */
      if (!((idl_union_t *)node)->nested.annotation)
        ((idl_union_t *)node)->nested.value = true;
    } else if (idl_is_module(node)) {
      idl_module_t *module = node;
      /* skip if annotated with @default_nested before */
      if (module->default_nested.annotation)
        continue;
      module->default_nested.value = true;
      ret = set_implicitly_nested(module->definitions);
    }
  }

  return ret;
}

static idl_retcode_t
annotate_default_nested(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  bool default_nested = true;

  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_mask(literal) == (IDL_LITERAL|IDL_BOOL));
    default_nested = literal->value.bln;
  }

  if ((idl_mask(node) & IDL_MODULE)) {
    idl_module_t *module = (idl_module_t *)node;
    if (!module->default_nested.annotation) {
      module->default_nested.annotation = annotation_appl;
      module->default_nested.value = default_nested;
      if (!module->default_nested.value)
        return IDL_RETCODE_OK;
      /* annotate all embedded structs and unions implicitly nested unless
         explicitly annotated with either @nested or @topic */
      return set_implicitly_nested(module->definitions);
    } else {
      idl_error(pstate, idl_location(annotation_appl),
        "@default_nested conflicts with earlier annotation");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@default_nested cannot be applied to %s elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }
}

static const idl_builtin_annotation_t annotations[] = {
  /* general purpose */
  { .syntax = "@annotation id { unsigned long value; };",
    .summary =
      "<p>Assign a 32-bit unsigned integer identifier to an element, with "
      "the underlying assumption that an identifier should be unique inside "
      "its scope of application. The precise scope of uniqueness depends on "
      "the type of specification making use of it.</p>",
    .callback = &annotate_id },
  { .syntax =
      "@annotation autoid {\n"
      "  enum AutoidKind { SEQUENTIAL, HASH };\n"
      "  AutoidKind value default HASH;\n"
      "};",
    .summary =
      "<p>Complements @id and is applicable to any set containing elements "
      "to which allocating 32-bit unsigned identifiers makes sense. @autoid "
      "instructs to automatically allocate identifiers to elements that have "
      "not been assigned a 32-bit unsigned identifiers explicitly.</p>",
    .callback = &annotate_autoid },
#if 0
  { .syntax = "@annotation optional { boolean value default TRUE; };",
    .summary =
      "<p>Set optionality on any element that makes sense to be optional.</p>",
    .callback = annotate_optional },
#endif
  { .syntax = "@annotation value { any value; };",
    .summary =
      "<p>Set a constant value to any element that may be given a constant "
      "value. More versatile than @id as it may carry any constant value, "
      "but does not carry any concept of uniqueness among a set.</p>",
    .callback = annotate_value },
  { .syntax =
      "@annotation extensibility {\n"
      "  enum ExtensibilityKind { FINAL, APPENDABLE, MUTABLE };\n"
      "  ExtensibilityKind value;\n"
      "};",
    .summary =
      "<p>Specify a given constructed element, such as a data type or an "
      "interface, is allowed to evolve.</p>",
    .callback = annotate_extensibility },
  { .syntax = "@annotation final { };",
    .summary = "<p>Shorthand for @extensibility(FINAL).</p>",
    .callback = annotate_extensibility },
  { .syntax = "@annotation appendable { };",
    .summary = "<p>Shorthand for @extensibility(APPENDABLE).</p>",
    .callback = annotate_extensibility },
  { .syntax = "@annotation mutable { };",
    .summary = "<p>Shorthand for @extensibility(MUTABLE).</p>",
    .callback = annotate_extensibility },
  /* data modeling */
  { .syntax = "@annotation key { boolean value default TRUE; };",
    .summary =
      "<p>Specify a data member is part of the key for the object whose type "
      "is the constructed data type owning this element.</p>",
    .callback = annotate_key },
#if 0
  { .syntax = "@annotation must_understand { boolean value default TRUE; };",
    .summary =
      "<p>Specify the data member must be understood by any application "
      "making use of that piece of data.</p>",
    .callback = annotate_must_understand },
  /* units and ranges */
  { .syntax = "@annotation default { any value; };",
    .summary =
       "<p>Specify a default value for the annotated element.</p>",
    .callback = annotate_default },
  { .syntax = "@annotation range { any min; any max; };",
    .summary =
      "<p>Specify a range of allowed value for the annotated element.</p>",
    .callback = annotate_range },
  { .syntax = "@annotation min { any value; };",
    .summary =
      "<p>Specify a minimum value for the annotated element.</p>",
    .callback = annotate_min },
  { .syntax = "@annotation max { any value; };",
    .summary =
      "<p>Specify a maximum value for the annotated element.</p>",
    .callback = annotate_max },
  { .syntax = "@annotation unit { string value; };",
    .summary =
      "<p>Specify a unit of measurement for the annotated element.</p>",
    .callback = annotate_unit },
#endif
  { .syntax = "@annotation nested { boolean value default TRUE; };",
    .summary =
      "<p>Specify annotated element is never used as top-level object.</p>",
    .callback = annotate_nested },
  /* extensible and dynamic topic types */
  { .syntax = "@annotation hashid { string value default \"\"; };",
    .summary =
      "<p>Assign a 32-bit unsigned integer identifier to an element "
      "derived from the member name or a string specified in the "
      "annotation parameter.</p>",
    .callback = annotate_hashid },
  { .syntax =
      "@annotation topic {\n"
      "  string name default \"\";\n"
      "  string platform default \"*\";\n"
      "};",
    .summary =
      "<p>Specify annotated element is used as top-level object.</p>",
    .callback = annotate_topic },
  { .syntax = "@annotation default_nested { boolean value default TRUE; };",
    .summary =
      "<p>Change aggregated types contained in annotated module are "
      "considered nested unless otherwise annotated by @topic or @nested.</p>",
    .callback = annotate_default_nested },
  { .syntax = NULL, .summary = NULL, .callback = 0 }
};

const idl_builtin_annotation_t *builtin_annotations = annotations;

static idl_annotation_appl_param_t *
find(
  const idl_annotation_appl_t *annotation_appl,
  const idl_annotation_member_t *annotation_member)
{
  const idl_annotation_appl_param_t *ap;

  for (ap = annotation_appl->parameters; ap; ap = idl_next(ap)) {
    if (ap->member == annotation_member)
      return (idl_annotation_appl_param_t *)ap;
  }

  return NULL;
}

static idl_retcode_t
eval(idl_pstate_t *pstate, void *node, idl_annotation_appl_t *appls)
{
  idl_retcode_t ret;

  (void)node;
  for (idl_annotation_appl_t *a = appls; a; a = idl_next(a)) {
    assert(a->annotation);
    idl_annotation_appl_param_t *ap;
    for (ap = a->parameters; ap; ap = idl_next(ap)) {
      idl_type_t type;
      idl_literal_t *literal = NULL;
      assert(ap->member);
      assert(ap->member->type_spec);
      type = idl_type(ap->member->type_spec);
      if ((ret = idl_evaluate(pstate, ap->const_expr, type, &literal)))
        return ret;
      assert(literal);
      if (!idl_scope(literal))
        ((idl_node_t *)literal)->parent = (idl_node_t *)ap;
      ap->const_expr = literal;
    }
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
dedup(idl_pstate_t *pstate, void *node, idl_annotation_appl_t *appls)
{
  (void)node;

  /* deduplicate parameters for annotations */
  for (idl_annotation_appl_t *a = appls; a; a = idl_next(a)) {
    idl_definition_t *d;
    idl_annotation_member_t *m;
    idl_annotation_appl_param_t *ap;
    assert(a->annotation);
    for (ap = a->parameters; ap; ap = idl_next(ap)) {
      assert(ap->member);
      idl_annotation_appl_param_t *cap, *nap;
      for (cap = idl_next(ap); cap; cap = nap) {
        nap = idl_next(cap);
        if (cap->member != ap->member)
          continue;
        if (idl_compare(cap->const_expr, ap->const_expr) != 0) {
          idl_error(pstate, idl_location(cap),
            "Incompatible assignment of '%s' in application of @%s",
            idl_identifier(cap->member->declarator),
            idl_identifier(a->annotation));
          return IDL_RETCODE_SEMANTIC_ERROR;
        }
        idl_unreference_node(cap);
      }
    }
    /* ensure all required parameters are provided */
    for (d = a->annotation->definitions; d; d = idl_next(d)) {
      if (!idl_is_annotation_member(d))
        continue;
      m = (idl_annotation_member_t *)d;
      if (m->const_expr || find(a, m))
        continue;
      idl_error(pstate, idl_location(a),
        "Missing assignment of '%s' in application of @%s",
        idl_identifier(m->declarator),
        idl_identifier(a->annotation));
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

  /* deduplication */
  for (idl_annotation_appl_t *a = appls; a; a = idl_next(a)) {
    idl_definition_t *d;
    idl_annotation_member_t *m;
    assert(a->annotation);
    for (idl_annotation_appl_t *na, *ca = idl_next(a); ca; ca = na) {
      na = idl_next(ca);
      if (ca->annotation != a->annotation)
        continue;
      for (d = a->annotation->definitions; d; d = idl_next(d)) {
        /* skip non-members */
        if (!idl_is_annotation_member(d))
          continue;
        m = (idl_annotation_member_t *)d;
        idl_const_expr_t *lval, *rval;
        idl_annotation_appl_param_t *ap;
        ap = find(a, m);
        lval = ap && ap->const_expr ? ap->const_expr : m->const_expr;
        ap = find(ca, m);
        rval = ap && ap->const_expr ? ap->const_expr : m->const_expr;
        if (lval != rval && idl_compare(lval, rval) != 0) {
          idl_error(pstate, idl_location(ap),
            "Incompatible reapplication of @%s",
            idl_identifier(a->annotation));
          return IDL_RETCODE_SEMANTIC_ERROR;
        }
      }
      idl_unreference_node(ca);
    }
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_annotate(
  idl_pstate_t *pstate,
  void *node,
  idl_annotation_appl_t *annotation_appls)
{
  idl_retcode_t ret;

  assert(pstate);
  assert(node);

  /* evaluate constant expressions */
  if ((ret = eval(pstate, node, annotation_appls)))
    return ret;
  /* discard redundant annotations and annotation parameters */
  if ((ret = dedup(pstate, node, annotation_appls)))
    return ret;

  for (idl_annotation_appl_t *a = annotation_appls; a; a = idl_next(a)) {
    idl_annotation_callback_t callback = a->annotation->callback;
    if (callback && (ret = callback(pstate, a, node)))
      return ret;
    a->node.parent = node;
  }

  ((idl_node_t *)node)->annotations = annotation_appls;
  return IDL_RETCODE_OK;
}
