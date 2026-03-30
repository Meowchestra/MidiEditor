#pragma once
#include "MmlModels.h"
#include <string>
#include <vector>

namespace MML {

class MmlConverter {
public:
    static std::vector<MmlEvent> ConvertAuto(const std::string& content, MmlDialect dialect = MmlDialect::Auto);
    static std::vector<MmlEvent> ConvertMML(const std::string& mml);

private:
    static std::string BuildCombinedMml(const ThreeMleFile& project);
};

} // namespace MML
