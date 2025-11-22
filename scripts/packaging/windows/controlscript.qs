function Controller()
{
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    var widget = gui.currentPageWidget();
    if (widget != null) {
        var targetDir = widget.TargetDirectoryLineEdit.text;
        // Normalize path separators just in case
        var maintenanceTool = targetDir.replace(/\\/g, "/") + "/maintenancetool.exe";
        
        console.log("Checking for maintenance tool at: " + maintenanceTool);

        if (installer.fileExists(maintenanceTool)) {
            var reply = QMessageBox.question(widget, "Existing Installation Detected",
                "An existing installation was found in " + targetDir + ".\n" +
                "Do you want to uninstall it before proceeding?\n" +
                "This will launch the maintenance tool for the existing installation.",
                QMessageBox.Yes | QMessageBox.No);
            
            if (reply == QMessageBox.Yes) {
                // Execute the maintenance tool.
                // We rely on the user to complete the uninstallation.
                installer.execute(maintenanceTool);
            }
        }
    }
}
