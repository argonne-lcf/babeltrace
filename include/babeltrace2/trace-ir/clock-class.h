/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_TRACE_IR_CLOCK_CLASS_H
#define BABELTRACE2_TRACE_IR_CLOCK_CLASS_H

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
@defgroup api-tir-clock-cls Clock class
@ingroup api-tir

@brief
    Class of \bt_stream clocks.

A <strong><em>clock class</em></strong> is the class of \bt_stream
clocks.

A clock class is a \ref api-tir "trace IR" metadata object.

<em>Stream clocks</em> only exist conceptually in \bt_name because
they're stateful objects. \bt_cp_msg cannot refer to stateful objects
because they must not change while being transported from one
\bt_comp to the other.

Instead of having a stream clock object, messages have a
default \bt_cs: this is a snapshot of the value of the
default clock of a stream (a clock class instance):

@image html clocks.png

In the illustration above, notice that:

- \bt_cp_stream (horizontal blue rectangles) are instances of a
  \bt_stream_cls (orange).

- A stream class has a default clock class (orange bell alarm clock).

- Each stream has a default clock (yellow bell alarm clock): this is an
  instance of the default clock class of the class of the stream.

- Each \bt_msg (objects in blue stream rectangles) created for a given
  stream has a default \bt_cs (yellow star): this is a snapshot of the
  default clock of the stream.

  In other words, a default clock snapshot contains the value of the
  default clock of the stream when this message occurred.

The default clock class property of a \bt_stream_cls is optional:
if a stream class has no default clock class, then its instances
(\bt_p_stream) have no default clock, therefore all the \bt_p_msg
created from this stream have no default clock snapshot.

A clock class is a \ref api-fund-shared-object "shared object": get a
new reference with bt_clock_class_get_ref() and put an existing
reference with bt_clock_class_put_ref().

Some library functions \ref api-fund-freezing "freeze" clock classes on
success. The documentation of those functions indicate this
postcondition.

The type of a clock class is #bt_clock_class.

Create a default clock class from a \bt_self_comp with
bt_clock_class_create().

Since Babeltrace&nbsp;2.1, get the effective \bt_mip version of
the trace processing \bt_graph containing the \bt_comp from which
a clock class was created with bt_clock_class_get_graph_mip_version().

<h1>\anchor api-tir-clock-cls-origin Clock value vs. clock origin</h1>

The value of a \bt_stream clock (a conceptual instance of a clock class)
is in <em>cycles</em>. This value is always positive and is relative to
the offset of the clock, which is itself relative to its origin.

The origin of a clock is one of, depending on its class:

<dl>
  <dt>If bt_clock_class_origin_is_known() returns #BT_FALSE</dt>
  <dd>
    Unknown.

    Two stream clocks of which the classes have an unknown
    origin only have a correlation if they share the same
    \link api-tir-clock-cls-prop-iden identity\endlink.

    Check whether or not two clock classes share the same identity
    with bt_clock_class_has_same_identity().
  </dd>

  <dt>If bt_clock_class_origin_is_unix_epoch() returns #BT_TRUE</dt>
  <dd>
    The
    <a href="https://en.wikipedia.org/wiki/Unix_time">Unix epoch</a>.

    All stream clocks with a Unix epoch origin, whatever their
    \link api-tir-clock-cls-prop-iden identity\endlink,
    have a correlation.
  </dd>

  <dt>
    Otherwise (only available when the clock class was created
    from a \bt_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;1;
    \bt_avail_since{1})
  </dt>
  <dd>
    The namespace, name, and
    <a href="https://en.wikipedia.org/wiki/Unique_identifier">unique identifier</a>
    (UID) tuple returned by
    bt_clock_class_get_origin_namespace(),
    bt_clock_class_get_origin_name(), and
    bt_clock_class_get_origin_uid().

    All stream clocks with the same custom origin, whatever their
    \link api-tir-clock-cls-prop-iden identity\endlink,
    have a correlation.

    Check whether or not two clock classes share the same identity
    with bt_clock_class_has_same_identity().
  </dd>
</dl>

To compute an effective stream clock value, in cycles from its origin:

-# Convert the
   \link api-tir-clock-cls-prop-offset "offset in seconds"\endlink
   property of the clock to cycles using its
   \ref api-tir-clock-cls-prop-freq "frequency".

-# Add the value of step&nbsp;1, the value of the stream clock,
   and the
   \link api-tir-clock-cls-prop-offset "offset in cycles"\endlink
   property of the clock.

Because typical tracer clocks have a high frequency (often 1&nbsp;GHz
and more), an effective stream clock value (cycles since Unix epoch, for
example) can be \em larger than \c UINT64_MAX. This is why a clock class
has two offset properties (one in seconds and one in cycles): to make it
possible for a stream clock to have smaller values, relative to this
offset.

The bt_clock_class_cycles_to_ns_from_origin(),
bt_util_clock_cycles_to_ns_from_origin(), and
bt_clock_snapshot_get_ns_from_origin() functions convert a stream clock
value (cycles) to an equivalent <em>nanoseconds from origin</em> value
using the relevant clock class properties (frequency and offset).

Those functions perform this computation:

-# Convert the "offset in cycles" property of the clock to seconds using
   its frequency.

-# Convert the value of the clock to seconds using its frequency.

-# Add the values of step&nbsp;1, step&nbsp;2, and the
   "offset in seconds" property of the clock.

-# Convert the value of step&nbsp;3 to nanoseconds.

The following illustration shows the possible scenarios:

@image html clock-terminology.png

The "offset in seconds" property of the clock can be negative. For
example, considering:

- Frequency: 1000&nbsp;Hz.
- Offset in seconds: −10&nbsp;seconds.
- Offset in cycles: 500&nbsp;cycles (that is, 0.5&nbsp;seconds).
- Stream clock value: 2000&nbsp;cycles (that is, 2&nbsp;seconds).

Then the computed value is −7.5&nbsp;seconds from origin, or
−7,500,000,000&nbsp;nanoseconds from origin.

<h1>Properties</h1>

A clock class has the following properties:

<dl>
  <dt>\anchor api-tir-clock-cls-prop-freq Frequency</dt>
  <dd>
    Frequency of the clock class instances (stream clocks)
    (cycles/second).

    Use bt_clock_class_set_frequency() and
    bt_clock_class_get_frequency().
  </dd>

  <dt>
    \anchor api-tir-clock-cls-prop-offset
    Offset (in seconds and in cycles)
  </dt>
  <dd>
    Offset in seconds relative to the
    \ref api-tir-clock-cls-origin "origin" of the clock class
    instances (stream clocks), and offset in cycles relative to the
    offset in seconds.

    The values of the clock class instances are relative to the
    computed offset.

    Use bt_clock_class_set_offset() and bt_clock_class_get_offset().
  </dd>

  <dt>
    \anchor api-tir-clock-cls-prop-precision Precision
    (optional when the clock class was created
    from a \bt_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;1;
    \bt_avail_since{1})
  </dt>
  <dd>
    Precision of the clock class instance (stream clocks) values
    (cycles).

    For example, considering a precision of 7&nbsp;cycles, an
    \link api-tir-clock-cls-prop-accuracy accuracy\endlink
    of 0&nbsp;cycles, and the stream
    clock value 42&nbsp;cycles, the real stream clock value can be
    anything between 35&nbsp;cycles and 49&nbsp;cycles.

    Use bt_clock_class_set_precision() and
    bt_clock_class_get_opt_precision().
  </dd>

  <dt>
    \anchor api-tir-clock-cls-prop-accuracy
    \bt_dt_opt Accuracy
    (only available when the clock class was created
    from a \bt_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip version&nbsp;1;
    \bt_avail_since{1})
  </dt>
  <dd>
    Accuracy of the clock class instance (stream clocks) values
    (cycles).

    For example, considering an accuracy of 7&nbsp;cycles, a
    \link api-tir-clock-cls-prop-precision precision\endlink
    of 0&nbsp;cycles, and the stream
    clock value 42&nbsp;cycles, the real stream clock value can be
    anything between 35&nbsp;cycles and 49&nbsp;cycles.

    Use bt_clock_class_set_accuracy() and
    bt_clock_class_get_accuracy().
  </dd>

  <dt>
    \anchor api-tir-clock-cls-prop-origin
    Origin
  </dt>
  <dd>
    Origin of the clock class instances (stream clocks).

    Depending on the effective \bt_mip (MIP) version of the trace
    processing \bt_graph:

    <dl>
      <dt>MIP&nbsp;0 or MIP&nbsp;1</dt>
      <dd>
        <dl>
          <dt>Unknown origin</dt>
          <dd>
            Use bt_clock_class_set_origin_unknown() and
            bt_clock_class_origin_is_known().
          </dd>

          <dt>Unix epoch origin</dt>
          <dd>
            Use bt_clock_class_set_origin_unix_epoch() and
            bt_clock_class_origin_is_unix_epoch(),
          </dd>
        </dl>
      </dd>

      <dt>MIP&nbsp;1: custom origin (\bt_avail_since{1})</dt>
      <dd>
        Use bt_clock_class_set_origin(),
        bt_clock_class_get_origin_namespace(),
        bt_clock_class_get_origin_name(), and
        bt_clock_class_get_origin_uid().
      </dd>
    </dl>
  </dd>

  <dt>\anchor api-tir-clock-cls-prop-iden \bt_dt_opt Identity</dt>
  <dd>
    Identity of the clock class instances (stream clocks).

    Depending on the effective \bt_mip (MIP) version of the trace
    processing \bt_graph:

    <dl>
      <dt>MIP&nbsp;0</dt>
      <dd>
        \anchor api-tir-clock-cls-prop-uuid
        The name and UUID property pair.

        A valid identity only requires the UUID property.

        Use bt_clock_class_set_name(), bt_clock_class_get_name(),
        bt_clock_class_set_uuid(), and bt_clock_class_get_uuid().
      </dd>

      <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
      <dd>
        The namespace, name, and UID property tuple.

        A valid identity only requires the name and UID properties.

        Use bt_clock_class_set_namespace(), bt_clock_class_get_namespace(),
        bt_clock_class_set_name(), bt_clock_class_get_name(),
        bt_clock_class_set_uid(), and bt_clock_class_get_uid().
      </dd>
    </dl>
  </dd>

  <dt>\anchor api-tir-clock-cls-prop-descr \bt_dt_opt Description</dt>
  <dd>
    Description of the clock class.

    Use bt_clock_class_set_description() and
    bt_clock_class_get_description().
  </dd>

  <dt>
    \anchor api-tir-clock-cls-prop-user-attrs
    \bt_dt_opt User attributes
  </dt>
  <dd>
    User attributes of the clock class.

    User attributes are custom attributes attached to a clock class.

    Use bt_clock_class_set_user_attributes(),
    bt_clock_class_borrow_user_attributes(), and
    bt_clock_class_borrow_user_attributes_const().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_clock_class bt_clock_class;

@brief
    Clock class.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a default clock class from the \bt_self_comp
    \bt_p{self_component}.

On success, the returned clock class has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-clock-cls-prop-freq "Frequency"
    <td>1&nbsp;GHz
  <tr>
    <td>\ref api-tir-clock-cls-prop-offset "Offset" in seconds
    <td>0&nbsp;seconds
  <tr>
    <td>\ref api-tir-clock-cls-prop-offset "Offset" in cycles
    <td>0&nbsp;cycles
  <tr>
    <td>\ref api-tir-clock-cls-prop-precision "Precision"
    <td>
      Depending on the effective \bt_mip (MIP) version of the trace
      processing \bt_graph:

      <dl>
        <dt>MIP&nbsp;0</dt>
        <dd>0&nbsp;cycles</dd>

        <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
        <dd>Unknown</dd>
      </dl>
  <tr>
    <td>
      \bt_mip version&nbsp;1:
      \ref api-tir-clock-cls-prop-accuracy "accuracy"
      (\bt_avail_since{1})
    <td>Unknown
  <tr>
    <td>\ref api-tir-clock-cls-prop-origin "Origin"
    <td>Unix epoch
  <tr>
    <td>\ref api-tir-clock-cls-prop-iden "Identity"
    <td>
      <em>None</em>, that is, depending on the effective \bt_mip (MIP)
      version of the trace processing \bt_graph:

      <dl>
        <dt>MIP&nbsp;0</dt>
        <dd>No name and no UUID</dd>

        <dt>MIP&nbsp;1 (\bt_avail_since{1})</dt>
        <dd>No namespace, no name, and no UID</dd>
      </dl>
  <tr>
    <td>\ref api-tir-clock-cls-prop-descr "Description"
    <td>\em None
  <tr>
    <td>\ref api-tir-clock-cls-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] self_component
    Self component from which to create the default clock class.

@returns
    New clock class reference, or \c NULL on memory error.

@bt_pre_not_null{self_component}
*/
extern bt_clock_class *bt_clock_class_create(bt_self_component *self_component)
		__BT_NOEXCEPT;

/*! @} */

/*!
@name Properties
@{
*/

/*!
@brief
    Sets the frequency (Hz) of the clock class \bt_p{clock_class} to
    \bt_p{frequency}.

See the \ref api-tir-clock-cls-prop-freq "frequency" property.

@param[in] clock_class
    Clock class of which to set the frequency to \bt_p{frequency}.
@param[in] frequency
    New frequency of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@pre
    \bt_p{frequency} isn't 0.
@pre
    \bt_p{frequency} isn't <code>UINT64_C(-1)</code>.
@pre
    \bt_p{frequency} is greater than the offset in cycles of the
    clock class
    (as returned by bt_clock_class_get_offset()).

@sa bt_clock_class_get_frequency() &mdash;
    Returns the frequency of a clock class.
*/
extern void bt_clock_class_set_frequency(bt_clock_class *clock_class,
		uint64_t frequency) __BT_NOEXCEPT;

/*!
@brief
    Returns the frequency (Hz) of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-freq "frequency" property.

@param[in] clock_class
    Clock class of which to get the frequency.

@returns
    Frequency (Hz) of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_frequency() &mdash;
    Sets the frequency of a clock class.
*/
extern uint64_t bt_clock_class_get_frequency(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Sets the offset of the clock class \bt_p{clock_class} to
    \bt_p{offset_seconds} plus \bt_p{offset_cycles} from its
    \ref api-tir-clock-cls-origin "origin".

See the \ref api-tir-clock-cls-prop-offset "offset" property.

@param[in] clock_class
    Clock class of which to set the offset to \bt_p{offset_seconds}
    and \bt_p{offset_cycles}.
@param[in] offset_seconds
    New offset in seconds of \bt_p{clock_class}.
@param[in] offset_cycles
    New offset in cycles of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@pre
    \bt_p{offset_cycles} is less than the frequency of the clock class
    (as returned by bt_clock_class_get_frequency()).

@sa bt_clock_class_get_offset() &mdash;
    Returns the offset of a clock class.
*/
extern void bt_clock_class_set_offset(bt_clock_class *clock_class,
		int64_t offset_seconds, uint64_t offset_cycles) __BT_NOEXCEPT;

/*!
@brief
    Returns the offsets in seconds and cycles of the clock class
    \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-offset "offset" property.

@param[in] clock_class
    Clock class of which to get the offset.
@param[out] offset_seconds
    When this function returns, \bt_p{*offset_seconds} is the offset in
    seconds of
    \bt_p{clock_class}.
@param[out] offset_cycles
    When this function returns, \bt_p{*offset_cycles} is the offset in
    cycles of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_not_null{offset_seconds}
@bt_pre_not_null{offset_cycles}

@sa bt_clock_class_set_offset() &mdash;
    Sets the offset of a clock class.
*/
extern void bt_clock_class_get_offset(const bt_clock_class *clock_class,
		int64_t *offset_seconds, uint64_t *offset_cycles) __BT_NOEXCEPT;

/*!
@brief
    Sets the precision (cycles) of the clock class \bt_p{clock_class} to
    \bt_p{precision}.

See the \ref api-tir-clock-cls-prop-precision "precision" property.

@param[in] clock_class
    Clock class of which to set the precision to \bt_p{precision}.
@param[in] precision
    New precision of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}

@sa bt_clock_class_get_precision() &mdash;
    Returns the precision of a clock class.
*/
extern void bt_clock_class_set_precision(bt_clock_class *clock_class,
		uint64_t precision) __BT_NOEXCEPT;

/*!
@brief
    Returns the precision (cycles) of the clock class
    \bt_p{clock_class}.

@deprecated
    Use bt_clock_class_get_opt_precision().

@note
    This function is only available when \bt_p{clock_class} was created
    from a \bt_comp which belongs to a trace processing \bt_graph with
    the effective \bt_mip (MIP) version&nbsp;0.

See the \ref api-tir-clock-cls-prop-precision "precision" property.

@param[in] clock_class
    Clock class of which to get the precision.

@returns
    Precision (cycles) of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 0}

@sa bt_clock_class_set_precision() &mdash;
    Sets the precision of a clock class.
*/
extern uint64_t bt_clock_class_get_precision(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Returns the precision of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-precision "precision" property.

@param[in] clock_class
    Clock class of which to get the precision.
@param[out] precision
    @parblock
    <strong>If this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, then \bt_p{*precision}
    is the precision (cycles) of \bt_p{clock_class}.

    Otherwise, the precision of \bt_p{clock_class} is unknown.
    @endparblock

@retval #BT_PROPERTY_AVAILABILITY_AVAILABLE
    The precision of \bt_p{clock_class} is known.
@retval #BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE
    The precision of \bt_p{clock_class} is unknown.

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_not_null{precision}

@sa bt_clock_class_set_precision() &mdash;
    Sets the precision of a clock class.
*/
extern bt_property_availability bt_clock_class_get_opt_precision(
		const struct bt_clock_class *clock_class,
		uint64_t *precision) __BT_NOEXCEPT;

/*!
@brief
    Sets the accuracy (cycles) of the clock class \bt_p{clock_class} to
    \bt_p{accuracy}.

See the \ref api-tir-clock-cls-prop-accuracy "accuracy" property.

@param[in] clock_class
    Clock class of which to set the accuracy to \bt_p{accuracy}.
@param[in] accuracy
    New accuracy of \bt_p{clock_class}.

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 1}

@sa bt_clock_class_get_accuracy() &mdash;
    Returns the accuracy of a clock class.
*/
extern void bt_clock_class_set_accuracy(bt_clock_class *clock_class,
		uint64_t accuracy) __BT_NOEXCEPT;

/*!
@brief
    Returns the accuracy of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-accuracy "accuracy" property.

@param[in] clock_class
    Clock class of which to get the accuracy.
@param[out] accuracy
    @parblock
    <strong>If this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*accuracy} is
    the accuracy (cycles) of \bt_p{clock_class}.

    Otherwise, the accuracy of \bt_p{clock_class} is unknown.
    @endparblock

@retval #BT_PROPERTY_AVAILABILITY_AVAILABLE
    The accuracy of \bt_p{clock_class} is known.
@retval #BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE
    The accuracy of \bt_p{clock_class} is unknown.

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_not_null{accuracy}

@sa bt_clock_class_set_accuracy() &mdash;
    Sets the accuracy of a clock class.
*/
extern bt_property_availability bt_clock_class_get_accuracy(
		const struct bt_clock_class *clock_class,
		uint64_t *accuracy) __BT_NOEXCEPT;

/*!
@brief
    Sets whether the \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class} is unknown or the
    <a href="https://en.wikipedia.org/wiki/Unix_time">Unix epoch</a>.

@deprecated
    Use bt_clock_class_set_origin_unknown() or
    bt_clock_class_set_origin_unix_epoch().

See the \ref api-tir-clock-cls-prop-origin "origin" property.

@param[in] clock_class
    Clock class of which to set whether its origin is unknown or
    the Unix epoch.
@param[in] origin_is_unix_epoch
    @parblock
    One of:

    <dl>
      <dt>#BT_FALSE</dt>
      <dd>Make the origin of \bt_p{clock_class} unknown.</dd>

      <dt>#BT_TRUE</dt>
      <dd>Make the origin of \bt_p{clock_class} the Unix epoch.</dd>
    </dl>
    @endparblock

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}

@sa bt_clock_class_origin_is_unix_epoch() &mdash;
    Returns whether or not the origin of a clock class is the
    Unix epoch.
*/
extern void bt_clock_class_set_origin_is_unix_epoch(bt_clock_class *clock_class,
		bt_bool origin_is_unix_epoch) __BT_NOEXCEPT;

/*!
@brief
    Makes the \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class} unknown.

See the \ref api-tir-clock-cls-prop-origin "origin" property.

@param[in] clock_class
    Clock class of which to make the origin unknown.

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}

@sa bt_clock_class_set_origin_unix_epoch() &mdash;
    Makes the origin of a clock class the Unix epoch.
@sa bt_clock_class_set_origin() &mdash;
    Sets the custom origin of a clock class.
@sa bt_clock_class_origin_is_known() &mdash;
    Returns whether or not the origin of a clock class is known.
*/
extern void
bt_clock_class_set_origin_unknown(bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Makes the \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class} the
    <a href="https://en.wikipedia.org/wiki/Unix_time">Unix epoch</a>.

See the \ref api-tir-clock-cls-prop-origin "origin" property.

@param[in] clock_class
    Clock class of which to make the origin the Unix epoch.

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}

@sa bt_clock_class_set_origin_unknown() &mdash;
    Makes the origin of a clock class unknown.
@sa bt_clock_class_set_origin() &mdash;
    Sets the custom origin of a clock class.
@sa bt_clock_class_origin_is_unix_epoch() &mdash;
    Returns whether or not the origin of a clock class is the
    Unix epoch.
*/
extern void
bt_clock_class_set_origin_unix_epoch(bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_clock_class_set_origin().

@bt_since{1}
*/
typedef enum bt_clock_class_set_origin_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_CLASS_SET_ORIGIN_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_CLOCK_CLASS_SET_ORIGIN_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_clock_class_set_origin_status;

/*!
@brief
    Sets the custom \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class} to the
    \bt_p{ns}, \bt_p{name}, and \bt_p{uid} tuple.

See the \ref api-tir-clock-cls-prop-origin "origin" property.

@param[in] clock_class
    Clock class of which to set the origin to the
    \bt_p{ns}, \bt_p{name}, and \bt_p{uid} tuple.
@param[in] ns
    @parblock
    Namespace of the custom origin of \bt_p{clock_class} (copied).

    Can be \c NULL.
    @endparblock
@param[in] name
    Name of the custom origin of \bt_p{clock_class} (copied).
@param[in] uid
    <a href="https://en.wikipedia.org/wiki/Unique_identifier">Unique identifier</a>
    (UID) of the custom origin of \bt_p{clock_class} (copied).

@retval #BT_CLOCK_CLASS_SET_ORIGIN_STATUS_OK
    Success.
@retval #BT_CLOCK_CLASS_SET_ORIGIN_STATUS_MEMORY_ERROR
    Out of memory.

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 1}
@bt_pre_not_null{name}
@bt_pre_not_null{uid}

@post
    bt_clock_class_origin_is_known() returns #BT_TRUE with
    \bt_p{clock_class}.
@post
    bt_clock_class_origin_is_unix_epoch() returns #BT_FALSE with
    \bt_p{clock_class}.

@sa bt_clock_class_set_origin_unknown() &mdash;
    Makes the origin of a clock class unknown.
@sa bt_clock_class_set_origin_unix_epoch() &mdash;
    Makes the origin of a clock class the Unix epoch.
@sa bt_clock_class_get_origin_namespace() &mdash;
    Returns the namespace of the custom origin of a clock class.
@sa bt_clock_class_get_origin_name() &mdash;
    Returns the name of the custom origin of a clock class.
@sa bt_clock_class_get_origin_uid() &mdash;
    Returns the UID of the custom origin of a clock class.
*/
extern bt_clock_class_set_origin_status bt_clock_class_set_origin(
		bt_clock_class *clock_class, const char *ns, const char *name,
		const char *uid) __BT_NOEXCEPT;

/*!
@brief
    Returns whether or not the \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class} is known.

See the \ref api-tir-clock-cls-prop-origin "origin" property.

The origin of \bt_p{clock_class} is known if it's either the Unix epoch
or custom.

@param[in] clock_class
    Clock class of which to get whether or not its origin is known.

@returns
    #BT_TRUE if the origin of \bt_p{clock_class} is known.

@bt_since{1}

@bt_pre_not_null{clock_class}

@sa bt_clock_class_origin_is_unix_epoch() &mdash;
    Returns whether or not the origin of a clock class is the
    Unix epoch.
@sa bt_clock_class_set_origin_unknown() &mdash;
    Makes the origin of a clock class unknown.
@sa bt_clock_class_set_origin_unix_epoch() &mdash;
    Makes the origin of a clock class the Unix epoch.
@sa bt_clock_class_set_origin() &mdash;
    Sets the custom origin of a clock class.
*/
extern bt_bool bt_clock_class_origin_is_known(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Returns whether or not the \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class} is the
    <a href="https://en.wikipedia.org/wiki/Unix_time">Unix epoch</a>.

See the \ref api-tir-clock-cls-prop-origin "origin" property.

@param[in] clock_class
    Clock class of which to get whether or not its origin is the
    Unix epoch.

@returns
    #BT_TRUE if the origin of \bt_p{clock_class} is the Unix epoch.

@bt_since{1}

@bt_pre_not_null{clock_class}

@sa bt_clock_class_origin_is_known() &mdash;
    Returns whether or not the origin of a clock class is known.
@sa bt_clock_class_set_origin_unknown() &mdash;
    Makes the origin of a clock class unknown.
@sa bt_clock_class_set_origin_unix_epoch() &mdash;
    Makes the origin of a clock class the Unix epoch.
@sa bt_clock_class_set_origin() &mdash;
    Sets the custom origin of a clock class.
*/
extern bt_bool bt_clock_class_origin_is_unix_epoch(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Returns the namespace of the custom
    \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-origin "origin" property.

@param[in] clock_class
    Clock class of which to get the custom origin namespace.

@returns
    @parblock
    Custom origin namespace of \bt_p{clock_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{clock_class}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 1}
@pre
    The origin of \bt_p{clock_class} is custom:
    bt_clock_class_origin_is_known() returns #BT_TRUE and
    bt_clock_class_origin_is_unix_epoch() returns #BT_FALSE with
    \bt_p{clock_class}.

@sa bt_clock_class_get_origin_name() &mdash;
    Returns the name of the custom origin of a clock class.
@sa bt_clock_class_get_origin_uid() &mdash;
    Returns the UID of the custom origin of a clock class.
@sa bt_clock_class_set_origin() &mdash;
    Sets the custom origin of a clock class.
*/
extern const char *bt_clock_class_get_origin_namespace(
	const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Returns the name of the custom
    \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-origin "origin" property.

@param[in] clock_class
    Clock class of which to get the custom origin name.

@returns
    @parblock
    Custom origin name of \bt_p{clock_class}, or \c NULL if none.

    If this function doesn't return \c NULL, then
    bt_clock_class_get_origin_uid() also doesn't return \c NULL.

    The returned pointer remains valid as long as \bt_p{clock_class}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 1}
@pre
    The origin of \bt_p{clock_class} is custom:
    bt_clock_class_origin_is_known() returns #BT_TRUE and
    bt_clock_class_origin_is_unix_epoch() returns #BT_FALSE with
    \bt_p{clock_class}.

@sa bt_clock_class_get_origin_namespace() &mdash;
    Returns the namespace of the custom origin of a clock class.
@sa bt_clock_class_get_origin_uid() &mdash;
    Returns the UID of the custom origin of a clock class.
@sa bt_clock_class_set_origin() &mdash;
    Sets the custom origin of a clock class.
*/
extern const char *bt_clock_class_get_origin_name(
	const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Returns the UID of the custom
    \ref api-tir-clock-cls-origin "origin"
    of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-origin "origin" property.

@param[in] clock_class
    Clock class of which to get the custom origin UID.

@returns
    @parblock
    Custom origin UID of \bt_p{clock_class}, or \c NULL if none.

    If this function doesn't return \c NULL, then
    bt_clock_class_get_origin_name() also doesn't return \c NULL.

    The returned pointer remains valid as long as \bt_p{clock_class}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 1}
@pre
    The origin of \bt_p{clock_class} is custom:
    bt_clock_class_origin_is_known() returns #BT_TRUE and
    bt_clock_class_origin_is_unix_epoch() returns #BT_FALSE with
    \bt_p{clock_class}.

@sa bt_clock_class_get_origin_namespace() &mdash;
    Returns the namespace of the custom origin of a clock class.
@sa bt_clock_class_get_origin_name() &mdash;
    Returns the name of the custom origin of a clock class.
@sa bt_clock_class_set_origin() &mdash;
    Sets the custom origin of a clock class.
*/
extern const char *bt_clock_class_get_origin_uid(
	const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_clock_class_set_namespace().

@bt_since{1}
*/
typedef enum bt_clock_class_set_namespace_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_CLASS_SET_NAMESPACE_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_CLOCK_CLASS_SET_NAMESPACE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_clock_class_set_namespace_status;

/*!
@brief
    Sets the namespace of the clock class \bt_p{clock_class} to
    a copy of \bt_p{ns}.

See the \ref api-tir-clock-cls-prop-iden "identity" property.

@param[in] clock_class
    Clock class of which to set the namespace to \bt_p{ns}.
@param[in] ns
    New namespace of \bt_p{clock_class} (copied).

@retval #BT_CLOCK_CLASS_SET_NAMESPACE_STATUS_OK
    Success.
@retval #BT_CLOCK_CLASS_SET_NAMESPACE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 1}
@bt_pre_not_null{namespace}

@sa bt_clock_class_get_namespace() &mdash;
    Returns the namespace of a clock class.
*/
extern bt_clock_class_set_namespace_status bt_clock_class_set_namespace(
		bt_clock_class *clock_class, const char *ns) __BT_NOEXCEPT;

/*!
@brief
    Returns the namespace of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-iden "identity" property.

If \bt_p{clock_class} has no namespace, then this function
returns \c NULL.

@param[in] clock_class
    Clock class of which to get the namespace.

@returns
    @parblock
    Namespace of \bt_p{clock_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{clock_class}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 1}

@sa bt_clock_class_set_namespace() &mdash;
    Sets the namespace of a clock class.
*/
extern const char *bt_clock_class_get_namespace(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_clock_class_set_name().
*/
typedef enum bt_clock_class_set_name_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_CLASS_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_CLOCK_CLASS_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_clock_class_set_name_status;

/*!
@brief
    Sets the name of the clock class \bt_p{clock_class} to
    a copy of \bt_p{name}.

See the \ref api-tir-clock-cls-prop-iden "identity" property.

@param[in] clock_class
    Clock class of which to set the name to \bt_p{name}.
@param[in] name
    New name of \bt_p{clock_class} (copied).

@retval #BT_CLOCK_CLASS_SET_NAME_STATUS_OK
    Success.
@retval #BT_CLOCK_CLASS_SET_NAME_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_not_null{name}

@sa bt_clock_class_get_name() &mdash;
    Returns the name of a clock class.
*/
extern bt_clock_class_set_name_status bt_clock_class_set_name(
		bt_clock_class *clock_class, const char *name) __BT_NOEXCEPT;

/*!
@brief
    Returns the name of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-iden "identity" property.

If \bt_p{clock_class} has no name, then this function returns \c NULL.

@param[in] clock_class
    Clock class of which to get the name.

@returns
    @parblock
    Name of \bt_p{clock_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{clock_class}
    isn't modified.
    @endparblock

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_name() &mdash;
    Sets the name of a clock class.
*/
extern const char *bt_clock_class_get_name(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_clock_class_set_uid().

@bt_since{1}
*/
typedef enum bt_clock_class_set_uid_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_CLASS_SET_UID_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_CLOCK_CLASS_SET_UID_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_clock_class_set_uid_status;

/*!
@brief
    Sets the
    <a href="https://en.wikipedia.org/wiki/Unique_identifier">unique identifier</a>
    (UID) of the clock class \bt_p{clock_class} to a copy of \bt_p{uid}.

See the \ref api-tir-clock-cls-prop-iden "identity" property.

@param[in] clock_class
    Clock class of which to set the UID to \bt_p{uid}.
@param[in] uid
    New UID of \bt_p{clock_class} (copied).

@retval #BT_CLOCK_CLASS_SET_UID_STATUS_OK
    Success.
@retval #BT_CLOCK_CLASS_SET_UID_STATUS_MEMORY_ERROR
    Out of memory.

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 1}
@bt_pre_not_null{uid}

@sa bt_clock_class_get_uid() &mdash;
    Returns the UID of a clock class.
*/
extern bt_clock_class_set_uid_status bt_clock_class_set_uid(
		bt_clock_class *clock_class, const char *uid) __BT_NOEXCEPT;

/*!
@brief
    Returns the UID of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-iden "identity" property.

If \bt_p{clock_class} has no UID, then this function returns \c NULL.

@param[in] clock_class
    Clock class of which to get the UID.

@returns
    @parblock
    UID of \bt_p{clock_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{clock_class}
    isn't modified.
    @endparblock

@bt_since{1}

@bt_pre_not_null{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 1}

@sa bt_clock_class_set_uid() &mdash;
    Sets the UID of a clock class.
*/
extern const char *
bt_clock_class_get_uid(const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Returns whether or not the clock classes \bt_p{clock_class_a}
    and \bt_p{clock_class_b} share the same identity.

See the \ref api-tir-clock-cls-prop-iden "identity" property.

Two clock classes share the same identity when all the following are
true:

- They both have a name and a UID.

- The values of their namespace, name, and UID property tuples
  are the same.

@param[in] clock_class_a
    Clock class A.
@param[in] clock_class_b
    Clock class B.

@returns
    #BT_TRUE if \bt_p{clock_class_a} and \bt_p{clock_class_b} share
    the same identity

@bt_since{1}

@bt_pre_not_null{clock_class_a}
@bt_pre_clock_cls_with_mip{clock_class_a, 1}
@bt_pre_not_null{clock_class_b}
@bt_pre_clock_cls_with_mip{clock_class_b, 1}
*/
extern bt_bool bt_clock_class_has_same_identity(
		const bt_clock_class *clock_class_a,
		const bt_clock_class *clock_class_b) __BT_NOEXCEPT;

/*!
@brief
    Status codes for bt_clock_class_set_description().
*/
typedef enum bt_clock_class_set_description_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_clock_class_set_description_status;

/*!
@brief
    Sets the
    <a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUID</a>
    of the clock class \bt_p{clock_class} to a copy of \bt_p{uuid}.

@note
    This function is only available when \bt_p{clock_class} was
    created from a \bt_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip (MIP) version&nbsp;0.

See the \ref api-tir-clock-cls-prop-iden "identity" property.

@param[in] clock_class
    Clock class of which to set the UUID to \bt_p{uuid}.
@param[in] uuid
    New UUID of \bt_p{clock_class} (copied).

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 0}
@bt_pre_not_null{uuid}

@sa bt_clock_class_get_uuid() &mdash;
    Returns the UUID of a clock class.
*/
extern void bt_clock_class_set_uuid(bt_clock_class *clock_class,
		bt_uuid uuid) __BT_NOEXCEPT;

/*!
@brief
    Returns the UUID of the clock class \bt_p{clock_class}.

@note
    This function is only available when \bt_p{clock_class} was
    created from a \bt_comp which belongs to a trace processing \bt_graph
    with the effective \bt_mip (MIP) version&nbsp;0.

See the \ref api-tir-clock-cls-prop-iden "identity" property.

If \bt_p{clock_class} has no UUID, then this function returns \c NULL.

@param[in] clock_class
    Clock class of which to get the UUID.

@returns
    @parblock
    UUID of \bt_p{clock_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{clock_class}
    isn't modified.
    @endparblock

@bt_pre_not_null{clock_class}
@bt_pre_clock_cls_with_mip{clock_class, 0}

@sa bt_clock_class_set_uuid() &mdash;
    Sets the UUID of a clock class.
*/
extern bt_uuid bt_clock_class_get_uuid(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Sets the description of the clock class \bt_p{clock_class} to a copy
    of \bt_p{description}.

See the \ref api-tir-clock-cls-prop-descr "description" property.

@param[in] clock_class
    Clock class of which to set the description to \bt_p{description}.
@param[in] description
    New description of \bt_p{clock_class} (copied).

@retval #BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_OK
    Success.
@retval #BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_not_null{description}

@sa bt_clock_class_get_description() &mdash;
    Returns the description of a clock class.
*/
extern bt_clock_class_set_description_status bt_clock_class_set_description(
		bt_clock_class *clock_class, const char *description)
		__BT_NOEXCEPT;

/*!
@brief
    Returns the description of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-descr "description" property.

If \bt_p{clock_class} has no description, then this function
returns \c NULL.

@param[in] clock_class
    Clock class of which to get the description.

@returns
    @parblock
    Description of \bt_p{clock_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{clock_class}
    isn't modified.
    @endparblock

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_description() &mdash;
    Sets the description of a clock class.
*/
extern const char *bt_clock_class_get_description(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Sets the user attributes of the clock class \bt_p{clock_class} to
    \bt_p{user_attributes}.

See the \ref api-tir-clock-cls-prop-user-attrs "user attributes"
property.

@note
    When you create a default clock class with bt_clock_class_create(),
    the initial user attributes of the clock class is an empty
    \bt_map_val. Therefore you can borrow it with
    bt_clock_class_borrow_user_attributes() and fill it directly instead
    of setting a new one with this function.

@param[in] clock_class
    Clock class of which to set the user attributes to
    \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{clock_class}.

@bt_pre_not_null{clock_class}
@bt_pre_hot{clock_class}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_clock_class_borrow_user_attributes() &mdash;
    Borrows the user attributes of a clock class.
*/
extern void bt_clock_class_set_user_attributes(
		bt_clock_class *clock_class, const bt_value *user_attributes)
		__BT_NOEXCEPT;

/*!
@brief
    Borrows the user attributes of the clock class \bt_p{clock_class}.

See the \ref api-tir-clock-cls-prop-user-attrs "user attributes"
property.

@note
    When you create a default clock class with bt_clock_class_create(),
    the initial user attributes of the clock class is an empty
    \bt_map_val.

@param[in] clock_class
    Clock class from which to borrow the user attributes.

@returns
    User attributes of \bt_p{clock_class} (a \bt_map_val).

@bt_pre_not_null{clock_class}

@sa bt_clock_class_set_user_attributes() &mdash;
    Sets the user attributes of a clock class.
@sa bt_clock_class_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_clock_class_borrow_user_attributes(
		bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Borrows the user attributes of the clock class \bt_p{clock_class}
    (\c const version).

See bt_clock_class_borrow_user_attributes().
*/
extern const bt_value *bt_clock_class_borrow_user_attributes_const(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Effective Message Interchange Protocol (MIP) version access
@{
*/

/*!
@brief
    Returns the effective \bt_mip (MIP) version of the trace processing
    \bt_graph containing the \bt_comp from which
    \bt_p{clock_class} was created.

@param[in] clock_class
    Clock class of which to get the effective MIP version.

@returns
    Effective MIP version of \bt_p{clock_class}.

@bt_since{1}

@bt_pre_not_null{clock_class}

@sa bt_self_component_get_graph_mip_version() &mdash;
    Returns the effective MIP version of the trace processing
    graph which contains a given component.
*/
extern uint64_t bt_clock_class_get_graph_mip_version(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*! @} */

/*!
@name Utilities
@{
*/

/*!
@brief
    Status codes for bt_clock_class_cycles_to_ns_from_origin().
*/
typedef enum bt_clock_class_cycles_to_ns_from_origin_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_CLASS_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Integer overflow while computing the result.
	*/
	BT_CLOCK_CLASS_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR	= __BT_FUNC_STATUS_OVERFLOW_ERROR,
} bt_clock_class_cycles_to_ns_from_origin_status;

/*!
@brief
    Converts the stream clock value \bt_p{value} from cycles to
    nanoseconds from the \ref api-tir-clock-cls-origin "origin" of the
    clock class \bt_p{clock_class} and sets \bt_p{*ns_from_origin}
    to the result.

This function:

-# Converts the
   \link api-tir-clock-cls-prop-offset "offset in cycles"\endlink
   property of \bt_p{clock_class} to seconds using its
   \ref api-tir-clock-cls-prop-freq "frequency".
-# Converts the \bt_p{value} value to seconds using the frequency of
   \bt_p{clock_class}.
-# Adds the values of step&nbsp;1, step&nbsp;2, and the
   \link api-tir-clock-cls-prop-offset "offset in seconds"\endlink
   property of \bt_p{clock_class}.
-# Converts the value of step&nbsp;3 to nanoseconds and sets
   \bt_p{*ns_from_origin} to this result.

The following illustration shows the possible scenarios:

@image html clock-terminology.png

This function can fail and return the
#BT_CLOCK_CLASS_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR status
code if any step of the computation process causes an integer overflow.

@param[in] clock_class
    Class of the stream clock.
@param[in] value
    Value of the stream clock (cycles) to convert to nanoseconds from
    the origin of \bt_p{clock_class}.
@param[out] ns_from_origin
    <strong>On success</strong>, \bt_p{*ns_from_origin} is \bt_p{value}
    converted to nanoseconds from the origin of \bt_p{clock_class}.

@retval #BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OK
    Success.
@retval #BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR
    Integer overflow while computing the result.

@bt_pre_not_null{clock_class}
@bt_pre_not_null{ns_from_origin}

@sa bt_util_clock_cycles_to_ns_from_origin() &mdash;
    Converts a clock value from cycles to nanoseconds from the origin
    of the clock.
*/
extern bt_clock_class_cycles_to_ns_from_origin_status
bt_clock_class_cycles_to_ns_from_origin(
		const bt_clock_class *clock_class,
		uint64_t value, int64_t *ns_from_origin) __BT_NOEXCEPT;

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the clock class \bt_p{clock_class}.

@param[in] clock_class
    @parblock
    Clock class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_clock_class_put_ref() &mdash;
    Decrements the reference count of a clock class.
*/
extern void bt_clock_class_get_ref(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the clock class \bt_p{clock_class}.

@param[in] clock_class
    @parblock
    Clock class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_clock_class_get_ref() &mdash;
    Increments the reference count of a clock class.
*/
extern void bt_clock_class_put_ref(
		const bt_clock_class *clock_class) __BT_NOEXCEPT;

/*!
@brief
    Decrements the reference count of the clock class
    \bt_p{_clock_class}, and then sets \bt_p{_clock_class} to \c NULL.

@param _clock_class
    @parblock
    Clock class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_clock_class}
*/
#define BT_CLOCK_CLASS_PUT_REF_AND_RESET(_clock_class)	\
	do {						\
		bt_clock_class_put_ref(_clock_class);	\
		(_clock_class) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the clock class \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a clock class reference from the expression
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
#define BT_CLOCK_CLASS_MOVE_REF(_dst, _src)	\
	do {					\
		bt_clock_class_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_CLOCK_CLASS_H */
