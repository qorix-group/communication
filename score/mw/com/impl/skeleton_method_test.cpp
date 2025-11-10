#include "score/mw/com/impl/skeleton_method.h"

#include "score/result/result.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_method.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_method.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>
#include <memory>

#include <score/callback.hpp>

namespace score::mw::com::impl
{

namespace
{
using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

const auto kInstanceSpecifier = InstanceSpecifier::Create(std::string{"abc/abc/TirePressurePort"}).value();
const auto kServiceIdentifier = make_ServiceIdentifierType("foo", 13, 37);
std::uint16_t kInstanceId{23U};
const ServiceInstanceDeployment kDeploymentInfo{kServiceIdentifier,
                                                LolaServiceInstanceDeployment{LolaServiceInstanceId{kInstanceId}},
                                                QualityType::kASIL_QM,
                                                kInstanceSpecifier};
std::uint16_t kServiceId{34U};
const ServiceTypeDeployment kTypeDeployment{LolaServiceTypeDeployment{kServiceId}};
const auto kInstanceIdWithLolaBinding = make_InstanceIdentifier(kDeploymentInfo, kTypeDeployment);

class EmptySkeleton final : public SkeletonBase
{
  public:
    using SkeletonBase::SkeletonBase;
};

using TestMethodType = bool(int, bool);

class SkeletonMethodTestFixture : public ::testing::Test
{
  public:
    auto& CreateSkeletonMethod()
    {
        EmptySkeleton empty_skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
        auto mock_method_binding_ptr = std::make_unique<mock_binding::SkeletonMethod>();

        auto& mock_method_binding = *mock_method_binding_ptr;
        method_ = std::make_unique<SkeletonMethod<TestMethodType>>(
            empty_skeleton, "dummy_method", std::move(mock_method_binding_ptr));
        return mock_method_binding;
    }

    std::unique_ptr<SkeletonMethod<TestMethodType>> method_{nullptr};
};

TEST(SkeletonMethodTests, NotCopyable)
{
    static_assert(!std::is_copy_constructible<SkeletonMethod<TestMethodType>>::value, "Is wrongly copyable");
    static_assert(!std::is_copy_assignable<SkeletonMethod<TestMethodType>>::value, "Is wrongly copyable");
}

TEST(SkeletonMethodTests, IsMoveable)
{
    static_assert(std::is_move_constructible<SkeletonMethod<TestMethodType>>::value, "Is not move constructible");
    static_assert(std::is_move_assignable<SkeletonMethod<TestMethodType>>::value, "Is not move assignable");
}

TEST(SkeletonMethodTest, SkeletonMethodContainsPublicMethodType)
{
    static_assert(std::is_same<SkeletonMethod<TestMethodType>::MethodType, TestMethodType>::value,
                  "Incorrect CallbackType.");
}

TEST(SkeletonMethodTest, ClassTypeDependsOnMethodType)
{
    using FirstSkeletonMethodType = SkeletonMethod<bool(int)>;
    using SecondSkeletonMethodType = SkeletonMethod<void(std::uint16_t)>;
    static_assert(!std::is_same_v<FirstSkeletonMethodType, SecondSkeletonMethodType>,
                  "Class type does not depend on event data type");
}

TEST_F(SkeletonMethodTestFixture, RegisterCallIsDispatchedToTheBinding)
{
    // Given A skeleton Method with a mock method binding
    auto& mock_method_binding = CreateSkeletonMethod();

    // Then it is expected that the register call is dispatched to the binding
    EXPECT_CALL(mock_method_binding, Register(_));

    // When a Register call is issued at the binding independent level
    score::cpp::callback<TestMethodType> test_callback = [](int, bool) noexcept {
        return false;
    };

    method_->Register(std::move(test_callback));
}

template <typename T>
class SkeletonMethodTypedTest : public ::testing::Test
{
  public:
    using Type = T;
};
struct MyDataStruct
{
    bool b;
    int i;
    double d;
    float f[4];
};
using RegisteredFunctionTypes = ::testing::Types<TestMethodType,
                                                 void(int),
                                                 void(const double, int*),
                                                 void(char[10], char),
                                                 int(),
                                                 void(),
                                                 void(),
                                                 MyDataStruct(MyDataStruct),
                                                 MyDataStruct*(MyDataStruct*, int, void*)>;
TYPED_TEST_SUITE(SkeletonMethodTypedTest, RegisteredFunctionTypes, );

TYPED_TEST(SkeletonMethodTypedTest, AnyCombinationOfReturnAndInputArgTypesCanBeRegistered)
{

    // Given A skeleton Method with a mock method binding
    EmptySkeleton empty_skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
    auto mock_method_binding_ptr = std::make_unique<mock_binding::SkeletonMethod>();
    auto& mock_method_binding = *mock_method_binding_ptr;

    // And a method type that can be one of the four qualitatively different Types
    //  void()          SomeType()
    //  void(SomeTypes) SomeType(SomeTypes)
    using FixtureMethodType = typename TestFixture::Type;
    SkeletonMethod<FixtureMethodType> method{empty_skeleton, "dummy_method", std::move(mock_method_binding_ptr)};

    // Then it is expected that the register call is dispatched to the binding without errors
    EXPECT_CALL(mock_method_binding, Register(_));

    // When a Register call is issued at the binding independent level
    score::cpp::callback<FixtureMethodType> test_callback{};

    method.Register(std::move(test_callback));
}

TEST_F(SkeletonMethodTestFixture, ACallbackWithAPointerAsStateCanBeRegistered)
{
    // Given A skeleton Method with a mock method binding
    auto& mock_method_binding = CreateSkeletonMethod();

    // And a callback with a unique_ptr as a state
    auto test_struct_p = std::make_unique<MyDataStruct>();
    score::cpp::callback<TestMethodType> test_callback_with_state = [state = std::move(test_struct_p)](int, bool b) noexcept {
        return state->b || b;
    };

    // Then it is expected that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding, Register(_));

    // When a Register call is issued at the binding independent level
    method_->Register(std::move(test_callback_with_state));
}

using Thing = int;
using InType1 = int;
using InType2 = int;
using VoidVoid = void();
using ThingVoid = Thing();
using VoidStuff = void(InType1, InType2);
using ThingStuff = Thing(InType1, InType2);

template <typename MethodType>
class SkeletonMethodGenericTestFixture : public ::testing::Test
{
    template <std::size_t size>
    static std::array<std::byte, size> make_zeroed_buffer() noexcept
    {
        std::array<std::byte, size> buffer{};
        std::memset(buffer.data(), 0, size);

        return buffer;
    }

  public:
    auto& CreateSkeletonMethodWithMockedTypeErasedCallback()
    {
        EmptySkeleton empty_skeleton{std::make_unique<mock_binding::Skeleton>(), kInstanceIdWithLolaBinding};
        auto mock_method_binding_ptr = std::make_unique<mock_binding::SkeletonMethod>();

        auto& mock_method_binding = *mock_method_binding_ptr;
        method_ = std::make_unique<SkeletonMethod<MethodType>>(
            empty_skeleton, "dummy_method", std::move(mock_method_binding_ptr));

        return mock_method_binding;
    }

    static constexpr std::size_t in_args_buffer_size = sizeof(InType1) + sizeof(InType2);
    void SerializeBuffers(Thing in_arg_1, Thing in_arg_2)
    {
        constexpr std::size_t in_type_1_size = sizeof(InType1);
        constexpr std::size_t in_type_2_size = sizeof(InType2);

        //  IN ARGs serialization
        std::byte* write_head = in_args_buffer.begin();
        std::memcpy(write_head, reinterpret_cast<std::byte*>(&in_arg_1), in_type_1_size);
        write_head += in_type_1_size;
        std::memcpy(write_head, reinterpret_cast<std::byte*>(&in_arg_2), in_type_2_size);
    }
    Thing GetTypedResult()
    {
        return *reinterpret_cast<Thing*>(out_arg_buffer.data());
    }
    std::array<std::byte, sizeof(Thing)> out_arg_buffer{make_zeroed_buffer<sizeof(Thing)>()};
    std::array<std::byte, in_args_buffer_size> in_args_buffer{make_zeroed_buffer<in_args_buffer_size>()};

    std::unique_ptr<SkeletonMethod<MethodType>> method_{nullptr};
    ::testing::MockFunction<MethodType> typed_callback_mock{};
    std::optional<SkeletonMethodBinding::TypeErasedCallback> typeerased_callback{};
};

using SkeletonMethodThingStuffFixture = SkeletonMethodGenericTestFixture<ThingStuff>;
TEST_F(SkeletonMethodThingStuffFixture, DataTransferBetweenTypedAndTypeErasedCallbacks)
{
    // Given A skeleton Method with a mock method binding
    auto& mock_method_binding = CreateSkeletonMethodWithMockedTypeErasedCallback();

    // Then it is expected that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding, Register(_)).WillOnce(Invoke([this](auto&& type_erased_callable) -> ResultBlank {
        typeerased_callback.emplace(std::move(type_erased_callable));
        return {};
    }));

    Thing ret_val{50255};
    InType1 in_arg_1{6};
    InType2 in_arg_2{17};

    EXPECT_CALL(typed_callback_mock, Call(in_arg_1, in_arg_2)).WillOnce(Return(ret_val));

    SerializeBuffers(in_arg_1, in_arg_2);
    method_->Register(typed_callback_mock.AsStdFunction());
    typeerased_callback.value()(out_arg_buffer, in_args_buffer);

    auto res = GetTypedResult();
    EXPECT_EQ(res, ret_val);
}

using SkeletonMethodThingVoidFixture = SkeletonMethodGenericTestFixture<ThingVoid>;
TEST_F(SkeletonMethodThingVoidFixture, DataTransferBetweenTypedAndTypeErasedCallbacks)
{
    // Given A skeleton Method with a mock method binding
    auto& mock_method_binding = CreateSkeletonMethodWithMockedTypeErasedCallback();

    // Then it is expected that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding, Register(_)).WillOnce(Invoke([this](auto&& type_erased_callable) -> ResultBlank {
        typeerased_callback.emplace(std::move(type_erased_callable));
        return {};
    }));

    Thing ret_val{50255};

    EXPECT_CALL(typed_callback_mock, Call()).WillOnce(Return(ret_val));

    method_->Register(typed_callback_mock.AsStdFunction());
    typeerased_callback.value()(out_arg_buffer, std::nullopt);

    auto res = GetTypedResult();
    EXPECT_EQ(res, ret_val);
}

using SkeletonMethodVoidStuffFixture = SkeletonMethodGenericTestFixture<VoidStuff>;
TEST_F(SkeletonMethodVoidStuffFixture, DataTransferBetweenTypedAndTypeErasedCallbacks)
{
    // Given A skeleton Method with a mock method binding
    auto& mock_method_binding = CreateSkeletonMethodWithMockedTypeErasedCallback();

    // Then it is expected that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding, Register(_)).WillOnce(Invoke([this](auto&& type_erased_callable) -> ResultBlank {
        typeerased_callback.emplace(std::move(type_erased_callable));
        return {};
    }));

    InType1 in_arg_1{6};
    InType2 in_arg_2{17};

    EXPECT_CALL(typed_callback_mock, Call(in_arg_1, in_arg_2)).WillOnce(Return());

    SerializeBuffers(in_arg_1, in_arg_2);
    method_->Register(typed_callback_mock.AsStdFunction());
    typeerased_callback.value()({}, in_args_buffer);
}

using SkeletonMethodVoidVoidFixture = SkeletonMethodGenericTestFixture<VoidVoid>;
TEST_F(SkeletonMethodVoidVoidFixture, DataTransferBetweenTypedAndTypeErasedCallbacks)
{
    // Given A skeleton Method with a mock method binding
    auto& mock_method_binding = CreateSkeletonMethodWithMockedTypeErasedCallback();

    // Then it is expected that the register call is dispatched to the binding without an error
    EXPECT_CALL(mock_method_binding, Register(_)).WillOnce(Invoke([this](auto&& type_erased_callable) -> ResultBlank {
        typeerased_callback.emplace(std::move(type_erased_callable));
        return {};
    }));

    EXPECT_CALL(typed_callback_mock, Call()).WillOnce(Return());

    method_->Register(typed_callback_mock.AsStdFunction());
    typeerased_callback.value()({}, {});
}
}  // namespace

}  // namespace score::mw::com::impl
