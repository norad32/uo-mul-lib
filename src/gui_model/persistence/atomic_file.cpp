#include "gui_model/persistence/atomic_file.h"
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

namespace uo_mule::gui
{
namespace
{

std::atomic<std::uint64_t> temporaryFileCounter = 0;
constexpr std::uint32_t maximumTemporaryFileAttempts = 128;

[[nodiscard]] int createTemporaryFile(const std::filesystem::path & path, std::string & temporaryPath)
{
    const auto basePath = path.string();
    for (std::uint32_t attempt = 0; attempt < maximumTemporaryFileAttempts; ++attempt)
    {
        temporaryPath = basePath + ".tmp." + std::to_string(getpid()) + "." + std::to_string(temporaryFileCounter.fetch_add(1));
        const int descriptor = open(temporaryPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (descriptor != -1 || errno != EEXIST)
        {
            if (descriptor == -1)
            {
                throw std::system_error(errno, std::generic_category(), "Cannot create temporary " + path.string());
            }
            return descriptor;
        }
    }

    throw std::system_error(EEXIST, std::generic_category(), "Cannot create temporary " + path.string());
}

} // namespace

void writeFileAtomically(const std::filesystem::path & path, std::string_view contents, std::string_view description)
{
    if (path.empty())
    {
        return;
    }

    std::error_code error;
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            throw std::system_error(error, "Cannot create directory for " + std::string(description));
        }
    }

    std::string temporaryPath;
    int descriptor = -1;
    bool temporaryOwned = false;
    try
    {
        descriptor = createTemporaryFile(path, temporaryPath);
        temporaryOwned = true;
        std::string_view remaining = contents;
        while (!remaining.empty())
        {
            const ssize_t written = write(descriptor, remaining.data(), remaining.size());
            if (written == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(), "Cannot write " + std::string(description));
            }
            if (written == 0)
            {
                throw std::system_error(EIO, std::generic_category(), "Cannot write " + std::string(description));
            }
            remaining.remove_prefix(static_cast<std::size_t>(written));
        }

        if (fsync(descriptor) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "Cannot sync " + std::string(description));
        }
        if (close(descriptor) == -1)
        {
            descriptor = -1;
            throw std::system_error(errno, std::generic_category(), "Cannot close " + std::string(description));
        }
        descriptor = -1;

        if (rename(temporaryPath.c_str(), path.c_str()) == -1)
        {
            throw std::system_error(errno, std::generic_category(), "Cannot replace " + std::string(description));
        }
        temporaryOwned = false;
    }
    catch (...)
    {
        if (descriptor != -1)
        {
            close(descriptor);
        }
        if (temporaryOwned)
        {
            unlink(temporaryPath.c_str());
        }
        throw;
    }
}

} // namespace uo_mule::gui
