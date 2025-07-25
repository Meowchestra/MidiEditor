#ifndef TOOLBARACTIONINFO_H
#define TOOLBARACTIONINFO_H

#include <QString>

class QAction;

struct ToolbarActionInfo {
    QString id;
    QString name;
    QString iconPath;
    QAction* action;
    bool enabled;
    bool essential; // Cannot be disabled (New, Open, Save, Undo, Redo)
    QString category;
};

#endif // TOOLBARACTIONINFO_H
