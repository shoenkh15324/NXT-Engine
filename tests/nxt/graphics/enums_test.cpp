
#include <cstdint>
#include <doctest/doctest.h>
#include <nxt/graphics/rhi/common/enums.hpp>
#include <type_traits>

using namespace nxt::graphics::rhi;

TEST_SUITE("graphics::rhi::Format") {

    TEST_CASE("모든 형식은 고유한 값을 가진다") {
        CHECK(Format::Undefined != Format::R8G8B8A8Unorm);
        CHECK(Format::R8G8B8A8Unorm != Format::B8G8R8A8Unorm);
        CHECK(Format::B8G8R8A8Unorm != Format::B8G8R8A8Srgb);
        CHECK(Format::B8G8R8A8Srgb != Format::D32SFloat);
        CHECK(Format::D32SFloat != Format::D24UnormS8Uint);
        CHECK(Format::D24UnormS8Uint != Format::D32SFloatS8Uint);
        CHECK(Format::D32SFloatS8Uint != Format::R32G32Sfloat);
        CHECK(Format::R32G32Sfloat != Format::R32G32B32Sfloat);
        CHECK(Format::R32G32B32Sfloat != Format::R32G32B32A32Sfloat);
    }

    TEST_CASE("Undefined는 0으로 정의된다") {
        CHECK(static_cast<std::uint8_t>(Format::Undefined) == 0);
    }

    TEST_CASE("형식은 8비트 기반 열거형이다") {
        CHECK(sizeof(Format) == sizeof(std::uint8_t));
    }
}

TEST_SUITE("graphics::rhi::ShaderStage") {

    TEST_CASE("셰이더 단계는 지정된 순서로 정의된다") {
        CHECK(static_cast<std::uint8_t>(ShaderStage::Vertex) == 0);
        CHECK(static_cast<std::uint8_t>(ShaderStage::Fragment) == 1);
    }

    TEST_CASE("셰이더 단계는 8비트 기반 열거형이다") {
        CHECK(sizeof(ShaderStage) == sizeof(std::uint8_t));
    }
}

TEST_SUITE("graphics::rhi::PresentMode") {

    TEST_CASE("제시 방식은 지정된 순서로 정의된다") {
        CHECK(static_cast<std::uint8_t>(PresentMode::Fifo) == 0);
        CHECK(static_cast<std::uint8_t>(PresentMode::FifoRelaxed) == 1);
        CHECK(static_cast<std::uint8_t>(PresentMode::Mailbox) == 2);
        CHECK(static_cast<std::uint8_t>(PresentMode::Immediate) == 3);
    }

    TEST_CASE("제시 방식은 서로 다른 값을 가진다") {
        CHECK(PresentMode::Fifo != PresentMode::FifoRelaxed);
        CHECK(PresentMode::FifoRelaxed != PresentMode::Mailbox);
        CHECK(PresentMode::Mailbox != PresentMode::Immediate);
    }
}

TEST_SUITE("graphics::rhi::AttachmentOps") {

    TEST_CASE("LoadOp는 지정된 순서로 정의된다") {
        CHECK(static_cast<std::uint8_t>(LoadOp::Load) == 0);
        CHECK(static_cast<std::uint8_t>(LoadOp::Clear) == 1);
        CHECK(static_cast<std::uint8_t>(LoadOp::DontCare) == 2);
    }

    TEST_CASE("StoreOp는 지정된 순서로 정의된다") {
        CHECK(static_cast<std::uint8_t>(StoreOp::Store) == 0);
        CHECK(static_cast<std::uint8_t>(StoreOp::DontCare) == 1);
    }
}

TEST_SUITE("graphics::rhi::Topology") {

    TEST_CASE("도형 연결 방식은 지정된 순서로 정의된다") {
        CHECK(static_cast<std::uint8_t>(Topology::PointList) == 0);
        CHECK(static_cast<std::uint8_t>(Topology::LineList) == 1);
        CHECK(static_cast<std::uint8_t>(Topology::LineStrip) == 2);
        CHECK(static_cast<std::uint8_t>(Topology::TriangleList) == 3);
        CHECK(static_cast<std::uint8_t>(Topology::TriangleStrip) == 4);
    }
}

TEST_SUITE("graphics::rhi::IndexType") {

    TEST_CASE("인덱스 형식은 지정된 순서로 정의된다") {
        CHECK(static_cast<std::uint8_t>(IndexType::U16) == 0);
        CHECK(static_cast<std::uint8_t>(IndexType::U32) == 1);
    }
}

TEST_SUITE("graphics::rhi::Rasterization") {

    TEST_CASE("컬링 방식은 지정된 순서로 정의된다") {
        CHECK(static_cast<std::uint8_t>(CullMode::None) == 0);
        CHECK(static_cast<std::uint8_t>(CullMode::Front) == 1);
        CHECK(static_cast<std::uint8_t>(CullMode::Back) == 2);
    }

    TEST_CASE("앞면 판정 방식은 지정된 순서로 정의된다") {
        CHECK(static_cast<std::uint8_t>(FrontFace::CounterClockwise) == 0);
        CHECK(static_cast<std::uint8_t>(FrontFace::Clockwise) == 1);
    }
}

TEST_SUITE("graphics::rhi::BufferUsage") {

    TEST_CASE("버퍼 용도는 지정된 비트 값을 가진다") {
        CHECK(static_cast<std::uint32_t>(BufferUsage::None) == 0u);
        CHECK(static_cast<std::uint32_t>(BufferUsage::Vertex) == (1u << 0));
        CHECK(static_cast<std::uint32_t>(BufferUsage::Index) == (1u << 1));
        CHECK(static_cast<std::uint32_t>(BufferUsage::Uniform) == (1u << 2));
        CHECK(static_cast<std::uint32_t>(BufferUsage::Storage) == (1u << 3));
        CHECK(static_cast<std::uint32_t>(BufferUsage::TransferSrc) == (1u << 4));
        CHECK(static_cast<std::uint32_t>(BufferUsage::TransferDst) == (1u << 5));
    }

    TEST_CASE("비트 OR 연산으로 여러 용도를 결합한다") {
        constexpr BufferUsage usage = BufferUsage::Vertex | BufferUsage::TransferDst;

        CHECK(static_cast<std::uint32_t>(usage) == ((1u << 0) | (1u << 5)));
    }

    TEST_CASE("복합 OR 연산으로 기존 용도에 새로운 용도를 추가한다") {
        BufferUsage usage = BufferUsage::Vertex;
        usage |= BufferUsage::Index;
        usage |= BufferUsage::Uniform;

        CHECK(static_cast<std::uint32_t>(usage) == ((1u << 0) | (1u << 1) | (1u << 2)));
    }

    TEST_CASE("동일한 용도를 결합해도 비트 값이 중복되지 않는다") {
        constexpr BufferUsage usage = BufferUsage::Vertex | BufferUsage::Vertex;

        CHECK(usage == BufferUsage::Vertex);
    }

    TEST_CASE("None과의 OR 연산은 기존 용도를 유지한다") {
        constexpr BufferUsage usage = BufferUsage::Vertex | BufferUsage::None;

        CHECK(usage == BufferUsage::Vertex);
    }

    TEST_CASE("서로 다른 버퍼 용도는 독립된 비트를 사용한다") {
        constexpr auto vertex = static_cast<std::uint32_t>(BufferUsage::Vertex);
        constexpr auto index = static_cast<std::uint32_t>(BufferUsage::Index);
        constexpr auto transfer = static_cast<std::uint32_t>(BufferUsage::TransferSrc);

        CHECK((vertex & index) == 0u);
        CHECK((vertex & transfer) == 0u);
        CHECK((index & transfer) == 0u);
    }

    TEST_CASE("버퍼 용도는 32비트 기반 열거형이다") {
        CHECK(sizeof(BufferUsage) == sizeof(std::uint32_t));
    }
}

TEST_SUITE("graphics::rhi::ImageUsage") {

    TEST_CASE("이미지 용도는 지정된 비트 값을 가진다") {
        CHECK(static_cast<std::uint32_t>(ImageUsage::None) == 0u);
        CHECK(static_cast<std::uint32_t>(ImageUsage::ColorAttachment) == (1u << 0));
        CHECK(static_cast<std::uint32_t>(ImageUsage::DepthStencilAttachment) == (1u << 1));
        CHECK(static_cast<std::uint32_t>(ImageUsage::Sampled) == (1u << 2));
        CHECK(static_cast<std::uint32_t>(ImageUsage::Storage) == (1u << 3));
        CHECK(static_cast<std::uint32_t>(ImageUsage::TransferSrc) == (1u << 4));
        CHECK(static_cast<std::uint32_t>(ImageUsage::TransferDst) == (1u << 5));
    }

    TEST_CASE("비트 OR 연산으로 여러 이미지 용도를 결합한다") {
        constexpr ImageUsage usage = ImageUsage::ColorAttachment | ImageUsage::Sampled | ImageUsage::TransferSrc;

        CHECK(static_cast<std::uint32_t>(usage) == ((1u << 0) | (1u << 2) | (1u << 4)));
    }

    TEST_CASE("복합 OR 연산으로 기존 이미지 용도에 새로운 용도를 추가한다") {
        ImageUsage usage = ImageUsage::ColorAttachment;
        usage |= ImageUsage::Sampled;
        usage |= ImageUsage::TransferDst;

        CHECK(static_cast<std::uint32_t>(usage) == ((1u << 0) | (1u << 2) | (1u << 5)));
    }

    TEST_CASE("동일한 이미지 용도를 결합해도 비트 값이 중복되지 않는다") {
        constexpr ImageUsage usage = ImageUsage::Sampled | ImageUsage::Sampled;

        CHECK(usage == ImageUsage::Sampled);
    }

    TEST_CASE("None과의 OR 연산은 기존 용도를 유지한다") {
        constexpr ImageUsage usage = ImageUsage::Sampled | ImageUsage::None;

        CHECK(usage == ImageUsage::Sampled);
    }

    TEST_CASE("서로 다른 이미지 용도는 독립된 비트를 사용한다") {
        constexpr auto color = static_cast<std::uint32_t>(ImageUsage::ColorAttachment);
        constexpr auto depth = static_cast<std::uint32_t>(ImageUsage::DepthStencilAttachment);
        constexpr auto sampled = static_cast<std::uint32_t>(ImageUsage::Sampled);

        CHECK((color & depth) == 0u);
        CHECK((color & sampled) == 0u);
        CHECK((depth & sampled) == 0u);
    }

    TEST_CASE("이미지 용도는 32비트 기반 열거형이다") {
        CHECK(sizeof(ImageUsage) == sizeof(std::uint32_t));
    }
}

TEST_SUITE("graphics::rhi::CompareOp") {

    TEST_CASE("깊이 비교 방식은 지정된 순서로 정의된다") {
        CHECK(static_cast<std::uint8_t>(CompareOp::Never) == 0);
        CHECK(static_cast<std::uint8_t>(CompareOp::Less) == 1);
        CHECK(static_cast<std::uint8_t>(CompareOp::Equal) == 2);
        CHECK(static_cast<std::uint8_t>(CompareOp::LessOrEqual) == 3);
        CHECK(static_cast<std::uint8_t>(CompareOp::Greater) == 4);
        CHECK(static_cast<std::uint8_t>(CompareOp::NotEqual) == 5);
        CHECK(static_cast<std::uint8_t>(CompareOp::GreaterOrEqual) == 6);
        CHECK(static_cast<std::uint8_t>(CompareOp::Always) == 7);
    }

    TEST_CASE("깊이 비교 방식은 서로 다른 값을 가진다") {
        CHECK(CompareOp::Never != CompareOp::Less);
        CHECK(CompareOp::Less != CompareOp::Equal);
        CHECK(CompareOp::Equal != CompareOp::LessOrEqual);
        CHECK(CompareOp::LessOrEqual != CompareOp::Greater);
        CHECK(CompareOp::Greater != CompareOp::NotEqual);
        CHECK(CompareOp::NotEqual != CompareOp::GreaterOrEqual);
        CHECK(CompareOp::GreaterOrEqual != CompareOp::Always);
    }
}

TEST_SUITE("graphics::rhi::TypeProperties") {

    TEST_CASE("모든 열거형은 정수 기반 타입이다") {
        CHECK(std::is_enum_v<Format>);
        CHECK(std::is_enum_v<ShaderStage>);
        CHECK(std::is_enum_v<PresentMode>);
        CHECK(std::is_enum_v<LoadOp>);
        CHECK(std::is_enum_v<StoreOp>);
        CHECK(std::is_enum_v<Topology>);
        CHECK(std::is_enum_v<IndexType>);
        CHECK(std::is_enum_v<CullMode>);
        CHECK(std::is_enum_v<FrontFace>);
        CHECK(std::is_enum_v<BufferUsage>);
        CHECK(std::is_enum_v<ImageUsage>);
        CHECK(std::is_enum_v<CompareOp>);
    }
}
