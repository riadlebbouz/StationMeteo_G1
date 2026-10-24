# Serial Monitor de la station meteo : affiche ce que la carte envoie sur COM3 (115200 bauds)
# et envoie a la carte ce qui est tape au clavier (commandes du mode configuration).
# Lancement : double-clic sur serial_monitor.bat dans l'Explorateur Windows. Pour quitter : Ctrl + C ou fermer la fenetre.
param([string]$Port = "COM3", [int]$Baud = 115200)

$Host.UI.RawUI.WindowTitle = "Serial Monitor $Port @ $Baud"
$p = New-Object System.IO.Ports.SerialPort $Port, $Baud, None, 8, One
try {
    $p.Open()
} catch {
    Write-Host "Impossible d'ouvrir $Port : carte debranchee ou port deja utilise." -ForegroundColor Red
    Read-Host "Appuie sur Entree pour fermer"
    exit 1
}
$p.DiscardInBuffer() # anciennes lignes gardees par le ST-Link
Write-Host "Connecte a $Port ($Baud bauds). Appuie sur RESET (bouton noir) pour voir la banniere. Ctrl + C pour quitter." -ForegroundColor Green
Write-Host "En mode configuration (LED jaune) : tape une commande puis Entree (ex. HELP)." -ForegroundColor Green
try {
    while ($true) {
        $txt = $p.ReadExisting()
        if ($txt) { Write-Host -NoNewline $txt }
        while ([Console]::KeyAvailable) {
            $k = [Console]::ReadKey($true)
            if ($k.Key -eq 'Enter') { $p.Write("`r") }
            elseif ($k.Key -eq 'Backspace') { $p.Write([string][char]8) }
            elseif ($k.KeyChar -ne [char]0) { $p.Write([string]$k.KeyChar) } # la carte renvoie l'echo
        }
        Start-Sleep -Milliseconds 20
    }
} finally {
    $p.Close()
}
