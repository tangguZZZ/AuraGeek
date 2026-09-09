# Save this script as UTF-8 with BOM for Windows PowerShell 5.1.
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$env:PYTHONPATH='D:\LVGL-Dev\python-image-tools'
$converter='D:\LVGL-Dev\sources\lvgl-9.5.0\scripts\LVGLImage.py'
$output=Join-Path $project 'src\ui\resources'
$items=@(@('Desktop_homepage','ag_home'),@('Stock_analysis','ag_stock'),@('Music_spectrum','ag_music'),@('Agent','ag_agent'),@('weather_component','ag_weather_unknown'),@('weather_icon_sunny','ag_weather_sunny'),@('weather_icon_cloudy','ag_weather_cloudy'),@('weather_icon_light_rain','ag_weather_rain'),@('weather_icon_light_snow','ag_weather_snow'),@('weather_icon_thunderstorm','ag_weather_thunder'))
foreach($item in $items){
 python $converter --ofmt C --cf RGB565A8 --name $item[1] -o $output (Join-Path $project ('Doc\UI素材\'+$item[0]+'.png'))
 if($LASTEXITCODE){throw ('Image conversion failed: '+$item[0])}
}
foreach($name in @('humidity','temperature','wifi_0','wifi_1')) {
 python $converter --ofmt C --cf RGB565A8 --name ('ag_'+$name) -o $output (Join-Path $project ('Doc\UI素材\'+$name+'.png'))
 if($LASTEXITCODE){throw ('Image conversion failed: '+$name)}
}
$intermediate=Join-Path $project 'simulator\build\wallpaper.png'
Add-Type -AssemblyName System.Drawing
$bitmap=[System.Drawing.Image]::FromFile((Join-Path $project 'Doc\UI素材\wallpaper.jpg'))
try {$bitmap.Save($intermediate,[System.Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
python $converter --ofmt C --cf RGB565 --name ag_wallpaper -o $output $intermediate
if($LASTEXITCODE){throw 'Wallpaper conversion failed'}
$menuIntermediate=Join-Path $project 'simulator\build\menu_wallpaper_320x240.png'
$menuSource=[System.Drawing.Image]::FromFile((Join-Path $project 'Doc\UI素材\Menu_wallpaper.jpg'))
$menuBitmap=New-Object System.Drawing.Bitmap 320,240
$graphics=[System.Drawing.Graphics]::FromImage($menuBitmap)
try {
 $graphics.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
 $graphics.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
 $graphics.DrawImage($menuSource,0,0,320,240)
 $menuBitmap.Save($menuIntermediate,[System.Drawing.Imaging.ImageFormat]::Png)
} finally {
 $graphics.Dispose();$menuBitmap.Dispose();$menuSource.Dispose()
}
python $converter --ofmt C --cf RGB565 --name ag_menu_wallpaper -o $output $menuIntermediate
if($LASTEXITCODE){throw 'Menu wallpaper conversion failed'}
$gif=Get-ChildItem -LiteralPath (Join-Path $project 'Doc\UI素材\机器人表情包gif_20260820') -Recurse -Filter 'neutral-01-idle-blink.gif' | Select-Object -First 1
if(!$gif){throw 'Idle GIF source not found'}
python $converter --ofmt C --cf RAW --name ag_idle -o $output $gif.FullName
if($LASTEXITCODE){throw 'GIF conversion failed'}
