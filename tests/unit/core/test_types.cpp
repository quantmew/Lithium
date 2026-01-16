#include <gtest/gtest.h>
#include "lithium/core/types.hpp"

using namespace lithium;

// ============================================================================
// Result Tests
// ============================================================================

TEST(ResultTest, OkResult) {
    Result<int, String> result = 42;

    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrorResult) {
    Result<int, String> result = make_error(String("error message"));

    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(result.error(), "error message");
}

TEST(ResultTest, ValueOr) {
    Result<int, String> ok_result = 42;
    Result<int, String> err_result = make_error(String("error"));

    EXPECT_EQ(ok_result.value_or(0), 42);
    EXPECT_EQ(err_result.value_or(0), 0);
}

TEST(ResultTest, Map) {
    Result<int, String> result = 21;
    auto mapped = result.map([](int x) { return x * 2; });

    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 42);
}

TEST(ResultTest, VoidResult) {
    Result<void, String> ok_result;
    Result<void, String> err_result = make_error(String("error"));

    EXPECT_TRUE(ok_result.is_ok());
    EXPECT_TRUE(err_result.is_err());
}

// ============================================================================
// Point Tests
// ============================================================================

TEST(PointTest, DefaultConstruction) {
    PointI p;
    EXPECT_EQ(p.x, 0);
    EXPECT_EQ(p.y, 0);
}

TEST(PointTest, ValueConstruction) {
    PointI p(10, 20);
    EXPECT_EQ(p.x, 10);
    EXPECT_EQ(p.y, 20);
}

TEST(PointTest, Addition) {
    PointI a(10, 20);
    PointI b(5, 15);
    auto c = a + b;

    EXPECT_EQ(c.x, 15);
    EXPECT_EQ(c.y, 35);
}

TEST(PointTest, Subtraction) {
    PointI a(10, 20);
    PointI b(5, 15);
    auto c = a - b;

    EXPECT_EQ(c.x, 5);
    EXPECT_EQ(c.y, 5);
}

TEST(PointTest, Equality) {
    PointI a(10, 20);
    PointI b(10, 20);
    PointI c(5, 20);

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// ============================================================================
// Size Tests
// ============================================================================

TEST(SizeTest, DefaultConstruction) {
    SizeI s;
    EXPECT_EQ(s.width, 0);
    EXPECT_EQ(s.height, 0);
}

TEST(SizeTest, IsEmpty) {
    SizeI empty1;
    SizeI empty2(0, 10);
    SizeI empty3(10, 0);
    SizeI non_empty(10, 10);

    EXPECT_TRUE(empty1.is_empty());
    EXPECT_TRUE(empty2.is_empty());
    EXPECT_TRUE(empty3.is_empty());
    EXPECT_FALSE(non_empty.is_empty());
}

// ============================================================================
// Rect Tests
// ============================================================================

TEST(RectTest, DefaultConstruction) {
    RectI r;
    EXPECT_EQ(r.x, 0);
    EXPECT_EQ(r.y, 0);
    EXPECT_EQ(r.width, 0);
    EXPECT_EQ(r.height, 0);
}

TEST(RectTest, Bounds) {
    RectI r(10, 20, 100, 50);

    EXPECT_EQ(r.left(), 10);
    EXPECT_EQ(r.top(), 20);
    EXPECT_EQ(r.right(), 110);
    EXPECT_EQ(r.bottom(), 70);
}

TEST(RectTest, Contains) {
    RectI r(10, 10, 100, 100);

    EXPECT_TRUE(r.contains(PointI(50, 50)));
    EXPECT_TRUE(r.contains(PointI(10, 10)));
    EXPECT_FALSE(r.contains(PointI(5, 50)));
    EXPECT_FALSE(r.contains(PointI(110, 50)));
}

TEST(RectTest, Intersects) {
    RectI r1(0, 0, 100, 100);
    RectI r2(50, 50, 100, 100);
    RectI r3(200, 200, 50, 50);

    EXPECT_TRUE(r1.intersects(r2));
    EXPECT_TRUE(r2.intersects(r1));
    EXPECT_FALSE(r1.intersects(r3));
}

TEST(RectTest, Intersection) {
    RectI r1(0, 0, 100, 100);
    RectI r2(50, 50, 100, 100);

    auto intersection = r1.intersection(r2);
    EXPECT_EQ(intersection.x, 50);
    EXPECT_EQ(intersection.y, 50);
    EXPECT_EQ(intersection.width, 50);
    EXPECT_EQ(intersection.height, 50);
}

// ============================================================================
// Color Tests
// ============================================================================

TEST(ColorTest, DefaultConstruction) {
    Color c;
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
    EXPECT_EQ(c.a, 255);
}

TEST(ColorTest, FromRGB) {
    auto c = Color::from_rgb(0xFF8800);
    EXPECT_EQ(c.r, 255);
    EXPECT_EQ(c.g, 136);
    EXPECT_EQ(c.b, 0);
    EXPECT_EQ(c.a, 255);
}

TEST(ColorTest, ToRGB) {
    Color c(255, 136, 0);
    EXPECT_EQ(c.to_rgb(), 0xFF8800u);
}

TEST(ColorTest, CommonColors) {
    EXPECT_EQ(Color::black(), Color(0, 0, 0));
    EXPECT_EQ(Color::white(), Color(255, 255, 255));
    EXPECT_EQ(Color::red(), Color(255, 0, 0));
    EXPECT_EQ(Color::green(), Color(0, 255, 0));
    EXPECT_EQ(Color::blue(), Color(0, 0, 255));
    EXPECT_EQ(Color::transparent(), Color(0, 0, 0, 0));
}

// ============================================================================
// RefPtr Tests
// ============================================================================

class TestRefCounted : public RefCounted {
public:
    static int instance_count;

    TestRefCounted() { ++instance_count; }
    ~TestRefCounted() { --instance_count; }
};

int TestRefCounted::instance_count = 0;

TEST(RefPtrTest, BasicUsage) {
    TestRefCounted::instance_count = 0;

    {
        auto ptr = make_ref<TestRefCounted>();
        EXPECT_EQ(TestRefCounted::instance_count, 1);
        EXPECT_EQ(ptr->ref_count(), 1u);
    }

    EXPECT_EQ(TestRefCounted::instance_count, 0);
}

TEST(RefPtrTest, CopyIncrementsRefCount) {
    TestRefCounted::instance_count = 0;

    auto ptr1 = make_ref<TestRefCounted>();
    EXPECT_EQ(ptr1->ref_count(), 1u);

    {
        RefPtr<TestRefCounted> ptr2 = ptr1;
        EXPECT_EQ(ptr1->ref_count(), 2u);
        EXPECT_EQ(TestRefCounted::instance_count, 1);
    }

    EXPECT_EQ(ptr1->ref_count(), 1u);
}

TEST(RefPtrTest, MoveDoesNotIncrementRefCount) {
    auto ptr1 = make_ref<TestRefCounted>();
    EXPECT_EQ(ptr1->ref_count(), 1u);

    RefPtr<TestRefCounted> ptr2 = std::move(ptr1);
    EXPECT_EQ(ptr2->ref_count(), 1u);
    EXPECT_EQ(ptr1.get(), nullptr);
}
