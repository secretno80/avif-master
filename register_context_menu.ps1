# AVIF-Master Context Menu Registration Script
# 관리자 권한이 필요합니다 (Run as Administrator)

# 변수 설정
$exePath = "C:\Program Files\AVIF-Master\AVIFMaster.exe"  # 실제 설치 경로로 변경 필요
$regPath = "HKCU:\Software\Classes"

# 스크립트가 관리자 권한으로 실행 중인지 확인
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
if (-not $isAdmin) {
    Write-Host "경고: 관리자 권한이 필요합니다. 관리자 권한으로 다시 실행하세요." -ForegroundColor Red
    Read-Host "계속하려면 Enter를 누르세요"
    exit 1
}

# 실행 파일 경로 확인
if (-not (Test-Path $exePath)) {
    Write-Host "오류: AVIF-Master 실행 파일을 찾을 수 없습니다: $exePath" -ForegroundColor Red
    Write-Host "설치 경로를 확인하고 스크립트의 \$exePath 변수를 수정하세요." -ForegroundColor Yellow
    Read-Host "계속하려면 Enter를 누르세요"
    exit 1
}

Write-Host "AVIF-Master 컨텍스트 메뉴를 등록 중입니다..." -ForegroundColor Green
Write-Host "실행 파일: $exePath" -ForegroundColor Cyan

# 1. *.jpg, *.jpeg, *.png, *.gif, *.webp 파일의 컨텍스트 메뉴
$extensions = @("jpg", "jpeg", "png", "gif", "webp")

foreach ($ext in $extensions) {
    $regPathExt = "$regPath\.$ext\shell"
    
    # "Convert with AVIF-Master (Now)" - 자동 변환 모드
    $autoPath = "$regPathExt\AVIFMasterAuto\command"
    New-Item -Path $autoPath -Force | Out-Null
    Set-ItemProperty -Path $autoPath -Name "(Default)" -Value "`"$exePath`" /auto %1" -Type String
    
    # "Convert with AVIF-Master" - 일반 모드 (사용자가 수동으로 시작)
    $normalPath = "$regPathExt\AVIFMasterNormal\command"
    New-Item -Path $normalPath -Force | Out-Null
    Set-ItemProperty -Path $normalPath -Name "(Default)" -Value "`"$exePath`" %1" -Type String
    
    # 메뉴 라벨 설정
    Set-ItemProperty -Path "$regPathExt\AVIFMasterAuto" -Name "(Default)" -Value "Convert with AVIF-Master (Start Now)" -Type String
    Set-ItemProperty -Path "$regPathExt\AVIFMasterNormal" -Name "(Default)" -Value "Convert with AVIF-Master" -Type String
    
    Write-Host "✓ .$ext 파일 컨텍스트 메뉴 등록 완료" -ForegroundColor Green
}

# 2. *.zip, *.7z 파일의 컨텍스트 메뉴
$archives = @("zip", "7z")

foreach ($ext in $archives) {
    $regPathExt = "$regPath\.$ext\shell"
    
    # "Convert with AVIF-Master (Now)" - 자동 변환 모드
    $autoPath = "$regPathExt\AVIFMasterAuto\command"
    New-Item -Path $autoPath -Force | Out-Null
    Set-ItemProperty -Path $autoPath -Name "(Default)" -Value "`"$exePath`" /auto %1" -Type String
    
    # "Convert with AVIF-Master" - 일반 모드
    $normalPath = "$regPathExt\AVIFMasterNormal\command"
    New-Item -Path $normalPath -Force | Out-Null
    Set-ItemProperty -Path $normalPath -Name "(Default)" -Value "`"$exePath`" %1" -Type String
    
    # 메뉴 라벨 설정
    Set-ItemProperty -Path "$regPathExt\AVIFMasterAuto" -Name "(Default)" -Value "Convert Archive with AVIF-Master (Start Now)" -Type String
    Set-ItemProperty -Path "$regPathExt\AVIFMasterNormal" -Name "(Default)" -Value "Convert Archive with AVIF-Master" -Type String
    
    Write-Host "✓ .$ext 파일 컨텍스트 메뉴 등록 완료" -ForegroundColor Green
}

# 3. 폴더의 컨텍스트 메뉴
$folderPath = "$regPath\Directory\shell"

# "Convert with AVIF-Master (Now)" - 자동 변환 모드
$autoPath = "$folderPath\AVIFMasterAuto\command"
New-Item -Path $autoPath -Force | Out-Null
Set-ItemProperty -Path $autoPath -Name "(Default)" -Value "`"$exePath`" /auto %1" -Type String

# "Convert with AVIF-Master" - 일반 모드
$normalPath = "$folderPath\AVIFMasterNormal\command"
New-Item -Path $normalPath -Force | Out-Null
Set-ItemProperty -Path $normalPath -Name "(Default)" -Value "`"$exePath`" %1" -Type String

# 메뉴 라벨 설정
Set-ItemProperty -Path "$folderPath\AVIFMasterAuto" -Name "(Default)" -Value "Convert Folder with AVIF-Master (Start Now)" -Type String
Set-ItemProperty -Path "$folderPath\AVIFMasterNormal" -Name "(Default)" -Value "Convert Folder with AVIF-Master" -Type String

Write-Host "✓ 폴더 컨텍스트 메뉴 등록 완료" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "컨텍스트 메뉴 등록이 완료되었습니다!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "사용 방법:" -ForegroundColor Yellow
Write-Host "  1. '~(Start Now)' 옵션: 파일을 선택하면 즉시 변환 시작" -ForegroundColor White
Write-Host "  2. '~' 옵션: 프로그램을 열고 사용자가 'Start Conversion' 버튼 클릭" -ForegroundColor White
Write-Host ""
Read-Host "계속하려면 Enter를 누르세요"
