#include "MmlConverter.h"
#include "ThreeMleParser.h"
#include "MmlLexer.h"
#include "MmlParser.h"
#include <QString>
#include <QRegularExpression>

namespace MML {

std::vector<MmlEvent> MmlConverter::ConvertAuto(const std::string& content, MmlDialect dialect) {
    QString qContent = QString::fromStdString(content);
    QString trimmed = qContent.trimmed();

    bool isIni = trimmed.contains("[Settings]") ||
                 trimmed.contains(QRegularExpression("^\\[Channel\\d+\\]", QRegularExpression::MultilineOption));

    bool isProjectFile = isIni ||
                         trimmed.startsWith("<") ||
                         trimmed.contains("MML@", Qt::CaseInsensitive) ||
                         trimmed.contains("<Track", Qt::CaseInsensitive);

    if (isProjectFile) {
        ThreeMleFile project = ThreeMleParser::Parse(content, dialect);
        return ConvertMML(BuildCombinedMml(project));
    }

    return ConvertMML(content);
}

std::vector<MmlEvent> MmlConverter::ConvertMML(const std::string& mml) {
    MmlLexer lexer(mml);
    auto tokens = lexer.Tokenize();
    
    MmlParser parser;
    return parser.Parse(tokens);
}

std::string MmlConverter::BuildCombinedMml(const ThreeMleFile& project) {
    QString sb = QString("T%1 ").arg(project.Tempo);
    for (const auto& track : project.Tracks) {
        int ch = track.Channel > 0 ? track.Channel : 1;
        
        QString mml = QString::fromStdString(track.MML);
        mml.replace(QRegularExpression("(?i)\\bt\\d+\\s*"), "");
        mml = mml.trimmed();
        
        if (!mml.isEmpty()) {
            sb += QString("CH%1 %2 ").arg(ch).arg(mml);
        }
    }
    return sb.trimmed().toStdString();
}

} // namespace MML
