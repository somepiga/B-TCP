#pragma once

#include "file_descriptor.h"
#include "lossy_fd_adapter.h"
#include "socket.h"
#include "tcp_config.h"
#include "tcp_segment.h"

#include <optional>
#include <utility>

//! \brief 文件描述符适配器的基本功能
//! \details See TCPOverIPv4OverTunFdAdapter for more information.
class FdAdapterBase {
   private:
    FdAdapterConfig _cfg{};  //!< 配置值
    bool _listen = false;    //!< 连接的TCP FSM是否处于监听状态？

   protected:
    FdAdapterConfig& config_mutable() { return _cfg; }

   public:
    //! \brief Set the listening flag
    //! \param[in] l is the new value for the flag
    void set_listening(const bool l) { _listen = l; }

    //! \brief Get the listening flag
    //! \returns whether the FdAdapter is listening for a new connection
    bool listening() const { return _listen; }

    //! \brief Get the current configuration
    //! \returns a const reference
    const FdAdapterConfig& config() const { return _cfg; }

    //! \brief Get the current configuration (mutable)
    //! \returns a mutable reference
    FdAdapterConfig& config_mut() { return _cfg; }

    //! Called periodically when time elapses
    void tick(const size_t unused [[maybe_unused]]) {}
};
