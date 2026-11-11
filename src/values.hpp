#pragma once
#include "database.hpp"
#include "document.hpp"
#include "access.hpp"
namespace rnet {
struct DocumentValue {
    std::size_t file=0,offset=0;
    std::uint32_t parameter=0,width=0;
    unsigned group=0,profile=0,section=0,element=0;
    bool numeric=false;
    std::int64_t number=0;
    std::u16string text;
};
// Validates all modern payload checksums and known file versions before mapping
// stored bytes. Numeric values are raw storage values, without UI conversions.
// Supplying warnings permits unknown versions to remain opaque; every skipped
// file is reported. Known versions with corrupt content are always rejected.
std::vector<DocumentValue> read_values(const RnetDocument&,const ParameterCatalog&,std::vector<std::string>* warnings=nullptr);
// Replay every changed mapped value under the chosen access level; reject any
// other byte changes, including unknown versions and document metadata.
std::vector<std::size_t> controller_write_plan(const RnetDocument&, const RnetDocument&, const ParameterCatalog&, AccessLevel);
struct ParameterChoice {std::int64_t value=0;std::u16string label;};
std::vector<ParameterChoice> parameter_choices(const ParameterDefinition&, const DocumentValue&, const ParameterCatalog&);
bool supports_direct_edit(const ParameterDefinition&, const DocumentValue&, const ParameterCatalog* catalog=nullptr);
// Encodes and validates one value without changing a document (also used by UI editors).
Bytes encode_parameter_value(const ParameterDefinition&, const DocumentValue&, const std::u16string&, const ParameterCatalog&);
// Edits verified direct numeric/text storage only; unknown file versions and
// conversion-based parameter types remain unchanged. Updates the group checksum.
bool edit_value(RnetDocument&,const ParameterCatalog&,const DocumentValue&,const std::u16string&,AccessLevel level=AccessLevel::OEM);
}
