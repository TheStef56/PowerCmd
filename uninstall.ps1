Remove-Item -Path "$env:PROGRAMFILES\PowerCmd\" -Recurse -Force

$SystemToolsDirectory = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\System Tools"
$DefaultShortcut = Get-ChildItem -Path $SystemToolsDirectory -Filter "*.lnk" | Where-Object { $_.Name -match "cmd|command|prompt" } | Select-Object -First 1

if ($DefaultShortcut -ne $null) {
    Remove-Item -Path $DefaultShortcut.FullName -Force
    Write-Output "Custom Command prompt shortcut removed successfully."
} else {
    Write-Output "Custom Command prompt shortcut not found."
}

$WshShell = New-Object -ComObject WScript.Shell
$CmdPath = "C:\Windows\System32\cmd.exe"

$Shortcut = $WshShell.CreateShortcut("$env:APPDATA\Microsoft\Windows\Start Menu\Programs\System Tools\Command Prompt.lnk")
$Shortcut.TargetPath = $CmdPath
$Shortcut.Save()

Write-Output "Operation completed"