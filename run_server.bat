@echo off
echo Starting Infiniplex Wasm Local Server...
echo.
echo ==============================================
echo [!] Navigating to localhost:8000 in 3 seconds
echo ==============================================
timeout /t 3 /nobreak >nul
start http://localhost:8000/
python -m http.server 8000
