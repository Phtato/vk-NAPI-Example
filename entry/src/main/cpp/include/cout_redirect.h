#ifndef VKNDKEXAMPLE_COUT_REDIRECT_H
#define VKNDKEXAMPLE_COUT_REDIRECT_H

#include <ostream>
#include "nativeStreamBuf.h" // 依赖已有的 DeviceBuf

// RAII helper: 将一个 std::ostream（默认 std::cout）重定向到一个 DeviceBuf。
// 构造时替换 rdbuf，析构时恢复原 rdbuf 并 flush。
// 注意：std::cout 重定向是全局行为，影响所有线程；请在可控的作用域内使用。
class CoutRedirect {
public:
    // 构造：重定向到一个内部的 DeviceBuf（使用指定容量）
    explicit CoutRedirect(std::ostream &os = std::cout, std::size_t buf_cap = 4096)
        : os_(os), dev_buf_(buf_cap), old_buf_(os_.rdbuf(&dev_buf_))
    {
        // os_ 的 rdbuf 现在指向 dev_buf_
    }

    // 禁止拷贝、移动（简单、安全）
    CoutRedirect(const CoutRedirect&) = delete;
    CoutRedirect& operator=(const CoutRedirect&) = delete;
    CoutRedirect(CoutRedirect&&) = delete;
    CoutRedirect& operator=(CoutRedirect&&) = delete;

    ~CoutRedirect() {
        // 如果 old_buf_ 非空，说明我们负责恢复
        if (old_buf_) {
            // 先 flush 当前流（会触发 DeviceBuf::sync）
            os_.flush();
            // 恢复原来的 rdbuf 指针
            os_.rdbuf(old_buf_);
            // dev_buf_ 会在此对象被销毁时析构（成员析构顺序）
        }
    }

private:
    std::ostream &os_;
    DeviceBuf dev_buf_;
    std::streambuf* old_buf_;
};

#endif // VKNDKEXAMPLE_COUT_REDIRECT_H
