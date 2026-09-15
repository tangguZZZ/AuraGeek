$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$env:PYTHONPATH='D:\LVGL-Dev\python-image-tools'
$converter='D:\LVGL-Dev\sources\lvgl-9.5.0\scripts\LVGLImage.py'
$source=Join-Path $project 'Doc\UI素材\机器人表情包gif_20260820\macbot表情包gif_第一版'
$items=@{
 ag_face_listen='surprised-03-alert.gif'
 ag_face_think='thinking-02-peeking.gif'
 ag_face_happy='happy-01-gentle-smile.gif'
 ag_face_sad='sad-02-frustrated-1to1.gif'
 ag_face_angry='angry-02-frowning.gif'
 ag_face_love='loving-02-heart-eyes.gif'
}
foreach($item in $items.GetEnumerator()){
 python $converter --ofmt C --cf RAW --name $item.Key -o (Join-Path $project 'src\ui\resources') (Join-Path $source $item.Value)
 if($LASTEXITCODE){throw ('GIF conversion failed: '+$item.Value)}
}
