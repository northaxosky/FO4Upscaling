"""Inspect a running Fallout 4 process for Streamline/DLSS modules and configuration."""
import subprocess
import sys

def get_loaded_modules(process_name="Fallout4.exe"):
    """Get all loaded DLLs in the target process."""
    result = subprocess.run(
        ["tasklist.exe", "/M", "/FI", f"IMAGENAME eq {process_name}"],
        capture_output=True, text=True
    )
    return result.stdout

def get_module_details(process_name="Fallout4.exe"):
    """Get detailed module info via PowerShell."""
    ps_cmd = f"""
    $proc = Get-Process -Name '{process_name.replace(".exe", "")}' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($proc) {{
        Write-Host "PID: $($proc.Id)"
        Write-Host "Memory: $([math]::Round($proc.WorkingSet64 / 1MB, 0))MB"
        Write-Host ""
        Write-Host "=== Streamline / DLSS / FidelityFX Modules ==="
        $proc.Modules | Where-Object {{
            $_.ModuleName -match 'sl\.|nvngx|dlss|fidelity|ffx|interposer|reflex|pcl'
        }} | ForEach-Object {{
            $size = [math]::Round($_.ModuleMemorySize / 1KB, 0)
            Write-Host ("  {{0,-35}} {{1,8}}KB  {{2}}" -f $_.ModuleName, $size, $_.FileName)
        }}
        Write-Host ""
        Write-Host "=== F4SE Plugins ==="
        $proc.Modules | Where-Object {{
            $_.FileName -match 'F4SE.*Plugins' -and $_.ModuleName -match '\.dll$'
        }} | ForEach-Object {{
            $size = [math]::Round($_.ModuleMemorySize / 1KB, 0)
            Write-Host ("  {{0,-35}} {{1,8}}KB  {{2}}" -f $_.ModuleName, $size, $_.FileName)
        }}
        Write-Host ""
        Write-Host "=== All D3D / DXGI Modules ==="
        $proc.Modules | Where-Object {{
            $_.ModuleName -match 'd3d|dxgi|d3dcompiler'
        }} | ForEach-Object {{
            $size = [math]::Round($_.ModuleMemorySize / 1KB, 0)
            Write-Host ("  {{0,-35}} {{1,8}}KB  {{2}}" -f $_.ModuleName, $size, $_.FileName)
        }}
    }} else {{
        Write-Host "Process not found: {process_name}"
    }}
    """
    result = subprocess.run(
        ["powershell.exe", "-Command", ps_cmd],
        capture_output=True, text=True
    )
    return result.stdout

if __name__ == "__main__":
    process = sys.argv[1] if len(sys.argv) > 1 else "Fallout4.exe"
    print(f"Inspecting: {process}\n")
    print(get_module_details(process))
