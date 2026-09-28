$source = Join-Path $PSScriptRoot 'html\master_web.htmlx'
$target = Join-Path $PSScriptRoot 'local_web_page.h'
$html = [System.IO.File]::ReadAllText($source)
$header = @'
#ifndef LOCAL_WEB_PAGE_H
#define LOCAL_WEB_PAGE_H

#include <Arduino.h>

static const char LOCAL_WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
'@
$footer = @'

)HTML";

#endif // LOCAL_WEB_PAGE_H
'@
[System.IO.File]::WriteAllText($target, $header + "`n" + $html + $footer, [System.Text.UTF8Encoding]::new($false))
