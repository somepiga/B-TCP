#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "buffer/string_buffer.h"

/**
 * @brief 对文件描述符一些重要内容的包装，使其更简洁
 */
class FileDescriptor {
  // FDWrapper：内核文件描述符的句柄
  // FileDescriptor 对象包含 FDWrapper 的 std::shared_ptr
  class FDWrapper {
   public:
    int fd_;
    bool eof_ = false;
    bool closed_ = false;
    bool non_blocking_ = false;
    unsigned read_count_ = 0;
    unsigned write_count_ = 0;

    // 从内核返回的文件描述符编号构造
    explicit FDWrapper(int fd);
    // 析构时关闭文件描述符
    ~FDWrapper();
    // 调用 [close(2)]
    void close();

    template <typename T>
    T CheckSystemCall(std::string_view s_attempt, T return_value) const;

    // FDWrapper 无法复制或移动
    FDWrapper(const FDWrapper& other) = delete;
    FDWrapper& operator=(const FDWrapper& other) = delete;
    FDWrapper(FDWrapper&& other) = delete;
    FDWrapper& operator=(FDWrapper&& other) = delete;
  };

  // 共享 FDWrapper 的引用计数句柄
  std::shared_ptr<FDWrapper> internal_fd_;

  // 复制 FileDescriptor 的私有构造函数（增加引用计数）
  explicit FileDescriptor(std::shared_ptr<FDWrapper> other_shared_ptr);

 protected:
  // 为 read() 分配的缓冲区大小
  static constexpr size_t kReadBufferSize = 16384;

  void set_eof() { internal_fd_->eof_ = true; }
  void register_read() { ++internal_fd_->read_count_; }
  void register_write() { ++internal_fd_->write_count_; }

  template <typename T>
  T CheckSystemCall(std::string_view s_attempt, T return_value) const;

 public:
  // 从内核返回的文件描述符编号构造
  explicit FileDescriptor(int fd);

  // 释放 std::shared_ptr；当 FDWrapper 析构函数调用 close() 时，引用计数为零
  ~FileDescriptor() = default;

  // 读入 `buffer`
  void read(std::string& buffer);
  void read(std::vector<std::string>& buffers);

  // 尝试写入缓冲区
  // 返回写入的字节数
  size_t write(std::string_view buffer);
  size_t write(const std::vector<std::string_view>& buffers);
  size_t write(const std::vector<Buffer>& buffers);

  // 关闭文件描述符
  void close() { internal_fd_->close(); }

  // 显式复制 FileDescriptor，增加 FDWrapper 引用计数
  FileDescriptor duplicate() const;

  // 设置阻塞（true）或非阻塞（false）
  void set_blocking(bool blocking);

  // 文件大小
  off_t size() const;

  // FDWrapper 访问器
  int fd_num() const { return internal_fd_->fd_; }
  bool eof() const { return internal_fd_->eof_; }
  bool closed() const { return internal_fd_->closed_; }
  unsigned int read_count() const {
    return internal_fd_->read_count_;
  }  //!< 读取次数
  unsigned int write_count() const {
    return internal_fd_->write_count_;
  }  //!< 写入次数

  // 文件描述符可以移动，但不能隐式复制(参见 duplicate() )
  FileDescriptor(const FileDescriptor& other) = delete;  // 禁止拷贝构造
  FileDescriptor& operator=(const FileDescriptor& other) = delete;  // 禁止拷贝
  FileDescriptor(FileDescriptor&& other) = default;  // 允许移动构造
  FileDescriptor& operator=(FileDescriptor&& other) = default;  // 允许移动分配
};
