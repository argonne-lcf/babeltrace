/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_GRAPH_SELF_MESSAGE_ITERATOR_H
#define BABELTRACE2_GRAPH_SELF_MESSAGE_ITERATOR_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-self-msg-iter Self message iterator
@ingroup api-msg-iter-cls

@brief
    Private view of a \bt_msg_iter for methods.

The #bt_self_message_iterator type is a private view of a \bt_msg_iter
from within a \bt_msg_iter_cls method.

Borrow the \bt_comp which provides a message iterator with
bt_self_message_iterator_borrow_component().

Borrow the \bt_oport on which a message iterator operates with
bt_self_message_iterator_borrow_port().

Set and get user data attached to a message iterator with
bt_self_message_iterator_set_data() and
bt_self_message_iterator_get_data().

Check whether or not a message iterator is interrupted with
bt_self_message_iterator_is_interrupted().

Set whether or not a message iterator can seek forward with
bt_self_message_iterator_configuration_set_can_seek_forward().
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_self_message_iterator bt_self_message_iterator;

@brief
    Self \bt_msg_iter.

@typedef struct bt_self_message_iterator_configuration bt_self_message_iterator_configuration;

@brief
    Self \bt_msg_iter configuration.

@}
*/

/*!
@name Component access
@{
*/

/*!
@brief
    Borrows the \bt_comp which provides the \bt_msg_iter
    \bt_p{self_message_iterator}.

@param[in] self_message_iterator
    Message iterator instance.

@returns
    Component which provides \bt_p{self_message_iterator}.

@bt_pre_not_null{self_message_iterator}
*/
extern bt_self_component *
bt_self_message_iterator_borrow_component(
		bt_self_message_iterator *self_message_iterator) __BT_NOEXCEPT;

/*! @} */

/*!
@name Output port access
@{
*/

/*!
@brief
    Borrows the \bt_oport on which the \bt_msg_iter
    \bt_p{self_message_iterator} operates.

@param[in] self_message_iterator
    Message iterator instance.

@returns
    Output port on which \bt_p{self_message_iterator} operates.

@bt_pre_not_null{self_message_iterator}
*/
extern bt_self_component_port_output *
bt_self_message_iterator_borrow_port(
		bt_self_message_iterator *self_message_iterator)
		__BT_NOEXCEPT;

/*! @} */

/*!
@name User data
@{
*/

/*!
@brief
    Sets the user data of the \bt_msg_iter \bt_p{self_message_iterator}
    to \bt_p{data}.

@param[in] self_message_iterator
    Message iterator instance.
@param[in] user_data
    New user data of \bt_p{self_message_iterator}.

@bt_pre_not_null{self_message_iterator}

@sa bt_self_message_iterator_get_data() &mdash;
    Returns the user data of a message iterator.
*/
extern void bt_self_message_iterator_set_data(
		bt_self_message_iterator *self_message_iterator,
		void *user_data) __BT_NOEXCEPT;

/*!
@brief
    Returns the user data of the \bt_msg_iter
    \bt_p{self_message_iterator}.

@param[in] self_message_iterator
    Message iterator instance.

@returns
    User data of \bt_p{self_message_iterator}.

@bt_pre_not_null{self_message_iterator}

@sa bt_self_message_iterator_set_data() &mdash;
    Sets the user data of a message iterator.
*/
extern
void *bt_self_message_iterator_get_data(
		const bt_self_message_iterator *self_message_iterator)
		__BT_NOEXCEPT;

/*! @} */

/*!
@name Interruption query
@{
*/

/*!
@brief
    Returns whether or not the \bt_msg_iter \bt_p{self_message_iterator}
    is interrupted, that is, whether or not any of its \bt_p_intr
    is set.

@param[in] self_message_iterator
    Message iterator instance.

@returns
    #BT_TRUE if \bt_p{self_message_iterator} is interrupted (any of its
    interrupters is set).

@bt_pre_not_null{self_message_iterator}

@sa bt_graph_borrow_default_interrupter() &mdash;
    Borrows the default interrupter of a trace processing graph.
@sa bt_graph_add_interrupter() &mdash;
    Adds an interrupter to a graph.
*/
extern bt_bool bt_self_message_iterator_is_interrupted(
		const bt_self_message_iterator *self_message_iterator)
		__BT_NOEXCEPT;

/*! @} */

/*!
@name Configuration
@{
*/

/*!
@brief
    Sets whether or not the \bt_msg_iter of which the configuration
    is \bt_p{configuration} can seek forward.

A message iterator can seek forward if all the \bt_p_msg of its
message sequence have some \bt_cs.

@attention
    You may only call this function during the execution of the
    \ref api-msg-iter-cls-meth-init "initialization method" of a
    message iterator.

@param[in] configuration
    Configuration of the message iterator of which to set whether or
    not it can seek forward.
@param[in] can_seek_forward
    #BT_TRUE to make the message iterator of which the configuration is
    \bt_p{configuration} forward-seekable.

@bt_pre_not_null{configuration}

@sa bt_message_iterator_can_seek_forward() &mdash;
    Returns whether or not a message iterator can seek forward.
*/
extern void bt_self_message_iterator_configuration_set_can_seek_forward(
		bt_self_message_iterator_configuration *configuration,
		bt_bool can_seek_forward) __BT_NOEXCEPT;

/*! @} */

/*!
@name Effective Message Interchange Protocol (MIP) version access
@{
*/

/*!
@brief
    Returns the effective \bt_mip (MIP) version of the trace processing
    \bt_graph which contains the \bt_msg_iter
    \bt_p{self_message_iterator}.

@param[in] self_message_iterator
    Message iterator instance.

@returns
    Effective MIP version of the trace processing graph which
    contains \bt_p{self_message_iterator}.

@bt_since{1}

@bt_pre_not_null{self_message_iterator}
*/
extern uint64_t bt_self_message_iterator_get_graph_mip_version(
		const bt_self_message_iterator *self_message_iterator)
		__BT_NOEXCEPT;

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_SELF_MESSAGE_ITERATOR_H */
