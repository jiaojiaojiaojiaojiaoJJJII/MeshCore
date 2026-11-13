param([switch]$CheckWithWord)
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $taskRoot
try {
    & python -X utf8 bin/export-l2-ui-word.py
    if ($LASTEXITCODE -ne 0) { throw 'Preparing Word content failed' }
    & python -X utf8 bin/build-l2-ui-docx.py
    if ($LASTEXITCODE -ne 0) { throw 'Building DOCX failed' }
    if ($CheckWithWord) {
        $taskWord = $null
        $taskDocument = $null
        $taskNoSave = 0
        [string]$taskInput = Join-Path $taskRoot 'docs/L2-UI-Requirements-0.5.1.docx'
        [string]$taskPdf = Join-Path $taskRoot 'out/l2-debug/doc-export/L2-UI-Requirements-0.5.1.pdf'
        try {
            $taskWord = New-Object -ComObject Word.Application
            $taskWord.Visible = $false
            $taskWord.DisplayAlerts = 0
            $taskDocument = $taskWord.Documents.Open($taskInput, $false, $true, $false)
            $taskDocument.ExportAsFixedFormat($taskPdf, 17)
        } finally {
            if ($null -ne $taskDocument) { $taskDocument.Close([ref]$taskNoSave); [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskDocument) }
            if ($null -ne $taskWord) { $taskWord.Quit([ref]$taskNoSave); [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskWord) }
        }
        & python -X utf8 bin/validate-l2-ui-word.py
        if ($LASTEXITCODE -ne 0) { throw 'DOCX validation failed' }
    }
    Get-Item -LiteralPath docs/L2-UI-Requirements-0.5.1.docx | Select-Object Name,Length
} finally { Pop-Location }
