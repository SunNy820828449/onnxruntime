#!/bin/bash
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

LocalNuGetRepo=$1
SourceRoot=$2

@echo "Downloading test data"
python build.by --update --download_test_data

MajorVersion=$(cat $SourceRoot/VERSION_NUMBER)
VersionSuffix=
if [ "$IsReleaseBuild" != "true" ]; then
    VersionSuffix = -dev-$(git rev-parse --short HEAD)
fi
export CurrentOnnxRuntimeVersion=$MajorVersion$VersionSuffix
echo "Current NuGet package version is $CurrentOnnxRuntimeVersion"

dotnet restore $SourceRoot/csharp/test/Microsoft.ML.OnnxRuntime.EndToEndTests/Microsoft.ML.OnnxRuntime.EndToEndTests.csproj -s $LocalNuGetRepo --configfile $SourceRoot/csharp/Nuget.CSharp.config
if [ $? -ne 0 ]
    echo "Failed to restore nuget packages for the test project"
    exit 1
)

dotnet test $SourceRoot/csharp/test/Microsoft.ML.OnnxRuntime.EndToEndTests/Microsoft.ML.OnnxRuntime.EndToEndTests.csproj --no-restore
if [ $? -ne 0 ]
    echo "Failed to build or execute the end-to-end test"
    exit 1
)