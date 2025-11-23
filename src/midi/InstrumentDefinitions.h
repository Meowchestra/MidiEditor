/*
 * MidiEditor
 *
 * Instrument Definitions Manager
 */

#ifndef INSTRUMENTDEFINITIONS_H_
#define INSTRUMENTDEFINITIONS_H_

#include <QString>
#include <QStringList>
#include <QMap>

#include <QSettings>

class InstrumentDefinitions {
public:
    static InstrumentDefinitions* instance();
    static void cleanup();

    /**
     * @brief Loads definitions from a file
     * @param filename Path to the definition file
     * @return True if successful
     */
    bool load(const QString& filename);

    /**
     * @brief Returns a list of available instrument/bank names in the loaded file
     */
    QStringList instruments() const;

    /**
     * @brief Selects the current instrument/bank to use
     */
    void selectInstrument(const QString& name);

    /**
     * @brief Returns the currently selected instrument name
     */
    QString currentInstrument() const;

    /**
     * @brief Returns the currently loaded file path
     */
    QString currentFile() const;

    /**
     * @brief Gets the name for a program number (0-127)
     * @param program Program number
     * @return The instrument name, or empty string if not defined
     */
    QString instrumentName(int program) const;

    /**
     * @brief Sets a custom name for a program number
     * @param program Program number (0-127)
     * @param name The name to set
     */
    void setInstrumentName(int program, const QString& name);
    
    /**
     * @brief Returns the map of program numbers to names for the current instrument
     */
    QMap<int, QString> instrumentNames() const;
    
    /**
     * @brief Clears all loaded definitions and overrides
     */
    void clear();

    /**
     * @brief Load overrides from settings
     */
    void loadOverrides(QSettings* settings);

    /**
     * @brief Save overrides to settings
     */
    void saveOverrides(QSettings* settings);

    /**
     * @brief Gets the GM instrument name for a program number
     * @param program Program number (0-127)
     * @return The GM instrument name
     */
    static QString gmInstrumentName(int program);

private:
    InstrumentDefinitions();
    ~InstrumentDefinitions();

    // Helper to recursively resolve inheritance
    void resolveInheritance(const QString& section, QList<QString>& visited);

    static InstrumentDefinitions* _instance;

    QString _currentFile;
    QString _currentInstrument;
    
    // Map of Section Name -> (Program Number -> Instrument Name)
    QMap<QString, QMap<int, QString> > _definitions;
    // Map of Section Name -> Base Section Name
    QMap<QString, QString> _inheritance;
    // User overrides: Section Name -> (Program Number -> Name)
    QMap<QString, QMap<int, QString> > _overrides;
};

#endif // INSTRUMENTDEFINITIONS_H_
