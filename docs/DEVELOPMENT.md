# Development

Configure and build with the commands in the root `README.md`.

Run C++ tests:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" `
  --test-dir build -C Release --output-on-failure

cmd /c """C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"" >nul && python server\game_server\tests\run_tests.py"
```

Run API health logic tests:

```powershell
Set-Location server\api_gateway
python -m unittest discover -s tests -t . -v
Set-Location ..\..
```

Local `.env` values are never committed. Copy `.env.example` to `.env` for
Docker development.
