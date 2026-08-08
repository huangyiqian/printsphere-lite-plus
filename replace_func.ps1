 = [System.IO.File]::ReadAllLines('M:\codex\1\src\main.cpp', [System.Text.Encoding]::UTF8)
Write-Output "Total lines: 0"

# Find start marker
 = -1
 = -1
 = 0
 = False
for ( = 0;  -lt .Length; ++) {
    if ([] -match 'String espHomeHtml\(\) \{') {
         = 
         = True
         = 1
    } elseif () {
         = [regex]::Matches([], '\{').Count
         = [regex]::Matches([], '\}').Count
         +=  - 
        if ( -le 0) {
             =  + 1
            break
        }
    }
}

Write-Output "Start line: 1, End line: "
