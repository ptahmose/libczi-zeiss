#pragma once

#include <vector>
#include <forward_list>
#include <atomic>
#include <memory>
#include <utility>
#include <tuple>
#include <algorithm>

namespace libCZI
{
    namespace detail
    {

        /// Append-only, lock-free singly-linked list:
        /// - Multiple threads can push_back()
        /// - Any number of threads may enumerate via for_each()
        /// - No removal; nodes are freed only in the destructor
        ///
        /// \tparam	T	Generic type parameter.
        template <typename T>
        class AppendOnlyList
        {
        private:
            struct Node
            {
                T value;
                std::atomic<Node*> next{ nullptr };

                template <class U>
                explicit Node(U&& v)
                    : value(std::forward<U>(v))
                {
                }
            };

            std::atomic<Node*> head_{ nullptr };
            std::atomic<Node*> tail_{ nullptr };

        public:
            AppendOnlyList() = default;

            ~AppendOnlyList()
            {
                // IMPORTANT: no other thread may use the list anymore.
                Node* n = head_.load(std::memory_order_relaxed);
                while (n)
                {
                    Node* next = n->next.load(std::memory_order_relaxed);
                    delete n;
                    n = next;
                }
            }

            AppendOnlyList(const AppendOnlyList&) = delete;
            AppendOnlyList& operator=(const AppendOnlyList&) = delete;
            AppendOnlyList(AppendOnlyList&&) = delete;
            AppendOnlyList& operator=(AppendOnlyList&&) = delete;

            /**
             * \brief Appends a copy of \a v to the end of the list.
             *
             * This overload participates in the lock-free multi-producer algorithm and
             * may be called concurrently from multiple threads. The value is copied into
             * a freshly allocated node, which becomes visible to subsequent traversals
             * once the underlying atomic operations complete.
             *
             * \param v Reference to the value that will be copied into the list.
             * \note This function never blocks, but it may allocate dynamically when the
             *       internal node structure is created.
             */
            void push_back(const T& v) { push_back_impl(v); }

            /**
             * \brief Appends \a v to the end of the list by moving from it.
             *
             * Use this overload to transfer ownership of resources without copying. Like
             * the const-reference variant, the operation is lock-free and safe to invoke
             * concurrently with other push_back() calls and for_each() enumerations.
             * Newly appended nodes become observable in unspecified order relative to
             * other threads, but each call eventually publishes its node.
             *
             * \param v Rvalue reference whose contents will be moved into the stored node.
             * \note After the call, \a v is left in a valid but unspecified state.
             */
            void push_back(T&& v) { push_back_impl(std::move(v)); }

            /**
             * \brief Enumerates every node currently reachable in the append-only list.
             *
             * This traversal is lock-free and may be invoked concurrently with push_back().
             * Newly appended elements might or might not be observed during enumeration,
             * but each element present when the call begins is visited exactly once.
             *
             * \tparam F Callable type invoked for each stored value.
             * \param f Functor or lambda that receives a reference to the node value and
             *          returns true to continue enumeration or false to stop early.
             * \return True if every visited node requested continuation, false if\n
             *         enumeration stopped early because \a f returned false.
             */
            template <class F>
            bool for_each(F&& f) const
            {
                Node* cur = head_.load(std::memory_order_acquire);
                while (cur)
                {
                    if (!f(cur->value))
                    {
                        return false;
                    }
                    cur = cur->next.load(std::memory_order_acquire);
                }

                return true;
            }

        private:
            template <class U>
            void push_back_impl(U&& v)
            {
                Node* newNode = new Node(std::forward<U>(v));
                newNode->next.store(nullptr, std::memory_order_relaxed);

                // Fast path: try to append to existing tail.
                while (true)
                {
                    Node* tail = tail_.load(std::memory_order_acquire);

                    // List might be empty.
                    if (tail == nullptr)
                    {
                        // Try to become the first node.
                        Node* expected = nullptr;
                        if (head_.compare_exchange_strong(
                            expected, newNode,
                            std::memory_order_acq_rel,
                            std::memory_order_relaxed))
                        {
                            // We won the race to create the first node.
                            tail_.store(newNode, std::memory_order_release);
                            return;
                        }

                        // Someone else inserted the first node; retry normally.
                        continue;
                    }

                    // Non-empty list: try to link after current tail.
                    Node* next = tail->next.load(std::memory_order_acquire);
                    if (next != nullptr)
                    {
                        // tail is lagging behind; try to move it forward.
                        tail_.compare_exchange_weak(
                            tail, next,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        continue;
                    }

                    // tail->next is null: this is the real end; try to attach newNode.
                    if (tail->next.compare_exchange_weak(
                        next, newNode,
                        std::memory_order_release,
                        std::memory_order_relaxed))
                    {
                        // We successfully attached at the end; swing tail forward (best effort).
                        tail_.compare_exchange_strong(
                            tail, newNode,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        return;
                    }

                    // CAS failed: some other thread raced us; try again.
                }
            }
        };

        /**
         * \brief Lock-free, append-only storage that hands out stable integer IDs.
         *
         * StructurePool grows in fixed-size blocks that contain pre-allocated slots for
         * objects of type \c T. Calls to Add() scan the blocks in allocation order and
         * claim the first free slot via an atomic flag; the returned ID is the global
         * zero-based index of that slot (block offset + intra-block index). IDs remain
         * valid for the lifetime of the stored item and are never reused until the slot
         * is explicitly freed (future work) or the pool is destroyed.
         *
         * Growth is controlled by the \a initialize_size parameter for the first block
         * and \a increment_size for subsequent blocks. Once the optional maximum count
         * is reached, Add() throws instead of allocating more capacity. Concurrent
         * Add() calls are safe: multiple threads may race to acquire slots, but each
         * slot's atomic flag guarantees that only one thread succeeds per index.
         *
         * \tparam T Trivially moveable or copyable type stored in the pool.
         */
        template <typename T>
        class StructurePool
        {
        private:
            struct Block
            {
                std::vector<T> values;
                std::unique_ptr<std::atomic_bool[]> in_use_flags;
                std::size_t size = 0;

                Block() = delete;
                explicit Block(std::uint32_t init_size)
                {
                    this->values.resize(init_size);
                    this->in_use_flags = std::make_unique<std::atomic_bool[]>(init_size);
                    for (std::uint32_t i = 0; i < init_size; ++i)
                    {
                        // use placement new to initialize atomic_bool
                        new (&this->in_use_flags[i]) std::atomic_bool(false);
                    }

                    this->size = init_size;
                }

                // Allow move semantics (unique_ptr handles this automatically)
                Block(Block&&) noexcept = default;
                Block& operator=(Block&&) noexcept = default;

                // Delete copy
                Block(const Block&) = delete;
                Block& operator=(const Block&) = delete;
            };

            AppendOnlyList<Block> blocks_;
            std::uint32_t increment_size_;
            std::atomic_uint32_t total_allocated_;
            std::uint32_t max_count_of_allocated_items_{ 0 };
        public:
            StructurePool(std::uint32_t initialize_size, std::uint32_t increment_size, std::uint32_t max_count_of_allocated_items = 0)
                : increment_size_(increment_size)
            {
                this->max_count_of_allocated_items_ = (std::max)(max_count_of_allocated_items, initialize_size);
                this->blocks_.push_back(Block{ initialize_size });
                this->total_allocated_ = initialize_size;
            }

            /**
             * \brief Appends a copy of \a data to the pool and returns its stable global index.
             *
             * This method atomically acquires a free slot from the append-only block storage and
             * stores a copy of the provided data. The returned index uniquely identifies the stored
             * item and remains valid for the lifetime of the item (until explicit removal or pool
             * destruction).
             *
             * ## Return Value Construction
             *
             * The returned index is a zero-based global slot identifier constructed as follows:
             *
             * 1. **Block Traversal**: The method iterates through all existing blocks in append order
             *    via AppendOnlyList::for_each(), maintaining a running counter `id` initialized to 0.
             *
             * 2. **Per-Block Slot Scanning**: For each block, the method scans every slot sequentially
             *    from index 0 to block.size-1:
             *    - Attempts an atomic compare-exchange on the `in_use_flags[i]` atomic boolean
             *    - If the flag is currently false (slot is free), the operation atomically sets it
             *      to true and claims the slot
             *    - If the flag is true (slot occupied), the slot is skipped
             *    - The counter `id` is incremented after processing each slot, whether occupied or free
             *
             * 3. **Index Calculation**: When a free slot is successfully claimed in a particular block:
             *    - `id` contains the cumulative count of all slots scanned up to and including the
             *      claimed slot across all previous blocks and the current block
             *    - This value represents the global zero-based index of the claimed slot
             *    - For example, if the first block has 100 slots and a slot in the second block at
             *      position 5 is claimed, the returned index is 105 (100 + 5)
             *
             * 4. **Block Allocation (if needed)**: If no free slot exists in any current block:
             *    - A new block is allocated with `increment_size_` slots
             *    - The loop retries from the beginning, eventually finding a free slot in the
             *      newly allocated block
             *    - The index is recalculated with the new block included in the traversal
             *
             * ## Thread Safety
             *
             * Multiple threads may invoke Add() concurrently. Each slot's atomic `in_use_flags[i]`
             * guarantees that only one thread succeeds in claiming any given slot via
             * compare_exchange_strong(). The returned index uniquely identifies the claimed slot
             * and is not reused until the slot is explicitly freed or the pool is destroyed.
             *
             * ## Exception Safety
             *
             * If the pool reaches its maximum capacity (as specified by `max_count_of_allocated_items_`)
             * or if `increment_size_` is 0 (no further growth possible), a std::runtime_error is
             * thrown with the message "StructurePool: exceeded maximum count of allocated items".
             *
             * \param data Reference to the value that will be copied into the claimed slot.
             * \return Zero-based global index of the claimed slot. This index can be passed to Get()
             *         or TryGetAndRemove() to retrieve the stored value.
             * \throws std::runtime_error if pool capacity is exceeded.
             *
             * \note The index is calculated during the atomic claim process and reflects the slot's
             *       position in the global addressing scheme at the moment of successful acquisition.
             */
            std::tuple<std::uint32_t, T*> Add(const T& data)
            {
                T* ptr = nullptr;
                for (;;)
                {
                    std::uint32_t id = 0;
                    if (!this->blocks_.for_each([&](Block& block)->bool
                        {
                            for (std::size_t i = 0; i < block.size; ++i)
                            {
                                bool expected = false;
                                if (block.in_use_flags[i].compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_relaxed))
                                {
                                    block.values[i] = data;
                                    ptr = &block.values[i];
                                    return false;
                                }

                                ++id;
                            }

                            return true;
                        }))
                    {
                        return std::make_tuple(id, ptr);
                    }

                    // didn't find a free slot in existing blocks - if we already reached max, or if the increment size is 0, throw
                    // an exception here (no more allocations possible)
                    if (id >= this->max_count_of_allocated_items_ || this->increment_size_ == 0)
                    {
                        throw std::runtime_error("StructurePool: exceeded maximum count of allocated items");
                    }

                    // we didn't find a free slot in existing blocks
                    // - so, we need to allocate a new block
                    this->blocks_.push_back(Block{ this->increment_size_ });
                    this->total_allocated_.fetch_add(this->increment_size_, std::memory_order_relaxed);
                }
            }

            /**
             * \brief Retrieves a pointer to the stored value associated with the given ID.
             *
             * This method performs a lock-free lookup of the slot identified by the global index
             * \a id (previously returned by Add()). If a valid item exists at that slot, a pointer
             * to the stored value is returned; otherwise, nullptr is returned.
             *
             * ## Lookup Mechanism
             *
             * The retrieval process mirrors the ID calculation performed during Add():
             *
             * 1. **Block Traversal**: Iterates through blocks in append order via
             *    AppendOnlyList::for_each(), maintaining a running counter `id_of_block` that
             *    tracks the cumulative slot offset.
             *
             * 2. **Block Location**: For each block, checks if the target index `id` falls within
             *    the block's range: `id < id_of_block + block.size`. Once the containing block
             *    is located, the local index within that block is calculated as
             *    `index = id - id_of_block`.
             *
             * 3. **Slot Validation**: Verifies that the `in_use_flags[index]` atomic boolean is
             *    true (indicating the slot has been claimed) using an acquire-semantics load
             *    to ensure visibility of the stored value.
             *
             * 4. **Return Value**: Returns a pointer to `block.values[index]` if the slot is in
             *    use, or nullptr if the slot is not found or not marked as in use.
             *
             * ## Lifetime and Validity
             *
             * **Critical**: The returned pointer is valid only as long as the `StructurePool`
             * object itself remains alive and the underlying slot has not been freed. Specifically:
             *
             * - The pointer references memory owned by the pool's internal block storage
             * - If the pool is destroyed, all returned pointers become dangling pointers
             * - Dereferencing a dangling pointer results in undefined behavior
             * - The caller must ensure the pool object outlives all usage of the returned pointer
             *
             * **Example of Unsafe Usage (DO NOT DO):**
             * ```cpp
             * int* ptr = nullptr;
             * {
             *     StructurePool<int> pool(10, 10);
             *     auto id = pool.Add(42);
             *     ptr = pool.Get(id);  // Valid inside this scope
             * }  // pool is destroyed here
             * // ptr is now a dangling pointer!
             * *ptr = 99;  // Undefined behavior!
             * ```
             *
             * **Example of Safe Usage:**
             * ```cpp
             * StructurePool<int> pool(10, 10);
             * auto id = pool.Add(42);
             * int* ptr = pool.Get(id);  // Valid as long as pool is alive
             * if (ptr) {
             *     std::cout << *ptr << std::endl;  // Safe
             * }
             * // Do not use ptr after pool is destroyed
             * ```
             *
             * ## Thread Safety
             *
             * Multiple threads may invoke Get() concurrently with Add() and TryGetAndRemove()
             * without synchronization. The atomic flag load ensures memory ordering consistency
             * with the CAS operations in Add(). However, if a slot is being removed concurrently,
             * this method may observe an inconsistent state; use TryGetAndRemove() if atomic
             * retrieval with removal is required.
             *
             * ## Return Value
             *
             * - Returns a pointer to the stored value if the slot exists and is marked in use
             * - Returns nullptr if:
             *   - The given \a id does not correspond to any allocated slot
             *   - The slot exists but has not been claimed (in_use_flags[index] is false)
             *
             * \param id Zero-based global slot index (as returned by Add())
             * \return Pointer to the stored value if found, nullptr otherwise.
             *         The pointer is valid only during the lifetime of the pool object.
             *
             * \see Add()
             * \see TryGetAndRemove()
             */
            T* Get(std::uint32_t id)
            {
                std::uint32_t id_of_block = 0;
                T* result = nullptr;
                if (!this->blocks_.for_each([&](Block& block)->bool
                    {
                        if (id < id_of_block + block.size)
                        {
                            std::uint32_t index = id - id_of_block;
                            if (block.in_use_flags[index].load(std::memory_order_acquire))
                            {
                                result = &block.values[index];
                            }

                            return false;
                        }

                        id_of_block += block.size;
                        return true;
                    }))
                {
                    return result;
                }

                return nullptr;
            }

            /**
             * \brief Atomically retrieves the value at \a id and marks the slot as free.
             *
             * This helper is intended for consumers that need single-shot ownership of an
             * entry: if the slot identified by the global index \a id is currently marked
             * in use, its payload is copied into \a out_value and the slot is immediately
             * released back to the pool for future Add() calls. If the slot is not in use
             * or \a id falls outside the allocated range, the method leaves \a out_value
             * untouched and returns false.
             *
             * \par Thread Safety
             * Multiple threads may call TryGetAndRemove() concurrently with Add() and Get().
             * Correctness relies on the atomic state transitions inside \c in_use_flags[]:
             * readers must observe the store that publishes the payload before the flag is
             * cleared. Callers should treat \a out_value as a snapshot; subsequent Get()s
             * for the same ID are not guaranteed to see the value once the slot is freed.
             *
             * \param id Zero-based global slot index previously obtained from Add().
             * \param out_value Destination that receives a copy of the stored value when
             *                  the slot is found in use.
             * \return True when the slot existed and was marked in use (\a out_value updated
             *         and the slot returned to the free list); false otherwise.
             */
            bool TryGetAndRemove(std::uint32_t id, T* out_value)
            {
                std::uint32_t id_of_block = 0;
                bool was_found_as_used = false;
                if (!this->blocks_.for_each([&](Block& block)->bool
                    {
                        if (id < id_of_block + block.size)
                        {
                            std::uint32_t index = id - id_of_block;
                            if (block.in_use_flags[index].load(std::memory_order_acquire))
                            {
                                if (out_value != nullptr)
                                {
                                    *out_value = block.values[index];
                                }

                                block.in_use_flags[index].store(false, std::memory_order_release);	// mark as free
                                was_found_as_used = true;
                            }

                            return false;
                        }

                        id_of_block += block.size;
                        return true;
                    }))
                {
                    return was_found_as_used;
                }

                return false;
            }
        };
    }
}
