Get-ChildItem -Directory -Filter "assignment*" | ForEach-Object {
    $dir = $_.FullName
    $name = $_.Name
    $main = Join-Path $dir "src\main.cpp"
    if (Test-Path $main) {
        $lines = (Get-Content $main).Length
        $content = Get-Content $main -Raw
        $hasWin = $content.Contains("glfwCreateWindow") -or $content.Contains("createWindow")
        $hasDraw = $content.Contains("vkCmdDraw") -or $content.Contains("vkCmdDispatch") -or $content.Contains("vkCmdTraceRays")
        Write-Output ("{0,-55} | Lines: {1,4} | Window: {2,-5} | Draw: {3,-5}" -f $name, $lines, $hasWin, $hasDraw)
    } else {
        Write-Output ("{0,-55} | NO MAIN" -f $name)
    }
}
