AVIF-Master 기술 사양서 및 개발 가이드

[Part 1: 시스템 개요 및 아키텍처 설계]

1. 프로젝트 개요
AVIF-Master는 AMD Ryzen 9 5950X (16코어/32스레드) 및 Radeon RX 6800 XT (16GB VRAM) 환경에 최적화된 로우레벨 하이브리드 변환 도구입니다. 
Win32 API 기반의 초경량 GUI, libarchive 라이브러리를 이용한 인메모리 스트림 처리, GPU VRAM 캐싱 및 OpenCL 전처리 파이프라인을 결합하여, 대량의 이미지 데이터를 지연 없이 초고속으로 처리하는 것을 목표로 합니다.

**v2.0 업데이트:**
- ✅ libarchive 통합으로 외부 의존성 제거 (7-Zip 불필요)
- ✅ 성능 향상 (프로세스 호출 오버헤드 제거)
- ✅ 안정성 향상 (라이브러리 기반 처리)
- ✅ 배포 단순화 (libarchive.dll 1개만 번들링)

2. 마스터-슬레이브(Master-Slave) 인스턴스 관리 설계 (데이터 누락 방지)
탐색기 컨텍스트 메뉴를 통해 다수의 파일이 유입될 때 발생할 수 있는 데이터 누락을 방지하기 위한 동기화 아키텍처입니다.

2.1 적응형 대기 시스템 (Adaptive Settling Time)
탐색기에서 대량의 항목을 선택하고 실행할 때, 윈도우 OS는 인자(Argument)를 여러 개의 프로세스로 나누어 실행하거나 순차적으로 실행할 수 있습니다. 마스터 프로세스가 이를 완벽하게 취합하기 위해 다음 로직을 사용합니다.

- 마스터 선점: 첫 번째 실행된 프로세스가 Global Mutex(wWinMain에서 CreateMutexW 활용)를 선점하여 Master가 됩니다. GUI 메시지 루프를 유지합니다.
- 슬레이브 행동: 후속 실행된 프로세스(Slave)들은 자신의 경로 인자를 Master의 뮤텍스를 소유할 수 없을 때, Temp 폴더 내에 고유 PID 명명 규칙을 가진 .args 파일에 기록하고 즉시 종료됩니다.
- 적응형 대기: Master는 wWinMain 진입 직후, .args 파일이 새로 생성되는지 감시(Polling)하는 타이머 기반 수집 단계를 가집니다. 마지막 .args 파일이 생성된 시점부터 300ms ~ 500ms 동안 추가 유입이 없는지 확인합니다. 
이 대기 시간이 충족되면 모든 .args 파일을 일괄 취합하여 그리드(GridView)에 원자적으로 등록하고, 워커 스레드(5950X 32스레드)를 가동합니다.

2.2 IPC (프로세스 간 통신)
초기 대기 시스템 가동 이후 프로그램이 실행 중일 때 추가로 실행된 건에 대해서는 WM_COPYDATA 메시지를 통해 마스터 윈도우의 그리드에 항목을 추가합니다.

2.3 자동 변환 모드 (Auto-Start on File Collection)
명령줄 인자 `/auto` 또는 `--auto-convert`를 통해 파일 수집 완료 후 자동으로 변환을 시작할 수 있습니다.
- 파일 수집 시작 → 모든 파일 수신 대기 (Settling Time) → 수집 완료 → **자동으로 변환 시작**
- 컨텍스트 메뉴에서 "Convert with AVIF-Master (Start Now)" 옵션을 선택하면 자동 모드 활성화
- 일반 "Convert with AVIF-Master" 옵션은 파일만 추가하고 사용자 확인 후 시작

[Part 2: 상세 기능 및 UI 명세]

3. 메인 화면 그리드(ListView) 컬럼 구성 및 동작
리소스 소모를 최소화하기 위해 Virtual ListView (LVS_OWNERDATA 모드)를 사용합니다. 각 컬럼은 워커 스레드로부터 상태 플래그를 받아 실시간 업데이트됩니다.

- 이름: 파일명 또는 압축파일명 표시 (SHGetFileInfo를 이용해 시스템 아이콘 Lazy Loading). Updated upon collection.
- 형식: 대상의 형식(JPG, PNG, WebP, ZIP, 7Z 등) 표시 (확장자 기반 단순 판별). Updated during collection.
- 원본 용량: 변환 전 데이터 크기를 사용자 친화적 단위(KB/MB/GB)로 표시 (GetFileAttributesExW 사용, 파일 헤더 오픈 없음). Updated during collection.
- 상태: 실시간 상태 표시 (대기 / 수집중... / 변환중(%) / 완료 / 실패). atomically updated.
- 결과 용량: 변환 완료 후 실제 생성된 AVIF 파일 크기 표시. Updated after successful encoder exit.
- 압축률: 원본 대비 절감 비율 (%) 표시 ((원본 용량 - 결과 용량) / 원본 용량 * 100). Updated after successful conversion.

4. 압축 파일 처리 전략
압축 파일(ZIP/7Z) 등록 시, 내부 파일을 전개하지 않고 **하나의 행(Row)**으로 등록합니다.

상태 칸에 [변환중] 45/200과 같이 내부 진행도를 텍스트로 업데이트하여 UI 부하를 줄입니다.

변환 완료 후 재압축(Repacking) 로직: 원본 압축 구조와 동일하게 .zip 또는 .7z로 재포장하며, 변환 완료된 AVIF 파일만 아카이브 내에서 교체합니다.

4.1 압축 파일 처리 상세 프로세스
- 압축 해제: 임시 폴더에 전개 (진행상태: "압축 해제 중...")
- 이미지 변환: 각 이미지를 in-place 변환 (진행상태: "변환 중 X/N")
- 원본 백업: "Preserve archive backup" 옵션 활성화 시 원본을 `_backup` 이름으로 보존
- 재압축: 같은 포맷(ZIP/7Z)으로 재압축 (진행상태: "재압축 중...")
- 로깅: "Save conversion log" 옵션 활성화 시 변환 결과 기록
  - 로그 위치: `원본_디렉토리\AVIF_Conversion_Logs\YYYYMMDD_HHMMSS_convert.log`
  - 기록 내용: 파일명, 시간, 성공/실패 통계, 실패 파일 목록

[Part 3: UI 레이아웃 및 옵션 상세 설정]

5. 메인 화면 구성 및 요소 별 기능
메인 윈도우는 크게 세 개의 기능 영역으로 나뉩니다.

5.1 그리드 영역 (상단)
전용 가상 리스트뷰(hWndList = CreateWindowExW(..., WC_LISTVIEWW, ...)) 배치. LVS_OWNERDATA 사용.

개별 체크박스를 통한 배치 선택 지원.

WM_DROPFILES 메시지를 처리하여 Drag & Drop 기능 구현.

5.2 성능 및 옵션 제어 영역 (우측 패널)
Performance 탭과 File Management 탭으로 구성된 탭 컨트롤 패널입니다.

Performance 탭
- 하드웨어 실행 방식 토글: [CPU Only] (정밀도 우선) / [CPU + GPU 하이브리드] (Radeon 6800 XT OpenCL 전처리 파이프라인 가속).
- GPU Caching 토글: GPU 모드 선택 시 활성화. 비동기 VRAM 사전 캐싱(Asynchronous Prefetching) 로직 활성화, 16GB VRAM 활용.
- 소프트웨어 엔진 토글: [avifenc(SVT-AV1)] / [FFmpeg] / [av1an] 중 선택 사용.
- CPU 전략 제어 (Batching): 동시에 처리할 이미지 개수(Concurrent Jobs)와 이미지당 할당 스레드 수(Threads per Job)를 하드웨어(5950X)에 맞춰 최적화 분할.
- SIMD/Assembly 토글: SVT-AV1 인코더의 AVX2/AVX-512 어셈블리 최적화 플래그 강제 활성화/비활성화.
- **Quality Preset** (새로 추가됨):
  - ⚡ **Fast** (Speed Priority): 빠른 변환, 낮은 압축률
    - avifenc: --preset 8 (낮은 품질)
    - FFmpeg: -crf 25 (낮은 품질)
    - av1an: --speed 2 (빠른 처리)
  - ⚙️ **Normal** (Balanced): 기본값, 성능과 품질의 균형
    - avifenc: --preset 6 (기본)
    - FFmpeg: -crf 30 (기본)
    - av1an: --speed 6 (기본)
  - 🎯 **High** (Compression Priority): 느린 변환, 높은 압축률과 품질
    - avifenc: --preset 4 (높은 품질)
    - FFmpeg: -crf 35 (높은 품질)
    - av1an: --speed 10 (고압축)

File Management 탭
- 출력 경로 설정: [원본 폴더] / [특정 폴더 지정] / [하위 폴더 구조 유지] (Radio buttons).
- 중복 변환 방지 토글: 동일 파일명 또는 이미 변환된 .avif가 존재할 경우 건너뛰기 옵션.
- 노이즈 제거 필터 토글: 활성화 시 GPU OpenCL fastNlMeansDenoising 필터를 사용하여 텍스처 노이즈 제거.
- 리사이징 설정: LANCZOS3 리사이징 필터 적용 및 특정 해상도 설정.
- 자동 종료 토글: 변환 완료 후 자동으로 프로그램 종료.
- **원본 아카이브 백업**: 압축 파일 변환 시 원본을 `_backup` 이름으로 보존 (ZIP/7Z 전용).
- **변환 로그 저장**: 변환 결과를 타임스탐프 포함 로그 파일로 기록 (기본 활성화).
- 프로파일 저장: 현재 설정 구성을 저장(Save)하거나 불러오기(Load) 기능.

5.3 상태 표시 영역 (하단 상태바)
하단 전체 영역 Smooth Progress Bar 배치.

- 좌측: 전체 진행 상태 표시 (Idle / 진행중... / 완료 / 실패).
- 우측: 현재 처리 속도(TPS/FPS), 남은 시간(ETA) 표시.

6. 완료 리포트 대시보드 (Success Report)
작업이 완료되면 나타나는 요약 보고창입니다.

총 진행 시간: 밀리초(ms) 단위까지 기록.

성공/실패 건수: 상세 오류 로그 링크 제공.
용량 변화: 원본 전체 용량 -> 결과 전체 용량 (절감 용량 % 및 bar chart로 표시).

[Part 4: 빌드, 설치 및 배포]
7. 컨텍스트 메뉴 통합
우클릭 컨텍스트 메뉴 이름: "AVIF-Master로 고속 변환"

동작: HKCR 레지스트리 키(*\shell 및 Directory\Background\shell) 설정을 통해 %1 인자를 마스터 프로세스에 전달.Adaptive Settling Time 로직(300-500ms 지연)을 통해 모든 대상 수집 후 그리드 등록.

8. 빌드 및 배포 (설치 파일 생성)
이 섹션은 MSBuild(Visual Studio)와 Inno Setup(Compiler CLI)을 사용하여 프로젝트를 컴파일하고 Windows 설치 파일을 생성하는 방법을 기술합니다. 하나의 배치 파일로 전체 과정을 관리합니다.

8.1 빌드 및 배포 배치 파일 (BuildAndDeploy.bat)
DOS
@echo off
REM AVIF-Master 빌드 및 배포 스크립트
REM -----------------------------------
REM 이 스크립트는 Visual Studio(MSBuild 포함)와 Inno Setup이 설치되어 있어야 합니다.
REM 아래 경로를 사용자 환경에 맞게 업데이트하세요.

set PROJECT_DIR=%~dp0
set MSBUILD_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set INNO_SETUP_COMPILER="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

set PROJECT_FILE="%PROJECT_DIR%AVIFMaster.sln"
set ISS_FILE="%PROJECT_DIR%AVIFMasterInstaller.iss"
set OUTPUT_DIR="%PROJECT_DIR%Build\Release"
set INSTALLER_OUTPUT_DIR="%PROJECT_DIR%Setup"

REM --- Cleanup ---
echo 이전 빌드 청소 중...
if exist %OUTPUT_DIR% rd /s /q %OUTPUT_DIR%
if exist %INSTALLER_OUTPUT_DIR% rd /s /q %INSTALLER_OUTPUT_DIR%
echo.

REM --- Step 1: C++ Win32 프로젝트 빌드 ---
echo AVIF-Master 빌드 중 (Release)...
call %MSBUILD_PATH% %PROJECT_FILE% /p:Configuration=Release /p:Platform=x64 /t:Rebuild /m
if errorlevel 1 goto BUILD_ERROR
echo AVIF-Master 빌드 성공.
echo.

REM --- 필수 종속성 복사 ---
echo 필수 종속성 복사 중 (7z.dll, SVT-AV1 바이너리)...
REM 예시 복사 명령, 실제 경로로 업데이트하세요.
REM copy "%PROJECT_DIR%Dependencies\7z.dll" %OUTPUT_DIR%
REM copy "%PROJECT_DIR%Dependencies\SVT-AV1\bin\*" %OUTPUT_DIR%
echo 종속성 복사 완료.
echo.

REM --- Step 2: Inno Setup 설치 파일 컴파일 ---
echo Inno Setup 설치 파일 컴파일 중...
REM 설치 아이콘 존재 확인:
if not exist "%PROJECT_DIR%Setup.ico" (
    echo 에러: Setup.ico를 %PROJECT_DIR%에서 찾을 수 없습니다.
    goto DEPLOY_ERROR
)

call %INNO_SETUP_COMPILER% %ISS_FILE%
if errorlevel 1 goto DEPLOY_ERROR
echo 설치 파일 컴파일 성공.
echo.

REM --- 최종 단계 ---
echo 빌드 및 배포 완료.
goto END

:BUILD_ERROR
echo MSBuild 실패. 설치 파일 생성을 중단합니다.
exit /b 1

:DEPLOY_ERROR
echo Inno Setup 컴파일 실패.

---

[Part 4: 사용 설명서 및 컨텍스트 메뉴 설정]

6. 컨텍스트 메뉴 등록

6.1 자동 설정 (권장)
1. 관리자 권한으로 PowerShell을 실행한다.
2. `register_context_menu.ps1` 스크립트를 실행한다:
   ```powershell
   Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process -Force
   & "C:\path\to\register_context_menu.ps1"
   ```
3. 스크립트에서 질문하는 대로 진행한다.

주의: `$exePath` 变수를 AVIF-Master 실행 파일의 실제 설치 경로로 수정해야 한다.

6.2 컨텍스트 메뉴 옵션

**이미지 파일 (*.jpg, *.jpeg, *.png, *.gif, *.webp)**
- "Convert with AVIF-Master (Start Now)": 파일을 선택하면 즉시 변환 시작
- "Convert with AVIF-Master": 프로그램을 열고 사용자가 확인 후 시작

**압축 파일 (*.zip, *.7z)**
- "Convert Archive with AVIF-Master (Start Now)": 압축 해제 → 변환 → 재압축 자동 실행
- "Convert Archive with AVIF-Master": 파일만 추가하고 사용자 확인 후 시작

**폴더**
- "Convert Folder with AVIF-Master (Start Now)": 폴더 내 이미지를 즉시 변환
- "Convert Folder with AVIF-Master": 폴더만 추가하고 사용자 확인 후 시작

6.3 컨텍스트 메뉴 제거
```powershell
Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process -Force
& "C:\path\to\unregister_context_menu.ps1"
```

7. 명령줄 인자

**자동 변환 모드 시작**
```
AVIFMaster.exe /auto file1.jpg file2.png archive.zip
```

**설명:**
- `/auto` 또는 `--auto-convert`: 파일 수집 완료 후 자동으로 변환 시작
- 컨텍스트 메뉴의 "(Start Now)" 옵션에서 자동으로 사용됨
- 파일 수집 시간 0.5초 후 자동 변환 시작

**정상 모드 (사용자 확인 필요)**
```
AVIFMaster.exe file1.jpg file2.png archive.zip
```

**설명:**
- 파일을 먼저 추가하고 GUI에서 "Start Conversion" 버튼 클릭 필요
- 컨텍스트 메뉴의 일반 옵션에서 사용됨
exit /b 1

:END
pause
exit /b 0
8.2 샘플 Inno Setup 스크립트 (AVIFMasterInstaller.iss)
이 스크립트는 컨텍스트 메뉴 통합 및 종속성 번들링에 필수적입니다.

Delphi
; AVIF-Master 설치 스크립트
; ---------------------------

[Setup]
AppName=AVIF-Master
AppVersion=1.0
AppPublisher=Advanced Micro Conversions
DefaultDirName={pf}\AVIFMaster
DefaultGroupName=AVIFMaster
UninstallDisplayIcon={app}\AVIFMaster.exe
Compression=lzma2/ultra
SolidCompression=yes
SetupIconFile=Setup.ico
SetupLogging=yes

[Files]
; 메인 애플리케이션 바이너리
Source: "Build\Release\AVIFMaster.exe"; DestDir: "{app}"; Flags: ignoreversion
; bundled Dependencies
Source: "Dependencies\7z.dll"; DestDir: "{app}"; Flags: ignoreversion
; 메인 설치 아이콘 (컨텍스트 메뉴에도 사용)
Source: "Setup.ico"; DestDir: "{app}"; Flags: ignoreversion
; 기타 필요한 SVT-AV1 DLLs/Executables (경로 업데이트)
; Source: "Dependencies\SVT-AV1\bin\*"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
; 파일에 대한 루트 컨텍스트 메뉴
Root: HKCR; Subkey: "*\shell\AVIFMaster"; ValueData: "AVIF-Master로 고속 변환"; Flags: uninsdeletekey
Root: HKCR; Subkey: "*\shell\AVIFMaster"; ValueName: "Icon"; ValueData: "{app}\Setup.ico,0"; Flags: uninsdeletekey
Root: HKCR; Subkey: "*\shell\AVIFMaster\command"; ValueData: """{app}\AVIFMaster.exe"" ""%1"""; Flags: uninsdeletekey

; 폴더 핸들링을 위한 디렉토리 배경의 루트 컨텍스트 메뉴
Root: HKCR; Subkey: "Directory\Background\shell\AVIFMaster"; ValueData: "AVIF-Master로 고속 변환"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Directory\Background\shell\AVIFMaster"; ValueName: "Icon"; ValueData: "{app}\Setup.ico,0"; Flags: uninsdeletekey
Root: HKCR; Subkey: "Directory\Background\shell\AVIFMaster\command"; ValueData: """{app}\AVIFMaster.exe"" ""%1"""; Flags: uninsdeletekey

[Icons]
Name: "{group}\AVIFMaster"; Filename: "{app}\AVIFMaster.exe"; IconFilename: "{app}\Setup.ico"
Name: "{group}\{cm:UninstallProgram,AVIFMaster}"; Filename: "{uninstaller}"

───────────────────────────────────────────────────────────────
[Part 5: 문제 해결 및 FAQ]

**⚠️ v2.0부터 libarchive 통합으로 7-Zip 외부 의존성이 완전히 제거되었습니다.**
- ✅ 7z.exe 설치 불필요
- ✅ libarchive.dll은 자동 번들링됨
- ✅ 압축파일 변환 즉시 작동

9. 자주 발생하는 오류 및 해결 방법

9.1 에러: "압축 해제 실패 (손상됨?)"
증상:
- 압축파일(.zip, .7z) 변환 시 "압축 해제 실패" 에러
- Status: "실패 N / 전체 N", Elapsed: 0.00초

원인:
- 압축파일이 손상된 경우
- 압축파일 포맷이 ZIP 또는 7Z가 아님
- 파일 권한 문제

해결 방법:
1. 압축파일 정합성 확인
   - 다른 압축 프로그램(WinRAR, Bandizip, 7-Zip)으로 열어보기
   - 파일 테스트 기능 사용하여 정합성 검증
   
2. 지원 형식 확인
   - 지원 아카이브: ZIP, 7Z만 가능
   - 다른 형식(.rar, .tar, .gz 등)은 변환 불가
   
3. 파일 권한 확인
   - 압축파일을 우클릭 > 속성
   - "읽기 전용" 체크박스 해제
   - 임시 폴더 권한 확인 (%TEMP% 경로)
   - PATH 환경변수에 등록된 경로

9.2 에러: "권한 부족 (읽기/쓰기)"
증상:
- 압축파일 변환 시 "권한 부족" 에러 발생
- 읽기 전용 폴더의 파일 처리 실패

해결 방법:
1. 파일 권한 확인
   - 압축파일을 우클릭 > 속성
   - "읽기 전용" 체크박스 해제
   - 적용 및 확인 클릭

2. 폴더 권한 확인
   - 파일이 있는 폴더의 권한 확인
   - 사용자가 파일 읽기/쓰기 권한 있는지 확인

3. 임시 폴더 권한 확인
   - Windows 임시 폴더(%TEMP%) 쓰기 권한 필수
   - C:\Users\[사용자명]\AppData\Local\Temp 권한 확인

9.3 에러: "압축파일 열기 실패"
증상:
- "Cannot open" 메시지와 함께 실패
- 손상된 압축파일로 의심

해결 방법:
1. 압축파일 정합성 확인
   - 다른 압축 프로그램(WinRAR, Bandizip 등)으로 열어보기
   - 파일 테스트 기능 사용 (7-Zip: 우클릭 > 테스트)

2. 압축파일 복구
   - 손상된 파일 재다운로드
   - 또는 다른 소스에서 파일 획득

3. 압축 형식 확인
   - ZIP 또는 7Z 형식만 지원
   - 다른 형식(.rar, .tar 등)의 경우 변환 불가

9.4 에러: "임시폴더 생성 실패 (디스크 공간 확인)"
증상:
- 압축파일 해제 단계에서 실패
- 임시 폴더 생성 불가

해결 방법:
1. 디스크 공간 확인
   - C: 드라이브 여유 공간 확인 (최소 1GB 권장)
   - 불필요한 파일 삭제하여 공간 확보
   - cleanup 도구 실행 (cleanmgr.exe 또는 Disk Cleanup)

2. 임시 폴더 위치 확인
   - %TEMP% 환경변수 확인
   - 기본: C:\Users\[사용자명]\AppData\Local\Temp
   - 수동 설정 가능: 환경변수 설정으로 다른 드라이브 지정

3. 권한 확인
   - 임시 폴더에 쓰기/삭제 권한 필수

9.5 에러: "이미지 없음"
증상:
- 압축파일 해제는 성공했지만 이미지를 찾을 수 없음
- 상태: "실패 0 / 전체 0"

일반적인 원인:
- 압축파일에 JPEG, PNG, GIF, WebP, TIFF 이미지가 없음
- 지원하지 않는 형식의 파일만 포함 (예: 텍스트, 비디오 등)
- 압축파일의 폴더 구조가 깊어서 이미지를 찾지 못함

해결 방법:
1. 압축파일 내용 확인
   - 7-Zip으로 압축파일 열기
   - JPEG, PNG, GIF, WebP, TIFF 이미지가 있는지 확인
   - 폴더 구조 확인

2. 지원 형식 목록
   입력 형식: JPEG, PNG, GIF, WebP, TIFF
   출력 형식: AVIF
   아카이브: ZIP, 7Z

9.6 변환 로그 확인 방법
변환 결과 상세 정보는 다음 위치의 로그 파일에 기록됩니다:
- 경로: [압축파일 위치]\AVIF_Conversion_Logs\
- 파일명: [타임스탐프]_convert.log (예: 20260412_143152_convert.log)
- 내용: 성공/실패 건수, 실패 파일 목록, 에러 상세 정보

로그를 확인하려면:
1. 변환 대상 파일이 있는 폴더로 이동
2. AVIF_Conversion_Logs 폴더 열기
3. 해당 시간의 로그 파일을 텍스트 에디터로 열기

10. 성능 최적화 팁

10.1 Quality Preset 선택
- ⚡ Fast: 빠른 처리, 낮은 품질 → 많은 파일 빠르게 처리 시 선택
- ⚙️ Normal (기본값): 품질과 속도의 균형
- 🎯 High: 느린 처리, 높은 품질 → 고품질 아카이브 생성 시 선택

10.2 Hardware 최적화
- CPU Only: 정밀도 우선 (디버깅 또는 검증 용)
- CPU + GPU (OpenCL): Radeon 6800 XT 활용 → 최고 성능
- GPU VRAM Cache: 16GB VRAM 환경에서 활성화 필수

10.3 Engine 선택
- avifenc (SVT-AV1) 권장: 빠르고 안정적
- FFmpeg: 폭넓은 호환성
- av1an: 고압축 지향

10.4 CPU 전략
- Concurrent Jobs: 5950X 기준 8-16 권장
- Threads/Job: 2-4 권장 (총 스레드 수 내에서 조정)
- SIMD/Assembly: AVX2/AVX-512 활성화 (기본값)