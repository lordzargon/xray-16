#include "stdafx.h"
#include "Layers/xrRender/BufferUtils.h"
#include "vkHW.h"
#include "xrCore/Threading/Lock.hpp"
#include "xrCore/Threading/ScopeLock.hpp"

#include <FlexibleVertexFormat.h>

namespace xray::render::RENDER_NAMESPACE
{
u32 GetFVFVertexSize(u32 FVF)
{
    return static_cast<u32>(::FVF::ComputeVertexSize(FVF));
}

u32 GetDeclVertexSize(const VertexElement* decl, u32 Stream)
{
    return static_cast<u32>(::FVF::ComputeVertexSize(decl, Stream));
}

u32 GetDeclLength(const VertexElement* decl)
{
    return static_cast<u32>(::FVF::GetDeclLength(decl));
}

void ConvertVertexDeclaration(const VertexElement* dxdecl, SDeclaration* decl)
{
    u32 dcl_size = GetDeclLength(dxdecl) + 1;
    decl->dcl_code.assign(dxdecl, dxdecl + dcl_size);
}

namespace BufferUtils
{
HRESULT CreateConstantBuffer(ConstantBufferHandle* ppBuffer, u32 DataSize)
{
    if (ppBuffer)
        *ppBuffer = VK_NULL_HANDLE;
    return S_OK;
}
}

static xr_map<VkBuffer, VkDeviceMemory> s_bufferMemoryMap;
static Lock s_bufferLock;

static void RegisterBufferMemory(VkBuffer buf, VkDeviceMemory mem)
{
    ScopeLock lock(&s_bufferLock);
    s_bufferMemoryMap[buf] = mem;
}

static VkDeviceMemory UnregisterBufferMemory(VkBuffer buf)
{
    ScopeLock lock(&s_bufferLock);
    auto it = s_bufferMemoryMap.find(buf);
    if (it != s_bufferMemoryMap.end())
    {
        VkDeviceMemory mem = it->second;
        s_bufferMemoryMap.erase(it);
        return mem;
    }
    return VK_NULL_HANDLE;
}

static VkDeviceMemory GetBufferMemory(VkBuffer buf)
{
    ScopeLock lock(&s_bufferLock);
    auto it = s_bufferMemoryMap.find(buf);
    if (it != s_bufferMemoryMap.end())
        return it->second;
    return VK_NULL_HANDLE;
}

//-----------------------------------------------------------------------------
VertexStagingBuffer::~VertexStagingBuffer()
{
    Destroy();
}

void VertexStagingBuffer::Create(size_t size, bool allowReadBack)
{
    m_Size = size;
    m_AllowReadBack = allowReadBack;
    m_HostBuffer = xr_alloc<u8>(size);
    AddRef();
}

bool VertexStagingBuffer::IsValid() const
{
    return m_HostBuffer != nullptr;
}

void* VertexStagingBuffer::Map(size_t offset, size_t size, bool read)
{
    VERIFY2(m_HostBuffer, "Buffer wasn't created or already discarded");
    VERIFY2(!read || m_AllowReadBack, "Can't read from write only buffer");
    VERIFY2((size + offset) <= m_Size, "Map region is too large");
    return static_cast<u8*>(m_HostBuffer) + offset;
}

void VertexStagingBuffer::Unmap(bool doFlush)
{
    if (!doFlush)
        return;
    VERIFY2(!m_DeviceBuffer, "Attempting to upload buffer twice");
    VERIFY(m_HostBuffer && m_Size);

    VkDeviceMemory mem = VK_NULL_HANDLE;
    HW.CreateBuffer(m_Size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_DeviceBuffer, mem);
    if (m_DeviceBuffer != VK_NULL_HANDLE && mem != VK_NULL_HANDLE)
    {
        RegisterBufferMemory(m_DeviceBuffer, mem);
        void* pData = nullptr;
        if (vkMapMemory(HW.m_device, mem, 0, m_Size, 0, &pData) == VK_SUCCESS)
        {
            CopyMemory(pData, m_HostBuffer, m_Size);
            vkUnmapMemory(HW.m_device, mem);
        }
    }

    if (!m_AllowReadBack)
    {
        DiscardHostBuffer();
    }
}

VertexBufferHandle VertexStagingBuffer::GetBufferHandle() const
{
    return m_DeviceBuffer;
}

void VertexStagingBuffer::Destroy()
{
    DiscardHostBuffer();
    m_Size = 0;
    if (m_DeviceBuffer != VK_NULL_HANDLE)
    {
        VkDeviceMemory mem = UnregisterBufferMemory(m_DeviceBuffer);
        if (mem != VK_NULL_HANDLE)
            vkFreeMemory(HW.m_device, mem, nullptr);
        vkDestroyBuffer(HW.m_device, m_DeviceBuffer, nullptr);
        m_DeviceBuffer = VK_NULL_HANDLE;
    }
}

void VertexStagingBuffer::DiscardHostBuffer()
{
    if (m_HostBuffer)
    {
        xr_free(m_HostBuffer);
        m_HostBuffer = nullptr;
    }
}

size_t VertexStagingBuffer::GetSystemMemoryUsage() const
{
    return m_HostBuffer ? m_Size : 0;
}

size_t VertexStagingBuffer::GetVideoMemoryUsage() const
{
    return 0;
}

//-----------------------------------------------------------------------------
IndexStagingBuffer::~IndexStagingBuffer()
{
    Destroy();
}

void IndexStagingBuffer::Create(size_t size, bool allowReadBack, bool /*managed*/)
{
    m_Size = size;
    m_AllowReadBack = allowReadBack;
    m_HostBuffer = xr_alloc<u8>(size);
    AddRef();
}

bool IndexStagingBuffer::IsValid() const
{
    return m_HostBuffer != nullptr;
}

void* IndexStagingBuffer::Map(size_t offset, size_t size, bool read)
{
    VERIFY2(m_HostBuffer, "Buffer wasn't created or already discarded");
    VERIFY2(!read || m_AllowReadBack, "Can't read from write only buffer");
    VERIFY2((size + offset) <= m_Size, "Map region is too large");
    return static_cast<u8*>(m_HostBuffer) + offset;
}

void IndexStagingBuffer::Unmap(bool doFlush)
{
    if (!doFlush)
        return;
    VERIFY2(!m_DeviceBuffer, "Attempting to upload buffer twice");
    VERIFY(m_HostBuffer && m_Size);

    VkDeviceMemory mem = VK_NULL_HANDLE;
    HW.CreateBuffer(m_Size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_DeviceBuffer, mem);
    if (m_DeviceBuffer != VK_NULL_HANDLE && mem != VK_NULL_HANDLE)
    {
        RegisterBufferMemory(m_DeviceBuffer, mem);
        void* pData = nullptr;
        if (vkMapMemory(HW.m_device, mem, 0, m_Size, 0, &pData) == VK_SUCCESS)
        {
            CopyMemory(pData, m_HostBuffer, m_Size);
            vkUnmapMemory(HW.m_device, mem);
        }
    }

    if (!m_AllowReadBack)
    {
        DiscardHostBuffer();
    }
}

IndexBufferHandle IndexStagingBuffer::GetBufferHandle() const
{
    return m_DeviceBuffer;
}

void IndexStagingBuffer::Destroy()
{
    DiscardHostBuffer();
    m_Size = 0;
    if (m_DeviceBuffer != VK_NULL_HANDLE)
    {
        VkDeviceMemory mem = UnregisterBufferMemory(m_DeviceBuffer);
        if (mem != VK_NULL_HANDLE)
            vkFreeMemory(HW.m_device, mem, nullptr);
        vkDestroyBuffer(HW.m_device, m_DeviceBuffer, nullptr);
        m_DeviceBuffer = VK_NULL_HANDLE;
    }
}

void IndexStagingBuffer::DiscardHostBuffer()
{
    if (m_HostBuffer)
    {
        xr_free(m_HostBuffer);
        m_HostBuffer = nullptr;
    }
}

size_t IndexStagingBuffer::GetSystemMemoryUsage() const
{
    return m_HostBuffer ? m_Size : 0;
}

size_t IndexStagingBuffer::GetVideoMemoryUsage() const
{
    return 0;
}

//-----------------------------------------------------------------------------
VertexStreamBuffer::~VertexStreamBuffer()
{
    Destroy();
}

void VertexStreamBuffer::Create(size_t size)
{
    VkDeviceMemory mem = VK_NULL_HANDLE;
    HW.CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_DeviceBuffer, mem);
    if (m_DeviceBuffer != VK_NULL_HANDLE)
    {
        RegisterBufferMemory(m_DeviceBuffer, mem);
    }
    AddRef();
}

void VertexStreamBuffer::Destroy()
{
    if (m_DeviceBuffer != VK_NULL_HANDLE)
    {
        VkDeviceMemory mem = UnregisterBufferMemory(m_DeviceBuffer);
        if (mem != VK_NULL_HANDLE)
            vkFreeMemory(HW.m_device, mem, nullptr);
        vkDestroyBuffer(HW.m_device, m_DeviceBuffer, nullptr);
        m_DeviceBuffer = VK_NULL_HANDLE;
    }
}

void* VertexStreamBuffer::Map(size_t offset, size_t size, bool /*flush*/)
{
    if (m_DeviceBuffer == VK_NULL_HANDLE)
        return nullptr;
    VkDeviceMemory mem = GetBufferMemory(m_DeviceBuffer);
    if (mem == VK_NULL_HANDLE)
        return nullptr;
    void* pData = nullptr;
    CHK_VK(vkMapMemory(HW.m_device, mem, offset, size, 0, &pData));
    return pData;
}

void VertexStreamBuffer::Unmap()
{
    if (m_DeviceBuffer == VK_NULL_HANDLE)
        return;
    VkDeviceMemory mem = GetBufferMemory(m_DeviceBuffer);
    if (mem != VK_NULL_HANDLE)
        vkUnmapMemory(HW.m_device, mem);
}

bool VertexStreamBuffer::IsValid() const
{
    return m_DeviceBuffer != VK_NULL_HANDLE;
}

//-----------------------------------------------------------------------------
IndexStreamBuffer::~IndexStreamBuffer()
{
    Destroy();
}

void IndexStreamBuffer::Create(size_t size)
{
    VkDeviceMemory mem = VK_NULL_HANDLE;
    HW.CreateBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_DeviceBuffer, mem);
    if (m_DeviceBuffer != VK_NULL_HANDLE)
    {
        RegisterBufferMemory(m_DeviceBuffer, mem);
    }
    AddRef();
}

void IndexStreamBuffer::Destroy()
{
    if (m_DeviceBuffer != VK_NULL_HANDLE)
    {
        VkDeviceMemory mem = UnregisterBufferMemory(m_DeviceBuffer);
        if (mem != VK_NULL_HANDLE)
            vkFreeMemory(HW.m_device, mem, nullptr);
        vkDestroyBuffer(HW.m_device, m_DeviceBuffer, nullptr);
        m_DeviceBuffer = VK_NULL_HANDLE;
    }
}

void* IndexStreamBuffer::Map(size_t offset, size_t size, bool /*flush*/)
{
    if (m_DeviceBuffer == VK_NULL_HANDLE)
        return nullptr;
    VkDeviceMemory mem = GetBufferMemory(m_DeviceBuffer);
    if (mem == VK_NULL_HANDLE)
        return nullptr;
    void* pData = nullptr;
    CHK_VK(vkMapMemory(HW.m_device, mem, offset, size, 0, &pData));
    return pData;
}

void IndexStreamBuffer::Unmap()
{
    if (m_DeviceBuffer == VK_NULL_HANDLE)
        return;
    VkDeviceMemory mem = GetBufferMemory(m_DeviceBuffer);
    if (mem != VK_NULL_HANDLE)
        vkUnmapMemory(HW.m_device, mem);
}

bool IndexStreamBuffer::IsValid() const
{
    return m_DeviceBuffer != VK_NULL_HANDLE;
}

} // namespace xray::render::RENDER_NAMESPACE
