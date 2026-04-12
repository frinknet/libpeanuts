@echo off
:: METABUILD - (c) 2025 FRINKnet & Friends - 0BSD

setlocal enabledelayedexpansion

:: THIS IS THE BUILD REPO
set REPO=ghcr.io/frinknet/metabuild
for %%i in ("%REPO%") do set IMAGE=metabuild
set WORKDIR=%cd%

:: MAKE SURE WE HAVE A CONTAINER
docker image inspect %IMAGE% >nul 2>&1
if errorlevel 1 (
    REM Try to pull
    docker pull %REPO%:latest >nul 2>&1
    if errorlevel 1 (
	echo ^>^>^> Building Docker Image: %IMAGE%
	docker build -t %IMAGE% .
    ) else (
	docker tag %REPO%:latest %IMAGE%
    )
)

:: GET PROJECT DIRECTORY
for %%I in (.) do set PRJ_DIR=%%~nI
set PRJ=%PRJ_DIR%

:: NOW USE IT...
docker run --rm -it ^
  -e PRJ=%PRJ% ^
  -v "%CD%:/build" ^
  %IMAGE% %*

endlocal
