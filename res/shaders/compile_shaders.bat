REM Copyright 2024 Beyond Reality Labs Ltd (https://beyondreality.io)
REM All rights reserved.

REM Check if the first parameter is provided
if "%~1"=="" (
    echo Nothing to process. Provide a shader name or shader type and name to process
    goto :end
)

REM Check the value of the first parameter
REM /i makes the comparison case-insensitive
if /i "%~1"=="comp" (
    glslc  %3/%2.comp -o %4/%2.comp.spv
) else if /i "%~1"=="geom" (
    glslc  %3/%2.geom -o %4/%2.geom.spv
) else if /i "%~1"=="tesc" (
    glslc  %3/%2.tesc -o %4/%2.tesc.spv
) else if /i "%~1"=="tese" (
    glslc  %3/%2.tese -o %4/%2.tese.spv
) else if /i "%~1"=="vert" (
    glslc  %3/%2.vert -o %4/%2.vert.spv
) else if /i "%~1"=="frag" (
    glslc  %3/%2.frag -o %4/%2.frag.spv
) else (
    glslc  %2/%1.vert -o %3/%1.vert.spv
    glslc  %2/%1.frag -o %3/%1.frag.spv
)

:end
endlocal