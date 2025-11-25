//
// Created by bilibili on 2025/11/24.
//

#ifndef VKNDKEXAMPLE_NATIVESTREAMBUF_H
#define VKNDKEXAMPLE_NATIVESTREAMBUF_H
#include <streambuf>
#include <ostream>
#include <string>
#include <mutex>
#include <vector>
#include <hilog/log.h>

class DeviceBuf : public std::streambuf {
public:
    // buffer capacity for accumulating characters before flushing to device
    explicit DeviceBuf(std::size_t cap = 4096) :
        buffer_cap_(cap)
    {
        buffer_.reserve(buffer_cap_);
    }

    ~DeviceBuf() override {
        sync(); // 确保销毁时 flush
    }

protected:
    // xsputn: 批量写入（效率更高），当上层有连续字符时会调用
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::lock_guard<std::mutex> lk(mtx_);
        const char* ptr = s;
        std::streamsize remaining = n;
        while (remaining > 0) {
            // 如果 we're not buffering, we can attempt to find newline to flush in chunks
            // Append up to capacity or until newline appears
            std::size_t space = buffer_cap_ - buffer_.size();
            if (space == 0) {
                // buffer full -> flush it first
                flush_buffer_unlocked();
                space = buffer_cap_;
            }
            std::size_t to_append = static_cast<std::size_t>(std::min<std::streamsize>(space, remaining));
            buffer_.append(ptr, to_append);
            // check if there's a newline inside the appended chunk -> flush up to last newline
            auto pos_newline = buffer_.find_last_of('\n');
            if (pos_newline != std::string::npos) {
                // send through device up to and including that newline
                device_write(buffer_.data(), pos_newline + 1);
                // keep the remainder (after the newline) in buffer
                std::string remainder = buffer_.substr(pos_newline + 1);
                buffer_.swap(remainder);
            }
            ptr += to_append;
            remaining -= to_append;
        }
        return n;
    }

    // overflow: 处理单字符写入（或当内部缓冲满/需要写单字符时）
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof()))
            return traits_type::not_eof(ch);

        char c = traits_type::to_char_type(ch);
        std::lock_guard<std::mutex> lk(mtx_);
        buffer_.push_back(c);
        if (c == '\n' || buffer_.size() >= buffer_cap_) {
            flush_buffer_unlocked();
        }
        return ch;
    }

    // sync: flush buffer to device (called by flush operations)
    int sync() override {
        std::lock_guard<std::mutex> lk(mtx_);
        flush_buffer_unlocked();
        return 0; // 0 表示成功，非 0 表示失败
    }

private:
    void flush_buffer_unlocked() {
        if (!buffer_.empty()) {
            device_write(buffer_.data(), buffer_.size());
            buffer_.clear();
        }
    }

    // 实际写入设备的函数，这里使用 HarmonyOS 的 hilog
    void device_write(const char* data, std::size_t len) {
        // 将数据以字符串形式输出到 hilog
        // 注意：hilog 的单次输出有长度限制（通常 ~1024 字节）
        // 如果数据很长，需要分段输出
        constexpr std::size_t MAX_LOG_LEN = 1024;
        
        std::size_t offset = 0;
        while (offset < len) {
            std::size_t chunk_size = std::min(MAX_LOG_LEN, len - offset);
            // 创建临时字符串确保有 null 终止符
            std::string msg(data + offset, chunk_size);
            // 移除末尾的换行符，因为 OH_LOG_Print 会自动添加
            if (!msg.empty() && msg.back() == '\n') {
                msg.pop_back();
            }
            // 输出到 hilog (Info 级别)
            OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, LOG_TAG, "%{public}s", msg.c_str());
            offset += chunk_size;
        }
    }

    std::string buffer_;
    std::size_t buffer_cap_;
    std::mutex mtx_;
};

#endif //VKNDKEXAMPLE_NATIVESTREAMBUF_H