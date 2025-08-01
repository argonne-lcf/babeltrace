/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_SELF_MESSAGE_ITERATOR_HPP
#define BABELTRACE_CPP_COMMON_BT2_SELF_MESSAGE_ITERATOR_HPP

#include <babeltrace2/babeltrace.h>

#include "common/common.h"

#include "borrowed-object.hpp"
#include "message-iterator.hpp"
#include "self-component-port.hpp"

namespace bt2 {

class SelfMessageIterator final : public BorrowedObject<bt_self_message_iterator>
{
public:
    explicit SelfMessageIterator(const LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    MessageIterator::Shared createMessageIterator(const SelfComponentInputPort port) const
    {
        bt_message_iterator *libMsgIterPtr = nullptr;
        const auto status = bt_message_iterator_create_from_message_iterator(
            this->libObjPtr(), port.libObjPtr(), &libMsgIterPtr);

        switch (status) {
        case BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK:
            return MessageIterator::Shared::createWithoutRef(libMsgIterPtr);
        case BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_MEMORY_ERROR:
            throw MemoryError {};
        case BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }

    SelfComponent component() const noexcept
    {
        return SelfComponent {bt_self_message_iterator_borrow_component(this->libObjPtr())};
    }

    SelfComponentOutputPort port() const noexcept
    {
        return SelfComponentOutputPort {bt_self_message_iterator_borrow_port(this->libObjPtr())};
    }

    bool isInterrupted() const noexcept
    {
        return static_cast<bool>(bt_self_message_iterator_is_interrupted(this->libObjPtr()));
    }

    std::uint64_t graphMipVersion() const noexcept
    {
        return bt_self_message_iterator_get_graph_mip_version(this->libObjPtr());
    }

    template <typename T>
    T& data() const noexcept
    {
        return *static_cast<T *>(bt_self_message_iterator_get_data(this->libObjPtr()));
    }

    template <typename T>
    SelfMessageIterator data(T& obj) const noexcept
    {
        bt_self_message_iterator_set_data(this->libObjPtr(),
                                          const_cast<void *>(static_cast<const void *>(&obj)));
        return *this;
    }

    StreamBeginningMessage::Shared createStreamBeginningMessage(const ConstStream stream) const
    {
        const auto libObjPtr =
            bt_message_stream_beginning_create(this->libObjPtr(), stream.libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return StreamBeginningMessage::Shared::createWithoutRef(libObjPtr);
    }

    StreamEndMessage::Shared createStreamEndMessage(const ConstStream stream) const
    {
        const auto libObjPtr = bt_message_stream_end_create(this->libObjPtr(), stream.libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return StreamEndMessage::Shared::createWithoutRef(libObjPtr);
    }

    EventMessage::Shared createEventMessage(const ConstEventClass eventCls,
                                            const ConstStream stream) const
    {
        const auto libObjPtr =
            bt_message_event_create(this->libObjPtr(), eventCls.libObjPtr(), stream.libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return EventMessage::Shared::createWithoutRef(libObjPtr);
    }

    EventMessage::Shared createEventMessage(const ConstEventClass eventCls,
                                            const ConstStream stream,
                                            const std::uint64_t clockSnapshotValue) const
    {
        const auto libObjPtr = bt_message_event_create_with_default_clock_snapshot(
            this->libObjPtr(), eventCls.libObjPtr(), stream.libObjPtr(), clockSnapshotValue);

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return EventMessage::Shared::createWithoutRef(libObjPtr);
    }

    EventMessage::Shared createEventMessage(const ConstEventClass eventCls,
                                            const ConstPacket packet) const
    {
        const auto libObjPtr = bt_message_event_create_with_packet(
            this->libObjPtr(), eventCls.libObjPtr(), packet.libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return EventMessage::Shared::createWithoutRef(libObjPtr);
    }

    EventMessage::Shared createEventMessage(const ConstEventClass eventCls,
                                            const ConstPacket packet,
                                            const std::uint64_t clockSnapshotValue) const
    {
        const auto libObjPtr = bt_message_event_create_with_packet_and_default_clock_snapshot(
            this->libObjPtr(), eventCls.libObjPtr(), packet.libObjPtr(), clockSnapshotValue);

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return EventMessage::Shared::createWithoutRef(libObjPtr);
    }

    PacketBeginningMessage::Shared createPacketBeginningMessage(const ConstPacket packet) const
    {
        const auto libObjPtr =
            bt_message_packet_beginning_create(this->libObjPtr(), packet.libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return PacketBeginningMessage::Shared::createWithoutRef(libObjPtr);
    }

    PacketBeginningMessage::Shared
    createPacketBeginningMessage(const ConstPacket packet,
                                 const std::uint64_t clockSnapshotValue) const
    {
        const auto libObjPtr = bt_message_packet_beginning_create_with_default_clock_snapshot(
            this->libObjPtr(), packet.libObjPtr(), clockSnapshotValue);

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return PacketBeginningMessage::Shared::createWithoutRef(libObjPtr);
    }

    PacketEndMessage::Shared createPacketEndMessage(const ConstPacket packet) const
    {
        const auto libObjPtr = bt_message_packet_end_create(this->libObjPtr(), packet.libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return PacketEndMessage::Shared::createWithoutRef(libObjPtr);
    }

    PacketEndMessage::Shared createPacketEndMessage(const ConstPacket packet,
                                                    const std::uint64_t clockSnapshotValue) const
    {
        const auto libObjPtr = bt_message_packet_end_create_with_default_clock_snapshot(
            this->libObjPtr(), packet.libObjPtr(), clockSnapshotValue);

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return PacketEndMessage::Shared::createWithoutRef(libObjPtr);
    }

    DiscardedEventsMessage::Shared createDiscardedEventsMessage(const ConstStream stream)
    {
        const auto libObjPtr =
            bt_message_discarded_events_create(this->libObjPtr(), stream.libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return DiscardedEventsMessage::Shared::createWithoutRef(libObjPtr);
    }

    DiscardedEventsMessage::Shared
    createDiscardedEventsMessage(const ConstStream stream,
                                 const std::uint64_t beginningClockSnapshotValue,
                                 const std::uint64_t endClockSnapshotValue) const
    {
        const auto libObjPtr = bt_message_discarded_events_create_with_default_clock_snapshots(
            this->libObjPtr(), stream.libObjPtr(), beginningClockSnapshotValue,
            endClockSnapshotValue);

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return DiscardedEventsMessage::Shared::createWithoutRef(libObjPtr);
    }

    DiscardedPacketsMessage::Shared createDiscardedPacketsMessage(const ConstStream stream) const
    {
        const auto libObjPtr =
            bt_message_discarded_packets_create(this->libObjPtr(), stream.libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return DiscardedPacketsMessage::Shared::createWithoutRef(libObjPtr);
    }

    DiscardedPacketsMessage::Shared
    createDiscardedPacketsMessage(const ConstStream stream,
                                  const std::uint64_t beginningClockSnapshotValue,
                                  const std::uint64_t endClockSnapshotValue) const
    {
        const auto libObjPtr = bt_message_discarded_packets_create_with_default_clock_snapshots(
            this->libObjPtr(), stream.libObjPtr(), beginningClockSnapshotValue,
            endClockSnapshotValue);

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return DiscardedPacketsMessage::Shared::createWithoutRef(libObjPtr);
    }

    MessageIteratorInactivityMessage::Shared
    createMessageIteratorInactivityMessage(const ConstClockClass clockClass,
                                           const std::uint64_t clockSnapshotValue) const
    {
        const auto libObjPtr = bt_message_message_iterator_inactivity_create(
            this->libObjPtr(), clockClass.libObjPtr(), clockSnapshotValue);

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return MessageIteratorInactivityMessage::Shared::createWithoutRef(libObjPtr);
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_SELF_MESSAGE_ITERATOR_HPP */
