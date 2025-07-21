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
#include <mutex>
#include <atomic>

class MessageBufferPool
{
public:
    static MessageBufferPool& Instance()
    {
        static MessageBufferPool instance;
        return instance;
    }

    std::unique_ptr<MessageBuffer> Acquire(std::size_t minSize = 4096)
    {
        std::lock_guard<std::mutex> lock(_poolMutex);
        
        if (!_pool.empty())
        {
            auto buffer = std::move(_pool.top());
            _pool.pop();
            
            if (buffer->GetBufferSize() >= minSize)
            {
                buffer->Reset();
                --_pooledCount;
                return buffer;
            }
            
            buffer->Resize(minSize);
            buffer->Reset();
            --_pooledCount;
            return buffer;
        }
        
        return std::make_unique<MessageBuffer>(minSize);
    }

    void Release(std::unique_ptr<MessageBuffer> buffer)
    {
        if (!buffer)
            return;

        std::lock_guard<std::mutex> lock(_poolMutex);
        
        if (_pooledCount < _maxPoolSize)
        {
            buffer->Reset();
            _pool.push(std::move(buffer));
            ++_pooledCount;
        }
    }

    void SetMaxPoolSize(std::size_t maxSize)
    {
        std::lock_guard<std::mutex> lock(_poolMutex);
        _maxPoolSize = maxSize;
        
        while (_pool.size() > _maxPoolSize)
        {
            _pool.pop();
            --_pooledCount;
        }
    }

    std::size_t GetPooledCount() const
    {
        return _pooledCount.load();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(_poolMutex);
        while (!_pool.empty())
        {
            _pool.pop();
        }
        _pooledCount = 0;
    }

private:
    MessageBufferPool() : _maxPoolSize(100), _pooledCount(0) {}
    ~MessageBufferPool() = default;
    MessageBufferPool(MessageBufferPool const&) = delete;
    MessageBufferPool& operator=(MessageBufferPool const&) = delete;

    std::stack<std::unique_ptr<MessageBuffer>> _pool;
    std::mutex _poolMutex;
    std::size_t _maxPoolSize;
    std::atomic<std::size_t> _pooledCount;
};

class PooledMessageBuffer
{
public:
    explicit PooledMessageBuffer(std::size_t minSize = 4096) 
        : _buffer(MessageBufferPool::Instance().Acquire(minSize)) {}
    
    ~PooledMessageBuffer()
    {
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

private:
    std::unique_ptr<MessageBuffer> _buffer;
};

#endif /* __MESSAGEBUFFERPOOL_H_ */