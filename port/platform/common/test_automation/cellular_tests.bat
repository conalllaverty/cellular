@echo off
echo This batch file sets up the environment variables for building/testing the cellular driver https://github.com/u-blox/cellular.
echo.

setlocal

rem local variables
set fetch=
set createdir=
set cleandir=
set cleanbuild=
set platform_number=
set platform_string=
set directory=
set config_directory=%~dp0config
set com_port=

rem Platform descriptions.  If you add a new one, search for "platform_1" in this batch file to 
rem find all the other places you need to append it.
rem Note: special characters need to be DOUBLE escaped (i.e. ^^) in these platform descriptions,
rem see https://www.robvanderwoude.com/escapechars.php for a list of special characters.
set platform_1=unit tests under latest Espressif SDK on the ESP32 chipset ^^(e.g. NINA-W1^^)^^, with a SARA-R4 module on a WHRE board
set platform_2=Amazon-FreeRTOS SDK^^, latest version^^, on the ESP32 chipset ^^(e.g. NINA-W1^^)^^, with a SARA-R4 module on a WHRE board
set platform_3=latest Espressif SDK on the ESP32 chipset ^^(e.g. NINA-W1^^)^^, with a SARA-R4 module on a WHRE board talking to AWS

rem Process command line parameters
set pos=0
:parameters
    rem get arg removing quotes from paths (so that we can concatenate paths here)
    set arg=%~1
    rem pos represents the number of a positional argument
    if not "%arg%"=="" (
        if "%arg%"=="/f" (
            set fetch=YES
        ) else if "%arg%"=="/d" (
            set createdir=YES
        ) else if "%arg%"=="/c" (
            set cleandir=YES
        ) else if "%arg%"=="/b" (
            set cleanbuild=YES
        ) else if "%pos%"=="0" (
            set platform_number=%arg%
            set /A pos=pos+1
        ) else if "%pos%"=="1" (
            set directory=%arg%
            set /A pos=pos+1
        ) else if "%pos%"=="2" (
            set com_port=%arg%
            set /A pos=pos+2
        ) else (
            echo %~n0: ERROR can't understand parameter "%arg%".
            goto usage
        )
        shift /1
        goto parameters
    )

rem Check/report parameters
if not "%platform_number%"=="" (
    echo %~n0: platform is %platform_number%.
) else (
    echo %~n0: ERROR must specify a platform.
    goto usage
)
if not "%directory%"=="" (
    echo %~n0: directory is "%directory%".
    if exist "%directory%" (
        if not "%cleandir%"=="" (
            echo %~n0: cleaning directory...
            del /s /q /f "%directory%\*" 1>nul
            for /f "delims=" %%f in ('dir /ad /b "%directory%"') do rd /s /q "%directory%\%%f" 1>nul
        )
    )
) else (
    echo %~n0: ERROR must specify a directory.
    goto usage
)
if not exist "%directory%" (
    if not "%createdir%"=="" (
        echo %~n0: directory "%directory%" does not exist, creating it...
        md "%directory%"
        if not exist "%directory%" (
            echo %~n0: ERROR unable to create directory "%directory%" ^(you must use backslash \, not /^).
            goto usage
        )
    ) else (
        echo %~n0: ERROR directory "%directory%" does not exist and command-line switch /c was not specified.
        goto usage
    )
)
if not "%com_port%"=="" (
    echo %~n0: COM port is %com_port%.
) else (
    echo %~n0: ERROR must specify a COM port, e.g. COM1.
    goto usage
)

rem Set the platform
if "%platform_number%"=="1" (
    set platform_string=%platform_1%
    goto build_start
) else if "%platform_number%"=="2" (
    set platform_string=%platform_2%
    goto build_start
) else if "%platform_number%"=="3" (
    set platform_string=%platform_3%
    goto build_start
) else (
    echo %~n0: ERROR don't understand platform %platform_number%.
    goto usage
)

rem Build start
:build_start
    echo.
    echo %~n0: ######## start of build/test for platform %platform_number%: %platform_string% ########
    echo.
    echo %~n0: changing directory to "%directory%"...
    pushd "%directory%"
    if not "%fetch%"=="" (
        if exist cellular (
            pushd cellular
            echo %~n0: updating cellular driver code...
            call git pull
            popd
        ) else (
            echo %~n0: cloning cellular driver from https://github.com/u-blox/cellular...
            call git clone https://github.com/u-blox/cellular.git
        )
    ) else (
        echo %~n0: not fetching any code, just building.
    )   
    if "%platform_number%"=="1" (
        goto build_platform_1
    ) else if "%platform_number%"=="2" (
        goto build_platform_2
    ) else if "%platform_number%"=="3" (
        goto build_platform_3
    )

rem Build end
:build_end
    popd
    echo.
    echo %~n0: ######## end of build/test for platform %platform_number%: %platform_string% ########
    echo.
    goto end

rem Build platform 1: unit tests on ESP32 with a SARA-R4 module on a WHRE board
:build_platform_1
    if not "%fetch%"=="" (
        if exist esp-idf (
            pushd esp-idf
            echo %~n0: updating ESP-IDF code...
            call git pull
            popd
        ) else (
            echo %~n0: cloning ESP-IDF from https://github.com/u-blox/esp-idf ^(using u-blox branch for fix for flash chip version used in NINA-W1^)...
            call git clone https://github.com/u-blox/esp-idf.git
        )
    )
    echo %~n0: setting up paths assuming Python 2.7 is in "C:\Python27"...
    set path=C:\Python27;C:\Python27\Scripts;%path%
    echo %~n0: ### Print version information start ###
    python --version
    echo %~n0: ### Print version information end ###
    echo %~n0: setting environment variables assuming the latest ESP-IDF tools will be installed in C:\Program Files\Espressif\ESP-IDF Tools latest...
    @echo on
    set IDF_TOOLS_PATH=C:\Program Files\Espressif\ESP-IDF Tools latest
    @echo off
    rem To run the installation process we need to fiddle with files in ESP-IDF Tools, which is in Program Files and requires
    rem administrator privilages, so check for that here, see:
    rem https://stackoverflow.com/questions/1894967/how-to-request-administrator-access-inside-a-batch-file/10052222#10052222
    if "%PROCESSOR_ARCHITECTURE%"=="amd64" (
        >nul 2>&1 "%SYSTEMROOT%\SysWOW64\cacls.exe" "%SYSTEMROOT%\SysWOW64\config\system"
    ) else (
        >nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
    )
    rem If error flag is set, we do not have admin.
    if not "%errorlevel%"=="0" (
        echo %~n0: ERROR administrator privileges are required to run the ESP-IDF installation batch file, please run as administrator.
        goto build_end
    )
    echo %~n0: calling ESP-IDF install.bat from ESP-IDF directory...
    pushd esp-idf
    echo %~n0: %CD%
    call install.bat
    echo %~n0: calling ESP-IDF export.bat from ESP-IDF directory...
    call export.bat
    popd
    echo %~n0: building tests and then downloading them over %com_port%...
    pushd cellular\port\platform\espressif\sdk\esp-idf\unit_test
    if not "%cleanbuild%"=="" (
        echo %~n0: build is a clean build.
        @echo on
        idf.py -c -p %com_port% -D TEST_COMPONENTS="cellular_tests" flash monitor
        @echo off
    ) else (
        @echo on
        idf.py -p %com_port% -D TEST_COMPONENTS="cellular_tests" flash monitor
        @echo off
    )
    popd
    goto build_end

rem Build platform 2: Amazon-FreeRTOS SDK on ESP32 with a SARA-R4 module on a WHRE board
:build_platform_2
    if not "%fetch%"=="" (
        if exist amazon-freertos (
            pushd amazon-freertos
            echo %~n0: updating Amazon FreeRTOS SDK code...
            call git pull
            popd
        ) else (
            echo %~n0: cloning Amazon FreeRTOS SDK from https://github.com/aws/amazon-freertos...
            call git clone https://github.com/aws/amazon-freertos.git
        )
    )
    echo %~n0: setting up paths assuming the V3.3 Espressif tools are in directory "C:\Program Files\Espressif\ESP-IDF Tools for v3.3",
    echo       CMake version 3.13 or later is in "C:\Program Files\CMake" and Python 2.7 is in "C:\Python27"...
    echo %~n0: note: you must have already installed all of these tools (WITHOUT adding them to the path, to do this you may need to
    echo       temporarily add them to the path with the line below) and run both "easy_install awscli" and "easy_install boto3".
    set path=C:\Program Files\Espressif\ESP-IDF Tools for v3.3\mconf-idf;C:\Program Files\Espressif\ESP-IDF Tools for v3.3\tools\bin;C:\Program Files\CMake\bin;C:\Python27;C:\Python27\Scripts;%path%
    echo %~n0: ### Print version information start ###
    xtensa-esp32-elf-gcc --version
    cmake --version
    python --version
    echo %~n0: ### Print version information end ###
    echo %~n0: Setting environment variables...
    @echo on
    set THING_NAME=rob_test
    set IDF_PATH=
    @echo off
    echo %~n0: copying configuration files...
    @echo on
    copy "%config_directory%\amazon-freertos\configure.json" amazon-freertos\tools\aws_config_quick_start
    @echo off
    goto build_end

rem Build platform 3: Espressif SDK v4 on ESP32 with a SARA-R4 module on a WHRE board talking to AWS
:build_platform_3
    echo %~n0: ERROR: not yet implemented.
    goto build_end

rem Usage string
:usage
    echo.
    echo Usage: %~n0 [/f /d /c /b] platform directory comport
    echo.
    echo where:
    echo.
    echo - /f indicates that a code fetch should be performed^; if this is NOT specified then all the necessary code is
    echo   assumed to have already been fetched to directory,
    echo - /d indicates that directory should be created if it does not exist,
    echo - /c indicates that directory should be cleaned first,
    echo - /b indicates that the build directory should be cleaned first,
    echo - platform is a number representing the platform you'd like to test/build for selected from the following:
    echo   1: %platform_1%
    echo   2: %platform_2%
    echo   3: %platform_3%
    rem Add more here when new SDKs/platforms are integrated
    echo - directory is the directory in which to fetch code and do building/testing.
    echo - comport is the port where the device under test is connected (e.g. COM1).
    echo.
    echo Note that the installation of various tools may require administrator privileges and so it is usually
    echo advisable to run this batch file as administrator.
    goto end

rem Done
:end
    echo.
    echo %~n0: end.
    exit /b
