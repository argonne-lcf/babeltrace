/*
 * Common Trace Format
 *
 * Strings read/write functions.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/ctf-text/types.h>
#include <stdio.h>
#include <limits.h>		/* C99 limits */
#include <string.h>

void ctf_text_string_write(struct stream_pos *ppos,
		      struct definition *definition)
{
	struct definition_string *string_definition =
		container_of(definition, struct definition_string, p);
	struct ctf_text_stream_pos *pos = ctf_text_pos(ppos);

	assert(string_definition->value != NULL);
	if (pos->dummy)
		return;
	print_pos_tabs(pos);
	fprintf(pos->fp, "%s\n", string_definition->value);
}
