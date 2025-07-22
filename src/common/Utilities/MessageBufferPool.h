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

#ifndef __MESSAGEBUFFERPOOL_H_
#define __MESSAGEBUFFERPOOL_H_

#include "MessageBuffer.h"
#include <memory>
#include <stack>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

class MessageBufferPool
{
private:
    struct ThreadLocalPool
    {
        std::stack<std::unique_ptr<MessageBuffer>> localPool;
        std::size_t localCount = 0;
        static constexpr std::size_t MAX_LOCAL_SIZE = 16;
    };

public:
    static MessageBufferPool& Instance()
    {
        static MessageBufferPool instance;
        return instance;
    }

    std::unique_ptr<MessageBuffer> Acquire(std::size_t minSize = 4096)
    {
        thread_local ThreadLocalPool tlsPool;
        
        // Try thread-local pool first (no locks)
        if (!tlsPool.localPool.empty())
        {
            auto buffer = std::move(tlsPool.localPool.top());
            tlsPool.localPool.pop();
            --tlsPool.localCount;
            
            if (buffer->GetBufferSize() >= minSize)
            {
                buffer->Reset();
                return buffer;
            }
            
            buffer->Resize(minSize);
            buffer->Reset();
            return buffer;
        }
        
        // Fallback to mutex-protected global pool
        {
            std::lock_guard<std::mutex> lock(_globalMutex);
            if (!_globalPool.empty())
            {
                auto buffer = std::move(_globalPool.back());
                _globalPool.pop_back();
                --_pooledCount;
                
                if (buffer->GetBufferSize() >= minSize)
                {
                    buffer->Reset();
                    return buffer;
                }
                
                buffer->Resize(minSize);
                buffer->Reset();
                return buffer;
            }
        }
        
        return std::make_unique<MessageBuffer>(minSize);
    }

    void Release(std::unique_ptr<MessageBuffer> buffer)
    {
        if (!buffer)
            return;

        thread_local ThreadLocalPool tlsPool;
        
        buffer->Reset();
        
        // Store in thread-local pool if space available
        if (tlsPool.localCount < ThreadLocalPool::MAX_LOCAL_SIZE)
        {
            tlsPool.localPool.push(std::move(buffer));
            ++tlsPool.localCount;
            return;
        }
        
        // Overflow to mutex-protected global pool
        {
            std::lock_guard<std::mutex> lock(_globalMutex);
            _globalPool.push_back(std::move(buffer));
            ++_pooledCount;
            
            // Trim excess buffers if needed
            while (_pooledCount > _maxPoolSize * 2 && !_globalPool.empty())
            {
                _globalPool.pop_back();
                --_pooledCount;
            }
        }
    }

    void SetMaxPoolSize(std::size_t maxSize)
    {
        _maxPoolSize = maxSize;
        
        // Trim global pool if needed
        std::lock_guard<std::mutex> lock(_globalMutex);
        while (_pooledCount.load() > _maxPoolSize && !_globalPool.empty())
        {
            _globalPool.pop_back();
            --_pooledCount;
        }
    }

    std::size_t GetPooledCount() const
    {
        return _pooledCount.load();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(_globalMutex);
        _globalPool.clear();
        _pooledCount = 0;
    }

private:
    MessageBufferPool() : _maxPoolSize(1000), _pooledCount(0) {}
    ~MessageBufferPool() = default;
    MessageBufferPool(MessageBufferPool const&) = delete;
    MessageBufferPool& operator=(MessageBufferPool const&) = delete;

    std::vector<std::unique_ptr<MessageBuffer>> _globalPool;
    std::mutex _globalMutex;
    std::atomic<std::size_t> _maxPoolSize;
    std::atomic<std::size_t> _pooledCount;
};

class PooledMessageBuffer
{
public:
    explicit PooledMessageBuffer(std::size_t minSize = 4096) 
        : _buffer(MessageBufferPool::Instance().Acquire(minSize)) {}
    
    ~PooledMessageBuffer()
    {
        if (_buffer)
            MessageBufferPool::Instance().Release(std::move(_buffer));
    }

    PooledMessageBuffer(PooledMessageBuffer const&) = delete;
    PooledMessageBuffer& operator=(PooledMessageBuffer const&) = delete;

    PooledMessageBuffer(PooledMessageBuffer&& other) noexcept
        : _buffer(std::move(other._buffer)) {}

    PooledMessageBuffer& operator=(PooledMessageBuffer&& other) noexcept
    {
        if (this != &other)
        {
            if (_buffer)
                MessageBufferPool::Instance().Release(std::move(_buffer));
            _buffer = std::move(other._buffer);
        }
        return *this;
    }

    MessageBuffer* operator->() { return _buffer.get(); }
    MessageBuffer const* operator->() const { return _buffer.get(); }
    MessageBuffer& operator*() { return *_buffer; }
    MessageBuffer const& operator*() const { return *_buffer; }
    
    MessageBuffer* Get() { return _buffer.get(); }
    MessageBuffer const* Get() const { return _buffer.get(); }

    // Extract the underlying MessageBuffer (for move into queues)
    MessageBuffer&& ExtractBuffer() { return std::move(*_buffer); }

private:
    std::unique_ptr<MessageBuffer> _buffer;
};

// Custom deleter for MessageBuffer that returns it to pool
struct MessageBufferPoolDeleter
{
    void operator()(MessageBuffer* buffer) const
    {
        if (buffer)
        {
            MessageBufferPool::Instance().Release(std::unique_ptr<MessageBuffer>(buffer));
        }
    }
};

using PooledMessageBufferPtr = std::unique_ptr<MessageBuffer, MessageBufferPoolDeleter>;

// Helper function to create pool-aware MessageBuffer
inline PooledMessageBufferPtr CreatePooledMessageBuffer(std::size_t size = 4096)
{
    auto buffer = MessageBufferPool::Instance().Acquire(size);
    return PooledMessageBufferPtr(buffer.release(), MessageBufferPoolDeleter{});
}

#endif /* __MESSAGEBUFFERPOOL_H_ */