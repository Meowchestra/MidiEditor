#pragma once
#include "MmlModels.h"
#include <string>
#include <vector>

namespace MML {

class ThreeMleParser {
public:
    static ThreeMleFile Parse(const std::string& content, MmlDialect dialect = MmlDialect::Auto);

private:
    static bool IsIniFormat(const std::string& s);
    static bool IsXmlFormat(const std::string& s);
    static bool IsMabiClipboard(const std::string& s);
    static bool LooksMabinogi(const std::string& s);

    static ThreeMleFile ParseIni(const std::string& content);
    static ThreeMleFile ParseXml(const std::string& xml);
    static ThreeMleFile ParseMabiClipboard(const std::string& text, bool isMabi);
    static ThreeMleFile ParseRawMultiTrack(const std::string& text);

    static std::string NormalizeMabiMML(const std::string& MML);
    static std::string NormalizeGenericMML(const std::string& MML);
    static std::string NormalizeXmlMML(const std::string& raw);
    static void ExtractTempoFromTracks(ThreeMleFile& file);
};

} // namespace MML
