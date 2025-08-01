/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2024 EfficiOS Inc.
 */

#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/component-class.hpp"
#include "cpp-common/bt2/error.hpp"
#include "cpp-common/bt2/graph.hpp"
#include "cpp-common/bt2/plugin-load.hpp"
#include "cpp-common/bt2c/call.hpp"
#include "cpp-common/vendor/fmt/format.h" /* IWYU pragma: keep */

#include "common.hpp"

#include "tap/tap.h"

namespace {

/* The types of messages a `TestSourceIter` is instructed to send. */
enum class MsgType
{
    /* Send stream beginning and stream end messages. */
    Stream,

    /* Send a message iterator inactivity message. */
    MsgIterInactivity,
};

__attribute__((used)) const char *format_as(MsgType msgType) noexcept
{
    switch (msgType) {
    case MsgType::Stream:
        return "stream beginning/end";

    case MsgType::MsgIterInactivity:
        return "message iterator inactivity";
    }

    bt_common_abort();
}

using CreateClockClass = bt2::ClockClass::Shared (*)(bt2::SelfComponent);

struct TestSourceData final
{
    /* The function to call to obtain a clock class. */
    CreateClockClass createClockClass;

    /* The type of messages to send. */
    MsgType msgType;

    /* If not empty, the clock snapshot to set on the message. */
    bt2s::optional<std::uint64_t> clockSnapshot;
};

class TestSource;

class TestSourceIter final : public bt2::UserMessageIterator<TestSourceIter, TestSource>
{
    friend bt2::UserMessageIterator<TestSourceIter, TestSource>;

public:
    explicit TestSourceIter(const bt2::SelfMessageIterator self,
                            bt2::SelfMessageIteratorConfiguration,
                            const bt2::SelfComponentOutputPort port) :
        bt2::UserMessageIterator<TestSourceIter, TestSource> {self, "TEST-SRC-MSG-ITER"},
        _mData {&port.data<const TestSourceData>()}, _mSelf {self}
    {
    }

private:
    void _next(bt2::ConstMessageArray& msgs)
    {
        if (_mDone) {
            return;
        }

        const auto clockCls = _mData->createClockClass(_mSelf.component());

        switch (_mData->msgType) {
        case MsgType::Stream:
        {
            const auto traceCls = _mSelf.component().createTraceClass();
            const auto streamCls = traceCls->createStreamClass();

            if (clockCls) {
                streamCls->defaultClockClass(*clockCls);
            }

            const auto stream = streamCls->instantiate(*traceCls->instantiate());

            /* Create stream beginning message. */
            msgs.append(bt2c::call([&] {
                const auto streamBeginningMsg = this->_createStreamBeginningMessage(*stream);

                /* Set clock snapshot if instructed to. */
                if (_mData->clockSnapshot) {
                    streamBeginningMsg->defaultClockSnapshot(*_mData->clockSnapshot);
                }

                return streamBeginningMsg;
            }));

            /*
             * The iterator needs to send a stream end message to avoid
             * a postcondition assertion failure, where it's ended but
             * didn't end all streams.
             *
             * The stream end messages don't play a role in the test
             * otherwise.
             */
            msgs.append(this->_createStreamEndMessage(*stream));
            break;
        }

        case MsgType::MsgIterInactivity:
            msgs.append(
                this->_createMessageIteratorInactivityMessage(*clockCls, *_mData->clockSnapshot));
            break;
        }

        _mDone = true;
    }

    bool _mDone = false;
    const TestSourceData *_mData;
    bt2::SelfMessageIterator _mSelf;
};

class TestSource final : public bt2::UserSourceComponent<TestSource, TestSourceIter, TestSourceData>
{
    friend class TestSourceIter;

    using _ThisUserSourceComponent =
        bt2::UserSourceComponent<TestSource, TestSourceIter, TestSourceData>;

public:
    static constexpr auto name = "test-source";

    explicit TestSource(const bt2::SelfSourceComponent self, bt2::ConstMapValue,
                        TestSourceData * const data) :
        _ThisUserSourceComponent {self, "TEST-SRC"},
        /* Don't use brace initialization, because of gcc 4.8. */
        _mData(*data)
    {
        this->_addOutputPort("out", _mData);
    }

private:
    TestSourceData _mData;
};

template <typename DerivedT>
class TestCase
{
public:
    explicit TestCase(const CreateClockClass createClockClass1,
                      const CreateClockClass createClockClass2, const std::uint64_t graphMipVersion,
                      const char * const testName) noexcept :
        _mCreateClockClass1 {createClockClass1},
        _mCreateClockClass2 {createClockClass2}, _mGraphMipVersion {graphMipVersion},
        _mTestName {testName}
    {
    }

    void run() const noexcept;

private:
    void _runOne(MsgType msgType1, MsgType msgType2) const noexcept;

    CreateClockClass _mCreateClockClass1;
    CreateClockClass _mCreateClockClass2;
    std::uint64_t _mGraphMipVersion;
    const char *_mTestName;
};

class SucceedTestCase final : public TestCase<SucceedTestCase>
{
    friend TestCase<SucceedTestCase>;

public:
    explicit SucceedTestCase(CreateClockClass createClockClass, const std::uint64_t graphMipVersion,
                             const char * const testName) noexcept :
        TestCase {createClockClass, createClockClass, graphMipVersion, testName}
    {
    }

private:
    void _handleResult(bool thrown, bt2::UniqueConstError error,
                       const std::string& specTestName) const noexcept;
};

class ErrorTestCase final : public TestCase<ErrorTestCase>
{
    friend TestCase<ErrorTestCase>;

public:
    explicit ErrorTestCase(CreateClockClass createClockClass1Param,
                           CreateClockClass createClockClass2Param,
                           const std::uint64_t graphMipVersion, const char * const testName,
                           const char * const expectedCauseMsg) noexcept :
        TestCase {createClockClass1Param, createClockClass2Param, graphMipVersion, testName},
        _mExpectedCauseMsg {expectedCauseMsg}
    {
    }

private:
    void _handleResult(bool thrown, bt2::UniqueConstError error,
                       const std::string& specTestName) const noexcept;

    const char *_mExpectedCauseMsg;
};

bt2::ClockClass::Shared noClockClass(bt2::SelfComponent) noexcept
{
    return bt2::ClockClass::Shared {};
}

template <typename Derived>
void TestCase<Derived>::run() const noexcept
{
    static constexpr std::array<MsgType, 2> msgTypes {
        MsgType::Stream,
        MsgType::MsgIterInactivity,
    };

    for (const auto msgType1 : msgTypes) {
        for (const auto msgType2 : msgTypes) {
            /*
             * It's not possible to create message iterator inactivity
             * messages without a clock class. Skip those cases.
             */
            if ((msgType1 == MsgType::MsgIterInactivity && _mCreateClockClass1 == noClockClass) ||
                (msgType2 == MsgType::MsgIterInactivity && _mCreateClockClass2 == noClockClass)) {
                continue;
            }

            /*
             * The test scenarios depend on the message with the first
             * clock class going through the muxer first.
             *
             * Between a message with a clock snapshot and a message
             * without a clock snapshot, the muxer always picks the
             * message without a clock snapshot first.
             *
             * Message iterator inactivity messages always have a clock
             * snapshot. Therefore, if:
             *
             * First message:
             *     A message iterator inactivity message (always has a
             *     clock snapshot).
             *
             * Second message:
             *     Doesn't have a clock class (never has a clock
             *     snapshot).
             *
             * Then there's no way for the first message to go through
             * first.
             *
             * Skip those cases.
             */
            if (msgType1 == MsgType::MsgIterInactivity && _mCreateClockClass2 == noClockClass) {
                continue;
            }

            this->_runOne(msgType1, msgType2);
        }
    }
}

std::string makeSpecTestName(const char * const testName, const MsgType msgType1,
                             const MsgType msgType2, const std::uint64_t graphMipVersion)
{
    return fmt::format("{} ({}, {}, MIP {})", testName, msgType1, msgType2, graphMipVersion);
}

template <typename Derived>
void TestCase<Derived>::_runOne(const MsgType msgType1, const MsgType msgType2) const noexcept
{
    const auto srcCompCls = bt2::SourceComponentClass::create<TestSource>();
    const auto graph = bt2::Graph::create(_mGraphMipVersion);

    {
        /*
         * The test scenarios depend on the message with the first clock class going through the
         * muxer first. Between a message with a clock snapshot and a message without a clock
         * snapshot, the muxer always picks the message without a clock snapshot first.
         *
         * Therefore, for the first message, only set a clock snapshot when absolutely necessary,
         * that is when the message type is "message iterator inactivity".
         *
         * For the second message, always set a clock snapshot when possible, that is when a clock
         * class is defined for that message.
         */
        const auto srcComp1 =
            graph->addComponent(*srcCompCls, "source-1",
                                TestSourceData {_mCreateClockClass1, msgType1,
                                                msgType1 == MsgType::MsgIterInactivity ?
                                                    bt2s::optional<std::uint64_t> {10} :
                                                    bt2s::nullopt});
        const auto srcComp2 =
            graph->addComponent(*srcCompCls, "source-2",
                                TestSourceData {_mCreateClockClass2, msgType2,
                                                _mCreateClockClass2 != noClockClass ?
                                                    bt2s::optional<std::uint64_t> {20} :
                                                    bt2s::nullopt});

        const auto utilsPlugin = bt2::findPlugin("utils");

        BT_ASSERT(utilsPlugin);

        /* Add muxer component */
        const auto muxerComp = bt2c::call([&] {
            const auto muxerCompCls = utilsPlugin->filterComponentClasses()["muxer"];

            BT_ASSERT(muxerCompCls);

            return graph->addComponent(*muxerCompCls, "the-muxer");
        });

        /* Add dummy sink component */
        const auto sinkComp = bt2c::call([&] {
            const auto dummySinkCompCls = utilsPlugin->sinkComponentClasses()["dummy"];

            BT_ASSERT(dummySinkCompCls);

            return graph->addComponent(*dummySinkCompCls, "the-sink");
        });

        /* Connect ports */
        graph->connectPorts(*srcComp1.outputPorts()["out"], *muxerComp.inputPorts()["in0"]);
        graph->connectPorts(*srcComp2.outputPorts()["out"], *muxerComp.inputPorts()["in1"]);
        graph->connectPorts(*muxerComp.outputPorts()["out"], *sinkComp.inputPorts()["in"]);
    }

    const auto thrown = bt2c::call([&graph] {
        try {
            graph->run();
        } catch (const bt2::Error&) {
            return true;
        }

        return false;
    });

    static_cast<const Derived&>(*this)._handleResult(
        thrown, bt2::takeCurrentThreadError(),
        makeSpecTestName(_mTestName, msgType1, msgType2, _mGraphMipVersion));
}

void SucceedTestCase::_handleResult(const bool thrown, const bt2::UniqueConstError error,
                                    const std::string& specTestName) const noexcept
{
    ok(!thrown, "%s - no `bt2::Error` thrown", specTestName.c_str());
    ok(!error, "%s - current thread has no error", specTestName.c_str());
}

void ErrorTestCase::_handleResult(const bool thrown, const bt2::UniqueConstError error,
                                  const std::string& specTestName) const noexcept
{
    ok(thrown, "%s - `bt2::Error` thrown", specTestName.c_str());
    ok(error, "%s - current thread has an error", specTestName.c_str());
    ok(error.length() > 0, "%s - error has at least one cause", specTestName.c_str());

    const auto cause = error[0];

    if (!ok(cause.message().startsWith(_mExpectedCauseMsg), "%s - cause's message is expected",
            specTestName.c_str())) {
        diag("expected: %s", _mExpectedCauseMsg);
        diag("actual: %s", cause.message().data());
    }

    ok(cause.actorTypeIsMessageIterator(), "%s - cause's actor type is message iterator",
       specTestName.c_str());
    ok(cause.asMessageIterator().componentName() == "the-muxer",
       "%s - causes's component name is `the-muxer`", specTestName.c_str());
}

const bt2c::Uuid uuidA {"f00aaf65-ebec-4eeb-85b2-fc255cf1aa8a"};
const bt2c::Uuid uuidB {"03482981-a77b-4d7b-94c4-592bf9e91785"};

std::vector<SucceedTestCase> createSucceedTestCases()
{
    std::vector<SucceedTestCase> cases;

    forEachMipVersion([&](const std::uint64_t graphMipVersion) {
        cases.emplace_back(noClockClass, graphMipVersion, "no clock classes");

        if (graphMipVersion == 0) {
            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    return self.createClockClass();
                },

                graphMipVersion, "clock classes with Unix epoch origins");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->setOriginIsUnknown().uuid(uuidA);
                    return clockCls;
                },
                graphMipVersion, "clock classes with unknown origins, with equal UUIDs");
        } else {
            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->origin("ze-origin-ns", "ze-origin-name", "ze-origin-uid");
                    return clockCls;
                },
                graphMipVersion, "clock classes with known origins");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->setOriginIsUnknown().nameSpace("ze-ns").name("ze-name").uid("ze-uid");
                    return clockCls;
                },
                graphMipVersion, "clock classes with unknown origins, with equal identities");
        }

        static bt2::ClockClass::Shared clockClassUnknownOriginUnknownId;

        cases.emplace_back(
            [](const bt2::SelfComponent self) {
                if (clockClassUnknownOriginUnknownId) {
                    return clockClassUnknownOriginUnknownId;
                }

                clockClassUnknownOriginUnknownId = self.createClockClass();

                clockClassUnknownOriginUnknownId->setOriginIsUnknown();
                return clockClassUnknownOriginUnknownId;
            },
            graphMipVersion, "clock classes with unknown origins, unknown identities");
    });

    return cases;
}

std::vector<ErrorTestCase> createErrorTestCases()
{
    std::vector<ErrorTestCase> cases;

    forEachMipVersion([&](const std::uint64_t graphMipVersion) {
        cases.emplace_back(
            noClockClass,
            [](const bt2::SelfComponent self) {
                return self.createClockClass();
            },
            graphMipVersion, "no clock class followed by clock class",
            "Expecting no clock class, got one");

        if (graphMipVersion == 0) {
            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    return self.createClockClass();
                },
                noClockClass, graphMipVersion,
                "clock class with a Unix epoch origin followed by no clock class",
                "Expecting a clock class with a Unix epoch origin, got none");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    return self.createClockClass();
                },
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false);
                    return clockCls;
                },
                graphMipVersion,
                "clock class with a Unix epoch origin followed by clock class with an unknown origin",
                "Expecting a clock class with a Unix epoch origin, got one with an unknown origin");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false).uuid(uuidA);
                    return clockCls;
                },
                noClockClass, graphMipVersion,
                "clock class with an unknown origin and a UUID followed by no clock class",
                "Expecting a clock class with an unknown origin and a specific UUID, got none");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false).uuid(uuidA);
                    return clockCls;
                },
                [](const bt2::SelfComponent self) {
                    return self.createClockClass();
                },
                graphMipVersion,
                "clock class with an unknown origin and a UUID followed by clock class with a Unix epoch origin",
                "Expecting a clock class with an unknown origin and a specific UUID, got one with a Unix epoch origin");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false).uuid(uuidA);
                    return clockCls;
                },
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false);
                    return clockCls;
                },
                graphMipVersion,
                "clock class with an unknown origin and a UUID followed by clock class with an unknown origin and no UUID",
                "Expecting a clock class with an unknown origin and a specific UUID, got one without a UUID");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false).uuid(uuidA);
                    return clockCls;
                },
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false).uuid(uuidB);
                    return clockCls;
                },
                graphMipVersion,
                "clock class with an unknown origin and a UUID followed by clock class with an unknown origin and another UUID",
                "Expecting a clock class with an unknown origin and a specific UUID, got one with a different UUID");
        } else {
            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    return self.createClockClass();
                },
                noClockClass, graphMipVersion,
                "clock class a known origin followed by no clock class",
                "Expecting a clock class with a known origin, got none");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    return self.createClockClass();
                },
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false);
                    return clockCls;
                },
                graphMipVersion,
                "clock class with a known origin followed by clock class with an unknown origin",
                "Expecting a clock class with a known origin, got one with an unknown origin");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false).nameSpace("ze-ns").name("ze-name").uid(
                        "ze-uid");
                    return clockCls;
                },
                noClockClass, graphMipVersion,
                "clock class with an unknown origin and an identity followed by no clock class",
                "Expecting a clock class with an unknown origin and a specific identity, got none");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false).nameSpace("ze-ns").name("ze-name").uid(
                        "ze-uid");
                    return clockCls;
                },
                [](const bt2::SelfComponent self) {
                    return self.createClockClass();
                },
                graphMipVersion,
                "clock class with an unknown origin and an identity followed by clock class with a known origin",
                "Expecting a clock class with an unknown origin and a specific identity, got one with a known origin");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false).nameSpace("ze-ns").name("ze-name").uid(
                        "ze-uid");
                    return clockCls;
                },
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false);
                    return clockCls;
                },
                graphMipVersion,
                "clock class with an unknown origin and an identity followed by clock class with an unknown origin and no identity",
                "Expecting a clock class with an unknown origin and a specific identity, got one without identity");

            cases.emplace_back(
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false).nameSpace("ze-ns").name("ze-name").uid(
                        "ze-uid");
                    return clockCls;
                },
                [](const bt2::SelfComponent self) {
                    const auto clockCls = self.createClockClass();

                    clockCls->originIsUnixEpoch(false)
                        .nameSpace("another-ns")
                        .name("another-name")
                        .uid("another-uid");
                    return clockCls;
                },
                graphMipVersion,
                "clock class with an unknown origin and an identity followed by clock class with an unknown origin and another identity",
                "Expecting a clock class with an unknown origin and a specific identity, got one with a different identity");
        }

        cases.emplace_back(
            [](const bt2::SelfComponent self) {
                const auto clockCls = self.createClockClass();

                clockCls->originIsUnixEpoch(false);
                return clockCls;
            },
            noClockClass, graphMipVersion,
            "clock class with an unknown origin and no UUID/identity followed by no clock class",
            "Expecting a clock class, got none");

        cases.emplace_back(
            [](const bt2::SelfComponent self) {
                const auto clockCls = self.createClockClass();

                clockCls->originIsUnixEpoch(false);
                return clockCls;
            },
            [](const bt2::SelfComponent self) {
                const auto clockCls = self.createClockClass();

                clockCls->originIsUnixEpoch(false);
                return clockCls;
            },
            graphMipVersion,
            "clock class with an unknown origin and no UUID/identity followed by different clock class",
            "Unexpected clock class");
    });

    return cases;
}

} /* namespace */

int main()
{
    plan_tests(352);

    for (auto& succeedTestCase : createSucceedTestCases()) {
        succeedTestCase.run();
    }

    for (auto& errorTestCase : createErrorTestCases()) {
        errorTestCase.run();
    }

    return exit_status();
}
