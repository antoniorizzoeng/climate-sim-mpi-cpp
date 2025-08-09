#include <gtest/gtest.h>
#include "field.hpp"

TEST(Field, AllocationAndSize) {
    Field f(4,3,1,1.0,1.0);
    ASSERT_EQ(f.data.size(),
              static_cast<size_t>(f.nx_total()*f.ny_total()));
}

TEST(Field, IndexingLayout) {
    Field f(2,2,1,1.0,1.0);
    for (int j=0; j<f.ny_total(); ++j)
      for (int i=0; i<f.nx_total(); ++i)
        f.at(i,j) = 10*j + i;

    EXPECT_DOUBLE_EQ(f.at(0,0), 0);
    EXPECT_DOUBLE_EQ(f.at(f.nx_total()-1,0), 3);
    EXPECT_DOUBLE_EQ(f.at(0,1), 10);
    EXPECT_DOUBLE_EQ(f.at(3,3), 33);
}

TEST(Field, OutOfBoundsThrows) {
    Field f(4,4,1,1.0,1.0);
    EXPECT_THROW((void)f.at(-1,0), std::out_of_range);
    EXPECT_THROW((void)f.at(f.nx_total(),0), std::out_of_range);
    EXPECT_THROW((void)f.at(0,f.ny_total()), std::out_of_range);
}
