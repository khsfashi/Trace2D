#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace trace2d::scene
{
using ComponentTypeIndex = std::uint32_t;
inline constexpr ComponentTypeIndex InvalidComponentTypeIndex = std::numeric_limits<ComponentTypeIndex>::max();

enum class ComponentClass : std::uint8_t { Authored = 0, RuntimeOnly = 1 };

enum class SemanticValueKind : std::uint8_t
{
    Boolean = 0,
    SignedInteger,
    UnsignedInteger,
    Float,
    Text,
    Float2,
    Float4,
    EntityReference,
    ResourceReference,
    EnumName,
};

struct SemanticValue final
{
    SemanticValueKind kind{SemanticValueKind::Text};
    bool booleanValue{false};
    std::int64_t signedIntegerValue{0};
    std::uint64_t unsignedIntegerValue{0};
    double floatValue{0.0};
    std::array<double, 4> vectorValue{};
    std::string textValue{};

    [[nodiscard]] bool operator==(const SemanticValue&) const noexcept = default;
};

struct ComponentAuthoringField final
{
    std::string name{};
    SemanticValue value{};
    [[nodiscard]] bool operator==(const ComponentAuthoringField&) const noexcept = default;
};

struct ComponentAuthoringObject final
{
    std::vector<ComponentAuthoringField> fields{};

    [[nodiscard]] const SemanticValue* Find(std::string_view name) const noexcept
    {
        const auto iterator = std::find_if(fields.begin(), fields.end(), [name](const ComponentAuthoringField& field)
        {
            return std::string_view{field.name} == name;
        });
        return iterator == fields.end() ? nullptr : &iterator->value;
    }
};

struct ComponentInspectionField final
{
    std::string name{};
    SemanticValue value{};
    [[nodiscard]] bool operator==(const ComponentInspectionField&) const noexcept = default;
};

struct ComponentInspectionSnapshot final
{
    std::string typeId{};
    std::uint32_t schemaVersion{0};
    ComponentClass componentClass{ComponentClass::RuntimeOnly};
    std::vector<ComponentInspectionField> fields{};
    [[nodiscard]] bool operator==(const ComponentInspectionSnapshot&) const noexcept = default;
};

struct Visibility2D final
{
    bool visible{true};
    [[nodiscard]] bool operator==(const Visibility2D&) const noexcept = default;
};

struct Camera2D;
class ComponentRegistry;

template <typename T>
class ComponentTypeHandle final
{
public:
    ComponentTypeHandle() = default;
    [[nodiscard]] bool IsValid() const noexcept { return owner_ != nullptr && index_ != InvalidComponentTypeIndex; }
    [[nodiscard]] ComponentTypeIndex Index() const noexcept { return index_; }
    [[nodiscard]] bool operator==(const ComponentTypeHandle&) const noexcept = default;

private:
    ComponentTypeHandle(const ComponentRegistry* owner, ComponentTypeIndex index) noexcept : owner_{owner}, index_{index} {}
    const ComponentRegistry* owner_{nullptr};
    ComponentTypeIndex index_{InvalidComponentTypeIndex};
    friend class ComponentRegistry;
    friend class Scene;
};

template <typename T>
struct ComponentRegistration final
{
    std::string typeId{};
    std::uint32_t schemaVersion{0};
    ComponentClass componentClass{ComponentClass::RuntimeOnly};
    std::function<bool(const ComponentAuthoringObject&, T&, std::string&)> parseAuthored{};
    std::function<bool(const T&, std::string&)> validate{};
    std::function<ComponentAuthoringObject(const T&)> serializeAuthored{};
    std::function<std::vector<ComponentInspectionField>(const T&)> inspect{};
};

class ComponentTypeDescriptor
{
public:
    virtual ~ComponentTypeDescriptor() = default;
    [[nodiscard]] virtual void* CreateDefault() const = 0;
    [[nodiscard]] virtual void* Clone(const void* source) const = 0;
    virtual void Destroy(void* object) const noexcept = 0;
    [[nodiscard]] virtual bool ParseAuthored(const ComponentAuthoringObject& source, void* object, std::string& error) const = 0;
    [[nodiscard]] virtual bool Validate(const void* object, std::string& error) const = 0;
    [[nodiscard]] virtual ComponentAuthoringObject SerializeAuthored(const void* object) const = 0;
    [[nodiscard]] virtual std::vector<ComponentInspectionField> Inspect(const void* object) const = 0;

    std::string typeId{};
    std::uint32_t schemaVersion{0};
    ComponentClass componentClass{ComponentClass::RuntimeOnly};
};

class ComponentInstance final
{
public:
    ComponentInstance() = default;
    ComponentInstance(const ComponentInstance& other)
        : index_{other.index_}
        , descriptor_{other.descriptor_}
        , data_{other.descriptor_ == nullptr ? nullptr : other.descriptor_->Clone(other.data_)}
    {
    }
    ComponentInstance& operator=(const ComponentInstance& other)
    {
        if (this == &other) return *this;
        ComponentInstance copy{other};
        Swap(copy);
        return *this;
    }
    ComponentInstance(ComponentInstance&& other) noexcept : index_{other.index_}, descriptor_{other.descriptor_}, data_{other.data_}
    {
        other.index_ = InvalidComponentTypeIndex;
        other.descriptor_ = nullptr;
        other.data_ = nullptr;
    }
    ComponentInstance& operator=(ComponentInstance&& other) noexcept
    {
        if (this == &other) return *this;
        Reset();
        index_ = other.index_;
        descriptor_ = other.descriptor_;
        data_ = other.data_;
        other.index_ = InvalidComponentTypeIndex;
        other.descriptor_ = nullptr;
        other.data_ = nullptr;
        return *this;
    }
    ~ComponentInstance() { Reset(); }

private:
    ComponentInstance(ComponentTypeIndex index, const ComponentTypeDescriptor* descriptor, void* data) noexcept : index_{index}, descriptor_{descriptor}, data_{data} {}
    void Reset() noexcept
    {
        if (descriptor_ != nullptr && data_ != nullptr) descriptor_->Destroy(data_);
        index_ = InvalidComponentTypeIndex;
        descriptor_ = nullptr;
        data_ = nullptr;
    }
    void Swap(ComponentInstance& other) noexcept
    {
        std::swap(index_, other.index_);
        std::swap(descriptor_, other.descriptor_);
        std::swap(data_, other.data_);
    }
    ComponentTypeIndex index_{InvalidComponentTypeIndex};
    const ComponentTypeDescriptor* descriptor_{nullptr};
    void* data_{nullptr};
    friend class ComponentRegistry;
    friend class Scene;
};

class ComponentRegistry final
{
public:
    ComponentRegistry() = default;
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;
    ComponentRegistry(ComponentRegistry&&) = delete;
    ComponentRegistry& operator=(ComponentRegistry&&) = delete;
    ~ComponentRegistry() = default;

    template <typename T>
    [[nodiscard]] ComponentTypeHandle<T> Register(ComponentRegistration<T> registration)
    {
        static_assert(std::is_default_constructible_v<T>);
        static_assert(std::is_copy_constructible_v<T>);
        if (frozen_) throw std::logic_error{"Component registry is frozen."};
        if (registration.typeId.empty()) throw std::invalid_argument{"Component type ID must not be empty."};
        if (FindById(registration.typeId) != nullptr) throw std::invalid_argument{"Component type ID must be unique."};
        if (registration.componentClass == ComponentClass::Authored)
        {
            if (registration.schemaVersion == 0U) throw std::invalid_argument{"Authored component schema version must be non-zero."};
            if (!registration.parseAuthored || !registration.validate || !registration.serializeAuthored)
                throw std::invalid_argument{"Authored component registration requires parse, validate, and serialize adapters."};
        }
        if (descriptors_.size() >= static_cast<std::size_t>(InvalidComponentTypeIndex))
            throw std::length_error{"Component registry exhausted its type index space."};
        const ComponentTypeIndex index = static_cast<ComponentTypeIndex>(descriptors_.size());
        descriptors_.push_back(std::make_unique<TypedDescriptor<T>>(std::move(registration)));
        return ComponentTypeHandle<T>{this, index};
    }

    void Freeze() noexcept { frozen_ = true; }
    [[nodiscard]] bool IsFrozen() const noexcept { return frozen_; }
    [[nodiscard]] std::size_t TypeCount() const noexcept { return descriptors_.size(); }

private:
    template <typename T>
    class TypedDescriptor final : public ComponentTypeDescriptor
    {
    public:
        explicit TypedDescriptor(ComponentRegistration<T> registration)
            : parse_{std::move(registration.parseAuthored)}
            , validate_{std::move(registration.validate)}
            , serialize_{std::move(registration.serializeAuthored)}
            , inspect_{std::move(registration.inspect)}
        {
            typeId = std::move(registration.typeId);
            schemaVersion = registration.schemaVersion;
            componentClass = registration.componentClass;
        }
        [[nodiscard]] void* CreateDefault() const override { return new T{}; }
        [[nodiscard]] void* Clone(const void* source) const override { return new T{*static_cast<const T*>(source)}; }
        void Destroy(void* object) const noexcept override { delete static_cast<T*>(object); }
        [[nodiscard]] bool ParseAuthored(const ComponentAuthoringObject& source, void* object, std::string& error) const override
        {
            return parse_ && parse_(source, *static_cast<T*>(object), error);
        }
        [[nodiscard]] bool Validate(const void* object, std::string& error) const override
        {
            return !validate_ || validate_(*static_cast<const T*>(object), error);
        }
        [[nodiscard]] ComponentAuthoringObject SerializeAuthored(const void* object) const override
        {
            return serialize_ ? serialize_(*static_cast<const T*>(object)) : ComponentAuthoringObject{};
        }
        [[nodiscard]] std::vector<ComponentInspectionField> Inspect(const void* object) const override
        {
            return inspect_ ? inspect_(*static_cast<const T*>(object)) : std::vector<ComponentInspectionField>{};
        }
    private:
        std::function<bool(const ComponentAuthoringObject&, T&, std::string&)> parse_{};
        std::function<bool(const T&, std::string&)> validate_{};
        std::function<ComponentAuthoringObject(const T&)> serialize_{};
        std::function<std::vector<ComponentInspectionField>(const T&)> inspect_{};
    };

    [[nodiscard]] const ComponentTypeDescriptor* FindById(std::string_view typeId) const noexcept
    {
        const auto iterator = std::find_if(descriptors_.begin(), descriptors_.end(), [typeId](const auto& descriptor)
        {
            return std::string_view{descriptor->typeId} == typeId;
        });
        return iterator == descriptors_.end() ? nullptr : iterator->get();
    }
    [[nodiscard]] const ComponentTypeDescriptor* Descriptor(ComponentTypeIndex index) const noexcept
    {
        return static_cast<std::size_t>(index) < descriptors_.size() ? descriptors_[index].get() : nullptr;
    }
    [[nodiscard]] std::optional<ComponentTypeIndex> FindIndexById(std::string_view typeId) const noexcept
    {
        for (std::size_t index = 0; index < descriptors_.size(); ++index)
            if (std::string_view{descriptors_[index]->typeId} == typeId) return static_cast<ComponentTypeIndex>(index);
        return std::nullopt;
    }
    std::vector<std::unique_ptr<ComponentTypeDescriptor>> descriptors_{};
    bool frozen_{false};
    friend class Scene;
};

struct SceneComponentTypes final
{
    ComponentTypeHandle<Visibility2D> visibility{};
    ComponentTypeHandle<Camera2D> camera{};
};

[[nodiscard]] SceneComponentTypes RegisterSceneComponents(ComponentRegistry& registry);
} // namespace trace2d::scene
