# 자동 attach PowerShell 스크립트 (usbipd 최신 명령어 기반)
$busid = "1-9"  # <- 위에서 확인한 BUSID로 수정하세요

# 관리자 권한으로 실행되어야 작동함
Start-Process usbipd -ArgumentList "attach --wsl --busid $busid" -Verb RunAs