/*
 * sequence.c
 *
 * BabelTrace - Sequence Type Converter
 *
 * Copyright 2010, 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#include <babeltrace/compiler.h>
#include <babeltrace/format.h>

#ifndef max
#define max(a, b)	((a) < (b) ? (b) : (a))
#endif

static
struct definition *_sequence_definition_new(struct declaration *declaration,
					struct definition_scope *parent_scope,
					GQuark field_name, int index);
static
void _sequence_definition_free(struct definition *definition);

void sequence_copy(struct stream_pos *dest, const struct format *fdest, 
		   struct stream_pos *src, const struct format *fsrc,
		   struct definition *definition)
{
	struct definition_sequence *sequence =
		container_of(definition, struct definition_sequence, p);
	struct declaration_sequence *sequence_declaration = sequence->declaration;
	uint64_t i;

	fsrc->sequence_begin(src, sequence_declaration);
	fdest->sequence_begin(dest, sequence_declaration);

	sequence->len->p.declaration->copy(dest, fdest, src, fsrc,
				    &sequence->len->p);

	for (i = 0; i < sequence->len->value._unsigned; i++) {
		struct definition *elem =
			sequence->current_element.definition;
		elem->declaration->copy(dest, fdest, src, fsrc, elem);
	}
	fsrc->sequence_end(src, sequence_declaration);
	fdest->sequence_end(dest, sequence_declaration);
}

static
void _sequence_declaration_free(struct declaration *declaration)
{
	struct declaration_sequence *sequence_declaration =
		container_of(declaration, struct declaration_sequence, p);

	free_declaration_scope(sequence_declaration->scope);
	declaration_unref(&sequence_declaration->len_declaration->p);
	declaration_unref(sequence_declaration->elem);
	g_free(sequence_declaration);
}

struct declaration_sequence *
	sequence_declaration_new(struct declaration_integer *len_declaration,
			  struct declaration *elem_declaration,
			  struct declaration_scope *parent_scope)
{
	struct declaration_sequence *sequence_declaration;
	struct declaration *declaration;

	sequence_declaration = g_new(struct declaration_sequence, 1);
	declaration = &sequence_declaration->p;
	assert(!len_declaration->signedness);
	declaration_ref(&len_declaration->p);
	sequence_declaration->len_declaration = len_declaration;
	declaration_ref(elem_declaration);
	sequence_declaration->elem = elem_declaration;
	sequence_declaration->scope = new_declaration_scope(parent_scope);
	declaration->id = CTF_TYPE_SEQUENCE;
	declaration->alignment = max(len_declaration->p.alignment, elem_declaration->alignment);
	declaration->copy = sequence_copy;
	declaration->declaration_free = _sequence_declaration_free;
	declaration->definition_new = _sequence_definition_new;
	declaration->definition_free = _sequence_definition_free;
	declaration->ref = 1;
	return sequence_declaration;
}

static
struct definition *_sequence_definition_new(struct declaration *declaration,
				struct definition_scope *parent_scope,
				GQuark field_name, int index)
{
	struct declaration_sequence *sequence_declaration =
		container_of(declaration, struct declaration_sequence, p);
	struct definition_sequence *sequence;
	struct definition *len_parent;

	sequence = g_new(struct definition_sequence, 1);
	declaration_ref(&sequence_declaration->p);
	sequence->p.declaration = declaration;
	sequence->declaration = sequence_declaration;
	sequence->p.ref = 1;
	sequence->p.index = index;
	sequence->scope = new_definition_scope(parent_scope, field_name);
	len_parent = sequence_declaration->len_declaration->p.definition_new(&sequence_declaration->len_declaration->p,
				parent_scope,
				g_quark_from_static_string("length"), 0);
	sequence->len =
		container_of(len_parent, struct definition_integer, p);
	sequence->current_element.definition =
		sequence_declaration->elem->definition_new(sequence_declaration->elem,
				parent_scope,
				g_quark_from_static_string("[]"), 1);
	return &sequence->p;
}

static
void _sequence_definition_free(struct definition *definition)
{
	struct definition_sequence *sequence =
		container_of(definition, struct definition_sequence, p);
	struct definition *len_definition = &sequence->len->p;
	struct definition *elem_definition =
		sequence->current_element.definition;

	len_definition->declaration->definition_free(len_definition);
	elem_definition->declaration->definition_free(elem_definition);
	free_definition_scope(sequence->scope);
	declaration_unref(sequence->p.declaration);
	g_free(sequence);
}
