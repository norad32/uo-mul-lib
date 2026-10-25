#include "gui_model/documents/document_contracts.h"
#include "gui_model/model/assets/asset_export_contracts.h"
#include "gui_model/model/assets/asset_window_contracts.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("assetExportRequest_Should_DefaultToSingleBmpFileExport")
{
    const uo_mule::gui::assets::AssetExportRequest request;

    REQUIRE(request.format == uo_mule::gui::assets::AssetExportFormat::bmp);
    REQUIRE(request.destination == uo_mule::gui::assets::AssetExportDestination::file);
    REQUIRE_FALSE(request.overwrite);
    REQUIRE(request.items.empty());
}

TEST_CASE("assetExportRequest_Should_RepresentPngDirectoryExport")
{
    uo_mule::gui::assets::AssetExportRequest request{
        .kind = uo_mule::gui::DocumentKind::texmapPair,
        .source =
            {
                .kind = uo_mule::gui::DocumentKind::texmapPair,
                .contextKind = uo_mule::gui::assets::AssetSourceContext::Kind::document,
                .source = {},
                .contextId = "document.texmaps.1",
            },
        .items = {{.value = "3"}},
        .format = uo_mule::gui::assets::AssetExportFormat::png,
        .destination = uo_mule::gui::assets::AssetExportDestination::directory,
        .outputPath = "textures",
        .overwrite = true,
    };

    REQUIRE(request.kind == uo_mule::gui::DocumentKind::texmapPair);
    REQUIRE(request.source.contextKind == uo_mule::gui::assets::AssetSourceContext::Kind::document);
    REQUIRE(request.items.size() == 1U);
    REQUIRE(request.format == uo_mule::gui::assets::AssetExportFormat::png);
    REQUIRE(request.destination == uo_mule::gui::assets::AssetExportDestination::directory);
    REQUIRE(request.outputPath == "textures");
    REQUIRE(request.overwrite);
}
