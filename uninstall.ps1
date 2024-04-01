# delete program folder

Remove-Item -Path "$env:APPDATA\PowerCmd\" -Recurse -Force

$SystemToolsDirectory = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\System Tools"
$DefaultShortcut = Get-ChildItem -Path $SystemToolsDirectory -Filter "*.lnk" | Where-Object { $_.Name -match "cmd|command|prompt" } | Select-Object -First 1

# delete custom shortcut

if ($DefaultShortcut -ne $null) {
    Remove-Item -Path $DefaultShortcut.FullName -Force
    Write-Output "Custom Command prompt shortcut removed successfully."
} else {
    Write-Output "Custom Command prompt shortcut not found."
}

# putting back cmd original shortcut

$WshShell = New-Object -ComObject WScript.Shell
$CmdPath = "C:\Windows\System32\cmd.exe"

$Shortcut = $WshShell.CreateShortcut("$env:APPDATA\Microsoft\Windows\Start Menu\Programs\System Tools\Command Prompt.lnk")
$Shortcut.TargetPath = $CmdPath
$Shortcut.Save()

# remove program folder from PATH

$programPathToRemove = "$env:APPDATA\PowerCmd\"

$currentPath = [System.Environment]::GetEnvironmentVariable("Path", [System.EnvironmentVariableTarget]::Machine)

if ($currentPath -like "*$programPathToRemove*") {
    $newPath = $currentPath -replace [regex]::Escape($programPathToRemove), ""
    $newPath = $newPath -replace ";;", ";"  # Remove any double semicolons
    $newPath = $newPath.Trim(';')           # Trim any leading or trailing semicolons
    [System.Environment]::SetEnvironmentVariable("Path", $newPath, [System.EnvironmentVariableTarget]::Machine)
    Write-Host "Program path removed from the PATH variable for all users."
} else {
    Write-Host "Program path does not exist in the PATH variable."
}

Write-Output "Operation completed"
