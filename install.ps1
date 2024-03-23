# compiling powercmd.c

gcc -o .\powercmd.exe .\powercmd.c
mkdir "$env:PROGRAMFILES\PowerCmd"
move .\powercmd.exe "$env:PROGRAMFILES\PowerCmd\powercmd.exe"

# removing default cmd shortcut

$SystemToolsDirectory = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\System Tools"
$DefaultShortcut = Get-ChildItem -Path $SystemToolsDirectory -Filter "*.lnk" | Where-Object { $_.Name -match "cmd|command|prompt" } | Select-Object -First 1

if ($DefaultShortcut -ne $null) {
    Remove-Item -Path $DefaultShortcut.FullName -Force
    Write-Output "Default Command Prompt shortcut removed successfully."
} else {
    Write-Output "Default Command Prompt shortcut not found."
}

# Creating a new shortcut

$WshShell = New-Object -ComObject WScript.Shell
$CmdPath = "C:\Windows\System32\cmd.exe"
$ProgramPath = "$env:PROGRAMFILES\PowerCmd\powercmd.exe"

$Shortcut = $WshShell.CreateShortcut("$env:APPDATA\Microsoft\Windows\Start Menu\Programs\System Tools\Command Prompt.lnk")
$Shortcut.Arguments = "/c `"$ProgramPath`"" 
$Shortcut.TargetPath = $CmdPath
$Shortcut.Save()

Write-Output "New shortcut created successfully"