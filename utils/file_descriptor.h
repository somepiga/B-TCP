#pragma once

#include "buffer.h"
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

// 文件描述符的引用计数句柄
class FileDescriptor {
    // FDWrapper: 内核文件描述符的句柄
    // FileDescriptor 含有指向 FDWrapper 的 std::shared_ptr
    class FDWrapper {
       public:
        int fd_;            // 文件描述符，由内核返回
        bool eof_ = false;  // 文件描述符是否已经到达了文件末尾
        bool closed_ = false;        // 文件描述符是否已经关闭
        bool non_blocking_ = false;  // 文件描述符是否是非阻塞的
        unsigned read_count_ = 0;    // 文件描述符被读取的次数
        unsigned write_count_ = 0;   // 文件描述符被写入的次数

        // 构造函数，接受一个文件描述符号作为参数
        explicit FDWrapper(int fd);

        ~FDWrapper();

        // 调用os的close(2)函数，来关闭文件描述符fd_
        void close();

        template <typename T>
        T CheckSystemCall(std::string_view s_attempt, T return_value) const;

        // FDWrapper 不能拷贝或移动
        FDWrapper(const FDWrapper& other) = delete;
        FDWrapper& operator=(const FDWrapper& other) = delete;
        FDWrapper(FDWrapper&& other) = delete;
        FDWrapper& operator=(FDWrapper&& other) = delete;
    };

    // 共享 FDWrapper 的引用计数句柄
    std::shared_ptr<FDWrapper> internal_fd_;

    // 用于复制 FileDescriptor 的私有构造函数(增加引用计数)
    explicit FileDescriptor(std::shared_ptr<FDWrapper> other_shared_ptr);

   protected:
    // 为read()分配的buffer
    static constexpr size_t kReadBufferSize = 16384;

    void set_eof() { internal_fd_->eof_ = true; }

    // 增加读取计数
    void register_read() { ++internal_fd_->read_count_; }

    // 增加写入计数
    void register_write() { ++internal_fd_->write_count_; }

    template <typename T>
    T CheckSystemCall(std::string_view s_attempt, T return_value) const;

   public:
    explicit FileDescriptor(int fd);

    // 释放std::shared_ptr; 当引用次数为0时，调用close()
    ~FileDescriptor() = default;

    // 读 buffer
    void read(std::string& buffer);
    void read(std::vector<std::string>& buffers);

    // 写 buffer; 返回写入字节数
    size_t write(std::string_view buffer);
    size_t write(const std::vector<std::string_view>& buffers);
    size_t write(const std::vector<Buffer>& buffers);

    // 关闭底层文件描述符
    void close() { internal_fd_->close(); }

    // 显式复制 FileDescriptor, 并增加 FDWrapper 引用计数
    FileDescriptor duplicate() const;

    // 设置阻塞(true)或非阻塞(false)
    void set_blocking(bool blocking);

    // 文件大小
    off_t size() const;

    // FDWrapper 访问器
    int fd_num() const { return internal_fd_->fd_; }
    bool eof() const { return internal_fd_->eof_; }        // EOF flag 状态
    bool closed() const { return internal_fd_->closed_; }  // closed flag 状态
    unsigned int read_count() const { return internal_fd_->read_count_; }
    unsigned int write_count() const { return internal_fd_->write_count_; }

    // Copy/move constructor/assignment operators
    // FileDescriptor 可以移动，但不能隐式复制( 参见duplicate() )
    FileDescriptor(const FileDescriptor& other) = delete;
    FileDescriptor& operator=(const FileDescriptor& other) = delete;
    FileDescriptor(FileDescriptor&& other) = default;
    FileDescriptor& operator=(FileDescriptor&& other) = default;
};