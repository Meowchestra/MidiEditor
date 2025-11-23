/*
 * MidiEditor
 *
 * Instrument Definitions Manager
 */

#include "InstrumentDefinitions.h"
#include "MidiFile.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

InstrumentDefinitions* InstrumentDefinitions::_instance = 0;

InstrumentDefinitions::InstrumentDefinitions() {
}

InstrumentDefinitions::~InstrumentDefinitions() {
}

InstrumentDefinitions* InstrumentDefinitions::instance() {
    if (!_instance) {
        _instance = new InstrumentDefinitions();
    }
    return _instance;
}

void InstrumentDefinitions::cleanup() {
    if (_instance) {
        delete _instance;
        _instance = 0;
    }
}

void InstrumentDefinitions::clear() {
    _definitions.clear();
    _overrides.clear();
    _inheritance.clear();
    _currentFile = "";
    _currentInstrument = "";
}

bool InstrumentDefinitions::load(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    _definitions.clear();
    _inheritance.clear();
    _currentFile = filename;
    
    // Keep the current instrument if it still exists, otherwise reset
    QString oldInstrument = _currentInstrument;
    _currentInstrument = "";

    QTextStream in(&file);
    QString currentSection = "";
    
    // Matches "10=Flute" or "10 = Flute"
    QRegularExpression entryRegex("^\\s*(\\d+)\\s*=\\s*(.+)$");
    
    // Matches "[Section Name]"
    QRegularExpression sectionRegex("^\\[(.+)\\]$");

    // Matches "BasedOn=Section Name"
    QRegularExpression basedOnRegex("^BasedOn=(.+)$");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(";") || line.startsWith(".")) {
            continue;
        }

        QRegularExpressionMatch sectionMatch = sectionRegex.match(line);
        if (sectionMatch.hasMatch()) {
            currentSection = sectionMatch.captured(1).trimmed();
            continue;
        }

        if (!currentSection.isEmpty()) {
            QRegularExpressionMatch entryMatch = entryRegex.match(line);
            if (entryMatch.hasMatch()) {
                bool ok;
                int program = entryMatch.captured(1).toInt(&ok);
                // MIDI programs are 0-127. Some files might use 1-128 or 0-127.
                // .ins files typically use 0-based indexing for patches.
                if (ok && program >= 0 && program < 128) {
                    _definitions[currentSection][program] = entryMatch.captured(2).trimmed();
                }
            } else {
                QRegularExpressionMatch basedOnMatch = basedOnRegex.match(line);
                if (basedOnMatch.hasMatch()) {
                    _inheritance[currentSection] = basedOnMatch.captured(1).trimmed();
                    // Ensure section exists in definitions so we can iterate it later
                    if (!_definitions.contains(currentSection)) {
                         _definitions[currentSection] = QMap<int, QString>();
                    }
                }
            }
        }
    }

    // Resolve inheritance
    QList<QString> visited;
    // Create a temporary list of keys to avoid issues if _definitions changes during iteration (though it shouldn't)
    QStringList sections = _definitions.keys(); 
    // Also include sections that are in _inheritance but might not have had any definitions
    // (though we added them above)
    
    foreach (const QString& section, sections) {
        visited.clear();
        resolveInheritance(section, visited);
    }
    
    if (_definitions.contains(oldInstrument)) {
        _currentInstrument = oldInstrument;
    } else if (!_definitions.isEmpty()) {
        // Select the first one by default if available
        _currentInstrument = _definitions.firstKey();
    }

    return true;
}

void InstrumentDefinitions::resolveInheritance(const QString& section, QList<QString>& visited) {
    if (visited.contains(section)) {
        return; // Cycle detected or already visited in this chain
    }
    visited.append(section);

    if (_inheritance.contains(section)) {
        QString base = _inheritance[section];
        
        // Recursively resolve base first
        resolveInheritance(base, visited);
        
        if (_definitions.contains(base)) {
            // Merge base into current
            // We want current definitions to override base definitions
            // So we start with base, and insert current
            QMap<int, QString> merged = _definitions[base];
            const QMap<int, QString>& current = _definitions[section];
            
            QMap<int, QString>::const_iterator it;
            for (it = current.constBegin(); it != current.constEnd(); ++it) {
                merged.insert(it.key(), it.value());
            }
            
            _definitions[section] = merged;
        }
    }
}

QStringList InstrumentDefinitions::instruments() const {
    return _definitions.keys();
}

void InstrumentDefinitions::selectInstrument(const QString& name) {
    if (_definitions.contains(name)) {
        _currentInstrument = name;
    }
}

QString InstrumentDefinitions::currentInstrument() const {
    return _currentInstrument;
}

QString InstrumentDefinitions::currentFile() const {
    return _currentFile;
}

void InstrumentDefinitions::setInstrumentName(int program, const QString& name) {
    // Allow empty instrument (defaults/overrides only)
    QString key = _currentInstrument;
    
    if (name.isEmpty()) {
        // If name is empty, remove override
        if (_overrides.contains(key)) {
            _overrides[key].remove(program);
        }
    } else {
        // Add or update override
        _overrides[key][program] = name;
    }
}

QMap<int, QString> InstrumentDefinitions::instrumentNames() const {
    QMap<int, QString> names;
    
    // First load base definitions
    if (!_currentInstrument.isEmpty() && _definitions.contains(_currentInstrument)) {
        names = _definitions[_currentInstrument];
    }
    
    // Then apply overrides
    // Use current instrument key (which might be empty string)
    QString key = _currentInstrument;
    if (_overrides.contains(key)) {
        QMapIterator<int, QString> it(_overrides[key]);
        while (it.hasNext()) {
            it.next();
            names.insert(it.key(), it.value());
        }
    }
    
    return names;
}

void InstrumentDefinitions::loadOverrides(QSettings* settings) {
    if (!settings) return;
    
    _overrides.clear();
    
    settings->beginGroup("InstrumentDefinitions/Overrides");
    QStringList instruments = settings->childGroups();
    foreach(QString section, instruments) {
        // Handle placeholder for custom/empty instrument
        QString instr = (section == "_UserCustom_") ? "" : section;
        
        settings->beginGroup(section);
        QStringList keys = settings->childKeys();
        foreach(QString key, keys) {
            bool ok;
            int program = key.toInt(&ok);
            if (ok) {
                _overrides[instr][program] = settings->value(key).toString();
            }
        }
        settings->endGroup();
    }
    settings->endGroup();
}

void InstrumentDefinitions::saveOverrides(QSettings* settings) {
    if (!settings) return;
    
    settings->beginGroup("InstrumentDefinitions/Overrides");
    settings->remove(""); // Clear previous overrides
    
    QMap<QString, QMap<int, QString> >::const_iterator it;
    for (it = _overrides.constBegin(); it != _overrides.constEnd(); ++it) {
        QString instr = it.key();
        // Use placeholder for empty instrument
        QString section = instr.isEmpty() ? "_UserCustom_" : instr;
        
        settings->beginGroup(section);
        
        QMap<int, QString> bank = it.value();
        QMap<int, QString>::const_iterator bankIt;
        for (bankIt = bank.constBegin(); bankIt != bank.constEnd(); ++bankIt) {
            settings->setValue(QString::number(bankIt.key()), bankIt.value());
        }
        
        settings->endGroup();
    }
    settings->endGroup();
}

QString InstrumentDefinitions::instrumentName(int program) const {
    // Check overrides first
    QString key = _currentInstrument;
    if (_overrides.contains(key) && _overrides[key].contains(program)) {
        return _overrides[key][program];
    }
    
    // Fallback to definitions
    if (!_currentInstrument.isEmpty() && _definitions.contains(_currentInstrument)) {
        const QMap<int, QString>& bank = _definitions.value(_currentInstrument);
        if (bank.contains(program)) {
            return bank.value(program);
        }
    }
    
    // Fallback to GM
    return gmInstrumentName(program);
}

QString InstrumentDefinitions::gmInstrumentName(int program) {
    return MidiFile::gmInstrumentName(program);
}
