REM Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
REM Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
REM SPDX-License-Identifier: Apache-2.0


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