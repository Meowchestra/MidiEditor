function Controller()
{
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    var widget = gui.currentPageWidget();
    if (widget != null) {
        var targetDir = widget.TargetDirectoryLineEdit.text;
        // Normalize path separators
        var normalizedDir = targetDir.replace(/\\/g, "/");
        var maintenanceTool = normalizedDir + "/maintenancetool.exe";

        console.log("Checking for maintenance tool at: " + maintenanceTool);

        if (installer.fileExists(maintenanceTool)) {
            var reply = QMessageBox.question(widget, "Existing Installation Detected",
                "An existing installation was found in " + targetDir + ".\n" +
                "Do you want to uninstall it before proceeding?\n" +
                "This will launch the maintenance tool for the existing installation.",
                QMessageBox.Yes | QMessageBox.No);

            if (reply == QMessageBox.Yes) {
                console.log("Launching existing maintenance tool...");

                // Launch the old MaintenanceTool GUI so the user can
                // see and confirm the uninstall. We use the GUI rather
                // than silent "purge" for compatibility with older QTIFW
                // versions that may not support the purge command.
                installer.execute(maintenanceTool);

                console.log("Maintenance tool exited. Cleaning up leftovers...");

                // Even though installer.execute() waited for the old
                // MaintenanceTool to exit, Windows may not have fully
                // released file locks yet. Use the same detached process
                // approach as installscript.qs to ensure cleanup succeeds.
                var nativeDir = targetDir.replace(/\//g, "\\");
                installer.execute("powershell.exe", [
                    "-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden", "-Command",
                    "Start-Process cmd.exe -ArgumentList '/c ping -n 2 127.0.0.1 >nul & rmdir /s /q \"" + nativeDir + "\"' -WindowStyle Hidden"
                ]);

                console.log("Cleanup scheduled.");
            }
        } else {
            // No MaintenanceTool found, but the directory might still have
            // leftover files from a previous botched uninstall.
            if (installer.fileExists(targetDir)) {
                var dirReply = QMessageBox.question(widget, "Directory Not Empty",
                    "The target directory " + targetDir + " already exists\n" +
                    "but does not contain a valid installation.\n\n" +
                    "Do you want to remove it before proceeding?\n" +
                    "(This will delete all files in the directory.)",
                    QMessageBox.Yes | QMessageBox.No);

                if (dirReply == QMessageBox.Yes) {
                    console.log("Removing leftover directory: " + targetDir);
                    var nativeDir = targetDir.replace(/\//g, "\\");
                    installer.execute("cmd", ["/c",
                        "rmdir /s /q \"" + nativeDir + "\""]);
                    console.log("Directory removed.");
                }
            }
        }
    }
}
