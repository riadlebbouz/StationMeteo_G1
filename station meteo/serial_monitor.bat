@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0serial_monitor.ps1" %*
