# Check PowerShell version
if (((Test-Path variable:PSVersionTable) -eq $False) -Or ($PSVersionTable.PSVersion.Major -lt 4))
{
    Write-Host "You need PowerShell v4.0 or greater to run this script."
    Write-Host "Please refer to:"
    Write-Host "https://docs.microsoft.com/en-us/powershell/scripting/setup/installing-windows-powershell?#upgrading-existing-windows-powershell"
    exit
}

# Get absolute path from relative path
function Resolve-FullPath
{
    [cmdletbinding()]
    param
    (
        [Parameter(
            Mandatory=$true,
            Position=0,
            ValueFromPipeline=$true)]
        [string] $path
    )
    
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($path)
}

# Create destination directory recursively
function createDestDir
{
    [cmdletbinding()]
    param
    (
        [Parameter(
            Mandatory=$true,
            Position=0,
            ValueFromPipeline=$true)]
        [string] $fullPath
    )
    
    $test = (Test-Path -Path "$fullPath")
    if ($test -eq $False)
    {
        New-Item -ItemType Directory -Path "$fullPath" -ErrorAction SilentlyContinue | Out-Null
        $test = (Test-Path -Path "$fullPath")
        if ($test -eq $False)
        {
            Write-Host "Error creating path: `"$fullPath`"."
            exit
        }
    }
}

# Download file from a GitHub release
function getGitHubRepoFile
{
    [cmdletbinding()]
    param
    (
        [Parameter(
            Mandatory=$true,
            Position=0,
            ValueFromPipeline=$true)]
        [string] $repository,
        [Parameter(
            Mandatory=$true,
            Position=1,
            ValueFromPipeline=$true)]
        [string] $fileToGet,
        [Parameter(
            Mandatory=$true,
            Position=2,
            ValueFromPipeline=$true)]
        [string] $destDir
    )
    
    Write-Host "Determining latest release for `"$repository`"..."
    
    $releases = "https://api.github.com/repos/$repository/releases"
    
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $tag = (Invoke-WebRequest -Uri $releases -UseBasicParsing | ConvertFrom-Json)[0].tag_name
    
    Write-Host "Downloading file `"$fileToGet`" from latest release `"$tag`"..."
    
    $download = "https://raw.githubusercontent.com/$repository/$tag/$fileToGet"
    
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest $download -Out "$destDir\$fileToGet"
    
    Write-Host "File `"$fileToGet`" downloaded to `"$destDir\$fileToGet`"."
}

# GitHub repository details
$repo = "DaveGamble/cJSON"

# Destination directory prefix
$dir_prefix = "..\include\external\cJSON"

# Convert relative path to absolute path
$dir_prefix = Resolve-FullPath($dir_prefix)

# Create directories if necessary
createDestDir($dir_prefix)

# Check if the necessary files are already available

$cjson_c = (Test-Path -Path "$dir_prefix\cJSON.c" -PathType Leaf)
if ($cjson_c -eq $False)
{
    # Download cJSON.c
    Write-Host "File `"cJSON.c`" not available."
    getGitHubRepoFile $repo "cJSON.c" $dir_prefix
} else {
    Write-Host "cJSON.c already available."
}

$cjson_h = (Test-Path -Path "$dir_prefix\cJSON.h" -PathType Leaf)
if ($cjson_h -eq $False)
{
    # Download cJSON.h
    Write-Host "File `"cJSON.h`" not available."
    getGitHubRepoFile $repo "cJSON.h" $dir_prefix
} else {
    Write-Host "cJSON.h already available."
}
