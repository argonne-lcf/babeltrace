/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_GRAPH_PRIVATE_QUERY_EXECUTOR_H
#define BABELTRACE2_GRAPH_PRIVATE_QUERY_EXECUTOR_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-priv-qexec Private query executor
@ingroup api-comp-cls-dev

@brief
    Private view of a \bt_qexec for a \bt_comp_cls
    \ref api-comp-cls-dev-meth-query "query method".

A <strong><em>private query executor</em></strong> is a private view,
from within a \bt_comp_cls
\ref api-comp-cls-dev-meth-query "query method", of a
\bt_qexec.

A query method receives a private query executor as its
\bt_p{query_executor} parameter.

As of \bt_name_version_min_maj, this API only offers the
bt_private_query_executor_as_query_executor_const() function to
\ref api-fund-c-typing "upcast" a private query executor to a
\c const query executor. You need this to get the
\ref api-qexec-prop-log-lvl "logging level" of the query executor.
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_private_query_executor bt_private_query_executor;

@brief
    Private query executor.

@}
*/

/*!
@name Upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the private query executor
    \bt_p{query_executor} to the public #bt_query_executor type.

@param[in] query_executor
    @parblock
    Private query executor to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{query_executor} as a public query executor.
*/
static inline
const bt_query_executor *
bt_private_query_executor_as_query_executor_const(
		bt_private_query_executor *query_executor) __BT_NOEXCEPT
{
	return __BT_UPCAST_CONST(bt_query_executor, query_executor);
}

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_PRIVATE_QUERY_EXECUTOR_H */
