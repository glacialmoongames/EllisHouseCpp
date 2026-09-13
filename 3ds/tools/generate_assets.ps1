param([string]$ProjectRoot=(Resolve-Path "$PSScriptRoot/../..").Path)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$drawingRefs=@([System.Drawing.Bitmap].Assembly.Location,[System.Drawing.Rectangle].Assembly.Location,(Join-Path $PSHOME 'System.Private.Windows.GdiPlus.dll'),(Join-Path $PSHOME 'System.Private.Windows.Core.dll'))
Add-Type -ReferencedAssemblies $drawingRefs -TypeDefinition @'
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices; using System.IO;
public static class AlphaExport {
 public static int[] Size(string p) { using(var b=new Bitmap(p)) return new[]{b.Width,b.Height}; }
 public static void Write(string src,string dst) { using(var input=new Bitmap(src)) using(var b=new Bitmap(input.Width,input.Height,PixelFormat.Format32bppArgb)) { using(var g=Graphics.FromImage(b)) g.DrawImageUnscaled(input,0,0); var r=new Rectangle(0,0,b.Width,b.Height); var d=b.LockBits(r,ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb); var raw=new byte[Math.Abs(d.Stride)*b.Height]; Marshal.Copy(d.Scan0,raw,0,raw.Length); b.UnlockBits(d); using(var f=File.Create(dst)) for(int y=0;y<b.Height;y++) for(int x=0;x<b.Width;x++) f.WriteByte(raw[y*d.Stride+x*4+3]); } }
}
'@
$port=Join-Path $ProjectRoot '3ds'; $gfx=Join-Path $port 'gfx'; $src=Join-Path $port 'gfx_src'; $rom=Join-Path $port 'romfs'
New-Item -ItemType Directory -Force $gfx,$src,(Join-Path $rom 'assets'),(Join-Path $rom 'masks'),(Join-Path $port 'source') | Out-Null
Copy-Item (Join-Path $ProjectRoot 'assets/game.manifest') (Join-Path $rom 'assets/game.manifest') -Force
New-Item -ItemType Directory -Force (Join-Path $rom 'assets/sounds') | Out-Null
Copy-Item (Join-Path $ProjectRoot 'assets/sounds/*') (Join-Path $rom 'assets/sounds') -Force
$manifestLines=Get-Content (Join-Path $ProjectRoot 'assets/game.manifest')
$audioOut=Join-Path $rom 'audio';New-Item -ItemType Directory -Force $audioOut | Out-Null
$ffmpeg=if($env:FFMPEG){$env:FFMPEG}else{(Get-Command ffmpeg -ErrorAction Stop).Source}
$soundPaths=$manifestLines | ForEach-Object {$f=$_ -split "`t";if($f[0]-eq 'SOUND' -and $f.Count-gt 2){$f[2]}}
foreach($soundPath in $soundPaths) {
 $audioName=([IO.Path]::GetFileNameWithoutExtension($soundPath).ToUpperInvariant()+'.PCM')
 & $ffmpeg -hide_banner -loglevel error -y -i (Join-Path $ProjectRoot ($soundPath-replace '/','\')) -ac 1 -ar 22050 -f s16le (Join-Path $audioOut $audioName)
 if($LASTEXITCODE){throw "Nintendo 3DS audio conversion failed: $soundPath"}
}
$paths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$cropPaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$graphicSprites=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$spriteFrames=@{}
$manifestLines | ForEach-Object { $f=$_ -split "`t"; if($f[0]-eq 'SPRITE' -and $f.Count-gt 12){$frames=$f[12]-split ';'|?{$_};$spriteFrames[$f[1]]=$frames;$frames|%{[void]$paths.Add($_)}} elseif($f[0]-eq 'FONT' -and $f.Count-gt 2){[void]$paths.Add($f[2]);[void]$cropPaths.Add($f[2])} elseif($f[0]-eq 'GRAPHIC' -and $f.Count-gt 2){[void]$graphicSprites.Add($f[2])} }
foreach($spriteName in $graphicSprites){foreach($framePath in $spriteFrames[$spriteName]){[void]$cropPaths.Add($framePath)}}
$assets=@(); $parts=@(); $batch=0; $batchArea=0; $batchImages=@(); $batches=@()
foreach($rel in ($paths|Sort-Object)) {
 $full=Join-Path $ProjectRoot ($rel -replace '/','\'); $size=[AlphaExport]::Size($full); $w=$size[0];$h=$size[1]; $first=$parts.Count
 $dedicated=$cropPaths.Contains($rel)
 $tiles=@(); for($y=0;$y-lt$h;$y+=512){for($x=0;$x-lt$w;$x+=512){$tw=[Math]::Min(512,$w-$x);$th=[Math]::Min(512,$h-$y);$tiles+=,@($x,$y,$tw,$th)}}
 foreach($t in $tiles){$area=($t[2]+4)*($t[3]+4);if(($dedicated -and $batchImages.Count) -or $batchImages.Count-ge 96 -or ($batchArea+$area)-gt 700000){$batches+=,@($batchImages);$batch++;$batchImages=@();$batchArea=0}
   $id=('i{0:d4}' -f $parts.Count);$png=Join-Path $src ($id+'.png');$bmp=[Drawing.Bitmap]::new($t[2],$t[3],[Drawing.Imaging.PixelFormat]::Format32bppArgb);$g=[Drawing.Graphics]::FromImage($bmp);$g.DrawImage([Drawing.Image]::FromFile($full),[Drawing.Rectangle]::new(0,0,$t[2],$t[3]),[Drawing.Rectangle]::new($t[0],$t[1],$t[2],$t[3]),[Drawing.GraphicsUnit]::Pixel);$g.Dispose();$bmp.Save($png,[Drawing.Imaging.ImageFormat]::Png);$bmp.Dispose()
   $parts+=,[pscustomobject]@{sheet=$batch;image=$batchImages.Count;x=$t[0];y=$t[1];w=$t[2];h=$t[3]};$batchImages+=,('../gfx_src/'+$id+'.png');$batchArea+=$area
   if($dedicated){$batches+=,@($batchImages);$batch++;$batchImages=@();$batchArea=0} }
 $mask=('m{0:d4}.a8' -f $assets.Count); [AlphaExport]::Write($full,(Join-Path $rom "masks/$mask")); $assets+=,[pscustomobject]@{path=('romfs:/'+($rel-replace '\\','/'));w=$w;h=$h;first=$first;count=$tiles.Count;mask=$mask}
}
if($batchImages.Count){$batches+=,@($batchImages)}
Remove-Item (Join-Path $gfx '*.t3s') -Force -ErrorAction SilentlyContinue
for($i=0;$i-lt$batches.Count;$i++){ $lines=@($(if($batches[$i].Count-gt 1){'--atlas -f rgba5551 -z auto'}else{'-f rgba5551 -z auto'})); $lines += $batches[$i]|%{($_ -replace '\\','/')}; Set-Content (Join-Path $gfx ('sheet{0:d3}.t3s' -f $i)) $lines -Encoding ascii }
$out=@('#include "texture_map.hpp"','const TextureAsset3DS gTextureAssets3DS[] = {')
foreach($a in $assets){$out+='  {"'+$a.path+'",'+$a.w+','+$a.h+','+$a.first+','+$a.count+'},'};$out+=('};','const std::size_t gTextureAssetCount3DS = sizeof(gTextureAssets3DS)/sizeof(gTextureAssets3DS[0]);','const TexturePart3DS gTextureParts3DS[] = {')
foreach($p in $parts){$out+='  {'+$p.sheet+','+$p.image+','+$p.x+','+$p.y+','+$p.w+','+$p.h+'},'};$out+=('};','const char* const gTextureSheets3DS[] = {')
for($i=0;$i-lt$batches.Count;$i++){$out+='  "romfs:/gfx/sheet'+('{0:d3}'-f $i)+'.t3x",'};$out+='};'
Set-Content (Join-Path $port 'source/texture_map.cpp') $out -Encoding utf8
Write-Host "Generated $($assets.Count) textures, $($parts.Count) parts, $($batches.Count) sheets."
