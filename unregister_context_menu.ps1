# AVIF-Master Context Menu Unregistration Script
# 관리자 권한이 필요합니다 (Run as Administrator)

# 변수 설정
$regPath = "HKCU:\Software\Classes"

# 스크립트가 관리자 권한으로 실행 중인지 확인
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")
if (-not $isAdmin) {
    Write-Host "경고: 관리자 권한이 필요합니다. 관리자 권한으로 다시 실행하세요." -ForegroundColor Red
    Read-Host "계속하려면 Enter를 누르세요"
    exit 1
}

Write-Host "AVIF-Master 컨텍스트 메뉴를 제거 중입니다..." -ForegroundColor Green

# 1. 이미지 파일의 컨텍스트 메뉴 제거
$extensions = @("jpg", "jpeg", "png", "gif", "webp")

foreach ($ext in $extensions) {
    $regPathExt = "$regPath\.$ext\shell"
    
    if (Test-Path "$regPathExt\AVIFMasterAuto") {
        Remove-Item -Path "$regPathExt\AVIFMasterAuto" -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path "$regPathExt\AVIFMasterNormal") {
        Remove-Item -Path "$regPathExt\AVIFMasterNormal" -Recurse -Force -ErrorAction SilentlyContinue
    }
    
    Write-Host "✓ .$ext 파일 컨텍스트 메뉴 제거 완료" -ForegroundColor Green
}

# 2. 압축 파일의 컨텍스트 메뉴 제거
$archives = @("zip", "7z")

foreach ($ext in $archives) {
    $regPathExt = "$regPath\.$ext\shell"
    
    if (Test-Path "$regPathExt\AVIFMasterAuto") {
        Remove-Item -Path "$regPathExt\AVIFMasterAuto" -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path "$regPathExt\AVIFMasterNormal") {
        Remove-Item -Path "$regPathExt\AVIFMasterNormal" -Recurse -Force -ErrorAction SilentlyContinue
    }
    
    Write-Host "✓ .$ext 파일 컨텍스트 메뉴 제거 완료" -ForegroundColor Green
}

# 3. 폴더의 컨텍스트 메뉴 제거
$folderPath = "$regPath\Directory\shell"

if (Test-Path "$folderPath\AVIFMasterAuto") {
    Remove-Item -Path "$folderPath\AVIFMasterAuto" -Recurse -Force -ErrorAction SilentlyContinue
}
if (Test-Path "$folderPath\AVIFMasterNormal") {
    Remove-Item -Path "$folderPath\AVIFMasterNormal" -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "✓ 폴더 컨텍스트 메뉴 제거 완료" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "컨텍스트 메뉴 제거가 완료되었습니다!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Read-Host "계속하려면 Enter를 누르세요"
