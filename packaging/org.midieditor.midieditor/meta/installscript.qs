function Component()
{
}

Component.prototype.createOperations = function()
{
    // Run the default operations (file extraction, shortcuts, etc.)
    component.createOperations();

    if (systemInfo.productType === "windows") {
        // Schedule a delayed cleanup for uninstallation.
        //
        // Problem: MaintenanceTool.exe cannot delete itself because it is
        // the running process performing the uninstall. This leaves behind
        // MaintenanceTool.exe (and related .dat/.ini files) plus the install
        // directory after uninstall completes.
        //
        // Solution: Use an Execute operation with UNDOEXECUTE so that a
        // cleanup command only runs during uninstallation (the "undo" of
        // installing). We use PowerShell's Start-Process to launch a fully
        // detached cmd.exe that outlives MaintenanceTool, waits a few seconds
        // for the process to exit and release file locks, then removes the
        // install directory.
        //
        // Using PowerShell avoids the notoriously fragile nested quoting
        // issues with cmd.exe's "start" command through QTIFW's Execute op.
        //
        // The "DO" side (install) is a harmless no-op echo.
        component.addElevatedOperation("Execute",
            "cmd", "/c", "echo MidiEditor installed successfully",
            "UNDOEXECUTE",
            "powershell.exe", "-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden", "-Command",
            "Start-Process cmd.exe -ArgumentList '/c ping -n 2 127.0.0.1 >nul & rmdir /s /q \"@TargetDir@\"' -WindowStyle Hidden");
    }
}
