param(
  [string]$Converter='C:/Users/MECHREVO/AppData/Local/npm-cache/_npx/b62fd1a864044392/node_modules/lv_font_conv/lv_font_conv.js',
  [string]$Node='D:/NODE_JS/node.exe',
  [string]$Font='D:/LVGL-Dev/sources/lvgl-9.5.0/tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf'
)
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$symbols='接口拒绝已暂停请求受限稍后再试调用记录异常不内存足更新模拟数据界面测试参考快照非实时日线未入缓存复权完成保留正在读取本地暂无等待联网检查标的编号单击切换页管理自选收盘半年全部区间涨跌'
foreach($size in @(10,12,16)) {
  $name='stock_font_'+$size
  & $Node $Converter --font $Font --range 0x20-0x7F --symbols $symbols --size $size --bpp 4 --format lvgl --no-compress --lv-font-name $name --lv-include lvgl.h -o (Join-Path $project ('src/ui/resources/'+$name+'.c'))
  if($LASTEXITCODE){throw ('Stock font generation failed: '+$name)}
}
