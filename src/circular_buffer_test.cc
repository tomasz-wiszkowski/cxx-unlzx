#include "circular_buffer.hh"

#include <gtest/gtest.h>

#include <span>
#include <vector>

using MyTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t>;

template <typename T>
class CircularBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    buffer_ = std::make_unique<CircularBuffer<T>>(10);
  }

  std::unique_ptr<CircularBuffer<T>> buffer_;
};

TYPED_TEST_SUITE(CircularBufferTest, MyTypes);

TYPED_TEST(CircularBufferTest, IsEmptyInitially) {
  EXPECT_TRUE(this->buffer_->is_empty());
  EXPECT_FALSE(this->buffer_->is_full());
  EXPECT_EQ(this->buffer_->size(), 0);
  EXPECT_EQ(this->buffer_->capacity(), 10);
}

TYPED_TEST(CircularBufferTest, AddAndReadSingleElement) {
  this->buffer_->push(42);
  EXPECT_FALSE(this->buffer_->is_empty());
  EXPECT_EQ(this->buffer_->size(), 1);

  auto spans = this->buffer_->read(1);
  ASSERT_EQ(spans.size(), 1);
  EXPECT_EQ(spans[0][0], 42);
}

TYPED_TEST(CircularBufferTest, AddAndReadMultipleElements) {
  for (uint8_t i = 0; i < 5; ++i) {
    this->buffer_->push(i);
  }
  EXPECT_EQ(this->buffer_->size(), 5);

  auto spans = this->buffer_->read(5);
  ASSERT_EQ(spans.size(), 1);
  for (uint8_t i = 0; i < 5; ++i) {
    EXPECT_EQ(spans[0][i], i);
  }
}

TYPED_TEST(CircularBufferTest, WrapAroundAligned) {
  // Fill up buffer
  for (uint8_t i = 0; i < 10; ++i) {
    this->buffer_->push(i);
  }

  EXPECT_EQ(this->buffer_->size(), 10);
  EXPECT_TRUE(this->buffer_->is_full());

  // Next, read the entire buffer back. Expect 1 span.
  auto spans = this->buffer_->read(10);
  ASSERT_EQ(spans.size(), 1);
  ASSERT_EQ(spans[0].size(), 10);

  for (uint8_t i = 0; i < 10; ++i) {
    EXPECT_EQ(spans[0][i], i);
  }
}

TYPED_TEST(CircularBufferTest, WrapAroundAlignedAfterFull) {
  // Fill up buffer
  for (uint8_t i = 0; i < 10; ++i) {
    this->buffer_->push(i);
  }

  // Drain buffer
  this->buffer_->consume(10);

  // Next, fill the buffer again
  for (uint8_t i = 0; i < 10; ++i) {
    this->buffer_->push(i + 10);
  }

  EXPECT_EQ(this->buffer_->size(), 10);
  EXPECT_TRUE(this->buffer_->is_full());

  // Next, read the entire buffer back. Expect 1 span.
  auto spans = this->buffer_->read(10);
  ASSERT_EQ(spans.size(), 1);
  ASSERT_EQ(spans[0].size(), 10);

  for (uint8_t i = 0; i < 10; ++i) {
    EXPECT_EQ(spans[0][i], i + 10);
  }
}

TYPED_TEST(CircularBufferTest, WrapAroundNotAligned) {
  // Fill up buffer
  for (uint8_t i = 0; i < 10; ++i) {
    this->buffer_->push(i);
  }
  EXPECT_TRUE(this->buffer_->is_full());

  // Consume 4 elements
  this->buffer_->consume(4);
  EXPECT_FALSE(this->buffer_->is_full());

  // and fill the buffer again.
  this->buffer_->push(10);
  this->buffer_->push(11);
  this->buffer_->push(12);
  this->buffer_->push(13);

  EXPECT_EQ(this->buffer_->size(), 10);
  EXPECT_TRUE(this->buffer_->is_full());

  // Lastly, read the entire buffer back. Expect 2 spans.
  auto spans = this->buffer_->read(10);
  ASSERT_EQ(spans.size(), 2);
  ASSERT_EQ(spans[0].size(), 6);
  ASSERT_EQ(spans[1].size(), 4);

  for (uint8_t i = 0; i < 6; ++i) {
    EXPECT_EQ(spans[0][i], i + 4);
  }

  for (uint8_t i = 0; i < 4; ++i) {
    EXPECT_EQ(spans[1][i], i + 10);
  }
}

TYPED_TEST(CircularBufferTest, ConsumeElements) {
  for (uint8_t i = 0; i < 5; ++i) {
    this->buffer_->push(i);
  }
  EXPECT_EQ(this->buffer_->size(), 5);

  this->buffer_->consume(3);
  EXPECT_EQ(this->buffer_->size(), 2);

  auto spans = this->buffer_->read(2);
  ASSERT_EQ(spans.size(), 1);
  EXPECT_EQ(spans[0][0], 3);
  EXPECT_EQ(spans[0][1], 4);
}

TYPED_TEST(CircularBufferTest, ReadMoreThanAvailable) {
  for (uint8_t i = 0; i < 5; ++i) {
    this->buffer_->push(i);
  }
  EXPECT_EQ(this->buffer_->size(), 5);

  auto spans = this->buffer_->read(10);
  ASSERT_EQ(spans.size(), 1);
  for (uint8_t i = 0; i < 5; ++i) {
    EXPECT_EQ(spans[0][i], i);
  }
}

TYPED_TEST(CircularBufferTest, ReadSpans) {
  for (uint8_t i = 0; i < 5; ++i) {
    this->buffer_->push(i);
  }
  EXPECT_EQ(this->buffer_->size(), 5);

  auto spans = this->buffer_->spans();
  ASSERT_EQ(spans.size(), 1);
  for (uint8_t i = 0; i < 5; ++i) {
    EXPECT_EQ(spans[0][i], i);
  }
}

TYPED_TEST(CircularBufferTest, ReadSpansWrapAround) {
  for (uint8_t i = 0; i < 10; ++i) {
    this->buffer_->push(i);
  }
  this->buffer_->consume(5);
  for (uint8_t i = 10; i < 15; ++i) {
    this->buffer_->push(i);
  }

  auto spans = this->buffer_->spans();
  ASSERT_EQ(spans.size(), 2);
  for (uint8_t i = 0; i < 5; ++i) {
    EXPECT_EQ(spans[0][i], i + 5);
  }
  for (uint8_t i = 0; i < 5; ++i) {
    EXPECT_EQ(spans[1][i], i + 10);
  }
}

TYPED_TEST(CircularBufferTest, OperatorAccess) {
  for (uint8_t i = 0; i < 5; ++i) {
    this->buffer_->push(i);
  }
  EXPECT_EQ((*this->buffer_)[0], 0);
  EXPECT_EQ((*this->buffer_)[1], 1);
  EXPECT_EQ((*this->buffer_)[2], 2);
  EXPECT_EQ((*this->buffer_)[3], 3);
  EXPECT_EQ((*this->buffer_)[4], 4);
}

TYPED_TEST(CircularBufferTest, Repeat) {
  for (uint8_t i = 0; i < 5; ++i) {
    this->buffer_->push(i);
  }
  this->buffer_->repeat(3, 2);
  EXPECT_EQ(this->buffer_->size(), 7);

  auto spans = this->buffer_->read(7);
  ASSERT_EQ(spans.size(), 1);
  EXPECT_EQ(spans[0][0], 0);
  EXPECT_EQ(spans[0][1], 1);
  EXPECT_EQ(spans[0][2], 2);
  EXPECT_EQ(spans[0][3], 3);
  EXPECT_EQ(spans[0][4], 4);
  EXPECT_EQ(spans[0][5], 2);
  EXPECT_EQ(spans[0][6], 3);
}
