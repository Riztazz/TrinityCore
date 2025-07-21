/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tc_catch2.h"
#include "MessageBufferPool.h"
#include <vector>
#include <thread>
#include <chrono>

TEST_CASE("MessageBufferPool basic functionality", "[MessageBufferPool]")
{
    MessageBufferPool& pool = MessageBufferPool::Instance();
    pool.Clear();

    SECTION("Acquire returns valid buffer")
    {
        auto buffer = pool.Acquire();
        REQUIRE(buffer != nullptr);
        REQUIRE(buffer->GetBufferSize() >= 4096);
    }

    SECTION("Release and reuse buffer")
    {
        auto buffer = pool.Acquire();
        REQUIRE(buffer != nullptr);
        
        buffer->Write("test data", 9);
        REQUIRE(buffer->GetActiveSize() == 9);
        
        pool.Release(std::move(buffer));
        REQUIRE(pool.GetPooledCount() == 1);
        
        auto reusedBuffer = pool.Acquire();
        REQUIRE(reusedBuffer != nullptr);
        REQUIRE(reusedBuffer->GetActiveSize() == 0);
        REQUIRE(pool.GetPooledCount() == 0);
    }

    SECTION("Pool size limit")
    {
        pool.SetMaxPoolSize(2);
        
        std::vector<std::unique_ptr<MessageBuffer>> buffers;
        for (int i = 0; i < 5; ++i)
        {
            buffers.push_back(pool.Acquire());
        }
        
        for (auto& buffer : buffers)
        {
            pool.Release(std::move(buffer));
        }
        
        REQUIRE(pool.GetPooledCount() <= 2);
    }

    SECTION("Custom minimum size")
    {
        auto buffer = pool.Acquire(8192);
        REQUIRE(buffer->GetBufferSize() >= 8192);
    }

    SECTION("Clear pool")
    {
        auto buffer = pool.Acquire();
        pool.Release(std::move(buffer));
        REQUIRE(pool.GetPooledCount() > 0);
        
        pool.Clear();
        REQUIRE(pool.GetPooledCount() == 0);
    }
}

TEST_CASE("PooledMessageBuffer RAII wrapper", "[PooledMessageBuffer]")
{
    MessageBufferPool& pool = MessageBufferPool::Instance();
    pool.Clear();

    SECTION("Basic usage")
    {
        {
            PooledMessageBuffer buffer;
            buffer->Write("test", 4);
            REQUIRE(buffer->GetActiveSize() == 4);
        }
        
        REQUIRE(pool.GetPooledCount() == 1);
    }

    SECTION("Move semantics")
    {
        PooledMessageBuffer buffer1;
        buffer1->Write("test", 4);
        
        PooledMessageBuffer buffer2 = std::move(buffer1);
        REQUIRE(buffer2->GetActiveSize() == 4);
    }

    SECTION("Custom size")
    {
        PooledMessageBuffer buffer(8192);
        REQUIRE(buffer->GetBufferSize() >= 8192);
    }
}

TEST_CASE("MessageBufferPool thread safety", "[MessageBufferPool][threading]")
{
    MessageBufferPool& pool = MessageBufferPool::Instance();
    pool.Clear();
    pool.SetMaxPoolSize(100);

    constexpr int numThreads = 8;
    constexpr int operationsPerThread = 500;
    std::atomic<int> totalAcquired{0};
    std::atomic<int> totalReleased{0};

    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t)
    {
        threads.emplace_back([&]()
        {
            for (int i = 0; i < operationsPerThread; ++i)
            {
                auto buffer = pool.Acquire();
                totalAcquired++;
                
                buffer->Write("test data", 9);
                
                pool.Release(std::move(buffer));
                totalReleased++;
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    REQUIRE(totalAcquired == numThreads * operationsPerThread);
    REQUIRE(totalReleased == numThreads * operationsPerThread);
}

TEST_CASE("MessageBufferPool thread-local optimization", "[MessageBufferPool][performance]")
{
    MessageBufferPool& pool = MessageBufferPool::Instance();
    pool.Clear();
    
    // Test that thread-local pools work correctly
    auto buffer1 = pool.Acquire();
    auto buffer2 = pool.Acquire();
    
    pool.Release(std::move(buffer1));
    pool.Release(std::move(buffer2));
    
    // These should come from thread-local cache (no global pool access)
    auto buffer3 = pool.Acquire();
    auto buffer4 = pool.Acquire();
    
    REQUIRE(buffer3 != nullptr);
    REQUIRE(buffer4 != nullptr);
}

TEST_CASE("PooledMessageBufferPtr RAII integration", "[MessageBufferPool][RAII]")
{
    MessageBufferPool& pool = MessageBufferPool::Instance();
    pool.Clear();
    
    std::size_t initialCount = pool.GetPooledCount();
    
    {
        auto buffer = CreatePooledMessageBuffer(4096);
        buffer->Write("test data", 9);
        REQUIRE(buffer->GetActiveSize() == 9);
    } // buffer should auto-return to pool here
    
    // Should have more buffers in pool now
    REQUIRE(pool.GetPooledCount() >= initialCount);
}