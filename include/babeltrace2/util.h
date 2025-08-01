/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_UTIL_H
#define BABELTRACE2_UTIL_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-util General purpose utilities

@brief
    General purpose utilities.

This API offers general purpose utilities.
*/

/*! @{ */

/*!
@brief
    Status codes for bt_util_clock_cycles_to_ns_from_origin().
*/
typedef enum bt_util_clock_cycles_to_ns_from_origin_status {
	/*!
	@brief
	    Success.
	*/
	BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Integer overflow while computing the result.
	*/
	BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR	= __BT_FUNC_STATUS_OVERFLOW_ERROR,
} bt_util_clock_cycles_to_ns_from_origin_status;

/*!
@brief
    Converts the clock value \bt_p{cycles} from cycles to nanoseconds
    from the origin of the clock and sets \bt_p{*ns_from_origin} to the
    result.

This function considers the frequency of the clock in Hz
(\bt_p{frequency}), an offset from its origin in seconds
(\bt_p{offset_seconds}) which can be negative, and an additional offset
in cycles (\bt_p{offset_cycles}).

This function:

-# Converts the \bt_p{offset_cycles} value to seconds using
   \bt_p{frequency}.
-# Converts the \bt_p{cycles} value to seconds using \bt_p{frequency}.
-# Adds the values of step&nbsp;1, step&nbsp;2,
   and \bt_p{offset_seconds}.
-# Converts the value of step&nbsp;3 to nanoseconds and sets
   \bt_p{*ns_from_origin} to this result.

The following illustration shows the possible scenarios:

@image html clock-terminology.png

\bt_p{offset_seconds} can be negative. For example, considering:

- A 1000&nbsp;Hz clock.
- \bt_p{offset_seconds} set to −10&nbsp;seconds.
- \bt_p{offset_cycles} set to 500&nbsp;cycles
  (that is, 0.5&nbsp;seconds).
- \bt_p{cycles} set to 2000&nbsp;cycles (that is, 2&nbsp;seconds).

The computed value is −7.5&nbsp;seconds, so this function sets
\bt_p{*ns_from_origin} to −7,500,000,000.

This function can fail and return the
#BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR status
code if any step of the computation process causes an integer overflow.

@param[in] cycles
    Value of the clock (cycles).
@param[in] frequency
    Frequency of the clock (Hz, or cycles/second).
@param[in] offset_seconds
    Offset, in seconds, from the origin of the clock to add to
    \bt_p{cycles} (once converted to seconds).
@param[in] offset_cycles
    Offset, in cycles, to add to \bt_p{cycles}.
@param[out] ns_from_origin
    <strong>On success</strong>, \bt_p{*ns_from_origin} is \bt_p{cycles}
    converted to nanoseconds from origin considering the properties
    of the clock.

@retval #BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OK
    Success.
@retval #BT_UTIL_CLOCK_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR
    Integer overflow while computing the result.

@pre
    \bt_p{frequency} isn't 0.
@pre
    \bt_p{frequency} isn't <code>UINT64_C(-1)</code>.
@pre
    \bt_p{frequency} is greater than \bt_p{offset_cycles}.
@pre
    \bt_p{offset_cycles} is less than \bt_p{frequency}.
@bt_pre_not_null{ns_from_origin}

@sa bt_clock_class_cycles_to_ns_from_origin() &mdash;
    Converts a stream clock value from cycles to nanoseconds from the
    origin of a given clock class.
*/
bt_util_clock_cycles_to_ns_from_origin_status
bt_util_clock_cycles_to_ns_from_origin(uint64_t cycles,
		uint64_t frequency, int64_t offset_seconds,
		uint64_t offset_cycles, int64_t *ns_from_origin) __BT_NOEXCEPT;

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_UTIL_H */
