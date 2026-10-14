#include "core/gui_log_sink.h"
#include <cstddef>
#include <mutex>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <utility>
#include <vector>

namespace im_cpp::core
{

GuiLogSink::GuiLogSink(std::size_t capacity) : capacity_(capacity)
{
    entries_.reserve(capacity);
}

std::vector<GuiLogEntry> GuiLogSink::drain()
{
    const std::scoped_lock lock(mutex_);
    std::vector<GuiLogEntry> entries;
    entries.swap(entries_);
    return entries;
}

void GuiLogSink::clear()
{
    const std::scoped_lock lock(mutex_);
    entries_.clear();
}

void GuiLogSink::sink_it_(const spdlog::details::log_msg & message)
{
    spdlog::memory_buf_t formattedMessage;
    formatter_->format(message, formattedMessage);
    GuiLogEntry entry{.level = message.level, .formattedMessage = SPDLOG_BUF_TO_STRING(formattedMessage)};
    if (capacity_ == 0)
    {
        return;
    }

    if (entries_.size() == capacity_)
    {
        entries_.erase(entries_.begin());
    }
    entries_.push_back(std::move(entry));
}

void GuiLogSink::flush_()
{
}

} // namespace im_cpp::core
