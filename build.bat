@echo off
REM METABUILD - (c) 2025 FRINKnet & Friends - 0BSD

setlocal enabledelayedexpansion

set REPO=ghcr.io/frinknet/metabuild
for %%i in ("%REPO%") do set IMAGE=metabuild
set WORKDIR=%cd%

REM Check for existing Docker image
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

REM Argument parsing: if %1 is "cli"
if "%1"=="cli" (
    shift
    docker run --rm -it -v "%WORKDIR%:/build" --entrypoint bash %IMAGE% %*
) else (
    docker run --rm -it -v "%WORKDIR%:/build" %IMAGE% %*
)

endlocal
