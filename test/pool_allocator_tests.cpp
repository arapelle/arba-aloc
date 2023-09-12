#include <arba/aloc/pool_allocator.hpp>
#include <gtest/gtest.h>
#include <list>

struct data
{
    unsigned integer;
    bool* bptr = nullptr;
    data(unsigned integer, bool& bvalue)
        : integer(integer), bptr(&bvalue)
    {
        *bptr = true;
    }
    ~data()
    {
        *bptr = false;
    }
    void print()
    {
        std::cout << integer << " " << *bptr << std::endl;
    }
};

TEST(pool_allocator_tests, test_)
{
    std::array<std::byte, 8*3> memory;
    aloc::init_chunk_range(memory, 8);

    std::array<std::byte, 4*3> memory_2;
    aloc::init_chunk_range<4>(memory_2, 4);

    aloc::linked_chunk_pool_block block(5, 8, nullptr);
    aloc::pool_allocator<data> pool_allocator(3);

    bool bvalue = false;
    data* data_ptr = pool_allocator.new_object(6, bvalue);
    data_ptr->print();
    pool_allocator.delete_object(data_ptr);
    std::cout << bvalue << std::endl;

//    pool_allocator.new_object(6, bvalue);
//    pool_allocator.new_object(6, bvalue);
//    pool_allocator.new_object(6, bvalue);
//    pool_allocator.new_object(6, bvalue);

    SUCCEED();
}
