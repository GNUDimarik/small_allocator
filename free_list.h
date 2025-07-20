#pragma once

#include <functional>

namespace utils::alloc_malloc::detail {

template <typename _NodeType>
_NodeType* __list_find_min(_NodeType* __head) noexcept
{
    auto __b = __head;

    while (__b) {

        if (__b->_M_prev) {
            __b = __b->_M_prev;
        } else {
            return __b;
        }
    }

    return __b;
}

template <typename _NodeType>
_NodeType* __list_insert_next(_NodeType* __head,
    _NodeType* __blk) noexcept
{
    auto __n = __head->_M_next;
    __head->_M_next = __blk;
    __blk->_M_prev = __head;
    __blk->_M_next = __n;

    if (__n) {
        __n->_M_prev = __blk;
    }

    return __head;
}

template <typename _NodeType>
_NodeType* __list_insert_prev(_NodeType* __head,
    _NodeType* __block) noexcept
{
    auto __p = __head->_M_prev;
    __head->_M_prev = __block;
    __block->_M_next = __head;
    __block->_M_prev = __p;

    if (__p) {
        __p->_M_next = __block;
    }

    return __block;
}

template <typename _NodeType, typename _Compare = std::less_equal<size_t>>
_NodeType* __list_insert_sorted(_NodeType* __head,
    _NodeType* __block) noexcept
{
    _Compare __cmp;
    auto __p = __head;

    if (__block->_M_size == __head->_M_size) {
        return __list_insert_next(__head, __block);
    }

    if (__cmp(__block->_M_size, __head->_M_size)) {
        while (__p->_M_prev && __p->_M_prev->_M_size > __block->_M_size) {
            __p = __p->_M_prev;
        }

        return __list_insert_prev(__p, __block);
    }

    while (__p->_M_next && __p->_M_next->_M_size < __block->_M_size) {
        __p = __p->_M_next;
    }

    __list_insert_next(__p, __block);
    return __head;
}

template <typename _NodeType>
_NodeType* __list_erase(_NodeType* __block) noexcept
{
    auto __n = __block->_M_next;
    auto __p = __block->_M_prev;

    if (__n) {
        __n->_M_prev = __p;
    }

    if (__p) {
        __p->_M_next = __n;
    }

    __block->_M_next = nullptr;
    __block->_M_prev = nullptr;
    return __p;
}

template <typename _NodeType>
_NodeType* __list_find(_NodeType* __head,
    size_t __size) noexcept
{
    auto __p = __head;

    while (__p) {
        if (__p->_M_size == __size) {
            return __p;
        }

        __p = __p->_M_next;
    }

    return __p;
}

} // namespace utils::alloc_malloc::detail
