/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_TRACE_IR_TRACE_H
#define BABELTRACE2_TRACE_IR_TRACE_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-tir-trace Trace
@ingroup api-tir

@brief
    Trace (set of \bt_p_stream).

A <strong><em>trace</em></strong> is a set of \bt_p_stream with
properties:

@image html trace-structure.png

In the illustration above, notice that a trace is an instance of a
\bt_trace_cls and that it contains streams.

Borrow the class of a trace with bt_trace_borrow_class() and
bt_trace_borrow_class_const().

A trace is a \ref api-tir "trace IR" data object.

A trace is a \ref api-fund-shared-object "shared object": get a
new reference with bt_trace_get_ref() and put an existing
reference with bt_trace_put_ref().

Some library functions \ref api-fund-freezing "freeze" traces on
success. The documentation of those functions indicate this
postcondition. With a frozen trace, you can still:

- Create \bt_p_stream from it with bt_stream_create() or
  bt_stream_create_with_id().
- Add a destruction listener to it with
  bt_trace_add_destruction_listener().

The type of a trace is #bt_trace.

A trace contains \bt_p_stream. All the streams of a
given trace have unique \ref api-tir-stream-prop-id "numeric IDs".
Get the number of streams in a trace with bt_trace_get_stream_count().
Borrow a specific stream from a trace with
bt_trace_borrow_stream_by_index(),
bt_trace_borrow_stream_by_index_const(), bt_trace_borrow_stream_by_id(),
or bt_trace_borrow_stream_by_id_const().

Create a default trace from a \bt_trace_cls with bt_trace_create().

Add to and remove a destruction listener from a trace with
bt_trace_add_destruction_listener() and
bt_trace_remove_destruction_listener().

<h1>Properties</h1>

A trace has the following properties:

<dl>
  <dt>
    \anchor api-tir-trace-prop-ns
    \bt_dt_opt Namespace
    (only available when the class of the trace was created
    from a \bt_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;1; \bt_avail_since{1})
  </dt>
  <dd>
    Namespace of the trace.

    Use bt_trace_set_namespace() and
    bt_trace_get_namespace().
  </dd>

  <dt>
    \anchor api-tir-trace-prop-name
    \bt_dt_opt Name
  </dt>
  <dd>
    Name of the trace.

    Use bt_trace_set_name() and bt_trace_get_name().
  </dd>

  <dt>
    \bt_dt_opt Depending on the effective \bt_mip (MIP) version of the
    trace processing \bt_graph:
  </dt>
  <dd>
    <dl>
      <dt>
        \anchor api-tir-trace-prop-uuid
        MIP&nbsp;0: UUID
      </dt>
      <dd>
        <a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUID</a>
        of the trace.

        The UUID of the trace uniquely identifies the trace.

        Use bt_trace_set_uuid() and bt_trace_get_uuid().
      </dd>

      <dt>
        \anchor api-tir-trace-prop-uid
        MIP&nbsp;1: UID (\bt_avail_since{1})
      </dt>
      <dd>
        <a href="https://en.wikipedia.org/wiki/Unique_identifier">Unique identifier</a>
        (UID) of the trace.

        The combination of the
        \link api-tir-trace-prop-name name\endlink and UID
        of the trace uniquely identifies it.

        Use bt_trace_set_uid() and bt_trace_get_uid().
      </dd>
    </dl>
  </dd>
  <dt>
    \anchor api-tir-trace-prop-env
    \bt_dt_opt Environment
  </dt>
  <dd>
    Generic key-value store which describes the environment of the trace
    (for example, the hostname of the system, its network address, the
    name and version of the tracer, and the rest).

    Trace environment keys are strings while values are signed integers
    or strings.

    Set the value of a trace environment entry with
    bt_trace_set_environment_entry_integer() and
    bt_trace_set_environment_entry_string().

    Get the number of environment entries in a trace with
    bt_trace_get_environment_entry_count().

    Borrow an environment entry from a trace with
    bt_trace_borrow_environment_entry_value_by_name_const().
  </dd>

  <dt>
    \anchor api-tir-trace-prop-user-attrs
    \bt_dt_opt User attributes
  </dt>
  <dd>
    User attributes of the trace.

    User attributes are custom attributes attached to a trace.

    Use bt_trace_set_user_attributes(),
    bt_trace_borrow_user_attributes(), and
    bt_trace_borrow_user_attributes_const().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_trace bt_trace;

@brief
    Trace.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a default trace from the \bt_trace_cls \bt_p{trace_class}.

This function instantiates \bt_p{trace_class}.

On success, the returned trace has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\bt_mip version&nbsp;1: \ref api-tir-trace-prop-ns "Namespace" (\bt_avail_since{1})
    <td>\em None
  <tr>
    <td>\ref api-tir-trace-prop-name "Name"
    <td>\em None
  <tr>
    <td>MIP&nbsp;0: \ref api-tir-trace-prop-uuid "UUID"
    <td>\em None
  <tr>
    <td>MIP&nbsp;1: \ref api-tir-trace-prop-uid "UID" (\bt_avail_since{1})
    <td>\em None
  <tr>
    <td>\ref api-tir-trace-prop-env "Environment"
    <td>Empty
  <tr>
    <td>\ref api-tir-trace-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class from which to create the default trace.

@returns
    New trace reference, or \c NULL on memory error.
*/
extern bt_trace *bt_trace_create(bt_trace_class *trace_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Class access
@{
*/

/*!
@brief
    Borrows the \ref api-tir-trace-cls "class" of the trace
    \bt_p{trace}.

@param[in] trace
    Trace of which to borrow the class.

@returns
    \em Borrowed reference of the class of \bt_p{trace}.

@bt_pre_not_null{trace}

@sa bt_trace_borrow_class_const() &mdash;
    \c const version of this function.
*/
extern bt_trace_class *bt_trace_borrow_class(bt_trace *trace) __BT_NOEXCEPT;

/*!
@brief
    Borrows the \ref api-tir-trace-cls "class" of the trace
    \bt_p{trace} (\c const version).

See bt_trace_borrow_class().
*/
extern const bt_trace_class *bt_trace_borrow_class_const(
		const bt_trace *trace) __BT_NOEXCEPT;

/*! @} */

/*!
@name Stream access
@{
*/

/*!
@brief
    Returns the number of \bt_p_stream contained in the trace
    \bt_p{trace}.

@param[in] trace
    Trace of which to get the number of contained streams.

@returns
    Number of contained streams in \bt_p{trace}.

@bt_pre_not_null{trace}
*/
extern uint64_t bt_trace_get_stream_count(const bt_trace *trace) __BT_NOEXCEPT;

/*!
@brief
    Borrows the \bt_stream at index \bt_p{index} from the
    trace \bt_p{trace}.

@param[in] trace
    Trace from which to borrow the stream at index
    \bt_p{index}.
@param[in] index
    Index of the stream to borrow from \bt_p{trace}.

@returns
    @parblock
    \em Borrowed reference of the stream of
    \bt_p{trace} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{trace}
    exists.
    @endparblock

@bt_pre_not_null{trace}
@pre
    \bt_p{index} is less than the number of streams in
    \bt_p{trace} (as returned by
    bt_trace_get_stream_count()).

@sa bt_trace_get_stream_count() &mdash;
    Returns the number of streams contained in a trace.
@sa bt_trace_borrow_stream_by_index_const() &mdash;
    \c const version of this function.
*/
extern bt_stream *bt_trace_borrow_stream_by_index(bt_trace *trace,
		uint64_t index) __BT_NOEXCEPT;

/*!
@brief
    Borrows the \bt_stream at index \bt_p{index} from the
    trace \bt_p{trace} (\c const version).

See bt_trace_borrow_stream_by_index().
*/
extern const bt_stream *bt_trace_borrow_stream_by_index_const(
		const bt_trace *trace, uint64_t index) __BT_NOEXCEPT;

/*!
@brief
    Borrows the \bt_stream having the numeric ID \bt_p{id} from the
    trace \bt_p{trace}.

If there's no stream having the numeric ID \bt_p{id} in
\bt_p{trace}, then this function returns \c NULL.

@param[in] trace
    Trace from which to borrow the stream having the
    numeric ID \bt_p{id}.
@param[in] id
    ID of the stream to borrow from \bt_p{trace}.

@returns
    @parblock
    \em Borrowed reference of the stream of
    \bt_p{trace} having the numeric ID \bt_p{id}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{trace}
    exists.
    @endparblock

@bt_pre_not_null{trace}

@sa bt_trace_borrow_stream_by_id_const() &mdash;
    \c const version of this function.
*/
extern bt_stream *bt_trace_borrow_stream_by_id(bt_trace *trace,
		uint64_t id) __BT_NOEXCEPT;

/*!
@brief
    Borrows the \bt_stream having the numeric ID \bt_p{id} from the
    trace \bt_p{trace} (\c const version).

See bt_trace_borrow_stream_by_id().
*/
extern const bt_stream *bt_trace_borrow_stream_by_id_const(
		const bt_trace *trace, uint64_t id) __BT_NOEXCEPT;

/*! @} */

/*!
@name Properties
@{
*/

/*!
@brief
    Status codes for bt_trace_set_namespace().

@bt_since{1}
*/
typedef enum bt_trace_set_namespace_status {
	/*!
	@brief
	    Success.
	*/
	BT_TRACE_SET_NAMESPACE_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_TRACE_SET_NAMESPACE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_trace_set_namespace_status;

/*!
@brief
    Sets the namespace of the trace \bt_p{trace} to a copy of \bt_p{ns}.

See the \ref api-tir-trace-prop-ns "namespace" property.

@param[in] trace
    Trace of which to set the namespace to \bt_p{ns}.
@param[in] ns
    New namespace of \bt_p{trace} (copied).

@retval #BT_TRACE_SET_NAMESPACE_STATUS_OK
    Success.
@retval #BT_TRACE_SET_NAMESPACE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_since{1}

@bt_pre_not_null{trace}
@bt_pre_hot{trace}
@bt_pre_trace_with_mip{trace, 1}
@bt_pre_not_null{ns}

@sa bt_trace_get_namespace() &mdash;
    Returns the namespace of a trace.
*/
extern bt_trace_set_namespace_status bt_trace_set_namespace(bt_trace *trace,
		const char *ns) __BT_NOEXCEPT;

/*!
@brief
    Returns the namespace of the trace \bt_p{trace}.

See the \ref api-tir-trace-prop-ns "namespace" property.

If \bt_p{trace} has no namespace, then this function returns \c NULL.

@param[in] trace
    Trace of which to get the namespace.

@returns
    @parblock
    Namespace of \bt_p{trace}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{trace}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{trace}
@bt_pre_stream_cls_with_mip{trace, 1}

@sa bt_trace_set_namespace() &mdash;
    Sets the namespace of a trace.
*/
extern const char *bt_trace_get_namespace(const bt_trace *trace) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_trace_set_name().
*/
typedef enum bt_trace_set_name_status {
	/*!
	@brief
	    Success.
	*/
	BT_TRACE_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_TRACE_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_trace_set_name_status;

/*!
@brief
    Sets the name of the trace \bt_p{trace} to a copy of \bt_p{name}.

See the \ref api-tir-trace-prop-name "name" property.

@param[in] trace
    Trace of which to set the name to \bt_p{name}.
@param[in] name
    New name of \bt_p{trace} (copied).

@retval #BT_TRACE_SET_NAME_STATUS_OK
    Success.
@retval #BT_TRACE_SET_NAME_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{trace}
@bt_pre_hot{trace}
@bt_pre_not_null{name}

@sa bt_trace_get_name() &mdash;
    Returns the name of a trace.
*/
extern bt_trace_set_name_status bt_trace_set_name(bt_trace *trace,
		const char *name) __BT_NOEXCEPT;

/*!
@brief
    Returns the name of the trace \bt_p{trace}.

See the \ref api-tir-trace-prop-name "name" property.

If \bt_p{trace} has no name, then this function returns \c NULL.

@param[in] trace
    Trace of which to get the name.

@returns
    @parblock
    Name of \bt_p{trace}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{trace}
    isn't modified.
    @endparblock

@bt_pre_not_null{trace}

@sa bt_trace_set_name() &mdash;
    Sets the name of a trace.
*/
extern const char *bt_trace_get_name(const bt_trace *trace) __BT_NOEXCEPT;

/*!
@brief
    Sets the
    <a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUID</a>
    of the trace \bt_p{trace} to a copy of \bt_p{uuid}.

@note
    @parblock
    This function is only available when the class of \bt_p{trace} was
    created from a \bt_comp which belongs to a trace processing
    \bt_graph with the effective \bt_mip (MIP) version&nbsp;0.

    With MIP&nbsp;1, see the
    \ref api-tir-trace-prop-uid "UID" property.
    @endparblock

See the \ref api-tir-trace-prop-uuid "UUID" property.

@param[in] trace
    Trace of which to set the UUID to \bt_p{uuid}.
@param[in] uuid
    New UUID of \bt_p{trace} (copied).

@bt_pre_not_null{trace}
@bt_pre_hot{trace}
@bt_pre_trace_with_mip{trace, 0}
@bt_pre_not_null{uuid}

@sa bt_trace_get_uuid() &mdash;
    Returns the UUID of a trace.
*/
extern void bt_trace_set_uuid(bt_trace *trace, bt_uuid uuid) __BT_NOEXCEPT;

/*!
@brief
    Returns the UUID of the trace \bt_p{trace}.

@note
    @parblock
    This function is only available when the class of \bt_p{trace} was
    created from a \bt_comp which belongs to a trace processing
    \bt_graph with the effective \bt_mip (MIP) version&nbsp;0.

    With MIP&nbsp;1, see the
    \ref api-tir-trace-prop-uid "UID" property.
    @endparblock

See the \ref api-tir-trace-prop-uuid "UUID" property.

If \bt_p{trace} has no UUID, then this function returns \c NULL.

@param[in] trace
    Trace of which to get the UUID.

@returns
    @parblock
    UUID of \bt_p{trace}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{trace}
    isn't modified.
    @endparblock

@bt_pre_not_null{trace}
@bt_pre_trace_with_mip{trace, 0}

@sa bt_trace_set_uuid() &mdash;
    Sets the UUID of a trace.
*/
extern bt_uuid bt_trace_get_uuid(const bt_trace *trace) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_trace_set_uid().

@bt_since{1}
*/
typedef enum bt_trace_set_uid_status {
	/*!
	@brief
	    Success.
	*/
	BT_TRACE_SET_UID_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_TRACE_SET_UID_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_trace_set_uid_status;

/*!
@brief
    Sets the
    <a href="https://en.wikipedia.org/wiki/Unique_identifier">unique identifier</a>
    (UID) of \bt_p{trace} to a copy of \bt_p{uid}.

See the \ref api-tir-trace-prop-uid "UID" property.

@param[in] trace
    Trace of which to set the UID to \bt_p{uid}.
@param[in] uid
    New UID of \bt_p{trace} (copied).

@retval #BT_TRACE_SET_UID_STATUS_OK
    Success.
@retval #BT_TRACE_SET_UID_STATUS_MEMORY_ERROR
    Out of memory.

@bt_since{1}

@bt_pre_not_null{trace}
@bt_pre_hot{trace}
@bt_pre_trace_with_mip{trace, 1}
@bt_pre_not_null{uid}

@sa bt_trace_get_uid() &mdash;
    Returns the UID of a trace.
*/
extern bt_trace_set_uid_status bt_trace_set_uid(bt_trace *trace, const char *uid);

/*!
@brief
    Returns the UID of the trace \bt_p{trace}.

See the \ref api-tir-trace-prop-uid "UID" property.

If \bt_p{trace} has no UID, then this function returns \c NULL.

@param[in] trace
    Trace of which to get the UID.

@returns
    @parblock
    UID of \bt_p{trace}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{trace}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{trace}
@bt_pre_trace_with_mip{trace, 1}

@sa bt_trace_set_uid() &mdash;
    Sets the UID of a trace.
*/
extern const char *bt_trace_get_uid(const bt_trace *trace);

/*!
@brief
    Status codes for bt_trace_set_name().
*/
typedef enum bt_trace_set_environment_entry_status {
	/*!
	@brief
	    Success.
	*/
	BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_trace_set_environment_entry_status;

/*!
@brief
    Sets the value of the environment entry of the trace \bt_p{trace}
    named \bt_p{name} to the signed integer \bt_p{value}.

See the \ref api-tir-trace-prop-env "environment" property.

On success, if \bt_p{trace} already contains an environment entry named
\bt_p{name}, this function replaces the value of the existing entry with
\bt_p{value}.

@param[in] trace
    Trace in which to insert or replace an environment entry named
    \bt_p{name} with the value \bt_p{value}.
@param[in] name
    Name of the entry to insert or replace in \bt_p{trace} (copied).
@param[in] value
    Value of the environment entry to insert or replace in \bt_p{trace}.

@retval #BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_OK
    Success.
@retval #BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{trace}
@bt_pre_hot{trace}
@bt_pre_not_null{name}

@sa bt_trace_set_environment_entry_string() &mdash;
    Sets the value of a trace environment entry to a string.
*/
extern bt_trace_set_environment_entry_status
bt_trace_set_environment_entry_integer(bt_trace *trace, const char *name,
		int64_t value) __BT_NOEXCEPT;

/*!
@brief
    Sets the value of the environment entry of the trace \bt_p{trace}
    named \bt_p{name} to the string \bt_p{value}.

See the \ref api-tir-trace-prop-env "environment" property.

On success, if \bt_p{trace} already contains an environment entry named
\bt_p{name}, this function replaces the value of the existing entry with
\bt_p{value}.

@param[in] trace
    Trace in which to insert or replace an environment entry named
    \bt_p{name} with the value \bt_p{value}.
@param[in] name
    Name of the entry to insert or replace in \bt_p{trace} (copied).
@param[in] value
    Value of the environment entry to insert or replace in \bt_p{trace}.

@retval #BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_OK
    Success.
@retval #BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{trace}
@bt_pre_hot{trace}
@bt_pre_not_null{name}
@bt_pre_not_null{value}

@sa bt_trace_set_environment_entry_integer() &mdash;
    Sets the value of a trace environment entry to a signed integer.
*/
extern bt_trace_set_environment_entry_status
bt_trace_set_environment_entry_string(bt_trace *trace, const char *name,
		const char *value) __BT_NOEXCEPT;

/*!
@brief
    Returns the number of environment entries contained in the trace
    \bt_p{trace}.

See the \ref api-tir-trace-prop-env "environment" property.

@param[in] trace
    Trace of which to get the number of environment entries.

@returns
    Number of environment entries in \bt_p{trace}.

@bt_pre_not_null{trace}
*/
extern uint64_t bt_trace_get_environment_entry_count(
		const bt_trace *trace) __BT_NOEXCEPT;

/*!
@brief
    Borrows the environment entry at index \bt_p{index} from the
    trace \bt_p{trace}, setting \bt_p{*name} to its name and
    \bt_p{*value} to its value.

See the \ref api-tir-trace-prop-env "environment" property.

@param[in] trace
    Trace from which to borrow the environment entry at index
    \bt_p{index}.
@param[in] index
    Index of the environment entry to borrow from \bt_p{trace}.
@param[in] name
    @parblock
    <strong>On success</strong>, \bt_p{*name} is the name of the
    environment entry at index \bt_p{index} in \bt_p{trace}.

    The returned pointer remains valid as long as \bt_p{trace}
    isn't modified.
    @endparblock
@param[in] value
    @parblock
    <strong>On success</strong>, \bt_p{*value} is a \em borrowed
    reference of the environment entry at index \bt_p{index} in
    \bt_p{trace}.

    \bt_p{*value} is either a \bt_sint_val
    (#BT_VALUE_TYPE_SIGNED_INTEGER) or a \bt_string_val
    (#BT_VALUE_TYPE_STRING).

    The returned pointer remains valid as long as \bt_p{trace}
    isn't modified.
    @endparblock

@bt_pre_not_null{trace}
@pre
    \bt_p{index} is less than the number of environment entries in
    \bt_p{trace} (as returned by
    bt_trace_get_environment_entry_count()).
@bt_pre_not_null{name}
@bt_pre_not_null{value}

@sa bt_trace_get_environment_entry_count() &mdash;
    Returns the number of environment entries contained in a trace.
*/
extern void bt_trace_borrow_environment_entry_by_index_const(
		const bt_trace *trace, uint64_t index,
		const char **name, const bt_value **value) __BT_NOEXCEPT;

/*!
@brief
    Borrows the value of the environment entry named \bt_p{name}
    in the trace \bt_p{trace}.

See the \ref api-tir-trace-prop-env "environment" property.

If there's no environment entry named \bt_p{name} in \bt_p{trace}, then
this function returns \c NULL.

@param[in] trace
    Trace from which to borrow the value of the environment entry
    named \bt_p{name}.
@param[in] name
    Name of the environment entry to borrow from \bt_p{trace}.

@returns
    @parblock
    \em Borrowed reference of the value of the environment entry named
    \bt_p{name} in \bt_p{trace}.

    The returned value is either a \bt_sint_val
    (#BT_VALUE_TYPE_SIGNED_INTEGER) or a \bt_string_val
    (#BT_VALUE_TYPE_STRING).

    The returned pointer remains valid as long as \bt_p{trace}
    isn't modified.
    @endparblock

@bt_pre_not_null{trace}
@bt_pre_not_null{name}
*/
extern const bt_value *bt_trace_borrow_environment_entry_value_by_name_const(
		const bt_trace *trace, const char *name) __BT_NOEXCEPT;

/*!
@brief
    Sets the user attributes of the trace \bt_p{trace} to
    \bt_p{user_attributes}.

See the \ref api-tir-trace-prop-user-attrs "user attributes"
property.

@note
    When you create a default trace with bt_trace_create(), the
    initial user attributes of the trace is an empty \bt_map_val.
    Therefore you can borrow it with bt_trace_borrow_user_attributes()
    and fill it directly instead of setting a new one with this
    function.

@param[in] trace
    Trace of which to set the user attributes to \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{trace}.

@bt_pre_not_null{trace}
@bt_pre_hot{trace}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_trace_borrow_user_attributes() &mdash;
    Borrows the user attributes of a trace.
*/
extern void bt_trace_set_user_attributes(
		bt_trace *trace, const bt_value *user_attributes) __BT_NOEXCEPT;

/*!
@brief
    Borrows the user attributes of the trace \bt_p{trace}.

See the \ref api-tir-trace-prop-user-attrs "user attributes"
property.

@note
    When you create a default trace with bt_trace_create(), the
    initial user attributes of the trace is an empty \bt_map_val.

@param[in] trace
    Trace from which to borrow the user attributes.

@returns
    User attributes of \bt_p{trace} (a \bt_map_val).

@bt_pre_not_null{trace}

@sa bt_trace_set_user_attributes() &mdash;
    Sets the user attributes of a trace.
@sa bt_trace_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_trace_borrow_user_attributes(bt_trace *trace) __BT_NOEXCEPT;

/*!
@brief
    Borrows the user attributes of the trace \bt_p{trace}
    (\c const version).

See bt_trace_borrow_user_attributes().
*/
extern const bt_value *bt_trace_borrow_user_attributes_const(
		const bt_trace *trace) __BT_NOEXCEPT;

/*! @} */

/*!
@name Listeners
@{
*/

/*!
@brief
    User function for bt_trace_add_destruction_listener().

This is the user function type for a trace destruction listener.

@param[in] trace
    Trace being destroyed (\ref api-fund-freezing "frozen").
@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_trace_add_destruction_listener().

@bt_pre_not_null{trace}

@post
    The reference count of \bt_p{trace} isn't changed.
@bt_post_no_error

@sa bt_trace_add_destruction_listener() &mdash;
    Adds a destruction listener to a trace.
*/
typedef void (* bt_trace_destruction_listener_func)(
		const bt_trace *trace, void *user_data);

/*!
@brief
    Status codes for bt_trace_add_destruction_listener().
*/
typedef enum bt_trace_add_listener_status {
	/*!
	@brief
	    Success.
	*/
	BT_TRACE_ADD_LISTENER_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_TRACE_ADD_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_trace_add_listener_status;

/*!
@brief
    Adds a destruction listener having the function \bt_p{user_func}
    to the trace \bt_p{trace}.

All the destruction listener user functions of a trace are called
when it's being destroyed.

If \bt_p{listener_id} isn't \c NULL, then this function, on success,
sets \bt_p{*listener_id} to the ID of the added destruction listener
within \bt_p{trace}. You can then use this ID to remove the
added destruction listener with bt_trace_remove_destruction_listener().

@param[in] trace
    Trace to add the destruction listener to.
@param[in] user_func
    User function of the destruction listener to add to
    \bt_p{trace}.
@param[in] user_data
    User data to pass as the \bt_p{user_data} parameter of
    \bt_p{user_func}.
@param[out] listener_id
    <strong>On success and if not \c NULL</strong>, \bt_p{*listener_id}
    is the ID of the added destruction listener within
    \bt_p{trace}.

@retval #BT_TRACE_ADD_LISTENER_STATUS_OK
    Success.
@retval #BT_TRACE_ADD_LISTENER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{trace}
@bt_pre_not_null{user_func}

@sa bt_trace_remove_destruction_listener() &mdash;
    Removes a destruction listener from a trace.
*/
extern bt_trace_add_listener_status bt_trace_add_destruction_listener(
		const bt_trace *trace,
		bt_trace_destruction_listener_func user_func,
		void *user_data, bt_listener_id *listener_id) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_trace_remove_destruction_listener().
*/
typedef enum bt_trace_remove_listener_status {
	/*!
	@brief
	    Success.
	*/
	BT_TRACE_REMOVE_LISTENER_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_TRACE_REMOVE_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_trace_remove_listener_status;

/*!
@brief
    Removes the destruction listener having the ID \bt_p{listener_id}
    from the trace \bt_p{trace}.

The destruction listener to remove from \bt_p{trace} was
previously added with bt_trace_add_destruction_listener().

You can call this function when \bt_p{trace} is
\ref api-fund-freezing "frozen".

@param[in] trace
    Trace from which to remove the destruction listener having
    the ID \bt_p{listener_id}.
@param[in] listener_id
    ID of the destruction listener to remove from \bt_p{trace}­.

@retval #BT_TRACE_REMOVE_LISTENER_STATUS_OK
    Success.
@retval #BT_TRACE_REMOVE_LISTENER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{trace}
@pre
    \bt_p{listener_id} is the ID of an existing destruction listener
    in \bt_p{trace}.

@sa bt_trace_add_destruction_listener() &mdash;
    Adds a destruction listener to a trace.
*/
extern bt_trace_remove_listener_status bt_trace_remove_destruction_listener(
		const bt_trace *trace, bt_listener_id listener_id)
		__BT_NOEXCEPT;

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the trace \bt_p{trace}.

@param[in] trace
    @parblock
    Trace of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_trace_put_ref() &mdash;
    Decrements the reference count of a trace.
*/
extern void bt_trace_get_ref(const bt_trace *trace) __BT_NOEXCEPT;

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the trace \bt_p{trace}.

@param[in] trace
    @parblock
    Trace of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_trace_get_ref() &mdash;
    Increments the reference count of a trace.
*/
extern void bt_trace_put_ref(const bt_trace *trace) __BT_NOEXCEPT;

/*!
@brief
    Decrements the reference count of the trace
    \bt_p{_trace}, and then sets \bt_p{_trace} to \c NULL.

@param _trace
    @parblock
    Trace of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_trace}
*/
#define BT_TRACE_PUT_REF_AND_RESET(_trace)		\
	do {						\
		bt_trace_put_ref(_trace);		\
		(_trace) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the trace \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a trace reference from the expression
\bt_p{_src} to the expression \bt_p{_dst}, putting the existing
\bt_p{_dst} reference.

@param _dst
    @parblock
    Destination expression.

    Can contain \c NULL.
    @endparblock
@param _src
    @parblock
    Source expression.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_dst}
@bt_pre_assign_expr{_src}
*/
#define BT_TRACE_MOVE_REF(_dst, _src)		\
	do {					\
		bt_trace_put_ref(_dst);		\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_TRACE_H */
