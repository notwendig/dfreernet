#pragma once
#include "database.hpp"
namespace rnet {
// Immutable, verified project resources embedded in every native helper.
const Bytes &bundled_catalog_bytes();
ParameterCatalog bundled_catalog();
ParameterCatalog read_catalog_database(const std::filesystem::path &path);
void write_catalog_database(const Bytes &plain, const std::filesystem::path &path);
const Bytes &bundled_default_document();
std::vector<MenuItem> bundled_menus();
std::vector<TextResource> bundled_text_resources();
namespace assets { Bytes database(); Bytes catalog(); Bytes document(); Bytes seed(); Bytes menu(); Bytes texts(); }
}
