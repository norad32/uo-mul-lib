#pragma once

#include "gui_model/model/assets/asset_window_contracts.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace uo_mule::gui::assets
{

inline constexpr std::uint32_t currentAssetStateSchemaVersion = 1;

struct AssetStateRecord
{
    std::string profileIdentity;
    std::string assetWindowIdentity;
    AssetSourceContext context;
    AssetWindowState state;
};

[[nodiscard]] std::filesystem::path assetStatePath();
[[nodiscard]] std::vector<AssetStateRecord> loadAssetState(const std::filesystem::path & path);
void saveAssetState(const std::filesystem::path & path, const std::vector<AssetStateRecord> & records);
void resetAssetState(std::vector<AssetStateRecord> & records) noexcept;
[[nodiscard]] const AssetStateRecord * findAssetState(const std::vector<AssetStateRecord> & records,
                                                      const std::string & profileIdentity,
                                                      const std::string & assetWindowIdentity,
                                                      const AssetSourceContext & context) noexcept;
void restoreAssetState(const AssetStateRecord & record, AssetWindowState & state) noexcept;
[[nodiscard]] AssetStateRecord captureAssetState(const std::string & profileIdentity,
                                                 const std::string & assetWindowIdentity,
                                                 const AssetSourceContext & context,
                                                 const AssetWindowState & state);

} // namespace uo_mule::gui::assets
