//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://kylelutz.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_COMMAND_QUEUE_H
#define BOOST_COMPUTE_COMMAND_QUEUE_H

#include <cstddef>

#include <boost/assert.hpp>

#include <boost/compute/config.hpp>
#include <boost/compute/event.hpp>
#include <boost/compute/buffer.hpp>
#include <boost/compute/device.hpp>
#include <boost/compute/kernel.hpp>
#include <boost/compute/context.hpp>
#include <boost/compute/image2d.hpp>
#include <boost/compute/image3d.hpp>
#include <boost/compute/exception.hpp>
#include <boost/compute/wait_list.hpp>
#include <boost/compute/detail/get_object_info.hpp>
#include <boost/compute/detail/assert_cl_success.hpp>

namespace boost {
namespace compute {
namespace detail {

inline void BOOST_COMPUTE_CL_CALLBACK
nullary_native_kernel_trampoline(void *user_func_ptr)
{
    void (*user_func)();
    std::memcpy(&user_func, user_func_ptr, sizeof(user_func));
    user_func();
}

} // end detail namespace

/// \class command_queue
/// \brief A command queue.
///
/// Command queues provide the interface for interacting with compute
/// devices. The command_queue class provides methods to copy data to
/// and from a compute device as well as execute compute kernels.
///
/// Command queues are created for a compute device within a compute
/// context.
///
/// For example, to create a context and command queue for the default device
/// on the system (this is the normal set up code used by almost all OpenCL
/// programs):
/// \code
/// #include <boost/compute/core.hpp>
///
/// // get the default compute device
/// boost::compute::device device = boost::compute::system::default_device();
///
/// // set up a compute context and command queue
/// boost::compute::context context(device);
/// boost::compute::command_queue queue(context, device);
/// \endcode
///
/// The default command queue for the system can be obtained with the
/// system::default_queue() method.
///
/// \see buffer, context, kernel
class command_queue
{
public:
    enum properties {
        enable_profiling = CL_QUEUE_PROFILING_ENABLE,
        enable_out_of_order_execution = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
    };

    enum map_flags {
        map_read = CL_MAP_READ,
        map_write = CL_MAP_WRITE
        #ifdef CL_VERSION_2_0
        ,
        map_write_invalidate_region = CL_MAP_WRITE_INVALIDATE_REGION
        #endif
    };

    /// Creates a null command queue.
    command_queue()
        : m_queue(0)
    {
    }

    explicit command_queue(cl_command_queue queue, bool retain = true)
        : m_queue(queue)
    {
        if(m_queue && retain){
            clRetainCommandQueue(m_queue);
        }
    }

    /// Creates a command queue in \p context for \p device with
    /// \p properties.
    ///
    /// \see_opencl_ref{clCreateCommandQueue}
    command_queue(const context &context,
                  const device &device,
                  cl_command_queue_properties properties = 0)
    {
        BOOST_ASSERT(device.id() != 0);

        cl_int error = 0;

        #ifdef CL_VERSION_2_0
        std::vector<cl_queue_properties> queue_properties;
        if(properties){
            queue_properties.push_back(CL_QUEUE_PROPERTIES);
            queue_properties.push_back(cl_queue_properties(properties));
            queue_properties.push_back(cl_queue_properties(0));
        }

        const cl_queue_properties *queue_properties_ptr =
            queue_properties.empty() ? 0 : &queue_properties[0];

        m_queue = clCreateCommandQueueWithProperties(
            context, device.id(), queue_properties_ptr, &error
        );
        #else
        m_queue = clCreateCommandQueue(
            context, device.id(), properties, &error
        );
        #endif

        if(!m_queue){
            BOOST_THROW_EXCEPTION(opencl_error(error));
        }
    }

    /// Creates a new command queue object as a copy of \p other.
    command_queue(const command_queue &other)
        : m_queue(other.m_queue)
    {
        if(m_queue){
            clRetainCommandQueue(m_queue);
        }
    }

    /// Copies the command queue object from \p other to \c *this.
    command_queue& operator=(const command_queue &other)
    {
        if(this != &other){
            if(m_queue){
                clReleaseCommandQueue(m_queue);
            }

            m_queue = other.m_queue;

            if(m_queue){
                clRetainCommandQueue(m_queue);
            }
        }

        return *this;
    }

    #ifndef BOOST_COMPUTE_NO_RVALUE_REFERENCES
    /// Move-constructs a new command queue object from \p other.
    command_queue(command_queue&& other) BOOST_NOEXCEPT
        : m_queue(other.m_queue)
    {
        other.m_queue = 0;
    }

    /// Move-assigns the command queue from \p other to \c *this.
    command_queue& operator=(command_queue&& other) BOOST_NOEXCEPT
    {
        if(m_queue){
            clReleaseCommandQueue(m_queue);
        }

        m_queue = other.m_queue;
        other.m_queue = 0;

        return *this;
    }
    #endif // BOOST_COMPUTE_NO_RVALUE_REFERENCES

    /// Destroys the command queue.
    ///
    /// \see_opencl_ref{clReleaseCommandQueue}
    ~command_queue()
    {
        if(m_queue){
            BOOST_COMPUTE_ASSERT_CL_SUCCESS(
                clReleaseCommandQueue(m_queue)
            );
        }
    }

    /// Returns the underlying OpenCL command queue.
    cl_command_queue& get() const
    {
        return const_cast<cl_command_queue &>(m_queue);
    }

    /// Returns the device that the command queue issues commands to.
    device get_device() const
    {
        return device(get_info<cl_device_id>(CL_QUEUE_DEVICE));
    }

    /// Returns the context for the command queue.
    context get_context() const
    {
        return context(get_info<cl_context>(CL_QUEUE_CONTEXT));
    }

    /// Returns information about the command queue.
    ///
    /// \see_opencl_ref{clGetCommandQueueInfo}
    template<class T>
    T get_info(cl_command_queue_info info) const
    {
        return detail::get_object_info<T>(clGetCommandQueueInfo, m_queue, info);
    }

    /// \overload
    template<int Enum>
    typename detail::get_object_info_type<command_queue, Enum>::type
    get_info() const;

    /// Returns the properties for the command queue.
    cl_command_queue_properties get_properties() const
    {
        return get_info<cl_command_queue_properties>(CL_QUEUE_PROPERTIES);
    }

    /// Enqueues a command to read data from \p buffer to host memory.
    ///
    /// \see_opencl_ref{clEnqueueReadBuffer}
    ///
    /// \see copy()
    void enqueue_read_buffer(const buffer &buffer,
                             size_t offset,
                             size_t size,
                             void *host_ptr,
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(size <= buffer.size());
        BOOST_ASSERT(buffer.get_context() == this->get_context());
        BOOST_ASSERT(host_ptr != 0);

        cl_int ret = clEnqueueReadBuffer(
            m_queue,
            buffer.get(),
            CL_TRUE,
            offset,
            size,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    /// Enqueues a command to read data from \p buffer to host memory. The
    /// copy will be performed asynchronously.
    ///
    /// \see_opencl_ref{clEnqueueReadBuffer}
    ///
    /// \see copy_async()
    event enqueue_read_buffer_async(const buffer &buffer,
                                    size_t offset,
                                    size_t size,
                                    void *host_ptr,
                                    const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(size <= buffer.size());
        BOOST_ASSERT(buffer.get_context() == this->get_context());
        BOOST_ASSERT(host_ptr != 0);

        event event_;

        cl_int ret = clEnqueueReadBuffer(
            m_queue,
            buffer.get(),
            CL_FALSE,
            offset,
            size,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    #if defined(CL_VERSION_1_1) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Enqueues a command to read a rectangular region from \p buffer to
    /// host memory.
    ///
    /// \see_opencl_ref{clEnqueueReadBufferRect}
    ///
    /// \opencl_version_warning{1,1}
    void enqueue_read_buffer_rect(const buffer &buffer,
                                  const size_t buffer_origin[3],
                                  const size_t host_origin[3],
                                  const size_t region[3],
                                  size_t buffer_row_pitch,
                                  size_t buffer_slice_pitch,
                                  size_t host_row_pitch,
                                  size_t host_slice_pitch,
                                  void *host_ptr,
                                  const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(buffer.get_context() == this->get_context());
        BOOST_ASSERT(host_ptr != 0);

        cl_int ret = clEnqueueReadBufferRect(
            m_queue,
            buffer.get(),
            CL_TRUE,
            buffer_origin,
            host_origin,
            region,
            buffer_row_pitch,
            buffer_slice_pitch,
            host_row_pitch,
            host_slice_pitch,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }
    #endif // CL_VERSION_1_1

    /// Enqueues a command to write data from host memory to \p buffer.
    ///
    /// \see_opencl_ref{clEnqueueWriteBuffer}
    ///
    /// \see copy()
    void enqueue_write_buffer(const buffer &buffer,
                              size_t offset,
                              size_t size,
                              const void *host_ptr,
                              const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(size <= buffer.size());
        BOOST_ASSERT(buffer.get_context() == this->get_context());
        BOOST_ASSERT(host_ptr != 0);

        cl_int ret = clEnqueueWriteBuffer(
            m_queue,
            buffer.get(),
            CL_TRUE,
            offset,
            size,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    /// Enqueues a command to write data from host memory to \p buffer.
    /// The copy is performed asynchronously.
    ///
    /// \see_opencl_ref{clEnqueueWriteBuffer}
    ///
    /// \see copy_async()
    event enqueue_write_buffer_async(const buffer &buffer,
                                     size_t offset,
                                     size_t size,
                                     const void *host_ptr,
                                     const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(size <= buffer.size());
        BOOST_ASSERT(buffer.get_context() == this->get_context());
        BOOST_ASSERT(host_ptr != 0);

        event event_;

        cl_int ret = clEnqueueWriteBuffer(
            m_queue,
            buffer.get(),
            CL_FALSE,
            offset,
            size,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    #if defined(CL_VERSION_1_1) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Enqueues a command to write a rectangular region from host memory
    /// to \p buffer.
    ///
    /// \see_opencl_ref{clEnqueueWriteBufferRect}
    ///
    /// \opencl_version_warning{1,1}
    void enqueue_write_buffer_rect(const buffer &buffer,
                                   const size_t buffer_origin[3],
                                   const size_t host_origin[3],
                                   const size_t region[3],
                                   size_t buffer_row_pitch,
                                   size_t buffer_slice_pitch,
                                   size_t host_row_pitch,
                                   size_t host_slice_pitch,
                                   void *host_ptr,
                                   const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(buffer.get_context() == this->get_context());
        BOOST_ASSERT(host_ptr != 0);

        cl_int ret = clEnqueueWriteBufferRect(
            m_queue,
            buffer.get(),
            CL_TRUE,
            buffer_origin,
            host_origin,
            region,
            buffer_row_pitch,
            buffer_slice_pitch,
            host_row_pitch,
            host_slice_pitch,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }
    #endif // CL_VERSION_1_1

    /// Enqueues a command to copy data from \p src_buffer to
    /// \p dst_buffer.
    ///
    /// \see_opencl_ref{clEnqueueCopyBuffer}
    ///
    /// \see copy()
    event enqueue_copy_buffer(const buffer &src_buffer,
                              const buffer &dst_buffer,
                              size_t src_offset,
                              size_t dst_offset,
                              size_t size,
                              const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_offset + size <= src_buffer.size());
        BOOST_ASSERT(dst_offset + size <= dst_buffer.size());
        BOOST_ASSERT(src_buffer.get_context() == this->get_context());
        BOOST_ASSERT(dst_buffer.get_context() == this->get_context());

        event event_;

        cl_int ret = clEnqueueCopyBuffer(
            m_queue,
            src_buffer.get(),
            dst_buffer.get(),
            src_offset,
            dst_offset,
            size,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    #if defined(CL_VERSION_1_1) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Enqueues a command to copy a rectangular region from
    /// \p src_buffer to \p dst_buffer.
    ///
    /// \see_opencl_ref{clEnqueueCopyBufferRect}
    ///
    /// \opencl_version_warning{1,1}
    event enqueue_copy_buffer_rect(const buffer &src_buffer,
                                   const buffer &dst_buffer,
                                   const size_t src_origin[3],
                                   const size_t dst_origin[3],
                                   const size_t region[3],
                                   size_t buffer_row_pitch,
                                   size_t buffer_slice_pitch,
                                   size_t host_row_pitch,
                                   size_t host_slice_pitch,
                                   const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_buffer.get_context() == this->get_context());
        BOOST_ASSERT(dst_buffer.get_context() == this->get_context());

        event event_;

        cl_int ret = clEnqueueCopyBufferRect(
            m_queue,
            src_buffer.get(),
            dst_buffer.get(),
            src_origin,
            dst_origin,
            region,
            buffer_row_pitch,
            buffer_slice_pitch,
            host_row_pitch,
            host_slice_pitch,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }
    #endif // CL_VERSION_1_1

    #if defined(CL_VERSION_1_2) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Enqueues a command to fill \p buffer with \p pattern.
    ///
    /// \see_opencl_ref{clEnqueueFillBuffer}
    ///
    /// \opencl_version_warning{1,2}
    ///
    /// \see fill()
    event enqueue_fill_buffer(const buffer &buffer,
                              const void *pattern,
                              size_t pattern_size,
                              size_t offset,
                              size_t size,
                              const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(offset + size <= buffer.size());
        BOOST_ASSERT(buffer.get_context() == this->get_context());

        event event_;

        cl_int ret = clEnqueueFillBuffer(
            m_queue,
            buffer.get(),
            pattern,
            pattern_size,
            offset,
            size,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }
    #endif // CL_VERSION_1_2

    /// Enqueues a command to map \p buffer into the host address space.
    ///
    /// \see_opencl_ref{clEnqueueMapBuffer}
    void* enqueue_map_buffer(const buffer &buffer,
                             cl_map_flags flags,
                             size_t offset,
                             size_t size,
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(offset + size <= buffer.size());
        BOOST_ASSERT(buffer.get_context() == this->get_context());

        cl_int ret = 0;
        void *pointer = clEnqueueMapBuffer(
            m_queue,
            buffer.get(),
            CL_TRUE,
            flags,
            offset,
            size,
            events.size(),
            events.get_event_ptr(),
            0,
            &ret
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return pointer;
    }

    /// Enqueues a command to unmap \p buffer from the host memory space.
    ///
    /// \see_opencl_ref{clEnqueueUnmapMemObject}
    event enqueue_unmap_buffer(const buffer &buffer,
                               void *mapped_ptr,
                               const wait_list &events = wait_list())
    {
        BOOST_ASSERT(buffer.get_context() == this->get_context());

        return enqueue_unmap_mem_object(buffer.get(), mapped_ptr, events);
    }

    /// Enqueues a command to unmap \p mem from the host memory space.
    ///
    /// \see_opencl_ref{clEnqueueUnmapMemObject}
    event enqueue_unmap_mem_object(cl_mem mem,
                                   void *mapped_ptr,
                                   const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);

        event event_;

        cl_int ret = clEnqueueUnmapMemObject(
            m_queue,
            mem,
            mapped_ptr,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to read data from \p image to host memory.
    ///
    /// \see_opencl_ref{clEnqueueReadImage}
    void enqueue_read_image(const image2d &image,
                            const size_t origin[2],
                            const size_t region[2],
                            size_t row_pitch,
                            void *host_ptr,
                            const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(image.get_context() == this->get_context());

        const size_t origin3[3] = { origin[0], origin[1], size_t(0) };
        const size_t region3[3] = { region[0], region[1], size_t(1) };

        cl_int ret = clEnqueueReadImage(
            m_queue,
            image.get(),
            CL_TRUE,
            origin3,
            region3,
            row_pitch,
            0,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    /// Enqueues a command to read data from \p image to host memory.
    ///
    /// \see_opencl_ref{clEnqueueReadImage}
    void enqueue_read_image(const image3d &image,
                            const size_t origin[3],
                            const size_t region[3],
                            size_t row_pitch,
                            size_t slice_pitch,
                            void *host_ptr,
                            const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(image.get_context() == this->get_context());

        cl_int ret = clEnqueueReadImage(
            m_queue,
            image.get(),
            CL_TRUE,
            origin,
            region,
            row_pitch,
            slice_pitch,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    /// Enqueues a command to write data from host memory to \p image.
    ///
    /// \see_opencl_ref{clEnqueueWriteImage}
    void enqueue_write_image(const image2d &image,
                             const size_t origin[2],
                             const size_t region[2],
                             size_t input_row_pitch,
                             const void *host_ptr,
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(image.get_context() == this->get_context());

        const size_t origin3[3] = { origin[0], origin[1], size_t(0) };
        const size_t region3[3] = { region[0], region[1], size_t(1) };

        cl_int ret = clEnqueueWriteImage(
            m_queue,
            image.get(),
            CL_TRUE,
            origin3,
            region3,
            input_row_pitch,
            0,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    /// Enqueues a command to write data from host memory to \p image.
    ///
    /// \see_opencl_ref{clEnqueueWriteImage}
    void enqueue_write_image(const image3d &image,
                             const size_t origin[3],
                             const size_t region[3],
                             size_t input_row_pitch,
                             size_t input_slice_pitch,
                             const void *host_ptr,
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(image.get_context() == this->get_context());

        cl_int ret = clEnqueueWriteImage(
            m_queue,
            image.get(),
            CL_TRUE,
            origin,
            region,
            input_row_pitch,
            input_slice_pitch,
            host_ptr,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    /// Enqueues a command to copy data from \p src_image to \p dst_image.
    ///
    /// \see_opencl_ref{clEnqueueCopyImage}
    event enqueue_copy_image(const image2d &src_image,
                             const image2d &dst_image,
                             const size_t src_origin[2],
                             const size_t dst_origin[2],
                             const size_t region[2],
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_image.get_context() == this->get_context());
        BOOST_ASSERT(dst_image.get_context() == this->get_context());
        BOOST_ASSERT_MSG(src_image.get_format() == dst_image.get_format(),
                         "Source and destination image formats must match.");

        const size_t src_origin3[3] = { src_origin[0], src_origin[1], size_t(0) };
        const size_t dst_origin3[3] = { dst_origin[0], dst_origin[1], size_t(0) };
        const size_t region3[3] = { region[0], region[1], size_t(1) };

        event event_;

        cl_int ret = clEnqueueCopyImage(
            m_queue,
            src_image.get(),
            dst_image.get(),
            src_origin3,
            dst_origin3,
            region3,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to copy data from \p src_image to \p dst_image.
    ///
    /// \see_opencl_ref{clEnqueueCopyImage}
    event enqueue_copy_image(const image2d &src_image,
                             const image3d &dst_image,
                             const size_t src_origin[2],
                             const size_t dst_origin[3],
                             const size_t region[2],
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_image.get_context() == this->get_context());
        BOOST_ASSERT(dst_image.get_context() == this->get_context());
        BOOST_ASSERT_MSG(src_image.get_format() == dst_image.get_format(),
                         "Source and destination image formats must match.");

        const size_t src_origin3[3] = { src_origin[0], src_origin[1], size_t(0) };
        const size_t region3[3] = { region[0], region[1], size_t(1) };

        event event_;

        cl_int ret = clEnqueueCopyImage(
            m_queue,
            src_image.get(),
            dst_image.get(),
            src_origin3,
            dst_origin,
            region3,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to copy data from \p src_image to \p dst_image.
    ///
    /// \see_opencl_ref{clEnqueueCopyImage}
    event enqueue_copy_image(const image3d &src_image,
                             const image2d &dst_image,
                             const size_t src_origin[3],
                             const size_t dst_origin[2],
                             const size_t region[2],
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_image.get_context() == this->get_context());
        BOOST_ASSERT(dst_image.get_context() == this->get_context());
        BOOST_ASSERT_MSG(src_image.get_format() == dst_image.get_format(),
                         "Source and destination image formats must match.");

        const size_t dst_origin3[3] = { dst_origin[0], dst_origin[1], size_t(0) };
        const size_t region3[3] = { region[0], region[1], size_t(1) };

        event event_;

        cl_int ret = clEnqueueCopyImage(
            m_queue,
            src_image.get(),
            dst_image.get(),
            src_origin,
            dst_origin3,
            region3,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to copy data from \p src_image to \p dst_image.
    ///
    /// \see_opencl_ref{clEnqueueCopyImage}
    event enqueue_copy_image(const image3d &src_image,
                             const image3d &dst_image,
                             const size_t src_origin[3],
                             const size_t dst_origin[3],
                             const size_t region[3],
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_image.get_context() == this->get_context());
        BOOST_ASSERT(dst_image.get_context() == this->get_context());
        BOOST_ASSERT_MSG(src_image.get_format() == dst_image.get_format(),
                         "Source and destination image formats must match.");

        event event_;

        cl_int ret = clEnqueueCopyImage(
            m_queue,
            src_image.get(),
            dst_image.get(),
            src_origin,
            dst_origin,
            region,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to copy data from \p src_image to \p dst_buffer.
    ///
    /// \see_opencl_ref{clEnqueueCopyImageToBuffer}
    event enqueue_copy_image_to_buffer(const image2d &src_image,
                                       const buffer &dst_buffer,
                                       const size_t src_origin[2],
                                       const size_t region[2],
                                       size_t dst_offset,
                                       const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_image.get_context() == this->get_context());
        BOOST_ASSERT(dst_buffer.get_context() == this->get_context());

        const size_t src_origin3[3] = { src_origin[0], src_origin[1], size_t(0) };
        const size_t region3[3] = { region[0], region[1], size_t(1) };

        event event_;

        cl_int ret = clEnqueueCopyImageToBuffer(
            m_queue,
            src_image.get(),
            dst_buffer.get(),
            src_origin3,
            region3,
            dst_offset,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to copy data from \p src_image to \p dst_buffer.
    ///
    /// \see_opencl_ref{clEnqueueCopyImageToBuffer}
    event enqueue_copy_image_to_buffer(const image3d &src_image,
                                       const buffer &dst_buffer,
                                       const size_t src_origin[3],
                                       const size_t region[3],
                                       size_t dst_offset,
                                       const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_image.get_context() == this->get_context());
        BOOST_ASSERT(dst_buffer.get_context() == this->get_context());

        event event_;

        cl_int ret = clEnqueueCopyImageToBuffer(
            m_queue,
            src_image.get(),
            dst_buffer.get(),
            src_origin,
            region,
            dst_offset,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to copy data from \p src_buffer to \p dst_image.
    ///
    /// \see_opencl_ref{clEnqueueCopyBufferToImage}
    event enqueue_copy_buffer_to_image(const buffer &src_buffer,
                                       const image2d &dst_image,
                                       size_t src_offset,
                                       const size_t dst_origin[3],
                                       const size_t region[3],
                                       const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_buffer.get_context() == this->get_context());
        BOOST_ASSERT(dst_image.get_context() == this->get_context());

        const size_t dst_origin3[3] = { dst_origin[0], dst_origin[1], size_t(0) };
        const size_t region3[3] = { region[0], region[1], size_t(1) };

        event event_;

        cl_int ret = clEnqueueCopyBufferToImage(
            m_queue,
            src_buffer.get(),
            dst_image.get(),
            src_offset,
            dst_origin3,
            region3,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to copy data from \p src_buffer to \p dst_image.
    ///
    /// \see_opencl_ref{clEnqueueCopyBufferToImage}
    event enqueue_copy_buffer_to_image(const buffer &src_buffer,
                                       const image3d &dst_image,
                                       size_t src_offset,
                                       const size_t dst_origin[3],
                                       const size_t region[3],
                                       const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(src_buffer.get_context() == this->get_context());
        BOOST_ASSERT(dst_image.get_context() == this->get_context());

        event event_;

        cl_int ret = clEnqueueCopyBufferToImage(
            m_queue,
            src_buffer.get(),
            dst_image.get(),
            src_offset,
            dst_origin,
            region,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    #if defined(CL_VERSION_1_2) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Enqueues a command to fill \p image with \p fill_color.
    ///
    /// \see_opencl_ref{clEnqueueFillImage}
    ///
    /// \opencl_version_warning{1,2}
    event enqueue_fill_image(const image2d &image,
                             const void *fill_color,
                             const size_t origin[2],
                             const size_t region[2],
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(image.get_context() == this->get_context());

        const size_t origin3[3] = { origin[0], origin[1], size_t(0) };
        const size_t region3[3] = { region[0], region[1], size_t(1) };

        event event_;

        cl_int ret = clEnqueueFillImage(
            m_queue,
            image.get(),
            fill_color,
            origin3,
            region3,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to fill \p image with \p fill_color.
    ///
    /// \see_opencl_ref{clEnqueueFillImage}
    ///
    /// \opencl_version_warning{1,2}
    event enqueue_fill_image(const image3d &image,
                             const void *fill_color,
                             const size_t origin[3],
                             const size_t region[3],
                             const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(image.get_context() == this->get_context());

        event event_;

        cl_int ret = clEnqueueFillImage(
            m_queue,
            image.get(),
            fill_color,
            origin,
            region,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to migrate \p mem_objects.
    ///
    /// \see_opencl_ref{clEnqueueMigrateMemObjects}
    ///
    /// \opencl_version_warning{1,2}
    event enqueue_migrate_memory_objects(uint_ num_mem_objects,
                                         const cl_mem *mem_objects,
                                         cl_mem_migration_flags flags,
                                         const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);

        event event_;

        cl_int ret = clEnqueueMigrateMemObjects(
            m_queue,
            num_mem_objects,
            mem_objects,
            flags,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }
    #endif // CL_VERSION_1_2

    /// Enqueues a kernel for execution.
    ///
    /// \see_opencl_ref{clEnqueueNDRangeKernel}
    event enqueue_nd_range_kernel(const kernel &kernel,
                                  size_t work_dim,
                                  const size_t *global_work_offset,
                                  const size_t *global_work_size,
                                  const size_t *local_work_size,
                                  const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(kernel.get_context() == this->get_context());

        event event_;

        cl_int ret = clEnqueueNDRangeKernel(
            m_queue,
            kernel,
            static_cast<cl_uint>(work_dim),
            global_work_offset,
            global_work_size,
            local_work_size,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Convenience method which calls enqueue_nd_range_kernel() with a
    /// one-dimensional range.
    event enqueue_1d_range_kernel(const kernel &kernel,
                                  size_t global_work_offset,
                                  size_t global_work_size,
                                  size_t local_work_size,
                                  const wait_list &events = wait_list())
    {
        return enqueue_nd_range_kernel(
            kernel,
            1,
            &global_work_offset,
            &global_work_size,
            local_work_size ? &local_work_size : 0,
            events
        );
    }

    /// Enqueues a kernel to execute using a single work-item.
    ///
    /// \see_opencl_ref{clEnqueueTask}
    event enqueue_task(const kernel &kernel, const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);
        BOOST_ASSERT(kernel.get_context() == this->get_context());

        event event_;

        // clEnqueueTask() was deprecated in OpenCL 2.0. In that case we
        // just forward to the equivalent clEnqueueNDRangeKernel() call.
        #ifdef CL_VERSION_2_0
        size_t one = 1;
        cl_int ret = clEnqueueNDRangeKernel(
            m_queue, kernel, 1, 0, &one, &one,
            events.size(), events.get_event_ptr(), &event_.get()
        );
        #else
        cl_int ret = clEnqueueTask(
            m_queue, kernel, events.size(), events.get_event_ptr(), &event_.get()
        );
        #endif

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a function to execute on the host.
    event enqueue_native_kernel(void (BOOST_COMPUTE_CL_CALLBACK *user_func)(void *),
                                void *args,
                                size_t cb_args,
                                uint_ num_mem_objects,
                                const cl_mem *mem_list,
                                const void **args_mem_loc,
                                const wait_list &events = wait_list())
    {
        BOOST_ASSERT(m_queue != 0);

        event event_;
        cl_int ret = clEnqueueNativeKernel(
            m_queue,
            user_func,
            args,
            cb_args,
            num_mem_objects,
            mem_list,
            args_mem_loc,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );
        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Convenience overload for enqueue_native_kernel() which enqueues a
    /// native kernel on the host with a nullary function.
    event enqueue_native_kernel(void (BOOST_COMPUTE_CL_CALLBACK *user_func)(void),
                                const wait_list &events = wait_list())
    {
        return enqueue_native_kernel(
            detail::nullary_native_kernel_trampoline,
            reinterpret_cast<void *>(&user_func),
            sizeof(user_func),
            0,
            0,
            0,
            events
        );
    }

    /// Flushes the command queue.
    ///
    /// \see_opencl_ref{clFlush}
    void flush()
    {
        BOOST_ASSERT(m_queue != 0);

        clFlush(m_queue);
    }

    /// Blocks until all outstanding commands in the queue have finished.
    ///
    /// \see_opencl_ref{clFinish}
    void finish()
    {
        BOOST_ASSERT(m_queue != 0);

        clFinish(m_queue);
    }

    /// Enqueues a barrier in the queue.
    void enqueue_barrier()
    {
        BOOST_ASSERT(m_queue != 0);

        #ifdef CL_VERSION_1_2
        clEnqueueBarrierWithWaitList(m_queue, 0, 0, 0);
        #else
        clEnqueueBarrier(m_queue);
        #endif
    }

    #if defined(CL_VERSION_1_2) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Enqueues a barrier in the queue after \p events.
    ///
    /// \opencl_version_warning{1,2}
    void enqueue_barrier(const wait_list &events)
    {
        BOOST_ASSERT(m_queue != 0);

        clEnqueueBarrierWithWaitList(
            m_queue, events.size(), events.get_event_ptr(), 0
        );
    }
    #endif // CL_VERSION_1_2

    /// Enqueues a marker in the queue and returns an event that can be
    /// used to track its progress.
    event enqueue_marker()
    {
        event event_;

        #ifdef CL_VERSION_1_2
        cl_int ret = clEnqueueMarkerWithWaitList(m_queue, 0, 0, &event_.get());
        #else
        cl_int ret = clEnqueueMarker(m_queue, &event_.get());
        #endif

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    #if defined(CL_VERSION_1_2) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Enqueues a marker after \p events in the queue and returns an
    /// event that can be used to track its progress.
    ///
    /// \opencl_version_warning{1,2}
    event enqueue_marker(const wait_list &events)
    {
        event event_;

        cl_int ret = clEnqueueMarkerWithWaitList(
            m_queue, events.size(), events.get_event_ptr(), &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }
    #endif // CL_VERSION_1_2

    #if defined(CL_VERSION_2_0) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
    /// Enqueues a command to copy \p size bytes of data from \p src_ptr to
    /// \p dst_ptr.
    ///
    /// \opencl_version_warning{2,0}
    ///
    /// \see_opencl2_ref{clEnqueueSVMMemcpy}
    void enqueue_svm_memcpy(void *dst_ptr,
                            const void *src_ptr,
                            size_t size,
                            const wait_list &events = wait_list())
    {
        cl_int ret = clEnqueueSVMMemcpy(
            m_queue,
            CL_TRUE,
            dst_ptr,
            src_ptr,
            size,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    /// Enqueues a command to copy \p size bytes of data from \p src_ptr to
    /// \p dst_ptr. The operation is performed asynchronously.
    ///
    /// \opencl_version_warning{2,0}
    ///
    /// \see_opencl2_ref{clEnqueueSVMMemcpy}
    event enqueue_svm_memcpy_async(void *dst_ptr,
                                   const void *src_ptr,
                                   size_t size,
                                   const wait_list &events = wait_list())
    {
        event event_;

        cl_int ret = clEnqueueSVMMemcpy(
            m_queue,
            CL_FALSE,
            dst_ptr,
            src_ptr,
            size,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to fill \p size bytes of data at \p svm_ptr with
    /// \p pattern.
    ///
    /// \opencl_version_warning{2,0}
    ///
    /// \see_opencl2_ref{clEnqueueSVMMemFill}
    event enqueue_svm_fill(void *svm_ptr,
                           const void *pattern,
                           size_t pattern_size,
                           size_t size,
                           const wait_list &events = wait_list())

    {
        event event_;

        cl_int ret = clEnqueueSVMMemFill(
            m_queue,
            svm_ptr,
            pattern,
            pattern_size,
            size,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to free \p svm_ptr.
    ///
    /// \opencl_version_warning{2,0}
    ///
    /// \see_opencl2_ref{clEnqueueSVMFree}
    ///
    /// \see svm_free()
    event enqueue_svm_free(void *svm_ptr,
                           const wait_list &events = wait_list())
    {
        event event_;

        cl_int ret = clEnqueueSVMFree(
            m_queue,
            1,
            &svm_ptr,
            0,
            0,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }

    /// Enqueues a command to map \p svm_ptr to the host memory space.
    ///
    /// \opencl_version_warning{2,0}
    ///
    /// \see_opencl2_ref{clEnqueueSVMMap}
    void enqueue_svm_map(void *svm_ptr,
                         size_t size,
                         cl_map_flags flags,
                         const wait_list &events = wait_list())
    {
        cl_int ret = clEnqueueSVMMap(
            m_queue,
            CL_TRUE,
            flags,
            svm_ptr,
            size,
            events.size(),
            events.get_event_ptr(),
            0
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }
    }

    /// Enqueues a command to unmap \p svm_ptr from the host memory space.
    ///
    /// \opencl_version_warning{2,0}
    ///
    /// \see_opencl2_ref{clEnqueueSVMUnmap}
    event enqueue_svm_unmap(void *svm_ptr,
                            const wait_list &events = wait_list())
    {
        event event_;

        cl_int ret = clEnqueueSVMUnmap(
            m_queue,
            svm_ptr,
            events.size(),
            events.get_event_ptr(),
            &event_.get()
        );

        if(ret != CL_SUCCESS){
            BOOST_THROW_EXCEPTION(opencl_error(ret));
        }

        return event_;
    }
    #endif // CL_VERSION_2_0

    /// Returns \c true if the command queue is the same at \p other.
    bool operator==(const command_queue &other) const
    {
        return m_queue == other.m_queue;
    }

    /// Returns \c true if the command queue is different from \p other.
    bool operator!=(const command_queue &other) const
    {
        return m_queue != other.m_queue;
    }

    /// \internal_
    operator cl_command_queue() const
    {
        return m_queue;
    }

    /// \internal_
    bool check_device_version(int major, int minor) const
    {
        return get_device().check_version(major, minor);
    }

private:
    cl_command_queue m_queue;
};

inline buffer buffer::clone(command_queue &queue) const
{
    buffer copy(this->get_context(), this->size());
    queue.enqueue_copy_buffer(*this, copy, 0, 0, this->size());
    return copy;
}

inline image2d image2d::clone(command_queue &queue) const
{
    image2d copy(
        get_context(), get_memory_flags(), get_format(), width(), height(), 0, 0
    );

    size_t origin[2] = { 0, 0 };
    size_t region[2] = { this->width(), this->height() };

    queue.enqueue_copy_image(*this, copy, origin, origin, region);

    return copy;
}

/// \internal_ define get_info() specializations for command_queue
BOOST_COMPUTE_DETAIL_DEFINE_GET_INFO_SPECIALIZATIONS(command_queue,
    ((cl_context, CL_QUEUE_CONTEXT))
    ((cl_device_id, CL_QUEUE_DEVICE))
    ((uint_, CL_QUEUE_REFERENCE_COUNT))
    ((cl_command_queue_properties, CL_QUEUE_PROPERTIES))
)

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_COMMAND_QUEUE_H
