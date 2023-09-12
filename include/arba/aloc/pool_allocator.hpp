#pragma once

#include <memory>
#include <variant>
#include <span>
#include <cstdint>
#include <cassert>
#include <arba/core/type_traits.hpp>

inline namespace arba
{
namespace aloc
{

struct chunk_link
{
    void* next;
};

template <unsigned index_size>
    requires (index_size < sizeof(void*))
struct chunk_index
{
    using index_type = core::make_integer_t<index_size * 8, unsigned>;
    index_type next_index;
};

inline void init_chunk_range(std::span<std::byte> bytes, std::size_t chunk_size)
{
    std::byte* addr = &(bytes[0]);
    for (std::size_t i = (bytes.size() / chunk_size) - 1; i > 0; --i)
    {
        std::byte* next = addr + chunk_size;
        reinterpret_cast<chunk_link*>(addr)->next = next;
        addr = next;
    }
    reinterpret_cast<chunk_link*>(addr)->next = nullptr;
}

template <unsigned index_size>
    requires (index_size < sizeof(void*))
inline void init_chunk_range(std::span<std::byte> bytes, std::size_t chunk_size)
{
    using chunk_index_t = chunk_index<index_size>;
    using index_t = chunk_index_t::index_type;

    std::byte* addr = &(bytes[0]);
    index_t index = 0;
    for (std::size_t i = (bytes.size() / chunk_size); i > 0; --i)
    {
        std::byte* next = addr + chunk_size;
        reinterpret_cast<chunk_index_t*>(addr)->next_index = ++index;
        addr = next;
    }
}

template <typename BlockT>
class chunk_pool_block
{
public:
    chunk_pool_block(std::size_t number_of_chunks, std::size_t chunk_size,
                     std::unique_ptr<BlockT> next_block)
        : bytes_(new std::byte[number_of_chunks * chunk_size]), next_block_(std::move(next_block))
    {
    }

    template <class ValueT = void>
    [[nodiscard]] inline ValueT* data() const { return reinterpret_cast<ValueT*>(&bytes_[0]); }

protected:
    std::unique_ptr<std::byte[]> bytes_;
    std::unique_ptr<BlockT> next_block_;
};

class linked_chunk_pool_block : public chunk_pool_block<linked_chunk_pool_block>
{
public:
    linked_chunk_pool_block(std::size_t number_of_chunks, std::size_t chunk_size,
                            std::unique_ptr<linked_chunk_pool_block> next_block)
        : chunk_pool_block<linked_chunk_pool_block>(number_of_chunks, chunk_size, std::move(next_block))
    {
        init_chunk_range(std::span(bytes_.get(), number_of_chunks * chunk_size), chunk_size);
    }
};

template <uint8_t chunk_size>
class indexed_chunk_pool_block : public chunk_pool_block<indexed_chunk_pool_block<chunk_size>>
{
public:
    static constexpr uint8_t index_size = std::bit_floor(chunk_size);

    indexed_chunk_pool_block(uint32_t number_of_chunks,
                             std::unique_ptr<indexed_chunk_pool_block> next_block)
        : chunk_pool_block<indexed_chunk_pool_block>(number_of_chunks, chunk_size, std::move(next_block)),
        number_of_chunks_(number_of_chunks),
        next_available_block_(nullptr)
    {
        init_chunk_range<index_size>(std::span(this->bytes_.get(), number_of_chunks_ * chunk_size), chunk_size);
    }

    [[nodiscard]] std::byte* allocate(indexed_chunk_pool_block*& available_block)
    {
        std::byte* res = this->template data<std::byte>() + (chunk_size * available_value_index_);
        available_value_index_ = reinterpret_cast<chunk_index<index_size>*>(res)->next_index;
        if (available_value_index_ == number_of_chunks_) [[unlikely]]
        {
            available_block = next_available_block_;
            next_available_block_ = nullptr;
        }
        return res;
    }

    void deallocate(std::byte* value_ptr, indexed_chunk_pool_block*& available_block)
    {
        if (contains_chunk_(value_ptr))
        {
            if (available_value_index_ == number_of_chunks_) [[unlikely]]
            {
                next_available_block_ = available_block;
                available_block = this;
            }
            reinterpret_cast<chunk_index<index_size>*>(value_ptr)->next_index = available_value_index_;
            available_value_index_ = (value_ptr - this->bytes_.get()) / chunk_size;
        }
        else if (this->next_block_)
        {
            this->next_block_->deallocate(value_ptr, available_block);
        }
        else
        {
            throw std::runtime_error("bad deallocate");
        }
    }

private:
    inline bool contains_chunk_(void* value_ptr) const
    {
        std::byte* data_begin = this->bytes_.get();
        return data_begin >= value_ptr && value_ptr < (data_begin + (chunk_size * number_of_chunks_));
    }

private:
    const uint32_t number_of_chunks_;
    uint32_t available_value_index_ = 0;
    indexed_chunk_pool_block* next_available_block_;
};

template <class ValueT>
    requires (sizeof(ValueT) >= sizeof(void*))
std::unique_ptr<linked_chunk_pool_block> make_linked_chunk_pool_block(std::size_t number_of_chunks,
                                                                      std::unique_ptr<linked_chunk_pool_block> next_block = nullptr)
{
    return std::make_unique<linked_chunk_pool_block>(number_of_chunks, sizeof(ValueT), std::move(next_block));
}

template <class ValueT>
    requires (sizeof(ValueT) < sizeof(void*))
std::unique_ptr<indexed_chunk_pool_block<sizeof(ValueT)>>
make_indexed_chunk_pool_block(uint32_t number_of_chunks, std::unique_ptr<indexed_chunk_pool_block<sizeof(ValueT)>> next_block = nullptr)
{
    return std::make_unique<indexed_chunk_pool_block>(number_of_chunks, sizeof(ValueT), std::move(next_block));
}

template <class ValueT>
    requires (sizeof(ValueT) >= sizeof(void*))
class pool_allocator
{
public:
    using value_type = ValueT;

public:
    explicit pool_allocator(std::size_t chunks_per_block)
        : chunks_per_block_(chunks_per_block)
    {}

    pool_allocator(const pool_allocator&) = delete;
    pool_allocator(pool_allocator&&) = default;

    pool_allocator& operator=(const pool_allocator&) = delete;
    pool_allocator& operator=(pool_allocator&&) = default;

    template <typename... ArgsTs>
    [[nodiscard]] value_type* new_object(ArgsTs&&... ctor_args)
    {
        value_type* res = allocate_();
        try {
            new(res) value_type(std::forward<ArgsTs&&>(ctor_args)...);
        }
        catch (...)
        {
            deallocate_(res);
            throw;
        }
        return res;
    }

    void delete_object(value_type* value_ptr)
    {
        value_ptr->~value_type();
        deallocate_(value_ptr);
    }

private:
    [[nodiscard]] value_type* allocate_()
    {
        if (available_value_ == nullptr) [[unlikely]]
            available_value_ = allocate_block_();
        value_type* res = available_value_;
        available_value_ = reinterpret_cast<value_type*>(reinterpret_cast<chunk_link*>(available_value_)->next);
        return res;
    }

    void deallocate_(value_type* value_ptr)
    {
        reinterpret_cast<chunk_link*>(value_ptr)->next = available_value_;
        available_value_ = value_ptr;
    }

    [[nodiscard]] value_type* allocate_block_()
    {
        blocks_ = make_linked_chunk_pool_block<value_type>(chunks_per_block_, std::move(blocks_));
        return blocks_->data<value_type>();
    }

private:
    std::size_t chunks_per_block_;
    value_type* available_value_ = nullptr;
    std::unique_ptr<linked_chunk_pool_block> blocks_;
};

template <class ValueT>
    requires (sizeof(ValueT) < sizeof(void*))
class index_pool_allocator
{
public:
    using value_type = ValueT;

public:
    explicit index_pool_allocator(std::size_t chunks_per_block)
        : chunks_per_block_(chunks_per_block)
    {}

    index_pool_allocator(const index_pool_allocator&) = delete;
    index_pool_allocator(index_pool_allocator&&) = default;

    index_pool_allocator& operator=(const index_pool_allocator&) = delete;
    index_pool_allocator& operator=(index_pool_allocator&&) = default;

    template <typename... ArgsTs>
    [[nodiscard]] value_type* new_object(ArgsTs&&... ctor_args)
    {
        value_type* res = allocate_();
        try {
            new(res) value_type(std::forward<ArgsTs&&>(ctor_args)...);
        }
        catch (...)
        {
            deallocate_(res);
            throw;
        }
        return res;
    }

    void delete_object(value_type* value_ptr)
    {
        value_ptr->~value_type();
        deallocate_(value_ptr);
    }

private:
    [[nodiscard]] value_type* allocate_()
    {
        if (available_block_ == nullptr) [[unlikely]]
            available_block_ = allocate_block_();
        value_type* res = available_block_->allocate(available_block_);
        return res;
    }

    void deallocate_(value_type* value_ptr)
    {
        blocks_->deallocate(value_ptr, available_block_);
    }

    [[nodiscard]] indexed_chunk_pool_block<sizeof(ValueT)>* allocate_block_()
    {
        blocks_ = make_indexed_chunk_pool_block<value_type>(chunks_per_block_, std::move(blocks_));
        return blocks_.get();
    }

    inline bool operator==(const index_pool_allocator& other) const noexcept { return this == &other; }
    inline bool operator!=(const index_pool_allocator& other) const noexcept { return this != &other; }

private:
    std::size_t chunks_per_block_;
    indexed_chunk_pool_block<sizeof(ValueT)>* available_block_ = nullptr;
    std::unique_ptr<indexed_chunk_pool_block<sizeof(ValueT)>> blocks_;
};

}
}
