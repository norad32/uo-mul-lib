#pragma once

#include "core/app_metadata.h"

namespace im_cpp::core
{

[[nodiscard]] constexpr std::string_view applicationName() noexcept
{
    return metadata::applicationName;
}

[[nodiscard]] constexpr std::string_view applicationId() noexcept
{
    return metadata::applicationId;
}

[[nodiscard]] constexpr std::string_view applicationVersion() noexcept
{
    return metadata::applicationVersion;
}

[[nodiscard]] constexpr std::string_view applicationDescription() noexcept
{
    return metadata::applicationDescription;
}

[[nodiscard]] constexpr std::string_view applicationHomepage() noexcept
{
    return metadata::applicationHomepage;
}

[[nodiscard]] constexpr std::string_view applicationLicense() noexcept
{
    return metadata::applicationLicense;
}

[[nodiscard]] constexpr std::string_view applicationAuthor() noexcept
{
    return metadata::applicationAuthor;
}

} // namespace im_cpp::core
