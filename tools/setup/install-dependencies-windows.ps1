param(
    [Parameter(Mandatory = $true)]
    [string]$GStreamerVersion,
    [switch]$SkipVulkan
)

$ErrorActionPreference = "Stop"

$gstRoot = $env:GST_ROOT
if ([string]::IsNullOrEmpty($gstRoot)) {
    $gstRoot = Join-Path $env:GITHUB_WORKSPACE "gstreamer\1.0\msvc_x86_64"
}

New-Item -ItemType Directory -Force -Path $gstRoot | Out-Null

$fallbackVersion = "1.16.3"
$baseUrl = "https://gstreamer.freedesktop.org/data/pkg/windows/$GStreamerVersion"
$runtimeMsi = Join-Path $env:TEMP "gstreamer-runtime.msi"
$develMsi = Join-Path $env:TEMP "gstreamer-devel.msi"

function Download-File($url, $path) {
    $max = 5
    for ($i = 1; $i -le $max; $i++) {
        try {
            Invoke-WebRequest -Uri $url -OutFile $path -UseBasicParsing
            return $true
        } catch {
            if ($i -eq $max) { throw }
            Start-Sleep -Seconds 5
        }
    }
    return $false
}

$runtimeUrl = "$baseUrl/gstreamer-1.0-msvc-x86_64-$GStreamerVersion.msi"
$develUrl = "$baseUrl/gstreamer-1.0-devel-msvc-x86_64-$GStreamerVersion.msi"

try {
    Download-File $runtimeUrl $runtimeMsi | Out-Null
    Download-File $develUrl $develMsi | Out-Null
} catch {
    Write-Host "GStreamer $GStreamerVersion not found, falling back to $fallbackVersion"
    $baseUrl = "https://gstreamer.freedesktop.org/data/pkg/windows/$fallbackVersion"
    $runtimeUrl = "$baseUrl/gstreamer-1.0-msvc-x86_64-$fallbackVersion.msi"
    $develUrl = "$baseUrl/gstreamer-1.0-devel-msvc-x86_64-$fallbackVersion.msi"
    Download-File $runtimeUrl $runtimeMsi | Out-Null
    Download-File $develUrl $develMsi | Out-Null
}

Start-Process msiexec.exe -Wait -ArgumentList "/i `"$runtimeMsi`" /qn /norestart INSTALLDIR=`"$gstRoot`""
Start-Process msiexec.exe -Wait -ArgumentList "/i `"$develMsi`" /qn /norestart INSTALLDIR=`"$gstRoot`""

Add-Content -Path $env:GITHUB_PATH -Value (Join-Path $gstRoot "bin")
Add-Content -Path $env:GITHUB_ENV -Value "GSTREAMER_1_0_ROOT_MSVC_X86_64=$gstRoot"
Add-Content -Path $env:GITHUB_ENV -Value "PKG_CONFIG_PATH=$gstRoot\lib\pkgconfig"
